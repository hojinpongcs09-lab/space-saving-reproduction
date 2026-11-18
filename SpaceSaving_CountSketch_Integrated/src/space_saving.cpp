#include "space_saving.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace space_saving {

SpaceSaving::SpaceSaving(std::size_t capacity) : capacity_(capacity) {
    if (capacity == 0 || capacity > (std::numeric_limits<std::size_t>::max() - 1) / 3)
        throw std::invalid_argument("capacity must be positive and not overflow the table");
    summary_.elements.resize(capacity);
    summary_.buckets.resize(capacity);
    summary_.hash_heads.resize(capacity * 3 + 1, nullptr);
    summary_.free_buckets.reserve(capacity - 1);
    summary_.smallest = summary_.largest = &summary_.buckets[0];
    for (std::size_t i = 1; i < capacity; ++i) summary_.free_buckets.push_back(&summary_.buckets[i]);
    for (auto& element : summary_.elements) attach(summary_.smallest, &element); // all initially unused, count=0
}

std::size_t SpaceSaving::hash_index(std::uint32_t id) const {
    // SplitMix64 finalizer; unlike the original hash31 helper, handles ID zero.
    std::uint64_t x = id;
    x += UINT64_C(0x9e3779b97f4a7c15);
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x ^= (x >> 31);
    return static_cast<std::size_t>(x % summary_.hash_heads.size());
}

SpaceSaving::Element* SpaceSaving::find(std::uint32_t id) const {
    for (Element* cur = summary_.hash_heads[hash_index(id)]; cur != nullptr; cur = cur->hash_next)
        if (cur->id == id) return cur;
    return nullptr;
}

void SpaceSaving::hash_insert(Element* element) {
    auto& head = summary_.hash_heads[hash_index(element->id)];
    element->hash_prev = nullptr;
    element->hash_next = head;
    if (head) head->hash_prev = element;
    head = element;
}

void SpaceSaving::hash_remove(Element* element) {
    if (element->hash_prev) element->hash_prev->hash_next = element->hash_next;
    else summary_.hash_heads[hash_index(element->id)] = element->hash_next;
    if (element->hash_next) element->hash_next->hash_prev = element->hash_prev;
    element->hash_prev = element->hash_next = nullptr;
}

void SpaceSaving::attach(Bucket* bucket, Element* element) {
    element->bucket = bucket;
    if (!bucket->head) {
        bucket->head = element;
        element->next = element->prev = element;
    } else {
        Element* head = bucket->head;
        element->next = head;
        element->prev = head->prev;
        head->prev->next = element;
        head->prev = element;
    }
}

void SpaceSaving::detach(Bucket* bucket, Element* element) {
    if (element->next == element) {
        bucket->head = nullptr;
    } else {
        element->prev->next = element->next;
        element->next->prev = element->prev;
        if (bucket->head == element) bucket->head = element->next;
    }
    element->next = element->prev = nullptr;
    element->bucket = nullptr;
}

void SpaceSaving::unlink_empty_bucket(Bucket* bucket) {
    if (bucket->head) throw std::logic_error("attempt to remove nonempty bucket");
    if (bucket->prev) bucket->prev->next = bucket->next;
    else summary_.smallest = bucket->next;
    if (bucket->next) bucket->next->prev = bucket->prev;
    else summary_.largest = bucket->prev;
    bucket->prev = bucket->next = nullptr;
    summary_.free_buckets.push_back(bucket);
}

SpaceSaving::Bucket* SpaceSaving::insert_bucket_after(Bucket* before, std::uint64_t count) {
    if (summary_.free_buckets.empty()) throw std::logic_error("exhausted bucket pool");
    Bucket* new_bucket = summary_.free_buckets.back();
    summary_.free_buckets.pop_back();
    new_bucket->count = count;
    new_bucket->head = nullptr;
    new_bucket->prev = before;
    new_bucket->next = before->next;
    if (before->next) before->next->prev = new_bucket;
    else summary_.largest = new_bucket;
    before->next = new_bucket;
    return new_bucket;
}

void SpaceSaving::increment(Element* element) {
    Bucket* old = element->bucket;
    if (!old) throw std::logic_error("element missing bucket");
    if (old->count == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("counter overflow");
    const std::uint64_t next_count = old->count + 1;
    Bucket* target = old->next;
    if (target && target->count == next_count) {
        detach(old, element);
        attach(target, element);
        if (!old->head) unlink_empty_bucket(old);
    } else if (old->head->next == old->head) {
        // The only item in this bucket; change the bucket's count in place.
        old->count = next_count;
    } else {
        target = insert_bucket_after(old, next_count);
        detach(old, element);
        attach(target, element);
    }
}

void SpaceSaving::update(std::uint32_t id) {
    if (processed_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("stream length overflow");
    Element* element = find(id);
    if (!element) {
        // Choose by *estimated count* only, never count-error.
        element = summary_.smallest->head;
        if (element->active) hash_remove(element);
        else ++monitored_;
        element->id = id;
        element->error = summary_.smallest->count; // paper: eps_new = min, count_new = min + 1
        element->active = true;
        hash_insert(element);
    }
    increment(element);
    ++processed_;
}

std::vector<CountEstimate> SpaceSaving::monitored_elements() const {
    std::vector<CountEstimate> result;
    result.reserve(monitored_);
    for (const Element& element : summary_.elements) {
        if (element.active)
            result.push_back({element.id, element.bucket->count, element.error});
    }
    std::sort(result.begin(), result.end(), [](const CountEstimate& a, const CountEstimate& b) {
        if (a.estimated_count != b.estimated_count)
            return a.estimated_count > b.estimated_count;
        return a.element < b.element;
    });
    return result;
}

TopKResult SpaceSaving::query_top_k(std::size_t k) const {
    if (k == 0) throw std::invalid_argument("k must be positive");
    auto ranked = monitored_elements();
    TopKResult result;
    std::size_t take = std::min(k, ranked.size());
    result.elements.assign(ranked.begin(), ranked.begin() + take);
    if (take < k) return result; // fewer than k distinct IDs have appeared

    // Figure 4: a monitored item has f in [count-error, count].
    std::uint64_t smallest_lower = std::numeric_limits<std::uint64_t>::max();
    result.guaranteed_order = true;
    for (std::size_t i = 0; i < k; ++i) {
        smallest_lower = std::min(smallest_lower, ranked[i].lower_bound());
        if (i + 1 < ranked.size() && ranked[i].lower_bound() < ranked[i + 1].estimated_count)
            result.guaranteed_order = false;
    }
    if (k < ranked.size()) {
        std::uint64_t next_upper = ranked[k].estimated_count;
        result.guaranteed_topk = smallest_lower >= next_upper;
        result.guaranteed_topk_strict = smallest_lower > next_upper;
    } else {
        // The paper's top-m special case requires epsilon_m = 0.
        result.guaranteed_topk = (ranked.back().error == 0) && result.guaranteed_order;
        result.guaranteed_topk_strict = (ranked.back().error == 0) && result.guaranteed_order;
    }
    return result;
}

std::uint64_t SpaceSaving::min_count() const { return summary_.smallest ? summary_.smallest->count : 0; }

std::size_t SpaceSaving::allocated_bytes() const {
    return sizeof(*this) + summary_.elements.capacity() * sizeof(Element)
        + summary_.buckets.capacity() * sizeof(Bucket)
        + summary_.free_buckets.capacity() * sizeof(Bucket*)
        + summary_.hash_heads.capacity() * sizeof(Element*);
}

bool SpaceSaving::validate() const {
    if (capacity_ == 0 || !summary_.smallest || !summary_.largest || summary_.smallest->prev || summary_.largest->next) return false;
    std::unordered_set<const Element*> visited;
    std::unordered_set<const Bucket*> buckets_seen;
    std::uint64_t sum = 0;
    std::size_t active = 0;
    const Bucket* previous = nullptr;
    for (const Bucket* bucket = summary_.smallest; bucket; bucket = bucket->next) {
        if (!buckets_seen.insert(bucket).second || bucket->prev != previous || !bucket->head) return false;
        if (previous && previous->count >= bucket->count) return false;
        const Element* head = bucket->head;
        const Element* e = head;
        std::size_t guard = 0;
        do {
            if (!e || ++guard > capacity_ || e->bucket != bucket || !e->next || !e->prev ||
                e->next->prev != e || e->prev->next != e || !visited.insert(e).second) return false;
            if (e->active) {
                if (e->error > bucket->count || find(e->id) != e) return false;
                ++active;
            } else if (bucket->count != 0) return false;
            sum += bucket->count;
            e = e->next;
        } while (e != head);
        previous = bucket;
    }
    if (previous != summary_.largest || visited.size() != capacity_ || active != monitored_ || sum != processed_) return false;
    for (std::size_t i = 0; i < summary_.hash_heads.size(); ++i) {
        const Element* prev = nullptr;
        for (const Element* e = summary_.hash_heads[i]; e; e = e->hash_next) {
            if (!e->active || e->hash_prev != prev || hash_index(e->id) != i) return false;
            prev = e;
        }
    }
    return true;
}

} // namespace space_saving
