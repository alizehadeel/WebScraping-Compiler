// main.cpp — dslc driver (owner: Hareem)
//
//   ./dslc prog.dsl --ast       parse and print the AST            (M1)
//   ./dslc prog.dsl --check     parse + semantic analysis only     (M2)
//   ./dslc prog.dsl --run       interpret the program              (M2, Phase 1)
//   ./dslc prog.dsl -o prog.ll  emit LLVM IR                       (M3, Phase 2)
//
// Error format (work plan, contract 4):
//   compile time:  prog.dsl:12: error: <message>      (stderr, exit 1)
//   run time:      line 12: runtime error: <message>  (stderr, exit 1)
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "frontend.h"

// TODO(M2): once Aleena's sema.h / interp lands, include it here and replace
// the "not implemented" branches below, e.g.
//   #include "sema.h"
//   if (!analyze(prog)) return 1;     // prints all errors itself
//   return interpret(prog);
// TODO(M3): #include "codegen.h" and call the emitter for -o.

const char *g_filename = "<input>";

void fatalError(int line, const char *fmt, ...) {
    fprintf(stderr, "%s:%d: error: ", g_filename, line);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static void usage() {
    fputs("usage: dslc <file.dsl> (--ast | --check | --run | -o <out.ll>)\n", stderr);
    exit(2);
}

int main(int argc, char **argv) {
    const char *input = nullptr, *outLL = nullptr;
    enum { NONE, AST, CHECK, RUN, EMIT } mode = NONE;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--ast")) mode = AST;
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
    Program *prog = parseFile(input);

    switch (mode) {
        case AST:
            printAst(prog);
            return 0;
        case CHECK:
        case RUN:
        case EMIT:
            (void)outLL;
            fprintf(stderr, "dslc: this mode is not implemented yet (needs sema/interp/codegen)\n");
            return 3;
        case NONE:
            break;
    }
    return 0;
}
