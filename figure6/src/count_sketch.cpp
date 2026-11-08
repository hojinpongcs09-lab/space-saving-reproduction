#include "count_sketch.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <random>
#include <stdexcept>
#include <utility>

namespace space_saving {

CountSketch::CountSketch(std::size_t width, std::size_t depth, std::size_t topk,
                         std::uint64_t seed)
    : width_(width), depth_(depth), capacity_(topk), shift_(0) {
    if (width < 2 || (width & (width - 1)) || width > (1U << 27))
        throw std::invalid_argument("CountSketch width must be a power of 2 >= 2 and <= 2^27");
    if (depth == 0 || depth > 31 || (depth % 2) == 0)
        throw std::invalid_argument("CountSketch depth must be odd, 1..31");
    if (topk == 0 || topk > (1U << 27))
        throw std::invalid_argument("CountSketch top-k candidate capacity must be positive");
    if (depth > std::numeric_limits<std::size_t>::max() / width)
        throw std::overflow_error("CountSketch counter capacity overflow");
    while ((1ULL << shift_) < width) ++shift_;
    shift_ = 64 - shift_;
    counters_.assign(depth * width, 0);
    hashes_.reserve(depth);
    heap_.reserve(topk);
    positions_.reserve(topk * 2);
    std::mt19937_64 rng(seed);
    for (std::size_t i = 0; i < depth; ++i) {
        // Independent multiply-add 64-bit hash families, odd multipliers.
        hashes_.push_back({rng() | 1ULL, rng(), rng() | 1ULL, rng()});
    }
}

std::size_t CountSketch::hash_bucket(std::size_t row, std::uint32_t id) const {
    const auto& h = hashes_[row];
    return static_cast<std::size_t>((h.bucket_a * id + h.bucket_b) >> shift_);
}
int CountSketch::hash_sign(std::size_t row, std::uint32_t id) const {
    const auto& h = hashes_[row];
    return ((h.sign_a * id + h.sign_b) >> 63) ? 1 : -1;
}

std::int64_t CountSketch::estimate(std::uint32_t id) const {
    std::array<std::int64_t, 31> values{};
    for (std::size_t row = 0; row < depth_; ++row) {
        values[row] = counters_[row * width_ + hash_bucket(row, id)] * hash_sign(row, id);
    }
    auto nth = values.begin() + static_cast<std::ptrdiff_t>(depth_ / 2);
    std::nth_element(values.begin(), nth, values.begin() + static_cast<std::ptrdiff_t>(depth_));
    return *nth;
}

bool CountSketch::less(const Candidate& a, const Candidate& b) const {
    if (a.estimated_count != b.estimated_count) return a.estimated_count < b.estimated_count;
    // If equal, retain smaller ID, making largest ID the weaker candidate.
    return a.element > b.element;
}
void CountSketch::swap_heap(std::size_t a, std::size_t b) {
    std::swap(heap_[a], heap_[b]);
    positions_[heap_[a].element] = a;
    positions_[heap_[b].element] = b;
}
void CountSketch::sift_up(std::size_t i) {
    while (i) {
        const std::size_t p = (i - 1) / 2;
        if (!less(heap_[i], heap_[p])) break;
        swap_heap(p, i);
        i = p;
    }
}
void CountSketch::sift_down(std::size_t i) {
    while (2 * i + 1 < heap_.size()) {
        std::size_t ch = 2 * i + 1;
        if (ch + 1 < heap_.size() && less(heap_[ch + 1], heap_[ch])) ++ch;
        if (!less(heap_[ch], heap_[i])) break;
        swap_heap(ch, i);
        i = ch;
    }
}

void CountSketch::update(std::uint32_t id) {
    if (processed_ >= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        throw std::overflow_error("CountSketch stream too long");
    for (std::size_t row = 0; row < depth_; ++row) {
        counters_[row * width_ + hash_bucket(row, id)] += hash_sign(row, id);
    }
    ++processed_;
    const auto pos = positions_.find(id);
    if (pos != positions_.end()) {
        // Only the item's own arrivals change its stored candidate count.
        // This follows the heap update in Charikar et al.'s Top-k procedure.
        ++heap_[pos->second].estimated_count;
        sift_down(pos->second);
        return;
    }
    const Candidate c{id, estimate(id)};
    if (heap_.size() < capacity_) {
        const std::size_t index = heap_.size();
        heap_.push_back(c);
        positions_.emplace(id, index);
        sift_up(index);
    } else if (less(heap_.front(), c)) {
        positions_.erase(heap_.front().element);
        heap_[0] = c;
        positions_[id] = 0;
        sift_down(0);
    }
}

std::vector<CountSketch::Candidate> CountSketch::query_top_k(std::size_t k) const {
    if (k == 0 || k > capacity_) throw std::invalid_argument("k must be 1..candidate_capacity");
    auto out = heap_;
    std::sort(out.begin(), out.end(), [](const Candidate& a, const Candidate& b) {
        if (a.estimated_count != b.estimated_count) return a.estimated_count > b.estimated_count;
        return a.element < b.element;
    });
    if (out.size() > k) out.resize(k);
    return out;
}

std::size_t CountSketch::allocated_bytes() const {
    // Deterministic estimate of owned allocations, excludes platform allocator
    // metadata. std::unordered_map nodes vary by STL ABI; count payload+pointer.
    return sizeof(*this) + counters_.capacity() * sizeof(std::int64_t)
        + hashes_.capacity() * sizeof(HashRow)
        + heap_.capacity() * sizeof(Candidate)
        + positions_.bucket_count() * sizeof(void*)
        + positions_.size() * (sizeof(std::pair<const std::uint32_t, std::size_t>) + sizeof(void*));
}

bool CountSketch::validate() const {
    if (heap_.size() > capacity_ || heap_.size() != positions_.size()) return false;
    for (std::size_t i = 0; i < heap_.size(); ++i) {
        const auto found = positions_.find(heap_[i].element);
        if (found == positions_.end() || found->second != i) return false;
        if (i && less(heap_[i], heap_[(i - 1) / 2])) return false;
    }
    return true;
}

} // namespace space_saving
