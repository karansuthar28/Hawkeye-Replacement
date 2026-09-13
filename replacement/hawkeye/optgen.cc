#include "optgen.h"

#include <stdexcept>

OPTgen::OPTgen(std::size_t num_sets,
               std::size_t associativity,
               std::size_t history_multiplier)
    : associativity(associativity),
      historyLength(associativity * history_multiplier),
      totalSets(num_sets)
{
    if (num_sets == 0)
        throw std::invalid_argument("OPTgen: num_sets must be positive");

    if (associativity == 0)
        throw std::invalid_argument("OPTgen: associativity must be positive");

    if (history_multiplier == 0)
        throw std::invalid_argument("OPTgen: history_multiplier must be positive");
}


bool OPTgen::access(std::size_t set_idx, uint64_t address)
{
    if (set_idx >= totalSets.size())
        throw std::out_of_range("OPTgen: set_idx out of range");

    setInfo& cacheSet = totalSets[set_idx];

    uint64_t currentTime = cacheSet.time;

    bool optHit = false;


    // Oldest timestamp currently represented by occVector.
    uint64_t windowStart =
        currentTime - static_cast<uint64_t>(cacheSet.occVector.size());


    auto previous = cacheSet.prevAccess.find(address);

    if (previous != cacheSet.prevAccess.end())
    {
        uint64_t previousTime = previous->second;

        // Convert the absolute timestamp into an index in our deque.
        std::size_t startIndex =
            static_cast<std::size_t>(previousTime - windowStart);

        bool hasCapacity = 1;


        // Check the usage interval [previousTime, currentTime).
        for (std::size_t i = startIndex;
             i < cacheSet.occVector.size();
             ++i)
        {
            if (cacheSet.occVector[i] >= associativity)
            {
                hasCapacity = 0;
                break;
            }
        }


        // If every point had free capacity, OPT would have kept the line.
        if (hasCapacity == 1)
        {
            optHit = 1;

            for (std::size_t i = startIndex;
                 i < cacheSet.occVector.size();
                 ++i)
            {
                ++cacheSet.occVector[i];
            }
        }
    }


    // If the 8W history is full, remove its oldest access.
    if (cacheSet.occVector.size() == historyLength)
    {
        uint64_t oldAddress = cacheSet.accessHistory.front();
        uint64_t oldTime = windowStart;

        auto oldEntry = cacheSet.lastAccess.find(oldAddress);

        // Remove it only if this really is the most recent record
        // for that address.
        if (oldEntry != cacheSet.lastAccess.end() &&
            oldEntry->second == oldTime)
        {
            cacheSet.lastAccess.erase(oldEntry);
        }

        cacheSet.occVector.pop_front();
        cacheSet.accessHistory.pop_front();
    }


    // Add the current access to the end of the history.
    cacheSet.occVector.push_back(0);
    cacheSet.accessHistory.push_back(address);

    cacheSet.lastAccess[address] = currentTime;

    ++cacheSet.time;

    return optHit;
}