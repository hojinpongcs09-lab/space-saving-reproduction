# Windowed experiment: three-seed summary

Configuration: N=2,000,000; A=10,000; W=100,000; Top-20; total counters=1,600; query interval=50,000; Zipf α=1, 1.5, 2; three seeds; B=2/4/8. Synthetic project workload, not an original-paper reproduction.

Precision/Recall are macro-averaged over 39 common query points. Detection metrics are query-sampled and only apply to the phase-shift workload. Runtime is measured on the current execution environment; do not compare its absolute values to the 2005 paper. Memory is persistent owned bytes and excludes query workspace, input and exact oracle.

## Static Zipf

| Method | B | Precision | Recall | Membership certification | Avg. memory (B) | Avg. runtime (ms) |
|---|---:|---:|---:|---:|---:|---:|
| Bound-Aware-Block-Merge | 2 | 1.0000 | 1.0000 | 0.9829 | 205176 | 92.61 |
| Bound-Aware-Block-Merge | 4 | 1.0000 | 1.0000 | 0.9801 | 205464 | 88.84 |
| Bound-Aware-Block-Merge | 8 | 0.9979 | 0.9979 | 0.7721 | 206040 | 88.90 |
| Cumulative-SpaceSaving | — | 0.9895 | 0.9895 | 0.9972 | 204936 | 94.19 |
| Naive-Block-Merge | 2 | 1.0000 | 1.0000 | N/A | 205176 | 89.26 |
| Naive-Block-Merge | 4 | 1.0000 | 1.0000 | N/A | 205464 | 85.76 |
| Naive-Block-Merge | 8 | 0.9984 | 0.9984 | N/A | 206040 | 84.65 |

## Abrupt popularity shift

| Method | B | Precision | Recall | Membership certification | Detection coverage | Avg. delay (packets) | Avg. memory (B) |
|---|---:|---:|---:|---:|---:|---:|---:|
| Bound-Aware-Block-Merge | 2 | 1.0000 | 1.0000 | 0.9858 | 1.0000 | 0.0 | 205176 |
| Bound-Aware-Block-Merge | 4 | 1.0000 | 1.0000 | 0.9829 | 1.0000 | 0.0 | 205464 |
| Bound-Aware-Block-Merge | 8 | 0.9990 | 0.9990 | 0.8860 | 0.9952 | 252.5 | 206040 |
| Cumulative-SpaceSaving | — | 0.6845 | 0.6845 | 0.9715 | 0.4723 | 303888.9 | 204936 |
| Naive-Block-Merge | 2 | 1.0000 | 1.0000 | N/A | 1.0000 | 0.0 | 205176 |
| Naive-Block-Merge | 4 | 1.0000 | 1.0000 | N/A | 1.0000 | 0.0 | 205464 |
| Naive-Block-Merge | 8 | 0.9997 | 0.9997 | N/A | 1.0000 | 252.5 | 206040 |

## Validation

- Total CSV rows: 126 (2 scenarios × 3 Zipf α × 3 seeds × 7 method configurations).
- Maximum observed bound violations: 0.
- Every query in validation mode checked exact frequencies against the merged per-flow bounds and the unseen-ID upper bound.
- Any reported membership certification was checked against the exact Top-20 set; any order certification was checked against exact frequencies.
- `Naive-Block-Merge` does not provide a certificate by design; its certification rate is recorded as zero/not applicable.
- A zero detection delay means the method returned a newly true Top-20 flow at the same sampled query point where the exact oracle first showed it in Top-20. It does not mean zero per-packet processing delay.

## Interpretation

- Under these synthetic settings, B=2 and B=4 achieved perfect or near-perfect recent Top-20 precision/recall; B=8 showed a small accuracy/certification reduction in some cases.
- The bound-aware method provides a verifiable membership certificate on most query points, unlike the naive count-only merge.
- The cumulative baseline loses accuracy after the popularity shift because it retains old-history counts.
- These results are preliminary evidence for this workload and parameter set, not a general guarantee that block-based merging always outperforms other sliding-window methods.
- The exact results and per-seed rows are in `windowed_comparison.csv`; plots and aggregated mean/std are in `plots/`.
