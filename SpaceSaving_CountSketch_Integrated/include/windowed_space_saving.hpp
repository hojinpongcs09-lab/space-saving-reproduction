#pragma once

#include "space_saving.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace space_saving {

// A conservative frequency interval for one element over the whole window.
// estimate() is the interval midpoint; the interval, not the midpoint, carries
// the correctness guarantee.
struct WindowCountBounds {
    std::uint32_t element = 0;
    std::uint64_t lower_bound = 0;
    std::uint64_t upper_bound = 0;

    std::uint64_t estimate() const {
        return lower_bound + (upper_bound - lower_bound) / 2;
    }
    std::uint64_t width() const { return upper_bound - lower_bound; }
};

struct NaiveWindowCandidate {
    std::uint32_t element = 0;
    std::uint64_t score = 0; // Sum of stored counts; missing block entries contribute zero.
};

struct WindowTopKResult {
    // Returned candidates are ordered by interval midpoint, then lower bound,
    // then ID. This is an approximate ranking; certification is reported below.
    std::vector<WindowCountBounds> elements;
    std::size_t candidate_count = 0;
    std::uint64_t unseen_upper_bound = 0;
    std::uint64_t min_selected_lower_bound = 0;
    std::uint64_t max_excluded_upper_bound = 0;
    bool membership_certified = false;
    bool order_certified = false; // Order among returned elements only.
};

// Block-aligned sliding-window approximation built from independent
// Space-Saving summaries. Queries are supported only after a complete window
// and only at block boundaries. The total counter budget is split equally.
class BlockWindowedSpaceSaving {
public:
    BlockWindowedSpaceSaving(std::size_t window_size,
                             std::size_t num_blocks,
                             std::size_t total_counter_budget);

    BlockWindowedSpaceSaving(const BlockWindowedSpaceSaving&) = delete;
    BlockWindowedSpaceSaving& operator=(const BlockWindowedSpaceSaving&) = delete;
    BlockWindowedSpaceSaving(BlockWindowedSpaceSaving&&) = delete;
    BlockWindowedSpaceSaving& operator=(BlockWindowedSpaceSaving&&) = delete;

    void update(std::uint32_t element);

    bool ready() const;
    std::uint64_t processed() const { return processed_; }
    std::size_t window_size() const { return window_size_; }
    std::size_t num_blocks() const { return num_blocks_; }
    std::size_t block_size() const { return block_size_; }
    std::size_t total_counter_budget() const { return total_counter_budget_; }
    std::size_t counters_per_block() const { return counters_per_block_; }

    // Naive baseline: sum only counters explicitly present in each block.
    // It deliberately treats a missing per-block entry as zero and has no
    // frequency-bound guarantee.
    std::vector<NaiveWindowCandidate> query_top_k_naive(std::size_t k) const;

    // Bound-aware merge: aggregate lower/upper frequency bounds across blocks,
    // rank by interval midpoint, and certify membership/order when intervals
    // are strictly separated.
    WindowTopKResult query_top_k(std::size_t k) const;

    // All candidates in the union of per-block summaries, ordered by midpoint.
    std::vector<WindowCountBounds> merged_bounds() const;

    // Persistent owned bytes only. Temporary query workspace and allocator
    // metadata are excluded and should be measured separately if needed.
    std::size_t allocated_bytes() const;

    // Deep structural check for tests / validation runs.
    bool validate() const;

private:
    void require_query_ready() const;

    std::size_t window_size_ = 0;
    std::size_t num_blocks_ = 0;
    std::size_t block_size_ = 0;
    std::size_t total_counter_budget_ = 0;
    std::size_t counters_per_block_ = 0;
    std::uint64_t processed_ = 0;
    std::size_t current_slot_ = 0;
    bool has_current_ = false;
    std::vector<std::unique_ptr<SpaceSaving>> blocks_;
};

} // namespace space_saving
