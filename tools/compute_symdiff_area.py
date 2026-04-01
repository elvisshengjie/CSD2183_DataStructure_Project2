import csv
import sys

from shapely.geometry import Polygon


def read_polygon(path: str) -> Polygon:
    rings: dict[int, list[tuple[float, float]]] = {}
    with open(path, newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            rings.setdefault(int(row["ring_id"]), []).append(
                (float(row["x"]), float(row["y"]))
            )

    if 0 not in rings:
        raise ValueError("Polygon CSV must contain ring 0.")

    holes = [rings[ring_id] for ring_id in sorted(rings) if ring_id != 0]
    return Polygon(rings[0], holes)


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("Usage: compute_symdiff_area.py <input.csv> <output.csv>")

    lhs = read_polygon(sys.argv[1])
    rhs = read_polygon(sys.argv[2])
    print(f"{lhs.symmetric_difference(rhs).area:.15f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
