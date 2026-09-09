#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
mkdir -p traces
BASE="https://dpc3.compas.cs.stonybrook.edu/champsim-traces/speccpu"
TRACES=(
  "456.hmmer-191B.champsimtrace.xz"
  "429.mcf-22B.champsimtrace.xz"
  "473.astar-42B.champsimtrace.xz"
)

for name in "${TRACES[@]}"; do
  if [[ -s "traces/$name" ]]; then
    echo "SKIP existing traces/$name"
    continue
  fi
  echo "Downloading $name ..."
  if command -v wget >/dev/null 2>&1; then
    wget -c "$BASE/$name" -O "traces/$name"
  elif command -v curl >/dev/null 2>&1; then
    curl -fL -C - "$BASE/$name" -o "traces/$name"
  else
    echo "ERROR: install wget or curl first" >&2
    exit 1
  fi
done

echo "All required traces are present in $ROOT/traces"
