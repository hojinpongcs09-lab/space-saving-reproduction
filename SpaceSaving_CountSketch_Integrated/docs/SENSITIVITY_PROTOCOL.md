# Expanded experiment protocol

## Rationale

Keep the alpha-specific policy unchanged. α<1 uses the full alphabet because the chosen theorem-based allocation formula is not applicable there; α=1 uses the capped formula. These are independent replication choices, not recovered Figure 6 settings. Forcing a smaller fallback would alter the policy and obscure its limitations. Sweep fixed budgets across the same 10,000-ID universe instead.

| Study | Fixed budgets | Other conditions |
|---|---|---|
| Figure 6 Space-Saving only | 500, 1000, 2000, 4000, 8000, 10000 | 10M events, k=100, α=0,.5,1,1.5,2,2.5,3, 3 seeds; compare with existing α-specific policy |
| Windowed | 400, 800, 1600, 3200, 6400, 10000 | 2M events, W=100000, k=20, α=1,1.5,2, B=2,4,8, 3 seeds, 4 scenarios, common Q=50000 |

Window budgets are divisible by all B. The original M=1600 remains the reference. At M=10000 and B=8, each block has only 1250 counters; a full-alphabet total budget does not imply exact block tracking. Under aligned queries, summed frequency intervals have worst-case width at most B*W/M (each missing or stored interval width <= minimum counter in its block). This motivates studying multiple M and B rather than claiming an optimal M.

## Workloads

All comparisons at a given scenario, α and repeat reuse the same stream and exact oracle; seed is independent of M and B. Static: Zipf over full alphabet. Abrupt shift: old/new disjoint halves of alphabet; shift after event 1,050,000. Gradual: linearly increase probability of sampling new half from 0 to 1 over W events after shift. Overlap: abrupt transition but retain first floor(k/2) rank IDs from old half (rank overlap is intentional; actual sampled Top-k membership may differ). Dynamic halves have 5,000 active IDs each, unlike static full alphabet; do not interpret cross-scenario differences as purely due to temporal shifts. Synthetic traffic only; no real trace evidence.

## Metrics

Original deterministic precision/recall retained. Let q=min(k, number of positive-frequency IDs), f_q the boundary frequency, H all items strictly above f_q. For returned list R, count distinct positive-frequency hits:

h = |R intersect H| + min(|{x in R: f(x)=f_q}|, q-|H|).

Tie-aware precision=h/|R|, recall=h/q, valid iff |R|=q and h=q. Duplicate or zero-frequency returns do not count. This is maximum overlap with any valid exact Top-k set, not permission to omit mandatory items. Strict separation certification is conservative: even a valid tied answer may not certify. Certification applies only to Bound-Aware recent-window results; Cumulative certifies a different history, Naive has no cert, hence N/A in expanded CSV. Order certification refers to internal ordering of returned elements, independently of membership.

Query-sampled delay: for each new-half ID ever entering exact Top-k on a query, compare earliest such query with first subsequent query on which it is both true and returned. Undetected entries count as misses; if no detection, mean delay is NaN; static/no eligible entrants: coverage and delay N/A. Not per-packet delay or recurring entry episodes. The updated rule excludes stale detections after the ID leaves true Top-k, so old historical delay numbers are not directly comparable.

## Measurement limitations

Persistent owned bytes (excluding allocator metadata, temporary merge workspace, raw input, and exact oracle), as in previous project. Equal counters are not the same as equal bytes. Separate update and query wall times; per-event timer overhead is present in window benchmark. Window naive/bound share the same update path but time their queries separately. Use clean sequential `make sensitivity` on target Linux for report-quality timings; container measurements can be noisy, especially if concurrently executed. Three seeds characterize the tested synthetic cases only. Cumulative remains a historical reference, not a same-window algorithm. Exact sliding window is the oracle, not a resource-matched baseline. No new published methods are implemented. Figure 6 remains partial replication (Probabilistic-InPlace omitted).
