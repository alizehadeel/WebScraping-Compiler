#!/bin/bash
# tests/run.sh - the test runner (make test)
# OWNER: Hareem | PHASE: 1 | STATUS: TODO
#   [ ] for every tests/ok/*.dsl: run  ./dslc X.dsl --run  and compare stdout with X.out
#   [ ] for every tests/err/*.dsl: the first line is  "# expect: <line>: <part of the message>";
#       run --run and check that stderr contains  ":<line>: error: ...<part>"  (compile-time)
#       or  "line <line>: runtime error: ...<part>"  (run-time), and that the exit code is 1
#   [ ] Phase 2: also compile every tests/ok program and compare the binary's output with --run
# dev/run_tests.sh (Aleena) already does the first two with the temporary driver - copy and adapt it.
echo "TODO: tests/run.sh is not written yet (Hareem)"; exit 1
