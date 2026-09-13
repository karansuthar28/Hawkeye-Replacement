#ifndef REPLACEMENT_HAWKEYE_PREDICTOR_H
#define REPLACEMENT_HAWKEYE_PREDICTOR_H

#include <cstddef>
#include <cstdint>
#include <vector>

class HawkeyePredictor 
{
  public:
  HawkeyePredictor(std::size_t num_entries = 8192, int counter_bits = 3);

  void train(uint64_t pc, bool opt_hit);
  bool predict(uint64_t pc) const;
  int get_counter(uint64_t pc) const;

  private:
  std::size_t pcIndex(uint64_t pc) const; // PC -> Index Onto Counter Table
  std::vector<int> ctrTable; // Counter Table
  int maxCounter; // Maximum value of counter can be i.e. 7
  int threshold; // We need boundary to differentiate between cache averse and cache friendly i.e. 4 (cache weakly friendly)
};

#endif
