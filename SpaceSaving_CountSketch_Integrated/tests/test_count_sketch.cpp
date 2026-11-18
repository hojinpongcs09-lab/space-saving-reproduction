#include "count_sketch.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <set>
using space_saving::CountSketch;
int main() {
    CountSketch one(1024,7,5,42);
    for(int i=0;i<150;i++) one.update(7);
    assert(one.validate() && one.estimate(7)==150 && one.candidates()==1);
    assert(one.query_top_k(1)[0].element==7);
    assert(one.allocated_bytes()>7*1024*sizeof(std::int64_t));
    CountSketch cs(4096,7,4,2030);
    for (std::uint32_t i=1;i<=200;i++) cs.update(i);
    for (int i=0;i<3000;i++) cs.update(9);
    for (int i=0;i<1500;i++) cs.update(15);
    for (std::uint32_t i=1000;i<1200;i++) cs.update(i);
    assert(cs.validate());
    auto top=cs.query_top_k(4);
    std::set<std::uint32_t> ids;
    for (auto x:top) ids.insert(x.element);
    assert(ids.size()==4 && ids.count(9) && ids.count(15));
    CountSketch a(1024,5,10,77),b(1024,5,10,77);
    for (std::uint32_t i=0;i<1000;i++){
        auto id=(i*107)%121+1;
        a.update(id); b.update(id);
    }
    assert(a.validate() && b.validate());
    for (std::uint32_t i=1;i<=121;i++)assert(a.estimate(i)==b.estimate(i));
    auto ra=a.query_top_k(10),rb=b.query_top_k(10);
    assert(ra.size()==rb.size());
    for(std::size_t i=0;i<ra.size();i++)
        assert(ra[i].element==rb[i].element && ra[i].estimated_count==rb[i].estimated_count);
    // Repeated ties, candidate bucket reuse and arbitrary initial estimates.
    CountSketch tied(16,3,7,100);
    for (int round=0;round<40;round++)
        for (std::uint32_t id=1;id<50;id++) tied.update(id);
    assert(tied.validate());
    std::cout<<"CountSketch Stream-Summary tests passed\n";
}
