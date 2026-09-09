#ifndef REPLACEMENT_HAWKEYE_HAWKEYE_H
#define REPLACEMENT_HAWKEYE_HAWKEYE_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "cache.h"
#include "modules.h"
#include "optgen.h"
#include "predictor.h"
#include "rrip.h"

struct hawkeye : public champsim::modules::replacement {
  explicit hawkeye(CACHE* cache);

  long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set,
                   const champsim::cache_block* current_set,
                   champsim::address ip, champsim::address full_addr,
                   access_type type);

  void replacement_cache_fill(uint32_t triggering_cpu, long set, long way,
                              champsim::address full_addr,
                              champsim::address ip,
                              champsim::address victim_addr,
                              access_type type);

  void update_replacement_state(uint32_t triggering_cpu, long set, long way,
                                champsim::address full_addr,
                                champsim::address ip,
                                champsim::address victim_addr,
                                access_type type, bool hit);

private:
  struct PcRecord {
    uint64_t timestamp = 0;
    uint64_t pc = 0;
  };

  struct PcHistorySet {
    explicit PcHistorySet(std::size_t history_length);

    std::vector<uint64_t> slot_address;
    std::vector<uint64_t> slot_timestamp;
    std::vector<unsigned char> slot_valid;
    std::unordered_map<uint64_t, PcRecord> last;
    uint64_t time = 0;
  };

  std::optional<uint64_t> previous_pc_and_record(std::size_t set_idx,
                                                  uint64_t address,
                                                  uint64_t pc);
  static Classification to_classification(bool cache_friendly);

  std::size_t num_sets_;
  std::size_t num_ways_;
  std::size_t history_length_;

  OPTgen optgen_;
  HawkeyePredictor predictor_;
  std::vector<std::vector<int>> rrpv_;
  std::vector<PcHistorySet> pc_history_;
};

#endif
