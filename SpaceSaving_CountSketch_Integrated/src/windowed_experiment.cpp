#include "topk_metrics.hpp"
#include <limits>
#include "exact_sliding_window.hpp"
#include "space_saving.hpp"
#include "windowed_space_saving.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using Clock = std::chrono::steady_clock;
using space_saving::BlockWindowedSpaceSaving;
using space_saving::ExactSlidingWindow;
using space_saving::SpaceSaving;

namespace {
struct Options {
    std::size_t events = 2000000;
    std::size_t alphabet = 10000;
    std::size_t window = 100000;
    std::size_t counters = 1600;
    std::size_t topk = 20;
    std::size_t query_interval = 50000;
    std::size_t repeats = 3;
    std::uint64_t seed = 2026;
    std::vector<double> alphas{1.0, 1.5, 2.0};
    std::vector<std::size_t> blocks{2, 4, 8};
    std::string scenarios = "all";
    std::string output = "results/windowed/windowed_comparison.csv";
    bool validate = false;
};

std::size_t positive(const std::string& value) {
    std::size_t end = 0;
    const auto parsed = std::stoull(value, &end);
    if (end != value.size() || parsed == 0)
        throw std::invalid_argument("expected positive integer: " + value);
    return static_cast<std::size_t>(parsed);
}

Options parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "--help" || key == "-h") {
            std::cout << R"(Block-windowed Space-Saving experiment
  --events N             stream length (default 2,000,000)
  --alphabet A           flow ID count (default 10,000; dynamic uses two halves)
  --window W             packet-count window (default 100,000)
  --counters M           total Space-Saving counter budget (default 1,600)
  --topk K               query size (default 20)
  --query-interval Q     common query interval (default 50,000)
  --repeats R            seeds per scenario/alpha (default 3)
  --seed S               base random seed (default 2026)
  --alphas CSV           Zipf alpha values (default 1,1.5,2)
  --scenarios MODE       all|static|shift|gradual|overlap (default all)
  --output PATH          output CSV
  --validate             validate data structures and every frequency bound

Scenarios:
  static: one Zipf distribution over the full alphabet.
  shift:  at shift_at = events/2 + window/2, the active flow-ID half changes.
  gradual: linearly mix old/new halves over one window after shift_at.
  overlap: abrupt shift, retaining the first floor(k/2) ranks' original IDs.

Methods:
  Cumulative-SpaceSaving: one summary over the entire history.
  Naive-Block-Merge: sum only stored per-block counts; missing entries add zero.
  Bound-Aware-Block-Merge: merge conservative per-block lower/upper bounds;
    rank candidates by interval midpoint and certify Top-k membership only
    under strict separation from all excluded candidates and unseen IDs.
  Exact Sliding Window is the evaluation oracle, not an approximate method.

Queries occur at common packet positions divisible by query_interval, after
at least one full window. query_interval must align with every block size.
)";
            std::exit(0);
        }
        if (key == "--validate") { o.validate = true; continue; }
        if (i + 1 >= argc) throw std::invalid_argument("missing value for " + key);
        const std::string value = argv[++i];
        if (key == "--events") o.events = positive(value);
        else if (key == "--alphabet") o.alphabet = positive(value);
        else if (key == "--window") o.window = positive(value);
        else if (key == "--counters") o.counters = positive(value);
        else if (key == "--topk") o.topk = positive(value);
        else if (key == "--query-interval") o.query_interval = positive(value);
        else if (key == "--repeats") o.repeats = positive(value);
        else if (key == "--seed") o.seed = std::stoull(value);
        else if (key == "--output") o.output = value;
        else if (key == "--scenarios") o.scenarios = value;
        else if (key == "--alphas") {
            o.alphas.clear();
            std::stringstream ss(value);
            std::string token;
            while (std::getline(ss, token, ',')) {
                std::size_t end = 0;
                const double alpha = std::stod(token, &end);
                if (end != token.size() || !std::isfinite(alpha) || alpha < 0)
                    throw std::invalid_argument("invalid Zipf alpha: " + token);
                o.alphas.push_back(alpha);
            }
        } else {
            throw std::invalid_argument("unknown option: " + key);
        }
    }

    if (o.scenarios != "all" && o.scenarios != "static" && o.scenarios != "shift" && o.scenarios != "gradual" && o.scenarios != "overlap")
        throw std::invalid_argument("--scenarios must be all, static, shift, gradual, or overlap");
    if (o.alphabet < 2 || o.alphabet % 2 != 0 || o.alphabet >= UINT32_MAX)
        throw std::invalid_argument("alphabet must be even, >=2, and < UINT32_MAX");
    if (o.topk == 0 || o.topk > o.alphabet || o.counters < o.topk)
        throw std::invalid_argument("require 1 <= topk <= alphabet and counters >= topk");
    if (o.window == 0 || o.query_interval == 0 || o.events < o.window)
        throw std::invalid_argument("require events >= window > 0 and query interval > 0");
    if (o.alphas.empty()) throw std::invalid_argument("at least one alpha is required");
    for (auto b : o.blocks) {
        if (o.window % b || o.counters % b)
            throw std::invalid_argument("window and counter budget must be divisible by each B");
        const auto block_size = o.window / b;
        if (o.query_interval % block_size)
            throw std::invalid_argument("query interval must be divisible by every block size");
        if (o.counters / b == 0)
            throw std::invalid_argument("each block must receive at least one counter");
    }
    return o;
}

class ZipfSampler {
public:
    ZipfSampler(std::size_t alphabet, double alpha, std::uint32_t offset)
        : probability_(alphabet), alias_(alphabet), offset_(offset) {
        std::vector<long double> weights(alphabet), scaled(alphabet);
        long double sum = 0;
        for (std::size_t i = 0; i < alphabet; ++i) {
            weights[i] = std::pow(static_cast<long double>(i + 1), -alpha);
            sum += weights[i];
        }
        std::vector<std::size_t> small, large;
        for (std::size_t i = 0; i < alphabet; ++i) {
            scaled[i] = weights[i] * alphabet / sum;
            (scaled[i] < 1 ? small : large).push_back(i);
        }
        while (!small.empty() && !large.empty()) {
            const auto lo = small.back(); small.pop_back();
            const auto hi = large.back(); large.pop_back();
            probability_[lo] = static_cast<double>(scaled[lo]);
            alias_[lo] = static_cast<std::uint32_t>(hi);
            scaled[hi] += scaled[lo] - 1;
            (scaled[hi] < 1 ? small : large).push_back(hi);
        }
        for (auto i : large) { probability_[i] = 1; alias_[i] = static_cast<std::uint32_t>(i); }
        for (auto i : small) { probability_[i] = 1; alias_[i] = static_cast<std::uint32_t>(i); }
    }

    std::uint32_t sample(std::mt19937_64& rng) const {
        const auto i = std::uniform_int_distribution<std::size_t>(0, probability_.size() - 1)(rng);
        const auto u = std::generate_canonical<double, 53>(rng);
        const auto rank = u < probability_[i] ? i : alias_[i];
        return offset_ + static_cast<std::uint32_t>(rank + 1);
    }

private:
    std::vector<double> probability_;
    std::vector<std::uint32_t> alias_;
    std::uint32_t offset_;
};

struct MethodMetrics {
    double update_ms = 0;
    double query_ms = 0;
    double precision_sum = 0;
    double tie_precision_sum=0,tie_recall_sum=0,valid_sum=0;
    double recall_sum = 0;
    double certification_sum = 0;
    double order_certification_sum = 0;
    double bound_width_sum = 0;
    std::uint64_t bound_width_item_count = 0;
    std::uint64_t query_count = 0;
    std::uint64_t candidate_sum = 0;
    std::uint64_t bound_violations = 0;
    std::size_t memory_bytes = 0;
    std::unordered_map<std::uint32_t, std::uint64_t> true_entry_time;
    std::unordered_map<std::uint32_t, std::uint64_t> detected_time;
};

std::unordered_set<std::uint32_t> to_set(const std::vector<space_saving::ExactCount>& top) {
    std::unordered_set<std::uint32_t> out;
    for (const auto& x : top) out.insert(x.element);
    return out;
}

template <class Candidate>
std::pair<double, double> score(const std::vector<Candidate>& returned,
                                const std::unordered_set<std::uint32_t>& truth) {
    std::size_t overlap = 0;
    for (const auto& x : returned) if (truth.count(x.element)) ++overlap;
    const double precision = returned.empty() ? 0.0 : static_cast<double>(overlap) / returned.size();
    const double recall = truth.empty() ? 0.0 : static_cast<double>(overlap) / truth.size();
    return {precision, recall};
}

std::vector<std::uint32_t> ids_of(const std::vector<space_saving::CountEstimate>& items) {
    std::vector<std::uint32_t> ids;
    ids.reserve(items.size());
    for (const auto& x : items) ids.push_back(x.element);
    return ids;
}
std::vector<std::uint32_t> ids_of(const std::vector<space_saving::NaiveWindowCandidate>& items) {
    std::vector<std::uint32_t> ids;
    ids.reserve(items.size());
    for (const auto& x : items) ids.push_back(x.element);
    return ids;
}
std::vector<std::uint32_t> ids_of(const std::vector<space_saving::WindowCountBounds>& items) {
    std::vector<std::uint32_t> ids;
    ids.reserve(items.size());
    for (const auto& x : items) ids.push_back(x.element);
    return ids;
}

void update_detection(MethodMetrics& metrics,
                      const std::vector<std::uint32_t>& returned,
                      const std::unordered_set<std::uint32_t>& true_top,
                      std::uint64_t packet_index,
                      std::uint32_t new_id_begin) {
    for (auto id : true_top) {
        if (id >= new_id_begin) metrics.true_entry_time.emplace(id, packet_index);
    }
    for (auto id : returned) {
        const auto it = metrics.true_entry_time.find(id);
        if (it != metrics.true_entry_time.end() && packet_index >= it->second && true_top.count(id))
            metrics.detected_time.emplace(id, packet_index);
    }
}

void validate_bounds(const BlockWindowedSpaceSaving& windowed,
                     const ExactSlidingWindow& exact,
                     std::size_t alphabet,
                     const space_saving::WindowTopKResult& topk_result,
                     MethodMetrics& metrics) {
    const auto all_bounds = windowed.merged_bounds();
    std::unordered_map<std::uint32_t, space_saving::WindowCountBounds> by_id;
    by_id.reserve(all_bounds.size() * 2 + 1);
    for (const auto& b : all_bounds) by_id.emplace(b.element, b);

    for (std::uint32_t id = 1; id <= alphabet; ++id) {
        const auto truth = exact.count(id);
        const auto it = by_id.find(id);
        if (it == by_id.end()) {
            if (truth > topk_result.unseen_upper_bound) ++metrics.bound_violations;
        } else if (truth < it->second.lower_bound || truth > it->second.upper_bound) {
            ++metrics.bound_violations;
        }
    }
}

struct RunRow {
    std::string scenario;
    double alpha = 0;
    std::size_t repeat = 0;
    std::uint64_t seed = 0;
    std::string method;
    std::size_t blocks = 0;
    std::size_t per_block_counters = 0;
    std::size_t events = 0, alphabet = 0, window = 0, counters = 0, topk = 0;
    std::size_t query_interval = 0;
    std::uint64_t shift_at = 0;
    std::uint64_t query_count = 0;
    double update_ms = 0, query_ms = 0, precision = 0, recall = 0;
    double tie_precision=0,tie_recall=0,valid_rate=0;
    double certification_rate = 0, order_certification_rate = 0, avg_bound_width = 0;
    double detection_coverage = 0, avg_detection_delay = 0;
    std::uint64_t missed_entries = 0, bound_violations = 0;
    double avg_candidates = 0;
    std::size_t memory_bytes = 0;
};

RunRow finalize(const MethodMetrics& m, const Options& o, const std::string& scenario,
                double alpha, std::size_t repeat, std::uint64_t seed,
                const std::string& method, std::size_t blocks,
                std::uint64_t shift_at) {
    RunRow row;
    row.scenario = scenario; row.alpha = alpha; row.repeat = repeat; row.seed = seed;
    row.method = method; row.blocks = blocks;
    row.per_block_counters = blocks ? o.counters / blocks : o.counters;
    row.events = o.events; row.alphabet = o.alphabet; row.window = o.window;
    row.counters = o.counters; row.topk = o.topk; row.query_interval = o.query_interval;
    row.shift_at = shift_at; row.query_count = m.query_count;
    row.update_ms = m.update_ms; row.query_ms = m.query_ms;
    if (m.query_count) {
        row.precision = m.precision_sum / m.query_count;
        row.recall = m.recall_sum / m.query_count;
        row.tie_precision=m.tie_precision_sum/m.query_count;
        row.tie_recall=m.tie_recall_sum/m.query_count;
        row.valid_rate=m.valid_sum/m.query_count;
        row.certification_rate = m.certification_sum / m.query_count;
        row.order_certification_rate = m.order_certification_sum / m.query_count;
        row.avg_bound_width = m.bound_width_item_count ? m.bound_width_sum / m.bound_width_item_count : 0.0;
        row.avg_candidates = static_cast<double>(m.candidate_sum) / m.query_count;
    }
    if(method!="Bound-Aware-Block-Merge") {
        row.certification_rate=row.order_certification_rate=row.avg_bound_width=std::numeric_limits<double>::quiet_NaN();
    }
    row.memory_bytes = m.memory_bytes;
    row.bound_violations = m.bound_violations;

    if (scenario != "static" && !m.true_entry_time.empty()) {
        std::uint64_t delay_sum = 0;
        for (const auto& kv : m.true_entry_time) {
            const auto detected = m.detected_time.find(kv.first);
            if (detected == m.detected_time.end()) continue;
            delay_sum += detected->second - kv.second;
        }
        row.detection_coverage = static_cast<double>(m.detected_time.size()) / m.true_entry_time.size();
        row.avg_detection_delay = m.detected_time.empty() ? std::numeric_limits<double>::quiet_NaN() :
            static_cast<double>(delay_sum) / m.detected_time.size();
        row.missed_entries = m.true_entry_time.size() - m.detected_time.size();
    }
    if(scenario=="static" || m.true_entry_time.empty()) row.detection_coverage=row.avg_detection_delay=std::numeric_limits<double>::quiet_NaN();
    return row;
}

void write_row(std::ofstream& out, const RunRow& r) {
    out << r.scenario << ',' << r.alpha << ',' << r.repeat << ',' << r.seed << ',' << r.method << ','
        << r.blocks << ',' << r.per_block_counters << ',' << r.events << ',' << r.alphabet << ','
        << r.window << ',' << r.counters << ',' << r.topk << ',' << r.query_interval << ','
        << r.shift_at << ',' << r.query_count << ',' << r.update_ms << ',' << r.query_ms << ','
        << r.update_ms + r.query_ms << ',' << r.precision << ',' << r.recall << ','
        << r.certification_rate << ',' << r.order_certification_rate << ',' << r.avg_bound_width << ','
        << r.detection_coverage << ',' << r.avg_detection_delay << ',' << r.missed_entries << ','
        << r.bound_violations << ',' << r.avg_candidates << ',' << r.memory_bytes << ',' <<r.tie_precision<<','<<r.tie_recall<<','<<r.valid_rate<<'\n';
}

void run_one(const Options& o, const std::string& scenario, double alpha,
             std::size_t repeat, std::uint64_t seed, std::ofstream& csv) {
    const std::size_t half = o.alphabet / 2;
    const std::uint64_t shift_at = o.events / 2 + o.window / 2;
    ZipfSampler static_sampler(o.alphabet, alpha, 0);
    ZipfSampler old_sampler(half, alpha, 0);
    ZipfSampler new_sampler(half, alpha, static_cast<std::uint32_t>(half));

    std::mt19937_64 rng(seed);
    std::vector<std::uint32_t> data(o.events);
    for (std::size_t i = 0; i < o.events; ++i) {
        if(scenario=="static") data[i]=static_sampler.sample(rng);
        else {
            bool after=i+1>shift_at;
            if(scenario=="gradual" && after) {
                const double p=std::min(1.0,double(i+1-shift_at)/o.window);
                after=std::generate_canonical<double,53>(rng)<p;
            }
            data[i]=after?new_sampler.sample(rng):old_sampler.sample(rng);
            // Keep half of the old top-k ranks unchanged after the shift.
            if(scenario=="overlap" && after && data[i]<=half+o.topk/2)
                data[i]-=static_cast<std::uint32_t>(half);
        }
    }

    SpaceSaving cumulative(o.counters);
    ExactSlidingWindow exact(o.window);
    std::vector<std::size_t> block_counts{2, 4, 8};
    std::vector<std::unique_ptr<BlockWindowedSpaceSaving>> windowed;
    windowed.reserve(block_counts.size());
    for (auto b : block_counts)
        windowed.push_back(std::make_unique<BlockWindowedSpaceSaving>(o.window, b, o.counters));

    MethodMetrics cumulative_metrics;
    cumulative_metrics.memory_bytes = cumulative.allocated_bytes();
    std::vector<MethodMetrics> naive_metrics(block_counts.size()), bound_metrics(block_counts.size());
    for (std::size_t i = 0; i < block_counts.size(); ++i) {
        naive_metrics[i].memory_bytes = windowed[i]->allocated_bytes();
        bound_metrics[i].memory_bytes = windowed[i]->allocated_bytes();
    }

    const std::uint32_t new_id_begin = static_cast<std::uint32_t>(half + 1);
    for (std::size_t i = 0; i < data.size(); ++i) {
        const std::uint32_t id = data[i];
        auto t0 = Clock::now();
        cumulative.update(id);
        auto t1 = Clock::now();
        cumulative_metrics.update_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();

        for (std::size_t b = 0; b < windowed.size(); ++b) {
            t0 = Clock::now();
            windowed[b]->update(id);
            t1 = Clock::now();
            const double elapsed = std::chrono::duration<double, std::milli>(t1 - t0).count();
            naive_metrics[b].update_ms += elapsed;
            bound_metrics[b].update_ms += elapsed;
        }
        exact.update(id);

        const std::uint64_t packet_index = i + 1;
        if (packet_index < o.window || packet_index % o.query_interval != 0) continue;

        const auto truth_top = exact.query_top_k(o.topk);
        const auto truth_set = to_set(truth_top);
        const auto kth=truth_top.empty()?0:truth_top.back().count;
        std::size_t mandatory=0;
        for(auto& x:truth_top) mandatory+=x.count>kth;
        auto add_tie=[&](MethodMetrics& m,const auto& items){
            const auto tie=space_saving::tie_metrics(items,truth_top.size(),kth,mandatory,[&](auto id){return exact.count(id);});
            m.tie_precision_sum+=tie.precision; m.tie_recall_sum+=tie.recall; m.valid_sum+=tie.valid;
        };

        // Cumulative Space-Saving baseline.
        t0 = Clock::now();
        const auto cumulative_top = cumulative.query_top_k(o.topk);
        t1 = Clock::now();
        cumulative_metrics.query_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
        const auto cumulative_ids = ids_of(cumulative_top.elements);
        const auto cumulative_score = score(cumulative_top.elements, truth_set);
        add_tie(cumulative_metrics,cumulative_top.elements);
        cumulative_metrics.precision_sum += cumulative_score.first;
        cumulative_metrics.recall_sum += cumulative_score.second;
        cumulative_metrics.certification_sum += cumulative_top.guaranteed_topk_strict ? 1.0 : 0.0;
        cumulative_metrics.order_certification_sum += cumulative_top.guaranteed_order ? 1.0 : 0.0;
        for (const auto& x : cumulative_top.elements) {
            cumulative_metrics.bound_width_sum += static_cast<double>(x.error);
            ++cumulative_metrics.bound_width_item_count;
        }
        cumulative_metrics.candidate_sum += cumulative.monitored_count();
        ++cumulative_metrics.query_count;
        if (scenario != "static")
            update_detection(cumulative_metrics, cumulative_ids, truth_set, packet_index, new_id_begin);

        for (std::size_t b = 0; b < windowed.size(); ++b) {
            if (!windowed[b]->ready())
                throw std::runtime_error("query point is not aligned for B=" + std::to_string(block_counts[b]));

            // Naive count-only merge.
            t0 = Clock::now();
            const auto naive_top = windowed[b]->query_top_k_naive(o.topk);
            t1 = Clock::now();
            naive_metrics[b].query_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
            const auto naive_ids = ids_of(naive_top);
            const auto naive_score = score(naive_top, truth_set);
            add_tie(naive_metrics[b],naive_top);
            naive_metrics[b].precision_sum += naive_score.first;
            naive_metrics[b].recall_sum += naive_score.second;
            ++naive_metrics[b].query_count;
            if (scenario != "static")
                update_detection(naive_metrics[b], naive_ids, truth_set, packet_index, new_id_begin);

            // Bound-aware merge and certification.
            t0 = Clock::now();
            const auto bounded_top = windowed[b]->query_top_k(o.topk);
            t1 = Clock::now();
            bound_metrics[b].query_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
            const auto bounded_ids = ids_of(bounded_top.elements);
            const auto bounded_score = score(bounded_top.elements, truth_set);
            add_tie(bound_metrics[b],bounded_top.elements);
            bound_metrics[b].precision_sum += bounded_score.first;
            bound_metrics[b].recall_sum += bounded_score.second;
            bound_metrics[b].certification_sum += bounded_top.membership_certified ? 1.0 : 0.0;
            bound_metrics[b].order_certification_sum += bounded_top.order_certified ? 1.0 : 0.0;
            for (const auto& x : bounded_top.elements) {
                bound_metrics[b].bound_width_sum += static_cast<double>(x.width());
                ++bound_metrics[b].bound_width_item_count;
            }
            bound_metrics[b].candidate_sum += bounded_top.candidate_count;
            naive_metrics[b].candidate_sum += bounded_top.candidate_count;
            ++bound_metrics[b].query_count;
            if (scenario != "static")
                update_detection(bound_metrics[b], bounded_ids, truth_set, packet_index, new_id_begin);

            if (o.validate) {
                if (!windowed[b]->validate() || (b==0 && !exact.validate()))
                    throw std::runtime_error("window/exact data structure invariant failed");
                validate_bounds(*windowed[b], exact, o.alphabet, bounded_top, bound_metrics[b]);
                if(bound_metrics[b].bound_violations) throw std::runtime_error("frequency bound violated");
                if (bounded_top.membership_certified) {
                    std::unordered_set<std::uint32_t> got(bounded_ids.begin(), bounded_ids.end());
                    if (got != truth_set)
                        throw std::runtime_error("Top-k certification produced an incorrect set");
                }
                if (bounded_top.order_certified) {
                    for (std::size_t i = 1; i < bounded_top.elements.size(); ++i) {
                        if (exact.count(bounded_top.elements[i - 1].element) <
                            exact.count(bounded_top.elements[i].element))
                            throw std::runtime_error("Top-k order certification is incorrect");
                    }
                }
            }
        }
    }

    for (std::size_t b = 0; b < block_counts.size(); ++b) {
        naive_metrics[b].memory_bytes = windowed[b]->allocated_bytes();
        bound_metrics[b].memory_bytes = windowed[b]->allocated_bytes();
    }

    write_row(csv, finalize(cumulative_metrics, o, scenario, alpha, repeat, seed,
                           "Cumulative-SpaceSaving", 0, shift_at));
    for (std::size_t b = 0; b < block_counts.size(); ++b) {
        write_row(csv, finalize(naive_metrics[b], o, scenario, alpha, repeat, seed,
                               "Naive-Block-Merge", block_counts[b], shift_at));
        write_row(csv, finalize(bound_metrics[b], o, scenario, alpha, repeat, seed,
                               "Bound-Aware-Block-Merge", block_counts[b], shift_at));
    }

    std::cout << "scenario=" << scenario << " alpha=" << alpha << " repeat=" << repeat
              << " seed=" << seed << " queries=" << cumulative_metrics.query_count
              << " | cumulative P/R="
              << (cumulative_metrics.query_count ? cumulative_metrics.precision_sum / cumulative_metrics.query_count : 0)
              << "/" << (cumulative_metrics.query_count ? cumulative_metrics.recall_sum / cumulative_metrics.query_count : 0)
              << " | bound-aware B=4 P/R="
              << (bound_metrics[1].query_count ? bound_metrics[1].precision_sum / bound_metrics[1].query_count : 0)
              << "/" << (bound_metrics[1].query_count ? bound_metrics[1].recall_sum / bound_metrics[1].query_count : 0)
              << " cert="
              << (bound_metrics[1].query_count ? bound_metrics[1].certification_sum / bound_metrics[1].query_count : 0)
              << " violations=" << bound_metrics[1].bound_violations << "\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options o = parse(argc, argv);
        std::ofstream csv(o.output);
        if (!csv) throw std::runtime_error("cannot open output CSV: " + o.output);
        csv << "scenario,alpha,repeat,seed,method,blocks,per_block_counters,events,alphabet,window,"
               "total_counters,topk,query_interval,shift_at,query_count,update_ms,query_ms,runtime_ms,"
               "precision,recall,certification_rate,order_certification_rate,avg_bound_width_per_returned_item,"
               "detection_coverage,avg_detection_delay_packets,missed_entries,bound_violations,"
               "avg_candidates,memory_bytes,tie_precision,tie_recall,topk_set_valid_rate\n";
        csv << std::fixed << std::setprecision(8);

        for (std::size_t ai = 0; ai < o.alphas.size(); ++ai) {
            for (std::size_t repeat = 0; repeat < o.repeats; ++repeat) {
                const std::uint64_t seed = o.seed + ai * 1000003ULL + repeat * 1009ULL;
                if (o.scenarios == "all" || o.scenarios == "static")
                    run_one(o, "static", o.alphas[ai], repeat, seed, csv);
                for(const std::string scenario: {"shift","gradual","overlap"})
                    if(o.scenarios=="all"||o.scenarios==scenario)
                        run_one(o,scenario,o.alphas[ai],repeat,seed ^ 0x9e3779b97f4a7c15ULL,csv);
                csv.flush();
            }
        }
        std::cout << "Saved " << o.output << "\n";
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
