#!/bin/bash
# dev/run_tests.sh - runs tests/ok and tests/err with the TEMPORARY driver (Aleena's check).
# Hareem's real runner is tests/run.sh.
cd "$(dirname "$0")"
pass=0; fail=0
for f in ../tests/ok/*.dsl; do
    exp="${f%.dsl}.out"
    if ./devc "$f" --run 2>/tmp/err.txt | cmp -s - "$exp"; then pass=$((pass+1))
    else fail=$((fail+1)); echo "FAIL (ok test): $f"; fi
done
for f in ../tests/err/*.dsl; do
    spec=$(head -1 "$f" | sed -n 's/^# expect: \([0-9]*\): \(.*\)$/\1|\2/p')
    line=${spec%%|*}; part=${spec#*|}
    ./devc "$f" --run >/dev/null 2>/tmp/err.txt; code=$?
    if [ $code -eq 1 ] && grep -Eq "(:$line: error: |line $line: runtime error: ).*$part" /tmp/err.txt; then pass=$((pass+1))
    else fail=$((fail+1)); echo "FAIL (err test): $f  expected line $line '$part'  got: $(head -1 /tmp/err.txt)"; fi
done
echo "passed $pass, failed $fail"
[ $fail -eq 0 ]
