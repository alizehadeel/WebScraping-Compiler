/*
 * src/parser.y - Bison grammar
 * OWNER: Hareem | PHASE: 1 | STATUS: TODO
 *
 * Checklist (from docs/PHASE1_TASKS.md):
 *  [ ] the full grammar from the proposal ("Grammar" section) building the AST in src/ast.h
 *  [ ] precedence with %left / %nonassoc (comparisons are non-associative: a < b < c is a syntax error)
 *  [ ] zero shift/reduce conflicts - check with: bison -Wcounterexamples
 *  [ ] "<< else >>" on one line, "else if" chains, syntax errors that name the line and the unexpected token
 *  [ ] follow the AST conventions in docs/HANDOFF_NOTES.md (else-if wrapping, call statements, count, ...)
 */
