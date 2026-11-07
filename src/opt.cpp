// src/opt.cpp - AST optimizations
// OWNER: Hareem | PHASE: 2 | STATUS: TODO (not needed for Phase 1)
//   [ ] constant folding of num expressions with literal operands (never fold / 0 or % 0)
//   [ ] remove statements that follow a give in the same block
// Run AFTER Sema (it needs the types) and only on the compile path (-o), so `--run` stays the reference.
