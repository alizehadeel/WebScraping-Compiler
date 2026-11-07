// src/interp.cpp - tree-walking interpreter
// OWNER: Aleena | PHASE: 1 | STATUS: DONE
//
// The interpreter is the REFERENCE for Phase 2: a compiled program is correct when its
// output matches `--run`. So the order of evaluation here is the order codegen must follow:
//   * binary operators:  left, then right (and/or short-circuit)
//   * call arguments:    left to right
//   * r[i] <- v:         the index, then the value, then the bounds check, then the store
//   * save v to p:       the value, then the path
//   * for i from a until b:  a, then b, each evaluated once
// Types are trusted: Sema already rejected every ill-typed program.
#include "interp.h"
#include "sema.h"
#include "rt.h"
#include <vector>

namespace {

struct Value {                       // one of the three fields is meaningful, chosen by the static type
    int64_t     num  = 0;
    const char* text = nullptr;      // literals point into the AST, grab results into the runtime's Row
    Row*        row  = nullptr;      // rows are references: copying a Value shares the row
};

struct Frame {                       // one per call (and one for the main program)
    std::vector<Value> slots;
    explicit Frame(int n) : slots(n) {}
};

enum class Flow { NORMAL, RETURN };

// num wraps on overflow. Signed overflow is undefined behaviour in C++, so use unsigned maths.
inline int64_t wrapAdd(int64_t a, int64_t b) { return (int64_t)((uint64_t)a + (uint64_t)b); }
inline int64_t wrapSub(int64_t a, int64_t b) { return (int64_t)((uint64_t)a - (uint64_t)b); }
inline int64_t wrapMul(int64_t a, int64_t b) { return (int64_t)((uint64_t)a * (uint64_t)b); }
inline int64_t wrapNeg(int64_t a)            { return (int64_t)(0 - (uint64_t)a); }

class Interp {
public:
    Flow  execBlock(Block& b, Frame& f);

private:
    Flow  exec(Stmt& s, Frame& f);
    Value eval(Expr& e, Frame& f);
    Value callRecipe(Expr& call, Frame& f);
    Value defaultValue(Type t, int line);

    Value ret_;                      // the value of the `give` that is unwinding the call stack
};

Value Interp::defaultValue(Type t, int line) {      // "reaching the end without a give"
    Value v;
    switch (t) {
        case Type::TEXT:     v.text = "";                         break;
        case Type::ROW_NUM:  v.row = rt_row_new(0, 0, line);      break;
        case Type::ROW_TEXT: v.row = rt_row_new(0, 1, line);      break;
        default: break;                                           // NUM: 0, VOID: nothing
    }
    return v;
}

Value Interp::callRecipe(Expr& call, Frame& f) {
    RecipeDef* d = call.sym->def;
    Frame callee(d->frameSize);                      // a fresh frame per call: recursion just works
    for (size_t i = 0; i < call.args.size(); ++i)    // arguments are evaluated left to right
        callee.slots[d->params[i].sym->slot] = eval(*call.args[i], f);   // rows copy the pointer only
    Flow fl = execBlock(*d->body, callee);
    if (fl == Flow::RETURN) {
        Value r = ret_;
        ret_ = Value();
        return r;
    }
    return defaultValue(d->retType, call.line);
}

Value Interp::eval(Expr& e, Frame& f) {
    Value v;
    switch (e.kind) {
    case ExprKind::NUMBER: v.num  = e.num;            return v;
    case ExprKind::STRING: v.text = e.text.c_str();   return v;
    case ExprKind::VAR:    return f.slots[e.sym->slot];

    case ExprKind::INDEX: {
        Row* r = f.slots[e.sym->slot].row;
        int64_t i = eval(*e.lhs, f).num;
        rt_check_index(r, i, e.line);
        if (r->is_text) v.text = ((char**)r->data)[i];
        else            v.num  = ((int64_t*)r->data)[i];
        return v;
    }

    case ExprKind::UNARY: {
        int64_t a = eval(*e.lhs, f).num;
        v.num = (e.op == Op::NOT) ? (a == 0) : wrapNeg(a);
        return v;
    }

    case ExprKind::BINARY: {
        if (e.op == Op::AND) {                       // short-circuit: right side only if left is non-zero
            v.num = eval(*e.lhs, f).num != 0 && eval(*e.rhs, f).num != 0;
            return v;
        }
        if (e.op == Op::OR) {                        // short-circuit: right side only if left is zero
            v.num = eval(*e.lhs, f).num != 0 || eval(*e.rhs, f).num != 0;
            return v;
        }
        Value a = eval(*e.lhs, f);
        Value b = eval(*e.rhs, f);
        switch (e.op) {
            case Op::ADD: v.num = wrapAdd(a.num, b.num); break;
            case Op::SUB: v.num = wrapSub(a.num, b.num); break;
            case Op::MUL: v.num = wrapMul(a.num, b.num); break;
            case Op::DIV:
                rt_check_div(b.num, e.line);
                v.num = (b.num == -1) ? wrapNeg(a.num) : a.num / b.num;   // INT64_MIN / -1 would trap
                break;
            case Op::MOD:
                rt_check_div(b.num, e.line);
                v.num = (b.num == -1) ? 0 : a.num % b.num;
                break;
            case Op::EQ: case Op::NE: {
                bool eq = (e.lhs->type == Type::TEXT) ? rt_text_eq(a.text, b.text) != 0
                                                      : a.num == b.num;
                v.num = (e.op == Op::EQ) ? eq : !eq;
                break;
            }
            case Op::LT: v.num = a.num <  b.num; break;
            case Op::LE: v.num = a.num <= b.num; break;
            case Op::GT: v.num = a.num >  b.num; break;
            case Op::GE: v.num = a.num >= b.num; break;
            default: break;
        }
        return v;
    }

    case ExprKind::CALL:
        if (e.isCount) { v.num = eval(*e.args[0], f).row->len; return v; }
        return callRecipe(e, f);

    case ExprKind::GRAB:
        v.row = rt_grab(e.text.c_str(), e.asNum ? 1 : 0, e.line);
        return v;

    case ExprKind::ROW_NEW:
        v.row = rt_row_new(eval(*e.lhs, f).num, e.rowOfText ? 1 : 0, e.line);
        return v;
    }
    return v;
}

Flow Interp::execBlock(Block& b, Frame& f) {
    for (Stmt* s : b.stmts)
        if (exec(*s, f) == Flow::RETURN) return Flow::RETURN;     // `give` unwinds every block
    return Flow::NORMAL;
}

Flow Interp::exec(Stmt& s, Frame& f) {
    switch (s.kind) {
    case StmtKind::HOLD:
    case StmtKind::ASSIGN:
        f.slots[s.sym->slot] = eval(*s.expr, f);
        break;

    case StmtKind::INDEX_ASSIGN: {
        Row* r = f.slots[s.sym->slot].row;
        int64_t i = eval(*s.index, f).num;
        Value v = eval(*s.expr, f);
        rt_check_index(r, i, s.line);
        if (r->is_text) ((const char**)r->data)[i] = v.text;
        else            ((int64_t*)r->data)[i]     = v.num;
        break;
    }

    case StmtKind::IF:
        if (eval(*s.expr, f).num != 0) return execBlock(*s.body, f);
        if (s.elseBody)                return execBlock(*s.elseBody, f);
        break;

    case StmtKind::WHILE:
        while (eval(*s.expr, f).num != 0)
            if (execBlock(*s.body, f) == Flow::RETURN) return Flow::RETURN;
        break;

    case StmtKind::FOR: {
        int64_t a = eval(*s.expr, f).num;            // both bounds are evaluated once, up front
        int64_t b = eval(*s.expr2, f).num;
        for (int64_t i = a; i < b; ++i) {            // b is excluded; if a >= b the body never runs
            f.slots[s.sym->slot].num = i;
            if (execBlock(*s.body, f) == Flow::RETURN) return Flow::RETURN;
        }
        break;
    }

    case StmtKind::GIVE:
        ret_ = s.expr ? eval(*s.expr, f) : Value();
        return Flow::RETURN;

    case StmtKind::OPEN:
        rt_open(eval(*s.expr, f).text, s.line);
        break;

    case StmtKind::SAVE: {
        Value v = eval(*s.expr, f);
        Value p = eval(*s.expr2, f);
        switch (s.expr->type) {
            case Type::NUM:  rt_save_num(v.num, p.text, s.line);  break;
            case Type::TEXT: rt_save_text(v.text, p.text, s.line); break;
            default:         rt_save_row(v.row, p.text, s.line);   break;
        }
        break;
    }

    case StmtKind::SHOW: {
        Value v = eval(*s.expr, f);
        switch (s.expr->type) {
            case Type::NUM:  rt_show_num(v.num);   break;
            case Type::TEXT: rt_show_text(v.text); break;
            default:         rt_show_row(v.row);   break;
        }
        break;
    }

    case StmtKind::CALL:
        eval(*s.expr, f);                            // the value, if any, is discarded
        break;
    }
    return Flow::NORMAL;
}

} // namespace

int runProgram(Program& prog) {
    Interp in;
    Frame main(prog.mainFrameSize);
    if (prog.mainBody) in.execBlock(*prog.mainBody, main);
    return 0;
}
