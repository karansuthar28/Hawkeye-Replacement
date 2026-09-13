#include "predictor.h"

#include <stdexcept>

HawkeyePredictor::HawkeyePredictor(std::size_t num_entries, int counter_bits): maxCounter(0), threshold(0) {
  if (num_entries == 0)
    throw std::invalid_argument("HawkeyePredictor: num_entries must be positive");

  if (counter_bits <= 0 or counter_bits >= 15)
    throw std::invalid_argument("HawkeyePredictor: Invalid counter_bits");

  maxCounter = (1 << counter_bits) - 1; // (1<<3) - 1 = 7
  threshold = 1 << (counter_bits - 1); // 1 << (3-1) = 4

  // Initially every PC is weakly cache-friendly.
  ctrTable.assign(num_entries, threshold);
}

std::size_t HawkeyePredictor::pcIndex(uint64_t pc) const
{
  uint64_t value = pc;

  constexpr uint64_t crc = 0xEDB88320; // crc = crcPolynomial 

  for (int bit = 0; bit < 32; ++bit)
  {
    if (value & 1) value = (value >> 1) ^ crc;
    else value = value >> 1;
  }

  return static_cast<std::size_t>(value % ctrTable.size());
}


void HawkeyePredictor::train(uint64_t pc, bool opt_hit)
{
    int& ctrValue = ctrTable[pcIndex(pc)]; // ctrValue
    opt_hit ? (ctrValue < maxCounter ? ++ctrValue : 0) : (ctrValue > 0 ?  --ctrValue : 0); 
}

bool HawkeyePredictor::predict(uint64_t pc) const
{
    return ctrTable[pcIndex(pc)] >= threshold;
}

int HawkeyePredictor::get_counter(uint64_t pc) const
{
    return ctrTable[pcIndex(pc)];
}