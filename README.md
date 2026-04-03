# CSD2183 Data Structures Project 2

Area- and topology-preserving polygon simplification in C++17 using an APSC-style greedy
collapse loop with linked-ring updates, a priority queue for candidate selection, and a spatial
grid for fast intersection checks.

## Current Status

This repository now contains a working `simplify` executable rather than the original starter-only
baseline. The current implementation:

- reads `./simplify <input_file.csv> <target_vertices>`
- preserves the signed area of each ring within floating-point tolerance
- preserves ring count and ring orientation
- avoids self-intersections and ring-ring intersections on the tested datasets
- reports total signed area and total areal displacement in scientific notation
- uses a priority queue plus versioning to lazily invalidate stale collapse candidates
- uses a spatial grid to accelerate intersection checks

The project also includes:

- smoke tests
- reference-file comparisons
- rubric-aligned geometric validation
- custom challenge datasets
- a benchmark runner for runtime and peak memory measurements

## Build

Build on Linux, macOS, or WSL:

```bash
make
```

This creates:

```bash
./simplify
```

## Run

```bash
./simplify tests/data/example_input.csv 12
```

Output format:

- `ring_id,vertex_id,x,y`
- one CSV row per output vertex
- three summary lines:
  - `Total signed area in input: ...`
  - `Total signed area in output: ...`
  - `Total areal displacement: ...`

Debug or status text should go to standard error, not standard output.

## Test Results

### Smoke Test

PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File tests/run_smoke_test.ps1
```

Local result:

```text
Smoke test passed.
```

### Rubric-Aligned Validation

PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File tests/run_rubric_checks.ps1
```

What this checks:

- ring count preservation
- per-ring signed-area preservation
- output ring orientation
- self-intersection and ring-ring intersection failures
- output metric consistency
- whether the output stays above the requested target

Local result:

```text
All rubric checks passed.
```

### Reference-File Comparison

PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File tests/run_fixture_tests.ps1
```

Note:

- the teaching-team clarification says exact coordinate-by-coordinate matching is not required
- the rubric-aligned validator is therefore the more meaningful correctness check for grading
- the fixture diff is still useful as a stricter regression check

## Custom Challenge Datasets

Additional datasets live in [tests/custom/README.md]

Included custom cases:

- `input_narrow_corridor_with_two_holes.csv`
  Targets narrow gaps between holes and the exterior ring, where valid collapses can easily create
  intersections if topology checks are too weak.
- `input_spiky_star_80.csv`
  Targets oscillating boundaries with alternating spikes and many local collapse candidates.
- `input_grid_with_nine_holes.csv`
  Targets multi-ring handling and repeated intersection checks across many interior rings.
- `input_high_vertex_wavy_ring_720.csv`
  Targets high vertex count and gives a cleaner scaling point for runtime and peak-memory plots.
- `input_near_degenerate_corrugated_strip_122.csv`
  Targets near-degenerate thin geometry with many almost-collinear edges, which is useful for
  floating-point robustness and metric-consistency checks.

Generate or regenerate them with:

```bash
python tests/custom/generate_custom_datasets.py
```

## Benchmarking

Benchmark artifacts live in `benchmarks/`.

Case manifest:

- [benchmarks/cases.csv](CSD2183_DataStructure_Project2/benchmarks/cases.csv)

Runner:

- [benchmarks/run_benchmarks.py](CSD2183_DataStructure_Project2/benchmarks/run_benchmarks.py)

PowerShell wrapper:

- [benchmarks/run_benchmarks.ps1](CSD2183_DataStructure_Project2/benchmarks/run_benchmarks.ps1)

Sample report:

- [benchmarks/REPORT.md](CSD2183_DataStructure_Project2/benchmarks/REPORT.md)

Run the benchmark suite from PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File benchmarks/run_benchmarks.ps1
```

Or directly inside Linux/WSL:

```bash
python3 benchmarks/run_benchmarks.py
```

The benchmark runner records:

- input vertex count
- output vertex count
- elapsed time in seconds
- peak RSS memory in kilobytes
- reported areal displacement

Results are written to:

```text
benchmarks/results/benchmark_results.csv
```

## Dependencies

Core build dependency:

- `g++` with C++17 support

Optional dependency for exact symmetric-difference displacement reporting:

- Python with `shapely`

If `shapely` is available, the program uses [tools/compute_symdiff_area.sh](/c:/Users/elvis/Desktop/CUSTOM_engine/CSD2183_DataStructure_Project2/tools/compute_symdiff_area.sh)
to compute exact final areal displacement from the emitted geometry. If not available, the code
falls back to its internal displacement estimate.

## Repository Layout

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
  custom/
  data/
  run_smoke_test.ps1
  run_fixture_tests.ps1
  run_rubric_checks.ps1
  validate_output.py
benchmarks/
  cases.csv
  run_benchmarks.py
  run_benchmarks.ps1
  REPORT.md
tools/
  compute_symdiff_area.py
  compute_symdiff_area.sh
```

## Notes

- The current implementation is aligned with the clarified grading guidance that focuses on area
  preservation, topology preservation, metric consistency, and valid greedy simplification rather
  than exact coordinate-by-coordinate matching with a reference file.
- For the final submission package, you still need the separate AI interaction log, AI usage report,
  and video presentation required by the PDF brief.
