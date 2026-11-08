// src/opt.h - AST optimizations (opt.cpp)
// OWNER: Hareem | PHASE: 2 | STATUS: DONE
#pragma once
#include "ast.h"

// Constant folding and unreachable-code removal, in place. Call after Sema
// accepted the program, and only on the -o path.
void optimize(Program &p);
