# Periodic-Reset Space-Saving baseline: completed default experiment

## Scope and provenance

- This is an **additional baseline** to the already integrated cumulative and B=2/4/8 naive/bound-aware methods. No changes to the original Figure 6 / CountSketch algorithm.
- N=2,000,000, A=10,000, W=100,000, K=20, M=1,600, Q=50,000, alpha=1/1.5/2, three seeds, static/shift/gradual/overlap: 36 paired runs x 8 configurations = 288 rows.
- Queries t=100K,150K,200K,...,2M. At t=nW, query the old summary first; reset before receiving packet nW+1. At t=nW+W/2 the periodic summary contains only W/2 packets, which is deliberately compared to an exact latest-W window.
- `tie_recall` measures sampled Top-20 accuracy against the full recent-W oracle at **all** common query positions. `aligned_tie_recall` uses only the 20 queries at t divisible by W (where Periodic Reset covers the full W). The two metrics have different sampling denominators.
- `observed_window_fraction`: 0.75641026 for periodic reset (20 full + 19 half out of 39), 1.0 for the others. This fraction measures the length of the represented time range, **not** the fraction of actual Flow counts correctly recovered.
- Periodic/Naive/Cumulative: no certified *recent-W* Top-k membership; certification fields N/A. Bounded merged summaries: certification valid on their aligned full-window queries.
- Query-sampled detection delay uses first eligible queried Top-k appearances, not true packet-by-packet detection. Unseen episodes and ties require careful interpretation.
- Runtime measured locally with per-update timing overhead; rerun sequentially on target machine for report quality. Existing 1512-row six-budget sensitivity file remains **historical** and does not yet contain periodic rows. `make sensitivity-window` regenerates it with the periodic method and expanded aligned fields.

## Default M=1600: mean across all four workloads, three alphas and three seeds

| Method | Tie-aware Recall (all queries) | Tie-aware Recall (only nW boundaries) | Mean fraction of W represented |
|---|---:|---:|---:|
| Bound-Aware-Block-Merge B=2 | 100.0000% | 100.0000% | 100.0000% |
| Bound-Aware-Block-Merge B=4 | 100.0000% | 100.0000% | 100.0000% |
| Bound-Aware-Block-Merge B=8 | 99.8789% | 99.8403% | 100.0000% |
| Cumulative-SpaceSaving B=0 | 79.1346% | 79.2153% | 100.0000% |
| Naive-Block-Merge B=2 | 100.0000% | 100.0000% | 100.0000% |
| Naive-Block-Merge B=4 | 99.9964% | 100.0000% | 100.0000% |
| Naive-Block-Merge B=8 | 99.9288% | 99.9236% | 100.0000% |
| Periodic-Reset-SpaceSaving B=0 | 99.1168% | 100.0000% | 75.6410% |

## Abrupt Shift: mean across three alphas, three seeds

| Method | Tie-aware Recall | Detection coverage | Mean delay among detected (packets) |
|---|---:|---:|---:|
| Cumulative-SpaceSaving B=0 | 68.4473% | 47.2311% | 303888.9 |
| Periodic-Reset-SpaceSaving B=0 | 99.2165% | 99.0119% | 1827.8 |
| Naive-Block-Merge B=4 | 100.0000% | 100.0000% | 0.0 |
| Bound-Aware-Block-Merge B=4 | 100.0000% | 100.0000% | 0.0 |

## Sanity checks

- `make test`: original Space-Saving, CountSketch, Windowed, Tie-aware and Periodic-Reset unit tests pass.
- `python scripts/check_periodic_csv.py results/periodic/windowed_with_periodic.csv`: 36 paired runs, 288 rows, query alignment and reset-coverage checks pass.
- Total Bound-Aware `bound_violations`: 0 in this run.
- `.csv` contains `aligned_query_count` and both overall/aligned metrics for every method; no result backfilled into older CSVs.

## Reproduce

```bash
make -j4 all
make test
make periodic-comparison
```
