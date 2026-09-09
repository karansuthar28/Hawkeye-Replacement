#!/usr/bin/env bash
set -euo pipefail

# Run from anywhere after this package has been copied into the ChampSim root.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

WARMUP=20000000
SIM=50000000
JOBS="${JOBS:-$(nproc)}"
FORCE="${FORCE:-0}"
mkdir -p results

if [[ ! -x ./config.sh ]]; then
  echo "ERROR: config.sh not found. Copy this package into the root of your ChampSim checkout." >&2
  exit 1
fi

for trace in \
  traces/456.hmmer-191B.champsimtrace.xz \
  traces/429.mcf-22B.champsimtrace.xz \
  traces/473.astar-42B.champsimtrace.xz; do
  if [[ ! -f "$trace" ]]; then
    echo "ERROR: missing required trace: $trace" >&2
    exit 1
  fi
done

build_config() {
  local cfg="$1"
  echo
  echo "============================================================"
  echo "CONFIGURE: $cfg"
  echo "============================================================"
  ./config.sh "$cfg"
  make -j"$JOBS"
  test -x bin/champsim || { echo "ERROR: bin/champsim was not produced" >&2; exit 1; }
}

run_trace() {
  local trace="$1"
  local log="$2"
  if [[ -s "$log" && "$FORCE" != "1" ]]; then
    echo "SKIP existing $log (set FORCE=1 to rerun)"
    return
  fi
  echo
  echo "RUN: $trace"
  echo "LOG: $log"
  bin/champsim \
    --warmup_instructions "$WARMUP" \
    --simulation_instructions "$SIM" \
    "$trace" 2>&1 | tee "$log"
}

# Plot 1: 456.hmmer at 4, 8, 16 ways under LRU and Hawkeye.
build_config configs/lru_4way.json
run_trace traces/456.hmmer-191B.champsimtrace.xz results/hmmer_lru_4way.log

build_config configs/hawkeye_4way.json
run_trace traces/456.hmmer-191B.champsimtrace.xz results/hmmer_hawkeye_4way.log

build_config configs/lru_8way.json
run_trace traces/456.hmmer-191B.champsimtrace.xz results/hmmer_lru_8way.log

build_config configs/hawkeye_8way.json
run_trace traces/456.hmmer-191B.champsimtrace.xz results/hmmer_hawkeye_8way.log

# Default 2 MiB, 16-way configuration. Reuse the hmmer run for Plot 2 and run
# mcf/astar under the same binary to avoid unnecessary rebuilds.
build_config configs/lru_16way.json
run_trace traces/456.hmmer-191B.champsimtrace.xz results/hmmer_lru_16way.log
run_trace traces/429.mcf-22B.champsimtrace.xz results/mcf_lru_16way.log
run_trace traces/473.astar-42B.champsimtrace.xz results/astar_lru_16way.log

build_config configs/hawkeye_16way.json
run_trace traces/456.hmmer-191B.champsimtrace.xz results/hmmer_hawkeye_16way.log
run_trace traces/429.mcf-22B.champsimtrace.xz results/mcf_hawkeye_16way.log
run_trace traces/473.astar-42B.champsimtrace.xz results/astar_hawkeye_16way.log

python3 scripts/parse_results.py
python3 scripts/make_plots.py
python3 scripts/build_report.py

echo
echo "DONE. Results, plots, and report are under: $ROOT/results and $ROOT/report"
