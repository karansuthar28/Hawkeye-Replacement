#!/usr/bin/env python3
"""Build a compact submission-ready PDF report from measured ChampSim results."""
from __future__ import annotations

import csv
import os
from pathlib import Path

try:
    import matplotlib.image as mpimg
    import matplotlib.pyplot as plt
    from matplotlib.backends.backend_pdf import PdfPages
except ImportError as exc:
    raise SystemExit("matplotlib is required: python3 -m pip install matplotlib") from exc

ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results"
REPORT_DIR = ROOT / "report"
REPORT_DIR.mkdir(exist_ok=True)


def read_csv(path: Path):
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def miss(rows, bench, ways, policy):
    for r in rows:
        if r["benchmark"] == bench and int(r["ways"]) == ways and r["policy"] == policy:
            return float(r["miss_rate_percent"])
    raise KeyError((bench, ways, policy))


def pct_reduction(lru, hawkeye):
    return (lru - hawkeye) / lru * 100.0


def add_text_page(pdf, title, blocks, footer=None):
    fig = plt.figure(figsize=(8.27, 11.69))
    ax = fig.add_axes([0.08, 0.06, 0.84, 0.88])
    ax.axis("off")
    y = 0.98
    ax.text(0.0, y, title, fontsize=18, fontweight="bold", va="top")
    y -= 0.07
    for heading, text in blocks:
        ax.text(0.0, y, heading, fontsize=12.5, fontweight="bold", va="top")
        y -= 0.035
        ax.text(0.02, y, text, fontsize=10.5, va="top", wrap=True, linespacing=1.35)
        lines = max(2, len(text) // 95 + 1)
        y -= 0.032 * lines + 0.025
    if footer:
        ax.text(0.0, 0.005, footer, fontsize=8.5, va="bottom")
    pdf.savefig(fig, bbox_inches="tight")
    plt.close(fig)


def add_figure_page(pdf, title, image_path, caption):
    fig = plt.figure(figsize=(8.27, 11.69))
    ax = fig.add_axes([0.08, 0.17, 0.84, 0.68])
    ax.axis("off")
    img = mpimg.imread(image_path)
    ax.imshow(img)
    fig.text(0.08, 0.93, title, fontsize=18, fontweight="bold", va="top")
    fig.text(0.08, 0.10, caption, fontsize=10.5, va="top", wrap=True)
    pdf.savefig(fig, bbox_inches="tight")
    plt.close(fig)


def main():
    rows = read_csv(RESULTS / "results.csv")
    reductions = read_csv(RESULTS / "reduction.csv")

    student_name = os.environ.get("STUDENT_NAME", "Student: ____________________")
    student_id = os.environ.get("STUDENT_ID", "Student ID: ____________________")

    hmmer_reductions = []
    for ways in (4, 8, 16):
        lru = miss(rows, "456.hmmer-191B", ways, "lru")
        hawk = miss(rows, "456.hmmer-191B", ways, "hawkeye")
        hmmer_reductions.append((ways, lru, hawk, pct_reduction(lru, hawk)))

    reduction_map = {r["benchmark"]: float(r["miss_rate_reduction_percent"]) for r in reductions}
    best_bench = max(reduction_map, key=reduction_map.get)
    best_value = reduction_map[best_bench]
    mean_reduction = sum(reduction_map.values()) / len(reduction_map)

    assoc_text = "; ".join(
        f"{ways}-way: LRU {lru:.3f}%, Hawkeye {hawk:.3f}% ({red:.2f}% relative reduction)"
        for ways, lru, hawk, red in hmmer_reductions
    )

    blocks = [
        (
            "Objective",
            "Implement Hawkeye as a ChampSim LLC replacement policy using three independently testable components: "
            "OPTgen for reconstructing past OPT decisions, a PC-indexed saturating-counter predictor, and RRIP-based replacement state.",
        ),
        (
            "Learning / Observation 1 - OPTgen models demand, not reuse distance alone",
            "A reuse interval is cacheable only when every occupancy-vector position in that interval has capacity. "
            "This made the key implementation point clear: a long reuse can still be cache-friendly under low cache demand, while a shorter reuse can be rejected under heavy overlap.",
        ),
        (
            "Learning / Observation 2 - Per-PC learning converts an oracle into an online policy",
            "OPTgen can label only past behavior, but the 3-bit predictor transfers that information to future accesses from the same load PC. "
            "Positive OPT labels increment the counter, negative labels decrement it, and the high-order bit directly selects cache-friendly versus cache-averse behavior.",
        ),
        (
            "Learning / Observation 3 - RRIP combines prediction with adaptation",
            f"Cache-friendly lines enter at RRPV 0 and cache-averse lines at RRPV 7, while aging provides adaptation when no immediate victim exists. "
            f"Measured 456.hmmer results were: {assoc_text}. Across the three default-LLC benchmarks, the mean Hawkeye miss-rate reduction over LRU was {mean_reduction:.2f}%; the largest was {best_bench} at {best_value:.2f}%.",
        ),
        (
            "Experimental methodology",
            "All measurements use 20,000,000 warmup instructions and 50,000,000 simulation instructions. Plot 1 keeps LLC capacity fixed at 2 MiB with 64 B lines while sweeping 4/8/16 ways (8192/4096/2048 sets). "
            "Plot 2 uses the default 2048-set, 16-way LLC and reports (missrate_LRU - missrate_Hawkeye) / missrate_LRU x 100.",
        ),
    ]

    out = REPORT_DIR / "HPCA_Assignment_01_Hawkeye_Report.pdf"
    with PdfPages(out) as pdf:
        add_text_page(
            pdf,
            "HPCA Assignment 01 - Hawkeye Replacement Policy in ChampSim",
            blocks,
            footer=f"{student_name}    |    {student_id}    |    IISc CSA",
        )
        add_figure_page(
            pdf,
            "Plot 1 - LLC miss rate vs. LLC associativity",
            RESULTS / "plot1_llc_miss_rate_vs_associativity.png",
            "Trace: 456.hmmer-191B.champsimtrace.xz. LLC capacity is held at 2 MiB by changing the number of sets inversely with associativity. Both LRU and Hawkeye use identical warmup and simulation lengths.",
        )
        add_figure_page(
            pdf,
            "Plot 2 - Hawkeye miss-rate reduction over LRU",
            RESULTS / "plot2_miss_rate_reduction_over_lru.png",
            "Default LLC: 2048 sets x 16 ways x 64 B = 2 MiB. Positive values mean Hawkeye has a lower LLC miss rate than LRU. Values are computed directly from the parsed ChampSim LLC TOTAL statistics.",
        )

    print(f"Wrote {out}")


if __name__ == "__main__":
    main()
