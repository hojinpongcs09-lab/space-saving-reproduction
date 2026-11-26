# Space-Saving / CountSketch / Block-Windowed Recent Top-k

A single C++17 project containing:

1. A method-aligned independent implementation and partial reproduction of Figure 6 from Metwally, Agrawal and El Abbadi (ICDT 2005).
2. A paired **Space-Saving-only** experiment comparing the α-specific counter policy with a fixed 1,000-counter policy. CountSketch is not part of this supplementary experiment.
3. A block-aligned Windowed Space-Saving extension for recent Top-k Flow monitoring, including conservative frequency-bound aggregation and Top-k membership/order certification.
4. A **Periodic-Reset-SpaceSaving** tumbling-window baseline at the same common query times. At interval boundaries it covers W packets; halfway through it covers only the current partial interval. The CSV explicitly tracks coverage and also supplies aligned-only accuracy metrics.

> **Scope:** This is not the original authors' source code and does not claim numerical reproduction of every Figure 6 bar. Probabilistic-InPlace is not implemented. All project-selected settings are identified below and in the CSV/configuration outputs.

## Quick start

Requirements: `g++` with C++17, `make`, Python 3, `numpy`, and `matplotlib`.

```bash
make -j4 all
make test
```

The build produces these executables:

- `build/figure6_comparison`
- `build/test_space_saving`
- `build/test_count_sketch`
- `build/windowed_experiment`
- `build/test_windowed_space_saving`
- `build/test_topk_metrics`
- `build/test_periodic_reset_space_saving`

## Experiments

### A. Figure 6: Space-Saving vs CountSketch

```bash
make reproduce
make plot
```

Default settings: `N=10,000,000`, `A=10,000` (project choice), `Top-k=100`, Zipf α = `0, 0.5, 1, 1.5, 2, 2.5, 3`, three seeds. Space-Saving uses the pre-specified α-specific policy based on the UCSB technical report's Theorem 7 proof for α≥1; for α<1, the report's stated bound is not applied and this project conservatively uses the full alphabet. CountSketch uses signed hash rows and a fixed-k Stream-Summary candidate structure.

The original paper's exact alphabet mapping, PRNG, seeds, CountSketch width/depth and source code are not fully available. This is therefore a **method-aligned independent partial reproduction**, not a bit-for-bit reproduction. The project omits Probabilistic-InPlace. CountSketch uses project-selected `7 × 8192` sketch counters and 100 candidates; `δ=0.01` is recorded as the paper's target but this chosen configuration is not claimed to prove a formal δ guarantee.

Outputs:

- `results/figure6_comparison.csv`
- `results/summary_by_alpha.csv`
- `results/comparison/figure6_comparison.png` and `.svg`
- individual Runtime / Precision / Recall / Space plots
- `results/configuration.json`

### B. Supplementary Space-Saving counter-budget comparison

```bash
make compare-ss-budgets
```

This experiment compares **only Space-Saving against Space-Saving**:

- `Space-Saving (alpha-specific)`: Theorem-7 sufficient bound for α≥1, with full-alphabet fallback for α<1.
- `Space-Saving (fixed-1000)`: exactly 1,000 counters at every α.

Both policies process the same generated stream and use the same exact Top-100 ground truth for each `(alpha, seed)`. CountSketch is neither run nor modified in this experiment.

The fixed `M=1,000` is a project-selected resource-control point, not a paper parameter, not a theorem-derived optimum, and not selected using Ground Truth. It was selected before these runs because it was already used by the earlier project diagnostic and is 10× Top-k. It should be described as a **fixed-budget sensitivity comparison**, not as the paper's configuration.

Outputs:

- `results/space_saving_budget_comparison.csv`
- `results/space_saving_budget_comparison_config.json`
- `results/space_saving_budget_comparison/summary_by_alpha.csv`
- four-panel PNG/SVG and individual metric plots in `results/space_saving_budget_comparison/`

### C. Block-based Windowed Space-Saving for recent Top-k

```bash
make windowed-smoke
make windowed-experiment
make windowed-plot
make periodic-comparison
```

Default project settings:

| Parameter | Value |
|---|---:|
| Stream length | 2,000,000 events |
| Alphabet | 10,000 Flow IDs |
| Window size `W` | 100,000 packets |
| Top-k | 20 |
| Total Space-Saving counters | 1,600 |
| Block counts `B` | 2, 4, 8 |
| Counters per block | 800, 400, 200 |
| Query interval | 50,000 packets |
| Zipf α | 1.0, 1.5, 2.0 |
| Repeats | 3 seeds |

The method keeps `B` independent Space-Saving summaries, each representing one non-overlapping block of `W/B` packets. When a new block starts, the oldest summary is evicted. A query is accepted only after a full window has been collected and at a block boundary. It is **block-aligned**, not an exact arbitrary-position sliding window.

Three approximate methods are evaluated:

1. **Cumulative-SpaceSaving**: one summary over all historical packets.
2. **Periodic-Reset-SpaceSaving**: one M-counter summary that resets every W packets; compared against exact latest-W at all common query points while explicitly recording its partial-window coverage.
3. **Naive-Block-Merge**: sums stored counts; a missing per-block candidate contributes zero. This is a deliberately non-certified baseline.
4. **Bound-Aware-Block-Merge**: merges conservative lower/upper frequency bounds and reports whether Top-k membership/order can be certified.

`ExactSlidingWindow` (FIFO + exact per-ID map) is used only as the Ground Truth oracle; its memory and processing cost are excluded from approximate-method measurements.

#### Bound-aware aggregation

For a candidate `x` tracked in block `b`:

`L_b(x) = count_b(x) - error_b(x)`

`U_b(x) = count_b(x)`

If `x` is absent from a full block, use `[0, min_count_b]`; if absent from a non-full block, use `[0,0]`. Sum the bounds over disjoint blocks:

`L(x) = sum_b L_b(x)`

`U(x) = sum_b U_b(x)`

An ID absent from every block has the conservative upper bound:

`U_unseen = sum_b min_count_b` over full summaries (zero for non-full summaries).

The returned Top-k membership set is certified only if its smallest selected lower bound is strictly greater than every excluded candidate upper bound and `U_unseen`. The order certificate concerns the order **among returned candidates only**. The returned candidate ranking uses interval midpoints; the midpoint is a point estimate, not the guarantee.

#### Windowed metrics

- Precision / Recall against exact recent-window Top-k.
- Membership certification rate and within-result order certification rate.
- Average bound width.
- Update throughput / update time and query latency.
- Persistent owned-structure memory bytes.
- Under abrupt distribution shift: query-sampled detection coverage and delay.
- Bound violations in validation mode (must be zero).

Detection delay is sampled at query points, not measured per packet. Memory excludes temporary query workspace, allocator metadata, input data and the exact oracle. Counter-budget equality does not imply byte-for-byte equality; measured owned bytes are also reported.

Outputs: `results/windowed/windowed_comparison.csv`, `results/windowed/RESULTS_SUMMARY.md`, and plots under `results/windowed/plots/`.

## Tests

```bash
make test
make smoke
make windowed-smoke
```

- `test_space_saving`: Stream-Summary invariants, replacement rule, error bounds and Top-k guarantee checks.
- `test_count_sketch`: signed-hash estimates, candidate Stream-Summary invariants and deterministic behavior.
- `test_windowed_space_saving`: ring rotation, exact-window differential bounds, unseen-ID upper bound and certification checks.
- `smoke`: short Figure 6 path with validation enabled.
- `windowed-smoke`: short windowed path with per-query bound validation enabled.

## Top-k tie handling

Ground Truth currently sorts by descending exact frequency and then ascending Flow ID. This makes every CSV reproducible, but the mathematical Top-k set is not unique when the kth and (k+1)th exact frequencies tie. The Figure 6 and fixed-counter CSVs record `observed_true_gap` so such cases can be identified. In particular, α=0 can have a tied or near-tied boundary; strict ID-based Precision/Recall may penalize a different but equally valid tied Top-k set. Before making a strong claim from tied-boundary cases, add/report tie-aware validity (all returned IDs must have frequency at least the kth threshold, and all IDs strictly above the threshold must be included).

## Important interpretation / limitations

1. Figure 6 is only partially reproduced: the project does not implement Probabilistic-InPlace and does not possess the original data generator, PRNG, seeds or complete CountSketch dimensions.
2. The Theorem-7 counter policy is based on the **noiseless Zipf frequency model** in the UCSB report. A finite IID sample can differ from that model; the selected capacity is not a guarantee that every random sample's exact Top-k will be found.
3. For α<1 the Theorem-7 bound is not applied; the full-alphabet fallback is exact counting and uses substantially more memory. Do not interpret this as an equal-memory comparison.
4. The fixed-1,000-counter run is supplementary and project-selected. One fixed budget is not enough to claim an optimal resource setting; a budget sensitivity sweep (e.g. 500/1,000/1,500/2,000) would strengthen the analysis.
5. The windowed implementation only supports block-boundary queries after a full window. It does not support arbitrary-time expiry or per-packet exact sliding-window queries.
6. A strict Top-k membership certificate can be unavailable even when the returned set is correct; “not certified” means the intervals overlap, not necessarily that the answer is wrong.
7. The windowed experiment's abrupt-shift workload uses disjoint old/new ID halves. This is a controlled synthetic workload, not a trace-driven evaluation. A trace-based or less extreme gradual-shift workload would strengthen external validity.
8. The reported memory is a deterministic estimate of persistent owned structures (or allocator live delta for the Figure 6 experiment), not peak RSS. State the metric when reporting memory.
9. Three random seeds are an initial replication level. For a final report, retain per-seed values and discuss variability; more seeds are advisable if runtime permits.
10. CountSketch uses a project-selected `7 × 8192` configuration. Although the paper mentions a target `δ=0.01` and a fitted factor, this implementation does not claim the chosen dimensions reproduce the original authors' exact parameterization or a formal δ guarantee.

## Related work for the recent-Top-k motivation

The project includes a reading/positioning note at `docs/RELATED_WORK.md`. Particularly relevant works are *Top-k Frequent Items and Item Frequency Tracking over Sliding Windows of Any Size* (2019), *Finding Top-k Elements in a Time-Sliding Window* (2011), *Memento: Making Sliding Windows Efficient for Heavy Hitters* (2018), and *Expiration Filter: Mining Recent Heavy Flows in High-Speed Networks* (2025). These works support the motivation and establish prior art; they are not all implemented as baselines in this project.

## References and attribution

- Metwally, A., Agrawal, D., El Abbadi, A. (2005). *Efficient Computation of Frequent and Top-k Elements in Data Streams*. ICDT 2005. DOI: `10.1007/978-3-540-30570-5_27`.
- Metwally, A., Agrawal, D., El Abbadi, A. (2005). UCSB Technical Report 2005-23. Used for algorithmic/theoretical details; its experimental setup differs from the ICDT conference Figure 6.
- Charikar, M., Chen, K., Farach-Colton, M. (2002). *Finding Frequent Items in Data Streams*. ICALP 2002.
- See `THIRD_PARTY_NOTICES.md` for source lineage and licensing conditions of the Cormode `frequent-items` design reference. Review those conditions before redistribution.

## Project layout

```text
include/      Space-Saving, CountSketch, windowed and exact-oracle interfaces
src/          algorithms and experiment drivers
tests/        unit and randomized differential tests
scripts/      result plotting utilities
docs/         detailed windowed method note
results/      CSV, configs, summaries and generated figures
```

## Expanded sensitivity study (revision 2)

The original Figure 6 and paired fixed-1000 result files are preserved as historical results. New results live under `results/sensitivity/` and include tie-aware metrics. No CountSketch dimensions, hash functions or parameters were changed. `make reproduce` recomputes Figure 6 including the new tie fields (published legacy CSVs predate that addition).

```sh
make -j4 all
make test
make sensitivity-ss      # 10M events, seven alphas, 3 seeds, 7 SS configurations
make sensitivity-window  # 2M events, 3 alphas, 3 seeds, 4 scenarios, 6 budgets
make sensitivity-plot
# or: make sensitivity
```

Full settings and metric definitions: `docs/SENSITIVITY_PROTOCOL.md`. The expanded sweeps deliberately include the full-alphabet cumulative counter capacity of 10,000. For windowing, 10,000 is the TOTAL across all blocks, not the per-block capacity; it need not give exact frequencies. Figure 6's alpha-specific policy remains unchanged, including its full-alphabet fallback for alpha < 1. Counter budgets are project parameters, not recovered original-paper settings.

### Periodic Reset: reproducing the omitted baseline

```sh
make test-periodic
make periodic-comparison
```

Output: `results/periodic/windowed_with_periodic.csv` and plots under
`results/periodic/plots/`. This independent output preserves previously
measured CSVs. The new columns `observed_window_fraction`,
`full_window_query_rate`, `aligned_query_count`, `aligned_*` allow reporting
both (i) all-query accuracy against true recent-W Top-k and (ii) the
at-full-boundary subset where the interval baseline represents exactly W.
The old sensitivity CSVs have not been automatically relabeled or fabricated.

[Periodic-reset experiment findings](results/periodic/RESULTS_SUMMARY.md).

## 文獻對照與小型視窗驗證

- [Sliding Window 文獻與現有方法對照](docs/SLIDING_WINDOW_PAPER_COMPARISON_ZH.md)
- [可手算案例與重跑方式](examples/windowed_validation/README.md)
- [驗證結果表](results/validation/WALKTHROUGH_ZH.md)
- [本次工作與驗證結論](docs/WINDOWED_VALIDATION_CONTRIBUTION_ZH.md)

```sh
make test windowed-walkthrough
```

## 中文結果分析與報告素材

[結果分析](docs/RESULTS_ANALYSIS_ZH.md)包含準確度、初步時間與認證率三組核心圖的判讀、比較口徑與口頭報告稿。數值由既有 Periodic CSV 重算，非新的效能量測。
