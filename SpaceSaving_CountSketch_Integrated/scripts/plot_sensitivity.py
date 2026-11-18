#!/usr/bin/env python3
"""Produce separate, budget-controlled sensitivity plots and per-condition summaries."""
import argparse
from pathlib import Path
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

p=argparse.ArgumentParser(); p.add_argument('--dir', default='results/sensitivity'); a=p.parse_args()
root=Path(a.dir); plots=root/'plots'; plots.mkdir(exist_ok=True)

def save(fig,name):
    fig.tight_layout(); fig.savefig(plots/f'{name}.png',dpi=170); fig.savefig(plots/f'{name}.svg'); plt.close(fig)

def summarize(df,keys,metrics,name):
    t=df.groupby(keys,dropna=False)[metrics].agg(['mean','std','count']).reset_index()
    t.columns=['_'.join(str(y) for y in x if y!='') if isinstance(x,tuple) else x for x in t.columns]
    t.to_csv(root/name,index=False)

ss=pd.read_csv(root/'ss_sweep.csv')
assert len(ss)==147 and not ss.duplicated(['alpha','repeat','algorithm']).any(), 'SS sweep incomplete'
metrics=['precision','recall','tie_precision','tie_recall','topk_set_valid','runtime_ms','memory_bytes']
summarize(ss,['alpha','algorithm','space_saving_counters'],metrics,'ss_summary.csv')
fixed=ss[ss.ss_budget_mode=='fixed']
for metric,label in [('tie_recall','Tie-aware Recall'),('runtime_ms','Runtime (ms)'),('memory_bytes','Persistent bytes'),('topk_set_valid','Exact Top-k set validity')]:
    fig,ax=plt.subplots(figsize=(8.2,5))
    for m,g in fixed.groupby('space_saving_counters'):
        s=g.groupby('alpha')[metric].mean(); ax.plot(s.index,s.values,'o-',label=f'M={m:,}')
    s=ss[ss.ss_budget_mode=='theory'].groupby('alpha')[metric].mean(); ax.plot(s.index,s.values,'k--s',label='Theory / fallback')
    ax.set(xlabel='Zipf α',ylabel=label,title='Space-Saving: fixed vs α-specific budget'); ax.grid(alpha=.25); ax.legend(fontsize=8,ncol=2)
    save(fig,'ss_'+metric)

files=sorted(root.glob('window_M*.csv'))
assert len(files)==6, 'Window sweep incomplete'
w=pd.concat([pd.read_csv(f) for f in files],ignore_index=True)
assert len(w)==1512 and not w.duplicated(['scenario','alpha','repeat','method','blocks','total_counters']).any(), 'Window sweep incomplete'
assert w[w.method=='Bound-Aware-Block-Merge'].bound_violations.sum()==0
w.to_csv(root/'window_sweep.csv',index=False)
metrics=['precision','recall','tie_precision','tie_recall','topk_set_valid_rate','certification_rate','order_certification_rate','avg_bound_width_per_returned_item','update_ms','query_ms','runtime_ms','memory_bytes','detection_coverage','avg_detection_delay_packets','missed_entries']
summarize(w,['scenario','alpha','total_counters','method','blocks'],metrics,'window_summary.csv')
# Facets by scenario at each alpha; no averaging over counter budgets or skew.
for alpha,g in w.groupby('alpha'):
    for metric,label in [('tie_recall','Tie-aware Recall'),('certification_rate','Certified membership rate'),('query_ms','Total query time (ms)'),('detection_coverage','Query-sampled detection coverage'),('avg_detection_delay_packets','Mean delay among detected entries (packets)')]:
        scenarios=['static','shift','gradual','overlap'] if metric not in ('detection_coverage','avg_detection_delay_packets') else ['shift','gradual','overlap']
        fig,axes=plt.subplots(1,len(scenarios),figsize=(4.2*len(scenarios),4.3),sharey=True)
        for ax,scenario in zip(axes,scenarios):
            for (method,b),d in g[g.scenario==scenario].groupby(['method','blocks']):
                s=d.groupby('total_counters')[metric].mean().dropna()
                if s.empty: continue
                name='Cumulative' if not b else ('Naive' if method.startswith('Naive') else 'Bound')+f' B={b}'
                ax.plot(s.index,s.values,'o-',label=name,markersize=3)
            ax.set_title(scenario); ax.set_xlabel('Total counters M'); ax.set_xscale('log'); ax.set_xticks([400,800,1600,3200,6400,10000],['400','800','1.6k','3.2k','6.4k','10k'],rotation=30); ax.grid(alpha=.25)
        axes[0].set_ylabel(label); axes[-1].legend(fontsize=7); fig.suptitle(f'Window sensitivity | α={alpha:g} | {label}')
        save(fig,f'window_{metric}_alpha{alpha:g}')
print('Wrote summaries and plots:',root)
