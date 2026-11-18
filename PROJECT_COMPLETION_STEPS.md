# How the project was finished: step by step

This file records everything that was done to bring the DSL web-scraping compiler
(<https://github.com/alizehadeel/WebScraping-Compiler>) to a finished, fully tested state.

---

## 1. Getting the repository

The local folder `Web Scraper Compiler` was already a clone of the GitHub repository
(`origin` = `https://github.com/alizehadeel/WebScraping-Compiler.git`), but it was
**9 commits behind** `origin/main`. Locally there was only the first commit (the proposal,
the work plan and two HTML pages).

- `git fetch origin` downloaded the team's newer work (front end, semantic analysis,
  interpreter, runtime, core code generator, optimizer, tests).
- Before pulling, I checked that the untracked local files (`rt.c`, `rt.h`, `test_rt.c`,
  `test_rt.sh` in the root folder) would not be overwritten. The repository keeps those
  files in `runtime/`, so there was no conflict.
- `git pull --ff-only` then fast-forwarded `main` to the latest commit (`1c34446`).

> The four untracked root files are **older drafts** of the runtime. They differ from the
> committed `runtime/rt.c`, `runtime/rt.h` and `runtime/test_rt.c`, and the build does not
> use them. They were left untouched. You can delete them once you have checked nothing
> in them is still needed.

## 2. Working out what was missing

Reading `README.md`, `docs/PHASE1_TASKS.md` and `docs/HANDOFF_NOTES.md` showed the state
of every part:

| Part | File(s) | Status found |
|---|---|---|
| Lexer and parser (Flex + Bison) | `src/lexer.l`, `src/parser.y`, `src/ast.*`, `src/main.cpp` | done |
| Semantic analysis | `src/sema.*` | done |
| Tree-walking interpreter (Phase 1) | `src/interp.*` | done |
| C runtime with libxml2 | `runtime/rt.*` | done |
| Optimizer (constant folding, dead code after `give`) | `src/opt.*` | done |
| Core LLVM IR generator | `src/codegen.h`, `src/codegen.cpp` | done |
| **IR for recipes, calls, `give`, rows** | **`src/codegen_recipes.cpp`** | **TODO: all 7 functions were placeholders** |

Every placeholder called `notImplemented()`, so `dslc prog.dsl -o prog.ll` stopped with exit
code 3 for any program that used a recipe, a row or `count`. The compile tests reported
**16 of the 26 test programs as SKIP**.

## 3. Implementing `src/codegen_recipes.cpp`

The rules came from `src/codegen.h` (value representation and helpers),
`docs/HANDOFF_NOTES.md` (Phase 2 section) and `src/interp.cpp`, which is the
**reference**: a compiled program counts as correct only when its output matches `--run`.

Value representation: `num` → `i64`; `text` and rows → `ptr`;
`%Row = type { i64 len, i64 is_text, ptr data }`.

| Function | What it generates |
|---|---|
| `genRecipe` | `define <ret> @dsl_<name>(<type> %arg.<p>, ...)`. Each parameter is stored into its own `alloca`, so parameters can be assigned and `mem2reg` later turns them into registers. Then the body. If the body can reach its end without a `give`, it returns the default value, the same as the interpreter: `0` for `num`, `""` for `text`, `rt_row_new(0, is_text)` for rows, `ret void` for no return type. |
| `genCall` | Arguments evaluated **left to right**, then `call <ret> @dsl_<name>(...)`. A call to a recipe with no return type returns no value. |
| `genGive` | `ret <type> <value>`, or `ret void` for a bare `give`. |
| `genRowNew` | `call ptr @rt_row_new(i64 n, i64 is_text, i64 line)`. The runtime reports a negative size. |
| `genIndexLoad` (`r[i]`) | load the row, evaluate the index, `rt_check_index` (bounds error with the line number), `getelementptr %Row … i32 2` to get `data`, then `getelementptr i64/ptr` by `i` and `load`. |
| `genIndexStore` (`r[i] <- v`) | Same as above, in the interpreter's order: **index, value, bounds check, store**. |
| `genCount` (`count(r)`) | `getelementptr %Row … i32 0` and load `len`. |

Design choices:
- Parameters are named `%arg.<name>` instead of `%<name>`. A parameter called `t1` would
  otherwise clash with the `%t1` temporaries the core codegen creates.
- Row elements are reached through `data` (field 2), so the layout matches the C struct
  `Row` in `runtime/rt.h` exactly.

Example: the IR now generated for `recipe fact(num n) -> num` (from `tests/ok/recursion.dsl`):

```llvm
define i64 @dsl_fact(i64 %arg.n) {
entry:
  %v1.n = alloca i64
  store i64 %arg.n, ptr %v1.n
  %t1 = load i64, ptr %v1.n
  %t2 = icmp sle i64 %t1, 1
  %t3 = zext i1 %t2 to i64
  %t4 = icmp ne i64 %t3, 0
  br i1 %t4, label %if.then.1, label %if.end.2
if.then.1:
  ret i64 1
if.end.2:
  %t5 = load i64, ptr %v1.n
  %t6 = load i64, ptr %v1.n
  %t7 = sub i64 %t6, 1
  %t8 = call i64 @dsl_fact(i64 %t7)
  %t9 = mul i64 %t5, %t8
  ret i64 %t9
}
```

## 4. Setting up a build environment (no sudo)

The project builds on Linux (`SETUP.md`). On this PC that means WSL Ubuntu 24.04, which had
`g++` and `make` but **not** flex, bison, libxml2-dev, clang or LLVM. `sudo apt install`
needs a password, so instead:

1. `apt-get download` (no sudo needed) fetched the official Ubuntu `.deb` packages:
   flex, bison, m4, libfl, libxml2-dev, libicu-dev, llvm-20, clang-20 and their libraries.
   The package index was out of date for libxml2-dev, so `libxml2-dev 2.9.14 …ubuntu3.10`
   was downloaded straight from `archive.ubuntu.com`. It matches the installed libxml2 2.9.14.
2. `dpkg -x` unpacked each package into `~/dsl-tools/root` (inside WSL, outside the project).
3. Small wrappers in `~/dsl-tools/bin`: `bison` (points it at its data folder and `m4`),
   `xml2-config` (prints the local include and library paths), and links for `clang`,
   `opt`, `llvm-as` and `llc`.
4. `~/dsl-tools/env.sh` adds those to `PATH` and `LD_LIBRARY_PATH`. To use them:

   ```bash
   source ~/dsl-tools/env.sh
   ```

   If you can use sudo, the normal way works too:
   `sudo apt install -y build-essential flex bison libxml2-dev clang llvm`.

## 5. A Windows line-ending problem (and the fix)

The first `make test` failed at once:

```
/usr/bin/env: 'bash\r': No such file or directory
```

Git for Windows has `core.autocrlf=true`, so it checked out `tests/run.sh` with CRLF line
endings, and bash cannot run that. To test without touching your Git settings, I cloned
the local repository a second time inside WSL (`~/dsl-work`, which gets LF endings) and
copied the new `codegen_recipes.cpp` into it.

To stop this from happening to anyone else, I added a **`.gitattributes`** file with
`*.sh text eol=lf`, so shell scripts always keep LF endings, even on Windows.

## 6. Building and testing

```bash
source ~/dsl-tools/env.sh
make            # builds ./dslc: no errors, no warnings
make test       # runtime self-test + every test in --run mode + every test in --compile mode
```

Results:

| Check | Result |
|---|---|
| Runtime self-test (`build/test_rt`) | all runtime checks passed (0 failures) |
| `tests/run.sh --run` (interpreter: 26 ok, 47 err, 2 opt) | **75 passed, 0 skipped** |
| `tests/run.sh --compile` (dslc -o → llvm-as → opt mem2reg → clang + runtime → run → compare) | **26 compiled and matched, 0 skipped** (it was 10 passed, 16 skipped) |
| Same with full optimization: `MIDDLE_END="opt -O2 -S" tests/run.sh --compile` | **26 compiled and matched** |
| The 7 run-time error programs (`tests/err/rt_*.dsl`), compiled to native code | each exits with code 1 and prints the expected `line N: runtime error: …` |
| `examples/` (`isort`, `msort`, `prices`, `headlines`) compiled with `-O2` | output identical to `examples/*.out` |

## 7. Documentation updates

- `src/codegen_recipes.cpp`: header changed to `STATUS: DONE`; the checklist is ticked.
- `docs/PHASE1_TASKS.md`: the Phase 2 row for `codegen_recipes.cpp` now says DONE.
- `docs/HANDOFF_NOTES.md`: "16 programs are currently skipped" is now "all 26 compile and match".
- `tests/run.sh`: header status changed from "compile-and-compare: TODO" to DONE.
- `.gitattributes`: new (see step 5).
- `PROJECT_COMPLETION_STEPS.md`: this file.

## 8. What is still open (not code)

These items are in the task list and are not compiler code:

- **`README.pdf`** (Hareem): the course hand-in document. `README.md` has the content to start from.
- **README section on type rules and errors** (Aleena, milestone M5).
- **Middle-end**: the course's "provided" middle-end has not arrived yet. Until it does,
  `tests/run.sh` uses `opt -passes=mem2reg` (and works with `opt -O2` as shown above).
- Confirm that `libxml2-dev` installs on the grading machine (risk listed in the task file).

## Quick reference: running a program

```bash
source ~/dsl-tools/env.sh
cd examples
../dslc prices.dsl --run                       # interpret
../dslc prices.dsl -o prices.ll                # emit LLVM IR
opt -O2 -S prices.ll -o prices.opt.ll          # middle-end
clang -Wno-override-module prices.opt.ll ../runtime/rt.c -I../runtime $(xml2-config --cflags --libs) -o prices
./prices
```
