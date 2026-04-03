#!/bin/sh

set -eu

if [ "$#" -ne 2 ]; then
    echo "Usage: compute_symdiff_area.sh <input.csv> <output.csv>" >&2
    exit 1
fi

INPUT_PATH="$1"
OUTPUT_PATH="$2"
SCRIPT_PATH="tools/compute_symdiff_area.py"

if command -v python3 >/dev/null 2>&1; then
    if python3 -c "import shapely" >/dev/null 2>&1; then
        exec python3 "$SCRIPT_PATH" "$INPUT_PATH" "$OUTPUT_PATH"
    fi
fi

if command -v cmd.exe >/dev/null 2>&1 && command -v wslpath >/dev/null 2>&1; then
    WIN_SCRIPT="$(wslpath -w "$SCRIPT_PATH")"
    WIN_INPUT="$(wslpath -w "$INPUT_PATH")"
    WIN_OUTPUT="$(wslpath -w "$OUTPUT_PATH")"
    exec cmd.exe /c python "$WIN_SCRIPT" "$WIN_INPUT" "$WIN_OUTPUT"
fi

echo "No Python environment with shapely was found for exact displacement." >&2
exit 1
