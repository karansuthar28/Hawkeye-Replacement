#ifndef REPLACEMENT_HAWKEYE_HAWKEYE_H
#define REPLACEMENT_HAWKEYE_HAWKEYE_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <unordered_map>
#include <vector>

#include "cache.h"
#include "modules.h"
#include "optgen.h"
#include "predictor.h"
#include "rrip.h"

struct hawkeye : public champsim::modules::replacement
{
  explicit hawkeye(CACHE* cache);

  long find_victim(
    uint32_t triggeringCpu,
    uint64_t instrId,
    long set,
    const champsim::cache_block* currentSet,
    champsim::address ip,
    champsim::address fullAddr,
    access_type type);

  void replacement_cache_fill(
    uint32_t triggeringCpu,
    long set,
    long way,
    champsim::address fullAddr,
    champsim::address ip,
    champsim::address victimAddr,
    access_type type);

  void update_replacement_state(
    uint32_t triggeringCpu,
    long set,
    long way,
    champsim::address fullAddr,
    champsim::address ip,
    champsim::address victimAddr,
    access_type type,
    bool hit);

  private:
  struct PcRecord
  {
    uint64_t timestamp = 0;
    uint64_t pc = 0;
  };

  struct HistoryEntry
  {
    uint64_t address = 0;
    uint64_t timestamp = 0;
  };

  struct PcHistorySet
  {
    std::deque<HistoryEntry> history;
    std::unordered_map<uint64_t, PcRecord> last;
    uint64_t time = 0;
  };

  std::optional<uint64_t> previousPcAndRecord(
    std::size_t setIdx,
    uint64_t address,
    uint64_t pc);

  static Classification toClassification(bool cacheFriendly);

  std::size_t numSets;
  std::size_t numWays;
  std::size_t historyLength;

  OPTgen optgen;
  HawkeyePredictor predictor;
  
  std::vector<std::vector<int>> rrpvTable;
  std::vector<PcHistorySet> pcHistory;
};

#endif