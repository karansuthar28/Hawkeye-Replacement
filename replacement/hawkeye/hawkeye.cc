#include "hawkeye.h"

#include <stdexcept>


// ============================================================
// Hawkeye constructor
// ============================================================

hawkeye::hawkeye(CACHE* cache)
    : replacement(cache),
      numSets(static_cast<std::size_t>(cache->NUM_SET)),
      numWays(static_cast<std::size_t>(cache->NUM_WAY)),
      historyLength(8 * numWays),
      optgen(numSets, numWays),
      predictor(),
      rrpvTable(
          numSets,
          std::vector<int>(numWays, 7)),
      pcHistory(numSets)
{
}


// ============================================================
// Convert predictor result into RRIP classification
// ============================================================

Classification hawkeye::toClassification(
    bool cacheFriendly)
{
  return cacheFriendly
             ? Classification::CACHE_FRIENDLY
             : Classification::CACHE_AVERSE;
}


// ============================================================
// Get previous PC and record current access
// ============================================================

std::optional<uint64_t>
hawkeye::previousPcAndRecord(
    std::size_t setIdx,
    uint64_t address,
    uint64_t pc)
{
  PcHistorySet& state =
      pcHistory.at(setIdx);

  const uint64_t currentTime =
      state.time;


  // ----------------------------------------------------------
  // Find the PC of the previous access to this address.
  // ----------------------------------------------------------

  std::optional<uint64_t> previousPc;

  const auto previous =
      state.last.find(address);

  if (previous != state.last.end())
  {
    previousPc =
        previous->second.pc;
  }


  // ----------------------------------------------------------
  // Keep only the most recent 8W accesses.
  // If deque is full, remove the oldest entry.
  // ----------------------------------------------------------

  if (state.history.size() == historyLength)
  {
    const HistoryEntry oldest =
        state.history.front();

    state.history.pop_front();


    const auto oldRecord =
        state.last.find(oldest.address);


    // Delete from map only if this oldest entry is still
    // the latest occurrence of that address.
    if (oldRecord != state.last.end() &&
        oldRecord->second.timestamp == oldest.timestamp)
    {
      state.last.erase(oldRecord);
    }
  }


  // ----------------------------------------------------------
  // Add current access to the end of the deque.
  // ----------------------------------------------------------

  state.history.push_back(
      HistoryEntry{
          address,
          currentTime
      });


  // Current access now becomes the latest access
  // for this address.
  state.last[address] =
      PcRecord{
          currentTime,
          pc
      };


  ++state.time;

  return previousPc;
}


// ============================================================
// Victim selection
// ============================================================

long hawkeye::find_victim(
    uint32_t /*triggeringCpu*/,
    uint64_t /*instrId*/,
    long set,
    const champsim::cache_block* /*currentSet*/,
    champsim::address /*ip*/,
    champsim::address /*fullAddr*/,
    access_type /*type*/)
{
  if (set < 0 ||
      static_cast<std::size_t>(set) >= numSets)
  {
    throw std::out_of_range(
        "hawkeye::find_victim: set out of range");
  }


  const std::size_t setIdx =
      static_cast<std::size_t>(set);


  return static_cast<long>(
      ::find_victim(
          rrpvTable.at(setIdx)));
}


// ============================================================
// Cache insertion after miss
// ============================================================

void hawkeye::replacement_cache_fill(
    uint32_t /*triggeringCpu*/,
    long set,
    long way,
    champsim::address /*fullAddr*/,
    champsim::address ip,
    champsim::address /*victimAddr*/,
    access_type /*type*/)
{
  if (set < 0 ||
      static_cast<std::size_t>(set) >= numSets)
  {
    throw std::out_of_range(
        "hawkeye::replacement_cache_fill: "
        "set out of range");
  }


  if (way < 0 ||
      static_cast<std::size_t>(way) >= numWays)
  {
    throw std::out_of_range(
        "hawkeye::replacement_cache_fill: "
        "way out of range");
  }


  const std::size_t setIdx =
      static_cast<std::size_t>(set);

  const std::size_t wayIdx =
      static_cast<std::size_t>(way);


  const uint64_t pc =
      ip.to<uint64_t>();


  const bool cacheFriendly =
      predictor.predict(pc);


  update_rrpv(
      rrpvTable.at(setIdx),
      wayIdx,
      toClassification(cacheFriendly),
      false);
}


// ============================================================
// Update Hawkeye after every cache access
// ============================================================

void hawkeye::update_replacement_state(
    uint32_t /*triggeringCpu*/,
    long set,
    long way,
    champsim::address fullAddr,
    champsim::address ip,
    champsim::address /*victimAddr*/,
    access_type /*type*/,
    bool hit)
{
  if (set < 0 ||
      static_cast<std::size_t>(set) >= numSets)
  {
    throw std::out_of_range(
        "hawkeye::update_replacement_state: "
        "set out of range");
  }


  const std::size_t setIdx =
      static_cast<std::size_t>(set);


  // Convert byte address to cache-line address.
  const uint64_t lineAddr =
      fullAddr.to<uint64_t>() >> 6;


  // Current instruction PC.
  const uint64_t pc =
      ip.to<uint64_t>();


  // Find previous PC and store current access.
  const std::optional<uint64_t> previousPc =
      previousPcAndRecord(
          setIdx,
          lineAddr,
          pc);


  // Ask OPTgen about the previous reference.
  const bool optHit =
      optgen.access(
          setIdx,
          lineAddr);


  // Train the PC responsible for previous access.
  if (previousPc.has_value())
  {
    predictor.train(
        *previousPc,
        optHit);
  }


  // On a miss, insertion RRPV will be handled later
  // by replacement_cache_fill().
  if (!hit)
  {
    return;
  }


  if (way < 0 ||
      static_cast<std::size_t>(way) >= numWays)
  {
    throw std::out_of_range(
        "hawkeye::update_replacement_state: "
        "way out of range");
  }


  const std::size_t wayIdx =
      static_cast<std::size_t>(way);


  const bool cacheFriendly =
      predictor.predict(pc);


  update_rrpv(
      rrpvTable.at(setIdx),
      wayIdx,
      toClassification(cacheFriendly),
      true);
}