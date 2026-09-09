#!/usr/bin/env python3
"""Parse ChampSim LLC TOTAL statistics from HPCA-01 experiment logs."""
from __future__ import annotations

import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results"

# Current ChampSim prints lines such as:
# cpu0->LLC TOTAL ACCESS: 123 HIT: 45 MISS: 78
LLC_RE = re.compile(
    r"(?:cpu\d+->)?LLC\s+TOTAL\s+ACCESS:\s*(\d+)\s+HIT:\s*(\d+)\s+MISS:\s*(\d+)",
    re.IGNORECASE,
)

EXPECTED = [
    ("456.hmmer-191B", 4, "lru", "hmmer_lru_4way.log"),
    ("456.hmmer-191B", 4, "hawkeye", "hmmer_hawkeye_4way.log"),
    ("456.hmmer-191B", 8, "lru", "hmmer_lru_8way.log"),
    ("456.hmmer-191B", 8, "hawkeye", "hmmer_hawkeye_8way.log"),
    ("456.hmmer-191B", 16, "lru", "hmmer_lru_16way.log"),
    ("456.hmmer-191B", 16, "hawkeye", "hmmer_hawkeye_16way.log"),
    ("429.mcf-22B", 16, "lru", "mcf_lru_16way.log"),
    ("429.mcf-22B", 16, "hawkeye", "mcf_hawkeye_16way.log"),
    ("473.astar-42B", 16, "lru", "astar_lru_16way.log"),
    ("473.astar-42B", 16, "hawkeye", "astar_hawkeye_16way.log"),
]


def parse_one(path: Path) -> tuple[int, int, int]:
    if not path.exists():
        raise FileNotFoundError(f"missing log: {path}")
    text = path.read_text(errors="replace")
    matches = list(LLC_RE.finditer(text))
    if not matches:
        # Give a useful diagnostic without guessing the miss rate.
        llc_lines = [line for line in text.splitlines() if "LLC" in line and "TOTAL" in line]
        hint = "\n".join(llc_lines[-5:]) if llc_lines else "(no LLC TOTAL lines found)"
        raise RuntimeError(f"could not parse LLC TOTAL stats from {path}\nLast candidate lines:\n{hint}")
    m = matches[-1]
    return tuple(int(m.group(i)) for i in range(1, 4))  # access, hit, miss


def main() -> int:
    RESULTS.mkdir(exist_ok=True)
    rows = []
    try:
        for benchmark, ways, policy, filename in EXPECTED:
            access, hit, miss = parse_one(RESULTS / filename)
            if access <= 0:
                raise RuntimeError(f"zero LLC accesses in {filename}")
            if hit + miss != access:
                print(
                    f"warning: {filename}: HIT+MISS={hit+miss} differs from ACCESS={access}",
                    file=sys.stderr,
                )
            miss_rate = 100.0 * miss / access
            rows.append(
                {
                    "benchmark": benchmark,
                    "ways": ways,
                    "policy": policy,
                    "accesses": access,
                    "hits": hit,
                    "misses": miss,
                    "miss_rate_percent": miss_rate,
                    "log": filename,
                }
            )
    except (FileNotFoundError, RuntimeError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    out_csv = RESULTS / "results.csv"
    with out_csv.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)

    # Plot-2 reductions use the default 16-way LLC.
    reduction_rows = []
    for benchmark in ("456.hmmer-191B", "429.mcf-22B", "473.astar-42B"):
        by_policy = {
            row["policy"]: row
            for row in rows
            if row["benchmark"] == benchmark and row["ways"] == 16
        }
        lru = float(by_policy["lru"]["miss_rate_percent"])
        hawkeye = float(by_policy["hawkeye"]["miss_rate_percent"])
        if lru == 0.0:
            raise RuntimeError(f"LRU miss rate is zero for {benchmark}; reduction is undefined")
        reduction = (lru - hawkeye) / lru * 100.0
        reduction_rows.append(
            {
                "benchmark": benchmark,
                "lru_miss_rate_percent": lru,
                "hawkeye_miss_rate_percent": hawkeye,
                "miss_rate_reduction_percent": reduction,
            }
        )

    reduction_csv = RESULTS / "reduction.csv"
    with reduction_csv.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=reduction_rows[0].keys())
        writer.writeheader()
        writer.writerows(reduction_rows)

    print(f"Wrote {out_csv}")
    print(f"Wrote {reduction_csv}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
