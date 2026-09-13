#include "optgen.h"

#include <stdexcept>

// Exceptional Handling I am doing (Edge Cases)
OPTgen::OPTgen(std::size_t num_sets, std::size_t associativity, std::size_t history_multiplier): setAssociativity(associativity), historyLength(associativity * history_multiplier), totalSets(num_sets)
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
    uint64_t currTime = cacheSet.time; // currTime = currentTime

    bool optHit = 0;

    uint64_t startQueue = currTime - cacheSet.occVector.size();
    auto prev = cacheSet.prevAccess.find(address);

    if (prev != cacheSet.prevAccess.end())
    {
        uint64_t prevTime = prev->second;

        // Convert the absolute timestamp into an index in our deque.
        std::size_t startIndex = static_cast<std::size_t>(prevTime - startQueue);

        bool flag = 0; // I am assuming that occVector[i] < associativity => flag = 0

        // Check the usage interval [prevTime, currTime).
        for (std::size_t i = startIndex; i < cacheSet.occVector.size(); ++i)
        {
            if (cacheSet.occVector[i] >= setAssociativity)
            {
                flag = 1;
                break;
            }
        }

        // If every point had free capacity, OPT would have kept the line.
        if (flag == 0)
        {
            optHit = 1;

            for (std::size_t i = startIndex; i < cacheSet.occVector.size(); ++i)
                ++cacheSet.occVector[i];
        }
    }

    // If the 8W history is full, remove its oldest access.
    if (cacheSet.occVector.size() == historyLength)
    {
        uint64_t oldAddress = cacheSet.accessHistory.front();
        uint64_t oldTime = startQueue;
        auto oldEntry = cacheSet.prevAccess.find(oldAddress);

        // Remove it only if this really is the most recent record for that address.
        if (oldEntry != cacheSet.prevAccess.end() && oldEntry->second == oldTime)
        {
            cacheSet.prevAccess.erase(oldEntry);
        }

        cacheSet.occVector.pop_front();
        cacheSet.accessHistory.pop_front();
    }

    // Add the current access to the end of the history.
    cacheSet.occVector.push_back(0);
    cacheSet.accessHistory.push_back(address);

    cacheSet.prevAccess[address] = currTime;

    ++cacheSet.time;

    return optHit;
}