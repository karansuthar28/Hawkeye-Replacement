#include "rrip.h"

#include <stdexcept>

namespace {
constexpr int MAX_RRPV = 7;
constexpr int MAX_FRIENDLY_RRPV = 6;
}

void update_rrpv(std::vector<int>& rrpv, std::size_t way,
                 Classification cls, bool is_hit)
{
  if (way >= rrpv.size())
    throw std::out_of_range("update_rrpv: way out of range");

  if (cls == Classification::CACHE_AVERSE) {
    rrpv[way] = MAX_RRPV;
    return;
  }

  if (!is_hit) {
    // Table 1: on a cache-friendly miss, age the other cache-friendly lines.
    // Friendly lines are capped at 6 so that value 7 remains reserved for the
    // strongest eviction priority.
    for (std::size_t i = 0; i < rrpv.size(); ++i) {
      if (i != way && rrpv[i] < MAX_FRIENDLY_RRPV)
        ++rrpv[i];
    }
  }

  // Cache-friendly hits and insertions receive highest priority.
  rrpv[way] = 0;
}

std::size_t find_victim(std::vector<int>& rrpv)
{
  if (rrpv.empty())
    throw std::invalid_argument("find_victim: RRPV vector must not be empty");

  for (;;) {
    for (std::size_t way = 0; way < rrpv.size(); ++way) {
      if (rrpv[way] >= MAX_RRPV)
        return way;
    }

    // Standard RRIP aging: when no line is at maximum RRPV, age the set until
    // at least one line becomes eligible. Values saturate at 7.
    for (int& value : rrpv) {
      if (value < MAX_RRPV)
        ++value;
    }
  }
}
