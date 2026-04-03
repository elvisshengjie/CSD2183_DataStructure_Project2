# Benchmark Report

This directory contains a lightweight benchmarking workflow for runtime and peak memory.

## Method

- Build `simplify` with `make`
- Run each case listed in [cases.csv](/c:/Users/elvis/Desktop/CUSTOM_engine/CSD2183_DataStructure_Project2/benchmarks/cases.csv)
- Measure elapsed time and peak resident set size using `/usr/bin/time`
- Record input size, output size, reported areal displacement, runtime, and peak RSS

## How To Run

PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File benchmarks/run_benchmarks.ps1
```

Linux / WSL:

```bash
python3 tests/custom/generate_custom_datasets.py
make
python3 benchmarks/run_benchmarks.py
```

Results are written to:

```text
benchmarks/results/benchmark_results.csv
```

## Sample Local Results

The following sample numbers were recorded locally on April 3, 2026 using the current build:

| Case | Input vertices | Output vertices | Time (s) | Peak RSS (KB) |
|---|---:|---:|---:|---:|
| `custom_narrow_corridor` | 12 | 12 | 0.570 | 30692 |
| `custom_spiky_star` | 80 | 40 | 0.510 | 30688 |
| `custom_nine_holes` | 76 | 36 | 0.460 | 30516 |
| `custom_high_vertex_ring` | 720 | 180 | 0.600 | 30488 |
| `custom_near_degenerate_strip` | 122 | 40 | 0.520 | 30516 |
| `official_lake_small` | 6733 | 99 | 0.590 | 30692 |
| `official_lake_large` | 409998 | 99 | 10.160 | 239180 |

These values are machine-dependent, so they should be treated as a reproducible example rather
than a final submission claim.

## Included Benchmark Cases

- `custom_narrow_corridor`: narrow-gap topology stress
- `custom_spiky_star`: oscillatory boundary stress
- `custom_nine_holes`: many-hole bookkeeping stress
- `official_lake_small`: medium-size instructor dataset
- `official_lake_large`: large-size instructor dataset

## Notes For Final Submission

This report scaffolds the required runtime and memory study, but the final submission should extend
it with:

- more input sizes for scaling curves
- plots of runtime vs input size
- plots of peak memory vs input size
- plots of areal displacement vs target vertex count
- fitted growth functions and discussion
