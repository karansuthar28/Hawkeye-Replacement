#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${TMPDIR:-/tmp}/hpca01_hawkeye_component_tests"
rm -rf "$BUILD"
mkdir -p "$BUILD"

CXX="${CXX:-g++}"
FLAGS=(-std=c++17 -Wall -Wextra -Werror -pedantic)

# Compile the three grading-driver skeletons exactly as shipped. The RRIP
# skeleton is intentionally not executed because the grader injects its vectors.
"$CXX" "${FLAGS[@]}" "$ROOT/hawkeye_tests/optgen_test.cc" "$ROOT/replacement/hawkeye/optgen.cc" -o "$BUILD/optgen_test"
"$CXX" "${FLAGS[@]}" "$ROOT/hawkeye_tests/predictor_test.cc" "$ROOT/replacement/hawkeye/predictor.cc" -o "$BUILD/predictor_test"
"$CXX" "${FLAGS[@]}" "$ROOT/hawkeye_tests/rrip_test.cc" "$ROOT/replacement/hawkeye/rrip.cc" -o "$BUILD/rrip_test"

cat > "$BUILD/self_test.cc" <<'CPP'
#include "optgen.h"
#include "predictor.h"
#include "rrip.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

int main()
{
  // Figure-6-style sequence for a 2-way cache.
  {
    OPTgen opt(1, 2);
    std::vector<uint64_t> seq = {'A','B','B','C','D','E','A','F','D','E','F','C'};
    std::vector<bool> expected = {false,false,true,false,false,false,true,false,true,false,true,false};
    for (std::size_t i = 0; i < seq.size(); ++i)
      assert(opt.access(0, seq[i]) == expected[i]);
  }

  // Exact history-window boundary and independent sets.
  {
    OPTgen opt(2, 1, 2);
    assert(!opt.access(0, 1));
    assert(!opt.access(0, 2));
    assert(opt.access(0, 1));
    assert(!opt.access(1, 9));
    assert(opt.access(1, 9));
  }

  // Predictor threshold and saturation.
  {
    HawkeyePredictor p;
    const uint64_t pc = 0x1234;
    assert(p.get_counter(pc) == 4 && p.predict(pc));
    p.train(pc, false);
    assert(p.get_counter(pc) == 3 && !p.predict(pc));
    for (int i = 0; i < 20; ++i) p.train(pc, false);
    assert(p.get_counter(pc) == 0);
    for (int i = 0; i < 20; ++i) p.train(pc, true);
    assert(p.get_counter(pc) == 7 && p.predict(pc));
  }

  // RRIP update and victim aging.
  {
    std::vector<int> r = {0, 2, 5, 6};
    update_rrpv(r, 0, Classification::CACHE_FRIENDLY, false);
    assert((r == std::vector<int>{0, 3, 6, 6}));
    update_rrpv(r, 1, Classification::CACHE_AVERSE, true);
    assert(find_victim(r) == 1);
  }
  {
    std::vector<int> r = {0, 1, 2, 3};
    assert(find_victim(r) == 3);
    assert((r == std::vector<int>{4, 5, 6, 7}));
  }

  std::cout << "ALL COMPONENT SELF-TESTS PASSED\n";
}
CPP

"$CXX" "${FLAGS[@]}" -I"$ROOT/replacement/hawkeye" \
  "$BUILD/self_test.cc" \
  "$ROOT/replacement/hawkeye/optgen.cc" \
  "$ROOT/replacement/hawkeye/predictor.cc" \
  "$ROOT/replacement/hawkeye/rrip.cc" \
  -o "$BUILD/self_test"

"$BUILD/self_test"
echo "All three required grading skeletons compile cleanly."
