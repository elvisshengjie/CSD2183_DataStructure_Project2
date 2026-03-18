#!/usr/bin/env sh
set -eu

make
./simplify tests/data/example_input.csv 12 > tests/smoke_output.tmp
diff -u tests/data/example_expected_noop.txt tests/smoke_output.tmp
rm -f tests/smoke_output.tmp
echo "Smoke test passed."
