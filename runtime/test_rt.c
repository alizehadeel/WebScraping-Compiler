/*
 * runtime/test_rt.c - standalone test of the runtime (no compiler involved)
 * OWNER: Alizeh | PHASE: 1 | STATUS: DONE
 *
 *   make test-rt      (run from the repo root; opens examples/shop.html and news.html)
 *
 * Prints what grab returns for both test pages and checks the selector parser
 * and number conversion. Exits 1 on the first failed check.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rt.h"

static int failures = 0;

static void check(int ok, const char *what) {
    printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

static void show_grab(const char *sel, int as_num) {
    Row *r = rt_grab(sel, as_num, 0);
    printf("grab \"%s\"%s -> %lld element(s)\n", sel, as_num ? " as num" : "", (long long)r->len);
    for (int64_t i = 0; i < r->len; i++) {
        if (as_num) printf("    %lld\n", (long long)((int64_t *)r->data)[i]);
        else printf("    \"%s\"\n", ((const char **)r->data)[i]);
    }
}

int main(void) {
    /* selector subset (also used by sema at compile time) */
    const char *valid[] = {"span", ".price", "#main", "span.price", "div#main",
                           "div.product span.price", "div  h2", "h2", ".price-old", "#main_2", NULL};
    const char *invalid[] = {"", "   ", "div > p", "a, b", "a[href]", ".", "#", "span.", "2div",
                             "div.a.b", "div#x.y", ":hover", "*", "a-b", NULL};
    for (int i = 0; valid[i]; i++) {
        char msg[128];
        snprintf(msg, sizeof msg, "valid selector \"%s\"", valid[i]);
        check(rt_selector_valid(valid[i]) == 1, msg);
    }
    for (int i = 0; invalid[i]; i++) {
        char msg[128];
        snprintf(msg, sizeof msg, "invalid selector \"%s\"", invalid[i]);
        check(rt_selector_valid(invalid[i]) == 0, msg);
    }

    /* shop.html */
    rt_open("examples/shop.html", 0);
    show_grab("span.price", 1);
    show_grab("span.price", 0);
    show_grab("div.product h2", 0);
    show_grab("#main h1", 0);
    show_grab(".note", 0);
    Row *p = rt_grab("span.price", 1, 0);
    int64_t expect[] = {1299, 450, 320, 2450, 150, -15};
    int same = p->len == 6;
    for (int i = 0; same && i < 6; i++) same = ((int64_t *)p->data)[i] == expect[i];
    check(same, "prices are 1299 450 320 2450 150 -15");
    check(rt_grab("span.missing", 0, 0)->len == 0, "no matches gives an empty row");
    check(rt_grab(".product", 0, 0)->len == 6, ".product matches the class among several");
    check(rt_grab(".price-old", 0, 0)->len == 2, ".price does not match price-old");

    /* news.html replaces shop.html as the current page */
    rt_open("examples/news.html", 0);
    show_grab("div.story h2", 0);
    Row *h = rt_grab("div.story h2", 0, 0);
    check(h->len == 4, "4 headlines inside div.story");
    check(h->len > 0 && strcmp(((const char **)h->data)[0], "Budget passes after long debate") == 0,
          "whitespace is trimmed and collapsed");
    check(rt_grab("span.price", 0, 0)->len == 0, "the previous page is gone after open");

    /* rows */
    Row *t = rt_row_new(3, 1, 0);
    check(t->len == 3 && strcmp(((const char **)t->data)[2], "") == 0, "text rows start as \"\"");
    check(rt_row_new(0, 0, 0)->len == 0, "empty rows are valid");
    check(rt_text_eq("abc", "abc") && !rt_text_eq("abc", "abd"), "text equality");

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "all runtime checks passed", failures,
           failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
