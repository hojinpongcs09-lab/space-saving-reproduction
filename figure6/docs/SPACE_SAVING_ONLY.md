# Space-Saving / Stream-Summary — Figure 6 Reproduction

**論文：** Ahmed Metwally, Divyakant Agrawal, Amr El Abbadi, *Efficient Computation of Frequent and Top-k Elements in Data Streams*, ICDT 2005, pp. 398–412.

**本版本範圍：** 只重現 **Figure 6 的 Space-Saving 系列**，包含 (a) Runtime、(b) Precision、(c) Recall、(d) Memory Usage。**沒有 CountSketch、Probabilistic-InPlace、Windowed 修改**。

此專案是根據你上傳的 `frequent-items-10(1).zip` 中 `src/lossycount.cc` 的 **LCU (linked-group Space-Saving)** 架構整理、重新命名並獨立化。特別參考 `LCU_Update`、`LCU_IncrementCounter`、`LCU_GetNewCounter` 以及 `lcu_group` / `lcu_item` 的資料結構。**不是原論文作者 2005 年發布的原始實驗 Source Code。** 第三方來源與授權註記見 [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)。

## 專案內容

```text
space-saving-figure6/
├── include/space_saving.hpp        SpaceSaving、StreamSummary、CountEstimate、TopKResult
├── src/space_saving.cpp            Space-Saving 核心 + Hash Table + Bucket Linked List
├── src/figure6_experiment.cpp      Zipf Generator、Ground Truth、四項測量、CSV
├── tests/test_space_saving.cpp     更新、誤差上下界、Top-k 保證及隨機對照測試
├── scripts/plot_figure6.py        四張單獨直方圖 + 一張 2×2 Figure 6 直方圖（每柱標實測值）
├── results/figure6_space_saving.csv     已跑的固定 Counter 結果
├── results/figure6_space_saving.png     已跑的固定 Counter 合併直方圖（PNG）
├── results/figure6_space_saving.svg     同一張合併直方圖（可放大向量 SVG）
├── results/figure6_calibrated.csv       已跑的離線 Counter 調整結果
├── results/calibrated/figure6_space_saving.png  離線 Counter 調整直方圖
├── Makefile
└── THIRD_PARTY_NOTICES.md
```

## 演算法與原論文對照

| 這份程式 | 原始 LCU | 論文名稱 | 實作 |
|---|---|---|---|
| `SpaceSaving::update` | `LCU_Update` | Space-Saving (Figure 1) | 已完成 |
| `StreamSummary` | `LCU_type` 的資料欄位 | Stream-Summary | 已完成 |
| `Bucket` | `lcu_group` | Group/Bucket | 已完成 |
| `Element` | `lcu_item` | Tracked element | 已完成 |
| `error` | `delta` | Maximum overestimation `εᵢ` | 已完成 |
| `query_top_k` | 原版本只有 threshold-based `LCU_Output` | QueryTop-k (Figure 4) | 已補上 |
| `TopKResult.guaranteed_topk` | 無 | Guarantee flag | 已補上 |
| `figure6_experiment` | `hh.cc` 原本是 Heavy Hitter threshold 實驗 | Figure 6 的 Space-Saving 部分 | 已補上 |

`SpaceSaving` 擁有 `StreamSummary`；後者包含固定長度的元素陣列、以頻率排序的 Bucket Doubly Linked List、每個 Bucket 內的 Circular Element Linked List、雜湊鏈結表。新元素只替換 **Count 最小** 的元素；其新 `count = min + 1`，`error = min`。已追蹤元素只更新 Count，不改 Error。

每個被追蹤元素滿足 `count - error <= true_frequency <= count`。測試會使用精確 Hash Map/陣列確認此界限，以及所有 Counter Count 之和等於 N。

## 環境

- GNU Make、g++（C++17）
- Python 3 + `matplotlib`（只用來畫圖，C++ 程式不需要 Python）
- Linux 或 macOS；不需要 Windows IDE / WinPcap

## 建置與測試

```bash
make
make test
make smoke
```

`make test` 會執行小型人工案例、capacity=1、ID=0、足夠 Counter 的精確情況，以及多組隨機資料的逐 Prefix 差分測試。

`make smoke` 使用 N=1,000,000、A=1,000、M=300、k=100，產生 `results/figure6_smoke.csv` 與 `results/smoke/` 的 PNG / SVG 直方圖。

### Figure 6 圖表格式（與原論文一致的柱狀呈現）

`python3 scripts/plot_figure6.py` 現在輸出 **四個分組位置的柱狀子圖**，依序是 (a) Runtime、(b) Precision、(c) Recall、(d) Space Used。每個 Zipf α 對應一根 **Space-Saving 黑色柱**，**柱頂標明 CSV 實測數字**；Runtime 標示到小數點後兩位、Precision / Recall 到兩位、Memory 以完整 Bytes 整數表示。另輸出四張獨立圖，檔名為 `figure6_runtime`、`figure6_precision`、`figure6_recall`、`figure6_memory`，所有圖同時提供 `.png` 和 `.svg`。

目前只重現 **Space-Saving 系列**，因此刻意**不畫** CountSketch 或 Probabilistic-InPlace 的柱子，以免誤導為已完成三演算法比較。預設固定 M=1,000，因此固定模式的 Memory 柱狀圖高度相同；`results/calibrated/` 則是另行標示為 **offline oracle-calibrated** 的不同實驗，不能當作原作者 Figure 6(d) 的等價配置。

若要改畫已有的 CSV，不用重跑 C++ 實驗：

```bash
python3 scripts/plot_figure6.py --csv results/figure6_space_saving.csv --outdir results
python3 scripts/plot_figure6.py --csv results/figure6_calibrated.csv --outdir results/calibrated
```

## 正式重現 Figure 6 的 Space-Saving 系列

**方法 A：固定 Counter 數量**（公平測試固定記憶體表現，預設）

```bash
make reproduce
```

等同於：

```bash
mkdir -p results
./build/figure6_experiment \
  --events 10000000 --alphabet 10000 --topk 100 --counters 1000 \
  --alphas 0,0.5,1,1.5,2,2.5,3 \
  --seed 2026 --mode fixed --output results/figure6_space_saving.csv
python3 scripts/plot_figure6.py \
  --csv results/figure6_space_saving.csv --outdir results
```

- `--events 10000000`、`--topk 100`、α=0..3 間隔 0.5：對齊論文 Section 5 / Figure 6 的主要設定。
- `--alphabet 10000`、`--counters 1000`、`--seed 2026`：為了可重現性 **本專案自己設定**，不能聲稱與論文所有原始隱含參數一致。
- 所有 α 使用 **相同 M=1000**，所以 Memory Bytes 為水平線。這是合理且預期的結果，不應該假裝它和原 Figure 6(d) 的變動曲線相同。
- 當 α 很低且 M 不足時，Precision / Recall 可能明顯低於論文。這不代表更新規則一定錯誤；需先對齊原文 Counter 配額與資料分布。

**方法 B：離線掃描 Counter 預算**（展示達成 Top-100 正確所需的 Memory）

```bash
make calibrate
```

等同使用 `--mode calibrate --budgets 100,200,400,800,1600,3200,6400,10000`。對每種 α，在候選 Counter 數量中依序測試，使用 **Ground Truth 在實驗結束後離線挑出第一個 Precision=Recall=1 的預算**。如果全部都無法達到 1，則使用最後一個預算。

> **非常重要：`calibrate` 使用 Ground Truth 作為 *offline oracle* 調參。它不是線上 Space-Saving 的一項能力，也不是聲稱原作者使用了相同調參法。** 用這個模式的目的是分析「為何 Figure 6 的空間用量隨 α 改變」，而不是宣稱同樣的數值重現成功。

可以另外加入 `--require-guarantee`，要求 `query_top_k()` 提供論文的非嚴格 Top-k Membership Guarantee，才視為選中候選預算。

兩種模式請 **分開報告**，不可把校準後的 Memory 曲線當成固定 M 的實驗結果。

## 四項評估指標如何計算

1. **Runtime（ms）**：`update_ms + query_ms`，只計量 Space-Saving 處理整個 stream 以及 QueryTop-k 的執行時間，**不包含**生成資料、建立精確 Ground Truth、校準多組 Counter、畫圖、測試 Assert。這會因 CPU、編譯器、最佳化設定而不同，不能與 2005 年 Pentium IV 的絕對毫秒數直接比較。
2. **Precision**：輸出的 Top-k 候選中，屬於 Ground Truth Top-k 的比例。
3. **Recall**：Ground Truth Top-k 中，被輸出候選涵蓋的比例。在本專案 Space-Saving 永遠輸出恰好 k 個候選（且資料中至少 k 個 distinct ID），因此 Precision=Recall 是**數學上必然**的；原 Figure 6 的其他演算法可能輸出更多候選，所以不同。
4. **Memory（bytes）**：`SpaceSaving::allocated_bytes()`，計入 SpaceSaving/StreamSummary 物件與固定 Hash Head / Element / Bucket / Free Bucket Pointer 陣列的配置容量。**不包含**輸入事件陣列、Ground Truth、Query 結果、Zipf 抽樣表、allocator metadata、執行檔及 RSS。因此是 **data-structure allocated bytes**，不是整個程序的 RSS；且 64-bit 編譯環境與原 32-bit 研究環境不同。

實驗用 Vose Alias Method 產生符合 `P(rank=i) ∝ i^{-α}` 的 ID 串流。`std::mt19937_64` 提供可重現的種子，並把每一筆 stream 的實際結果存入 Ground Truth。若 `alpha=0` 的頻率有大量 ties，我們按 **element ID 升序**打破 ties；此 tie-breaking 是本專案的明確選擇，而非論文另行規定的標準。

## 已執行的 10^7 事件測試（這台環境）

| α | Fixed M | Fixed Precision/Recall | Calibrated M (offline) | Calibrated Precision/Recall |
|---:|---:|---:|---:|---:|
| 0.0 | 1,000 | 0.01 | 10,000 | 1.00 |
| 0.5 | 1,000 | 0.63 | 3,200 | 1.00 |
| 1.0 | 1,000 | 1.00 | 800 | 1.00 |
| 1.5 | 1,000 | 1.00 | 400 | 1.00 |
| 2.0 | 1,000 | 1.00 | 400 | 1.00 |
| 2.5 | 1,000 | 1.00 | 200 | 1.00 |
| 3.0 | 1,000 | 1.00 | 400 | 1.00 |

上述是**這份程式、這些指定參數、這台環境實際測出的結果**，並非論文原圖的數值。詳細 Runtime、Memory、Seeds 與 Guarantee Flag 見 CSV。

### 現階段未完成／刻意排除

- 未實作 CountSketch 和 Probabilistic-InPlace（使用者要求此階段不做對照）。
- 未重現原 Figure 6 兩個對照演算法的柱狀圖。
- 未驗證與原論文 Figure 6 的**逐點數值**完全一致；原始比較用參數及資料生成設定並未在 15 頁版本全部公開。
- 未實作 Windowed Modification，應在 Baseline 驗證後另行加入。
- 沒有把 `byronknoll/space-saving` 的二次掃描候選計數器合併進來，避免破壞這份實作的單次串流誤差語意。

## 學術使用與授權

原始 `frequent-items-10` 的 LCU 來源歸屬見 `THIRD_PARTY_NOTICES.md`，其中原始資料標示為 Creative Commons Attribution–NonCommercial。使用、提交及再散布時請遵守課程 AI 使用規範、原始來源與授權條款，並在報告中清楚區分「第三方參考實作」、「自行整理修改」及「實驗結果」。
