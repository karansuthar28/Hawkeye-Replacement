#ifndef REPLACEMENT_HAWKEYE_OPTGEN_H
#define REPLACEMENT_HAWKEYE_OPTGEN_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <unordered_map>
#include <vector>

class OPTgen
{
public:
    OPTgen(std::size_t num_sets,
           std::size_t associativity,
           std::size_t history_multiplier = 8);

    bool access(std::size_t set_idx, uint64_t address);

private:
    struct setInfo
    {
        std::deque<std::size_t> occVector; // Occupancy values for the accesses currently inside the 8W history.
        std::deque<uint64_t> accessHistory; // Address corresponding to each entry in occVector.
        std::unordered_map<uint64_t, uint64_t> prevAccess; // address -> most recent set-local access time
        uint64_t time = 0; // Number of accesses seen by this set.
    };

    std::size_t associativity; // Let's say 16-way then W = 16
    std::size_t historyLength; // 8W = 8x16 = 128
    std::vector<setInfo> totalSets; // Number of cache lines/ways
};

#endif