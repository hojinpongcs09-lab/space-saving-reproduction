#pragma once
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace space_saving {

// Signed-hash CountSketch with the fixed-k, count-ordered Stream-Summary
// candidate structure described by Metwally et al. (2005).
// This is deliberately NOT a Count-Min sketch and NOT a min heap.
class CountSketch {
public:
    struct Candidate { std::uint32_t element; std::int64_t estimated_count; };
    CountSketch(std::size_t width, std::size_t depth, std::size_t topk,
                std::uint64_t seed = 2026);
    void update(std::uint32_t element);
    std::int64_t estimate(std::uint32_t element) const;
    std::vector<Candidate> query_top_k(std::size_t k) const;
    std::size_t allocated_bytes() const;
    bool validate() const;
    std::size_t width() const { return width_; }
    std::size_t depth() const { return depth_; }
    std::size_t candidates() const { return size_; }
    std::size_t topk_capacity() const { return capacity_; }
    std::uint64_t processed() const { return processed_; }
private:
    struct HashRow { std::uint64_t bucket_a, bucket_b, sign_a, sign_b; };
    struct Group;
    struct Node {
        std::uint32_t element = 0;
        std::int64_t estimate = 0;
        Group* group = nullptr;
        Node* prev = nullptr;
        Node* next = nullptr;
    };
    struct Group {
        std::int64_t count = 0;
        Node* head = nullptr;
        Group* prev = nullptr;
        Group* next = nullptr;
    };
    std::size_t width_, depth_, capacity_, shift_, size_ = 0;
    std::uint64_t processed_ = 0;
    std::vector<std::int64_t> counters_;
    std::vector<HashRow> hashes_;
    // Fixed-size Stream-Summary: linked, sorted frequency groups with
    // circular doubly-linked member lists, indexed by id.
    std::vector<Node> nodes_;
    std::vector<Group> groups_;
    std::vector<Group*> free_groups_;
    std::unordered_map<std::uint32_t, Node*> index_;
    Group* smallest_ = nullptr;
    Group* largest_ = nullptr;

    std::size_t hash_bucket(std::size_t row, std::uint32_t id) const;
    int hash_sign(std::size_t row, std::uint32_t id) const;
    static void attach(Group* g, Node* n);
    static void detach(Group* g, Node* n);
    void remove_empty(Group* g);
    Group* add_group_after(Group* before, std::int64_t count);
    void insert_ranked(Node* n, std::int64_t count);
    void increment(Node* n);
};
} // namespace space_saving
