#include "periodic_reset_space_saving.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <vector>

using space_saving::PeriodicResetSpaceSaving;

static std::uint64_t freq(const std::unordered_map<std::uint32_t, std::uint64_t>& f, std::uint32_t id) {
    auto it = f.find(id);
    return it == f.end() ? 0 : it->second;
}

int main() {
    {
        PeriodicResetSpaceSaving ss(4, 4);
        const std::vector<std::uint32_t> data{1,1,1,1, 2,2, 2,2, 3,3,3,3};
        for (std::size_t i=0; i<data.size(); ++i) {
            ss.update(data[i]);
            assert(ss.validate());
            assert(ss.active_span() == (i%4+1));
            const auto top = ss.query_top_k(1);
            assert(top.elements.size()==1);
            assert(top.elements[0].element == (i<4 ? 1u : i<8 ? 2u : 3u));
            if (i==3 || i==7 || i==11) assert(ss.complete_window());
            else assert(!ss.complete_window());
        }
    }
    {
        PeriodicResetSpaceSaving ss(5, 2);
        const std::vector<std::uint32_t> data{1,2,3,3,4, 5,1,1,6,6, 7,7,7};
        std::unordered_map<std::uint32_t,std::uint64_t> exact_interval;
        for (std::size_t i=0; i<data.size(); ++i) {
            if (i && i%5==0) exact_interval.clear();
            ++exact_interval[data[i]];
            ss.update(data[i]);
            assert(ss.validate());
            for (const auto& x : ss.query_top_k(2).elements) {
                assert(x.lower_bound() <= freq(exact_interval,x.element));
                assert(freq(exact_interval,x.element) <= x.estimated_count);
            }
        }
    }
    std::cout << "Periodic-reset tests passed\n";
}
