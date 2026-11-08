/*
 * runtime/rt.c - the C runtime library (libxml2 based)
 * OWNER: Alizeh | PHASE: 1 | STATUS: DONE
 *
 * Called directly by the interpreter (Phase 1) and by the generated IR (Phase 2).
 * The API and the Row layout are in runtime/rt.h; what the interpreter expects is
 * in docs/HANDOFF_NOTES.md ("For Alizeh").
 *
 * Memory: rows and strings are never freed (programs are short-lived), so every
 * pointer handed out stays valid until the program ends. Only the previous page
 * is freed when a new one is opened.
 */
#include "rt.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>

/* ------------------------------------------------------------------ errors */

/* Every run-time error: "line N: runtime error: message" on stderr, exit 1.
   stdout is flushed first so earlier `show` output is not lost. */
static void die(int64_t line, const char *fmt, ...) {
    va_list ap;
    fflush(stdout);
    fprintf(stderr, "line %lld: runtime error: ", (long long)line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) {
        fflush(stdout);
        fputs("runtime error: out of memory\n", stderr);
        exit(1);
    }
    return p;
}

/* ------------------------------------------------------------------ rows */

Row *rt_row_new(int64_t n, int64_t is_text, int64_t line) {
    if (n < 0) die(line, "negative row size (%lld)", (long long)n);
    Row *r = xmalloc(sizeof(Row));
    r->len = n;
    r->is_text = is_text;
    if (is_text) {
        const char **d = xmalloc((size_t)n * sizeof(char *));
        for (int64_t i = 0; i < n; i++) d[i] = "";   /* never NULL (HANDOFF_NOTES 2) */
        r->data = (void *)d;
    } else {
        int64_t *d = xmalloc((size_t)n * sizeof(int64_t));
        memset(d, 0, (size_t)n * sizeof(int64_t));
        r->data = d;
    }
    return r;
}

void rt_check_index(Row *r, int64_t i, int64_t line) {
    if (i < 0 || i >= r->len)
        die(line, "row index %lld out of bounds (size %lld)", (long long)i, (long long)r->len);
}

void rt_check_div(int64_t divisor, int64_t line) {
    if (divisor == 0) die(line, "division or modulo by zero");
}

int64_t rt_text_eq(const char *a, const char *b) { return strcmp(a, b) == 0; }

/* ------------------------------------------------------------------ show / save */

static void write_row(FILE *f, Row *r) {
    for (int64_t i = 0; i < r->len; i++) {
        if (r->is_text) fprintf(f, "%s\n", ((const char **)r->data)[i]);
        else fprintf(f, "%lld\n", (long long)((int64_t *)r->data)[i]);
    }
}

void rt_show_num(int64_t v) { printf("%lld\n", (long long)v); }
void rt_show_text(const char *s) { printf("%s\n", s); }
void rt_show_row(Row *r) { write_row(stdout, r); }

static FILE *open_out(const char *path, int64_t line) {
    FILE *f = fopen(path, "w");                     /* "w" replaces the contents */
    if (!f) die(line, "cannot write file '%s'", path);
    return f;
}

static void close_out(FILE *f, const char *path, int64_t line) {
    if (fclose(f) != 0) die(line, "cannot write file '%s'", path);
}

void rt_save_num(int64_t v, const char *path, int64_t line) {
    FILE *f = open_out(path, line);
    fprintf(f, "%lld\n", (long long)v);
    close_out(f, path, line);
}

void rt_save_text(const char *s, const char *path, int64_t line) {
    FILE *f = open_out(path, line);
    fprintf(f, "%s\n", s);
    close_out(f, path, line);
}

void rt_save_row(Row *r, const char *path, int64_t line) {
    FILE *f = open_out(path, line);
    write_row(f, r);
    close_out(f, path, line);
}

/* ------------------------------------------------------------------ selectors */
/*
 * Supported subset (proposal, "Domain operations"):
 *   simple   = tag | .class | #id | tag.class | tag#id
 *   selector = simple { spaces simple }        (descendant combinator)
 * Each simple selector becomes one XPath step and steps are joined with //:
 *   "div.story h2" -> //div[contains(concat(' ', normalize-space(@class), ' '), ' story ')]//h2
 * rt_selector_valid and rt_grab share this parser, so they always agree.
 */

static int is_name_char(int c) { return isalnum(c) || c == '-' || c == '_'; }

/* Appends the XPath step for s[0..n) to out (if out != NULL). Returns 0 if invalid. */
static int simple_to_xpath(const char *s, size_t n, char *out) {
    size_t i = 0, tag_end;
    if (n == 0) return 0;
    if (isalpha((unsigned char)s[0])) {             /* tag name: letter, then letters/digits */
        while (i < n && isalnum((unsigned char)s[i])) i++;
    }
    tag_end = i;
    char kind = 0;
    size_t name_start = 0, name_end = 0;
    if (i < n && (s[i] == '.' || s[i] == '#')) {
        kind = s[i++];
        name_start = i;
        while (i < n && is_name_char((unsigned char)s[i])) i++;
        name_end = i;
        if (name_end == name_start) return 0;       /* "." or "#" with no name */
    }
    if (i != n || (tag_end == 0 && kind == 0)) return 0;   /* trailing junk, or nothing */

    if (out) {
        strcat(out, "//");
        if (tag_end) strncat(out, s, tag_end);
        else strcat(out, "*");
        if (kind == '.') {
            strcat(out, "[contains(concat(' ', normalize-space(@class), ' '), ' ");
            strncat(out, s + name_start, name_end - name_start);
            strcat(out, " ')]");
        } else if (kind == '#') {
            strcat(out, "[@id='");
            strncat(out, s + name_start, name_end - name_start);
            strcat(out, "']");
        }
    }
    return 1;
}

/* Parses a whole selector. If out != NULL it receives the XPath query (out must
   hold at least selector_xpath_size(sel) bytes). Returns 1 if valid.
   Must not depend on a page: sema calls it at compile time. */
static int parse_selector(const char *sel, char *out) {
    size_t i = 0, n = strlen(sel);
    int any = 0;
    if (out) out[0] = '\0';
    while (i < n) {
        if (sel[i] == ' ') { i++; continue; }
        if (isspace((unsigned char)sel[i])) return 0;    /* tabs/newlines: not in the subset */
        size_t st = i;
        while (i < n && sel[i] != ' ') i++;
        if (!simple_to_xpath(sel + st, i - st, out)) return 0;
        any = 1;
    }
    return any;
}

static size_t selector_xpath_size(const char *sel) {
    /* each simple selector grows by at most ~60 bytes of XPath boilerplate */
    return strlen(sel) * 4 + 64 * (strlen(sel) + 1) + 1;
}

int rt_selector_valid(const char *sel) { return parse_selector(sel, NULL); }

/* ------------------------------------------------------------------ pages */

static htmlDocPtr g_page = NULL;

void rt_open(const char *path, int64_t line) {
    FILE *f = fopen(path, "rb");
    if (!f) die(line, "cannot read page file '%s'", path);
    fclose(f);

    htmlDocPtr doc = htmlReadFile(path, NULL,
                                  HTML_PARSE_RECOVER | HTML_PARSE_NOERROR |
                                  HTML_PARSE_NOWARNING | HTML_PARSE_NONET);
    if (!doc) {
        /* libxml2 returns NULL for an empty file: treat it as an empty page */
        doc = htmlReadMemory("<html></html>", 13, path, NULL, HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
        if (!doc) die(line, "cannot read page file '%s'", path);
    }
    if (g_page) xmlFreeDoc(g_page);                 /* keep one current page */
    g_page = doc;
}

/* Trim and collapse every run of whitespace to one space. */
static char *clean_text(const char *s) {
    char *out = xmalloc(strlen(s) + 1);
    size_t o = 0;
    int pending_space = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (isspace(*p)) {
            pending_space = 1;
        } else {
            if (pending_space && o > 0) out[o++] = ' ';
            pending_space = 0;
            out[o++] = (char)*p;
        }
    }
    out[o] = '\0';
    return out;
}

/* Proposal: take the first integer in the text, ignoring commas between digits
   and dropping any fractional part: "Rs. 1,299.50" -> 1299, "-15" -> -15. */
static int64_t text_to_num(const char *text, const char *sel, int64_t line) {
    const char *p = text;
    while (*p && !isdigit((unsigned char)*p)) p++;
    if (!*p)
        die(line, "grab \"%s\" as num: element text \"%s\" contains no digits", sel, text);
    int neg = (p > text && p[-1] == '-');
    uint64_t v = 0;
    const uint64_t limit = neg ? (uint64_t)INT64_MAX + 1 : (uint64_t)INT64_MAX;
    for (;;) {
        if (isdigit((unsigned char)*p)) {
            uint64_t d = (uint64_t)(*p - '0');
            if (v > (limit - d) / 10)
                die(line, "grab \"%s\" as num: number in \"%s\" is too large for a num", sel, text);
            v = v * 10 + d;
            p++;
        } else if (*p == ',' && isdigit((unsigned char)p[1])) {
            p++;                                    /* 1,299: comma between digits */
        } else {
            break;                                  /* '.', space, end, ... */
        }
    }
    return neg ? (int64_t)(0 - v) : (int64_t)v;
}

Row *rt_grab(const char *selector, int64_t as_num, int64_t line) {
    if (!g_page) die(line, "grab with no page open");   /* sema prevents this */

    char *xpath = xmalloc(selector_xpath_size(selector));
    if (!parse_selector(selector, xpath))
        die(line, "selector \"%s\" is not in the supported subset", selector);   /* sema prevents this */

    xmlXPathContextPtr ctx = xmlXPathNewContext(g_page);
    xmlXPathObjectPtr res = ctx ? xmlXPathEvalExpression((const xmlChar *)xpath, ctx) : NULL;
    free(xpath);
    if (!res) die(line, "cannot evaluate selector \"%s\"", selector);

    /* XPath node-sets are already in document order without duplicates */
    int n = res->nodesetval ? res->nodesetval->nodeNr : 0;
    Row *r = rt_row_new(n, as_num ? 0 : 1, line);
    for (int i = 0; i < n; i++) {
        xmlChar *raw = xmlNodeGetContent(res->nodesetval->nodeTab[i]);
        char *text = clean_text(raw ? (const char *)raw : "");
        if (raw) xmlFree(raw);
        if (as_num) {
            ((int64_t *)r->data)[i] = text_to_num(text, selector, line);
            free(text);
        } else {
            ((const char **)r->data)[i] = text;    /* kept until the program ends */
        }
    }
    xmlXPathFreeObject(res);
    xmlXPathFreeContext(ctx);
    return r;
}
