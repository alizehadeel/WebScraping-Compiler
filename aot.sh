#!/usr/bin/env bash
# aot.sh - Phase 2 ahead-of-time compile in one step: program.dsl -> native executable
#
#   ./aot.sh prog.dsl            builds ./prog  (next to prog.dsl), keeps prog.ll
#   ./aot.sh prog.dsl -o name    builds ./name
#
# Steps: dslc -o (LLVM IR)  ->  opt -O2 (middle-end)  ->  clang (link with runtime/rt.c + libxml2)
# Tools can be overridden, e.g.  OPT=opt-15 CLANG=clang-15 ./aot.sh prog.dsl
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OPT="${OPT:-opt}"
CLANG="${CLANG:-clang}"
MIDDLE_END_FLAGS="${MIDDLE_END_FLAGS:--O2}"

if [ $# -ne 1 ] && { [ $# -ne 3 ] || [ "$2" != "-o" ]; }; then
    echo "usage: ./aot.sh <prog.dsl> [-o <executable>]" >&2
    exit 2
fi
src="$1"
base="${src%.dsl}"
exe="${3:-$base}"

[ -x "$ROOT/dslc" ] || { echo "aot.sh: ./dslc not found - run 'make' first" >&2; exit 1; }

"$ROOT/dslc" "$src" -o "$base.ll"                                   # front end + IR generation
"$OPT" $MIDDLE_END_FLAGS -S "$base.ll" -o "$base.opt.ll"             # middle-end
"$CLANG" -Wno-override-module "$base.opt.ll" "$ROOT/runtime/rt.c" -I"$ROOT/runtime" \
    $(xml2-config --cflags --libs) -o "$exe"                         # back end + link
rm -f "$base.opt.ll"
echo "built $exe (LLVM IR in $base.ll)"
