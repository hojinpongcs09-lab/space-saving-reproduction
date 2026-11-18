#include "count_sketch.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <random>
#include <stdexcept>
#include <unordered_set>

namespace space_saving {

CountSketch::CountSketch(std::size_t width, std::size_t depth, std::size_t topk,
                         std::uint64_t seed)
    : width_(width), depth_(depth), capacity_(topk), shift_(0) {
    if (width < 2 || (width & (width-1)) || width > (1U << 27))
        throw std::invalid_argument("CountSketch width: power-of-2 in [2,2^27]");
    if (!depth || depth > 31 || !(depth & 1))
        throw std::invalid_argument("CountSketch depth must be odd (1..31)");
    if (!topk || topk > (1U << 26))
        throw std::invalid_argument("CountSketch topk out of range");
    if (depth > std::numeric_limits<std::size_t>::max() / width)
        throw std::overflow_error("sketch size overflow");
    while ((1ULL << shift_) < width) ++shift_;
    shift_ = 64 - shift_;
    counters_.assign(depth*width, 0);
    hashes_.reserve(depth);
    nodes_.resize(topk);
    groups_.resize(topk);
    free_groups_.reserve(topk);
    for (auto &g : groups_) free_groups_.push_back(&g);
    index_.reserve(topk * 2);
    std::mt19937_64 rng(seed);
    for (std::size_t r=0;r<depth;r++)
        hashes_.push_back({rng()|1ULL, rng(), rng()|1ULL, rng()});
}

std::size_t CountSketch::hash_bucket(std::size_t row, std::uint32_t id) const {
    const auto& h = hashes_[row];
    return static_cast<std::size_t>((h.bucket_a * id + h.bucket_b) >> shift_);
}
int CountSketch::hash_sign(std::size_t row, std::uint32_t id) const {
    const auto& h = hashes_[row];
    return ((h.sign_a * id + h.sign_b) >> 63) ? 1 : -1;
}
std::int64_t CountSketch::estimate(std::uint32_t id) const {
    std::array<std::int64_t,31> values{};
    for (std::size_t r=0;r<depth_;r++)
        values[r] = counters_[r*width_+hash_bucket(r,id)] * hash_sign(r,id);
    // Hoare-style in-place selection with a median-of-three pivot.
    // The 2005 experiment specifies median-of-three median selection rather
    // than the C++ library's implementation-defined nth_element algorithm.
    std::size_t lo=0, hi=depth_-1, k=depth_/2;
    while(lo<hi) {
        const auto a=values[lo],b=values[(lo+hi)/2],c=values[hi];
        const auto pivot=std::max(std::min(a,b),std::min(std::max(a,b),c));
        std::size_t i=lo,j=hi;
        while(true) {
            while(values[i]<pivot) ++i;
            while(values[j]>pivot) --j;
            if(i>=j) break;
            std::swap(values[i],values[j]);
            ++i; if(j>0)--j;
        }
        if(k<=j) hi=j;
        else lo=j+1;
    }
    return values[k];
}
void CountSketch::attach(Group* g, Node* n) {
    n->group = g;
    if (!g->head) { g->head=n; n->prev=n->next=n; return; }
    Node* h=g->head;
    n->next=h; n->prev=h->prev;
    h->prev->next=n; h->prev=n;
}
void CountSketch::detach(Group* g, Node* n) {
    if (n->next==n) g->head=nullptr;
    else {
        n->prev->next=n->next;
        n->next->prev=n->prev;
        if (g->head==n) g->head=n->next;
    }
    n->group=nullptr; n->next=n->prev=nullptr;
}
void CountSketch::remove_empty(Group* g) {
    if (g->head) throw std::logic_error("nonempty CS group deletion");
    if (g->prev) g->prev->next=g->next; else smallest_=g->next;
    if (g->next) g->next->prev=g->prev; else largest_=g->prev;
    g->prev=g->next=nullptr;
    free_groups_.push_back(g);
}
CountSketch::Group* CountSketch::add_group_after(Group* before, std::int64_t count) {
    if (free_groups_.empty()) throw std::logic_error("CS group pool exhausted");
    Group* g=free_groups_.back(); free_groups_.pop_back();
    g->count=count; g->head=nullptr; g->prev=before;
    if (before) {
        g->next=before->next;
        before->next=g;
    } else {
        g->next=smallest_;
        smallest_=g;
    }
    if (g->next) g->next->prev=g; else largest_=g;
    if (!smallest_) smallest_=g;
    return g;
}
// New candidate can arrive with an arbitrary signed-hash estimate. Unlike
// normal hits, placement may require searching groups (O(distinct counts)).
// The paper only claims constant-time updates for *monitored* candidates.
void CountSketch::insert_ranked(Node* n, std::int64_t count) {
    Group* p=nullptr;
    Group* cur=smallest_;
    while(cur && cur->count < count) { p=cur; cur=cur->next; }
    if (cur && cur->count==count) attach(cur,n);
    else attach(add_group_after(p,count),n);
    n->estimate=count;
}
void CountSketch::increment(Node* n) {
    Group* old=n->group;
    if (n->estimate == std::numeric_limits<std::int64_t>::max())
        throw std::overflow_error("CS candidate counter overflow");
    const std::int64_t v=n->estimate+1;
    Group* next=old->next;
    if (next && next->count==v) {
        detach(old,n); attach(next,n);
        if (!old->head) remove_empty(old);
    } else if (old->head->next==old->head) {
        old->count=v;
    } else {
        Group* g=add_group_after(old,v);
        detach(old,n); attach(g,n);
    }
    n->estimate=v;
}
void CountSketch::update(std::uint32_t id) {
    if (processed_>=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        throw std::overflow_error("CS stream too long");
    for (std::size_t r=0;r<depth_;r++)
        counters_[r*width_+hash_bucket(r,id)] += hash_sign(r,id);
    ++processed_;
    const auto it=index_.find(id);
    if (it!=index_.end()) { increment(it->second); return; }
    const std::int64_t estimate_new=estimate(id);
    if (size_<capacity_) {
        Node* n=&nodes_[size_++];
        n->element=id;
        index_.emplace(id,n);
        insert_ranked(n,estimate_new);
    } else {
        Node* n=smallest_->head;
        if (estimate_new < n->estimate ||
           (estimate_new==n->estimate && id>=n->element)) return;
        index_.erase(n->element);
        Group* old=n->group;
        detach(old,n);
        if (!old->head) remove_empty(old);
        n->element=id;
        index_.emplace(id,n);
        insert_ranked(n,estimate_new);
    }
}
std::vector<CountSketch::Candidate> CountSketch::query_top_k(std::size_t k) const {
    if (!k || k>capacity_) throw std::invalid_argument("invalid CS query_top_k k");
    std::vector<Candidate> out;
    out.reserve(std::min(k,size_));
    for (Group* g=largest_;g && out.size()<k;g=g->prev) {
        Node* p=g->head;
        if (!p) throw std::logic_error("empty CS group");
        do { out.push_back({p->element,p->estimate}); p=p->next; }
        while (p!=g->head && out.size()<k);
    }
    std::sort(out.begin(),out.end(),[](const Candidate&a,const Candidate&b){
        if (a.estimated_count!=b.estimated_count)
            return a.estimated_count>b.estimated_count;
        return a.element<b.element;
    });
    if(out.size()>k) out.resize(k);
    return out;
}
std::size_t CountSketch::allocated_bytes() const {
    return sizeof(*this)+counters_.capacity()*sizeof(std::int64_t)
        +hashes_.capacity()*sizeof(HashRow)+nodes_.capacity()*sizeof(Node)
        +groups_.capacity()*sizeof(Group)
        +free_groups_.capacity()*sizeof(Group*)
        +index_.bucket_count()*sizeof(void*)
        +index_.size()*(sizeof(std::pair<const std::uint32_t,Node*>)+sizeof(void*));
}
bool CountSketch::validate() const {
    if(size_>capacity_ || index_.size()!=size_ || (size_==0)!=(smallest_==nullptr))return false;
    std::unordered_set<const Group*> seen_groups;
    std::unordered_set<const Node*> seen_nodes;
    const Group* prev=nullptr;
    for (const Group* g=smallest_;g;g=g->next) {
        if(!seen_groups.insert(g).second || g->prev!=prev || !g->head ||
           (prev && prev->count>=g->count)) return false;
        const Node* n=g->head;
        do {
            if (!n || n->group!=g || n->estimate!=g->count || !n->prev || !n->next ||
                n->next->prev!=n || n->prev->next!=n || !seen_nodes.insert(n).second) return false;
            auto it=index_.find(n->element);
            if(it==index_.end() || it->second!=n) return false;
            n=n->next;
        } while(n!=g->head);
        prev=g;
    }
    return prev==largest_ && seen_nodes.size()==size_ &&
           (capacity_-seen_groups.size())==free_groups_.size();
}
} // namespace space_saving
