import argparse
import csv
import math
import sys
from dataclasses import dataclass
from pathlib import Path

from shapely.errors import GEOSException
from shapely.geometry import LinearRing, Polygon
from shapely.validation import explain_validity


@dataclass
class Metrics:
    reported_input_area: float | None = None
    reported_output_area: float | None = None
    reported_displacement: float | None = None


def signed_area(ring: list[tuple[float, float]]) -> float:
    area = 0.0
    for i, (x1, y1) in enumerate(ring):
        x2, y2 = ring[(i + 1) % len(ring)]
        area += x1 * y2 - x2 * y1
    return area / 2.0


def within_tolerance(actual: float, expected: float, abs_tol: float, rel_tol: float) -> bool:
    return math.isclose(actual, expected, abs_tol=abs_tol, rel_tol=rel_tol)


def parse_input_csv(path: Path) -> list[list[tuple[float, float]]]:
    rings: dict[int, list[tuple[float, float]]] = {}
    with path.open(newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            rings.setdefault(int(row["ring_id"]), []).append(
                (float(row["x"]), float(row["y"]))
            )
    return [rings[idx] for idx in sorted(rings)]


def parse_program_output(path: Path) -> tuple[list[list[tuple[float, float]]], Metrics]:
    rings: dict[int, list[tuple[float, float]]] = {}
    metrics = Metrics()

    with path.open(encoding="utf-8") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if not line or line == "ring_id,vertex_id,x,y":
                continue
            if line.startswith("Total signed area in input:"):
                metrics.reported_input_area = float(line.split(":", 1)[1])
                continue
            if line.startswith("Total signed area in output:"):
                metrics.reported_output_area = float(line.split(":", 1)[1])
                continue
            if line.startswith("Total areal displacement:"):
                metrics.reported_displacement = float(line.split(":", 1)[1])
                continue

            ring_id, _, x, y = line.split(",")
            rings.setdefault(int(ring_id), []).append((float(x), float(y)))

    return [rings[idx] for idx in sorted(rings)], metrics


def build_polygon(rings: list[list[tuple[float, float]]]) -> Polygon:
    if not rings:
        raise ValueError("No rings found in output.")
    return Polygon(rings[0], rings[1:])


def boundary_pair_has_intersection(
    lhs: list[tuple[float, float]], rhs: list[tuple[float, float]]
) -> bool:
    intersection = LinearRing(lhs).intersection(LinearRing(rhs))
    return not intersection.is_empty


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate a simplify output against the clarified project rubric."
    )
    parser.add_argument("--input", required=True, type=Path, help="Input CSV path")
    parser.add_argument("--output", required=True, type=Path, help="Program output path")
    parser.add_argument("--target", type=int, default=None, help="Requested vertex count")
    parser.add_argument("--abs-tol", type=float, default=1e-4, help="Absolute tolerance")
    parser.add_argument("--rel-tol", type=float, default=1e-9, help="Relative tolerance")
    args = parser.parse_args()

    input_rings = parse_input_csv(args.input)
    output_rings, metrics = parse_program_output(args.output)

    failures: list[str] = []
    warnings: list[str] = []

    if len(input_rings) != len(output_rings):
        failures.append(
            f"Ring count changed: input={len(input_rings)} output={len(output_rings)}"
        )

    for idx, ring in enumerate(output_rings):
        if len(ring) < 3:
            failures.append(f"Ring {idx} has fewer than 3 vertices.")

    if len(input_rings) == len(output_rings):
        for idx, (in_ring, out_ring) in enumerate(zip(input_rings, output_rings)):
            in_area = signed_area(in_ring)
            out_area = signed_area(out_ring)
            if not within_tolerance(out_area, in_area, args.abs_tol, args.rel_tol):
                failures.append(
                    f"Ring {idx} signed area changed too much: input={in_area:.12f} output={out_area:.12f}"
                )

    for idx, ring in enumerate(output_rings):
        lr = LinearRing(ring)
        if idx == 0 and not lr.is_ccw:
            failures.append("Exterior ring is not counterclockwise.")
        if idx > 0 and lr.is_ccw:
            failures.append(f"Interior ring {idx} is not clockwise.")
        if not lr.is_simple:
            failures.append(f"Ring {idx} self-intersects.")

    for i in range(len(output_rings)):
        for j in range(i + 1, len(output_rings)):
            if boundary_pair_has_intersection(output_rings[i], output_rings[j]):
                failures.append(f"Ring {i} intersects ring {j}.")

    output_polygon = build_polygon(output_rings)
    if not output_polygon.is_valid:
        failures.append(f"Output polygon is invalid: {explain_validity(output_polygon)}")

    input_total_signed = sum(signed_area(ring) for ring in input_rings)
    output_total_signed = sum(signed_area(ring) for ring in output_rings)

    if metrics.reported_input_area is None:
        failures.append("Missing reported input signed area.")
    elif not within_tolerance(
        metrics.reported_input_area, input_total_signed, args.abs_tol, args.rel_tol
    ):
        failures.append(
            f"Reported input signed area is inconsistent: reported={metrics.reported_input_area:.12f} computed={input_total_signed:.12f}"
        )

    if metrics.reported_output_area is None:
        failures.append("Missing reported output signed area.")
    elif not within_tolerance(
        metrics.reported_output_area, output_total_signed, args.abs_tol, args.rel_tol
    ):
        failures.append(
            f"Reported output signed area is inconsistent: reported={metrics.reported_output_area:.12f} computed={output_total_signed:.12f}"
        )

    input_polygon = build_polygon(input_rings)
    if metrics.reported_displacement is None:
        failures.append("Missing reported areal displacement.")
    else:
        try:
            actual_displacement = input_polygon.symmetric_difference(output_polygon).area
            if not within_tolerance(
                metrics.reported_displacement,
                actual_displacement,
                args.abs_tol,
                args.rel_tol,
            ):
                failures.append(
                    f"Reported displacement is inconsistent: reported={metrics.reported_displacement:.12f} actual={actual_displacement:.12f}"
                )
        except GEOSException as exc:
            failures.append(f"Could not compute symmetric difference: {exc}")

    vertex_count = sum(len(ring) for ring in output_rings)
    if args.target is not None and vertex_count > args.target:
        warnings.append(
            f"Vertex count exceeds target: output={vertex_count} target={args.target}. Manual greedy-removability review still needed."
        )

    if args.target is not None and vertex_count == args.target:
        warnings.append("Vertex count hits the requested target exactly.")

    status = "PASS" if not failures else "FAIL"
    print(f"{status}: {args.output}")
    print(f"Input rings: {len(input_rings)} | Output rings: {len(output_rings)}")
    print(f"Output vertices: {vertex_count}")

    if warnings:
        for warning in warnings:
            print(f"WARNING: {warning}")

    if failures:
        for failure in failures:
            print(f"ERROR: {failure}")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
