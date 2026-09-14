#include "hawkeye.h"

#include <stdexcept>

// Hawkeye Constructor
hawkeye::hawkeye(CACHE* cachePtr): replacement(cachePtr),
    numSets(static_cast<std::size_t>(cachePtr -> NUM_SET)),
    numWays(static_cast<std::size_t>(cachePtr -> NUM_WAY)),
    historyLength(8 * numWays),
    optgen(numSets, numWays),
    predictor(),
    rrpvTable(numSets, std::vector<int>(numWays, 7)),
    pcHistory(numSets){};

Classification hawkeye::toClassification(bool cacheFriendly) {
    return cacheFriendly ? Classification::CACHE_FRIENDLY : Classification::CACHE_AVERSE;
}

std::optional<uint64_t> hawkeye::prevPcAndRecord(std::size_t setIdx, uint64_t address, uint64_t pc) 
{
    pcHistorySet& historyState = pcHistory[setIdx];
    const uint64_t currentTime = historyState.time;

    std::optional<uint64_t> prevPc;

    const auto previous = historyState.last.find(address);

    if (previous != historyState.last.end())
        prevPc = previous->second.pc;
    
    if (historyState.history.size() == historyLength)
    {
        const historyEntry oldest = historyState.history.front();
        historyState.history.pop_front();

        const auto oldRecord = historyState.last.find(oldest.blockAddr);

        if (oldRecord != historyState.last.end() && oldRecord->second.timestamp == oldest.timestamp)
            historyState.last.erase(oldRecord);
    }

    historyState.history.push_back(historyEntry{address, currentTime});
    historyState.last[address] = pcRecord{currentTime, pc};

    ++historyState.time;
    return prevPc;
}

long hawkeye::find_victim(
    uint32_t /*triggeringCpu*/,
    uint64_t /*instrId*/,
    long set,
    const champsim::cache_block* /*currentSet*/,
    champsim::address /*ip*/,
    champsim::address /*fullAddr*/,
    access_type /*type*/)
{
    if (set < 0 || static_cast<std::size_t>(set) >= numSets)
        throw std::out_of_range("hawkeye::find_victim: set out of range");

    const std::size_t setIdx = static_cast<std::size_t>(set);

    return static_cast<long>(::find_victim(rrpvTable[setIdx]));
}

void hawkeye::replacement_cache_fill(
    uint32_t /*triggeringCpu*/,
    long set,
    long way,
    champsim::address /*fullAddr*/,
    champsim::address ip,
    champsim::address /*victimAddr*/,
    access_type /*type*/)
{
    if (set < 0 || static_cast<std::size_t>(set) >= numSets)
        throw std::out_of_range("hawkeye::replacement_cache_fill: set out of range");

    if (way < 0 || static_cast<std::size_t>(way) >= numWays)
        throw std::out_of_range("hawkeye::replacement_cache_fill: way out of range");

    const std::size_t setIdx = static_cast<std::size_t>(set);
    const std::size_t wayIdx = static_cast<std::size_t>(way);

    const uint64_t pc = ip.to<uint64_t>();
    const bool cacheFriendly = predictor.predict(pc);

    update_rrpv(rrpvTable.at(setIdx), wayIdx, toClassification(cacheFriendly), false);
}

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
    if (set < 0 || static_cast<std::size_t>(set) >= numSets)
        throw std::out_of_range("hawkeye::update_replacement_state: set out of range");

    const std::size_t setIdx = static_cast<std::size_t>(set);
    const uint64_t lineAddr = fullAddr.to<uint64_t>() >> 6;
    const uint64_t pc = ip.to<uint64_t>();
    const std::optional<uint64_t> prevPc = prevPcAndRecord(setIdx, lineAddr, pc);
    const bool optHit = optgen.access(setIdx, lineAddr);

    // Training
    if (prevPc.has_value())
        predictor.train(*prevPc, optHit);

    if (!hit)
        return;

    if (way < 0 || static_cast<std::size_t>(way) >= numWays)
        throw std::out_of_range("hawkeye::update_replacement_state: way out of range");

    const std::size_t wayIdx = static_cast<std::size_t>(way);
    const bool cacheFriendly = predictor.predict(pc);

    update_rrpv(rrpvTable.at(setIdx), wayIdx, toClassification(cacheFriendly), true);
}