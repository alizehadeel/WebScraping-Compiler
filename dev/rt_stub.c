/*
 * dev/rt_stub.c - Aleena's TEMPORARY stand-in for Alizeh's runtime (runtime/rt.c).
 * Everything except open/grab behaves like the real runtime should. grab returns FAKE data,
 * so never create a .out file from the output of a program that uses grab.
 * Delete once runtime/rt.c works.
 */
#include "rt.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void die(int64_t line, const char *fmt, ...) {
    va_list ap;
    fflush(stdout);
    fprintf(stderr, "line %lld: runtime error: ", (long long)line);
    va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

void rt_open(const char *path, int64_t line) {
    FILE *f = fopen(path, "r");
    if (!f) die(line, "cannot read page file '%s'", path);
    fclose(f);
}

Row *rt_row_new(int64_t n, int64_t is_text, int64_t line) {
    if (n < 0) die(line, "negative row size (%lld)", (long long)n);
    Row *r = malloc(sizeof(Row));
    r->len = n; r->is_text = is_text;
    if (is_text) {
        char **d = calloc(n ? n : 1, sizeof(char *));
        for (int64_t i = 0; i < n; i++) d[i] = "";
        r->data = d;
    } else {
        r->data = calloc(n ? n : 1, sizeof(int64_t));
    }
    return r;
}

Row *rt_grab(const char *selector, int64_t as_num, int64_t line) {
    (void)selector;
    if (as_num) {
        static const int64_t v[] = {1299, 450, 99, 700, 250};
        Row *r = rt_row_new(5, 0, line);
        memcpy(r->data, v, sizeof v);
        return r;
    }
    static const char *t[] = {"Hello", "Advertisement", "World"};
    Row *r = rt_row_new(3, 1, line);
    memcpy(r->data, t, sizeof t);
    return r;
}

void rt_check_index(Row *r, int64_t i, int64_t line) {
    if (i < 0 || i >= r->len)
        die(line, "row index %lld out of bounds (size %lld)", (long long)i, (long long)r->len);
}

void rt_check_div(int64_t d, int64_t line) { if (d == 0) die(line, "division or modulo by zero"); }
int64_t rt_text_eq(const char *a, const char *b) { return strcmp(a, b) == 0; }

void rt_show_num(int64_t v) { printf("%lld\n", (long long)v); }
void rt_show_text(const char *s) { printf("%s\n", s); }
void rt_show_row(Row *r) {
    for (int64_t i = 0; i < r->len; i++) {
        if (r->is_text) printf("%s\n", ((char **)r->data)[i]);
        else printf("%lld\n", (long long)((int64_t *)r->data)[i]);
    }
}

static FILE *open_out(const char *path, int64_t line) {
    FILE *f = fopen(path, "w");
    if (!f) die(line, "cannot write file '%s'", path);
    return f;
}
void rt_save_num(int64_t v, const char *path, int64_t line) { FILE *f = open_out(path, line); fprintf(f, "%lld\n", (long long)v); fclose(f); }
void rt_save_text(const char *s, const char *path, int64_t line) { FILE *f = open_out(path, line); fprintf(f, "%s\n", s); fclose(f); }
void rt_save_row(Row *r, const char *path, int64_t line) {
    FILE *f = open_out(path, line);
    for (int64_t i = 0; i < r->len; i++) {
        if (r->is_text) fprintf(f, "%s\n", ((char **)r->data)[i]);
        else fprintf(f, "%lld\n", (long long)((int64_t *)r->data)[i]);
    }
    fclose(f);
}

/* Supported subset: simple selectors (tag, .class, #id, tag.class, tag#id) separated by spaces. */
static int simple_ok(const char *s, size_t n) {
    size_t i = 0;
    if (n == 0) return 0;
    if (isalpha((unsigned char)s[i])) { while (i < n && isalnum((unsigned char)s[i])) i++; }
    if (i < n && (s[i] == '.' || s[i] == '#')) {
        i++;
        size_t st = i;
        while (i < n && (isalnum((unsigned char)s[i]) || s[i] == '-' || s[i] == '_')) i++;
        if (i == st) return 0;
    }
    return i == n;
}
int rt_selector_valid(const char *sel) {
    size_t i = 0, n = strlen(sel);
    int any = 0;
    while (i < n) {
        while (i < n && sel[i] == ' ') i++;
        size_t st = i;
        while (i < n && sel[i] != ' ') i++;
        if (i > st) { if (!simple_ok(sel + st, i - st)) return 0; any = 1; }
    }
    return any;
}
