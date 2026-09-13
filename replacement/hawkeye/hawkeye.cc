#include "hawkeye.h"

#include <stdexcept>


// ============================================================
// PcHistorySet constructor
// ============================================================

hawkeye::PcHistorySet::PcHistorySet(std::size_t history_length)
    : slot_address(history_length, 0),
      slot_timestamp(history_length, 0),
      slot_valid(history_length, 0)
{
}


// ============================================================
// Hawkeye constructor
// ============================================================

hawkeye::hawkeye(CACHE* cache)
    : replacement(cache),
      num_sets_(static_cast<std::size_t>(cache->NUM_SET)),
      num_ways_(static_cast<std::size_t>(cache->NUM_WAY)),
      history_length_(8 * num_ways_),

      // OPTgen:
      // one occupancy vector per set
      // W = number of ways
      optgen_(num_sets_, num_ways_),

      predictor_(),

      // Initially every line has maximum RRPV.
      rrpv_(
          num_sets_,
          std::vector<int>(num_ways_, 7))
{
  // Create one PC-history structure per cache set.
  pc_history_.reserve(num_sets_);

  for (std::size_t set = 0; set < num_sets_; ++set)
    pc_history_.emplace_back(history_length_);
}


// ============================================================
// Convert predictor result into RRIP classification
// ============================================================

Classification hawkeye::to_classification(bool cache_friendly)
{
  return cache_friendly
             ? Classification::CACHE_FRIENDLY
             : Classification::CACHE_AVERSE;
}


// ============================================================
// Get previous PC of this cache line and record current access
// ============================================================

std::optional<uint64_t>
hawkeye::previous_pc_and_record(
    std::size_t set_idx,
    uint64_t address,
    uint64_t pc)
{
  PcHistorySet& state = pc_history_.at(set_idx);

  const uint64_t current_time = state.time;

  // Circular history slot.
  const std::size_t current_slot =
      static_cast<std::size_t>(
          current_time % history_length_);


  // ----------------------------------------------------------
  // Find PC responsible for PREVIOUS access to this line.
  // ----------------------------------------------------------

  std::optional<uint64_t> previous_pc;

  const auto it = state.last.find(address);

  if (it != state.last.end())
    previous_pc = it->second.pc;


  // ----------------------------------------------------------
  // Expire oldest entry from circular history.
  // ----------------------------------------------------------

  if (state.slot_valid[current_slot]) {

    const uint64_t old_address =
        state.slot_address[current_slot];

    const uint64_t old_timestamp =
        state.slot_timestamp[current_slot];

    const auto old_it =
        state.last.find(old_address);

    // Remove only if this slot still corresponds to the
    // latest recorded reference of old_address.
    if (old_it != state.last.end() &&
        old_it->second.timestamp == old_timestamp)
    {
      state.last.erase(old_it);
    }
  }


  // ----------------------------------------------------------
  // Record current access.
  // ----------------------------------------------------------

  state.slot_address[current_slot] = address;
  state.slot_timestamp[current_slot] = current_time;
  state.slot_valid[current_slot] = 1;

  state.last[address] =
      PcRecord{current_time, pc};

  ++state.time;

  return previous_pc;
}


// ============================================================
// Victim selection
// ============================================================

long hawkeye::find_victim(
    uint32_t /*triggering_cpu*/,
    uint64_t /*instr_id*/,
    long set,
    const champsim::cache_block* /*current_set*/,
    champsim::address /*ip*/,
    champsim::address /*full_addr*/,
    access_type /*type*/)
{
  if (set < 0 ||
      static_cast<std::size_t>(set) >= num_sets_)
  {
    throw std::out_of_range(
        "hawkeye::find_victim: set out of range");
  }

  const std::size_t set_idx =
      static_cast<std::size_t>(set);

  return static_cast<long>(
      ::find_victim(rrpv_.at(set_idx)));
}


// ============================================================
// Cache insertion after a miss
// ============================================================

void hawkeye::replacement_cache_fill(
    uint32_t /*triggering_cpu*/,
    long set,
    long way,
    champsim::address /*full_addr*/,
    champsim::address ip,
    champsim::address /*victim_addr*/,
    access_type /*type*/)
{
  if (set < 0 ||
      static_cast<std::size_t>(set) >= num_sets_)
  {
    throw std::out_of_range(
        "hawkeye::replacement_cache_fill: set out of range");
  }

  if (way < 0 ||
      static_cast<std::size_t>(way) >= num_ways_)
  {
    throw std::out_of_range(
        "hawkeye::replacement_cache_fill: way out of range");
  }


  const std::size_t set_idx =
      static_cast<std::size_t>(set);

  const std::size_t way_idx =
      static_cast<std::size_t>(way);


  // ----------------------------------------------------------
  // IMPORTANT FIX:
  //
  // Previously:
  //
  // WRITE -> cache_friendly = false
  //
  // which forced every WRITE fill to CACHE_AVERSE / RRPV 7.
  //
  // Now ALL accesses use predictor classification.
  // ----------------------------------------------------------

  const uint64_t pc = ip.to<uint64_t>();

  const bool cache_friendly =
      predictor_.predict(pc);


  // This is an INSERTION after a cache miss.
  update_rrpv(
      rrpv_.at(set_idx),
      way_idx,
      to_classification(cache_friendly),
      /*is_hit=*/false);
}


// ============================================================
// Update Hawkeye after every cache access
// ============================================================

void hawkeye::update_replacement_state(
    uint32_t /*triggering_cpu*/,
    long set,
    long way,
    champsim::address full_addr,
    champsim::address ip,
    champsim::address /*victim_addr*/,
    access_type /*type*/,
    bool hit)
{
  if (set < 0 ||
      static_cast<std::size_t>(set) >= num_sets_)
  {
    throw std::out_of_range(
        "hawkeye::update_replacement_state: set out of range");
  }


  const std::size_t set_idx =
      static_cast<std::size_t>(set);


  // ==========================================================
  // IMPORTANT FIX #1:
  //
  // Use CACHE-LINE address, not byte address.
  //
  // Assignment uses 64-byte cache lines.
  //
  // Therefore:
  //
  //     line_addr = full_addr / 64
  //               = full_addr >> 6
  //
  // Example:
  //
  // 0x1000
  // 0x1008
  // 0x1010
  //
  // are all accesses to the SAME cache line.
  // ==========================================================

  const uint64_t line_addr =
      full_addr.to<uint64_t>() >> 6;


  const uint64_t pc =
      ip.to<uint64_t>();


  // ==========================================================
  // IMPORTANT FIX #2:
  //
  // Previously:
  //
  // if (type == access_type::WRITE)
  //     return;
  //
  // Therefore WRITE accesses never reached:
  //
  //   OPTgen
  //   predictor training
  //   RRPV update
  //
  // That special-case has now been removed.
  // ==========================================================


  // ----------------------------------------------------------
  // Find the PC responsible for previous reference to this
  // cache line.
  // ----------------------------------------------------------

  const std::optional<uint64_t> previous_pc =
      previous_pc_and_record(
          set_idx,
          line_addr,
          pc);


  // ----------------------------------------------------------
  // Ask OPTgen whether PREVIOUS reference would have produced
  // an OPT hit.
  // ----------------------------------------------------------

  const bool opt_hit =
      optgen_.access(
          set_idx,
          line_addr);


  // ----------------------------------------------------------
  // Train PC responsible for PREVIOUS access.
  //
  // Hawkeye paper:
  //
  // Current reuse tells us whether the previous reference
  // should have been considered cache-friendly.
  // ----------------------------------------------------------

  if (previous_pc.has_value())
  {
    predictor_.train(
        *previous_pc,
        opt_hit);
  }


  // ----------------------------------------------------------
  // If this was a REAL cache hit, update the RRPV immediately.
  //
  // On a miss, insertion is handled later in
  // replacement_cache_fill().
  // ----------------------------------------------------------

  if (hit)
  {
    if (way < 0 ||
        static_cast<std::size_t>(way) >= num_ways_)
    {
      throw std::out_of_range(
          "hawkeye::update_replacement_state: "
          "way out of range");
    }


    const std::size_t way_idx =
        static_cast<std::size_t>(way);


    // Predictor classification for CURRENT PC.
    const bool cache_friendly =
        predictor_.predict(pc);


    update_rrpv(
        rrpv_.at(set_idx),
        way_idx,
        to_classification(cache_friendly),
        /*is_hit=*/true);
  }
}