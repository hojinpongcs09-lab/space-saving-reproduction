#include "space_saving.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

using space_saving::SpaceSaving;
using Clock = std::chrono::steady_clock;

namespace {

struct Options {
    std::size_t events = 10000000;             // Section 5: 10^7 events
    std::size_t alphabet = 10000;             // user-set: paper does not fully pin down this choice
    std::size_t topk = 100;                   // Figure 6: top-100
    std::size_t counters = 1000;              // user-set: not an original-paper universal constant
    std::size_t repeats = 1;
    std::uint64_t seed = 2026;
    std::vector<double> alphas{0, 0.5, 1, 1.5, 2, 2.5, 3};
    std::vector<std::size_t> budgets{100, 200, 400, 800, 1600, 3200, 6400, 10000};
    std::string mode = "fixed";               // calibrate is an OFFLINE, oracle-assisted budget sweep
    std::string output = "results/figure6_space_saving.csv";
    bool validate = false;
    bool require_guarantee = false;
};

std::size_t to_size(const std::string& value, const std::string& name) {
    std::size_t pos = 0;
    unsigned long long v = std::stoull(value, &pos);
    if (pos != value.size() || v == 0 || v > std::numeric_limits<std::size_t>::max())
        throw std::invalid_argument("invalid positive integer for " + name + ": " + value);
    return static_cast<std::size_t>(v);
}

std::vector<std::size_t> parse_budgets(const std::string& value) {
    std::vector<std::size_t> result;
    std::stringstream s(value);
    std::string token;
    while (std::getline(s, token, ',')) result.push_back(to_size(token, "budgets"));
    if (result.empty()) throw std::invalid_argument("budgets must not be empty");
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

std::vector<double> parse_alphas(const std::string& value) {
    std::vector<double> result;
    std::stringstream s(value);
    std::string token;
    while (std::getline(s, token, ',')) {
        std::size_t pos = 0;
        double x = std::stod(token, &pos);
        if (pos != token.size() || !std::isfinite(x) || x < 0)
            throw std::invalid_argument("invalid alpha: " + token);
        result.push_back(x);
    }
    if (result.empty()) throw std::invalid_argument("alpha list must not be empty");
    return result;
}

void usage() {
    std::cout << R"(Space-Saving Figure 6 reproduction (Space-Saving series ONLY)
Usage: ./build/figure6_experiment [options]
  --events N             Number of events (paper: 10000000; default: 10000000)
  --alphabet A           Number of distinct possible IDs (default: 10000)
  --topk K               Top-k size (paper: 100; default: 100)
  --counters M           Number of monitored counters in fixed mode (default: 1000)
  --alphas CSV           Alpha values (default: 0,0.5,1,1.5,2,2.5,3)
  --seed S               Base PRNG seed (default: 2026)
  --repeats R            Independent seeds per alpha (default: 1)
  --mode fixed|calibrate fixed=M, or offline choose smallest successful budget
  --budgets CSV          Candidate M values for calibration (default: 100,...,10000)
  --require-guarantee    In calibration require paper's top-k guarantee as well
  --validate             Deep consistency check after each run (outside timing)
  --output PATH          Output CSV (default: results/figure6_space_saving.csv)
  --help                 Show this help

In calibrate mode, ground truth is used for OFFLINE counter tuning. This is
not part of the original streaming algorithm, and not the same experiment as a
fixed-memory run. Both modes exclude data generation and ground-truth timing.
)";
}

Options parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--help" || a == "-h") { usage(); std::exit(0); }
        if (a == "--validate") { o.validate = true; continue; }
        if (a == "--require-guarantee") { o.require_guarantee = true; continue; }
        if (i + 1 >= argc) throw std::invalid_argument("missing value after " + a);
        const std::string v = argv[++i];
        if (a == "--events") o.events = to_size(v, a);
        else if (a == "--alphabet") o.alphabet = to_size(v, a);
        else if (a == "--topk") o.topk = to_size(v, a);
        else if (a == "--counters") o.counters = to_size(v, a);
        else if (a == "--repeats") o.repeats = to_size(v, a);
        else if (a == "--seed") o.seed = static_cast<std::uint64_t>(std::stoull(v));
        else if (a == "--alphas") o.alphas = parse_alphas(v);
        else if (a == "--budgets") o.budgets = parse_budgets(v);
        else if (a == "--mode") o.mode = v;
        else if (a == "--output") o.output = v;
        else throw std::invalid_argument("unknown argument: " + a);
    }
    if (o.mode != "fixed" && o.mode != "calibrate") throw std::invalid_argument("invalid mode");
    if (o.alphabet > std::numeric_limits<std::uint32_t>::max() - 1)
        throw std::invalid_argument("alphabet exceeds uint32 ID range");
    if (o.topk > o.alphabet || o.topk > o.events)
        throw std::invalid_argument("topk must be <= alphabet and <= events");
    if (o.mode == "fixed" && o.counters < o.topk)
        throw std::invalid_argument("counters must be >= topk");
    if (o.mode == "calibrate") {
        o.budgets.erase(std::remove_if(o.budgets.begin(), o.budgets.end(),
                       [&o](std::size_t m){ return m < o.topk || m > o.alphabet; }), o.budgets.end());
        if (o.budgets.empty()) throw std::invalid_argument("no budgets between topk and alphabet");
        if (o.budgets.back() < o.alphabet) o.budgets.push_back(o.alphabet);
    }
    return o;
}

// Vose alias method: O(alphabet) build, O(1) sample for Zipf(alpha).
// Alpha=0 corresponds to a uniform categorical distribution.
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
        // uniform_int_distribution / generate_canonical are reproducible with
        // a given standard library; platform/library differences are possible.
        std::size_t i = std::uniform_int_distribution<std::size_t>(0, prob_.size() - 1)(rng);
        double u = std::generate_canonical<double, 53>(rng);
        return static_cast<std::uint32_t>((u < prob_[i] ? i : alias_[i]) + 1);
    }
private:
    std::vector<double> prob_;
    std::vector<std::uint32_t> alias_;
};

struct Metrics {
    std::size_t m = 0, overlap = 0, returned = 0, distinct = 0;
    double update_ms = 0, query_ms = 0, precision = 0, recall = 0;
    std::size_t memory_bytes = 0;
    bool guaranteed = false, strict = false, order = false;
};

Metrics evaluate(const std::vector<std::uint32_t>& events,
                 const std::vector<std::uint64_t>& truth,
                 const std::unordered_set<std::uint32_t>& true_top,
                 std::size_t m, std::size_t k, bool validate) {
    SpaceSaving ss(m);
    auto begin = Clock::now();
    for (auto id : events) ss.update(id);
    auto updated = Clock::now();
    auto result = ss.query_top_k(k);
    auto queried = Clock::now();
    if (validate && !ss.validate()) throw std::runtime_error("internal Stream-Summary invariant failed");
    if (validate) {
        for (const auto& e : ss.monitored_elements()) {
            if (e.lower_bound() > truth[e.element] || truth[e.element] > e.estimated_count)
                throw std::runtime_error("frequency bound violated for element " + std::to_string(e.element));
        }
    }
    Metrics out;
    out.m = m;
    out.update_ms = std::chrono::duration<double, std::milli>(updated - begin).count();
    out.query_ms = std::chrono::duration<double, std::milli>(queried - updated).count();
    out.memory_bytes = ss.allocated_bytes();
    out.returned = result.elements.size();
    out.distinct = ss.monitored_count();
    out.guaranteed = result.guaranteed_topk;
    out.strict = result.guaranteed_topk_strict;
    out.order = result.guaranteed_order;
    for (const auto& e : result.elements)
        if (true_top.count(e.element)) ++out.overlap;
    out.precision = out.returned ? static_cast<double>(out.overlap) / out.returned : 0;
    out.recall = true_top.empty() ? 0 : static_cast<double>(out.overlap) / true_top.size();
    return out;
}

std::unordered_set<std::uint32_t> exact_top_k(const std::vector<std::uint64_t>& truth, std::size_t k) {
    std::vector<std::uint32_t> ids;
    ids.reserve(truth.size());
    for (std::uint32_t i = 1; i < truth.size(); ++i) if (truth[i]) ids.push_back(i);
    std::sort(ids.begin(), ids.end(), [&](std::uint32_t a, std::uint32_t b) {
        if (truth[a] != truth[b]) return truth[a] > truth[b];
        return a < b; // deterministic rule for frequency ties
    });
    if (ids.size() > k) ids.resize(k);
    return {ids.begin(), ids.end()};
}

} // namespace

int main(int argc, char** argv) {
    try {
        Options opt = parse(argc, argv);
        std::ofstream csv(opt.output);
        if (!csv) throw std::runtime_error("cannot write CSV: " + opt.output + " (create directory first)");
        csv << "alpha,repeat,seed,events,alphabet,topk,counters,mode,update_ms,query_ms,runtime_ms,precision,recall,"
               "memory_bytes,overlap,returned,monitored,guaranteed_topk,guaranteed_order,guaranteed_topk_strict\n";
        csv << std::fixed << std::setprecision(8);
        for (std::size_t ai = 0; ai < opt.alphas.size(); ++ai) {
            const double alpha = opt.alphas[ai];
            ZipfSampler sampler(opt.alphabet, alpha);
            for (std::size_t rep = 0; rep < opt.repeats; ++rep) {
                const std::uint64_t seed = opt.seed + ai * 1000003ULL + rep * 1009ULL;
                std::mt19937_64 rng(seed);
                std::vector<std::uint32_t> events(opt.events);
                std::vector<std::uint64_t> truth(opt.alphabet + 1, 0);
                for (auto& id : events) {
                    id = sampler.sample(rng);
                    ++truth[id];
                }
                auto exact = exact_top_k(truth, opt.topk);
                if (exact.size() != opt.topk)
                    throw std::runtime_error("fewer than topk observed distinct elements; increase events");
                Metrics selected;
                if (opt.mode == "fixed") {
                    selected = evaluate(events, truth, exact, opt.counters, opt.topk, opt.validate);
                } else {
                    // The selection uses the exact ground truth after the stream is known.
                    // It is a reproducible, ORACLE-assisted offline sweep, not a streaming adaptation.
                    for (auto m : opt.budgets) {
                        selected = evaluate(events, truth, exact, m, opt.topk, opt.validate);
                        if (selected.precision == 1.0 && selected.recall == 1.0 &&
                            (!opt.require_guarantee || selected.guaranteed)) break;
                    }
                }
                csv << alpha << ',' << rep << ',' << seed << ',' << opt.events << ',' << opt.alphabet << ','
                    << opt.topk << ',' << selected.m << ',' << opt.mode << ',' << selected.update_ms << ','
                    << selected.query_ms << ',' << (selected.update_ms + selected.query_ms) << ','
                    << selected.precision << ',' << selected.recall << ',' << selected.memory_bytes << ','
                    << selected.overlap << ',' << selected.returned << ',' << selected.distinct << ','
                    << selected.guaranteed << ',' << selected.order << ',' << selected.strict << '\n';
                csv.flush();
                std::cout << "alpha=" << alpha << " rep=" << rep << " M=" << selected.m
                          << " precision=" << selected.precision << " recall=" << selected.recall
                          << " runtime=" << std::setprecision(2) << selected.update_ms + selected.query_ms
                          << "ms memory=" << selected.memory_bytes << "B"
                          << " guarantee=" << selected.guaranteed << std::setprecision(8) << '\n';
            }
        }
        std::cout << "Wrote " << opt.output << '\n';
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
}
