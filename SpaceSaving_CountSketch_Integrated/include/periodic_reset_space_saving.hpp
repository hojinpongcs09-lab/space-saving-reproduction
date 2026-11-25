#pragma once

#include "space_saving.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>

namespace space_saving {

// Tumbling/periodic-reset baseline. The current summary only contains packets
// since the latest aligned W-boundary. Queries at n*W are made *before* the
// next packet triggers a reset, so they cover exactly the last W packets.
// Queries at intermediate times cover fewer than W packets, intentionally.
class PeriodicResetSpaceSaving {
public:
    PeriodicResetSpaceSaving(std::size_t window, std::size_t counters)
        : window_(window), counters_(counters), summary_(std::make_unique<SpaceSaving>(counters)) {
        if (!window || !counters) throw std::invalid_argument("window and counters must be positive");
    }

    void update(std::uint32_t element) {
        if (total_processed_ && (total_processed_ % window_ == 0)) {
            // Drop the old allocation first to avoid a temporary 2x peak.
            summary_.reset();
            summary_ = std::make_unique<SpaceSaving>(counters_);
            active_span_ = 0;
        }
        summary_->update(element);
        ++active_span_;
        ++total_processed_;
    }

    TopKResult query_top_k(std::size_t k) const { return summary_->query_top_k(k); }
    std::size_t monitored_count() const { return summary_->monitored_count(); }
    std::uint64_t active_span() const { return active_span_; }
    std::uint64_t processed() const { return total_processed_; }
    bool complete_window() const { return active_span_ == window_; }
    std::size_t allocated_bytes() const { return summary_->allocated_bytes(); }
    bool validate() const { return summary_->validate() && summary_->processed() == active_span_ && active_span_ <= window_; }

private:
    std::size_t window_;
    std::size_t counters_;
    std::unique_ptr<SpaceSaving> summary_;
    std::uint64_t total_processed_ = 0;
    std::uint64_t active_span_ = 0;
};

} // namespace space_saving
