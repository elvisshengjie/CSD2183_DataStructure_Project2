#!/bin/bash
# run_tests.sh — runs all known fixture test cases and reports PASS/FAIL
#
# Usage:  bash run_tests.sh
# Requires: ./simplify binary already built, test files in tests/data/

SIMPLIFY="./simplify"
DATA="tests/data"

# Each entry: "input_filename target_vertices"
TESTS=(
    "input_rectangle_with_two_holes.csv    7"
    "input_cushion_with_hexagonal_hole.csv 13"
    "input_blob_with_two_holes.csv         17"
    "input_wavy_with_three_holes.csv       21"
    "input_lake_with_two_islands.csv       17"
    "input_original_01.csv                 99"
    "input_original_02.csv                 99"
    "input_original_03.csv                 99"
    "input_original_04.csv                 99"
    "input_original_05.csv                 99"
    "input_original_06.csv                 99"
    "input_original_07.csv                 99"
    "input_original_08.csv                 99"
    "input_original_09.csv                 99"
    "input_original_10.csv                 99"
)

PASS=0
FAIL=0
SKIP=0

for entry in "${TESTS[@]}"; do
    input=$(echo $entry | awk '{print $1}')
    target=$(echo $entry | awk '{print $2}')

    input_path="$DATA/$input"
    expected_path="$DATA/output_${input#input_}"
    expected_path="${expected_path%.csv}.txt"

    # Skip if input file doesn't exist
    if [ ! -f "$input_path" ]; then
        echo "SKIP  $input (input file not found)"
        ((SKIP++))
        continue
    fi

    # Skip if expected output doesn't exist
    if [ ! -f "$expected_path" ]; then
        echo "SKIP  $input (expected output not found: $expected_path)"
        ((SKIP++))
        continue
    fi

    # Run the simplifier and filter out summary lines (Total signed area, Total areal displacement)
    actual=$($SIMPLIFY "$input_path" "$target" 2>/dev/null | grep -v "^Total" | grep -v "^$")
    expected=$(cat "$expected_path" | tr -d '\r')  # strip Windows CRLF

    if [ "$actual" = "$expected" ]; then
        echo "PASS  $input  target=$target"
        ((PASS++))
    else
        echo "FAIL  $input  target=$target"
        echo "      --- first differing lines ---"
        diff <(echo "$actual") <(echo "$expected") | head -10 | sed 's/^/      /'
        ((FAIL++))
    fi
done

echo ""
echo "Results: $PASS passed, $FAIL failed, $SKIP skipped"
[ $FAIL -eq 0 ] && exit 0 || exit 1