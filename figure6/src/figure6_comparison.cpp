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

using Clock = std::chrono::steady_clock;
using space_saving::CountSketch;
using space_saving::SpaceSaving;

namespace {

struct Options {
    std::size_t events = 10000000, alphabet = 10000, topk = 100;
    std::size_t counters = 1000, cs_width = 8192, cs_depth = 7, repeats = 1;
    std::uint64_t seed = 2026;
    std::vector<double> alphas{0, 0.5, 1, 1.5, 2, 2.5, 3};
    std::string output = "results/figure6_comparison.csv";
    bool validate = false;
};

std::size_t positive(const std::string& s) {
    std::size_t end = 0;
    std::uint64_t val = std::stoull(s, &end);
    if (end != s.size() || val == 0 || val > std::numeric_limits<std::size_t>::max())
        throw std::invalid_argument("expected positive integer: " + s);
    return static_cast<std::size_t>(val);
}

Options parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string key = argv[i];
        if (key == "--help" || key == "-h") {
            std::cout << R"(Figure 6 Space-Saving vs CountSketch (same data stream)
  --events N                 default 10000000 (paper Section 5)
  --alphabet A               default 10000 (project choice)
  --topk K                   default 100 (paper Figure 6)
  --counters M               Space-Saving fixed counters (default 1000)
  --cs-width W               CountSketch width power-of-2 (default 8192)
  --cs-depth D               CountSketch odd rows (default 7)
  --alphas CSV               default 0,0.5,1,1.5,2,2.5,3
  --seed N                   default 2026
  --repeats R                default 1
  --validate                 check invariants and Space-Saving frequency bounds
  --output PATH              default results/figure6_comparison.csv

Times exclude generator, Ground Truth and plotting. Counts/Sketch width are
project-specific, not asserted to match the paper's unpublished parameters.
)";
            std::exit(0);
        }
        if (key == "--validate") { o.validate = true; continue; }
        if (i + 1 == argc) throw std::invalid_argument("missing value for " + key);
        std::string val = argv[++i];
        if (key == "--events") o.events = positive(val);
        else if (key == "--alphabet") o.alphabet = positive(val);
        else if (key == "--topk") o.topk = positive(val);
        else if (key == "--counters") o.counters = positive(val);
        else if (key == "--cs-width") o.cs_width = positive(val);
        else if (key == "--cs-depth") o.cs_depth = positive(val);
        else if (key == "--repeats") o.repeats = positive(val);
        else if (key == "--seed") o.seed = std::stoull(val);
        else if (key == "--output") o.output = val;
        else if (key == "--alphas") {
            o.alphas.clear();
            std::stringstream ss(val); std::string token;
            while (std::getline(ss, token, ',')) {
                std::size_t end = 0;
                const double alpha = std::stod(token, &end);
                if (end != token.size() || !std::isfinite(alpha) || alpha < 0)
                    throw std::invalid_argument("invalid alpha: " + token);
                o.alphas.push_back(alpha);
            }
            if (o.alphas.empty()) throw std::invalid_argument("empty alpha list");
        } else throw std::invalid_argument("unknown option: " + key);
    }
    if (o.alphabet > std::numeric_limits<std::uint32_t>::max() - 1 || o.alphabet < o.topk ||
        o.events < o.topk || o.counters < o.topk)
        throw std::invalid_argument("require topk <= alphabet/events/counters and alphabet <= uint32 max");
    if (o.cs_width < 2 || (o.cs_width & (o.cs_width - 1)))
        throw std::invalid_argument("cs-width must be a power of 2 >= 2");
    if (o.cs_depth > 31 || o.cs_depth == 0 || !(o.cs_depth & 1))
        throw std::invalid_argument("cs-depth must be odd, 1..31");
    return o;
}

// Same Vose alias method as the original single-algorithm experiment.
class ZipfSampler {
public:
    ZipfSampler(std::size_t alphabet, double alpha) {
        prob_.resize(alphabet);
        alias_.resize(alphabet);
        std::vector<long double> weights(alphabet);
        long double sum = 0;
        for (std::size_t i = 0; i < alphabet; ++i) {
            weights[i] = 1.0L / std::pow(static_cast<long double>(i + 1), alpha);
            sum += weights[i];
        }
        std::vector<std::size_t> small, large;
        small.reserve(alphabet); large.reserve(alphabet);
        std::vector<long double> scaled(alphabet);
        for (std::size_t i = 0; i < alphabet; ++i) {
            scaled[i] = weights[i] * alphabet / sum;
            if (scaled[i] < 1) small.push_back(i);
            else large.push_back(i);
        }
        while (!small.empty() && !large.empty()) {
            std::size_t s = small.back(); small.pop_back();
            std::size_t l = large.back(); large.pop_back();
            prob_[s] = static_cast<double>(scaled[s]);
            alias_[s] = static_cast<std::uint32_t>(l);
            scaled[l] = scaled[l] + scaled[s] - 1;
            if (scaled[l] < 1) small.push_back(l);
            else large.push_back(l);
        }
        for (std::size_t i : large) { prob_[i] = 1.0; alias_[i] = static_cast<std::uint32_t>(i); }
        for (std::size_t i : small) { prob_[i] = 1.0; alias_[i] = static_cast<std::uint32_t>(i); }
    }
    std::uint32_t sample(std::mt19937_64& rng) const {
        std::size_t i = std::uniform_int_distribution<std::size_t>(0, prob_.size() - 1)(rng);
        double u = std::generate_canonical<double, 53>(rng);
        return static_cast<std::uint32_t>((u < prob_[i] ? i : alias_[i]) + 1);
    }
private:
    std::vector<double> prob_;
    std::vector<std::uint32_t> alias_;
};

std::unordered_set<std::uint32_t> exact_top_k(const std::vector<std::uint64_t>& truth, std::size_t k) {
    std::vector<std::uint32_t> ids;
    ids.reserve(truth.size());
    for (std::uint32_t i = 1; i < truth.size(); ++i) if (truth[i]) ids.push_back(i);
    std::sort(ids.begin(), ids.end(), [&](std::uint32_t a, std::uint32_t b) {
        if (truth[a] != truth[b]) return truth[a] > truth[b];
        return a < b;
    });
    if (ids.size() > k) ids.resize(k);
    return {ids.begin(), ids.end()};
}

struct Result {
    double update_ms = 0, query_ms = 0, precision = 0, recall = 0;
    std::size_t memory_bytes = 0, returned = 0, overlap = 0, monitored = 0;
};

template <typename Candidates>
void score(Result& r, const Candidates& candidates,
           const std::unordered_set<std::uint32_t>& correct) {
    r.returned = candidates.size();
    for (auto& c : candidates) if (correct.count(c.element)) ++r.overlap;
    r.precision = r.returned ? static_cast<double>(r.overlap) / r.returned : 0;
    r.recall = correct.empty() ? 0 : static_cast<double>(r.overlap) / correct.size();
}

Result run_space_saving(const std::vector<std::uint32_t>& data,
                        const std::vector<std::uint64_t>& truth,
                        const std::unordered_set<std::uint32_t>& top,
                        const Options& o) {
    SpaceSaving a(o.counters);
    auto t0 = Clock::now();
    for (auto id : data) a.update(id);
    auto t1 = Clock::now();
    auto candidates = a.query_top_k(o.topk).elements;
    auto t2 = Clock::now();
    if (o.validate) {
        if (!a.validate()) throw std::runtime_error("Space-Saving invariant failed");
        for (const auto& x : a.monitored_elements()) {
            if (x.lower_bound() > truth[x.element] || truth[x.element] > x.estimated_count)
                throw std::runtime_error("Space-Saving frequency bound violated");
        }
    }
    Result r;
    r.update_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    r.query_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
    r.memory_bytes = a.allocated_bytes();
    r.monitored = a.monitored_count();
    score(r, candidates, top);
    return r;
}

Result run_count_sketch(const std::vector<std::uint32_t>& data,
                        const std::unordered_set<std::uint32_t>& top,
                        const Options& o, std::uint64_t seed) {
    CountSketch a(o.cs_width, o.cs_depth, o.topk, seed);
    auto t0 = Clock::now();
    for (auto id : data) a.update(id);
    auto t1 = Clock::now();
    auto candidates = a.query_top_k(o.topk);
    auto t2 = Clock::now();
    if (o.validate && !a.validate()) throw std::runtime_error("CountSketch heap invariant failed");
    Result r;
    r.update_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    r.query_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
    r.memory_bytes = a.allocated_bytes();
    r.monitored = a.candidates();
    score(r, candidates, top);
    return r;
}

void output_row(std::ofstream& f, double alpha, std::size_t repeat,
                std::uint64_t seed, const Options& o, const std::string& algorithm,
                const Result& r) {
    f << alpha << ',' << repeat << ',' << seed << ',' << o.events << ',' << o.alphabet << ','
      << o.topk << ',' << algorithm << ',' << o.counters << ',' << o.cs_width << ','
      << o.cs_depth << ',' << o.topk << ',' << r.update_ms << ',' << r.query_ms << ','
      << r.update_ms + r.query_ms << ',' << r.precision << ',' << r.recall << ','
      << r.memory_bytes << ',' << r.overlap << ',' << r.returned << ',' << r.monitored << '\n';
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options o = parse(argc, argv);
        std::ofstream csv(o.output);
        if (!csv) throw std::runtime_error("cannot open output: " + o.output);
        csv << "alpha,repeat,seed,events,alphabet,topk,algorithm,space_saving_counters,countsketch_width,"
               "countsketch_depth,countsketch_candidates,update_ms,query_ms,runtime_ms,precision,recall,"
               "memory_bytes,overlap,returned,monitored\n";
        csv << std::fixed << std::setprecision(8);
        for (std::size_t ai = 0; ai < o.alphas.size(); ++ai) {
            const double alpha = o.alphas[ai];
            ZipfSampler sampler(o.alphabet, alpha);
            for (std::size_t rep = 0; rep < o.repeats; ++rep) {
                const std::uint64_t seed = o.seed + ai * 1000003ULL + rep * 1009ULL;
                std::mt19937_64 rng(seed);
                std::vector<std::uint32_t> data(o.events);
                std::vector<std::uint64_t> truth(o.alphabet + 1, 0);
                for (auto& id : data) {
                    id = sampler.sample(rng);
                    ++truth[id];
                }
                const auto top = exact_top_k(truth, o.topk);
                if (top.size() != o.topk)
                    throw std::runtime_error("too few distinct IDs for top-k; increase N");
                const Result ss = run_space_saving(data, truth, top, o);
                output_row(csv, alpha, rep, seed, o, "Space-Saving", ss);
                const Result cs = run_count_sketch(data, top, o, seed ^ 0xA72EA2E45B65959DULL);
                output_row(csv, alpha, rep, seed, o, "CountSketch", cs);
                csv.flush();
                std::cout << "alpha=" << alpha << " rep=" << rep
                          << " Space-Saving P/R=" << ss.precision << '/' << ss.recall
                          << " t=" << ss.update_ms + ss.query_ms << "ms mem=" << ss.memory_bytes
                          << "B | CountSketch P/R=" << cs.precision << '/' << cs.recall
                          << " t=" << cs.update_ms + cs.query_ms << "ms mem=" << cs.memory_bytes << "B\n";
            }
        }
        std::cout << "Saved " << o.output << '\n';
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
}
