# Hand-off notes (what each part expects from the others)

Written by Aleena while building `sema.cpp` and `interp.cpp`. These are the exact assumptions in that code.
If you change one, tell the group and run `make -C dev test` (while `dev/` still exists).

## For Hareem - building the AST

The node definitions and the comments that explain every field are in `src/ast.h`. In short:

1. **Lines.** Every node's `line` is the source line of its first token (for a binary expression, of the operator).
2. **Program.** Put recipes into `Program::recipes` in source order and every top-level statement into
   `Program::mainBody->stmts`. Recipes and statements may be mixed in any order in the file.
3. **`count(r)`** must become an ordinary `CALL` expression whose `name` is `"count"` (even if the lexer gives
   `count` its own token). Sema marks it as the built-in and rejects `hold count` and `recipe count`.
4. **`else if`.** Put the nested `IF` statement inside a `Block` and store that block in `elseBody`.
5. **Call statements** (a recipe call on its own line) are `StmtKind::CALL` with `expr` = the `CALL` expression.
6. **Field use per statement kind** is listed in the comments of `struct Stmt` (for example `SAVE`: `expr` = value,
   `expr2` = path; `FOR`: `expr` = from, `expr2` = until).
7. **Text literals** are stored with escapes already processed in `Expr::text`. A selector is the `text` of a `GRAB`.
8. **Unary minus** is a `UNARY` node with `Op::NEG` (do not fold it into the literal in the parser).
9. **Syntax errors** print `file:LINE: error: message` on stderr and exit with code 1, before semantic analysis runs.
10. **The driver** needs only this (also see the comment in `src/main.cpp`):
    ```cpp
    Sema sema;
    if (!sema.analyze(*prog)) { sema.printErrors(filename); return 1; }
    return runProgram(*prog);          // for --run
    ```
11. **Optimizations** (`opt.cpp`, Phase 2) must run after sema and only on the `-o` path, so `--run` stays the reference.
12. **`Type::ERROR`** in `ast.h` is an addition sema needs ("not typed / had an error").

## For Alizeh - what the interpreter expects from the runtime

1. **Row layout.** The work plan says `{ i64 len, i64 is_text, ptr data }`; the proposal says `{ i64 length, ptr data }`.
   The interpreter is written against the work plan's version (`runtime/rt.h`). **Decide this in the first meeting**:
   it also changes the field index codegen uses for `getelementptr`.
2. **Text rows.** The interpreter stores `const char*` pointers into `data` (string literals, other rows' elements),
   so never free or modify element strings. `rt_row_new(n, 1, ...)` must set every element to `""`, not `NULL`.
3. **Empty rows.** `rt_row_new(0, ...)` must return a valid empty row (used as the default return value).
4. **Run-time errors.** `rt_check_index`, `rt_check_div`, `rt_row_new` (negative size), `rt_open`, `rt_grab`, `rt_save_*`
   print `line N: runtime error: message` on stderr, flush stdout first, and exit with code 1. The tests match on the
   line number and a part of the message: `out of bounds`, `negative`, `zero`.
5. **`rt_selector_valid`** is called at **compile time**, before any page is open, so it must not depend on a page.
   It returns 0 for everything outside the subset: a tag, `.class`, `#id`, `tag.class`, `tag#id`, and those joined by
   spaces (descendants). For example `div > p`, `a, b`, `a[href]` and `""` are invalid.
6. **`rt_grab(sel, as_num, line)`**: results must stay valid until the program ends (the interpreter never frees them).
7. **Printing.** `rt_show_text` and `rt_show_num` print a newline; rows print one element per line (also in `rt_save_row`).
8. **`dev/rt_stub.c`** is a working version of everything except the HTML parts (and its `grab` returns fake data).
   Copy what is useful. Do not create `.out` files from programs that use the stub's `grab`.

## Language decisions made in sema (tell Aleena if the team disagrees)

- A recipe's parameters share the **same scope** as the top level of its body, so `hold n <- 1` inside a recipe with
  a parameter `n` is a "declared twice" error.
- A `for` loop variable shares the scope of the loop body for the same reason (and is read-only).
- Error messages contain fixed phrases that the error tests match on (for example `undeclared variable`,
  `needs num`, `arguments`, `preceded by an open`, `not a row`). Do not reword them without updating `tests/err/`.
- All errors are sorted by line before printing; a duplicate recipe is reported on the second definition.

## For Aleena - Phase 2 (`codegen_recipes.cpp`)

Written with the core codegen (`codegen.h` / `codegen.cpp`).

1. **Interface.** Your seven methods are declared in `src/codegen.h` and are placeholders in
   `src/codegen_recipes.cpp` (the plan for each one is at the top of that file). Until a method is real,
   `dslc -o` exits with code 3 and `tests/run.sh --compile` reports the program as SKIP.
2. **Values.** num = `i64`, text and rows = `ptr`. `genExpr()` returns the operand without its type
   (`%t12`, `42`, `@.str.3`); use `CodeGen::llType(e.type)` for the type.
3. **Variables.** `varAddr(sym)` returns the variable's alloca (created in the entry block on first use).
   For parameters: `store(param.type, "%" + param.name, varAddr(param.sym))` at the top of the body.
4. **Functions.** `beginFunction()`, then generate the body, then `endFunction("define i64 @dsl_name(i64 %n)")`.
   Name recipes with `recipeName(r)` (`@dsl_<name>`, so a recipe called `main` cannot clash with `@main`).
   Set `currentRecipe` while generating a recipe; `genGive` needs its return type.
5. **Terminators.** `ret()` and `br()` mark the block as finished; anything emitted afterwards goes into a
   fresh unreachable block automatically. At the end of a recipe body, emit the default return only
   `if (!terminated())`.
6. **Rows.** `%Row = type { i64, i64, ptr }` (len, is_text, data) is declared at the top of the module.
   count = `getelementptr %Row, ptr %r, i32 0, i32 0` + load; data = field 2.
7. **Check.** `make test` runs `tests/run.sh --compile`: every program that compiles must print exactly
   its `.out` file. 16 programs are currently skipped waiting for this file.
