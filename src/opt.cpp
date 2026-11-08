// src/opt.cpp - AST optimizations
// OWNER: Hareem | PHASE: 2 | STATUS: DONE
//
//  1. constant folding of num expressions whose operands are literals
//     (60 * 60 -> 3600). Division or modulo by a literal zero is NOT folded,
//     so it stays a run-time error.
//  2. removal of statements that follow a `give` in the same block.
//
// Runs AFTER Sema and only on the compile path (-o), so `--run` stays the
// reference (docs/HANDOFF_NOTES.md, point 11). Folded nodes get type NUM.
#include "opt.h"

#include <cstdint>

namespace {

int64_t wrapAdd(int64_t a, int64_t b) { return (int64_t)((uint64_t)a + (uint64_t)b); }
int64_t wrapSub(int64_t a, int64_t b) { return (int64_t)((uint64_t)a - (uint64_t)b); }
int64_t wrapMul(int64_t a, int64_t b) { return (int64_t)((uint64_t)a * (uint64_t)b); }

bool isNumLit(const Expr *e, int64_t &v) {
    if (!e || e->kind != ExprKind::NUMBER) return false;
    v = e->num;
    return true;
}

Expr *makeNum(int64_t v, int line) {
    Expr *n = new Expr(ExprKind::NUMBER, line);
    n->num = v;
    n->type = Type::NUM;
    return n;
}

// Returns the folded expression (possibly a new node). Children first, so
// (2 + 3) * 4 folds all the way to 20.
Expr *fold(Expr *e) {
    if (!e) return e;
    switch (e->kind) {
        case ExprKind::NUMBER:
        case ExprKind::STRING:
        case ExprKind::VAR:
        case ExprKind::GRAB:
            return e;
        case ExprKind::INDEX:
        case ExprKind::ROW_NEW:
            e->lhs = fold(e->lhs);
            return e;
        case ExprKind::CALL:
            for (auto &a : e->args) a = fold(a);
            return e;
        case ExprKind::UNARY: {
            e->lhs = fold(e->lhs);
            int64_t v;
            if (!isNumLit(e->lhs, v)) return e;
            if (e->op == Op::NEG) return makeNum(wrapSub(0, v), e->line);
            return makeNum(v == 0 ? 1 : 0, e->line);  // not
        }
        case ExprKind::BINARY: {
            e->lhs = fold(e->lhs);
            e->rhs = fold(e->rhs);
            int64_t l, r;
            // Only num literals: "a" == "a" is left for the runtime.
            if (!isNumLit(e->lhs, l) || !isNumLit(e->rhs, r)) return e;
            switch (e->op) {
                case Op::ADD: return makeNum(wrapAdd(l, r), e->line);
                case Op::SUB: return makeNum(wrapSub(l, r), e->line);
                case Op::MUL: return makeNum(wrapMul(l, r), e->line);
                case Op::DIV:
                case Op::MOD:
                    // Never fold x / 0 (must stay a run-time error) or
                    // INT64_MIN / -1 (overflow is undefined in C++).
                    if (r == 0 || (l == INT64_MIN && r == -1)) return e;
                    // C++ / truncates toward zero and % takes the sign of the
                    // left operand, exactly as the proposal specifies.
                    return makeNum(e->op == Op::DIV ? l / r : l % r, e->line);
                case Op::EQ: return makeNum(l == r, e->line);
                case Op::NE: return makeNum(l != r, e->line);
                case Op::LT: return makeNum(l < r, e->line);
                case Op::LE: return makeNum(l <= r, e->line);
                case Op::GT: return makeNum(l > r, e->line);
                case Op::GE: return makeNum(l >= r, e->line);
                case Op::AND: return makeNum(l != 0 && r != 0, e->line);
                case Op::OR: return makeNum(l != 0 || r != 0, e->line);
                default: return e;
            }
        }
    }
    return e;
}

void optBlock(Block *b) {
    if (!b) return;
    for (size_t i = 0; i < b->stmts.size(); i++) {
        Stmt *s = b->stmts[i];
        s->index = fold(s->index);
        s->expr = fold(s->expr);
        s->expr2 = fold(s->expr2);
        optBlock(s->body);
        optBlock(s->elseBody);
        if (s->kind == StmtKind::GIVE) {
            b->stmts.resize(i + 1);  // everything after a give is unreachable
            break;
        }
    }
}

}  // namespace

void optimize(Program &p) {
    for (auto *r : p.recipes) optBlock(r->body);
    optBlock(p.mainBody);
}
