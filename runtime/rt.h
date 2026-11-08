/*
 * runtime/rt.h - runtime API contract
 * OWNER: Alizeh | PHASE: 1 | STATUS: DONE
 *
 * The interpreter calls these functions directly; Phase 2 generated IR declares
 * and calls the same functions.
 *
 * Row layout (decided): the work plan's three fields, { i64 len, i64 is_text, ptr data }.
 * In IR:  %Row = type { i64, i64, ptr }
 *         count(r)  = load i64 from field 0
 *         r[i]      = load ptr from field 2, then getelementptr i64 / ptr by i
 * (The proposal's Pipeline section mentions { i64 length, ptr data }; it should be
 *  updated to match.)
 *
 * Errors: every function taking `line` reports problems as
 *   line N: runtime error: <message>       on stderr, flushes stdout, exits with 1.
 * Memory: rows and strings are never freed, so every returned pointer stays valid.
 */
#ifndef RT_H
#define RT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { int64_t len; int64_t is_text; void *data; } Row; /* data: int64_t* or char** */

void  rt_open(const char *path, int64_t line);
Row  *rt_grab(const char *selector, int64_t as_num, int64_t line);
Row  *rt_row_new(int64_t n, int64_t is_text, int64_t line);
void  rt_check_index(Row *r, int64_t i, int64_t line);
void  rt_check_div(int64_t divisor, int64_t line);
int64_t rt_text_eq(const char *a, const char *b);
void  rt_show_num(int64_t v);
void  rt_show_text(const char *s);
void  rt_show_row(Row *r);
void  rt_save_num(int64_t v, const char *path, int64_t line);
void  rt_save_text(const char *s, const char *path, int64_t line);
void  rt_save_row(Row *r, const char *path, int64_t line);
/* 1 if sel is in the supported subset: tag, .class, #id, tag.class, tag#id, and those
   separated by spaces (descendants). Needs no page: sema calls it at compile time. */
int   rt_selector_valid(const char *sel);

#ifdef __cplusplus
}
#endif
#endif /* RT_H */
