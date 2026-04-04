# CSD2183 Data Structures — Project 2
## Area- and Topology-Preserving Polygon Simplification

![Build Status](https://img.shields.io/badge/build-passing-brightgreen)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![License](https://img.shields.io/badge/license-academic-lightgrey)

---

## What This Project Does

This program simplifies a planar polygon — one outer boundary with zero or more
holes — by reducing the number of boundary vertices while satisfying three hard
constraints:

| Constraint | Description |
|---|---|
| **Area preservation** | The signed area of every ring (outer boundary and each hole) is preserved exactly within floating-point tolerance. |
| **Topology preservation** | No ring may self-intersect, and no two rings may cross each other. The ring count stays the same. |
| **Minimum areal displacement** | Among all valid simplifications, the algorithm greedily minimises the total area "moved" by the boundary changes. |

### Why Does This Matter?

Geographic information systems (GIS), web mapping platforms, and real-time game
worlds all use polygons to represent territories, buildings, and navigation areas.
Complex polygons with thousands of vertices are slow to render, transmit, and query.
Simplifying them while keeping area exact preserves land-use statistics, tax parcel
areas, and simulation metrics — the shapes look different but their measured
properties are unchanged.

---

## Algorithm Overview (APSC)

The core algorithm is **Area-Preserving Segment Collapse (APSC)** from
Kronenfeld et al. (2020). Here is an intuitive explanation:

```
Original ring: ... → A → B → C → D → ...
                          ↑
                     Remove B and C,
                     replace with new vertex E
Simplified ring: ... → A → E → D → ...
```

**Step 1 — Find E:** E is placed so that the signed area of the ring is unchanged.
This reduces to a linear equation: E must lie on a specific line derived from the
shoelace-area formula applied to the quadrilateral A–B–C–D. E is found by
intersecting that area-constraint line with either line AB or line CD.

**Step 2 — Measure displacement:** The areal displacement of this collapse is the
area of the symmetric difference between quadrilateral ABCD and triangle AED
(computed via Sutherland-Hodgman polygon clipping).

**Step 3 — Greedy selection:** All possible collapses are ranked by displacement in
a min-heap (priority queue). The cheapest valid collapse is applied first.

**Step 4 — Topology check:** Before committing any collapse, the two new edges
(A–E and E–D) are tested against all other segments using a spatial grid. If
either new edge intersects any existing edge, the collapse is rejected.

**Step 5 — Repeat:** After each collapse, only the four locally affected candidates
need to be updated in the heap (lazy incremental update). Repeat until the target
vertex count is reached or no valid collapse exists.

---

## Data Structures Used

| Structure | Purpose | Complexity |
|---|---|---|
| Doubly-linked circular list (per ring) | O(1) vertex removal and re-linking after each collapse | O(1) collapse |
| Min-heap (priority_queue) with version stamps | Lazy invalidation of stale candidates without a decrease-key operation | O(log n) per pop |
| Spatial hash grid | Accelerate intersection checks; segments are indexed by bounding-box cells | O(√n) avg per query |
| Original-chain tracking | Track intermediate removed vertices for exact displacement bookkeeping | O(1) per collapse |

**Version stamping** is the key heap optimisation: every Candidate in the heap
stores the version numbers of vertices B and C at push time. When a collapse
moves B or updates its neighbours, those vertices' version counters increment.
A stale candidate popped from the heap has mismatched versions and is simply
discarded — no expensive heap restructuring needed.

---

## Build

### Requirements

- `g++` with C++17 support (GCC 7+ or Clang 5+)
- `make`
- Linux, macOS, or WSL for the provided `make` / shell-script workflow

### Optional tools

- Python 3 for validation and benchmarking helpers
- `shapely` for `tools/compute_symdiff_area.py`:
  ```bash
  pip install shapely
  ```

### Build the executable

```bash
git clone <your-repo-url>
cd <repo-root>
make
```

This produces `./simplify` in the repository root.

---

## Run

```bash
./simplify <input_file.csv> <target_vertices>
```

### Examples

```bash
# Simplify to at most 12 vertices
./simplify tests/data/example_input.csv 12

# Simplify a benchmark-scale polygon to 2500 vertices
./simplify tests/custom/input_tc5_spiral_5000v.csv 2500
```

### Input CSV Format

```
ring_id,vertex_id,x,y
0,0,-0.5,-1.0
0,1,1.5,-1.0
0,2,1.5,1.0
0,3,-0.5,1.0
1,0,-0.2,0.5
1,1,0.5,0.5
1,2,0.5,-0.5
```

- `ring_id = 0` is the **exterior ring** (counterclockwise orientation).
- `ring_id >= 1` are **interior rings / holes** (clockwise orientation).
- Vertices within each ring are listed in order; the ring is implicitly closed
  (last vertex connects back to first — do **not** repeat the first vertex).

### Output Format

```
ring_id,vertex_id,x,y
0,0,-0.5,-1
0,1,1.5,-1
...
Total signed area in input: 3.210000000000000e+00
Total signed area in output: 3.210000000000000e+00
Total areal displacement: 1.600000000000000e-02
```

- Vertices are listed in ring order, ring IDs starting at 0.
- Three summary lines in scientific notation follow all vertex rows.
- Debug/log messages go to **standard error**, not standard output.

---

## Test Results

### Smoke Test

```bash
# Linux / macOS / WSL
bash tests/run_smoke_test.sh

# PowerShell (Windows + WSL)
powershell -ExecutionPolicy Bypass -File tests/run_smoke_test.ps1
```

Expected result:
```
Smoke test passed.
```

### Rubric-Aligned Validation

This validator checks all correctness criteria from the project rubric:

```bash
# Rebuild the simplify executable if you deleted build artifacts
make

# Run the rubric checks
powershell -ExecutionPolicy Bypass -File tests\run_rubric_checks.ps1
```

Run the rebuild step in an environment where `make` and `g++` are available
(for example Linux, macOS, or WSL).

What is checked:
- **Ring count** — output has same number of rings as input.
- **Per-ring signed area** — each ring's area matches input within 1e-9 tolerance.
- **Orientation** — exterior ring counterclockwise, holes clockwise.
- **Self-intersection** — no ring self-intersects.
- **Ring-ring intersection** — no two rings cross each other.
- **Metric consistency** — reported input and output areas match computed values.
- **Vertex count** — output vertex count is at or below the requested target.

Expected result:
```
All rubric checks passed.
```

### Reference File Comparison

```bash
# Linux / macOS / WSL
bash tests/run_fixture_tests.sh

# PowerShell (Windows + WSL)
powershell -ExecutionPolicy Bypass -File tests/run_fixture_tests.ps1
```

> **Note:** The teaching team's clarification states that exact coordinate-by-coordinate
> matching is not required; the rubric-aligned validator is the primary correctness check.
> The fixture comparison is provided as a regression tool.

---

## Custom Challenge Datasets

Ten custom datasets are included in `tests/custom/` to stress-test specific
algorithmic failure modes, from large benchmark-scale inputs to smaller
topology edge cases.

These CSVs are already checked into the repository, so no dataset-generation
step is required before benchmarking or using the dashboard.

| Dataset | Target | What It Tests |
|---|---:|---|
| `input_tc1_circle_4000v.csv` | 2000 | Uniform circle baseline; near-zero displacement case for clean scaling checks. |
| `input_tc2_fractal_3072v.csv` | 1536 | Koch-style fractal coastline; stresses the E-point solver on self-similar near-collinear runs. |
| `input_tc3_holes_50.csv` | 332 | Outer ring with 50 holes; tests multi-ring budget routing and per-ring orientation handling. |
| `input_tc4_spikes_2003v.csv` | 1001 | Near-degenerate zigzag spikes (`h = 1e-4`); stresses epsilon handling and tiny triangle collapses. |
| `input_tc5_spiral_5000v.csv` | 2500 | Outward spiral with self-proximate edges; hammers spatial-grid intersection queries. |
| `input_narrow_corridor_with_two_holes.csv` | 9 | Narrow gaps between holes and exterior ring; easy to create crossings if topology checks are weak. |
| `input_spiky_star_80.csv` | 40 | 80-vertex star with alternating spikes; many near-equal-displacement candidates that stress the heap. |
| `input_grid_with_nine_holes.csv` | 36 | Nine interior rings; tests multi-ring bookkeeping and repeated spatial index queries. |
| `input_high_vertex_wavy_ring_720.csv` | 180 | 720-vertex wavy boundary; primary runtime scaling benchmark point. |
| `input_near_degenerate_corrugated_strip_122.csv` | 40 | Thin corrugated geometry with near-collinear edges; tests floating-point robustness. |

---

## Dashboard (Interactive Web UI)

The project includes a browser-based dashboard for visual exploration, served by
the C++ HTTP server in `src/server.cpp`.

### Start the server

```bash
# Build the dashboard server
make server

# Start the server (defaults to port 8080)
./dashboard_server

# Or build and run in one step
make run-server
```

Then open **http://localhost:8080** in your browser.

### What the dashboard shows

- **Dataset selector** — browse and select any test CSV from `tests/`.
- **Target vertex input** — set the desired output vertex count.
- **Input polygon view** — SVG rendering of the original boundary.
- **Output polygon view** — SVG rendering after simplification.
- **Overlay view** — both boundaries superimposed (teal = original, coral = simplified).
- **Metrics panel** — area drift, areal displacement, runtime, vertex counts.
- **Reduction curve** — sweep target values automatically and plot displacement vs. vertices removed.
- **Benchmark charts** — scatter plots of runtime and memory vs. input size, loaded from benchmark results CSV.

---

## Benchmarking

```bash
# Linux / macOS / WSL
make
python3 benchmarks/run_benchmarks.py

# PowerShell (uses WSL for `make` and `python3`)
powershell -ExecutionPolicy Bypass -File benchmarks/run_benchmarks.ps1
```

Results are written to `benchmarks/results/benchmark_results.csv` with columns:
`label, input_path, target_vertices, input_vertices, output_vertices, elapsed_seconds, peak_rss_kb, reported_areal_displacement, notes`.

See `benchmarks/REPORT.md` for analysis, scaling plots, and fitted functions.

---

## Repository Layout

```
.
├── include/
│   ├── csv_io.hpp          # CSV I/O declarations
│   ├── geometry.hpp        # Point, Vertex, Candidate structs
│   └── simplifier.hpp      # Simplifier class declaration
├── src/
│   ├── csv_io.cpp          # CSV loading, output printing, cleanup
│   ├── dashboard.html      # Interactive browser dashboard
│   ├── geometry.cpp        # Cross product, polygon area, struct implementations
│   ├── main.cpp            # Entry point: parse args, load, simplify, print
│   ├── server.cpp          # C++ HTTP server for the dashboard API/UI
│   └── simplifier.cpp      # APSC algorithm, spatial grid, priority queue
├── tests/
│   ├── custom/             # Custom challenge datasets
│   ├── data/               # Smoke-test fixture data
│   ├── README.md           # Fixture inventory and expected outputs
│   ├── run_fixture_tests.sh
│   ├── run_tests.sh        # Alternate shell runner for fixture comparisons
│   ├── run_smoke_test.ps1
│   ├── run_smoke_test.sh
│   ├── run_fixture_tests.ps1
│   ├── run_rubric_checks.ps1
│   └── validate_output.py  # Geometry validation script
├── benchmarks/
│   ├── cases.csv           # Benchmark case manifest
│   ├── run_benchmarks.py   # Benchmark runner
│   ├── run_benchmarks.ps1
│   ├── results/            # Benchmark output CSV
│   └── REPORT.md           # Scaling analysis and plots
├── tools/
│   ├── compute_symdiff_area.py   # Exact displacement via shapely
│   └── compute_symdiff_area.sh  # Shell wrapper
├── Makefile
└── README.md
```

---

## Dependencies

| Dependency | Required? | Purpose |
|---|---|---|
| `g++` (C++17) | **Yes** | Compile the simplify binary |
| `make` | **Yes** | Build system |
| Python 3 | Optional | Run validation and benchmarking helpers |
| `shapely` (Python) | Optional | Exact symmetric-difference helper in `tools/compute_symdiff_area.py` |
| WSL | Optional | Lets the PowerShell wrappers invoke `make`, `bash`, and `python3` on Windows |

---

## Notes

- Area is preserved within floating-point tolerance (~1e-12 for typical polygon sizes).
- Topology is verified via the spatial grid for large rings and an explicit O(n²)
  check for rings with ≤ 512 vertices.
- If no valid collapse exists before the target is reached, the program outputs
  the best simplification achievable without violating constraints.
- All measurements (runtime, memory) are single-threaded; do not use `make -j`
  for performance comparisons.

---

## References

Kronenfeld, B. J., Stanislawski, L. V., Buttenfield, B. P., and Brockmeyer, T. (2020).
"Simplification of polylines by segment collapse: minimizing areal displacement while
preserving area." *International Journal of Cartography*, 6(1), pp. 22–46.
https://doi.org/10.1080/23729333.2019.1631535
