#ifndef REPLACEMENT_HAWKEYE_OPTGEN_H
#define REPLACEMENT_HAWKEYE_OPTGEN_H

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

class OPTgen 
{
  public:
  // num_sets: number of cache sets tracked independently
  // associativity: W, the cache associativity (occupancy vector cap)
  // history_multiplier: length of tracked history, in units of the set's
  // capacity (paper uses 8x; see Figure 2).
  OPTgen(std::size_t num_sets, std::size_t associativity, std::size_t history_multiplier = 8);

  // Processes one access to `address`, mapped to set `set_idx`, per
  bool access(std::size_t set_idx, uint64_t address);

  private:
  struct SetState 
  {
    explicit SetState(std::size_t history_length);
    
    std::vector<std::size_t> occupancy;
    std::vector<uint64_t> slot_address;
    std::vector<uint64_t> slot_timestamp;
    std::vector<unsigned char> slot_valid;
    std::unordered_map<uint64_t, uint64_t> last_access;
    uint64_t time = 0;
  };
  
  std::size_t associativity_;
  std::size_t history_length_;
  std::vector<SetState> sets_;
};

#endif
