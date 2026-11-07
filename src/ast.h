// ast.h — AST for the web-scraping DSL (owner: Hareem, reviewed by all)
//
// Contract (work plan, section 2):
//   * every node stores its source line
//   * every expression has a `type` that semantic analysis fills in
//   * variable / call nodes carry a pointer that semantic analysis fills in,
//     so the interpreter and codegen never repeat name lookup
//
// Usage pattern for later passes:
//     switch (e->kind) {
//       case ExprKind::Binary: { auto *b = static_cast<BinaryExpr *>(e); ... }
//     }
//
// Memory: nodes are allocated with `new` and never freed (the compiler is a
// short-lived process), so passes may keep raw pointers freely.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

// ---------------------------------------------------------------- types
enum class Type { NUM, TEXT, ROW_NUM, ROW_TEXT, VOID, ERROR };

const char *typeName(Type t);  // "num", "text", "row num", ...

// Defined by semantic analysis (sema.h). The front end only stores nullptr.
struct Symbol;
struct Recipe;

// ---------------------------------------------------------------- base
struct Node {
    int line;
    explicit Node(int l) : line(l) {}
    virtual ~Node() = default;
};

// ---------------------------------------------------------------- expressions
enum class ExprKind { Num, Text, Var, Index, Call, Unary, Binary, Grab, RowNew };

enum class UnOp { Neg, Not };
enum class BinOp { Add, Sub, Mul, Div, Mod, Eq, Ne, Lt, Le, Gt, Ge, And, Or };

const char *opName(UnOp op);   // "-", "not"
const char *opName(BinOp op);  // "+", "==", "and", ...

struct Expr : Node {
    ExprKind kind;
    Type type = Type::VOID;  // filled in by semantic analysis
    Expr(ExprKind k, int l) : Node(l), kind(k) {}
};

struct NumExpr : Expr {  // 500
    int64_t value;
    NumExpr(int64_t v, int l) : Expr(ExprKind::Num, l), value(v) {}
};

struct TextExpr : Expr {  // "abc"   (escapes already decoded)
    std::string value;
    TextExpr(std::string v, int l) : Expr(ExprKind::Text, l), value(std::move(v)) {}
};

struct VarExpr : Expr {  // x
    std::string name;
    Symbol *sym = nullptr;  // sema
    VarExpr(std::string n, int l) : Expr(ExprKind::Var, l), name(std::move(n)) {}
};

struct IndexExpr : Expr {  // r[i]
    std::string name;
    Expr *index;
    Symbol *sym = nullptr;  // sema
    IndexExpr(std::string n, Expr *i, int l)
        : Expr(ExprKind::Index, l), name(std::move(n)), index(i) {}
};

struct CallExpr : Expr {  // f(a, b)   — also count(r); sema decides
    std::string name;
    std::vector<Expr *> args;
    Recipe *target = nullptr;  // sema; stays nullptr for the built-in count
    CallExpr(std::string n, std::vector<Expr *> a, int l)
        : Expr(ExprKind::Call, l), name(std::move(n)), args(std::move(a)) {}
};

struct UnaryExpr : Expr {  // -x, not x
    UnOp op;
    Expr *operand;
    UnaryExpr(UnOp o, Expr *e, int l) : Expr(ExprKind::Unary, l), op(o), operand(e) {}
};

struct BinaryExpr : Expr {  // a + b, a and b, ...
    BinOp op;
    Expr *lhs, *rhs;
    BinaryExpr(BinOp o, Expr *a, Expr *b, int l)
        : Expr(ExprKind::Binary, l), op(o), lhs(a), rhs(b) {}
};

struct GrabExpr : Expr {  // grab "span.price" as num
    std::string selector;
    bool asNum;
    GrabExpr(std::string s, bool n, int l)
        : Expr(ExprKind::Grab, l), selector(std::move(s)), asNum(n) {}
};

struct RowNewExpr : Expr {  // row num of n
    bool isText;
    Expr *size;
    RowNewExpr(bool t, Expr *s, int l) : Expr(ExprKind::RowNew, l), isText(t), size(s) {}
};

// ---------------------------------------------------------------- statements
enum class StmtKind { Hold, Assign, IndexAssign, If, While, For, Give, Open, Save, Show, Call };

struct Stmt : Node {
    StmtKind kind;
    Stmt(StmtKind k, int l) : Node(l), kind(k) {}
};

// A >> ... << block. Every block is a new scope.
struct Block : Node {
    std::vector<Stmt *> stmts;
    explicit Block(int l) : Node(l) {}
};

struct HoldStmt : Stmt {  // hold x <- e
    std::string name;
    Expr *init;
    Symbol *sym = nullptr;  // sema
    HoldStmt(std::string n, Expr *e, int l)
        : Stmt(StmtKind::Hold, l), name(std::move(n)), init(e) {}
};

struct AssignStmt : Stmt {  // x <- e
    std::string name;
    Expr *value;
    Symbol *sym = nullptr;  // sema
    AssignStmt(std::string n, Expr *e, int l)
        : Stmt(StmtKind::Assign, l), name(std::move(n)), value(e) {}
};

struct IndexAssignStmt : Stmt {  // r[i] <- e
    std::string name;
    Expr *index, *value;
    Symbol *sym = nullptr;  // sema
    IndexAssignStmt(std::string n, Expr *i, Expr *e, int l)
        : Stmt(StmtKind::IndexAssign, l), name(std::move(n)), index(i), value(e) {}
};

// if c >> .. << else >> .. <<
// `else if` is stored as an elseBlock holding exactly one IfStmt, so later
// passes only ever deal with blocks. elseBlock is nullptr when there is no else.
struct IfStmt : Stmt {
    Expr *cond;
    Block *thenBlock;
    Block *elseBlock;
    bool elseIsIf = false;  // true when elseBlock is a synthetic `else if` wrapper
    IfStmt(Expr *c, Block *t, Block *e, int l)
        : Stmt(StmtKind::If, l), cond(c), thenBlock(t), elseBlock(e) {}
};

struct WhileStmt : Stmt {
    Expr *cond;
    Block *body;
    WhileStmt(Expr *c, Block *b, int l) : Stmt(StmtKind::While, l), cond(c), body(b) {}
};

struct ForStmt : Stmt {  // for i from a until b >> .. <<
    std::string var;
    Expr *from, *until;
    Block *body;
    Symbol *sym = nullptr;  // sema: the loop variable
    ForStmt(std::string v, Expr *f, Expr *u, Block *b, int l)
        : Stmt(StmtKind::For, l), var(std::move(v)), from(f), until(u), body(b) {}
};

struct GiveStmt : Stmt {  // give [e]
    Expr *value;  // nullptr for a bare give
    GiveStmt(Expr *e, int l) : Stmt(StmtKind::Give, l), value(e) {}
};

struct OpenStmt : Stmt {
    Expr *path;
    OpenStmt(Expr *p, int l) : Stmt(StmtKind::Open, l), path(p) {}
};

struct SaveStmt : Stmt {  // save v to p
    Expr *value, *path;
    SaveStmt(Expr *v, Expr *p, int l) : Stmt(StmtKind::Save, l), value(v), path(p) {}
};

struct ShowStmt : Stmt {
    Expr *value;
    ShowStmt(Expr *v, int l) : Stmt(StmtKind::Show, l), value(v) {}
};

struct CallStmt : Stmt {  // f(a) on its own line
    CallExpr *call;
    CallStmt(CallExpr *c, int l) : Stmt(StmtKind::Call, l), call(c) {}
};

// ---------------------------------------------------------------- top level
struct Param : Node {
    Type type;
    std::string name;
    Symbol *sym = nullptr;  // sema
    Param(Type t, std::string n, int l) : Node(l), type(t), name(std::move(n)) {}
};

struct Recipe : Node {
    std::string name;
    std::vector<Param *> params;
    Type ret;  // Type::VOID when there is no `-> type`
    Block *body;
    Recipe(std::string n, std::vector<Param *> p, Type r, Block *b, int l)
        : Node(l), name(std::move(n)), params(std::move(p)), ret(r), body(b) {}
};

struct Program {
    std::vector<Recipe *> recipes;  // in source order
    Block *main;                    // all top-level statements, in source order
};

// ---------------------------------------------------------------- helpers
void printAst(const Program *p);  // --ast output (ast.cpp)
