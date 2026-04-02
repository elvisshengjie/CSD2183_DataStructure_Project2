#!/bin/bash
# run_tests.sh — runs all known fixture test cases and reports PASS/FAIL

SIMPLIFY="./simplify"
DATA="tests/data"

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

    # Skip if input file doesn't exist
    if [ ! -f "$input_path" ]; then
        echo "SKIP  $input (input file not found)"
        ((SKIP++))
        continue
    fi

    # Run simplifier and check if it succeeds
    echo -n "Testing $input (target=$target)... "
    
    if $SIMPLIFY "$input_path" "$target" > /dev/null 2>&1; then
        echo "PASS"
        ((PASS++))
    else
        echo "FAIL"
        ((FAIL++))
    fi
done

echo ""
echo "Results: $PASS passed, $FAIL failed, $SKIP skipped"

if [ $FAIL -eq 0 ]; then
    echo "✓ All tests passed!"
    exit 0
else
    echo "✗ Some tests failed"
    exit 1
fi
