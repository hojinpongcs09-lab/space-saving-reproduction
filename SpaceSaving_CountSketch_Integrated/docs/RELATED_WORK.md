# Related work: recent Top-k and sliding-window heavy hitters

This note supports the motivation and positions the project against prior work. It does **not** claim that block windows, summary merging, or sliding-window heavy hitters are new.

## Directly related to recent Top-k

1. **Top-k Frequent Items and Item Frequency Tracking over Sliding Windows of Any Size** (Information Sciences, 2019). DOI: https://doi.org/10.1016/j.ins.2018.09.066
   - Directly studies Top-k frequent-item queries over sliding windows.
   - Use it to support the problem definition: recent popularity can differ from lifetime popularity, and maintaining frequency information over a moving window is a distinct streaming problem.

2. **Finding Top-k Elements in a Time-Sliding Window** (Evolving Systems, 2011). DOI: https://doi.org/10.1007/s12530-010-9020-z
   - Directly relevant prior work on Space-Saving-style summaries and Top-k queries over time-sliding windows.
   - Use it to position the project as an implementation/evaluation study, not as the first Space-Saving sliding-window method.

## Network-monitoring motivation

3. **Memento: Making Sliding Windows Efficient for Heavy Hitters** (ACM CoNEXT 2018; extended journal publication). https://arxiv.org/abs/1810.02899
   - Studies sliding-window heavy hitters in network monitoring and motivates why recent traffic changes matter for timely network operations.
   - It is threshold-based Heavy Hitter detection rather than exactly the same Top-k ranking problem in this project.

4. **Expiration Filter: Mining Recent Heavy Flows in High-Speed Networks** (Computer Networks, 2025). DOI: https://doi.org/10.1016/j.comnet.2024.111010
   - Directly studies recent heavy flows in high-speed networks and is useful for motivating recent-flow monitoring under resource constraints.
   - Treat it as related network-monitoring work, not as an implementation baseline unless its full method and experimental conditions are reproduced.

5. **Heavy Hitters in Streams and Sliding Windows** (IEEE INFOCOM 2016). DOI: https://doi.org/10.1109/INFOCOM.2016.7524364
   - Provides additional prior work on heavy hitters under stream and sliding-window models.

## How this project should position itself

- **Problem motivation:** use the recent Top-k literature to explain why a lifetime Top-k can become stale when traffic popularity changes.
- **Application motivation:** use network-heavy-flow literature to explain why recently dominant flows matter to network operators.
- **Prior-art boundary:** acknowledge that sliding-window Top-k and sliding-window heavy-hitter algorithms already exist, including Space-Saving-related approaches.
- **Project contribution:** present the work as a controlled implementation and empirical study of block-aligned Space-Saving summaries, conservative bound aggregation, and the accuracy/certification/overhead trade-off under a fixed counter budget. Do not claim to invent sliding-window Top-k or the underlying Space-Saving error bounds.

Before the final report, read the full papers and record the exact model, window semantics, memory model, and evaluation baselines used by each. Similar terminology does not necessarily mean identical problem definitions.

## Direct comparison matrix (revision 2)

| Reference | Task / mechanism | Difference from our experiment |
|---|---|---|
| Song, Liu, Ge & Ge, Information Sciences 475 (2019), 100–120, https://doi.org/10.1016/j.ins.2018.09.066 | Floating Top-k uses randomized levels and a compact tuple pool, allowing window sizes selected at query time. | We use a fixed-length packet-count window and block-aligned queries with deterministic frequency intervals; not an arbitrary-window solution. |
| Homem & Carvalho, Evolving Systems (2011), https://doi.org/10.1007/s12530-010-9020-z | Filtered Space-Saving with Sliding Windows (FSW), per Song et al.'s discussion of prior methods. | We keep original Space-Saving in fixed blocks and compare count-only vs interval aggregation; FSW is related but not reproduced. |
| Ben Basat et al., CoNEXT (2018), extended IEEE/ACM ToN (2022), https://doi.org/10.1109/TNET.2021.3132385 | Online threshold HH/HHH monitoring; motivated by timely network detection, including flood mitigation. | Our output is fixed-k ranking, not threshold/HHH or a network-wide deployment. We cannot transfer their detection improvements to our system. |
| Han, Huang, Sun, Liu & Chen, Computer Networks 258 (2025), 111010, https://doi.org/10.1016/j.comnet.2024.111010 | Expiration Filter combines filtering of small flows and self-cleaning stale records, designed for constrained switch hardware. Evaluates recent Top-k and HH. | Our CPU prototype expires whole blocks instead of self-cleaning individual records; no line-rate switch claim or direct performance comparison. |
| Ben Basat et al., INFOCOM (2016), https://doi.org/10.1109/INFOCOM.2016.7524364 | Statically allocated summary structures for stream/window HH with constant-time point queries. | Our focus is Top-k enumeration and certification; query complexity and metrics differ. |
| Agarwal et al., ACM TODS 38(4) (2013), https://doi.org/10.1145/2500128 | Formal mergeability and size/error guarantees for HH summaries, MG and SpaceSaving equivalence. | We aggregate intervals across retained blocks at query time; do not claim a new mergeable-summary theorem or fixed-size re-compressed summary. |

These sources establish importance and technical context, not experimental superiority of our implementation. The matrix is based on publisher abstracts/introduction and original project notes; in particular, FSW is characterized through Song et al.'s discussion rather than a verified full-text reconstruction. Read the complete papers before making detailed algorithmic comparisons or selecting new baselines.
