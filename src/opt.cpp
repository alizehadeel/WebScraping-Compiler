// opt.cpp — constant folding and unreachable-code removal (owner: Hareem)
#include "opt.h"

#include <cstdint>

namespace {

// num arithmetic wraps on overflow (proposal, "Types and values"). Doing it in
// uint64_t avoids undefined behaviour in C++ and matches LLVM's i64 add/sub/mul.
int64_t wrapAdd(int64_t a, int64_t b) { return (int64_t)((uint64_t)a + (uint64_t)b); }
int64_t wrapSub(int64_t a, int64_t b) { return (int64_t)((uint64_t)a - (uint64_t)b); }
int64_t wrapMul(int64_t a, int64_t b) { return (int64_t)((uint64_t)a * (uint64_t)b); }

bool isNumLit(const Expr *e, int64_t &v) {
    if (e->kind != ExprKind::Num) return false;
    v = static_cast<const NumExpr *>(e)->value;
    return true;
}

Expr *makeNum(int64_t v, int line) {
    auto *n = new NumExpr(v, line);
    n->type = Type::NUM;
    return n;
}

// Returns the folded expression (possibly a new node). Children first, so
// (2 + 3) * 4 folds all the way to 20.
Expr *fold(Expr *e) {
    switch (e->kind) {
        case ExprKind::Num:
        case ExprKind::Text:
        case ExprKind::Var:
        case ExprKind::Grab:
            return e;

        case ExprKind::Index: {
            auto *x = static_cast<IndexExpr *>(e);
            x->index = fold(x->index);
            return e;
        }
        case ExprKind::Call: {
            auto *c = static_cast<CallExpr *>(e);
            for (auto &a : c->args) a = fold(a);
            return e;
        }
        case ExprKind::RowNew: {
            auto *r = static_cast<RowNewExpr *>(e);
            r->size = fold(r->size);
            return e;
        }
        case ExprKind::Unary: {
            auto *u = static_cast<UnaryExpr *>(e);
            u->operand = fold(u->operand);
            int64_t v;
            if (!isNumLit(u->operand, v)) return e;
            if (u->op == UnOp::Neg) return makeNum(wrapSub(0, v), e->line);
            return makeNum(v == 0 ? 1 : 0, e->line);  // not
        }
        case ExprKind::Binary: {
            auto *b = static_cast<BinaryExpr *>(e);
            b->lhs = fold(b->lhs);
            b->rhs = fold(b->rhs);
            int64_t l, r;
            // Only num literals: "a" == "a" is left for the runtime.
            if (!isNumLit(b->lhs, l) || !isNumLit(b->rhs, r)) return e;
            switch (b->op) {
                case BinOp::Add: return makeNum(wrapAdd(l, r), e->line);
                case BinOp::Sub: return makeNum(wrapSub(l, r), e->line);
                case BinOp::Mul: return makeNum(wrapMul(l, r), e->line);
                case BinOp::Div:
                case BinOp::Mod:
                    // Never fold x / 0 (must stay a run-time error) or
                    // INT64_MIN / -1 (overflow is undefined in C++ and LLVM).
                    if (r == 0 || (l == INT64_MIN && r == -1)) return e;
                    // C++ / truncates toward zero and % takes the sign of the
                    // left operand, exactly as the proposal specifies.
                    return makeNum(b->op == BinOp::Div ? l / r : l % r, e->line);
                case BinOp::Eq: return makeNum(l == r, e->line);
                case BinOp::Ne: return makeNum(l != r, e->line);
                case BinOp::Lt: return makeNum(l < r, e->line);
                case BinOp::Le: return makeNum(l <= r, e->line);
                case BinOp::Gt: return makeNum(l > r, e->line);
                case BinOp::Ge: return makeNum(l >= r, e->line);
                case BinOp::And: return makeNum(l != 0 && r != 0, e->line);
                case BinOp::Or: return makeNum(l != 0 || r != 0, e->line);
            }
            return e;
        }
    }
    return e;
}

void optBlock(Block *b);

void optStmt(Stmt *s) {
    switch (s->kind) {
        case StmtKind::Hold: {
            auto *h = static_cast<HoldStmt *>(s);
            h->init = fold(h->init);
            break;
        }
        case StmtKind::Assign: {
            auto *a = static_cast<AssignStmt *>(s);
            a->value = fold(a->value);
            break;
        }
        case StmtKind::IndexAssign: {
            auto *a = static_cast<IndexAssignStmt *>(s);
            a->index = fold(a->index);
            a->value = fold(a->value);
            break;
        }
        case StmtKind::If: {
            auto *i = static_cast<IfStmt *>(s);
            i->cond = fold(i->cond);
            optBlock(i->thenBlock);
            if (i->elseBlock) optBlock(i->elseBlock);
            break;
        }
        case StmtKind::While: {
            auto *w = static_cast<WhileStmt *>(s);
            w->cond = fold(w->cond);
            optBlock(w->body);
            break;
        }
        case StmtKind::For: {
            auto *f = static_cast<ForStmt *>(s);
            f->from = fold(f->from);
            f->until = fold(f->until);
            optBlock(f->body);
            break;
        }
        case StmtKind::Give: {
            auto *g = static_cast<GiveStmt *>(s);
            if (g->value) g->value = fold(g->value);
            break;
        }
        case StmtKind::Open: {
            auto *o = static_cast<OpenStmt *>(s);
            o->path = fold(o->path);
            break;
        }
        case StmtKind::Save: {
            auto *v = static_cast<SaveStmt *>(s);
            v->value = fold(v->value);
            v->path = fold(v->path);
            break;
        }
        case StmtKind::Show: {
            auto *v = static_cast<ShowStmt *>(s);
            v->value = fold(v->value);
            break;
        }
        case StmtKind::Call:
            fold(static_cast<CallStmt *>(s)->call);  // folds args in place
            break;
    }
}

void optBlock(Block *b) {
    for (size_t i = 0; i < b->stmts.size(); i++) {
        optStmt(b->stmts[i]);
        if (b->stmts[i]->kind == StmtKind::Give) {
            b->stmts.resize(i + 1);  // everything after a give is unreachable
            break;
        }
    }
}

}  // namespace

void optimize(Program *p) {
    for (auto *r : p->recipes) optBlock(r->body);
    optBlock(p->main);
}
