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
  explicit hawkeye(CACHE* cachePtr);

  long find_victim(
    uint32_t trigger,
    uint64_t instrnID,
    long set,
    const champsim::cache_block* currSet,
    champsim::address instrnPtr,
    champsim::address fullAddr,
    access_type t);

  void replacement_cache_fill(
    uint32_t trigger,
    long set,
    long way,
    champsim::address fullAddr,
    champsim::address instrnPtr,
    champsim::address victimAddr,
    access_type t);

  void update_replacement_state(
    uint32_t trigger,
    long set,
    long way,
    champsim::address fullAddr,
    champsim::address instrnPtr,
    champsim::address victimAddr,
    access_type t,
    bool hit);

  private:
  struct pcRecord
  {
    uint64_t timestamp = 0;
    uint64_t pc = 0;
  };

  struct historyEntry
  {
    uint64_t blockAddr = 0;
    uint64_t timestamp = 0;
  };

  struct pcHistorySet
  {
    std::deque<historyEntry> history;
    std::unordered_map<uint64_t, pcRecord> last;
    uint64_t time = 0;
  };

  std::optional<uint64_t> prevPcAndRecord(std::size_t setIdx, uint64_t address, uint64_t pc);

  static Classification toClassification(bool cacheFriendly);

  std::size_t numSets;
  std::size_t numWays;
  std::size_t historyLength;

  OPTgen optgen;
  HawkeyePredictor predictor;
  
  std::vector<std::vector<int>> rrpvTable;
  std::vector<pcHistorySet> pcHistory;
};

#endif