// src/codegen.h - LLVM IR generation: context and helpers
// OWNER: Alizeh | PHASE: 2 | STATUS: DONE (Aleena's methods are declared here, implemented in codegen_recipes.cpp)
//
// Emits textual LLVM IR (opaque pointers, LLVM 15+). No LLVM C++ API is used.
//
// Value representation (same as the interpreter, see runtime/rt.h):
//   num  -> i64        text -> ptr (C string)        row num / row text -> ptr to Row
//   %Row = type { i64, i64, ptr }      ; len, is_text, data
//   conditions: any i64, true when != 0 (icmp ne 0); comparisons give i64 0/1 (icmp + zext)
//
// Every variable (params included) lives in an alloca in the function's entry
// block; the middle-end (mem2reg) turns them into registers.
//
// Evaluation order must match src/interp.cpp (the reference):
//   binary operators left then right (and/or short-circuit), call arguments left to right,
//   r[i] <- v: index, value, bounds check, store;  save v to p: value then path;
//   for: from, then until, each once.
#pragma once

#include <sstream>
#include <string>
#include <vector>

#include "ast.h"

struct Symbol;

class CodeGen {
public:
    // Whole program -> LLVM IR module text. Expects a program Sema accepted
    // (types and symbols filled in); opt.cpp may already have run.
    std::string emitModule(Program &prog);

    // ------------------------------------------------------------------ helpers (Alizeh, codegen.cpp)
    std::string tmp();                                // fresh SSA name: %t12
    std::string label(const char *hint);              // fresh label name: while.cond.3 (no %)
    void emit(const std::string &instr);              // one instruction in the current block
    void startBlock(const std::string &lbl);          // closes nothing; previous block must be terminated
    void br(const std::string &lbl);                  // unconditional branch (terminator)
    void condBr(const std::string &i1, const std::string &t, const std::string &f);
    void ret(const std::string &typedValue);          // "ret i64 %t3", "ret void", ... (terminator)
    bool terminated() const { return terminated_; }   // current block already has a terminator
    std::string lastLabel() const { return curLabel_; }  // label of the block being filled (for phi)
    std::string strConst(const std::string &s);       // @.str.N (pooled), an i8 array global
    static std::string llType(Type t);                // num -> i64, text/rows -> ptr, VOID -> void
    std::string varAddr(Symbol *sym);                 // the variable's alloca (created on first use)
    std::string load(Type t, const std::string &addr);
    void store(Type t, const std::string &value, const std::string &addr);
    std::string toBool(const std::string &i64);       // icmp ne i64 x, 0  -> i1
    std::string fromBool(const std::string &i1);      // zext i1 -> i64
    std::string lineArg(int line) const { return "i64 " + std::to_string(line); }

    // Function framing used by @main (Alizeh) and recipes (Aleena)
    void beginFunction();                             // resets allocas, body, terminated flag
    void endFunction(const std::string &header);      // writes "define ... {" entry: allocas, body, "}"

    // ------------------------------------------------------------------ core codegen (Alizeh, codegen.cpp)
    std::string genExpr(Expr &e);                     // returns the value operand (no type prefix)
    void genStmt(Stmt &s);
    void genBlock(Block &b);
    void genMain(Block &mainBody);

    // ------------------------------------------------------------------ Aleena (codegen_recipes.cpp)
    // Each returns the value operand like genExpr. If a construct is not implemented
    // yet, call notImplemented(), which makes `dslc -o` exit with code 3.
    void genRecipe(RecipeDef &r);                     // one `define` per recipe
    std::string genCall(Expr &call);                  // CALL to a recipe (not count); "" for void
    void genGive(Stmt &s);                            // give [e] -> ret
    std::string genRowNew(Expr &e);                   // row num/text of n -> rt_row_new
    std::string genIndexLoad(Expr &e);                // r[i]: rt_check_index, then load from data
    void genIndexStore(Stmt &s);                      // r[i] <- v
    std::string genCount(Expr &e);                    // count(r): load field 0
    static std::string recipeName(const RecipeDef &r) { return "@dsl_" + r.name; }

    [[noreturn]] void notImplemented(const char *what, int line);

    // the recipe currently being generated (nullptr in @main); used by genGive
    RecipeDef *currentRecipe = nullptr;

private:
    std::ostringstream body_;                         // instructions of the current function
    std::vector<std::string> allocas_;                // entry-block allocas of the current function
    std::ostringstream functions_;                    // finished functions
    std::vector<std::string> strings_;                // pooled string constants, index = N in @.str.N
    std::vector<std::string> varSyms_;                // per-function: names already allocated
    int tmp_ = 0, label_ = 0;
    bool terminated_ = false;
    std::string curLabel_ = "entry";
};

// Writes the IR for prog to path. Returns 0 on success, 3 if a construct is not
// implemented yet, 1 if the file cannot be written.
int emitProgram(Program &prog, const char *path);
