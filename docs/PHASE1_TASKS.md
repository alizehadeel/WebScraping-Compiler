# Phase 1 tasks (interpreter)

Source: `docs/Compiler_Project_Work_Plan.pdf`. Names are the work plan's starting suggestion; swap freely,
but keep **one owner per file**. Tick a box only when the work is merged into `main`, has tests, `make test`
passes and one other person has reviewed it.

Phase 1 = lexer, parser, AST, semantic analysis, runtime library and a **tree-walking interpreter** (`--run`).
Phase 2 (LLVM IR) starts after M2.

## Who owns which Phase 1 file

| File | Owner | Status |
|---|---|---|
| `src/lexer.l` | **Hareem** | DONE |
| `src/parser.y` | **Hareem** | DONE |
| `src/ast.h` | **Hareem** (first draft written by Aleena) | DONE - Aleena's draft kept unchanged; the parser builds it |
| `src/ast.cpp` | **Hareem** | DONE |
| `src/main.cpp` | **Hareem** | DONE |
| `Makefile` | **Hareem** | DONE |
| `tests/run.sh` | **Hareem** | DONE |
| `src/sema.h`, `src/sema.cpp` | **Aleena** | DONE |
| `src/interp.h`, `src/interp.cpp` | **Aleena** | DONE |
| `runtime/rt.h` | **Alizeh** | DONE - Row layout: the work plan's { len, is_text, data } |
| `runtime/rt.c` | **Alizeh** | DONE |
| `runtime/test_rt.c` | **Alizeh** | DONE |
| `examples/shop.html`, `examples/news.html` | **Alizeh** | DONE |
| `tests/ok/`, `tests/err/` | everyone - each person tests their own area | Aleena's tests done (see below) |

Not Phase 1 (do not start yet): `src/opt.cpp` (Hareem), `src/codegen.h`, `src/codegen.cpp` (Alizeh),
`src/codegen_recipes.cpp` (Aleena).

## Milestones

| Milestone | Hareem | Aleena | Alizeh | Done when |
|---|---|---|---|---|
| **M0 Setup** | Repo, Makefile skeleton, draft `ast.h` | Write the 4 example programs and expected output | Draft `rt.h`, write `shop.html` and `news.html` | Contracts merged; each person has added 5 error programs to `tests/err/` |
| **M1 Front end** | Lexer, parser, AST, `--ast` printer | Symbol table and type rules, written against `ast.h` | Runtime library, tested alone with a small C program | `--ast` prints a tree for every example; the C test prints the prices grabbed from `shop.html` |
| **M2 Phase 1 done** | Driver, `--check` / `--run`, test runner, syntax-error tests | All semantic checks, interpreter | Hook runtime into the interpreter, run-time error tests | Every `tests/ok` program gives its expected output under `--run`; every `tests/err` program gives its expected error |

## Hareem - front end and integration

- [x] Lexer: every keyword, identifiers, number literals (error if too large for 64 bits), text literals with `\"` `\\` `\n`, `#` comments
- [x] Lexer: longest match for `<- << <= <`, `-> -`, `>> >= >`; a lone `=` is an error
- [x] Lexer: one NL token per line that has a token, plus one at end of file if the last line has no newline; track line numbers
- [x] Parser: full grammar from the proposal, precedence with `%left` / `%nonassoc` (comparisons non-associative), **zero** shift/reduce conflicts (`bison -Wcounterexamples`)
- [x] Parser: `<< else >>` on one line, `else if` chains, syntax errors that name the line and the unexpected token
- [x] AST construction (follow `docs/HANDOFF_NOTES.md`) and the `--ast` printer
- [x] Driver `src/main.cpp` with `--ast`, `--check`, `--run` (and `-o` in Phase 2); Makefile targets `make`, `make test`, `make clean`
- [x] `tests/run.sh`: runs `tests/ok` and `tests/err` under `--run`
- [x] Syntax-error and lexical-error tests in `tests/err/`
- [x] Review `src/ast.h` and `src/sema.*` / `src/interp.*` (they were written against the draft AST)

## Aleena - semantics and interpreter

M0
- [x] 4 example programs: `isort`, `msort`, `prices`, `headlines` (in `examples/`)
- [x] Expected output for `prices.dsl` and `headlines.dsl` (added with the runtime: `examples/*.out`, `tests/ok/prices.out`, `tests/ok/headlines.out`; Aleena please double-check)
- [x] Error programs in `tests/err/` (34 programs, covering every compile-time error in the proposal and the run-time errors that need no HTML)

M1 / M2
- [x] Symbol table: stack of scopes for variables, a separate table for recipes, `count` reserved
- [x] Pass 1: collect every recipe signature; report duplicate recipes
- [x] Pass 2: declarations, assignments and the type of every expression, following the proposal's type and operator rules
- [x] Recipe checks: argument count and types, a no-return recipe used as a value, all four `give` rules
- [x] Assigning to a `for` loop variable is an error
- [x] Domain checks: selector validity (`rt_selector_valid`), no `open` / `grab` in recipes, every `grab` preceded by an `open` in the same or an enclosing block
- [x] Keep going after an error and report all of them (`ERROR` type, no cascades)
- [x] Interpreter: values, a fresh frame per call, short-circuit `and` / `or`, `for` bounds evaluated once, `give` as a return signal, default return values, all run-time checks through `rt_*`
- [x] 18 `tests/ok` programs with hand-computed `.out` files
- [x] Re-run everything with Hareem's real parser and Alizeh's real runtime: all tests pass, sanitizers clean
- [ ] README section: type rules and errors (M5)

Tested so far with the temporary harness in `dev/` (a throwaway parser and a stub runtime): all 52 tests pass,
and the sanitizers (address + undefined behaviour) are clean.

## Alizeh - runtime and core codegen

- [x] `shop.html` and `news.html` test pages (M0)
- [x] `rt_open`: libxml2 `htmlReadFile` (recover mode, warnings off); keep one current page, free the previous one
- [x] Selector parser for the supported subset, turned into an XPath query (descendants joined with `//`, a class matched with `contains(concat(' ', normalize-space(@class), ' '), ' price ')`); `rt_selector_valid` uses the same parser
- [x] `rt_grab`: text content of each match, trimmed with inner whitespace collapsed; number conversion exactly as the proposal states, with its run-time error
- [x] Rows, `rt_show_*`, `rt_save_*`, `rt_check_index`, `rt_check_div`, `rt_text_eq`; every error prints the source line and exits with 1
- [x] `runtime/test_rt.c`: a standalone test that opens both test pages and prints what it grabs
- [x] Finalise `runtime/rt.h` (including the Row layout question, see `docs/HANDOFF_NOTES.md`)
- [x] Run-time error tests in `tests/err/` for the errors that need real files (page cannot be read, `grab ... as num` with no digits, output file cannot be written)
- [ ] Risk: confirm `libxml2-dev` installs on the grading machine (install command is in SETUP.md)

## Phase 2 (after M2) - for reference

| Area | Owner | Work |
|---|---|---|
| IR for recipes, calls, `give`, rows | Aleena | `src/codegen_recipes.cpp` |
| Codegen context, expressions, variables, control flow, short-circuit, runtime calls, `@main` | Alizeh | `src/codegen.h`, `src/codegen.cpp` |
| Constant folding, unreachable-code removal, middle-end hookup, `README.pdf` | Hareem | `src/opt.cpp`, `Makefile`, test runner |
