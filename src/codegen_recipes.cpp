// src/codegen_recipes.cpp - LLVM IR for recipes, calls, give and rows
// OWNER: Aleena | PHASE: 2 | STATUS: TODO - every function below is a placeholder
//
// The placeholders call notImplemented(), so `dslc -o` exits with code 3 and the
// test runner reports such programs as SKIP instead of FAIL. Replace each body.
// Helpers available from codegen.h (Alizeh): tmp(), label(), emit(), br(), condBr(),
// ret(), startBlock(), terminated(), varAddr(sym), load(), store(), llType(),
// lineArg(line), strConst(), genExpr(), genBlock(), beginFunction(), endFunction().
//
// Plan:
//   [ ] genRecipe: beginFunction(); "define <ret> @dsl_<name>(<typed params>)";
//       copy each param into varAddr(param.sym); genBlock(body); if !terminated()
//       ret the default (0, "" via strConst(""), or rt_row_new(0, is_text, line));
//       endFunction(header). Set currentRecipe while generating.
//   [ ] genCall: evaluate args left to right, "call <ret> @dsl_<name>(...)"
//   [ ] genGive: "ret <type> <value>" or "ret void"
//   [ ] genRowNew: call ptr @rt_row_new(i64 n, i64 is_text, i64 line)
//   [ ] genIndexLoad: index, rt_check_index, getelementptr %Row field 2, load data ptr,
//       getelementptr i64/ptr by index, load
//   [ ] genIndexStore: index, value, rt_check_index, then store (order from interp.cpp)
//   [ ] genCount: getelementptr %Row field 0, load i64
// Follow the evaluation order documented at the top of src/interp.cpp.
#include "codegen.h"

void CodeGen::genRecipe(RecipeDef &r) { notImplemented("recipes", r.line); }
std::string CodeGen::genCall(Expr &call) { notImplemented("recipe calls", call.line); }
void CodeGen::genGive(Stmt &s) { notImplemented("give", s.line); }
std::string CodeGen::genRowNew(Expr &e) { notImplemented("row ... of", e.line); }
std::string CodeGen::genIndexLoad(Expr &e) { notImplemented("row indexing", e.line); }
void CodeGen::genIndexStore(Stmt &s) { notImplemented("row element assignment", s.line); }
std::string CodeGen::genCount(Expr &e) { notImplemented("count", e.line); }
