// src/codegen_recipes.cpp - LLVM IR for recipes, calls, give and rows
// OWNER: Aleena | PHASE: 2 | STATUS: TODO (not needed for Phase 1)
//
// Plan (see docs/PHASE1_TASKS.md, "Phase 2"):
//   [ ] one `define` per recipe: typed parameters (num = i64, text/row = ptr), params copied into allocas
//   [ ] calls; `give` = ret; a default ret at the end of the body (0, "" or an empty row)
//   [ ] rows: rt_row_new, rt_check_index then getelementptr into data, count = load of field 0
// Follow the evaluation order documented at the top of src/interp.cpp - the interpreter is the reference.
