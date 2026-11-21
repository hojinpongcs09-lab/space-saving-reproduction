#include "space_saving.hpp"
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct Rule {
    std::uint64_t first_alert = 0, first_detection = 0;
    bool false_alert = false;
};
struct Source {
    std::uint64_t count = 0, reached = 0;
    Rule estimate, lower;
};

std::uint64_t positive(const std::string& s) {
    if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos)
        throw std::invalid_argument("expected positive integer: " + s);
    const auto n = std::stoull(s);
    if (!n) throw std::invalid_argument("expected positive integer: " + s);
    return n;
}
std::uint32_t ipv4(const std::string& s) {
    std::stringstream in(s);
    std::string part;
    std::uint32_t id = 0;
    for (int i = 0; i < 4; ++i) {
        if (!std::getline(in, part, '.') || part.empty() || part.size() > 3 ||
            part.find_first_not_of("0123456789") != std::string::npos)
            throw std::invalid_argument("invalid IPv4: " + s);
        const auto octet = std::stoul(part);
        if (octet > 255) throw std::invalid_argument("invalid IPv4: " + s);
        id = (id << 8) | static_cast<std::uint32_t>(octet);
    }
    if (in.peek() != EOF || s.back() == '.') throw std::invalid_argument("invalid IPv4: " + s);
    return id;
}
std::string address(std::uint32_t id) {
    return std::to_string(id >> 24) + "." + std::to_string((id >> 16) & 255) + "." +
           std::to_string((id >> 8) & 255) + "." + std::to_string(id & 255);
}
void evaluate(Rule& r, bool alert, std::uint64_t f, std::uint64_t threshold, std::uint64_t packet) {
    if (!alert) return;
    if (!r.first_alert) r.first_alert = packet;
    if (f < threshold) r.false_alert = true;
    // An early false alert does not suppress later valid detection.
    if (f >= threshold && !r.first_detection) r.first_detection = packet;
}

int main(int argc, char** argv) {
    try {
        std::string input, events_path, sources_path;
        std::uint64_t counters = 4, threshold = 5;
        for (int i = 1; i < argc; ++i) {
            const std::string key = argv[i];
            if (key == "--help") {
                std::cout << "alert_validation --input IPv4.txt --events events.csv --sources sources.csv [--counters 4] [--threshold 5]\n";
                return 0;
            }
            if (++i == argc) throw std::invalid_argument("missing value for " + key);
            const std::string value = argv[i];
            if (key == "--input") input = value;
            else if (key == "--events") events_path = value;
            else if (key == "--sources") sources_path = value;
            else if (key == "--counters") counters = positive(value);
            else if (key == "--threshold") threshold = positive(value);
            else throw std::invalid_argument("unknown option: " + key);
        }
        if (input.empty() || events_path.empty() || sources_path.empty())
            throw std::invalid_argument("--input, --events and --sources are required");
        if (input == events_path || input == sources_path || events_path == sources_path)
            throw std::invalid_argument("input and output paths must differ");
        if (counters > std::numeric_limits<std::size_t>::max())
            throw std::invalid_argument("counter budget out of range");
        std::ifstream in(input);
        if (!in) throw std::runtime_error("cannot open input");
        // Validate all input before opening outputs, so malformed data cannot
        // leave a partial result that looks like a successful experiment.
        std::vector<std::uint32_t> packets;
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            packets.push_back(ipv4(line));
        }
        if (in.bad() || packets.empty()) throw std::runtime_error("empty or unreadable input");
        std::ofstream events(events_path), sources(sources_path);
        if (!events || !sources) throw std::runtime_error("cannot open output CSV");
        events << "packet,source_ip,true_count,estimate,error,lower_bound,estimate_alert,lower_alert\n";
        sources << "source_ip,final_true_count,true_threshold_packet,rule,first_alert_packet,false_alert,first_detection_packet,delay_packets,missed\n";
        space_saving::SpaceSaving ss(static_cast<std::size_t>(counters));
        std::map<std::uint32_t, Source> truth;
        std::uint64_t packet = 0;
        for (const auto id : packets) {
            ++packet;
            auto& s = truth[id];
            if (++s.count == threshold) s.reached = packet;
            ss.update(id);
            const auto items = ss.monitored_elements();
            const auto it = std::find_if(items.begin(), items.end(), [id](const auto& x) { return x.element == id; });
            if (it == items.end() || !ss.validate()) throw std::runtime_error("Space-Saving invariant failed");
            // This is an educational validator, not a timed benchmark. Check
            // all monitored bounds against the exact cumulative oracle.
            for (const auto& x : items) {
                const auto f = truth.at(x.element).count;
                if (x.lower_bound() > f || f > x.estimated_count)
                    throw std::runtime_error("frequency bound violated");
            }
            const bool est = it->estimated_count >= threshold, low = it->lower_bound() >= threshold;
            evaluate(s.estimate, est, s.count, threshold, packet);
            evaluate(s.lower, low, s.count, threshold, packet);
            events << packet << ',' << address(id) << ',' << s.count << ',' << it->estimated_count << ','
                   << it->error << ',' << it->lower_bound() << ',' << est << ',' << low << '\n';
        }
        for (const auto& entry : truth) {
            const auto& s = entry.second;
            for (const auto& name : {std::string("estimate"), std::string("lower")}) {
                const auto& r = name == "estimate" ? s.estimate : s.lower;
                sources << address(entry.first) << ',' << s.count << ',';
                if (s.reached) sources << s.reached;
                sources << ',' << name << ',';
                if (r.first_alert) sources << r.first_alert;
                sources << ',' << r.false_alert << ',';
                if (r.first_detection) sources << r.first_detection;
                sources << ',';
                if (r.first_detection) sources << r.first_detection - s.reached;
                sources << ',';
                if (s.reached) sources << !r.first_detection;
                sources << '\n';
            }
        }
        events.flush(); sources.flush();
        if (!events || !sources) throw std::runtime_error("CSV write failed");
        std::cout << "Validated " << packet << " packets, " << truth.size() << " sources\n";
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
}
