#!/bin/sh
set -eu

cd "$(dirname "$0")/.."
make

failures=0

while read -r input target expected; do
    [ -n "$input" ] || continue

    if ! actual="$(timeout 15s ./simplify "tests/$input" "$target")"; then
        status=$?
        if [ "$status" -eq 124 ]; then
            printf '%s\tTIMEOUT\n' "$input"
        else
            printf '%s\tERROR(%s)\n' "$input" "$status"
        fi
        failures=$((failures + 1))
        continue
    fi

    if ! diff -u "tests/$expected" - >/dev/null <<EOF
$actual
EOF
    then
        printf '%s\tFAIL\n' "$input"
        failures=$((failures + 1))
        continue
    fi

    printf '%s\tPASS\n' "$input"
done <<'EOF'
input_rectangle_with_two_holes.csv 7 output_rectangle_with_two_holes.txt
input_cushion_with_hexagonal_hole.csv 13 output_cushion_with_hexagonal_hole.txt
input_blob_with_two_holes.csv 17 output_blob_with_two_holes.txt
input_wavy_with_three_holes.csv 21 output_wavy_with_three_holes.txt
input_lake_with_two_islands.csv 17 output_lake_with_two_islands.txt
input_original_01.csv 99 output_original_01.txt
input_original_02.csv 99 output_original_02.txt
input_original_03.csv 99 output_original_03.txt
input_original_04.csv 99 output_original_04.txt
input_original_05.csv 99 output_original_05.txt
input_original_06.csv 99 output_original_06.txt
input_original_07.csv 99 output_original_07.txt
input_original_08.csv 99 output_original_08.txt
input_original_09.csv 99 output_original_09.txt
input_original_10.csv 99 output_original_10.txt
EOF

if [ "$failures" -ne 0 ]; then
    printf '%s fixture test(s) did not pass.\n' "$failures" >&2
    exit 1
fi

printf 'All fixture tests passed.\n'
