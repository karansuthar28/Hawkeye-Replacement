#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if grep -RInE 'std::(cout|cerr)|printf\s*\(|fmt::print' "$ROOT/replacement/hawkeye"; then
  echo "ERROR: debug/output statement found in replacement/hawkeye" >&2
  exit 1
fi
echo "PASS: no debug print statements in replacement/hawkeye"
