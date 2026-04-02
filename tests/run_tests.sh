#!/usr/bin/env bash
set -euo pipefail

# Runs all known fixture test cases and reports PASS/FAIL.

cd "$(dirname "$0")/.."
make

SIMPLIFY="./simplify"
DATA="tests"

TESTS=(
    "input_rectangle_with_two_holes.csv 7 output_rectangle_with_two_holes.txt"
    "input_cushion_with_hexagonal_hole.csv 13 output_cushion_with_hexagonal_hole.txt"
    "input_blob_with_two_holes.csv 17 output_blob_with_two_holes.txt"
    "input_wavy_with_three_holes.csv 21 output_wavy_with_three_holes.txt"
    "input_lake_with_two_islands.csv 17 output_lake_with_two_islands.txt"
    "input_original_01.csv 99 output_original_01.txt"
    "input_original_02.csv 99 output_original_02.txt"
    "input_original_03.csv 99 output_original_03.txt"
    "input_original_04.csv 99 output_original_04.txt"
    "input_original_05.csv 99 output_original_05.txt"
    "input_original_06.csv 99 output_original_06.txt"
    "input_original_07.csv 99 output_original_07.txt"
    "input_original_08.csv 99 output_original_08.txt"
    "input_original_09.csv 99 output_original_09.txt"
    "input_original_10.csv 99 output_original_10.txt"
)

pass=0
fail=0

for entry in "${TESTS[@]}"; do
    read -r input target expected <<<"$entry"

    input_path="$DATA/$input"
    expected_path="$DATA/$expected"

    if ! actual="$("$SIMPLIFY" "$input_path" "$target")"; then
        echo "FAIL  $input  target=$target (program error)"
        ((fail += 1))
        continue
    fi

    if diff -u "$expected_path" - >/dev/null <<EOF
$actual
EOF
    then
        echo "PASS  $input  target=$target"
        ((pass += 1))
    else
        echo "FAIL  $input  target=$target"
        ((fail += 1))
    fi
done

echo
echo "Results: $pass passed, $fail failed"
if [ "$fail" -eq 0 ]; then
    exit 0
fi

exit 1
