// ast.cpp — names and the --ast printer (owner: Hareem)
#include "ast.h"

#include <cstdio>

const char *typeName(Type t) {
    switch (t) {
        case Type::NUM: return "num";
        case Type::TEXT: return "text";
        case Type::ROW_NUM: return "row num";
        case Type::ROW_TEXT: return "row text";
        case Type::VOID: return "void";
        case Type::ERROR: return "<error>";
    }
    return "?";
}

const char *opName(UnOp op) { return op == UnOp::Neg ? "-" : "not"; }

const char *opName(BinOp op) {
    switch (op) {
        case BinOp::Add: return "+";
        case BinOp::Sub: return "-";
        case BinOp::Mul: return "*";
        case BinOp::Div: return "/";
        case BinOp::Mod: return "%";
        case BinOp::Eq: return "==";
        case BinOp::Ne: return "!=";
        case BinOp::Lt: return "<";
        case BinOp::Le: return "<=";
        case BinOp::Gt: return ">";
        case BinOp::Ge: return ">=";
        case BinOp::And: return "and";
        case BinOp::Or: return "or";
    }
    return "?";
}

// ---------------------------------------------------------------- printer
// One node per line, children indented by two spaces, `@N` = source line.

static void indent(int d) {
    for (int i = 0; i < d; i++) fputs("  ", stdout);
}

static std::string quote(const std::string &s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else out += c;
    }
    return out + "\"";
}

static void printExpr(const Expr *e, int d);
static void printBlock(const Block *b, int d);

static void label(const char *name, int d) {
    indent(d);
    printf("%s:\n", name);
}

static void printExpr(const Expr *e, int d) {
    indent(d);
    switch (e->kind) {
        case ExprKind::Num:
            printf("Num %lld @%d\n", (long long)static_cast<const NumExpr *>(e)->value, e->line);
            break;
        case ExprKind::Text:
            printf("Text %s @%d\n", quote(static_cast<const TextExpr *>(e)->value).c_str(), e->line);
            break;
        case ExprKind::Var:
            printf("Var %s @%d\n", static_cast<const VarExpr *>(e)->name.c_str(), e->line);
            break;
        case ExprKind::Index: {
            auto *x = static_cast<const IndexExpr *>(e);
            printf("Index %s @%d\n", x->name.c_str(), e->line);
            printExpr(x->index, d + 1);
            break;
        }
        case ExprKind::Call: {
            auto *c = static_cast<const CallExpr *>(e);
            printf("Call %s @%d\n", c->name.c_str(), e->line);
            for (auto *a : c->args) printExpr(a, d + 1);
            break;
        }
        case ExprKind::Unary: {
            auto *u = static_cast<const UnaryExpr *>(e);
            printf("Unary %s @%d\n", opName(u->op), e->line);
            printExpr(u->operand, d + 1);
            break;
        }
        case ExprKind::Binary: {
            auto *b = static_cast<const BinaryExpr *>(e);
            printf("Binary %s @%d\n", opName(b->op), e->line);
            printExpr(b->lhs, d + 1);
            printExpr(b->rhs, d + 1);
            break;
        }
        case ExprKind::Grab: {
            auto *g = static_cast<const GrabExpr *>(e);
            printf("Grab %s as %s @%d\n", quote(g->selector).c_str(), g->asNum ? "num" : "text",
                   e->line);
            break;
        }
        case ExprKind::RowNew: {
            auto *r = static_cast<const RowNewExpr *>(e);
            printf("RowNew %s @%d\n", r->isText ? "text" : "num", e->line);
            printExpr(r->size, d + 1);
            break;
        }
    }
}

static void printStmt(const Stmt *s, int d) {
    indent(d);
    switch (s->kind) {
        case StmtKind::Hold: {
            auto *h = static_cast<const HoldStmt *>(s);
            printf("Hold %s @%d\n", h->name.c_str(), s->line);
            printExpr(h->init, d + 1);
            break;
        }
        case StmtKind::Assign: {
            auto *a = static_cast<const AssignStmt *>(s);
            printf("Assign %s @%d\n", a->name.c_str(), s->line);
            printExpr(a->value, d + 1);
            break;
        }
        case StmtKind::IndexAssign: {
            auto *a = static_cast<const IndexAssignStmt *>(s);
            printf("IndexAssign %s @%d\n", a->name.c_str(), s->line);
            label("index", d + 1);
            printExpr(a->index, d + 2);
            label("value", d + 1);
            printExpr(a->value, d + 2);
            break;
        }
        case StmtKind::If: {
            auto *i = static_cast<const IfStmt *>(s);
            printf("If @%d\n", s->line);
            label("cond", d + 1);
            printExpr(i->cond, d + 2);
            label("then", d + 1);
            printBlock(i->thenBlock, d + 2);
            if (i->elseBlock) {
                label(i->elseIsIf ? "else-if" : "else", d + 1);
                printBlock(i->elseBlock, d + 2);
            }
            break;
        }
        case StmtKind::While: {
            auto *w = static_cast<const WhileStmt *>(s);
            printf("While @%d\n", s->line);
            label("cond", d + 1);
            printExpr(w->cond, d + 2);
            label("body", d + 1);
            printBlock(w->body, d + 2);
            break;
        }
        case StmtKind::For: {
            auto *f = static_cast<const ForStmt *>(s);
            printf("For %s @%d\n", f->var.c_str(), s->line);
            label("from", d + 1);
            printExpr(f->from, d + 2);
            label("until", d + 1);
            printExpr(f->until, d + 2);
            label("body", d + 1);
            printBlock(f->body, d + 2);
            break;
        }
        case StmtKind::Give: {
            auto *g = static_cast<const GiveStmt *>(s);
            printf("Give @%d\n", s->line);
            if (g->value) printExpr(g->value, d + 1);
            break;
        }
        case StmtKind::Open:
            printf("Open @%d\n", s->line);
            printExpr(static_cast<const OpenStmt *>(s)->path, d + 1);
            break;
        case StmtKind::Save: {
            auto *v = static_cast<const SaveStmt *>(s);
            printf("Save @%d\n", s->line);
            label("value", d + 1);
            printExpr(v->value, d + 2);
            label("to", d + 1);
            printExpr(v->path, d + 2);
            break;
        }
        case StmtKind::Show:
            printf("Show @%d\n", s->line);
            printExpr(static_cast<const ShowStmt *>(s)->value, d + 1);
            break;
        case StmtKind::Call:
            printf("CallStmt @%d\n", s->line);
            printExpr(static_cast<const CallStmt *>(s)->call, d + 1);
            break;
    }
}

static void printBlock(const Block *b, int d) {
    if (b->stmts.empty()) {
        indent(d);
        puts("(empty)");
    }
    for (auto *s : b->stmts) printStmt(s, d);
}

void printAst(const Program *p) {
    puts("Program");
    for (auto *r : p->recipes) {
        indent(1);
        printf("Recipe %s(", r->name.c_str());
        for (size_t i = 0; i < r->params.size(); i++)
            printf("%s%s %s", i ? ", " : "", typeName(r->params[i]->type),
                   r->params[i]->name.c_str());
        printf(") -> %s @%d\n", typeName(r->ret), r->line);
        printBlock(r->body, 2);
    }
    indent(1);
    puts("Main");
    printBlock(p->main, 2);
}
