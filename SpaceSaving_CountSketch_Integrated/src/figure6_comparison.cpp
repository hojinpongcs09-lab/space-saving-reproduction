#include "topk_metrics.hpp"
#include "count_sketch.hpp"
#include "space_saving.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>
#ifdef __GLIBC__
#include <malloc.h>
#endif

using Clock=std::chrono::steady_clock;
using space_saving::CountSketch;
using space_saving::SpaceSaving;
namespace {
struct Options {
    std::size_t events=10000000, alphabet=10000, topk=100;
    std::size_t counters=1000, cs_width=8192, cs_depth=7, repeats=1;
    double cs_delta=0.01;
    std::string ss_budget="theory", stream_order="iid";
    std::uint64_t seed=2026;
    std::vector<double> alphas{0,.5,1,1.5,2,2.5,3};
    std::string output="results/figure6_comparison.csv";
    bool validate=false;
    bool ss_budget_comparison=false;
    bool ss_budget_sweep=false;
};
std::size_t positive(const std::string& s) {
    std::size_t end=0;
    auto v=std::stoull(s,&end);
    if(end!=s.size()||v==0||v>std::numeric_limits<std::size_t>::max())
        throw std::invalid_argument("expected positive integer: "+s);
    return static_cast<std::size_t>(v);
}
Options parse(int argc,char** argv) {
    Options o;
    for(int i=1;i<argc;i++) {
        std::string key=argv[i];
        if(key=="--help"||key=="-h") {
            std::cout << R"(Method-aligned Figure 6 (ICDT 2005, not original dataset/code)
  --events N           default 10000000    (conference paper)
  --alphabet A         default 10000       (project assumption)
  --topk K             default 100         (conference paper)
  --ss-budget MODE     theory|fixed        (default theory)
  --counters M         fixed-mode count (default 1000)
  --cs-width W         CountSketch width, power of two (default 8192)
  --cs-depth D         odd rows (default 7)
  --cs-delta P         recorded target failure probability (default 0.01)
  --stream-order MODE  iid|sorted|shuffled (default iid)
  --alphas CSV         default 0,0.5,1,1.5,2,2.5,3
  --seed N             default 2026
  --repeats R          default 1
  --validate           deep data-structure / error checks
  --ss-budget-sweep       compare theory and fixed M=500,1000,2000,4000,8000,10000
  --ss-budget-comparison  run paired Space-Saving theory-policy vs fixed-M only
  --output PATH        CSV destination

Space-Saving 'theory': calculate a counter bound BEFORE sampling by
  m > (H_A(alpha)+1)*(k/alpha)^(1/alpha)*(k+1) + H_A(alpha)
for alpha>=1 (UCSB technical report, Theorem 7 proof), capped at alphabet A.
For alpha<1 select full alphabet, since the paper's alpha>=1 bound does not apply.
The report is for noiseless Zipf rank frequencies; sampled streams are noisy.
CountSketch: signed-hash median + fixed-k Stream-Summary candidate buckets.
Width/depth and alphabet/seed are OUR choices; delta=0.01 from paper.
Times omit dataset generation and ground truth. Memory reports owned estimate
and platform malloc live heap delta (not peak RSS).
)";
            std::exit(0);
        }
        if(key=="--validate") {o.validate=true; continue;}
        if(key=="--ss-budget-sweep") {o.ss_budget_sweep=true; continue;}
        if(key=="--ss-budget-comparison") {o.ss_budget_comparison=true; continue;}
        if(i+1==argc) throw std::invalid_argument("missing value for "+key);
        const std::string v=argv[++i];
        if(key=="--events")o.events=positive(v);
        else if(key=="--alphabet")o.alphabet=positive(v);
        else if(key=="--topk")o.topk=positive(v);
        else if(key=="--counters")o.counters=positive(v);
        else if(key=="--cs-width")o.cs_width=positive(v);
        else if(key=="--cs-depth")o.cs_depth=positive(v);
        else if(key=="--repeats")o.repeats=positive(v);
        else if(key=="--cs-delta")o.cs_delta=std::stod(v);
        else if(key=="--seed")o.seed=std::stoull(v);
        else if(key=="--output")o.output=v;
        else if(key=="--ss-budget")o.ss_budget=v;
        else if(key=="--stream-order")o.stream_order=v;
        else if(key=="--alphas") {
            o.alphas.clear();std::stringstream ss(v);std::string tok;
            while(std::getline(ss,tok,',')) {
                std::size_t end=0;double a=std::stod(tok,&end);
                if(end!=tok.size()||!std::isfinite(a)||a<0)throw std::invalid_argument("bad alpha");
                o.alphas.push_back(a);
            }
        } else throw std::invalid_argument("unknown option "+key);
    }
    if(o.ss_budget!="fixed"&&o.ss_budget!="theory")throw std::invalid_argument("ss-budget: theory|fixed");
    if(o.stream_order!="iid"&&o.stream_order!="sorted"&&o.stream_order!="shuffled")
        throw std::invalid_argument("stream-order: iid|sorted|shuffled");
    if(o.alphas.empty()||o.alphabet<o.topk||o.events<o.topk||
        o.alphabet>=std::numeric_limits<std::uint32_t>::max())
        throw std::invalid_argument("require nonempty alpha and topk <= alphabet/events < UINT32_MAX");
    if((o.ss_budget=="fixed"||o.ss_budget_comparison)&&o.counters<o.topk)
        throw std::invalid_argument("fixed counter budget must be >=topk");
    if(o.cs_width<2||(o.cs_width&(o.cs_width-1)))throw std::invalid_argument("width power of 2");
    if(!o.cs_depth||(o.cs_depth%2)==0||o.cs_depth>31)throw std::invalid_argument("depth odd 1..31");
    if(!(o.cs_delta>0&&o.cs_delta<1))throw std::invalid_argument("delta in (0,1)");
    return o;
}

// The *normalized* Zipf PMF p(r)=r^{-alpha}/sum_{j=1}^A j^{-alpha},
// sampled IID by Vose's alias table. Elements are labeled by frequency rank.
class ZipfSampler {
public:
    ZipfSampler(std::size_t a,double alpha):prob_(a),alias_(a) {
        std::vector<long double> w(a),scaled(a);
        long double s=0;
        for(std::size_t i=0;i<a;i++) {w[i]=std::pow((long double)(i+1),-alpha);s+=w[i];}
        std::vector<std::size_t> small,large;
        for(std::size_t i=0;i<a;i++){
            scaled[i]=w[i]*a/s;
            if(scaled[i]<1)small.push_back(i);else large.push_back(i);
        }
        while(!small.empty()&&!large.empty()){
            auto lo=small.back();small.pop_back();
            auto hi=large.back();large.pop_back();
            prob_[lo]=(double)scaled[lo];alias_[lo]=(std::uint32_t)hi;
            scaled[hi]+=scaled[lo]-1;
            if(scaled[hi]<1)small.push_back(hi);else large.push_back(hi);
        }
        for(auto i:large){prob_[i]=1;alias_[i]=(std::uint32_t)i;}
        for(auto i:small){prob_[i]=1;alias_[i]=(std::uint32_t)i;}
    }
    std::uint32_t sample(std::mt19937_64& rng)const {
        auto i=std::uniform_int_distribution<std::size_t>(0,prob_.size()-1)(rng);
        auto u=std::generate_canonical<double,53>(rng);
        return (std::uint32_t)((u<prob_[i]?i:alias_[i])+1);
    }
private:
    std::vector<double> prob_;
    std::vector<std::uint32_t> alias_;
};
long double harmonic(std::size_t a,double alpha) {
    long double h=0;
    for(std::size_t i=1;i<=a;i++) h+=std::pow((long double)i,-alpha);
    return h;
}
struct Budget {std::size_t count;double formula_uncapped;std::string origin;};
Budget ss_budget(const Options& o,double alpha) {
    if(o.ss_budget=="fixed")return {o.counters,(double)o.counters,"fixed-project"};
    if(alpha<1)return {o.alphabet,(double)o.alphabet,"full-alphabet-fallback-alpha-lt-1"};
    const long double h=harmonic(o.alphabet,alpha);
    const long double bound=(h+1.0L)*std::pow((long double)o.topk/alpha,1.0L/alpha)*
                         (o.topk+1)+h;
    // Strict m > bound, so next integer strictly above it. Full alphabet is exact.
    const std::size_t m=(bound>=(long double)o.alphabet)?o.alphabet:
        std::max(o.topk,(std::size_t)std::floor(bound)+1);
    return {m,(double)bound,"technical-report-theorem7"};
}
std::unordered_set<std::uint32_t> exact_top_k(const std::vector<std::uint64_t>& truth,std::size_t k){
    std::vector<std::uint32_t> ids;
    for(std::uint32_t i=1;i<truth.size();i++) if(truth[i])ids.push_back(i);
    std::sort(ids.begin(),ids.end(),[&](auto a,auto b){
        if(truth[a]!=truth[b]) return truth[a]>truth[b];
        return a<b;
    });
    if(ids.size()>k) ids.resize(k);
    return {ids.begin(),ids.end()};
}
std::size_t malloc_live_bytes(){
#ifdef __GLIBC__
    const auto mi=mallinfo2();
    return (std::size_t)(mi.uordblks + mi.hblkhd);
#else
    return 0;
#endif
}
struct Result {
    double update_ms=0,query_ms=0,precision=0,recall=0;
    std::size_t memory_bytes=0,allocator_live_delta_bytes=0,returned=0,overlap=0,monitored=0;
    bool guaranteed=false,guaranteed_order=false;
    space_saving::TieMetrics tie;
};
template<class Candidates>
void score(Result& r,const Candidates& c,const std::unordered_set<std::uint32_t>& truth, const std::vector<std::uint64_t>& counts){
    r.returned=c.size();
    for(auto& x:c)if(truth.count(x.element))r.overlap++;
    r.precision=r.returned?(double)r.overlap/r.returned:0;
    r.recall=truth.empty()?0:(double)r.overlap/truth.size();
    std::uint64_t kth=UINT64_MAX;
    for(auto id:truth) kth=std::min(kth,counts[id]);
    std::size_t mandatory=0;
    for(auto id:truth) mandatory+=counts[id]>kth;
    r.tie=space_saving::tie_metrics(c,truth.size(),kth,mandatory,[&](auto id){ return id<counts.size()?counts[id]:0; });
}
Result run_ss(const std::vector<std::uint32_t>& data,
              const std::vector<std::uint64_t>& truth,
              const std::unordered_set<std::uint32_t>& top,
              const Options& o,std::size_t counters) {
    auto before=malloc_live_bytes();
    SpaceSaving ss(counters);
    auto start=Clock::now();
    for(auto id:data)ss.update(id);
    auto end=Clock::now();
    Result r;
    r.allocator_live_delta_bytes=malloc_live_bytes()>before?malloc_live_bytes()-before:0;
    r.memory_bytes=ss.allocated_bytes();r.monitored=ss.monitored_count();
    auto queried=ss.query_top_k(o.topk);
    auto qend=Clock::now();
    r.guaranteed=queried.guaranteed_topk_strict;
    r.guaranteed_order=queried.guaranteed_order;
    r.update_ms=std::chrono::duration<double,std::milli>(end-start).count();
    r.query_ms=std::chrono::duration<double,std::milli>(qend-end).count();
    score(r,queried.elements,top,truth);
    if(o.validate){
        if(!ss.validate())throw std::runtime_error("Space-Saving structure invariant failed");
        for(auto x:ss.monitored_elements())
            if(x.lower_bound()>truth[x.element]||truth[x.element]>x.estimated_count)
                throw std::runtime_error("Space-Saving error bound violated");
        if(r.guaranteed&&r.overlap!=o.topk)
            throw std::runtime_error("claimed strictly guaranteed top-k is wrong");
    }
    return r;
}
Result run_cs(const std::vector<std::uint32_t>& data,
              const std::vector<std::uint64_t>& truth,
              const std::unordered_set<std::uint32_t>& top,
              const Options& o,std::uint64_t seed) {
    auto before=malloc_live_bytes();
    CountSketch cs(o.cs_width,o.cs_depth,o.topk,seed);
    auto start=Clock::now();
    for(auto id:data)cs.update(id);
    auto end=Clock::now();
    Result r;
    r.allocator_live_delta_bytes=malloc_live_bytes()>before?malloc_live_bytes()-before:0;
    r.memory_bytes=cs.allocated_bytes();r.monitored=cs.candidates();
    auto queried=cs.query_top_k(o.topk);
    auto qend=Clock::now();
    r.update_ms=std::chrono::duration<double,std::milli>(end-start).count();
    r.query_ms=std::chrono::duration<double,std::milli>(qend-end).count();
    score(r,queried,top,truth);
    if(o.validate&&!cs.validate()) throw std::runtime_error("CountSketch StreamSummary invariant failed");
    return r;
}
void out(std::ofstream& f,double alpha,std::size_t rep,std::uint64_t seed,
         const Options& o,const Budget& b,std::uint64_t observed_gap,
         const std::string& algorithm,const Result& r){
    f<<alpha<<','<<rep<<','<<seed<<','<<o.events<<','<<o.alphabet<<','<<o.topk<<','
     <<o.stream_order<<','<<algorithm<<','<<o.ss_budget<<','<<b.origin<<','
     <<b.count<<','<<b.formula_uncapped<<','<<o.cs_width<<','<<o.cs_depth<<','
     <<o.cs_delta<<','<<o.topk<<','<<r.update_ms<<','<<r.query_ms<<','
     <<r.update_ms+r.query_ms<<','<<r.precision<<','<<r.recall<<','
     <<r.memory_bytes<<','<<r.allocator_live_delta_bytes<<','<<r.overlap<<','
     <<r.returned<<','<<r.monitored<<','<<(r.guaranteed?1:0)<<','
     <<(r.guaranteed_order?1:0)<<','<<observed_gap<<','<<r.tie.precision<<','<<r.tie.recall<<','<<r.tie.valid<<'\n';
}
} // namespace

int main(int argc,char** argv) {
    try {
        const Options o=parse(argc,argv);
        std::ofstream f(o.output);
        if(!f)throw std::runtime_error("cannot open output: "+o.output);
        f<<"alpha,repeat,seed,events,alphabet,topk,stream_order,algorithm,ss_budget_mode,ss_budget_source,"
            "space_saving_counters,space_saving_bound_uncapped,countsketch_width,countsketch_depth,"
            "countsketch_delta,countsketch_candidates,update_ms,query_ms,runtime_ms,precision,recall,"
            "memory_bytes,allocator_live_delta_bytes,overlap,returned,monitored,guaranteed_topk_strict,"
            "guaranteed_order,observed_true_gap,tie_precision,tie_recall,topk_set_valid\n";
        f<<std::fixed<<std::setprecision(8);
        for(std::size_t ai=0;ai<o.alphas.size();ai++){
            const double alpha=o.alphas[ai];
            const auto budget=ss_budget(o,alpha);
            ZipfSampler sampler(o.alphabet,alpha);
            for(std::size_t rep=0;rep<o.repeats;rep++){
                const std::uint64_t seed=o.seed+ai*1000003ULL+rep*1009ULL;
                std::mt19937_64 rng(seed);
                std::vector<std::uint32_t> data(o.events);
                std::vector<std::uint64_t> truth(o.alphabet+1,0);
                for(auto& id:data) {id=sampler.sample(rng);truth[id]++;}
                if(o.stream_order=="sorted")std::sort(data.begin(),data.end());
                else if(o.stream_order=="shuffled")std::shuffle(data.begin(),data.end(),rng);
                const auto exact=exact_top_k(truth,o.topk);
                if(exact.size()!=o.topk)throw std::runtime_error("too few distinct samples for topk");
                std::vector<std::uint64_t> counts(truth.begin()+1,truth.end());
                std::nth_element(counts.begin(),counts.begin()+o.topk-1,counts.end(),std::greater<>());
                std::uint64_t kth=counts[o.topk-1];
                if(o.topk<counts.size())
                    std::nth_element(counts.begin(),counts.begin()+o.topk,counts.end(),std::greater<>());
                std::uint64_t k1=(o.topk<counts.size())?counts[o.topk]:0;
                const std::uint64_t gap=kth-k1;
                if(o.ss_budget_sweep) {
                    std::vector<std::size_t> budgets{500,1000,2000,4000,8000,10000};
                    // Use identical data and rotate algorithm order by repeat.
                    budgets.insert(budgets.begin(),0);
                    std::rotate(budgets.begin(),budgets.begin()+rep%budgets.size(),budgets.end());
                    for(auto m:budgets){
                        if(m && (m<o.topk || m>o.alphabet)) continue;
                        Options cfg=o; cfg.ss_budget=m?"fixed":"theory";
                        auto b= m?Budget{m,double(m),"fixed-project"}:ss_budget(cfg,alpha);
                        auto r=run_ss(data,truth,exact,cfg,b.count);
                        out(f,alpha,rep,seed,cfg,b,gap,m?"Space-Saving (fixed-"+std::to_string(m)+")":"Space-Saving (alpha-specific)",r);
                    }
                    f.flush();
                    std::cout<<"sweep alpha="<<alpha<<" repeat="<<rep<<std::endl;
                    continue;
                }
                if(o.ss_budget_comparison) {
                    // Paired Space-Saving-only experiment: both policies see the exact same
                    // generated stream and ground truth. Alternate execution order by repeat
                    // to reduce systematic warm-cache/order bias in runtime measurements.
                    Options fixed_options=o;
                    fixed_options.ss_budget="fixed";
                    const Budget fixed_budget{o.counters,(double)o.counters,"fixed-project"};
                    Result theory_result, fixed_result;
                    if((rep%2)==0) {
                        theory_result=run_ss(data,truth,exact,o,budget.count);
                        fixed_result=run_ss(data,truth,exact,fixed_options,o.counters);
                    } else {
                        fixed_result=run_ss(data,truth,exact,fixed_options,o.counters);
                        theory_result=run_ss(data,truth,exact,o,budget.count);
                    }
                    out(f,alpha,rep,seed,o,budget,gap,"Space-Saving (alpha-specific)",theory_result);
                    out(f,alpha,rep,seed,fixed_options,fixed_budget,gap,
                        "Space-Saving (fixed-"+std::to_string(o.counters)+")",fixed_result);
                    f.flush();
                    std::cout<<"alpha="<<alpha<<" theory_M="<<budget.count
                             <<" fixed_M="<<o.counters<<" true_gap="<<gap
                             <<" | theory P/R="<<theory_result.precision<<"/"<<theory_result.recall
                             <<" t="<<theory_result.update_ms+theory_result.query_ms
                             <<"ms mem="<<theory_result.memory_bytes<<"B"
                             <<" | fixed P/R="<<fixed_result.precision<<"/"<<fixed_result.recall
                             <<" t="<<fixed_result.update_ms+fixed_result.query_ms
                             <<"ms mem="<<fixed_result.memory_bytes<<"B\n";
                    continue;
                }
                auto ss=run_ss(data,truth,exact,o,budget.count);
                out(f,alpha,rep,seed,o,budget,gap,"Space-Saving",ss);
                auto cs=run_cs(data,truth,exact,o,seed^0xA72EA2E45B65959DULL);
                out(f,alpha,rep,seed,o,budget,gap,"CountSketch",cs);
                f.flush();
                std::cout<<"alpha="<<alpha<<" M="<<budget.count<<" source="<<budget.origin
                         <<" true_gap="<<gap
                         <<" | SS P/R="<<ss.precision<<"/"<<ss.recall<<" t="<<ss.update_ms+ss.query_ms
                         <<"ms mem="<<ss.memory_bytes<<"B heap="<<ss.allocator_live_delta_bytes
                         <<"B guarantee="<<ss.guaranteed
                         <<" | CS P/R="<<cs.precision<<"/"<<cs.recall
                         <<" t="<<cs.update_ms+cs.query_ms<<"ms mem="<<cs.memory_bytes
                         <<"B heap="<<cs.allocator_live_delta_bytes<<"B\n";
            }
        }
        std::cout<<"Saved "<<o.output<<"\n";
    } catch(const std::exception& e) {
        std::cerr<<"ERROR: "<<e.what()<<"\n";return 1;
    }
}
