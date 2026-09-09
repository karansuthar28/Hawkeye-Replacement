#include "optgen.h"

#include <stdexcept>

OPTgen::SetState::SetState(std::size_t history_length)
    : occupancy(history_length, 0),
      slot_address(history_length, 0),
      slot_timestamp(history_length, 0),
      slot_valid(history_length, 0)
{
}

OPTgen::OPTgen(std::size_t num_sets, std::size_t associativity,
               std::size_t history_multiplier)
    : associativity_(associativity),
      history_length_(associativity * history_multiplier)
{
  if (num_sets == 0)
    throw std::invalid_argument("OPTgen: num_sets must be positive");
  if (associativity == 0)
    throw std::invalid_argument("OPTgen: associativity must be positive");
  if (history_multiplier == 0)
    throw std::invalid_argument("OPTgen: history_multiplier must be positive");

  sets_.reserve(num_sets);
  for (std::size_t i = 0; i < num_sets; ++i)
    sets_.emplace_back(history_length_);
}

bool OPTgen::access(std::size_t set_idx, uint64_t address)
{
  if (set_idx >= sets_.size())
    throw std::out_of_range("OPTgen: set_idx out of range");

  SetState& state = sets_[set_idx];
  const uint64_t current_time = state.time;
  const std::size_t current_slot =
      static_cast<std::size_t>(current_time % history_length_);

  // Look up the previous reference before reusing the circular slot. This
  // intentionally allows a reuse interval of exactly history_length_ entries:
  // at that instant the oldest slot is still available for the final decision.
  bool opt_hit = false;
  const auto previous_it = state.last_access.find(address);

  if (previous_it != state.last_access.end()) {
    const uint64_t previous_time = previous_it->second;

    // A reuse is an OPT hit iff every point in the liveness interval has
    // spare capacity. The interval is [previous_time, current_time).
    bool has_capacity = true;
    for (uint64_t t = previous_time; t < current_time; ++t) {
      const std::size_t slot = static_cast<std::size_t>(t % history_length_);
      if (state.occupancy[slot] >= associativity_) {
        has_capacity = false;
        break;
      }
    }

    if (has_capacity) {
      opt_hit = true;
      for (uint64_t t = previous_time; t < current_time; ++t) {
        const std::size_t slot = static_cast<std::size_t>(t % history_length_);
        ++state.occupancy[slot];
      }
    }
  }

  // The old time represented by current_slot has now been fully consumed and
  // falls out of the history window. Remove its last-access record only if it
  // still names that exact old timestamp, then reuse the slot for time t.
  if (state.slot_valid[current_slot]) {
    const uint64_t old_address = state.slot_address[current_slot];
    const uint64_t old_timestamp = state.slot_timestamp[current_slot];
    auto it = state.last_access.find(old_address);
    if (it != state.last_access.end() && it->second == old_timestamp)
      state.last_access.erase(it);
  }
  state.occupancy[current_slot] = 0;

  state.slot_address[current_slot] = address;
  state.slot_timestamp[current_slot] = current_time;
  state.slot_valid[current_slot] = 1;
  state.last_access[address] = current_time;
  ++state.time;

  return opt_hit;
}
