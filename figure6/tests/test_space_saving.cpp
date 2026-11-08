#include "space_saving.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using space_saving::SpaceSaving;

void check_frequencies(const SpaceSaving& ss, const std::unordered_map<std::uint32_t, std::uint64_t>& truth) {
    assert(ss.validate());
    auto snapshots = ss.monitored_elements();
    std::uint64_t sum = 0;
    std::uint64_t max_untracked = 0;
    std::unordered_set<std::uint32_t> monitored;
    for (const auto& s : snapshots) {
        monitored.insert(s.element);
        auto f = truth.at(s.element);
        assert(s.lower_bound() <= f && f <= s.estimated_count);
        sum += s.estimated_count;
    }
    // Unmonitored elements have true frequency <= minimum monitored count.
    for (const auto& kv : truth)
        if (!monitored.count(kv.first)) max_untracked = std::max(max_untracked, kv.second);
    assert(max_untracked <= ss.min_count());
    assert(sum == ss.processed());
    assert(snapshots.size() == ss.monitored_count());
}

void check_guarantees(const SpaceSaving& ss, const std::unordered_map<std::uint32_t, std::uint64_t>& truth) {
    for (std::size_t k = 1; k <= ss.monitored_count(); ++k) {
        auto res = ss.query_top_k(k);
        if (res.guaranteed_topk) {
            std::unordered_set<std::uint32_t> picked;
            std::uint64_t min_picked = UINT64_MAX;
            std::uint64_t max_other = 0;
            for (const auto& e : res.elements) {
                picked.insert(e.element);
                min_picked = std::min(min_picked, truth.at(e.element));
            }
            for (const auto& kv : truth)
                if (!picked.count(kv.first)) max_other = std::max(max_other, kv.second);
            if (!(min_picked >= max_other)) { std::cerr << "BUG k=" << k << " m=" << ss.capacity() << " total=" << ss.processed() << " min=" << ss.min_count() << " selected min true=" << min_picked << " outside max=" << max_other << "\n"; for (auto e : ss.monitored_elements()) std::cerr << "  id=" << e.element << " count=" << e.estimated_count << " err=" << e.error << " truth=" << truth.at(e.element) << "\n"; for (auto p : truth) std::cerr << " " << p.first << ":" << p.second; std::cerr << "\n"; std::abort(); } // non-strict correctness with ties
        }
        if (res.guaranteed_order) {
            // By definition of the order guarantee, every reported adjacent pair
            // should be nonincreasing in actual frequency.
            for (std::size_t i = 1; i < res.elements.size(); ++i)
                assert(truth.at(res.elements[i - 1].element) >= truth.at(res.elements[i].element));
        }
    }
}

int main() {
    {
        SpaceSaving ss(2);
        std::unordered_map<std::uint32_t, std::uint64_t> truth;
        for (auto id : {1u, 2u, 2u, 3u}) {
            ss.update(id);
            truth[id]++;
            check_frequencies(ss, truth);
        }
        auto all = ss.monitored_elements();
        assert(all.size() == 2);
        bool found_y = false, found_z = false;
        for (const auto& x : all) {
            if (x.element == 2) { assert(x.estimated_count == 2 && x.error == 0); found_y = true; }
            if (x.element == 3) { assert(x.estimated_count == 2 && x.error == 1); found_z = true; }
        }
        assert(found_y && found_z);
        ss.update(3);
        truth[3]++;
        check_frequencies(ss, truth);
        for (const auto& x : ss.monitored_elements())
            if (x.element == 3) assert(x.estimated_count == 3 && x.error == 1);
    }
    {
        // Zero ID is a valid item. Capacity of one must not trigger free-list errors.
        SpaceSaving ss(1);
        std::unordered_map<std::uint32_t, std::uint64_t> truth;
        for (auto id : {0u, 0u, 1u, 0u, 2u, 1u}) {
            ss.update(id);
            truth[id]++;
            check_frequencies(ss, truth);
        }
    }
    {
        // If capacity >= distinct items, exact frequency and zero error.
        SpaceSaving ss(10);
        for (auto id : {0u, 9u, 9u, 1u, 1u, 1u}) ss.update(id);
        for (const auto& e : ss.monitored_elements()) assert(e.error == 0);
        auto top = ss.query_top_k(2);
        assert(top.elements.size() == 2);
        assert(top.elements[0].element == 1 && top.elements[0].estimated_count == 3);
        assert(top.guaranteed_topk);
    }
    {
        // Differential tests of every prefix, including ties, evictions and collisions.
        for (std::size_t m = 1; m <= 15; ++m) {
            for (int seed = 0; seed < 12; ++seed) {
                SpaceSaving ss(m);
                std::unordered_map<std::uint32_t, std::uint64_t> truth;
                std::mt19937 rng(100 + seed * 7);
                std::uniform_int_distribution<int> dist(0, 35);
                for (int t = 0; t < 300; ++t) {
                    auto id = static_cast<std::uint32_t>(dist(rng));
                    ss.update(id);
                    truth[id]++;
                    check_frequencies(ss, truth);
                    check_guarantees(ss, truth);
                }
            }
        }
    }
    std::cout << "PASS: Space-Saving / Stream-Summary / error bounds / Top-k checks\n";
}
