# Custom Challenge Datasets

These datasets are intended to exercise failure modes that are not always obvious from the
instructor-provided fixtures.

## Included Datasets

`input_narrow_corridor_with_two_holes.csv`

- Property targeted: narrow gaps and constrained corridors
- Why it matters: aggressive collapses can easily make a hole touch the exterior ring or another
  hole if topology checks are too local or too permissive

`input_spiky_star_80.csv`

- Property targeted: highly oscillatory single-ring geometry
- Why it matters: many nearly equivalent collapse choices make candidate ordering and tie-breaking
  more important, and areal displacement can change sharply with small local decisions

`input_grid_with_nine_holes.csv`

- Property targeted: many interior rings
- Why it matters: this stresses ring bookkeeping, hole orientation preservation, and repeated
  intersection checks across multiple boundaries

`input_high_vertex_wavy_ring_720.csv`

- Property targeted: high vertex count
- Why it matters: this is intended for runtime and memory scaling checks, especially to see whether
  the priority queue and spatial intersection logic remain efficient as the number of candidate
  collapses grows

`input_near_degenerate_corrugated_strip_122.csv`

- Property targeted: near-degeneracies and almost-collinear edges
- Why it matters: very thin polygons can amplify floating-point sensitivity and make area-preserving
  collapses harder to validate without accidentally creating invalid geometry or unstable metrics

## Regeneration

To regenerate the CSV files:

```bash
python tests/custom/generate_custom_datasets.py
```
