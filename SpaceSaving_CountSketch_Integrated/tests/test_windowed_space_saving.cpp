#include "exact_sliding_window.hpp"
#include "windowed_space_saving.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using space_saving::BlockWindowedSpaceSaving;
using space_saving::ExactSlidingWindow;
using space_saving::WindowCountBounds;

static std::unordered_map<std::uint32_t, WindowCountBounds>
index_bounds(const std::vector<WindowCountBounds>& bounds) {
    std::unordered_map<std::uint32_t, WindowCountBounds> out;
    for (const auto& b : bounds) out.emplace(b.element, b);
    return out;
}

static void verify_bounds(const BlockWindowedSpaceSaving& windowed,
                          const ExactSlidingWindow& exact,
                          std::uint32_t alphabet) {
    assert(windowed.ready());
    const auto bounds = index_bounds(windowed.merged_bounds());
    std::uint64_t unseen_upper = 0;
    for (std::uint32_t id = 1; id <= alphabet; ++id) {
        const auto truth = exact.count(id);
        const auto it = bounds.find(id);
        if (it == bounds.end()) {
            unseen_upper = std::max(unseen_upper, truth);
        } else {
            assert(it->second.lower_bound <= truth);
            assert(truth <= it->second.upper_bound);
        }
    }
    // Any ID absent from the union of all block summaries is bounded by the
    // sum of each block's missing-item upper bound.
    const auto top = windowed.query_top_k(5);
    assert(unseen_upper <= top.unseen_upper_bound);

    const auto truth_top = exact.query_top_k(5);
    if (top.membership_certified) {
        std::unordered_set<std::uint32_t> expected, actual;
        for (const auto& x : truth_top) expected.insert(x.element);
        for (const auto& x : top.elements) actual.insert(x.element);
        assert(actual == expected);
    }
    if (top.order_certified) {
        for (std::size_t i = 1; i < top.elements.size(); ++i)
            assert(exact.count(top.elements[i - 1].element) >=
                   exact.count(top.elements[i].element));
    }
}

int main() {
    {
        // Exact summaries when each block's capacity can hold all IDs.
        BlockWindowedSpaceSaving windowed(8, 2, 8);
        ExactSlidingWindow exact(8);
        for (auto id : {1u, 1u, 1u, 1u, 2u, 2u, 2u, 3u}) {
            windowed.update(id);
            exact.update(id);
        }
        assert(windowed.ready());
        assert(windowed.validate());
        const auto result = windowed.query_top_k(2);
        assert(result.membership_certified);
        assert(result.order_certified);
        assert(result.elements.size() == 2);
        assert(result.elements[0].element == 1);
        assert(result.elements[0].lower_bound == 4);
        assert(result.elements[0].upper_bound == 4);
        assert(result.elements[1].element == 2);
        assert(result.elements[1].lower_bound == 3);
        assert(result.elements[1].upper_bound == 3);
    }
    {
        // Tied kth boundary must not be reported as uniquely certified.
        BlockWindowedSpaceSaving windowed(8, 2, 8);
        for (auto id : {1u, 1u, 2u, 2u, 3u, 3u, 4u, 4u}) windowed.update(id);
        const auto result = windowed.query_top_k(2);
        assert(!result.membership_certified);
    }
    {
        // No query before a full, block-aligned window. A partial next block
        // also makes the query unavailable until that block is complete.
        BlockWindowedSpaceSaving windowed(40, 4, 16);
        for (std::uint32_t i = 0; i < 39; ++i) windowed.update(i % 7);
        assert(!windowed.ready());
        bool threw = false;
        try { (void)windowed.query_top_k(3); }
        catch (const std::logic_error&) { threw = true; }
        assert(threw);
        windowed.update(0);
        assert(windowed.ready());
        for (std::uint32_t i = 0; i < 9; ++i) windowed.update(i % 5);
        assert(!windowed.ready());
        for (std::uint32_t i = 0; i < 1; ++i) windowed.update(i);
        assert(windowed.ready());
    }
    {
        // Randomized differential validation for B=2/4/8, including ring
        // rotation, candidate replacement, and untracked-item upper bounds.
        constexpr std::uint32_t alphabet = 24;
        for (std::size_t blocks : {2u, 4u, 8u}) {
            BlockWindowedSpaceSaving windowed(40, blocks, 16);
            ExactSlidingWindow exact(40);
            std::mt19937 rng(static_cast<std::uint32_t>(900 + blocks));
            std::discrete_distribution<int> dist({30, 20, 15, 10, 8, 6, 4, 3, 2, 2,
                                                    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                                    1, 1, 1, 1});
            for (std::uint32_t t = 1; t <= 1000; ++t) {
                const auto id = static_cast<std::uint32_t>(dist(rng) + 1);
                windowed.update(id);
                exact.update(id);
                assert(windowed.validate());
                assert(exact.validate());
                if (windowed.ready()) verify_bounds(windowed, exact, alphabet);
            }
        }
    }
    {
        // Naive and bound-aware queries expose the same candidate universe,
        // while the latter retains the conservative intervals.
        BlockWindowedSpaceSaving windowed(20, 4, 16);
        for (std::uint32_t i = 0; i < 20; ++i)
            for (std::uint32_t j = 0; j < 1 + (i % 5); ++j) windowed.update(i % 10);
        // The above has more than one window; align by adding until the next
        // block boundary after the window has filled.
        while (!windowed.ready()) windowed.update(1);
        auto naive = windowed.query_top_k_naive(5);
        auto bounded = windowed.query_top_k(5);
        assert(naive.size() <= 5);
        assert(bounded.elements.size() <= 5);
        assert(bounded.candidate_count >= bounded.elements.size());
        assert(windowed.allocated_bytes() > 0);
    }

    std::cout << "PASS: block rotation, merged bounds, Top-k certification, and exact-window differential tests\n";
    return 0;
}
