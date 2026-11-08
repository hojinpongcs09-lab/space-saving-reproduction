# Source lineage and third-party notices

## Original paper (algorithm specification)

Ahmed Metwally, Divyakant Agrawal, Amr El Abbadi, *Efficient Computation of Frequent and Top-k Elements in Data Streams*, ICDT 2005, LNCS 3363, pp. 398–412.

The paper defines Space-Saving (Figure 1), Stream-Summary (Section 3.1), frequency error bounds (Section 3.2), QueryTop-k / guarantee (Figure 4), and the synthetic-data evaluation (Figure 6).

## Uploaded `frequent-items-10(1).zip` (source design reference)

The uploaded archive contains the `LCU_*` linked-group implementation in `frequent-items/src/lossycount.cc` and `frequent-items/src/lossycount.h`.

**Original attribution notice, as written in that source:**

> Implementation by G. Cormode 2002, 2003. Original Code: 2002-11. This version: 2003-10.

The source itself states that the work is licensed under the **Creative Commons Attribution-NonCommercial License** and refers to:
https://creativecommons.org/licenses/by-nc/1.0/

This project keeps the LCU algorithm and two-level linked-group data-structure design as a reference, replaces the original C-style API with `SpaceSaving`, `StreamSummary`, `Bucket`, `Element`, `CountEstimate`, rewrites its container management as C++17, and adds independent `query_top_k`, tests, data generation, measurement and plotting code. This is a modified implementation, not a claim that it originated with the current authors. Respect the source's licensing conditions for use and redistribution.

The `byronknoll/space-saving` repository discussed separately was **not copied or merged** into this implementation. It was left out because its second-pass exact counting approach is different from this one-pass error-tracking implementation.

## CountSketch added to the current project

CountSketch was introduced by M. Charikar, K. Chen, and M. Farach-Colton,
*Finding Frequent Items in Data Streams*, ICALP 2002. The signed-hash and
median estimator, and the top-k candidate heap, follow the public algorithmic
specification. `src/count_sketch.cpp` and `include/count_sketch.hpp` are newly
written here, not copied from an external CountSketch implementation.

The experimental comparison in the Metwally et al. paper uses Stream-Summary
for CountSketch candidates rather than the original heap. The current code uses
the heap and does not claim exact reproduction of that runtime implementation.
