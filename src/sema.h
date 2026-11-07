// src/sema.h - semantic analysis
// OWNER: Aleena | PHASE: 1 | STATUS: DONE (needs to be re-checked against Hareem's final ast.h)
#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "ast.h"

// ---------------------------------------------------------------- type helpers
inline bool isRow(Type t) { return t == Type::ROW_NUM || t == Type::ROW_TEXT; }

inline Type elementType(Type t) {            // type of r[i]
    return t == Type::ROW_NUM ? Type::NUM : t == Type::ROW_TEXT ? Type::TEXT : Type::ERROR;
}

inline const char* typeName(Type t) {        // for error messages
    switch (t) {
        case Type::NUM:      return "num";
        case Type::TEXT:     return "text";
        case Type::ROW_NUM:  return "row num";
        case Type::ROW_TEXT: return "row text";
        case Type::VOID:     return "nothing";
        default:             return "<error>";
    }
}

// ---------------------------------------------------------------- symbols
// One Symbol per declared variable, parameter or recipe. The AST's variable and
// call nodes keep a Symbol* (filled in by Sema), so the interpreter and codegen
// never do name lookup again.
struct Symbol {
    enum Kind { VARIABLE, RECIPE } kind = VARIABLE;
    std::string name;
    Type type = Type::ERROR;          // variable type, or a recipe's return type (VOID = none)
    int line = 0;                     // declaration line
    bool readonly = false;            // true for a for-loop variable
    std::vector<Type> paramTypes;     // recipes only
    RecipeDef* def = nullptr;         // recipes only: the definition (the interpreter calls it)
    int slot = -1;                    // variables only: index into the frame of the enclosing recipe/main
    std::string irName;               // optional: Phase 2 alloca / function name
};

struct Scope {
    std::unordered_map<std::string, Symbol*> vars;
    bool pageOpened = false;          // copied from the parent when the scope is created
};

class SymbolTable {
public:
    // variables
    void pushScope();                 // new scope; inherits the parent's pageOpened flag
    void popScope();
    // Returns nullptr if the name is already declared in the CURRENT scope.
    Symbol* declareVar(const std::string& name, Type t, int line, bool readonly = false);
    Symbol* lookupVar(const std::string& name) const;   // innermost to outermost, or nullptr

    // recipes (one flat table)
    Symbol* declareRecipe(const std::string& name, Type ret,
                          std::vector<Type> params, int line);   // nullptr if duplicate
    Symbol* lookupRecipe(const std::string& name) const;

    // A recipe body starts with an empty scope stack, so main-program
    // variables are invisible inside it.
    void enterRecipe();
    void leaveRecipe();

    // "page opened" flag: set on the current scope only. Inner scopes inherit
    // it when created; outer scopes never see it.
    void markPageOpened();
    bool pageOpened() const;

private:
    std::vector<std::unique_ptr<Symbol>> arena_;   // owns every Symbol, so AST pointers
                                                   // stay valid after popScope()
    std::vector<Scope> scopes_;
    std::vector<std::vector<Scope>> saved_;        // main-program scopes while inside a recipe
    std::unordered_map<std::string, Symbol*> recipes_;
};

// ---------------------------------------------------------------- analysis
struct SemaError { int line; std::string message; };

class Sema {
public:
    bool analyze(Program& prog);                          // true = no errors
    const std::vector<SemaError>& errors() const { return errors_; }
    void printErrors(const std::string& filename) const;  // "file:LINE: error: msg" on stderr

private:
    void collectRecipes(Program& prog);   // pass 1
    void checkRecipe(RecipeDef& r);       // pass 2: parameters live in the body's top scope
    void checkBlock(Block& b);            // pushScope / checkStmts / popScope
    void checkStmts(Block& b);            // the statements only, no new scope
    void checkStmt(Stmt& s);
    Type checkExpr(Expr& e);              // returns the type AND stores it in the node (may be VOID)
    Type valueExpr(Expr& e);              // like checkExpr, but a no-return call is an error
    Type expectNum(Expr& e, const char* what);   // valueExpr + "<what> must be num"
    Symbol* declareName(const std::string& name, Type t, int line, bool readonly = false);
    void report(int line, const std::string& msg);   // records the error, never stops

    SymbolTable syms_;
    std::vector<SemaError> errors_;
    RecipeDef* currentRecipe_ = nullptr;  // nullptr while checking the main program
    int nextSlot_ = 0;                    // next free variable slot in the current frame
};
