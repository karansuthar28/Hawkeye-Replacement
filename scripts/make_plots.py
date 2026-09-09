#!/usr/bin/env python3
"""Generate the two plots required by HPCA Assignment 01."""
from __future__ import annotations

import csv
from pathlib import Path

try:
    import matplotlib.pyplot as plt
except ImportError as exc:
    raise SystemExit("matplotlib is required: python3 -m pip install matplotlib") from exc

ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results"


def read_csv(path: Path):
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def plot1(rows):
    hmmer = [r for r in rows if r["benchmark"] == "456.hmmer-191B"]
    fig, ax = plt.subplots(figsize=(7.2, 4.8))
    for policy in ("lru", "hawkeye"):
        ps = sorted((r for r in hmmer if r["policy"] == policy), key=lambda r: int(r["ways"]))
        ax.plot(
            [int(r["ways"]) for r in ps],
            [float(r["miss_rate_percent"]) for r in ps],
            marker="o",
            linewidth=2,
            label=policy.upper() if policy == "lru" else "Hawkeye",
        )
    ax.set_xlabel("LLC associativity (ways)")
    ax.set_ylabel("LLC miss rate (%)")
    ax.set_title("LLC miss rate vs. associativity - 456.hmmer-191B")
    ax.set_xticks([4, 8, 16])
    ax.grid(True, alpha=0.25)
    ax.legend()
    fig.tight_layout()
    fig.savefig(RESULTS / "plot1_llc_miss_rate_vs_associativity.png", dpi=220)
    fig.savefig(RESULTS / "plot1_llc_miss_rate_vs_associativity.pdf")
    plt.close(fig)


def plot2(rows):
    labels = ["456.hmmer-191B", "429.mcf-22B", "473.astar-42B"]
    by_bench = {r["benchmark"]: float(r["miss_rate_reduction_percent"]) for r in rows}
    values = [by_bench[b] for b in labels]

    fig, ax = plt.subplots(figsize=(7.2, 4.8))
    bars = ax.bar(labels, values)
    ax.axhline(0, linewidth=0.8)
    ax.set_ylabel("Miss-rate reduction over LRU (%)")
    ax.set_title("Hawkeye LLC miss-rate reduction over LRU - 16-way LLC")
    ax.tick_params(axis="x", rotation=15)
    ax.grid(True, axis="y", alpha=0.25)
    for bar, value in zip(bars, values):
        y = bar.get_height()
        ax.text(
            bar.get_x() + bar.get_width() / 2,
            y,
            f"{value:.2f}%",
            ha="center",
            va="bottom" if y >= 0 else "top",
        )
    fig.tight_layout()
    fig.savefig(RESULTS / "plot2_miss_rate_reduction_over_lru.png", dpi=220)
    fig.savefig(RESULTS / "plot2_miss_rate_reduction_over_lru.pdf")
    plt.close(fig)


def main():
    results = read_csv(RESULTS / "results.csv")
    reductions = read_csv(RESULTS / "reduction.csv")
    plot1(results)
    plot2(reductions)
    print("Generated both required plots in results/")


if __name__ == "__main__":
    main()
