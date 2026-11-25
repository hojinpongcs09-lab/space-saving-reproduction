// Small deterministic regression and report generator. Checks remain active
// under NDEBUG; this is an educational validator, not a performance benchmark.
#include "exact_sliding_window.hpp"
#include "windowed_space_saving.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using space_saving::BlockWindowedSpaceSaving;
using space_saving::ExactSlidingWindow;
void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}
void run(const std::string& name, std::ostream& table, std::ostream& csv) {
    std::ifstream input("examples/windowed_validation/" + name + ".txt");
    require(bool(input), "cannot open fixture");
    std::vector<std::uint32_t> data;
    std::uint32_t id;
    while (input >> id) { require(id >= 1 && id <= 9, "invalid fixture ID"); data.push_back(id); }
    require(input.eof() && data.size() == 16, "fixture must contain 16 numeric IDs");
    const std::vector<std::uint32_t> expected = name == "shift" ?
        std::vector<std::uint32_t>{1,1,1,1,2,2,2,3,4,4,4,4,4,4,4,4} :
        std::vector<std::uint32_t>{1,2,3,4,5,6,7,8,9,9,9,9,9,9,9,9};
    require(data == expected, "fixture changed: update the hand-calculated expectations");
    BlockWindowedSpaceSaving window(8, 2, 4);
    ExactSlidingWindow oracle(8);
    std::size_t queries = 0;
    for (std::size_t i = 0; i < data.size(); ++i) {
        window.update(data[i]); oracle.update(data[i]);
        const auto packet = i + 1;
        require(window.validate() && oracle.validate(), "structure invariant failed");
        const bool ready = packet >= 8 && packet % 4 == 0;
        require(window.ready() == ready, "incorrect query availability");
        if (!ready) {
            bool rejected = false;
            try { (void)window.query_top_k(1); }
            catch (const std::logic_error&) { rejected = true; }
            require(rejected, "partial-window query must be rejected");
            continue;
        }
        ++queries;
        // Independently rebuild counts directly from the original last eight
        // packets, rather than relying solely on the FIFO oracle.
        std::map<std::uint32_t, std::uint64_t> truth;
        for (std::size_t j = packet - 8; j < packet; ++j) ++truth[data[j]];
        const auto result = window.query_top_k(1);
        const auto bounds = window.merged_bounds();
        std::uint32_t exact_top = 0;
        std::uint64_t max_count = 0;
        for (auto key = 1u; key <= 9; ++key) {
            require(oracle.count(key) == truth[key], "FIFO disagrees with direct count");
            if (truth[key] > max_count) { max_count = truth[key]; exact_top = key; }
            auto it = std::find_if(bounds.begin(), bounds.end(), [key](const auto& b){ return b.element == key; });
            const bool tracked = it != bounds.end();
            const auto lower = tracked ? it->lower_bound : 0;
            const auto upper = tracked ? it->upper_bound : result.unseen_upper_bound;
            require(lower <= truth[key] && truth[key] <= upper, "frequency bound violated");
            csv << name << ',' << packet << ',' << packet-7 << ',' << packet << ',' << key << ','
                << truth[key] << ',' << tracked << ',' << lower << ',' << upper << '\n';
        }
        require(result.elements.size() == 1, "expected Top-1 result");
        const auto& top = result.elements.front();
        const auto expected_top = name == "shift" ? (packet == 8 ? 1u : 4u) : (packet == 8 ? 1u : 9u);
        require(exact_top == expected_top, "incorrect hand-calculated exact Top-1");
        if (name == "shift" || packet > 8) {
            require(top.element == expected_top, "new dominant source not returned");
            require(result.membership_certified, "expected strictly certified Top-1");
        } else {
            require(!result.membership_certified, "replacement/tied result must not certify");
            require(result.unseen_upper_bound == 4, "expected unseen bound 2+2");
        }
        if (packet == 12) {
            require(oracle.count(1) == 0, "expired source still counted");
            require(std::none_of(bounds.begin(), bounds.end(), [](const auto& b){return b.element==1;}),
                    "expired block still contributes a candidate");
        }
        if (result.membership_certified)
            require(truth[top.element] == max_count, "incorrect certified membership");
        std::ostringstream sequence;
        for (std::size_t j = packet-8; j < packet; ++j) sequence << (j==packet-8 ? "" : " ") << data[j];
        table << "| " << name << " | " << packet << " | " << packet-7 << "–" << packet
              << " | `" << sequence.str() << "` | " << exact_top << "（" << max_count << "） | "
              << top.element << " | [" << top.lower_bound << ", " << top.upper_bound << "] | "
              << result.unseen_upper_bound << " | " << (result.membership_certified ? "是" : "否") << " |\n";
    }
    require(queries == 3, "expected queries at 8, 12, 16");
}
int main(int argc, char** argv) {
    try {
        std::ostringstream report, csv;
        report << "# 小型視窗驗證結果\n\nW=8、B=2、總計數器 M=4（每區塊 2 個）、Top-1；每 4 筆查詢。\n\n"
               << "| 案例 | 查詢序號 | 視窗範圍 | 視窗內容 | 精確 Top-1（次數） | 回傳 Top-1 | 回傳頻率界限 | 未見項目上界 | 集合認證 |\n"
               << "|---|---:|---|---|---|---|---|---:|---|\n";
        csv << "scenario,packet,window_start,window_end,element,true_count,in_candidate_union,lower_bound,upper_bound\n";
        run("shift", report, csv); run("replacement", report, csv);
        report << "\nreplacement 在第 8 筆的真實頻率全部為 1，精確 Top-1 以較小 ID 打破平手；其他同頻項目也屬有效答案。"
               << "未見項目以 [0, U_unseen] 驗證。未認證不等於答案錯誤。\n";
        if (argc == 2 && std::string(argv[1]) == "--write-results") {
            std::ofstream md("results/validation/WALKTHROUGH_ZH.md"), out("results/validation/window_bounds.csv");
            require(bool(md) && bool(out), "cannot open result files");
            md << report.str(); out << csv.str(); md.flush(); out.flush();
            require(bool(md) && bool(out), "result write failed");
        } else require(argc == 1, "usage: windowed_walkthrough [--write-results]");
        std::cout << "PASS: two fixtures, six aligned queries, partial-query rejection, direct-count oracle, expiration, bounds and certificates\n";
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
