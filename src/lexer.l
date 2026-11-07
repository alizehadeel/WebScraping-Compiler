/*
 * src/lexer.l - Flex lexer
 * OWNER: Hareem | PHASE: 1 | STATUS: TODO
 *
 * Checklist (from docs/PHASE1_TASKS.md):
 *  [ ] every keyword (open grab as hold recipe give save to show row of num text for from until
 *      while if else and or not), plus identifiers; `count` is reserved (either lex it as an ID
 *      and let sema reject `hold count`, or give it its own token - the AST wants a CALL named "count")
 *  [ ] number literals: error if too large for 64 bits
 *  [ ] text literals with \" \\ \n escapes; error on unterminated literal
 *  [ ] # comments (to end of line); blank and comment-only lines produce no NL token
 *  [ ] longest match for <- << <= <   -> -   >> >= >   ; a lone = is an error
 *  [ ] one NL token per line that has a token, plus one at end of file if the last line has no newline
 *  [ ] track line numbers (every AST node stores its line)
 *  Errors: print  file:LINE: error: message  on stderr, exit code 1.
 */
