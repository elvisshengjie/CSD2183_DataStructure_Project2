import csv
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parent


def write_polygon(path: Path, rings: list[list[tuple[float, float]]]) -> None:
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["ring_id", "vertex_id", "x", "y"])
        for ring_id, ring in enumerate(rings):
            for vertex_id, (x, y) in enumerate(ring):
                writer.writerow([ring_id, vertex_id, f"{x:.12g}", f"{y:.12g}"])


def rectangle_ccw(x0: float, y0: float, x1: float, y1: float) -> list[tuple[float, float]]:
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def rectangle_cw(x0: float, y0: float, x1: float, y1: float) -> list[tuple[float, float]]:
    return [(x0, y1), (x1, y1), (x1, y0), (x0, y0)]


def star_ring(center_x: float, center_y: float, outer_r: float, inner_r: float, spikes: int) -> list[tuple[float, float]]:
    pts: list[tuple[float, float]] = []
    for i in range(spikes * 2):
        radius = outer_r if i % 2 == 0 else inner_r
        angle = (math.pi * i) / spikes
        pts.append((center_x + radius * math.cos(angle), center_y + radius * math.sin(angle)))
    return pts


def octagon_cw(center_x: float, center_y: float, radius: float) -> list[tuple[float, float]]:
    pts = []
    for i in range(8):
        angle = -2.0 * math.pi * i / 8.0
        pts.append((center_x + radius * math.cos(angle), center_y + radius * math.sin(angle)))
    return pts


def wavy_ring(
    center_x: float,
    center_y: float,
    base_radius: float,
    amplitude: float,
    waves: int,
    points: int,
) -> list[tuple[float, float]]:
    pts = []
    for i in range(points):
        angle = 2.0 * math.pi * i / points
        radius = base_radius + amplitude * math.sin(waves * angle)
        pts.append((center_x + radius * math.cos(angle), center_y + radius * math.sin(angle)))
    return pts


def thin_corrugated_strip(
    x0: float,
    x1: float,
    y_center: float,
    half_height: float,
    steps: int,
    wobble: float,
) -> list[tuple[float, float]]:
    top = []
    bottom = []
    for i in range(steps + 1):
        x = x0 + (x1 - x0) * i / steps
        phase = 2.0 * math.pi * i / 8.0
        top.append((x, y_center + half_height + wobble * math.sin(phase)))
        bottom.append((x, y_center - half_height + wobble * math.cos(phase)))
    return top + list(reversed(bottom))


def main() -> None:
    write_polygon(
        ROOT / "input_narrow_corridor_with_two_holes.csv",
        [
            rectangle_ccw(0.0, 0.0, 200.0, 120.0),
            rectangle_cw(18.0, 26.0, 96.0, 94.0),
            rectangle_cw(104.0, 26.0, 182.0, 94.0),
        ],
    )

    write_polygon(
        ROOT / "input_spiky_star_80.csv",
        [star_ring(0.0, 0.0, 100.0, 58.0, 40)],
    )

    holes = []
    for row in range(3):
        for col in range(3):
            holes.append(octagon_cw(70.0 + 80.0 * col, 70.0 + 80.0 * row, 18.0))

    write_polygon(
        ROOT / "input_grid_with_nine_holes.csv",
        [rectangle_ccw(0.0, 0.0, 300.0, 300.0), *holes],
    )

    write_polygon(
        ROOT / "input_high_vertex_wavy_ring_720.csv",
        [wavy_ring(0.0, 0.0, 200.0, 24.0, 18, 720)],
    )

    write_polygon(
        ROOT / "input_near_degenerate_corrugated_strip_122.csv",
        [thin_corrugated_strip(0.0, 420.0, 0.0, 2.2, 60, 0.18)],
    )


if __name__ == "__main__":
    main()
