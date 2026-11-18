#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <stdexcept>
#include <limits>
#include <unordered_map>
#include <vector>

namespace space_saving {

struct ExactCount {
    std::uint32_t element = 0;
    std::uint64_t count = 0;
};

// Exact oracle for the most recent W events. This structure is for evaluation
// only; its queue and full per-element map are not part of the approximate
// algorithm's memory budget.
class ExactSlidingWindow {
public:
    explicit ExactSlidingWindow(std::size_t window_size) : window_size_(window_size) {
        if (window_size_ == 0) throw std::invalid_argument("window size must be positive");
    }

    void update(std::uint32_t element) {
        if (processed_ == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("exact window stream length overflow");
        events_.push_back(element);
        ++counts_[element];
        ++processed_;
        if (events_.size() > window_size_) {
            const auto old = events_.front();
            events_.pop_front();
            auto it = counts_.find(old);
            if (it == counts_.end() || it->second == 0)
                throw std::logic_error("exact window count underflow");
            if (--it->second == 0) counts_.erase(it);
        }
    }

    std::vector<ExactCount> query_top_k(std::size_t k) const {
        if (k == 0) throw std::invalid_argument("k must be positive");
        std::vector<ExactCount> ranked;
        ranked.reserve(counts_.size());
        for (const auto& kv : counts_) ranked.push_back({kv.first, kv.second});
        std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
            if (a.count != b.count) return a.count > b.count;
            return a.element < b.element;
        });
        if (ranked.size() > k) ranked.resize(k);
        return ranked;
    }

    std::uint64_t count(std::uint32_t element) const {
        const auto it = counts_.find(element);
        return it == counts_.end() ? 0 : it->second;
    }

    const std::unordered_map<std::uint32_t, std::uint64_t>& counts() const { return counts_; }
    std::size_t window_size() const { return window_size_; }
    std::size_t size() const { return events_.size(); }
    std::uint64_t processed() const { return processed_; }
    bool full() const { return events_.size() == window_size_; }

    bool validate() const {
        if (events_.size() > window_size_) return false;
        std::uint64_t sum = 0;
        for (const auto& kv : counts_) {
            if (kv.second == 0) return false;
            sum += kv.second;
        }
        if (sum != events_.size()) return false;
        std::unordered_map<std::uint32_t, std::uint64_t> rebuilt;
        for (auto id : events_) ++rebuilt[id];
        return rebuilt == counts_;
    }

private:
    std::size_t window_size_;
    std::uint64_t processed_ = 0;
    std::deque<std::uint32_t> events_;
    std::unordered_map<std::uint32_t, std::uint64_t> counts_;
};

} // namespace space_saving
