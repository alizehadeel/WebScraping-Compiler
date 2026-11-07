/*
 * runtime/rt.h - runtime API contract
 * OWNER: Alizeh | PHASE: 1 | STATUS: contract copied from the work plan (Alizeh: finalise)
 *
 * The interpreter calls these functions directly; Phase 2 generated IR declares
 * and calls the same functions.
 *
 * OPEN QUESTION (settle in the first meeting): the work plan declares a row as
 * { i64 len, i64 is_text, ptr data } but the proposal says { i64 length, ptr data }.
 * The proposal wins when they disagree, so decide, then update this file and ast/codegen.
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
int   rt_selector_valid(const char *sel); /* also called by semantic analysis */

#ifdef __cplusplus
}
#endif
#endif /* RT_H */
