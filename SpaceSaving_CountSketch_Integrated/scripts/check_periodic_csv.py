#!/usr/bin/env python3
"""Check key invariants in a paired periodic-reset/windowed CSV."""
import csv
import sys
from pathlib import Path

path = Path(sys.argv[1]) if len(sys.argv)>1 else Path('results/periodic/windowed_with_periodic.csv')
with path.open(newline='') as f:
    rows=list(csv.DictReader(f))
assert rows, 'Empty CSV'
by_run={}
for r in rows:
    key=(r['scenario'],r['alpha'],r['repeat'],r['seed'],r['total_counters'])
    by_run.setdefault(key,{})[r['method']+((':'+r['blocks']) if int(r['blocks']) else '')]=r
for key, run in by_run.items():
    expected={'Cumulative-SpaceSaving','Periodic-Reset-SpaceSaving'}
    expected|={f'{method}:{b}' for method in ('Naive-Block-Merge','Bound-Aware-Block-Merge') for b in (2,4,8)}
    assert set(run)==expected, (key, set(run)^expected)
    periodic=run['Periodic-Reset-SpaceSaving']
    w=int(periodic['window']); q=int(periodic['query_interval']); n=int(periodic['events'])
    first=((w+q-1)//q)*q
    times=range(first,n+1,q)
    cov=[((t-1)%w+1)/w for t in times]
    assert len(times)==int(periodic['query_count'])
    assert abs(float(periodic['observed_window_fraction'])-sum(cov)/len(cov))<1e-7
    assert int(periodic['aligned_query_count'])==sum(t%w==0 for t in times)
    assert abs(float(periodic['full_window_query_rate'])-sum(t%w==0 for t in times)/len(times))<1e-7
    for method, r in run.items():
        assert int(r['query_count'])==len(times), (key, method)
        assert int(r['aligned_query_count'])==int(periodic['aligned_query_count'])
        if method!='Periodic-Reset-SpaceSaving':
            assert abs(float(r['observed_window_fraction'])-1)<1e-9
print(f'PASS: {len(rows)} rows, {len(by_run)} paired runs; periodic span and alignment correct')
