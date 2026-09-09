#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
TRACE="${1:-traces/456.hmmer-191B.champsimtrace.xz}"
if [[ ! -f "$TRACE" ]]; then
  echo "ERROR: trace not found: $TRACE" >&2
  exit 1
fi
./config.sh hawkeye_config.json
make -j"${JOBS:-$(nproc)}"
bin/champsim --warmup_instructions 1000000 --simulation_instructions 2000000 "$TRACE"
