#include "topk_metrics.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <cstdint>
struct Item { std::uint32_t element; };
int main(){
    const std::vector<std::uint64_t> counts{0,10,8,8,8,1,0};
    auto count=[&](auto id){return counts.at(id);};
    using space_saving::tie_metrics;
    auto a=tie_metrics(std::vector<Item>{{1},{3},{4}},3,8,1,count);
    assert(a.valid && a.precision==1 && a.recall==1);
    auto b=tie_metrics(std::vector<Item>{{2},{3},{4}},3,8,1,count);
    assert(!b.valid && b.precision==2.0/3 && b.recall==2.0/3);
    auto c=tie_metrics(std::vector<Item>{{1},{2},{2}},3,8,1,count);
    assert(!c.valid && c.recall==2.0/3);
    auto d=tie_metrics(std::vector<Item>{{1},{2}},3,8,1,count);
    assert(!d.valid && d.precision==1 && d.recall==2.0/3);
    auto e=tie_metrics(std::vector<Item>{{1},{2},{3},{4}},3,8,1,count);
    assert(!e.valid && e.precision==.75 && e.recall==1);
    std::cout<<"PASS: tie-aware metrics (alternative ties, missing mandatory, duplicate, short/long result)\n";
}
