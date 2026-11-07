// src/main.cpp - the dslc driver
// OWNER: Hareem | PHASE: 1 | STATUS: TODO
//
//   [ ] ./dslc prog.dsl --ast     parse and print the AST            (needed by M1)
//   [ ] ./dslc prog.dsl --check   parse and run semantic analysis    (needed by M2)
//   [ ] ./dslc prog.dsl --run     interpret the program              (needed by M2)
//   [ ] ./dslc prog.dsl -o prog.ll   emit LLVM IR                    (Phase 2)
//
// How to call the other people's parts (this is all the driver needs):
//
//   #include "sema.h"        // Aleena
//   #include "interp.h"      // Aleena
//
//   Program* prog = ...parse the file (syntax errors: print file:LINE: error: ... and exit 1)...;
//   Sema sema;
//   if (!sema.analyze(*prog)) { sema.printErrors(filename); return 1; }   // --check stops here
//   return runProgram(*prog);                                             // --run
//
// Tip: dev/dev_main.cpp (Aleena's temporary driver) is a complete working example of exactly this.
int main() { return 0; }
