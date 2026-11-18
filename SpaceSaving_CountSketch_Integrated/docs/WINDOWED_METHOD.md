# Block-windowed Space-Saving: implementation and guarantees

## 1. Scope

This extension preserves the existing cumulative `SpaceSaving` implementation and adds a block-aligned windowed wrapper. It does not change the Figure 6 Space-Saving or CountSketch implementations.

The windowed method uses `B` independent Space-Saving summaries. The packet-count window has length `W`, each block contains `W/B` events, and the total counter budget `M` is divided evenly (`M/B` counters per block). The implementation requires `W % B == 0` and `M % B == 0`.

A query is valid only when:

- at least `W` events have been processed;
- the current event position is a block boundary; and
- all `B` retained summaries represent complete blocks.

At the first event of a new block, the ring slot holding the oldest summary is reinitialized. Thus the data structure is intentionally **block-aligned**, not an exact arbitrary-position sliding window. For `W=100,000`, `B=4`, the method queries only after complete 25,000-event blocks. The experiment's common query interval is 50,000, which aligns with `B=2,4,8`.

## 2. Per-block frequency bounds

For a monitored element `x` in block `b`, Space-Saving provides:

`c_b(x) - e_b(x) <= f_b(x) <= c_b(x)`.

For an element absent from a block summary:

- if the summary is not full, its frequency in that block is zero;
- if the summary is full, its frequency is at most the block's minimum counter `min_b`.

Define the per-block interval:

- tracked: `L_b(x) = c_b(x) - e_b(x)`, `U_b(x) = c_b(x)`;
- untracked in a full summary: `L_b(x) = 0`, `U_b(x) = min_b`;
- untracked in a non-full summary: `L_b(x) = U_b(x) = 0`.

Since the blocks are disjoint portions of the window, their true frequencies add:

`L(x) = sum_b L_b(x)`

`U(x) = sum_b U_b(x)`

Therefore `L(x) <= f_window(x) <= U(x)` for every element in the union of the block summaries.

For an element absent from **all** block summaries, a conservative upper bound is:

`U_unseen = sum_b (min_b if block b is full else 0)`.

This is the value used to account for IDs outside the candidate union during Top-k certification.

## 3. Bound-aware candidate ranking and certification

The implementation ranks the union of block candidates by the midpoint of its aggregate interval:

`estimate(x) = L(x) + floor((U(x) - L(x))/2)`.

The midpoint is a deterministic point estimate; it is not itself the correctness guarantee.

Let `S` be the `k` candidates returned by this midpoint ranking. The returned membership is certified only if the strict separation condition holds:

`min_{x in S} L(x) > max(max_{y not in S} U(y), U_unseen)`.

Strict separation is used so that a tie at the kth boundary is not reported as a uniquely certified membership set. If fewer than `k` candidates exist, membership is certified only when `U_unseen == 0`.

The implementation separately reports `order_certified`, which certifies the order **among the returned candidates only** by requiring each returned candidate's lower bound to exceed every later returned candidate's upper bound. Membership certification and order certification are different claims.

## 4. Baselines

- `Cumulative-SpaceSaving`: one Space-Saving summary with `M` counters over the entire stream. It is compared against recent-window ground truth to show the effect of retaining old history.
- `Naive-Block-Merge`: sum stored counts for each candidate across blocks; a missing per-block candidate contributes zero. This is a deliberately simple merge baseline and carries no valid merged frequency interval.
- `Bound-Aware-Block-Merge`: aggregate per-block lower/upper bounds, rank by interval midpoint, and report membership/order certification.
- `ExactSlidingWindow`: exact FIFO + hash-map counts used only as the evaluation oracle. Its memory and runtime are excluded from approximate-method measurements.

The periodic-reset control is not included in this implementation's primary comparison because a single reset summary every `W` packets does not represent the latest `W` packets at intermediate common query points (e.g. at `150K` when `W=100K`). It can be added as a separate tumbling-window control queried only at its own interval boundaries.

## 5. Default experiment assumptions

These are project choices, not parameters specified by the ICDT 2005 Figure 6 paper:

| Parameter | Default |
|---|---:|
| Stream length | 2,000,000 events |
| Flow-ID alphabet | 10,000 |
| Window `W` | 100,000 events |
| Top-k | 20 |
| Total Space-Saving counters `M` | 1,600 |
| Block counts `B` | 2, 4, 8 |
| Per-block counters | 800, 400, 200 |
| Query interval | 50,000 events |
| Zipf alpha | 1.0, 1.5, 2.0 |
| Repeats | 3 seeds |
| Base seed | 2026 |

Two workloads are generated:

- `static`: one IID Zipf distribution over all IDs `1..A`.
- `shift`: before `shift_at = N/2 + W/2`, sample from the first half of the IDs; after that point, sample from the disjoint second half. The first new-phase event is packet `shift_at + 1`.

The generator, alphabet, seeds, phase construction, and query cadence are reproducibility choices of this project, not claimed to be the original paper's data generator.

## 6. Metrics and interpretation

- Precision / Recall: macro-average over common query points against exact Top-k of the latest `W` events.
- Certification rate: fraction of query points where the returned membership set is strictly certified by the merged intervals.
- Order certification rate: fraction of query points where the returned candidates' internal order is certified.
- Average bound width: mean `U-L` across returned candidates and query points.
- Detection coverage: in the shift workload, fraction of new-phase IDs observed entering exact Top-k at a query point that are subsequently returned by the method.
- Detection delay: query-sampled packet distance from the first query point where a new-phase ID appears in exact Top-k to the first subsequent query point where the method returns it. It is not a per-packet alert latency.
- Update/query time: approximate algorithm only; excludes data generation and exact ground-truth maintenance.
- Memory bytes: persistent owned data-structure bytes. It excludes temporary query workspace, allocator metadata, input data, and exact ground truth.
- Bound violations: validation-mode count of any exact per-ID window frequency outside its merged interval, or any untracked ID exceeding `U_unseen`. A correct run should report zero.

A certification rate of zero does not mean the returned Top-k is wrong; it means the available intervals were not sufficiently separated to prove membership. Conversely, certification must never be claimed when the exact validation disagrees.

## 7. Build, test, and run

```bash
make -j4 all
make test
make windowed-smoke
make windowed-experiment
make windowed-plot
```

The smoke target uses a small stream and enables bound validation. The full target uses the defaults above and writes `results/windowed/windowed_comparison.csv`.
