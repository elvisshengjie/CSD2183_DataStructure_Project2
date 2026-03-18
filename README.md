# CSD2183 Data Structures Project 2 Starter

This repository is a compile-ready starter for the assignment in
`area_and_topology_preserving_polygon_simplification.pdf`.

It already includes:

- a `Makefile` that builds `simplify`
- CSV input parsing for polygons with one exterior ring and zero or more holes
- geometry helpers for signed area, ring orientation, simplicity checks, and ring intersection checks
- a starter `Simplifier` class with APSC-oriented extension points and TODO markers
- sample input/output files and smoke-test scripts

It does **not** yet implement the full Area-Preserving Segment Collapse (APSC) algorithm from
Kronenfeld et al. (2020). Right now, if the target vertex count is lower than the current vertex
count, the program keeps the polygon unchanged and reports zero areal displacement. That makes
this a safe baseline to build on, not a finished submission.

## Assignment Requirements

From the PDF, your final submission needs to do all of the following:

- read `./simplify <input_file.csv> <target_vertices>`
- output the simplified polygon in the exact CSV-style format required by the brief
- preserve the area of each ring within floating-point tolerance
- preserve topology: same number of rings, no self-intersections, no ring-ring intersections
- reduce the polygon to at most the requested number of vertices when possible
- minimize areal displacement using APSC as the core algorithm
- include tests, documentation, and performance evaluation

## Project Layout

```text
include/
  csv_io.hpp
  geometry.hpp
  simplifier.hpp
src/
  csv_io.cpp
  geometry.cpp
  main.cpp
  simplifier.cpp
tests/
  data/
    example_input.csv
    example_expected_noop.txt
  run_smoke_test.ps1
  run_smoke_test.sh
Makefile
```

## Build

On macOS / Linux / WSL:

```bash
make
```

This should create:

```bash
./simplify
```

## Run

```bash
./simplify tests/data/example_input.csv 12
```

If the target is equal to or larger than the current vertex count, the output should match the
input geometry and print area totals plus zero displacement.

## What To Implement Next

The main missing work is inside [`src/simplifier.cpp`](/c:/Users/elvis/Desktop/CUSTOM_engine/CSD2183_DataStructure_Project2/src/simplifier.cpp):

1. Compute the APSC replacement point `E` for each 4-vertex chain `A -> B -> C -> D`.
2. Estimate areal displacement for each candidate collapse.
3. Store candidates in a priority queue keyed by minimum displacement.
4. Check whether replacing `A -> B -> C -> D` with `A -> E -> D` keeps topology valid.
5. Apply the collapse and update only the affected local neighborhood.
6. Repeat until the target vertex count is reached or no valid collapse remains.

## Suggested Implementation Roadmap

1. Replace the current vector-only ring representation with a structure that supports fast local
   edits, such as a linked vertex structure or DCEL-inspired ring nodes.
2. Add a `std::priority_queue` or custom heap for candidate collapses.
3. Add a spatial index for fast intersection checks.
4. Implement lazy invalidation or versioning so old heap entries can be discarded cheaply.
5. Add instructor-provided test cases and your own stress tests.
6. Measure runtime and memory usage on increasing input sizes.

## Smoke Test

Unix-like shell:

```bash
sh tests/run_smoke_test.sh
```

PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File tests/run_smoke_test.ps1
```

## Notes

- Debug/status messages go to standard error so standard output stays in the assignment format.
- The code includes comments around the key extension points so it is easier to continue from here.
- No third-party geometry library is required for this starter.
