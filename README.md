# Block-based Windowed Space-Saving for Recent Top-k Flow Monitoring

軟體定義網路及網路功能虛擬化（SDNFV）課程期末專題：**Accuracy, Certification, and Resource Trade-offs**。

> **研究方向已與助教確認為 Sliding Window（近期 Top-k）。** 早期規劃的「D10 門檻告警（`c ≥ T` vs. `c − e ≥ T`）」已不是本專題的主要研究問題，最多只作為額外的小型功能驗證，不應拿來當作近期 Top-k 的核心成果。

## 研究問題

> Under a fixed counter budget, how do block granularity and summary aggregation strategies affect the accuracy, certifiability, detection delay, and processing overhead of recent Top-k flow monitoring?

**在有限 Counter 預算下，Block 的切分粒度與摘要合併策略，會如何影響近期熱門 Flow 偵測的準確率、結果可認證性、偵測延遲及執行成本？**

一句話：我們不是在研究怎麼偵測「封包數超過門檻的 Flow」，而是在研究**有限記憶體下如何維護最近 W 筆封包中最熱門的 k 個 Flow，以及不同近似方法在準確率、可信度與效能上的取捨。**

## 專題三個部分

| 部分 | 內容 | 角色 |
| --- | --- | --- |
| **Part 1 · 原論文重現** | Figure 6：Space-Saving vs CountSketch，Zipf α = 0–3 下的 Precision / Recall / Runtime / Memory | 基礎 |
| **Part 2 · 額外參數實驗** | Space-Saving Counter Budget Sensitivity：α-specific 配置 vs 固定 M ∈ {500, 1K, 2K, 4K, 8K, 10K} | 補充 |
| **Part 3 · 主要研究修改** | Block-based Windowed Space-Saving：比較 Cumulative、Periodic-Reset、Naive Block Merge、Bound-Aware Block Merge（B = 2、4、8） | **核心** |

Part 3 的重點：

- 每個 Block 一份獨立 Space-Saving 摘要（總預算 M 平分給 B 個 Block），以 Ring Buffer 讓整個 Block 過期，而不是逐筆刪除。
- **Bound-Aware Merge** 利用 `c − e ≤ f ≤ c` 對各 Block 上下界加總，以區間中點排名，並在 `min L(S) > max(U(非選), U_unseen)` 時給出 **Top-k Membership Certification**。
- 以 Exact Sliding Window 作為 Ground Truth；同時報告 all-query 與 aligned-only（完整 W 邊界）準確率，避免對 Periodic-Reset 不公平。
- 限制：只支援 Block 邊界查詢（Block-aligned window）；Detection Delay 為每 50K 查詢取樣；Memory 不含 Merge 暫存空間與 Ground Truth。

## 程式與文件位置

主要整合專案：[`SpaceSaving_CountSketch_Integrated/`](SpaceSaving_CountSketch_Integrated/)

```sh
cd SpaceSaving_CountSketch_Integrated
make -j4 all
make test                    # Space-Saving、CountSketch、Windowed、Tie-aware、Periodic-Reset、手算 walkthrough
make periodic-comparison     # 四種方法家族、M=1600 的主要比較
make windowed-walkthrough    # 可手算的小型視窗驗證
```

| 主題 | 文件 |
| --- | --- |
| 整合專案說明與所有實驗指令 | [README](SpaceSaving_CountSketch_Integrated/README.md) |
| Windowed 方法、上下界與認證定義 | [WINDOWED_METHOD.md](SpaceSaving_CountSketch_Integrated/docs/WINDOWED_METHOD.md) |
| Sensitivity 實驗定義與指標 | [SENSITIVITY_PROTOCOL.md](SpaceSaving_CountSketch_Integrated/docs/SENSITIVITY_PROTOCOL.md) |
| Periodic-Reset 比較結果 | [results/periodic/RESULTS_SUMMARY.md](SpaceSaving_CountSketch_Integrated/results/periodic/RESULTS_SUMMARY.md) |
| Sensitivity 結果 | [results/sensitivity/RESULTS_SUMMARY.md](SpaceSaving_CountSketch_Integrated/results/sensitivity/RESULTS_SUMMARY.md) |
| 相關研究 | [RELATED_WORK.md](SpaceSaving_CountSketch_Integrated/docs/RELATED_WORK.md)、[Sliding Window 文獻對照（FSP／FSW）](SpaceSaving_CountSketch_Integrated/docs/SLIDING_WINDOW_PAPER_COMPARISON_ZH.md) |
| 可手算的視窗驗證 | [案例說明](SpaceSaving_CountSketch_Integrated/examples/windowed_validation/README.md)、[結果表](SpaceSaving_CountSketch_Integrated/results/validation/WALKTHROUGH_ZH.md)、[工作說明](SpaceSaving_CountSketch_Integrated/docs/WINDOWED_VALIDATION_CONTRIBUTION_ZH.md) |
| 目前進度與待辦 | [PROJECT_STATUS.md](SpaceSaving_CountSketch_Integrated/docs/PROJECT_STATUS.md) |

[`figure6/`](figure6/) 保留最早獨立整合的 Figure 6 重現版本（含 calibrated / equal-memory 圖表），詳見 [figure6/README.md](figure6/README.md) 與 [第三方授權說明](figure6/THIRD_PARTY_NOTICES.md)。

## 研究定位與限制

- 本專題是課程中的**實作、方法組合與實驗分析**；不宣稱首次提出 Sliding Window、Block 淘汰、摘要合併或 Space-Saving 頻率上下界。
- Figure 6 是依照方法的部分重現：未實作 Probabilistic-InPlace，原作者的亂數、部分參數與程式無法完整取得。
- 目前結果皆為合成 Zipf 資料（Static、Abrupt、Gradual、Overlapping Shift），尚未以真實網路 Trace 驗證。
- Periodic-Reset 在全部六種 M 的 Counter Sensitivity 尚未重跑；效能數字需在同一 Linux 主機依序重跑後才作最終結論。

## 分工

| 組員 | 負責範圍 | 交付物 |
| --- | --- | --- |
| [@Akhilesh1004](https://github.com/Akhilesh1004) | **Part 1–3 的演算法實作與實驗。** Space-Saving、CountSketch、Figure 6 部分重現；Space-Saving Counter Budget Sensitivity；Block-based Windowed Space-Saving（Block Rotation、Naive／Bound-Aware Merge、Top-k Membership／Order Certification）；Exact Sliding Window Ground Truth、Tie-aware 評估；四種流量情境；Periodic-Reset Baseline 與 Windowed Counter Sensitivity 實驗。 | `figure6/` 的實驗程式與結果；`SpaceSaving_CountSketch_Integrated/` 的 `include/`、`src/`、`tests/`、`scripts/`、`results/`（`results/validation/` 除外），以及 `WINDOWED_METHOD.md`、`SENSITIVITY_PROTOCOL.md`、`RELATED_WORK.md`、`PROJECT_STATUS.md` |
| 何錦鵬（[@hojinpongcs09-lab](https://github.com/hojinpongcs09-lab)） | **Repo 建立、文獻對照與手算驗證。** 建立 repo 與初版專題規劃；將 Figure 6 實驗整合進 repo 並補上授權說明；修訂首頁 README；整理 Sliding Window 文獻（FSP／FSW）與本專題方法的對照，並新增可手算的區塊視窗驗證。 | `docs/SLIDING_WINDOW_PAPER_COMPARISON_ZH.md`、`docs/WINDOWED_VALIDATION_CONTRIBUTION_ZH.md`、`examples/windowed_validation/`、`tests/windowed_walkthrough.cpp`、`results/validation/` |
| 李曄 |  |  |

## 參考資料

- Metwally, Agrawal, El Abbadi. [Efficient Computation of Frequent and Top-k Elements in Data Streams](https://doi.org/10.1007/978-3-540-30570-5_27). ICDT 2005.
- Homem, Carvalho. [Finding Top-k Elements in a Time-Sliding Window](https://doi.org/10.1007/s12530-010-9020-z). Evolving Systems, 2011.
- [Floating Top-k](https://doi.org/10.1016/j.ins.2018.09.066)（2019）、[Memento](https://arxiv.org/abs/1810.02899)（2018）、[Expiration Filter](https://doi.org/10.1016/j.comnet.2024.111010)（2025）、[Mergeable Summaries](https://doi.org/10.1145/2500128)（2013）。
- [byronknoll 的 C++ Space-Saving 實作](https://github.com/byronknoll/space-saving)、[Frequent Items 演算法比較套件](https://hadjieleftheriou.com/frequent-items/)。

## 中文實驗結果分析

[三組核心圖與報告稿](SpaceSaving_CountSketch_Integrated/docs/RESULTS_ANALYSIS_ZH.md)串聯文獻、手算驗證與 Periodic 比較結果，並附上資料重算表與限制說明。
