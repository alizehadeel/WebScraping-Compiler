#!/usr/bin/env bash
# tests/run.sh - the test runner (make test)
# OWNER: Hareem | PHASE: 1 | STATUS: DONE (Phase 2 compile-and-compare: TODO)
#
#   tests/run.sh [--ast | --check | --run | --compile]      (default: --run)
#
# tests/ok/*.dsl   must succeed; under --run its stdout must equal the .out file
# tests/opt/*.dsl  `--ast-opt` output must equal the .ast file (optimizer)
# tests/err/*.dsl  must fail; first line is   # expect: <line>: <part of message>
#                  stderr must mention that line ("file:N:" or "line N:") and
#                  contain the message part.
# Under --ast only lexical/syntax errors can show up, so an err test that
# parses fine is reported as SKIP rather than FAIL.
#
# --compile (Phase 2): every tests/ok program is compiled with -o, checked with
# llvm-as, passed through the middle-end, linked with the runtime and run; its
# output must equal the .out file (the interpreter's output). Programs that use
# a construct codegen does not support yet (dslc exits 3) are reported as SKIP.
# The middle-end command can be overridden:  MIDDLE_END="opt -O2 -S" tests/run.sh --compile

set -u
cd "$(dirname "$0")/.."
MODE="${1:---run}"
DSLC="$PWD/dslc"
pass=0 fail=0 skip=0
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

red()   { printf '\033[31m%s\033[0m\n' "$*"; }
green() { printf '\033[32m%s\033[0m\n' "$*"; }

if [ "$MODE" = "--compile" ]; then
    MIDDLE_END="${MIDDLE_END:-opt -passes=mem2reg -S}"
    XML="$(xml2-config --cflags --libs)"
    for f in tests/ok/*.dsl; do
        [ -e "$f" ] || continue
        exp="${f%.dsl}.out"; dir="$(dirname "$f")"; base="$(basename "$f" .dsl)"
        "$DSLC" "$f" -o "$tmp/$base.ll" 2>"$tmp/err"; code=$?
        if [ $code -eq 3 ]; then skip=$((skip+1)); continue; fi
        if [ $code -ne 0 ]; then red "FAIL $f (dslc -o exit $code)"; sed 's/^/     /' "$tmp/err" | head -5; fail=$((fail+1)); continue; fi
        if ! llvm-as "$tmp/$base.ll" -o /dev/null 2>"$tmp/err"; then
            red "FAIL $f (invalid IR)"; sed 's/^/     /' "$tmp/err" | head -5; fail=$((fail+1)); continue; fi
        if ! $MIDDLE_END "$tmp/$base.ll" -o "$tmp/$base.opt.ll" 2>"$tmp/err"; then
            red "FAIL $f (middle-end)"; sed 's/^/     /' "$tmp/err" | head -5; fail=$((fail+1)); continue; fi
        if ! clang -Wno-override-module "$tmp/$base.opt.ll" runtime/rt.c -Iruntime $XML -o "$tmp/$base.bin" 2>"$tmp/err"; then
            red "FAIL $f (link)"; sed 's/^/     /' "$tmp/err" | head -5; fail=$((fail+1)); continue; fi
        ( cd "$dir" && "$tmp/$base.bin" ) >"$tmp/out" 2>"$tmp/err"
        if ! diff -u "$exp" "$tmp/out" >"$tmp/diff"; then
            red "FAIL $f (compiled output differs from --run)"; sed 's/^/     /' "$tmp/diff" | head -20; fail=$((fail+1)); continue; fi
        pass=$((pass+1))
    done
    echo
    if [ $fail -eq 0 ]; then green "all passed: $pass compiled and matched, $skip skipped (mode --compile)"; else red "$fail failed, $pass passed, $skip skipped (mode --compile)"; fi
    [ $fail -eq 0 ]
    exit
fi

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
