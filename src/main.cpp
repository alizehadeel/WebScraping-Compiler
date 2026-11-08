// src/main.cpp - the dslc driver
// OWNER: Hareem | PHASE: 1 + 2 | STATUS: DONE
//
//   ./dslc prog.dsl --ast        parse and print the AST
//   ./dslc prog.dsl --ast-opt    parse, run opt.cpp, print the AST (debug aid)
//   ./dslc prog.dsl --check      parse and run semantic analysis
//   ./dslc prog.dsl --run        interpret the program
//   ./dslc prog.dsl -o prog.ll   emit LLVM IR (Phase 2)
//
// Errors: compile time  prog.dsl:12: error: <message>      (stderr, exit 1)
//         run time      line 12: runtime error: <message>  (stderr, exit 1, printed by rt_*)
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "codegen.h"
#include "frontend.h"
#include "interp.h"
#include "opt.h"
#include "sema.h"

const char *g_filename = "<input>";

void fatalError(int line, const char *fmt, ...) {
    fflush(stdout);
    fprintf(stderr, "%s:%d: error: ", g_filename, line);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static void usage() {
    fputs("usage: dslc <file.dsl> (--ast | --ast-opt | --check | --run | -o <out.ll>)\n", stderr);
    exit(2);
}

int main(int argc, char **argv) {
    const char *input = nullptr, *outLL = nullptr;
    enum { NONE, AST, AST_OPT, CHECK, RUN, EMIT } mode = NONE;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--ast")) mode = AST;
        else if (!strcmp(argv[i], "--ast-opt")) mode = AST_OPT;
        else if (!strcmp(argv[i], "--check")) mode = CHECK;
        else if (!strcmp(argv[i], "--run")) mode = RUN;
        else if (!strcmp(argv[i], "-o")) {
            if (++i >= argc) usage();
            mode = EMIT;
            outLL = argv[i];
        } else if (argv[i][0] == '-') usage();
        else if (!input) input = argv[i];
        else usage();
    }
    if (!input || mode == NONE) usage();

    g_filename = input;
    Program *prog = parseFile(input);  // lexical/syntax errors exit(1) here

    if (mode == AST) {
        printAst(*prog);
        return 0;
    }
    if (mode == AST_OPT) {
        optimize(*prog);
        printAst(*prog);
        return 0;
    }

    Sema sema;
    if (!sema.analyze(*prog)) {
        sema.printErrors(input);
        return 1;
    }
    if (mode == CHECK) return 0;
    if (mode == RUN) return runProgram(*prog);

    // EMIT: optimizations run only here, so --run stays the reference
    optimize(*prog);
    return emitProgram(*prog, outLL);
}
