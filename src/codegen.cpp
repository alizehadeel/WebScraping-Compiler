// src/codegen.cpp - LLVM IR for expressions, variables, if/while/for, short-circuit and/or, runtime calls, @main
// OWNER: Alizeh | PHASE: 2 | STATUS: DONE
//
// See codegen.h for the value representation and the evaluation order.
#include "codegen.h"

#include <cstdio>
#include <stdexcept>

#include "sema.h"

namespace {

struct NotImplemented : std::runtime_error {
    int line;
    NotImplemented(const std::string &what, int l) : std::runtime_error(what), line(l) {}
};

// Escapes a string for an LLVM c"..." literal.
std::string irEscape(const std::string &s) {
    static const char *hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if (c >= 32 && c < 127 && c != '"' && c != '\\') out += (char)c;
        else {
            out += '\\';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

const char *kRuntimeDecls =
    "%Row = type { i64, i64, ptr }\n"
    "\n"
    "declare void @rt_open(ptr, i64)\n"
    "declare ptr @rt_grab(ptr, i64, i64)\n"
    "declare ptr @rt_row_new(i64, i64, i64)\n"
    "declare void @rt_check_index(ptr, i64, i64)\n"
    "declare void @rt_check_div(i64, i64)\n"
    "declare i64 @rt_text_eq(ptr, ptr)\n"
    "declare void @rt_show_num(i64)\n"
    "declare void @rt_show_text(ptr)\n"
    "declare void @rt_show_row(ptr)\n"
    "declare void @rt_save_num(i64, ptr, i64)\n"
    "declare void @rt_save_text(ptr, ptr, i64)\n"
    "declare void @rt_save_row(ptr, ptr, i64)\n";

}  // namespace

// ============================================================== helpers

std::string CodeGen::tmp() { return "%t" + std::to_string(++tmp_); }

std::string CodeGen::label(const char *hint) { return std::string(hint) + "." + std::to_string(++label_); }

void CodeGen::emit(const std::string &instr) {
    // Code after a terminator (e.g. after a give) goes into a fresh block that no
    // branch reaches. LLVM accepts unreachable blocks; every block still ends in
    // exactly one terminator.
    if (terminated_) startBlock(label("dead"));
    body_ << "  " << instr << "\n";
}

void CodeGen::startBlock(const std::string &lbl) {
    body_ << lbl << ":\n";
    curLabel_ = lbl;
    terminated_ = false;
}

void CodeGen::br(const std::string &lbl) {
    emit("br label %" + lbl);
    terminated_ = true;
}

void CodeGen::condBr(const std::string &i1, const std::string &t, const std::string &f) {
    emit("br i1 " + i1 + ", label %" + t + ", label %" + f);
    terminated_ = true;
}

void CodeGen::ret(const std::string &typedValue) {
    emit("ret " + typedValue);
    terminated_ = true;
}

std::string CodeGen::strConst(const std::string &s) {
    for (size_t i = 0; i < strings_.size(); i++)
        if (strings_[i] == s) return "@.str." + std::to_string(i);
    strings_.push_back(s);
    return "@.str." + std::to_string(strings_.size() - 1);
}

std::string CodeGen::llType(Type t) {
    switch (t) {
        case Type::NUM: return "i64";
        case Type::VOID: return "void";
        default: return "ptr";  // text, row num, row text
    }
}

std::string CodeGen::varAddr(Symbol *sym) {
    if (sym->irName.empty()) {
        static int counter = 0;
        sym->irName = "%v" + std::to_string(++counter) + "." + sym->name;
        allocas_.push_back(sym->irName + " = alloca " + llType(sym->type));
    }
    return sym->irName;
}

std::string CodeGen::load(Type t, const std::string &addr) {
    std::string r = tmp();
    emit(r + " = load " + llType(t) + ", ptr " + addr);
    return r;
}

void CodeGen::store(Type t, const std::string &value, const std::string &addr) {
    emit("store " + llType(t) + " " + value + ", ptr " + addr);
}

std::string CodeGen::toBool(const std::string &i64) {
    std::string r = tmp();
    emit(r + " = icmp ne i64 " + i64 + ", 0");
    return r;
}

std::string CodeGen::fromBool(const std::string &i1) {
    std::string r = tmp();
    emit(r + " = zext i1 " + i1 + " to i64");
    return r;
}

void CodeGen::beginFunction() {
    body_.str("");
    body_.clear();
    allocas_.clear();
    terminated_ = false;
    curLabel_ = "entry";
}

void CodeGen::endFunction(const std::string &header) {
    functions_ << header << " {\nentry:\n";
    for (auto &a : allocas_) functions_ << "  " << a << "\n";
    functions_ << body_.str() << "}\n\n";
}

void CodeGen::notImplemented(const char *what, int line) { throw NotImplemented(what, line); }

// ============================================================== expressions

std::string CodeGen::genExpr(Expr &e) {
    switch (e.kind) {
        case ExprKind::NUMBER:
            return std::to_string(e.num);

        case ExprKind::STRING:
            return strConst(e.text);

        case ExprKind::VAR:
            return load(e.type, varAddr(e.sym));

        case ExprKind::INDEX:
            return genIndexLoad(e);

        case ExprKind::CALL:
            return e.isCount ? genCount(e) : genCall(e);

        case ExprKind::ROW_NEW:
            return genRowNew(e);

        case ExprKind::GRAB: {
            std::string r = tmp();
            emit(r + " = call ptr @rt_grab(ptr " + strConst(e.text) + ", i64 " + (e.asNum ? "1" : "0") +
                 ", " + lineArg(e.line) + ")");
            return r;
        }

        case ExprKind::UNARY: {
            std::string v = genExpr(*e.lhs);
            if (e.op == Op::NEG) {
                std::string r = tmp();
                emit(r + " = sub i64 0, " + v);  // wraps like the interpreter
                return r;
            }
            std::string c = tmp();  // not: 1 if operand is 0
            emit(c + " = icmp eq i64 " + v + ", 0");
            return fromBool(c);
        }

        case ExprKind::BINARY: {
            if (e.op == Op::AND || e.op == Op::OR) {
                // Short-circuit: and evaluates the right side only if the left is non-zero,
                // or only if the left is zero. Result is 0/1 via a phi.
                bool isAnd = e.op == Op::AND;
                std::string rhsLbl = label(isAnd ? "and.rhs" : "or.rhs");
                std::string endLbl = label(isAnd ? "and.end" : "or.end");
                std::string l = toBool(genExpr(*e.lhs));
                std::string fromLbl = lastLabel();
                if (isAnd) condBr(l, rhsLbl, endLbl);
                else condBr(l, endLbl, rhsLbl);
                startBlock(rhsLbl);
                std::string r = toBool(genExpr(*e.rhs));
                std::string rhsEnd = lastLabel();
                br(endLbl);
                startBlock(endLbl);
                std::string phi = tmp();
                emit(phi + " = phi i1 [ " + (isAnd ? "false" : "true") + ", %" + fromLbl + " ], [ " + r +
                     ", %" + rhsEnd + " ]");
                return fromBool(phi);
            }

            std::string a = genExpr(*e.lhs);
            std::string b = genExpr(*e.rhs);
            std::string r = tmp();
            switch (e.op) {
                case Op::ADD: emit(r + " = add i64 " + a + ", " + b); return r;
                case Op::SUB: emit(r + " = sub i64 " + a + ", " + b); return r;
                case Op::MUL: emit(r + " = mul i64 " + a + ", " + b); return r;
                case Op::DIV:
                case Op::MOD: {
                    // rt_check_div stops on zero. A divisor of -1 is special-cased like the
                    // interpreter (INT64_MIN / -1 would trap): x / -1 = -x, x % -1 = 0.
                    emit("call void @rt_check_div(i64 " + b + ", " + lineArg(e.line) + ")");
                    std::string m1 = tmp(), safe = tmp(), q = tmp();
                    emit(m1 + " = icmp eq i64 " + b + ", -1");
                    emit(safe + " = select i1 " + m1 + ", i64 1, i64 " + b);
                    if (e.op == Op::DIV) {
                        std::string neg = tmp();
                        emit(q + " = sdiv i64 " + a + ", " + safe);
                        emit(neg + " = sub i64 0, " + a);
                        emit(r + " = select i1 " + m1 + ", i64 " + neg + ", i64 " + q);
                    } else {
                        emit(q + " = srem i64 " + a + ", " + safe);
                        emit(r + " = select i1 " + m1 + ", i64 0, i64 " + q);
                    }
                    return r;
                }
                case Op::EQ:
                case Op::NE:
                    if (e.lhs->type == Type::TEXT) {  // texts compare by content
                        emit(r + " = call i64 @rt_text_eq(ptr " + a + ", ptr " + b + ")");
                        if (e.op == Op::EQ) return r;
                        std::string n = tmp();
                        emit(n + " = xor i64 " + r + ", 1");
                        return n;
                    }
                    emit(r + " = icmp " + (e.op == Op::EQ ? "eq" : "ne") + " i64 " + a + ", " + b);
                    return fromBool(r);
                case Op::LT: emit(r + " = icmp slt i64 " + a + ", " + b); return fromBool(r);
                case Op::LE: emit(r + " = icmp sle i64 " + a + ", " + b); return fromBool(r);
                case Op::GT: emit(r + " = icmp sgt i64 " + a + ", " + b); return fromBool(r);
                case Op::GE: emit(r + " = icmp sge i64 " + a + ", " + b); return fromBool(r);
                default: break;
            }
            break;
        }
    }
    notImplemented("expression", e.line);
}

// ============================================================== statements

void CodeGen::genBlock(Block &b) {
    for (Stmt *s : b.stmts) genStmt(*s);
}

void CodeGen::genStmt(Stmt &s) {
    switch (s.kind) {
        case StmtKind::HOLD:
        case StmtKind::ASSIGN: {
            std::string v = genExpr(*s.expr);
            store(s.sym->type, v, varAddr(s.sym));
            break;
        }

        case StmtKind::INDEX_ASSIGN:
            genIndexStore(s);
            break;

        case StmtKind::IF: {
            std::string thenLbl = label("if.then");
            std::string elseLbl = s.elseBody ? label("if.else") : "";
            std::string endLbl = label("if.end");
            std::string c = toBool(genExpr(*s.expr));
            condBr(c, thenLbl, s.elseBody ? elseLbl : endLbl);
            startBlock(thenLbl);
            genBlock(*s.body);
            if (!terminated_) br(endLbl);
            if (s.elseBody) {
                startBlock(elseLbl);
                genBlock(*s.elseBody);
                if (!terminated_) br(endLbl);
            }
            startBlock(endLbl);
            break;
        }

        case StmtKind::WHILE: {
            std::string condLbl = label("while.cond");
            std::string bodyLbl = label("while.body");
            std::string endLbl = label("while.end");
            br(condLbl);
            startBlock(condLbl);
            std::string c = toBool(genExpr(*s.expr));
            condBr(c, bodyLbl, endLbl);
            startBlock(bodyLbl);
            genBlock(*s.body);
            if (!terminated_) br(condLbl);
            startBlock(endLbl);
            break;
        }

        case StmtKind::FOR: {
            // for i from a until b: a and b evaluated once; i = a, a+1, ..., b-1.
            // The counter lives in its own alloca so the body cannot disturb it;
            // the loop variable's alloca is set from it at the start of each pass.
            std::string a = genExpr(*s.expr);
            std::string b = genExpr(*s.expr2);
            static int forCounter = 0;
            std::string ctr = "%for" + std::to_string(++forCounter) + ".i";
            allocas_.push_back(ctr + " = alloca i64");
            std::string bound = "%for" + std::to_string(forCounter) + ".until";
            allocas_.push_back(bound + " = alloca i64");
            store(Type::NUM, a, ctr);
            store(Type::NUM, b, bound);
            std::string condLbl = label("for.cond");
            std::string bodyLbl = label("for.body");
            std::string endLbl = label("for.end");
            br(condLbl);
            startBlock(condLbl);
            std::string i = load(Type::NUM, ctr);
            std::string lim = load(Type::NUM, bound);
            std::string c = tmp();
            emit(c + " = icmp slt i64 " + i + ", " + lim);
            condBr(c, bodyLbl, endLbl);
            startBlock(bodyLbl);
            store(Type::NUM, i, varAddr(s.sym));
            genBlock(*s.body);
            if (!terminated_) {
                std::string cur = load(Type::NUM, ctr);
                std::string next = tmp();
                emit(next + " = add i64 " + cur + ", 1");
                store(Type::NUM, next, ctr);
                br(condLbl);
            }
            startBlock(endLbl);
            break;
        }

        case StmtKind::GIVE:
            genGive(s);
            break;

        case StmtKind::OPEN: {
            std::string p = genExpr(*s.expr);
            emit("call void @rt_open(ptr " + p + ", " + lineArg(s.line) + ")");
            break;
        }

        case StmtKind::SAVE: {
            std::string v = genExpr(*s.expr);   // value first, then path
            std::string p = genExpr(*s.expr2);
            Type t = s.expr->type;
            const char *fn = t == Type::NUM ? "rt_save_num" : t == Type::TEXT ? "rt_save_text" : "rt_save_row";
            emit(std::string("call void @") + fn + "(" + llType(t) + " " + v + ", ptr " + p + ", " +
                 lineArg(s.line) + ")");
            break;
        }

        case StmtKind::SHOW: {
            std::string v = genExpr(*s.expr);
            Type t = s.expr->type;
            const char *fn = t == Type::NUM ? "rt_show_num" : t == Type::TEXT ? "rt_show_text" : "rt_show_row";
            emit(std::string("call void @") + fn + "(" + llType(t) + " " + v + ")");
            break;
        }

        case StmtKind::CALL:
            if (s.expr->isCount) genCount(*s.expr);  // value discarded
            else genCall(*s.expr);
            break;
    }
}

// ============================================================== program

void CodeGen::genMain(Block &mainBody) {
    currentRecipe = nullptr;
    beginFunction();
    genBlock(mainBody);
    if (!terminated_) ret("i32 0");
    endFunction("define i32 @main()");
}

std::string CodeGen::emitModule(Program &prog) {
    for (RecipeDef *r : prog.recipes) genRecipe(*r);
    genMain(*prog.mainBody);

    std::ostringstream m;
    m << "; generated by dslc\n\n" << kRuntimeDecls << "\n";
    for (size_t i = 0; i < strings_.size(); i++)
        m << "@.str." << i << " = private unnamed_addr constant [" << strings_[i].size() + 1 << " x i8] c\""
          << irEscape(strings_[i]) << "\\00\"\n";
    if (!strings_.empty()) m << "\n";
    m << functions_.str();
    return m.str();
}

int emitProgram(Program &prog, const char *path) {
    std::string ir;
    try {
        CodeGen cg;
        ir = cg.emitModule(prog);
    } catch (const NotImplemented &ni) {
        fprintf(stderr, "dslc: line %d: IR generation for %s is not implemented yet\n", ni.line, ni.what());
        return 3;
    }
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "dslc: cannot write %s\n", path);
        return 1;
    }
    fputs(ir.c_str(), f);
    fclose(f);
    return 0;
}
