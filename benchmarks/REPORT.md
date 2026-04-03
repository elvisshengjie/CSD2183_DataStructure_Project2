# Benchmark Report

This directory contains a lightweight benchmarking workflow for runtime and peak memory.

## Method

- Build `simplify` with `make`
- Run each case listed in [cases.csv](cases.csv)
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
| `TC1 – Smooth Circle (4000v)` | 4000 | 2000 | 0.102 | 8076 |
| `TC2 – Fractal Coastline (3072v)` | 3072 | 1536 | 0.095 | 7680 |
| `TC3 – 50 Holes Multi-Ring (664v)` | 664 | 332 | 0.092 | 8288 |
| `TC4 – Near-Degenerate Spikes (2003v)` | 2003 | 1001 | 0.108 | 11844 |
| `TC5 – Outward Spiral (5000v)` | 5001 | 2500 | 0.159 | 8832 |
| `custom_narrow_corridor` | 12 | 12 | 0.610 | 30696 |
| `custom_spiky_star` | 80 | 40 | 0.490 | 30688 |
| `custom_nine_holes` | 76 | 36 | 0.550 | 30692 |
| `custom_high_vertex_ring` | 720 | 180 | 0.600 | 30488 |
| `custom_near_degenerate_strip` | 122 | 40 | 0.520 | 30516 |
| `official_lake_small` | 6733 | 99 | 0.590 | 30692 |
| `official_lake_large` | 409998 | 99 | 10.160 | 239180 |

These values are machine-dependent, so they should be treated as a reproducible example rather
than a final submission claim.

## Included Benchmark Cases

### Custom Stress Cases (Group A — scaling and geometry variety)

- **TC1 – Smooth Circle (4000v)**: uniform circular polygon; baseline for near-zero displacement and clean O(n log n) scaling
- **TC2 – Fractal Coastline (3072v)**: Koch snowflake at depth 5; self-similar near-collinear runs stress the E-point solver
- **TC3 – 50 Holes Multi-Ring (664v)**: one outer ring plus 50 interior holes; exercises multi-ring budget routing and per-ring orientation guards
- **TC4 – Near-Degenerate Spikes (2003v)**: zigzag geometry with spike height 1e-4; tests epsilon thresholding for near-degenerate triangles
- **TC5 – Outward Spiral (5000v)**: self-proximate edges throughout; hammers the spatial grid intersection query count per collapse

### Custom Stress Cases (Group B — topology and local failure modes)

- **custom_narrow_corridor**: narrow-gap polygon with two interior holes; aggressive collapses can cause hole–exterior touching if topology checks are too local
- **custom_spiky_star**: 80-point star ring with highly oscillatory boundary; many near-equivalent collapse choices stress tie-breaking and candidate ordering
- **custom_nine_holes**: 300×300 rectangle with nine octagonal holes; stresses ring bookkeeping, hole orientation preservation, and repeated intersection checks
- **custom_high_vertex_ring**: 720-vertex wavy ring; intended for runtime and memory scaling checks on priority queue and spatial intersection logic
- **custom_near_degenerate_strip**: thin corrugated strip with near-collinear edges; amplifies floating-point sensitivity and makes area-preserving collapses harder to validate



