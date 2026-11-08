# DSL for static web scraping - compiler project

A small compiler for a made-up language that extracts data from saved HTML pages.
Flex + Bison front end, semantic analysis, a tree-walking interpreter (Phase 1) and an LLVM IR code generator (Phase 2).
The language is defined in `docs/Compiler_Project_Proposal_revised.docx` - **when the work plan and the proposal
disagree, the proposal wins.**

```
recipe isort(row num r, num n) >>
    for i from 1 until n >>
        hold key <- r[i]
        hold j <- i - 1
        while j >= 0 and r[j] > key >>
            r[j + 1] <- r[j]
            j <- j - 1
        <<
        r[j + 1] <- key
    <<
<<
```

## Team and ownership

Every source file starts with a header saying **OWNER**, **PHASE** and **STATUS**. One owner per file.

| Area | Owner | Phase 1 (interpreter) | Phase 2 (LLVM IR) |
|---|---|---|---|
| Front end and integration | Hareem | `lexer.l` `parser.y` `ast.h` `ast.cpp` `main.cpp` `Makefile` `tests/run.sh` | `opt.cpp`, middle-end hookup, README.pdf |
| Semantics and interpreter | Aleena | `sema.h` `sema.cpp` `interp.h` `interp.cpp` | `codegen_recipes.cpp` |
| Runtime and core codegen | Alizeh | `runtime/rt.h` `runtime/rt.c` `runtime/test_rt.c` `examples/*.html` | `codegen.h` `codegen.cpp` |

**Start here:** [`docs/PHASE1_TASKS.md`](docs/PHASE1_TASKS.md) has the checklist for each person and the milestones.
[`docs/HANDOFF_NOTES.md`](docs/HANDOFF_NOTES.md) says what each part expects from the others.

## Repository layout

```
src/        compiler (C++17): lexer.l parser.y ast.h ast.cpp main.cpp opt.cpp
            sema.h sema.cpp interp.h interp.cpp codegen.h codegen.cpp codegen_recipes.cpp
runtime/    C runtime library: rt.h rt.c test_rt.c
tests/ok/   correct programs: X.dsl + X.out (expected stdout)
tests/err/  wrong programs: first line is  # expect: <line>: <part of the message>
examples/   isort.dsl msort.dsl prices.dsl headlines.dsl shop.html news.html
dev/        TEMPORARY test harness (a throwaway parser + stub runtime) - delete when the real ones land
docs/       proposal, work plan, task list, hand-off notes
```

## Command line (when the driver exists)

| Command | Does |
|---|---|
| `./dslc prog.dsl --ast` | parse and print the AST |
| `./dslc prog.dsl --check` | parse and run semantic analysis |
| `./dslc prog.dsl --run` | interpret the program |
| `./dslc prog.dsl -o prog.ll` | emit LLVM IR (Phase 2) |

Errors: compile-time `prog.dsl:12: error: message`, run-time `line 12: runtime error: message`; both on stderr, exit code 1.

## Build and test

```
sudo apt install -y build-essential flex bison libxml2-dev clang llvm   # see SETUP.md
make            # builds ./dslc
make test       # runtime self-test (test_rt) + every program in tests/ok, tests/err, tests/opt
cd examples && ../dslc prices.dsl --run
```

`dev/` (Aleena's temporary harness) is no longer needed by the build and can be deleted.

## Working rules

- Work on a branch; merge through a pull request that one other member reviews.
- A task is done only when it is merged, has tests in `tests/`, and `make test` passes.
- Changing a contract (`ast.h`, `rt.h`, error format, command line) needs a message to the group.
- Changing the language goes into the proposal first, then the code.
