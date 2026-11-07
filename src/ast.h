// src/ast.h - AST definitions
// OWNER: Hareem | PHASE: 1 | STATUS: DRAFT written by Aleena so semantic analysis and the
// interpreter could be built and tested. Hareem: you own this file now. Change it freely,
// but tell the group (it is contract #2) and keep these promises:
//   * every node stores its source line
//   * expression nodes have a `type` that sema fills in
//   * variable and call nodes have a `sym` pointer that sema fills in
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct Symbol;      // defined in sema.h
struct RecipeDef;

// ERROR means "not typed yet" or "had an error"; sema uses it to avoid cascading errors.
enum class Type { NUM, TEXT, ROW_NUM, ROW_TEXT, VOID, ERROR };

// ----------------------------------------------------------------------------- expressions
enum class ExprKind {
    NUMBER,    // num
    STRING,    // text
    VAR,       // name
    INDEX,     // name[lhs]
    CALL,      // name(args...)      count(r) is a CALL named "count"
    UNARY,     // op lhs             op is NOT or NEG
    BINARY,    // lhs op rhs
    GRAB,      // grab "text" [as num|text]
    ROW_NEW    // row num|text of lhs
};

enum class Op { ADD, SUB, MUL, DIV, MOD, EQ, NE, LT, LE, GT, GE, AND, OR, NOT, NEG };

struct Expr {
    ExprKind kind;
    int line;
    Type type = Type::ERROR;     // filled by sema
    int64_t num = 0;             // NUMBER
    std::string text;            // STRING: the value (escapes already processed); GRAB: the selector
    std::string name;            // VAR, INDEX (the row's name), CALL (the callee)
    Op op = Op::ADD;             // UNARY, BINARY
    Expr* lhs = nullptr;         // BINARY left; UNARY operand; INDEX index; ROW_NEW size
    Expr* rhs = nullptr;         // BINARY right
    std::vector<Expr*> args;     // CALL
    bool asNum = false;          // GRAB: written "as num"
    bool rowOfText = false;      // ROW_NEW: row text (false = row num)
    Symbol* sym = nullptr;       // filled by sema: VAR, INDEX -> variable; CALL -> recipe
    bool isCount = false;        // filled by sema: this CALL is the built-in count()

    Expr(ExprKind k, int ln) : kind(k), line(ln) {}
};

// ----------------------------------------------------------------------------- statements
enum class StmtKind { HOLD, ASSIGN, INDEX_ASSIGN, IF, WHILE, FOR, GIVE, OPEN, SAVE, SHOW, CALL };

struct Block;

struct Stmt {
    StmtKind kind;
    int line;
    std::string name;            // HOLD / ASSIGN / INDEX_ASSIGN target; FOR loop variable
    Expr* index = nullptr;       // INDEX_ASSIGN: the index expression
    Expr* expr = nullptr;        // HOLD/ASSIGN/INDEX_ASSIGN: the value      IF/WHILE: the condition
                                 // FOR: the "from" value                    GIVE: the value (may be null)
                                 // OPEN: the path      SAVE: the value      SHOW: the value
                                 // CALL: the call expression (an Expr of kind CALL)
    Expr* expr2 = nullptr;       // FOR: the "until" value                   SAVE: the path
    Block* body = nullptr;       // IF: then-block      WHILE / FOR: the body
    Block* elseBody = nullptr;   // IF: else-block. For "else if" the parser wraps the nested IF
                                 // statement in a Block of its own.
    Symbol* sym = nullptr;       // filled by sema: the variable declared (HOLD, FOR) or used (ASSIGN, INDEX_ASSIGN)

    Stmt(StmtKind k, int ln) : kind(k), line(ln) {}
};

struct Block {
    std::vector<Stmt*> stmts;
    int line = 0;
};

// ----------------------------------------------------------------------------- recipes and program
struct Param {
    Type type;
    std::string name;
    int line;
    Symbol* sym = nullptr;       // filled by sema
};

struct RecipeDef {
    std::string name;
    std::vector<Param> params;
    Type retType = Type::VOID;   // VOID = no return type
    Block* body = nullptr;
    int line = 0;
    Symbol* sym = nullptr;       // filled by sema
    int frameSize = 0;           // filled by sema: number of variable slots (params + locals)
};

struct Program {
    std::vector<RecipeDef*> recipes;   // in source order
    Block* mainBody = nullptr;         // the top-level statements, in source order
    int mainFrameSize = 0;             // filled by sema
};

void printAst(const Program& prog);    // implemented by Hareem in ast.cpp (the --ast flag)
