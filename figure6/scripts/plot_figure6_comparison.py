#!/usr/bin/env python3
"""Figure 6-style two-series bar charts from measured comparison CSV only."""
import argparse
import csv
from collections import defaultdict
from pathlib import Path
from statistics import mean, stdev

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter, MultipleLocator
import numpy as np

PANELS = [
    ('runtime_ms', '(a) Run Time for Top-{k} on Synthetic Data', 'Run Time (ms)', 'runtime'),
    ('precision', '(b) Precision for Top-{k} on Synthetic Data', 'Precision', 'precision'),
    ('recall', '(c) Recall for Top-{k} on Synthetic Data', 'Recall', 'recall'),
    ('memory_bytes', '(d) Space Used for Top-{k} on Synthetic Data', 'Space Used (Bytes)', 'memory'),
]
ALGORITHMS = ['Space-Saving', 'CountSketch']
COLORS = {'Space-Saving': '#292929', 'CountSketch': '#00a0d2'}


def label(v, metric):
    if metric == 'memory_bytes':
        return f'{v:,.0f}'
    if metric in ('precision', 'recall'):
        return f'{v:.2f}'
    return f'{v:,.0f}'


def make_panel(ax, metric, title, ylabel, alphas, grouped, k, legend=True):
    x = np.arange(len(alphas), dtype=float)
    width = .34
    series_max = 0
    plotted = []
    for j, algorithm in enumerate(ALGORITHMS):
        means = [mean(float(z[metric]) for z in grouped[(a, algorithm)]) for a in alphas]
        devs = [stdev(float(z[metric]) for z in grouped[(a, algorithm)])
                if len(grouped[(a, algorithm)]) > 1 else 0 for a in alphas]
        offset = (j - (len(ALGORITHMS)-1)/2) * width
        bars = ax.bar(x + offset, means, width=width, color=COLORS[algorithm],
                      edgecolor='#161616', linewidth=.5, label=algorithm, zorder=3)
        if any(v > 0 for v in devs):
            ax.errorbar(x + offset, means, yerr=devs, fmt='none', color='#888888',
                        linewidth=.75, capsize=2.5, zorder=4)
        plotted.append((bars, means, devs))
        series_max = max(series_max, max((a + b for a,b in zip(means,devs)), default=0))
    ax.set_title(title.format(k=k), fontsize=11.5, weight='bold', pad=12)
    ax.set_xlabel('Zipf Alpha', fontweight='bold', fontsize=10)
    ax.set_ylabel(ylabel, fontsize=10, fontweight='bold')
    ax.set_xticks(x, [f'{a:g}' for a in alphas])
    ax.tick_params(axis='both', labelsize=9)
    ax.set_xlim(-.65, len(alphas) - .35)
    ax.grid(axis='y', color='#e4e4e7', linewidth=.7, zorder=0)
    ax.set_axisbelow(True)
    if metric in ('precision', 'recall'):
        ax.set_ylim(0, 1.17)
        ax.yaxis.set_major_locator(MultipleLocator(.2))
        ax.yaxis.set_major_formatter(FuncFormatter(lambda v, _: f'{v:.1f}' if v <= 1.001 else ''))
    else:
        ax.set_ylim(0, max(1, series_max * 1.24))
        ax.yaxis.set_major_formatter(FuncFormatter(lambda v, _: f'{v:,.0f}'))
    y_max = ax.get_ylim()[1]
    for bars,means,devs in plotted:
        for bar,v,dev in zip(bars,means,devs):
            y = min(v + dev + .012 * y_max, .97*y_max)
            ax.text(bar.get_x()+bar.get_width()/2, y, label(v,metric),
                    ha='center', va='bottom', fontsize=7.5, fontweight='medium',
                    rotation=0, color='#161616', clip_on=False, zorder=5)
    if legend:
        ax.legend(loc='upper right', ncol=2, frameon=True, fontsize=9,
                  fancybox=False, edgecolor='#d4d4d8')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--csv', default='results/figure6_comparison.csv')
    p.add_argument('--outdir', default='results/comparison')
    p.add_argument('--dpi', type=int, default=200)
    args = p.parse_args()
    with open(args.csv, encoding='utf-8', newline='') as f:
        rows = list(csv.DictReader(f))
    if not rows:
        raise SystemExit('Empty CSV')
    if {r['algorithm'] for r in rows} != set(ALGORITHMS):
        raise SystemExit('Need both algorithm series to draw grouped bars')
    if len({(r['topk'],r['events'],r['alphabet'],r['space_saving_counters'],r['countsketch_width'],r['countsketch_depth']) for r in rows}) != 1:
        raise SystemExit('Do not mix different configurations in one plot')
    grouped = defaultdict(list)
    for r in rows: grouped[(float(r['alpha']),r['algorithm'])].append(r)
    alphas = sorted({float(r['alpha']) for r in rows})
    for alpha in alphas:
        for algo in ALGORITHMS:
            if (alpha,algo) not in grouped: raise SystemExit(f'Missing {algo} at alpha={alpha}')
            if len(grouped[(alpha,algo)]) != len(grouped[(alpha,ALGORITHMS[0])]):
                raise SystemExit('Missing repeated seed measurements')
    out = Path(args.outdir)
    out.mkdir(parents=True, exist_ok=True)
    k = rows[0]['topk']
    setup = (f'Space-Saving M={rows[0]["space_saving_counters"]}; '
             f'CountSketch {rows[0]["countsketch_depth"]}×{rows[0]["countsketch_width"]}, top-{k} candidates')
    plt.rcParams.update({'font.family':'DejaVu Sans',
                         'axes.spines.top':False,'axes.spines.right':False})
    fig,axs=plt.subplots(2,2,figsize=(17.5,10.5))
    for ax,(metric,title,ylabel,stem) in zip(axs.flat,PANELS):
        make_panel(ax,metric,title,ylabel,alphas,grouped,k)
        single,sax=plt.subplots(figsize=(9.3,5.4))
        make_panel(sax,metric,title,ylabel,alphas,grouped,k)
        single.suptitle(setup,fontsize=9,y=.98)
        single.subplots_adjust(left=.12,right=.975,bottom=.15,top=.83)
        for ext in ('png','svg'):
            single.savefig(out/f'figure6_{stem}.{ext}',dpi=args.dpi,
                           facecolor='white',bbox_inches='tight')
        plt.close(single)
    fig.suptitle(f'Figure 6 Reproduction — Space-Saving vs CountSketch | Top-{k}',
                 fontsize=16,fontweight='bold',y=.986)
    fig.text(.5,.028,setup + ' | measured data, not paper-identical configuration',
             ha='center', fontsize=10, color='#52525b')
    fig.subplots_adjust(left=.07,right=.986,bottom=.09,top=.9,hspace=.48,wspace=.24)
    for ext in ('png','svg'):
        fig.savefig(out/f'figure6_comparison.{ext}',dpi=args.dpi,facecolor='white')
    plt.close(fig)
    print(f'Generated four grouped-bar panels in {out}')

if __name__=='__main__':
    main()
