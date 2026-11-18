# Expanded sensitivity results (measured)

Full SS sweep: 147 rows (7 alphas × 3 repeats × 7 configurations).
Full Window sweep: 1,512 rows (6 budgets × 4 scenarios × 3 alphas × 3 repeats × 7 configurations). All bounds validation checks passed (zero violations). All certified memberships and returned orders were checked against the exact oracle. Tables below average per-run metrics; raw per-seed records and standard deviations are included in CSVs. Results concern synthetic traffic only.

## Space-Saving-only budget sweep: mean tie-aware Recall

| alpha | M=500 | 1000 | 2000 | 4000 | 8000 | 10000 | Alpha-specific |
|---:|---:|---:|---:|---:|---:|---:|---:|
|0|0.0267|0.0167|0.0167|0.0133|0.0367|1|1|
|0.5|0.4167|0.6367|0.9967|1|1|1|1|
|1|0.9533|1|1|1|1|1|1|
|1.5|1|1|1|1|1|1|1|
|2|1|1|1|1|1|1|1|
|2.5|1|1|1|1|1|1|1|
|3|1|1|1|1|1|1|1|

At alpha=0, a budget smaller than the alphabet does not guarantee good Top-100 precision. The true ordering reflects small random count differences between nearly equally likely IDs; exact ties are accounted for but near-ties are not treated as ties. At alpha=0.5, this experiment shows a steep improvement between M=1000 and M=4000. For alpha>=1.5, all tested fixed budgets reach perfect sampled recall; that is not a universal accuracy guarantee. The alpha-specific policy includes exact full-alphabet configurations for alpha<=1 and remains an independent policy, not the paper's recovered allocations.

## Windowed: B=8, mean across four scenarios, three alphas and three seeds

| Total M | Naive tie Recall | Bound-aware tie Recall | Bound-aware certified membership |
|---:|---:|---:|---:|
|400|0.9178|0.9373|0.1403|
|800|0.9736|0.9844|0.6360|
|1600|0.9993|0.9988|0.8469|
|3200|1|1|0.9793|
|6400|1|1|0.9843|
|10000|1|1|0.9865|

At M=400/800, the midpoint-based interval ranking can outperform naive count-only aggregation in this B=8 experiment. At M=1600 the naive average is slightly higher. Neither uniformly dominates. The expanded sweep reveals an accuracy/resource/certification trade-off obscured by the earlier high-accuracy workload at M=1600. Even an empirically valid Top-k may remain uncertified because frequency intervals overlap or the kth frequencies tie. Certification is membership only (strict separation); the original cumulative certification is inapplicable to the recent window and is now N/A.

At M=1600, B=4, the bound-aware method reaches mean tie-aware Recall=1 in each scenario. Its certification rates are 0.9801 (static), 0.9829 (shift), 0.9772 (gradual) and 0.9772 (overlap). Query-sampled delay cannot establish packet-by-packet latency. Delay estimates are conditional on successful detections; coverage must be read alongside them.

Tie-aware recall differs from deterministic recall in 1 of 147 SS rows and 40 of 1512 window rows. Difference counts refer to aggregated run records, not numbers of individual query instances. Do not infer broad accuracy superiority or statistical significance from these means alone.

## Reproducibility caveat

Expanded runs were completed locally; some timing measurements were collected while other independent experiments ran. Hence accuracy, certification and structural-byte results are useful, but runtimes should be rerun sequentially on a common team machine before final performance claims. Existing Figure 6/paired 1000-budget results remain historical; CountSketch was not rerun for the full expanded experiment because this is a Space-Saving-only sensitivity study. The new tie metrics are supported for CountSketch and smoke-tested; `make reproduce` produces an updated full Figure 6 CSV if needed.
