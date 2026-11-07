// dev/dev_main.cpp - Aleena's TEMPORARY test driver. NOT part of the real compiler.
//
// A tiny hand-written lexer + recursive-descent parser that builds the AST from src/ast.h,
// so sema.cpp and interp.cpp can be tested before Hareem's Flex/Bison front end exists.
// Usage:  devc prog.dsl --check | --run
// Delete this folder once Hareem's parser and driver (src/main.cpp) work.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "ast.h"
#include "sema.h"
#include "interp.h"

namespace {

enum class T { ID, KW, NUM, STR, OP, NL, END };
struct Tok { T t; std::string s; int64_t n; int line; };

std::string g_file;

[[noreturn]] void fail(int line, const std::string& msg) {
    std::fprintf(stderr, "%s:%d: error: %s\n", g_file.c_str(), line, msg.c_str());
    std::exit(1);
}

bool isKeyword(const std::string& s) {
    static const char* kw[] = {"open","grab","as","hold","recipe","give","save","to","show","row","of",
                               "num","text","for","from","until","while","if","else","and","or","not"};
    for (const char* k : kw) if (s == k) return true;
    return false;
}

std::vector<Tok> lex(const std::string& src) {
    std::vector<Tok> out;
    int line = 1; size_t i = 0; bool has = false;
    auto endLine = [&]() { if (has) { out.push_back({T::NL, "", 0, line}); has = false; } };
    auto two = [&](const char* s) { return src.compare(i, 2, s) == 0; };
    while (i < src.size()) {
        char c = src[i];
        if (c == '\n') { endLine(); ++line; ++i; continue; }
        if (c == ' ' || c == '\t' || c == '\r') { ++i; continue; }
        if (c == '#') { while (i < src.size() && src[i] != '\n') ++i; continue; }
        has = true;
        if (isdigit((unsigned char)c)) {
            size_t j = i; while (j < src.size() && isdigit((unsigned char)src[j])) ++j;
            errno = 0; long long n = strtoll(src.substr(i, j - i).c_str(), nullptr, 10);
            if (errno) fail(line, "number too large");
            out.push_back({T::NUM, "", n, line}); i = j; continue;
        }
        if (isalpha((unsigned char)c) || c == '_') {
            size_t j = i; while (j < src.size() && (isalnum((unsigned char)src[j]) || src[j] == '_')) ++j;
            std::string w = src.substr(i, j - i);
            out.push_back({isKeyword(w) ? T::KW : T::ID, w, 0, line}); i = j; continue;
        }
        if (c == '"') {
            std::string v; ++i;
            while (true) {
                if (i >= src.size() || src[i] == '\n') fail(line, "unterminated text literal");
                if (src[i] == '"') { ++i; break; }
                if (src[i] == '\\' && i + 1 < src.size()) {
                    char e = src[i + 1];
                    v += (e == 'n') ? '\n' : e; i += 2;
                } else v += src[i++];
            }
            out.push_back({T::STR, v, 0, line}); continue;
        }
        const char* ops2[] = {"<-", "->", ">>", "<<", "==", "!=", "<=", ">="};
        bool done = false;
        for (const char* o : ops2) if (two(o)) { out.push_back({T::OP, o, 0, line}); i += 2; done = true; break; }
        if (done) continue;
        if (std::string("+-*/%<>()[],").find(c) != std::string::npos) {
            out.push_back({T::OP, std::string(1, c), 0, line}); ++i; continue;
        }
        fail(line, std::string("invalid character '") + c + "'");
    }
    endLine();
    out.push_back({T::END, "", 0, line});
    return out;
}

struct Parser {
    std::vector<Tok> toks; size_t p = 0;
    Tok& cur() { return toks[p]; }
    void next() { if (cur().t != T::END) ++p; }
    bool isOp(const char* s) { return cur().t == T::OP && cur().s == s; }
    bool isKw(const char* s) { return cur().t == T::KW && cur().s == s; }
    [[noreturn]] void bad() {
        std::string d = cur().t == T::NL ? "end of line" : cur().t == T::END ? "end of file" : cur().s;
        fail(cur().line, "syntax error near '" + d + "'");
    }
    void expOp(const char* s) { if (!isOp(s)) bad(); next(); }
    void expKw(const char* s) { if (!isKw(s)) bad(); next(); }
    void expNL() { if (cur().t == T::NL) next(); else if (cur().t != T::END) bad(); }
    std::string expId() { if (cur().t != T::ID) bad(); std::string s = cur().s; next(); return s; }

    Type parseType() {
        if (isKw("num"))  { next(); return Type::NUM; }
        if (isKw("text")) { next(); return Type::TEXT; }
        if (isKw("row"))  { next();
            if (isKw("num"))  { next(); return Type::ROW_NUM; }
            if (isKw("text")) { next(); return Type::ROW_TEXT; } }
        bad();
    }

    Program* parseProgram() {
        Program* prog = new Program; prog->mainBody = new Block;
        while (cur().t != T::END) {
            if (isKw("recipe")) prog->recipes.push_back(parseRecipe());
            else prog->mainBody->stmts.push_back(parseStmt());
            expNL();
        }
        return prog;
    }

    RecipeDef* parseRecipe() {
        RecipeDef* r = new RecipeDef; r->line = cur().line; next();
        r->name = expId(); expOp("(");
        if (!isOp(")")) {
            while (true) {
                Param pa; pa.line = cur().line; pa.type = parseType(); pa.name = expId();
                r->params.push_back(pa);
                if (isOp(",")) { next(); continue; }
                break;
            }
        }
        expOp(")");
        if (isOp("->")) { next(); r->retType = parseType(); }
        r->body = parseBlock();
        return r;
    }

    Block* parseBlock() {
        Block* b = new Block; b->line = cur().line;
        expOp(">>"); expNL();
        while (!isOp("<<")) {
            if (cur().t == T::END) bad();
            b->stmts.push_back(parseStmt()); expNL();
        }
        next();
        return b;
    }

    Stmt* parseIf() {
        Stmt* s = new Stmt(StmtKind::IF, cur().line); next();
        s->expr = parseOr(); s->body = parseBlock();
        if (isKw("else")) {
            next();
            if (isKw("if")) { Block* b = new Block; b->line = cur().line; b->stmts.push_back(parseIf()); s->elseBody = b; }
            else s->elseBody = parseBlock();
        }
        return s;
    }

    Stmt* parseStmt() {
        int ln = cur().line;
        if (isKw("hold")) { Stmt* s = new Stmt(StmtKind::HOLD, ln); next(); s->name = expId(); expOp("<-"); s->expr = parseOr(); return s; }
        if (isKw("if")) return parseIf();
        if (isKw("while")) { Stmt* s = new Stmt(StmtKind::WHILE, ln); next(); s->expr = parseOr(); s->body = parseBlock(); return s; }
        if (isKw("for")) {
            Stmt* s = new Stmt(StmtKind::FOR, ln); next(); s->name = expId();
            expKw("from"); s->expr = parseOr(); expKw("until"); s->expr2 = parseOr(); s->body = parseBlock(); return s;
        }
        if (isKw("give")) { Stmt* s = new Stmt(StmtKind::GIVE, ln); next(); if (cur().t != T::NL && cur().t != T::END) s->expr = parseOr(); return s; }
        if (isKw("open")) { Stmt* s = new Stmt(StmtKind::OPEN, ln); next(); s->expr = parseOr(); return s; }
        if (isKw("save")) { Stmt* s = new Stmt(StmtKind::SAVE, ln); next(); s->expr = parseOr(); expKw("to"); s->expr2 = parseOr(); return s; }
        if (isKw("show")) { Stmt* s = new Stmt(StmtKind::SHOW, ln); next(); s->expr = parseOr(); return s; }
        if (cur().t == T::ID) {
            const Tok& n = toks[p + 1];
            if (n.t == T::OP && n.s == "(") { Stmt* s = new Stmt(StmtKind::CALL, ln); s->expr = parsePrimary(); return s; }
            if (n.t == T::OP && n.s == "<-") { Stmt* s = new Stmt(StmtKind::ASSIGN, ln); s->name = expId(); next(); s->expr = parseOr(); return s; }
            if (n.t == T::OP && n.s == "[") {
                Stmt* s = new Stmt(StmtKind::INDEX_ASSIGN, ln); s->name = expId(); next();
                s->index = parseOr(); expOp("]"); expOp("<-"); s->expr = parseOr(); return s;
            }
        }
        bad();
    }

    Expr* bin(Op op, Expr* l, Expr* r, int ln) { Expr* e = new Expr(ExprKind::BINARY, ln); e->op = op; e->lhs = l; e->rhs = r; return e; }
    Expr* un(Op op, Expr* x, int ln) { Expr* e = new Expr(ExprKind::UNARY, ln); e->op = op; e->lhs = x; return e; }

    Expr* parseOr()  { Expr* l = parseAnd(); while (isKw("or"))  { int ln = cur().line; next(); l = bin(Op::OR,  l, parseAnd(), ln); } return l; }
    Expr* parseAnd() { Expr* l = parseNot(); while (isKw("and")) { int ln = cur().line; next(); l = bin(Op::AND, l, parseNot(), ln); } return l; }
    Expr* parseNot() { if (isKw("not")) { int ln = cur().line; next(); return un(Op::NOT, parseNot(), ln); } return parseCmp(); }

    bool cmpOp(Op& op) {
        if (cur().t != T::OP) return false;
        const std::string& s = cur().s;
        if (s == "==") op = Op::EQ; else if (s == "!=") op = Op::NE; else if (s == "<") op = Op::LT;
        else if (s == "<=") op = Op::LE; else if (s == ">") op = Op::GT; else if (s == ">=") op = Op::GE;
        else return false;
        return true;
    }
    Expr* parseCmp() {
        Expr* l = parseAdd(); Op op;
        if (cmpOp(op)) {
            int ln = cur().line; next(); l = bin(op, l, parseAdd(), ln);
            if (cmpOp(op)) fail(cur().line, "comparison operators cannot be chained");
        }
        return l;
    }
    Expr* parseAdd() {
        Expr* l = parseMul();
        while (isOp("+") || isOp("-")) { int ln = cur().line; Op op = isOp("+") ? Op::ADD : Op::SUB; next(); l = bin(op, l, parseMul(), ln); }
        return l;
    }
    Expr* parseMul() {
        Expr* l = parseUnary();
        while (isOp("*") || isOp("/") || isOp("%")) {
            int ln = cur().line; Op op = isOp("*") ? Op::MUL : isOp("/") ? Op::DIV : Op::MOD; next(); l = bin(op, l, parseUnary(), ln);
        }
        return l;
    }
    Expr* parseUnary() { if (isOp("-")) { int ln = cur().line; next(); return un(Op::NEG, parseUnary(), ln); } return parsePrimary(); }

    Expr* parsePrimary() {
        int ln = cur().line;
        if (cur().t == T::NUM) { Expr* e = new Expr(ExprKind::NUMBER, ln); e->num = cur().n; next(); return e; }
        if (cur().t == T::STR) { Expr* e = new Expr(ExprKind::STRING, ln); e->text = cur().s; next(); return e; }
        if (cur().t == T::ID) {
            std::string name = expId();
            if (isOp("(")) {
                Expr* e = new Expr(ExprKind::CALL, ln); e->name = name; next();
                if (!isOp(")")) while (true) { e->args.push_back(parseOr()); if (isOp(",")) { next(); continue; } break; }
                expOp(")"); return e;
            }
            if (isOp("[")) { Expr* e = new Expr(ExprKind::INDEX, ln); e->name = name; next(); e->lhs = parseOr(); expOp("]"); return e; }
            Expr* e = new Expr(ExprKind::VAR, ln); e->name = name; return e;
        }
        if (isOp("(")) { next(); Expr* e = parseOr(); expOp(")"); return e; }
        if (isKw("grab")) {
            Expr* e = new Expr(ExprKind::GRAB, ln); next();
            if (cur().t != T::STR) bad();
            e->text = cur().s; next();
            if (isKw("as")) { next(); if (isKw("num")) e->asNum = true; else if (!isKw("text")) bad(); next(); }
            return e;
        }
        if (isKw("row")) {
            Expr* e = new Expr(ExprKind::ROW_NEW, ln); next();
            if (isKw("text")) e->rowOfText = true; else if (!isKw("num")) bad();
            next(); expKw("of"); e->lhs = parseOr(); return e;
        }
        bad();
    }
};

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) { std::fprintf(stderr, "usage: %s prog.dsl --check|--run\n", argv[0]); return 2; }
    g_file = argv[1];
    std::ifstream in(g_file);
    if (!in) { std::fprintf(stderr, "cannot read %s\n", g_file.c_str()); return 2; }
    std::stringstream ss; ss << in.rdbuf();

    Parser ps; ps.toks = lex(ss.str());
    Program* prog = ps.parseProgram();

    Sema sema;
    if (!sema.analyze(*prog)) { sema.printErrors(g_file); return 1; }
    if (std::string(argv[2]) == "--run") return runProgram(*prog);
    return 0;
}
