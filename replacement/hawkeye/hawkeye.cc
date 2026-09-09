#include "hawkeye.h"

#include <stdexcept>

hawkeye::PcHistorySet::PcHistorySet(std::size_t history_length)
    : slot_address(history_length, 0),
      slot_timestamp(history_length, 0),
      slot_valid(history_length, 0)
{
}

hawkeye::hawkeye(CACHE* cache)
    : replacement(cache),
      num_sets_(static_cast<std::size_t>(cache->NUM_SET)),
      num_ways_(static_cast<std::size_t>(cache->NUM_WAY)),
      history_length_(8 * num_ways_),
      optgen_(num_sets_, num_ways_),
      predictor_(),
      rrpv_(num_sets_, std::vector<int>(num_ways_, 7))
{
  pc_history_.reserve(num_sets_);
  for (std::size_t set = 0; set < num_sets_; ++set)
    pc_history_.emplace_back(history_length_);
}

Classification hawkeye::to_classification(bool cache_friendly)
{
  return cache_friendly ? Classification::CACHE_FRIENDLY
                        : Classification::CACHE_AVERSE;
}

std::optional<uint64_t>
hawkeye::previous_pc_and_record(std::size_t set_idx, uint64_t address,
                                uint64_t pc)
{
  PcHistorySet& state = pc_history_.at(set_idx);
  const uint64_t current_time = state.time;
  const std::size_t current_slot =
      static_cast<std::size_t>(current_time % history_length_);

  // Read the previous PC before expiring the circular slot so that an access
  // exactly history_length_ references old receives the same label as OPTgen.
  std::optional<uint64_t> previous_pc;
  const auto it = state.last.find(address);
  if (it != state.last.end())
    previous_pc = it->second.pc;

  if (state.slot_valid[current_slot]) {
    const uint64_t old_address = state.slot_address[current_slot];
    const uint64_t old_timestamp = state.slot_timestamp[current_slot];
    const auto old_it = state.last.find(old_address);
    if (old_it != state.last.end() && old_it->second.timestamp == old_timestamp)
      state.last.erase(old_it);
  }

  state.slot_address[current_slot] = address;
  state.slot_timestamp[current_slot] = current_time;
  state.slot_valid[current_slot] = 1;
  state.last[address] = PcRecord{current_time, pc};
  ++state.time;

  return previous_pc;
}

long hawkeye::find_victim(uint32_t /*triggering_cpu*/, uint64_t /*instr_id*/,
                          long set,
                          const champsim::cache_block* /*current_set*/,
                          champsim::address /*ip*/,
                          champsim::address /*full_addr*/,
                          access_type /*type*/)
{
  if (set < 0 || static_cast<std::size_t>(set) >= num_sets_)
    throw std::out_of_range("hawkeye::find_victim: set out of range");

  return static_cast<long>(::find_victim(rrpv_.at(static_cast<std::size_t>(set))));
}

void hawkeye::replacement_cache_fill(uint32_t /*triggering_cpu*/, long set,
                                     long way, champsim::address /*full_addr*/,
                                     champsim::address ip,
                                     champsim::address /*victim_addr*/,
                                     access_type type)
{
  if (set < 0 || way < 0)
    throw std::out_of_range("hawkeye::replacement_cache_fill: negative set/way");

  const std::size_t set_idx = static_cast<std::size_t>(set);
  const std::size_t way_idx = static_cast<std::size_t>(way);

  // Writeback fills are not demand evidence for the PC predictor and should
  // not displace useful demand data aggressively.
  const bool cache_friendly =
      (type == access_type::WRITE) ? false : predictor_.predict(ip.to<uint64_t>());

  update_rrpv(rrpv_.at(set_idx), way_idx,
              to_classification(cache_friendly), /*is_hit=*/false);
}

void hawkeye::update_replacement_state(
    uint32_t /*triggering_cpu*/, long set, long way,
    champsim::address full_addr, champsim::address ip,
    champsim::address /*victim_addr*/, access_type type, bool hit)
{
  if (set < 0 || static_cast<std::size_t>(set) >= num_sets_)
    throw std::out_of_range("hawkeye::update_replacement_state: set out of range");

  // As in ChampSim's LRU policy, writeback accesses are not treated as demand
  // reuse. A writeback fill itself is handled in replacement_cache_fill().
  if (type == access_type::WRITE)
    return;

  const std::size_t set_idx = static_cast<std::size_t>(set);
  const uint64_t address = full_addr.to<uint64_t>();
  const uint64_t pc = ip.to<uint64_t>();

  // OPTgen labels the *previous* reference when the next reference arrives,
  // so train the PC that made that previous reference.
  const std::optional<uint64_t> previous_pc =
      previous_pc_and_record(set_idx, address, pc);
  const bool opt_hit = optgen_.access(set_idx, address);
  if (previous_pc.has_value())
    predictor_.train(*previous_pc, opt_hit);

  // On a real cache hit, update this resident line immediately. On a miss,
  // ChampSim passes way == NUM_WAY at tag-check time; insertion is therefore
  // handled later by replacement_cache_fill().
  if (hit) {
    if (way < 0 || static_cast<std::size_t>(way) >= num_ways_)
      throw std::out_of_range("hawkeye::update_replacement_state: way out of range");

    const bool cache_friendly = predictor_.predict(pc);
    update_rrpv(rrpv_.at(set_idx), static_cast<std::size_t>(way),
                to_classification(cache_friendly), /*is_hit=*/true);
  }
}
