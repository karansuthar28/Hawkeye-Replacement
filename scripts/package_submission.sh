#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

bash scripts/component_tests.sh
bash scripts/verify_no_debug_prints.sh

OUT="${1:-HPCA01_Hawkeye_Submission_Code.zip}"
rm -f "$OUT"
zip -r "$OUT" \
  replacement/hawkeye \
  hawkeye_tests \
  hawkeye_config.json \
  configs \
  -x '*/__pycache__/*' '*.o' '*.d' '*.a' '*.so' '*.out'

echo "Created $ROOT/$OUT"
