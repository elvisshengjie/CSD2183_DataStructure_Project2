import csv
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
CASE_FILE = ROOT / "benchmarks" / "cases.csv"
RESULT_DIR = ROOT / "benchmarks" / "results"
RESULT_FILE = RESULT_DIR / "benchmark_results.csv"


def count_input_vertices(path: Path) -> int:
    with path.open(newline="", encoding="utf-8") as handle:
        return sum(1 for _ in csv.DictReader(handle))


def parse_output(stdout: str) -> tuple[int, float | None]:
    output_vertices = 0
    displacement = None
    for line in stdout.splitlines():
        if line.startswith("ring_id,vertex_id,x,y") or not line:
            continue
        if line.startswith("Total areal displacement:"):
            displacement = float(line.split(":", 1)[1].strip())
        elif not line.startswith("Total "):
            output_vertices += 1
    return output_vertices, displacement


def run_case(input_path: Path, target_vertices: int) -> tuple[float, int, str]:
    timed_command = (
        f"/usr/bin/time -f '__TIME__ %e %M' ./simplify "
        f"'{input_path.as_posix()}' {target_vertices}"
    )
    completed = subprocess.run(
        ["bash", "-lc", timed_command],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(completed.stderr.strip() or completed.stdout.strip())

    elapsed_seconds = 0.0
    peak_rss_kb = 0
    for line in completed.stderr.splitlines():
        if line.startswith("__TIME__ "):
            _, elapsed_text, rss_text = line.split()
            elapsed_seconds = float(elapsed_text)
            peak_rss_kb = int(rss_text)
            break

    return elapsed_seconds, peak_rss_kb, completed.stdout


def main() -> int:
    RESULT_DIR.mkdir(parents=True, exist_ok=True)

    rows = []
    with CASE_FILE.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            label = row["label"]
            input_path = ROOT / row["input_path"]
            target_vertices = int(row["target_vertices"])
            input_vertices = count_input_vertices(input_path)
            elapsed_seconds, peak_rss_kb, stdout = run_case(input_path, target_vertices)
            output_vertices, displacement = parse_output(stdout)
            rows.append(
                {
                    "label": label,
                    "input_path": row["input_path"],
                    "target_vertices": target_vertices,
                    "input_vertices": input_vertices,
                    "output_vertices": output_vertices,
                    "elapsed_seconds": f"{elapsed_seconds:.6f}",
                    "peak_rss_kb": peak_rss_kb,
                    "reported_areal_displacement": "" if displacement is None else f"{displacement:.15g}",
                    "notes": row["notes"],
                }
            )

    with RESULT_FILE.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=[
                "label",
                "input_path",
                "target_vertices",
                "input_vertices",
                "output_vertices",
                "elapsed_seconds",
                "peak_rss_kb",
                "reported_areal_displacement",
                "notes",
            ],
        )
        writer.writeheader()
        writer.writerows(rows)

    print(f"Wrote benchmark results to {RESULT_FILE}")
    for row in rows:
        print(
            f"{row['label']}: n_in={row['input_vertices']} "
            f"n_out={row['output_vertices']} "
            f"time={row['elapsed_seconds']}s "
            f"rss={row['peak_rss_kb']} KB"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
