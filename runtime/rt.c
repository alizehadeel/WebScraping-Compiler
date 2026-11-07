/*
 * runtime/rt.c - the C runtime library (libxml2 based)
 * OWNER: Alizeh | PHASE: 1 | STATUS: TODO
 *
 * Checklist (from docs/PHASE1_TASKS.md); the API is in runtime/rt.h:
 *  [ ] rt_open: libxml2 htmlReadFile (recover mode, warnings off); keep one current page, free the previous
 *  [ ] selector parser for the supported subset, turned into an XPath query
 *      (descendants joined with //, a class matched with
 *       contains(concat(' ', normalize-space(@class), ' '), ' price '));
 *      rt_selector_valid uses the same parser
 *  [ ] rt_grab: text of each match, trimmed, inner whitespace collapsed; number conversion exactly as the proposal states,
 *      with its run-time error (names the selector and the text)
 *  [ ] rows (rt_row_new), rt_show_*, rt_save_*, rt_check_index, rt_check_div, rt_text_eq
 *  [ ] every run-time error prints "line N: runtime error: message" and exits with 1
 *
 * dev/rt_stub.c already implements everything except open/grab/real selector handling and can be copied as a start.
 * See docs/HANDOFF_NOTES.md for what the interpreter expects.
 */
