#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_set>
namespace space_saving {
struct TieMetrics { double precision=0,recall=0; bool valid=false; };
// Maximum overlap with ANY valid exact top-k set. Mandatory items have
// frequency > kth; only the remaining slots may be filled by tied items.
// Zero-frequency keys and duplicate outputs never count as hits.
template<class Items, class Count>
TieMetrics tie_metrics(const Items& items, std::size_t target_size,
                       std::uint64_t kth, std::size_t mandatory, Count count) {
    std::unordered_set<std::uint32_t> seen;
    std::size_t strict_hits=0, tied_hits=0;
    for(const auto& item:items) {
        if(!seen.insert(item.element).second) continue;
        const auto f=count(item.element);
        if(f>kth && f>0) ++strict_hits;
        else if(f==kth && f>0) ++tied_hits;
    }
    const auto hits=strict_hits+std::min(tied_hits,target_size-mandatory);
    return {items.empty()?0.0:double(hits)/items.size(),
            target_size?double(hits)/target_size:0.0,
            items.size()==target_size && hits==target_size};
}
}
