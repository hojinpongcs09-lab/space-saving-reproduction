#!/usr/bin/env python3
"""Recompute report tables from the committed default periodic CSV (stdlib)."""
import csv
import hashlib
import math
from collections import defaultdict
from pathlib import Path
from statistics import mean, stdev

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'results/periodic/windowed_with_periodic.csv'
OUTPUT = ROOT / 'results/periodic/analysis/REPORT_TABLES.md'


def require(ok, message):
    if not ok:
        raise ValueError(message)


def main():
    with SOURCE.open(newline='') as f:
        rows = list(csv.DictReader(f))
    methods = {('Cumulative-SpaceSaving', 0), ('Periodic-Reset-SpaceSaving', 0)}
    methods |= {(m, b) for m in ('Naive-Block-Merge', 'Bound-Aware-Block-Merge') for b in (2, 4, 8)}
    paired = defaultdict(set)
    grouped = defaultdict(list)
    for r in rows:
        method = (r['method'], int(r['blocks']))
        key = (r['scenario'], float(r['alpha']), int(r['repeat']))
        require(method not in paired[key], 'duplicate configuration')
        paired[key].add(method)
        for field, expected in [('events',2000000), ('alphabet',10000), ('window',100000),
                                ('total_counters',1600), ('topk',20), ('query_interval',50000),
                                ('query_count',39), ('aligned_query_count',20)]:
            require(int(r[field]) == expected, f'unexpected default {field}')
        require(0 <= float(r['tie_recall']) <= 1, 'invalid recall')
        if r['method'] == 'Bound-Aware-Block-Merge':
            require(int(r['bound_violations']) == 0, 'bound violation')
        else:
            require(math.isnan(float(r['certification_rate'])), 'non-bound certification must be N/A')
        grouped[(r['scenario'], *method)].append(r)
    expected_keys = {(s,a,r) for s in ('static','shift','gradual','overlap')
                     for a in (1.0,1.5,2.0) for r in range(3)}
    require(set(paired) == expected_keys and all(v == methods for v in paired.values()),
            'incomplete paired experiment')
    require(len(rows) == 288, 'expected 288 records')
    lines = ['# 由原始 CSV 重算的報告表格', '',
             '來源：[`windowed_with_periodic.csv`](../windowed_with_periodic.csv)。', '',
             f'來源 SHA-256：`{hashlib.sha256(SOURCE.read_bytes()).hexdigest()}`', '',
             '每個情境／方法為 3 個 alpha × 3 個種子，共 9 筆 run 等權平均。',
             '時間為每次完整 run 的總更新／總查詢時間，不是單筆延遲；不是正式效能結論。', '',
             '| 情境 | 方法 | B | Tie Recall % | Aligned Tie Recall % | 認證率 % | 更新 ms | 查詢 ms |',
             '|---|---|---:|---:|---:|---:|---:|---:|']
    for (scenario, method, b), group in sorted(grouped.items()):
        def avg(field):
            return mean(float(r[field]) for r in group)
        cert = avg('certification_rate')
        lines.append(f'| {scenario} | {method} | {b} | {avg("tie_recall")*100:.4f} | '
                     f'{avg("aligned_tie_recall")*100:.4f} | '
                     f'{"N/A" if math.isnan(cert) else f"{cert*100:.4f}"} | '
                     f'{avg("update_ms"):.4f} | {avg("query_ms"):.4f} |')
    lines += ['', '## Shift 每個 alpha 的三種子變異', '',
              '平均 ± 樣本標準差（n=3）；不把跨 alpha 差異當作隨機變異，也不是信賴區間。', '',
              '| 方法 | B | alpha | Tie Recall % | 認證率 % | 查詢總時間 ms |',
              '|---|---:|---:|---|---|---|']
    for (scenario, method, b), group in sorted(grouped.items()):
        if scenario != 'shift':
            continue
        for alpha in (1.0,1.5,2.0):
            samples = [r for r in group if float(r['alpha']) == alpha]
            def display(field, scale=1):
                values = [float(r[field])*scale for r in samples]
                return 'N/A' if any(math.isnan(x) for x in values) else f'{mean(values):.4f} ± {stdev(values):.4f}'
            lines.append(f'| {method} | {b} | {alpha:g} | {display("tie_recall",100)} | '
                         f'{display("certification_rate",100)} | {display("query_ms")} |')
    OUTPUT.parent.mkdir(exist_ok=True)
    OUTPUT.write_text('\n'.join(lines)+'\n')
    print('PASS: 288 rows / 36 complete paired runs; wrote', OUTPUT.relative_to(ROOT))


if __name__ == '__main__':
    main()
