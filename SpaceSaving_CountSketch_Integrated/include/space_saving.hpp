#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace space_saving {

// One monitored element, with a provable frequency interval:
//   estimated_count - error <= true_count <= estimated_count.
struct CountEstimate {
    std::uint32_t element;
    std::uint64_t estimated_count;
    std::uint64_t error;
    std::uint64_t lower_bound() const { return estimated_count - error; }
};

struct TopKResult {
    std::vector<CountEstimate> elements; // descending estimated frequency; ID breaks ties
    bool guaranteed_topk = false;        // paper's non-strict bound; ties may be ambiguous
    bool guaranteed_order = false;       // paper's non-strict adjacent bounds
    bool guaranteed_topk_strict = false; // strictly separated, unique top-k membership
};

// Refactored from the *LCU* (linked-group space-saving) design included in
// Cormode et al.'s frequent-items-10 package. See THIRD_PARTY_NOTICES.md.
//
// Stream-Summary stores counters in doubly linked COUNT buckets. Each bucket
// contains a circular doubly linked list of Element nodes. A separate chained
// hash table maps an element ID to its node; there are exactly capacity counters.
class SpaceSaving {
public:
    explicit SpaceSaving(std::size_t capacity);

    SpaceSaving(const SpaceSaving&) = delete;
    SpaceSaving& operator=(const SpaceSaving&) = delete;
    SpaceSaving(SpaceSaving&&) = delete;
    SpaceSaving& operator=(SpaceSaving&&) = delete;

    void update(std::uint32_t element);
    TopKResult query_top_k(std::size_t k) const;
    std::vector<CountEstimate> monitored_elements() const;

    std::uint64_t processed() const { return processed_; }
    std::size_t capacity() const { return capacity_; }
    std::size_t monitored_count() const { return monitored_; }
    std::uint64_t min_count() const;

    // Count the object's own allocated memory (not input, generator, truth,
    // query output, or allocator metadata). Deterministic on a given ABI.
    std::size_t allocated_bytes() const;

    // Deep integrity check, intended for tests, not normal measured updates.
    bool validate() const;

private:
    struct Bucket;
    struct Element {
        std::uint32_t id = 0;
        std::uint64_t error = 0;
        bool active = false;
        Bucket* bucket = nullptr;
        Element* prev = nullptr;
        Element* next = nullptr;
        Element* hash_prev = nullptr;
        Element* hash_next = nullptr;
    };
    struct Bucket {
        std::uint64_t count = 0;
        Element* head = nullptr;
        Bucket* prev = nullptr;
        Bucket* next = nullptr;
    };

    std::size_t capacity_ = 0;
    std::uint64_t processed_ = 0;
    std::size_t monitored_ = 0;
    // The actual Stream-Summary: hash index + ordered frequency buckets
    // containing circular lists of monitored element nodes.
    struct StreamSummary {
        std::vector<Element> elements;
        std::vector<Bucket> buckets;
        std::vector<Bucket*> free_buckets;
        std::vector<Element*> hash_heads;
        Bucket* smallest = nullptr;
        Bucket* largest = nullptr;
    } summary_;

    std::size_t hash_index(std::uint32_t id) const;
    Element* find(std::uint32_t id) const;
    void hash_insert(Element* element);
    void hash_remove(Element* element);
    static void attach(Bucket* bucket, Element* element);
    static void detach(Bucket* bucket, Element* element);
    void unlink_empty_bucket(Bucket* bucket);
    Bucket* insert_bucket_after(Bucket* before, std::uint64_t count);
    void increment(Element* element);
};

} // namespace space_saving
