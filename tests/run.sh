#!/usr/bin/env bash
# tests/run.sh — test runner (owner: Hareem)
#
#   tests/run.sh [--ast | --check | --run]      (default: --run)
#
# tests/ok/*.dsl   must succeed; under --run its stdout must equal the .out file
# tests/opt/*.dsl  `--ast-opt` output must equal the .ast file (optimizer)
# tests/err/*.dsl  must fail; first line is   # expect: <line>: <part of message>
#                  stderr must mention that line ("file:N:" or "line N:") and
#                  contain the message part.
# Under --ast only lexical/syntax errors can show up, so an err test that
# parses fine is reported as SKIP rather than FAIL.
#
# Phase 2 (M3): also compile each ok program with -o and compare the binary's
# output with --run.  TODO once codegen exists.

set -u
cd "$(dirname "$0")/.."
MODE="${1:---run}"
DSLC="$PWD/dslc"
pass=0 fail=0 skip=0
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

red()   { printf '\033[31m%s\033[0m\n' "$*"; }
green() { printf '\033[32m%s\033[0m\n' "$*"; }

for f in tests/ok/*.dsl; do
    [ -e "$f" ] || continue
    exp="${f%.dsl}.out"
    # programs are run from their own directory so "shop.html" resolves
    ( cd "$(dirname "$f")" && "$DSLC" "$(basename "$f")" "$MODE" ) >"$tmp/out" 2>"$tmp/err"
    code=$?
    if [ $code -ne 0 ]; then
        red "FAIL $f (exit $code)"; sed 's/^/     /' "$tmp/err" | head -5; fail=$((fail+1)); continue
    fi
    if [ "$MODE" = "--run" ] && [ -e "$exp" ] && ! diff -u "$exp" "$tmp/out" >"$tmp/diff"; then
        red "FAIL $f (output differs)"; sed 's/^/     /' "$tmp/diff" | head -20; fail=$((fail+1)); continue
    fi
    pass=$((pass+1))
done

for f in tests/err/*.dsl; do
    [ -e "$f" ] || continue
    spec=$(head -n1 "$f" | sed -n 's/^# *expect: *//p')
    if [ -z "$spec" ]; then red "FAIL $f (first line must be '# expect: <line>: <message>')"; fail=$((fail+1)); continue; fi
    line="${spec%%:*}"
    msg="$(echo "${spec#*:}" | sed 's/^ *//')"
    ( cd "$(dirname "$f")" && "$DSLC" "$(basename "$f")" "$MODE" ) >/dev/null 2>"$tmp/err"
    code=$?
    if [ $code -eq 0 ] || [ $code -eq 3 ]; then
        if [ "$MODE" != "--run" ]; then skip=$((skip+1)); continue; fi
        red "FAIL $f (expected an error, got exit $code)"; fail=$((fail+1)); continue
    fi
    if ! grep -qE "(:$line: |^line $line: )" "$tmp/err" || ! grep -qF -- "$msg" "$tmp/err"; then
        red "FAIL $f (expected line $line: $msg)"; sed 's/^/     got: /' "$tmp/err" | head -3; fail=$((fail+1)); continue
    fi
    pass=$((pass+1))
done

for f in tests/opt/*.dsl; do
    [ -e "$f" ] || continue
    if ! "$DSLC" "$f" --ast-opt 2>&1 | diff -u "${f%.dsl}.ast" - >"$tmp/diff"; then
        red "FAIL $f (optimized AST differs)"; sed 's/^/     /' "$tmp/diff" | head -20; fail=$((fail+1)); continue
    fi
    pass=$((pass+1))
done

echo
if [ $fail -eq 0 ]; then green "all passed: $pass passed, $skip skipped (mode $MODE)"; else red "$fail failed, $pass passed, $skip skipped (mode $MODE)"; fi
[ $fail -eq 0 ]
