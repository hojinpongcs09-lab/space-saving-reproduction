#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace space_saving {

// CountSketch (Charikar, Chen, Farach-Colton): signed bucket hashes, median
// frequency estimator, and a k-candidate indexed min heap for Top-k enumeration.
// The heap stores an estimate when an item enters and increments it when that
// item is seen again, following the original CountSketch top-k proposal.
// Candidate scores are NOT guaranteed frequency bounds (unlike Space-Saving).
class CountSketch {
public:
    struct Candidate {
        std::uint32_t element;
        std::int64_t estimated_count;
    };

    // width must be a power of 2 >= 2; depth must be odd (1..31).
    CountSketch(std::size_t width, std::size_t depth, std::size_t topk,
                std::uint64_t seed = 2026);

    void update(std::uint32_t element);
    std::int64_t estimate(std::uint32_t element) const;
    std::vector<Candidate> query_top_k(std::size_t k) const;
    std::size_t allocated_bytes() const;
    bool validate() const;

    std::size_t width() const { return width_; }
    std::size_t depth() const { return depth_; }
    std::size_t candidates() const { return heap_.size(); }
    std::size_t topk_capacity() const { return capacity_; }
    std::uint64_t processed() const { return processed_; }

private:
    struct HashRow {
        std::uint64_t bucket_a, bucket_b, sign_a, sign_b;
    };

    std::size_t width_, depth_, capacity_, shift_;
    std::uint64_t processed_ = 0;
    std::vector<std::int64_t> counters_;
    std::vector<HashRow> hashes_;
    std::vector<Candidate> heap_; // min-heap, lowest estimate first
    std::unordered_map<std::uint32_t, std::size_t> positions_;

    std::size_t hash_bucket(std::size_t row, std::uint32_t id) const;
    int hash_sign(std::size_t row, std::uint32_t id) const;
    bool less(const Candidate& a, const Candidate& b) const;
    void swap_heap(std::size_t a, std::size_t b);
    void sift_up(std::size_t i);
    void sift_down(std::size_t i);
};

} // namespace space_saving
