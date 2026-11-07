// opt.h — AST optimizations (owner: Hareem)
#pragma once

#include "ast.h"

// Runs both optimizations from the proposal, in place:
//  1. constant folding of num expressions whose operands are literals
//     (60 * 60 -> 3600). Division or modulo by a literal zero is NOT folded,
//     so it stays a run-time error.
//  2. removal of statements that follow a `give` in the same block.
//
// Safe to run before or after semantic analysis. Folded nodes get
// type = Type::NUM, so a typed AST stays typed. Intended to run after
// sema and before codegen (Phase 2).
void optimize(Program *p);
