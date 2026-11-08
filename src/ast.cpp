// src/ast.cpp - AST printer (the --ast flag)
// OWNER: Hareem | PHASE: 1 | STATUS: DONE
//
// One node per line, children indented by two spaces, `@N` = source line.
#include "ast.h"

#include <cstdio>
#include <string>

namespace {

const char *tname(Type t) {
    switch (t) {
        case Type::NUM: return "num";
        case Type::TEXT: return "text";
        case Type::ROW_NUM: return "row num";
        case Type::ROW_TEXT: return "row text";
        case Type::VOID: return "void";
        default: return "<error>";
    }
}

const char *opName(Op op) {
    switch (op) {
        case Op::ADD: return "+";
        case Op::SUB: return "-";
        case Op::MUL: return "*";
        case Op::DIV: return "/";
        case Op::MOD: return "%";
        case Op::EQ: return "==";
        case Op::NE: return "!=";
        case Op::LT: return "<";
        case Op::LE: return "<=";
        case Op::GT: return ">";
        case Op::GE: return ">=";
        case Op::AND: return "and";
        case Op::OR: return "or";
        case Op::NOT: return "not";
        case Op::NEG: return "-";
    }
    return "?";
}

void indent(int d) {
    for (int i = 0; i < d; i++) fputs("  ", stdout);
}

std::string quote(const std::string &s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else out += c;
    }
    return out + "\"";
}

void label(const char *name, int d) {
    indent(d);
    printf("%s:\n", name);
}

void printBlock(const Block *b, int d);

void printExpr(const Expr *e, int d) {
    indent(d);
    switch (e->kind) {
        case ExprKind::NUMBER:
            printf("Num %lld @%d\n", (long long)e->num, e->line);
            break;
        case ExprKind::STRING:
            printf("Text %s @%d\n", quote(e->text).c_str(), e->line);
            break;
        case ExprKind::VAR:
            printf("Var %s @%d\n", e->name.c_str(), e->line);
            break;
        case ExprKind::INDEX:
            printf("Index %s @%d\n", e->name.c_str(), e->line);
            printExpr(e->lhs, d + 1);
            break;
        case ExprKind::CALL:
            printf("Call %s @%d\n", e->name.c_str(), e->line);
            for (auto *a : e->args) printExpr(a, d + 1);
            break;
        case ExprKind::UNARY:
            printf("Unary %s @%d\n", opName(e->op), e->line);
            printExpr(e->lhs, d + 1);
            break;
        case ExprKind::BINARY:
            printf("Binary %s @%d\n", opName(e->op), e->line);
            printExpr(e->lhs, d + 1);
            printExpr(e->rhs, d + 1);
            break;
        case ExprKind::GRAB:
            printf("Grab %s as %s @%d\n", quote(e->text).c_str(), e->asNum ? "num" : "text", e->line);
            break;
        case ExprKind::ROW_NEW:
            printf("RowNew %s @%d\n", e->rowOfText ? "text" : "num", e->line);
            printExpr(e->lhs, d + 1);
            break;
    }
}

void printStmt(const Stmt *s, int d) {
    indent(d);
    switch (s->kind) {
        case StmtKind::HOLD:
            printf("Hold %s @%d\n", s->name.c_str(), s->line);
            printExpr(s->expr, d + 1);
            break;
        case StmtKind::ASSIGN:
            printf("Assign %s @%d\n", s->name.c_str(), s->line);
            printExpr(s->expr, d + 1);
            break;
        case StmtKind::INDEX_ASSIGN:
            printf("IndexAssign %s @%d\n", s->name.c_str(), s->line);
            label("index", d + 1);
            printExpr(s->index, d + 2);
            label("value", d + 1);
            printExpr(s->expr, d + 2);
            break;
        case StmtKind::IF:
            printf("If @%d\n", s->line);
            label("cond", d + 1);
            printExpr(s->expr, d + 2);
            label("then", d + 1);
            printBlock(s->body, d + 2);
            if (s->elseBody) {
                label("else", d + 1);
                printBlock(s->elseBody, d + 2);
            }
            break;
        case StmtKind::WHILE:
            printf("While @%d\n", s->line);
            label("cond", d + 1);
            printExpr(s->expr, d + 2);
            label("body", d + 1);
            printBlock(s->body, d + 2);
            break;
        case StmtKind::FOR:
            printf("For %s @%d\n", s->name.c_str(), s->line);
            label("from", d + 1);
            printExpr(s->expr, d + 2);
            label("until", d + 1);
            printExpr(s->expr2, d + 2);
            label("body", d + 1);
            printBlock(s->body, d + 2);
            break;
        case StmtKind::GIVE:
            printf("Give @%d\n", s->line);
            if (s->expr) printExpr(s->expr, d + 1);
            break;
        case StmtKind::OPEN:
            printf("Open @%d\n", s->line);
            printExpr(s->expr, d + 1);
            break;
        case StmtKind::SAVE:
            printf("Save @%d\n", s->line);
            label("value", d + 1);
            printExpr(s->expr, d + 2);
            label("to", d + 1);
            printExpr(s->expr2, d + 2);
            break;
        case StmtKind::SHOW:
            printf("Show @%d\n", s->line);
            printExpr(s->expr, d + 1);
            break;
        case StmtKind::CALL:
            printf("CallStmt @%d\n", s->line);
            printExpr(s->expr, d + 1);
            break;
    }
}

void printBlock(const Block *b, int d) {
    if (b->stmts.empty()) {
        indent(d);
        puts("(empty)");
    }
    for (auto *s : b->stmts) printStmt(s, d);
}

}  // namespace

void printAst(const Program &p) {
    puts("Program");
    for (auto *r : p.recipes) {
        indent(1);
        printf("Recipe %s(", r->name.c_str());
        for (size_t i = 0; i < r->params.size(); i++)
            printf("%s%s %s", i ? ", " : "", tname(r->params[i].type), r->params[i].name.c_str());
        printf(") -> %s @%d\n", tname(r->retType), r->line);
        printBlock(r->body, 2);
    }
    indent(1);
    puts("Main");
    printBlock(p.mainBody, 2);
}
