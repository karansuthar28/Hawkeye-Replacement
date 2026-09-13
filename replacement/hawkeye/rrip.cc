#include "rrip.h"

#include <stdexcept>

namespace {
constexpr int maxRRPV = 7;
constexpr int maxFriendlyRRPV = 6;
}

void update_rrpv(std::vector<int>& rrpv, std::size_t way, Classification cls, bool is_hit)
{
  if (way >= rrpv.size())
    throw std::out_of_range("update_rrpv: way out of range");

  if (cls == Classification::CACHE_AVERSE) {
    rrpv[way] = maxRRPV;
    return;
  }

  // Cache Friendly
  if (!is_hit) 
  {
    for (std::size_t i = 0; i < rrpv.size(); ++i) 
      if (i != way && rrpv[i] < maxFriendlyRRPV) ++rrpv[i];
  }

  // Cache-friendly hits and insertions receive highest priority.
  rrpv[way] = 0;
}

std::size_t find_victim(std::vector<int>& rrpv)
{
  if (rrpv.empty())
    throw std::invalid_argument("find_victim: RRPV vector must not be empty");

  while(1) {
    for (std::size_t way = 0; way < rrpv.size(); ++way) 
      if (rrpv[way] == maxRRPV) return way;

    for (int& value : rrpv)
      value++;
  }
}
