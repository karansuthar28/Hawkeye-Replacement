#include "predictor.h"

#include <stdexcept>

HawkeyePredictor::HawkeyePredictor(std::size_t num_entries, int counter_bits)
    : counter_max_(0), friendly_threshold_(0)
{
  if (num_entries == 0)
    throw std::invalid_argument("HawkeyePredictor: num_entries must be positive");
  if (counter_bits <= 0 || counter_bits >= 31)
    throw std::invalid_argument("HawkeyePredictor: counter_bits must be in [1, 30]");

  counter_max_ = (1 << counter_bits) - 1;
  friendly_threshold_ = 1 << (counter_bits - 1);

  // Weakly cache-friendly. This makes an unseen PC predict friendly, while a
  // single negative OPT label moves it just below the high-bit threshold.
  counters_.assign(num_entries, friendly_threshold_);
}

std::size_t HawkeyePredictor::index_for(uint64_t pc) const
{
  // Hawkeye's reference implementation hashes PCs with the reflected CRC-32
  // polynomial 0xEDB88320, then indexes the finite predictor table.  Using the
  // same compact hash preserves the paper's intended aliasing behavior while
  // keeping this component independent of ChampSim.
  uint64_t hash = pc;
  constexpr uint64_t polynomial = 0xEDB88320ULL;
  for (int bit = 0; bit < 32; ++bit)
    hash = (hash & 1ULL) ? ((hash >> 1) ^ polynomial) : (hash >> 1);

  return static_cast<std::size_t>(hash % counters_.size());
}

void HawkeyePredictor::train(uint64_t pc, bool opt_hit)
{
  int& counter = counters_[index_for(pc)];
  if (opt_hit) {
    if (counter < counter_max_)
      ++counter;
  } else {
    if (counter > 0)
      --counter;
  }
}

bool HawkeyePredictor::predict(uint64_t pc) const
{
  return counters_[index_for(pc)] >= friendly_threshold_;
}

int HawkeyePredictor::get_counter(uint64_t pc) const
{
  return counters_[index_for(pc)];
}
