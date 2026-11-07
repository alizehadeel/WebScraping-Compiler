// src/interp.h - the tree-walking interpreter (the --run flag)
// OWNER: Aleena | PHASE: 1 | STATUS: DONE
#pragma once
#include "ast.h"

// Runs a program that Sema::analyze accepted (types, symbols and slots must be filled in).
// Run-time errors are reported by the runtime library (rt_*), which exits with code 1.
// Returns 0 when the program finishes normally.
int runProgram(Program& prog);
