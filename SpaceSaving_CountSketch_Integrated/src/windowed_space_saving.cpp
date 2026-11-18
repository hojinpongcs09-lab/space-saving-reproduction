#include "windowed_space_saving.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace space_saving {

BlockWindowedSpaceSaving::BlockWindowedSpaceSaving(std::size_t window_size,
                                                   std::size_t num_blocks,
                                                   std::size_t total_counter_budget)
    : window_size_(window_size),
      num_blocks_(num_blocks),
      total_counter_budget_(total_counter_budget) {
    if (window_size_ == 0 || num_blocks_ == 0 || total_counter_budget_ == 0)
        throw std::invalid_argument("window size, block count, and counter budget must be positive");
    if (window_size_ % num_blocks_ != 0)
        throw std::invalid_argument("window size must be divisible by block count");
    if (total_counter_budget_ % num_blocks_ != 0)
        throw std::invalid_argument("total counter budget must be divisible by block count");
    block_size_ = window_size_ / num_blocks_;
    counters_per_block_ = total_counter_budget_ / num_blocks_;
    if (block_size_ == 0 || counters_per_block_ == 0)
        throw std::invalid_argument("each block must have a positive size and counter capacity");
    blocks_.resize(num_blocks_);
}

void BlockWindowedSpaceSaving::update(std::uint32_t element) {
    if (processed_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("window stream length overflow");

    if (!has_current_) {
        current_slot_ = 0;
        blocks_[current_slot_] = std::make_unique<SpaceSaving>(counters_per_block_);
        has_current_ = true;
    } else if (blocks_[current_slot_]->processed() == block_size_) {
        // Start the next block. Reusing this ring slot expires the oldest
        // summary. Until the next block completes, queries are intentionally
        // unavailable: the algorithm promises exact W-packet alignment only
        // at completed-block boundaries.
        current_slot_ = (current_slot_ + 1) % num_blocks_;
        blocks_[current_slot_] = std::make_unique<SpaceSaving>(counters_per_block_);
    }

    blocks_[current_slot_]->update(element);
    ++processed_;
}

bool BlockWindowedSpaceSaving::ready() const {
    if (!has_current_ || processed_ < window_size_ || processed_ % block_size_ != 0)
        return false;
    for (const auto& block : blocks_) {
        if (!block || block->processed() != block_size_)
            return false;
    }
    return true;
}

void BlockWindowedSpaceSaving::require_query_ready() const {
    if (!ready())
        throw std::logic_error("query requires a full window at a block boundary");
}

std::vector<NaiveWindowCandidate>
BlockWindowedSpaceSaving::query_top_k_naive(std::size_t k) const {
    if (k == 0) throw std::invalid_argument("k must be positive");
    require_query_ready();

    std::unordered_map<std::uint32_t, std::uint64_t> scores;
    scores.reserve(total_counter_budget_);
    for (const auto& block : blocks_) {
        for (const auto& item : block->monitored_elements())
            scores[item.element] += item.estimated_count;
    }

    std::vector<NaiveWindowCandidate> ranked;
    ranked.reserve(scores.size());
    for (const auto& kv : scores) ranked.push_back({kv.first, kv.second});
    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.element < b.element;
    });
    if (ranked.size() > k) ranked.resize(k);
    return ranked;
}

std::vector<WindowCountBounds> BlockWindowedSpaceSaving::merged_bounds() const {
    require_query_ready();

    std::unordered_map<std::uint32_t, WindowCountBounds> merged;
    merged.reserve(total_counter_budget_);
    std::vector<std::unordered_set<std::uint32_t>> local_ids;
    local_ids.reserve(num_blocks_);
    std::vector<std::uint64_t> missing_upper;
    missing_upper.reserve(num_blocks_);
    std::uint64_t unseen_upper = 0;

    for (const auto& block : blocks_) {
        const auto items = block->monitored_elements();
        auto& ids = local_ids.emplace_back();
        ids.reserve(items.size() * 2 + 1);

        for (const auto& item : items) {
            ids.insert(item.element);
            auto [it, inserted] = merged.emplace(
                item.element, WindowCountBounds{item.element, 0, 0});
            (void)inserted;
            it->second.lower_bound += item.lower_bound();
            it->second.upper_bound += item.estimated_count;
        }

        // If a summary is not full, every observed ID is still represented;
        // an absent ID therefore has true frequency zero in that block.
        const std::uint64_t miss =
            block->monitored_count() == block->capacity() ? block->min_count() : 0;
        missing_upper.push_back(miss);
        unseen_upper += miss;
    }

    // For a candidate absent from a full block, use that block's minimum
    // counter as a conservative upper bound. Its lower bound contribution is 0.
    for (auto& kv : merged) {
        for (std::size_t b = 0; b < num_blocks_; ++b) {
            if (local_ids[b].find(kv.first) == local_ids[b].end())
                kv.second.upper_bound += missing_upper[b];
        }
        if (kv.second.lower_bound > kv.second.upper_bound)
            throw std::logic_error("merged frequency interval is inverted");
    }

    std::vector<WindowCountBounds> result;
    result.reserve(merged.size());
    for (const auto& kv : merged) result.push_back(kv.second);
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        if (a.estimate() != b.estimate()) return a.estimate() > b.estimate();
        if (a.lower_bound != b.lower_bound) return a.lower_bound > b.lower_bound;
        if (a.upper_bound != b.upper_bound) return a.upper_bound > b.upper_bound;
        return a.element < b.element;
    });
    return result;
}

WindowTopKResult BlockWindowedSpaceSaving::query_top_k(std::size_t k) const {
    if (k == 0) throw std::invalid_argument("k must be positive");
    require_query_ready();

    WindowTopKResult result;
    auto all = merged_bounds();
    result.candidate_count = all.size();
    result.unseen_upper_bound = 0;
    for (std::size_t b = 0; b < num_blocks_; ++b) {
        const auto& block = blocks_[b];
        if (block->monitored_count() == block->capacity())
            result.unseen_upper_bound += block->min_count();
    }

    const std::size_t take = std::min(k, all.size());
    result.elements.assign(all.begin(), all.begin() + take);

    if (take == 0) {
        result.membership_certified = (result.unseen_upper_bound == 0);
        result.order_certified = true;
        return result;
    }

    result.min_selected_lower_bound = std::numeric_limits<std::uint64_t>::max();
    for (const auto& item : result.elements)
        result.min_selected_lower_bound =
            std::min(result.min_selected_lower_bound, item.lower_bound);

    for (std::size_t i = take; i < all.size(); ++i)
        result.max_excluded_upper_bound =
            std::max(result.max_excluded_upper_bound, all[i].upper_bound);

    if (all.size() < k) {
        // Fewer than k candidates are sufficient only if no unseen ID can
        // have a positive frequency in the window.
        result.membership_certified = (result.unseen_upper_bound == 0);
    } else {
        const auto outside_upper =
            std::max(result.max_excluded_upper_bound, result.unseen_upper_bound);
        // Strict separation avoids claiming a unique membership when a tie
        // at the kth boundary is still possible.
        result.membership_certified =
            result.min_selected_lower_bound > outside_upper;
    }

    // Certify the reported order within the returned set. For each position,
    // its lower bound must exceed every later returned candidate's upper bound.
    result.order_certified = true;
    std::uint64_t suffix_max_upper = 0;
    for (std::size_t i = take; i-- > 0;) {
        if (i + 1 < take && result.elements[i].lower_bound <= suffix_max_upper)
            result.order_certified = false;
        suffix_max_upper = std::max(suffix_max_upper, result.elements[i].upper_bound);
    }
    return result;
}

std::size_t BlockWindowedSpaceSaving::allocated_bytes() const {
    std::size_t bytes = sizeof(*this) + blocks_.capacity() * sizeof(blocks_[0]);
    for (const auto& block : blocks_)
        if (block) bytes += block->allocated_bytes();
    return bytes;
}

bool BlockWindowedSpaceSaving::validate() const {
    if (window_size_ == 0 || num_blocks_ == 0 || block_size_ * num_blocks_ != window_size_ ||
        counters_per_block_ * num_blocks_ != total_counter_budget_ || blocks_.size() != num_blocks_)
        return false;

    if (processed_ == 0)
        return !has_current_ &&
               std::all_of(blocks_.begin(), blocks_.end(), [](const auto& p) { return !p; });
    if (!has_current_ || current_slot_ >= num_blocks_ || !blocks_[current_slot_]) return false;

    const auto expected_current = static_cast<std::size_t>((processed_ - 1) % block_size_) + 1;
    if (blocks_[current_slot_]->processed() != expected_current) return false;

    const std::size_t expected_initialized =
        std::min(num_blocks_, static_cast<std::size_t>(((processed_ - 1) / block_size_) + 1));
    std::size_t initialized = 0;
    for (std::size_t i = 0; i < num_blocks_; ++i) {
        if (!blocks_[i]) continue;
        ++initialized;
        if (!blocks_[i]->validate()) return false;
        if (i != current_slot_ && blocks_[i]->processed() != block_size_) return false;
    }
    if (initialized != expected_initialized) return false;

    if (ready()) {
        for (const auto& block : blocks_)
            if (!block || block->processed() != block_size_) return false;
    }
    return true;
}

} // namespace space_saving
