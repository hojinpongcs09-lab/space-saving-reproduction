# 由原始 CSV 重算的報告表格

來源：[`windowed_with_periodic.csv`](../windowed_with_periodic.csv)。

來源 SHA-256：`ea85d03024ccf3490018499e3560fedeb106480263c993370cd705cb2ede41ae`

每個情境／方法為 3 個 alpha × 3 個種子，共 9 筆 run 等權平均。
時間為每次完整 run 的總更新／總查詢時間，不是單筆延遲；不是正式效能結論。

| 情境 | 方法 | B | Tie Recall % | Aligned Tie Recall % | 認證率 % | 更新 ms | 查詢 ms |
|---|---|---:|---:|---:|---:|---:|---:|
| gradual | Bound-Aware-Block-Merge | 2 | 100.0000 | 100.0000 | 98.2906 | 80.6424 | 7.8516 |
| gradual | Bound-Aware-Block-Merge | 4 | 100.0000 | 100.0000 | 97.7208 | 78.2234 | 7.7830 |
| gradual | Bound-Aware-Block-Merge | 8 | 99.8860 | 99.8333 | 85.1852 | 76.9888 | 8.6332 |
| gradual | Cumulative-SpaceSaving | 0 | 69.5584 | 69.5278 | N/A | 89.9372 | 2.9270 |
| gradual | Naive-Block-Merge | 2 | 100.0000 | 100.0000 | N/A | 80.6424 | 5.7123 |
| gradual | Naive-Block-Merge | 4 | 99.9858 | 100.0000 | N/A | 78.2234 | 4.9197 |
| gradual | Naive-Block-Merge | 8 | 99.9145 | 99.9444 | N/A | 76.9888 | 4.8188 |
| gradual | Periodic-Reset-SpaceSaving | 0 | 98.8177 | 100.0000 | N/A | 80.4000 | 2.2569 |
| overlap | Bound-Aware-Block-Merge | 2 | 100.0000 | 100.0000 | 98.2906 | 80.6056 | 7.8730 |
| overlap | Bound-Aware-Block-Merge | 4 | 100.0000 | 100.0000 | 97.7208 | 77.9639 | 7.9486 |
| overlap | Bound-Aware-Block-Merge | 8 | 99.9145 | 99.8889 | 87.7493 | 77.2592 | 8.9030 |
| overlap | Cumulative-SpaceSaving | 0 | 79.5869 | 79.8333 | N/A | 88.8656 | 2.9445 |
| overlap | Naive-Block-Merge | 2 | 100.0000 | 100.0000 | N/A | 80.6056 | 5.9016 |
| overlap | Naive-Block-Merge | 4 | 100.0000 | 100.0000 | N/A | 77.9639 | 5.1679 |
| overlap | Naive-Block-Merge | 8 | 99.9715 | 99.9722 | N/A | 77.2592 | 4.7416 |
| overlap | Periodic-Reset-SpaceSaving | 0 | 99.2165 | 100.0000 | N/A | 79.7588 | 2.1686 |
| shift | Bound-Aware-Block-Merge | 2 | 100.0000 | 100.0000 | 98.5755 | 78.9668 | 8.2035 |
| shift | Bound-Aware-Block-Merge | 4 | 100.0000 | 100.0000 | 98.2906 | 77.4880 | 7.6744 |
| shift | Bound-Aware-Block-Merge | 8 | 99.9145 | 99.8889 | 88.6040 | 76.1504 | 8.4630 |
| shift | Cumulative-SpaceSaving | 0 | 68.4473 | 68.5556 | N/A | 86.6843 | 2.9070 |
| shift | Naive-Block-Merge | 2 | 100.0000 | 100.0000 | N/A | 78.9668 | 5.6043 |
| shift | Naive-Block-Merge | 4 | 100.0000 | 100.0000 | N/A | 77.4880 | 4.9622 |
| shift | Naive-Block-Merge | 8 | 99.9715 | 99.9722 | N/A | 76.1504 | 4.8699 |
| shift | Periodic-Reset-SpaceSaving | 0 | 99.2165 | 100.0000 | N/A | 78.9410 | 2.1937 |
| static | Bound-Aware-Block-Merge | 2 | 100.0000 | 100.0000 | 98.2906 | 80.1879 | 7.9696 |
| static | Bound-Aware-Block-Merge | 4 | 100.0000 | 100.0000 | 98.0057 | 77.3142 | 7.8557 |
| static | Bound-Aware-Block-Merge | 8 | 99.8006 | 99.7500 | 77.2080 | 76.5510 | 8.7529 |
| static | Cumulative-SpaceSaving | 0 | 98.9459 | 98.9444 | N/A | 87.1220 | 2.9028 |
| static | Naive-Block-Merge | 2 | 100.0000 | 100.0000 | N/A | 80.1879 | 5.9455 |
| static | Naive-Block-Merge | 4 | 100.0000 | 100.0000 | N/A | 77.3142 | 4.9464 |
| static | Naive-Block-Merge | 8 | 99.8575 | 99.8056 | N/A | 76.5510 | 4.8546 |
| static | Periodic-Reset-SpaceSaving | 0 | 99.2165 | 100.0000 | N/A | 79.0267 | 2.2405 |

## Shift 每個 alpha 的三種子變異

平均 ± 樣本標準差（n=3）；不把跨 alpha 差異當作隨機變異，也不是信賴區間。

| 方法 | B | alpha | Tie Recall % | 認證率 % | 查詢總時間 ms |
|---|---:|---:|---|---|---|
| Bound-Aware-Block-Merge | 2 | 1 | 100.0000 ± 0.0000 | 97.4359 ± 2.5641 | 12.0574 ± 3.5042 |
| Bound-Aware-Block-Merge | 2 | 1.5 | 100.0000 ± 0.0000 | 99.1453 ± 1.4804 | 9.6927 ± 0.1366 |
| Bound-Aware-Block-Merge | 2 | 2 | 100.0000 ± 0.0000 | 99.1453 ± 1.4804 | 2.8604 ± 0.0684 |
| Bound-Aware-Block-Merge | 4 | 1 | 100.0000 ± 0.0000 | 96.5812 ± 1.4804 | 10.3082 ± 0.5501 |
| Bound-Aware-Block-Merge | 4 | 1.5 | 100.0000 ± 0.0000 | 99.1453 ± 1.4804 | 8.8489 ± 0.1460 |
| Bound-Aware-Block-Merge | 4 | 2 | 100.0000 ± 0.0000 | 99.1453 ± 1.4804 | 3.8660 ± 0.0345 |
| Bound-Aware-Block-Merge | 8 | 1 | 99.7436 ± 0.1282 | 67.5214 ± 2.9608 | 11.5542 ± 0.5818 |
| Bound-Aware-Block-Merge | 8 | 1.5 | 100.0000 ± 0.0000 | 99.1453 ± 1.4804 | 8.5575 ± 0.2488 |
| Bound-Aware-Block-Merge | 8 | 2 | 100.0000 ± 0.0000 | 99.1453 ± 1.4804 | 5.2775 ± 0.0628 |
| Cumulative-SpaceSaving | 0 | 1 | 66.3248 ± 0.2669 | N/A | 3.1472 ± 0.1104 |
| Cumulative-SpaceSaving | 0 | 1.5 | 69.0598 ± 0.1480 | N/A | 3.1678 ± 0.0865 |
| Cumulative-SpaceSaving | 0 | 2 | 69.9573 ± 0.1480 | N/A | 2.4059 ± 0.0373 |
| Naive-Block-Merge | 2 | 1 | 100.0000 ± 0.0000 | N/A | 7.3831 ± 0.2634 |
| Naive-Block-Merge | 2 | 1.5 | 100.0000 ± 0.0000 | N/A | 6.8149 ± 0.1270 |
| Naive-Block-Merge | 2 | 2 | 100.0000 ± 0.0000 | N/A | 2.6149 ± 0.1024 |
| Naive-Block-Merge | 4 | 1 | 100.0000 ± 0.0000 | N/A | 6.4852 ± 0.2866 |
| Naive-Block-Merge | 4 | 1.5 | 100.0000 ± 0.0000 | N/A | 5.6245 ± 0.1733 |
| Naive-Block-Merge | 4 | 2 | 100.0000 ± 0.0000 | N/A | 2.7768 ± 0.1651 |
| Naive-Block-Merge | 8 | 1 | 99.9145 ± 0.0740 | N/A | 6.1476 ± 0.1808 |
| Naive-Block-Merge | 8 | 1.5 | 100.0000 ± 0.0000 | N/A | 5.1592 ± 0.3038 |
| Naive-Block-Merge | 8 | 2 | 100.0000 ± 0.0000 | N/A | 3.3030 ± 0.1031 |
| Periodic-Reset-SpaceSaving | 0 | 1 | 99.1026 ± 0.3392 | N/A | 3.1805 ± 0.2006 |
| Periodic-Reset-SpaceSaving | 0 | 1.5 | 99.3590 ± 0.1282 | N/A | 2.6983 ± 0.0080 |
| Periodic-Reset-SpaceSaving | 0 | 2 | 99.1880 ± 0.3701 | N/A | 0.7022 ± 0.0370 |
