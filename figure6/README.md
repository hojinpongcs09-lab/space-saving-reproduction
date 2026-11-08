# Figure 6 Reproduction — Space-Saving + CountSketch

**Paper:** Ahmed Metwally, Divyakant Agrawal, Amr El Abbadi, *Efficient Computation of Frequent and Top-k Elements in Data Streams*, ICDT 2005, pp. 398–412.

This package adds **CountSketch** as a second comparison algorithm, while preserving the prior, tested **Space-Saving / Stream-Summary** baseline. It generates four Figure 6-style grouped bar charts with values above every bar:

1. (a) Runtime (ms), update + Top-k query.
2. (b) Top-k Precision.
3. (c) Top-k Recall.
4. (d) Data-structure Memory (bytes).

**Scope:** two algorithm series, **not** Probabilistic-InPlace. These are *new measurements inspired by the paper*, not the original authors' 2005 run. Do not claim the charts reproduce the original numerical values.

## Quick start

Requires **g++ with C++17**, GNU Make, **Python 3**, `matplotlib` and `numpy` (plotting only). Run from project root:

```bash
make -j4
make test
make compare-smoke        # quick correctness + end-to-end comparison
make compare              # full N=10,000,000 × 7 Zipf alpha points
```

Outputs:

- `results/figure6_comparison.csv` — numeric measurements, **two rows per alpha**.
- `results/comparison/figure6_comparison.png` — 2×2 bar chart (black Space-Saving, blue CountSketch).
- `results/comparison/figure6_comparison.svg` — vector version.
- `results/comparison/figure6_{runtime,precision,recall,memory}.{png,svg}` — separate panels.

Re-plot **without rerunning** the experiment:

```bash
make compare-plot
```

### Memory-matched secondary comparison

```bash
make compare-equal-memory
```

This runs the same dataset, but changes CountSketch to 7 × 2048 counters. On this 64-bit build the estimated owned bytes are **128,136 B (Space-Saving)** and **120,768 B (CountSketch)**; this is **roughly** matched, not exactly equal memory. Results: `results/figure6_equal_memory.csv`, `results/equal_memory/figure6_comparison.png`.

### Keep the original single-algorithm reproduction

```bash
make reproduce            # previous Space-Saving-only experiment
make plot                 # replot previous results
make calibrate            # offline oracle-assisted SS budget sweep
```

The detailed original README is retained at [`docs/SPACE_SAVING_ONLY.md`](docs/SPACE_SAVING_ONLY.md).

## Experiment details

The two algorithms receive **identical, pre-generated event IDs** for each `(alpha, seed)`; this prevents different random inputs from confounding the comparison. Exact Ground Truth is built separately and is excluded from timing and algorithm memory.

| Setting | Full comparison default | Provenance |
|---|---|---|
| N | 10,000,000 events | Paper Section 5 |
| Zipf alpha | 0, 0.5, 1, 1.5, 2, 2.5, 3 | Paper Figure 6 |
| Top-k | 100 | Paper Figure 6 |
| Alphabet | 10,000 | Our explicit assumption |
| Space-Saving M | 1,000 counters | Our explicit assumption |
| CountSketch depth | 7 independent rows | Our explicit assumption |
| CountSketch width | 8192 buckets/row | Our explicit assumption |
| CountSketch candidate set | 100 entries | Original CountSketch Top-k scheme |
| Seed | 2026 (derived per alpha) | Our explicit assumption |
| Repeats | 1 (set `--repeats 5` for uncertainty studies) | Our explicit assumption |

Under **both** algorithms, `Precision = |reported∩exact_topk| / |reported|` and `Recall = |reported∩exact_topk| / k`. Both implementations return exactly k candidates (after at least k unique events), so here Precision and Recall are **necessarily equal**. Original Figure 6's Probabilistic-InPlace can output more candidates, so its Precision and Recall need not match.

`Runtime` measures the complete scan of the supplied events **plus the Top-k query**, not data generation, sorting Ground Truth, program startup, validation, or charting. Times are dependent on CPU/compiler and **not comparable as absolute ms** with a 2005 Pentium IV.

`Memory` means the estimated owned data-structure allocation: SS Element, Bucket, Hash arrays; CS sketch matrix, hash parameter array, candidate heap, and approximate unordered-map bucket/node bytes. It excludes input vector, truth table, generator, result vector, OS RSS, and allocator overhead. For **unordered_map**, the node bytes are an estimate rather than allocator-reported bytes. Both fixed-parameter series naturally produce constant memory across alpha. Do not compare the plot's values directly to original Figure 6(d) without matching the original's parameters, allocator and hardware ABI.

### What exactly was implemented?

- **Space-Saving**: one-pass update `count=min+1; error=min` on replacement, Hash Table + frequency-ordered Stream-Summary bucket linked lists; `query_top_k` + paper-style membership/order flags. From the supplied `frequent-items-10` linked-group LCU design and the current paper.
- **CountSketch**: per-row signed hash bucket increments, frequency estimate = median of signed row counts, and a **top-k candidate min-heap** with an index hash map; on a candidate's arrival its stored estimated count increases by 1, otherwise a new item is considered for replacing the minimum candidate. This follows the algorithmic outline in **Charikar, Chen, Farach-Colton (ICALP 2002)**. The CountSketch code here was written separately; it was **not copied** from a Count-Min Sketch implementation.
- The paper's *Figure 6 CountSketch implementation* reports replacing the original top-k heap with a fixed-size Stream-Summary and using a specific median implementation (Hoare median-of-three). **This project instead uses the original candidate heap and `std::nth_element`**. This difference affects runtime and data structure overhead, so results are a **mechanism-level comparison, not a bit-for-bit duplication of the paper's CountSketch system**.
- The CountSketch `estimate()` returns a signed estimate, not a strict upper/lower bound; no Space-Saving-style guaranteed Top-k flag is claimed for it.

### Adjust configuration yourself

```bash
./build/figure6_comparison \
  --events 1000000 --alphabet 10000 --topk 100 \
  --counters 1000 --cs-width 4096 --cs-depth 7 \
  --alphas 0,0.5,1,1.5,2,2.5,3 \
  --seed 2026 --repeats 3 --validate \
  --output results/my_comparison.csv

python3 scripts/plot_figure6_comparison.py \
  --csv results/my_comparison.csv --outdir results/my_comparison
```

CountSketch width must be power-of-two; depth must be odd (1–31). Parameters should be held fixed throughout a single comparison plot. Try `--repeats 5` with independently derived seeds to assess variance in CountSketch accuracy.

## Directory map

```text
include/space_saving.hpp            Original Space-Saving baseline
include/count_sketch.hpp            Added CountSketch API
src/space_saving.cpp                Original Space-Saving implementation
src/count_sketch.cpp                Signed-hash sketch + top-k indexed heap
src/figure6_experiment.cpp          Previous SS-only and oracle calibration
src/figure6_comparison.cpp          One shared stream, two algorithms, CSV
scripts/plot_figure6.py            Previous SS-only plotter
scripts/plot_figure6_comparison.py Added paper-style grouped-bar plotter
tests/test_space_saving.cpp         Original correctness tests
tests/test_count_sketch.cpp         Added CountSketch tests
results/comparison/                  Full 2-algorithm charts
results/equal_memory/                Roughly memory-matched secondary charts
results/figure6_comparison.csv     Full measured records
results/figure6_equal_memory.csv   Secondary measured records
THIRD_PARTY_NOTICES.md              Source attribution and licensing
```

## Academic / reproducibility limitations

1. `frequent-items-10`'s original LCU notice is **Creative Commons Attribution–NonCommercial**; keep attribution and review course-use requirements. Details in `THIRD_PARTY_NOTICES.md`.
2. CountSketch itself was introduced by Charikar, Chen, and Farach-Colton (ICALP 2002); its implementation here is original code developed for this project, not the 2005 authors' comparison code.
3. Full reproduction of the original paper's Figure 6 requires matching its **hidden choices of CountSketch width, rows, candidate maintenance, memory accounting, input generation, random seeds, and runtime environment**. This package does **not** assert that.
4. Runtime comparisons between algorithm families can be sensitive to implementation quality and memory allocations. A strong course report should show at least 5 repeats for statistical stability and a **memory budget sweep**, not merely one parameter setting.
