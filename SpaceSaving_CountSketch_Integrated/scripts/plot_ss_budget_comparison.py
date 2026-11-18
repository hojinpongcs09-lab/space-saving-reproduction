#!/usr/bin/env python3
"""Plot paired Space-Saving results: alpha-specific theory policy vs fixed counter budget."""
import argparse
import csv
from collections import defaultdict
from pathlib import Path
from statistics import mean, stdev

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Patch
from matplotlib.ticker import FuncFormatter, MultipleLocator

SERIES = ['Space-Saving (alpha-specific)', 'Space-Saving (fixed-1000)']
COLORS = {
    'Space-Saving (alpha-specific)': '#292929',
    'Space-Saving (fixed-1000)': '#00a0d2',
}
PANELS = [
    ('runtime_ms', 'Run Time for Space-Saving', 'Run Time (ms)', 'runtime'),
    ('precision', 'Precision for Space-Saving', 'Precision', 'precision'),
    ('recall', 'Recall for Space-Saving', 'Recall', 'recall'),
    ('memory_bytes', 'Space Used for Space-Saving', 'Space Used (Bytes)', 'memory'),
]


def fmt(value, metric):
    if metric == 'memory_bytes':
        return f'{value:,.0f}'
    if metric in ('precision', 'recall'):
        return f'{value:.3f}'
    return f'{value:,.0f}'


def aggregate(rows):
    grouped = defaultdict(list)
    for row in rows:
        grouped[(float(row['alpha']), row['algorithm'])].append(row)
    return grouped


def draw_panel(ax, metric, title, ylabel, alphas, grouped, legend=True):
    width = 0.36
    max_y = 0.0
    drawn = []
    for index, series in enumerate(SERIES):
        values = [float(r[metric]) for a in alphas for r in grouped[(a, series)]]
        means, deviations = [], []
        for alpha in alphas:
            sample = [float(r[metric]) for r in grouped[(alpha, series)]]
            means.append(mean(sample))
            deviations.append(stdev(sample) if len(sample) > 1 else 0.0)
        xs = [i + (index - 0.5) * width for i in range(len(alphas))]
        bars = ax.bar(xs, means, width=width, color=COLORS[series], label=series,
                      edgecolor='#161616', linewidth=.5, zorder=3)
        if any(deviations):
            ax.errorbar(xs, means, yerr=deviations, fmt='none', color='#777777',
                        linewidth=.8, capsize=2.5, zorder=4)
        drawn.append((bars, means, deviations))
        max_y = max(max_y, max((m+d for m,d in zip(means,deviations)), default=0.0))

    ax.set_title(title, fontsize=11.5, weight='bold', pad=10)
    ax.set_xlabel('Zipf Alpha', fontsize=10, weight='bold')
    ax.set_ylabel(ylabel, fontsize=10, weight='bold')
    ax.set_xticks(range(len(alphas)), [f'{a:g}' for a in alphas])
    ax.grid(axis='y', color='#e4e4e7', linewidth=.7, zorder=0)
    ax.set_axisbelow(True)
    ax.tick_params(axis='both', labelsize=9)
    if metric in ('precision', 'recall'):
        ax.set_ylim(0, 1.17)
        ax.yaxis.set_major_locator(MultipleLocator(.2))
        ax.yaxis.set_major_formatter(FuncFormatter(lambda v, _: f'{v:.1f}' if v <= 1.001 else ''))
    else:
        ax.set_ylim(0, max(1.0, max_y * 1.28))
        ax.yaxis.set_major_formatter(FuncFormatter(lambda v, _: f'{v:,.0f}'))
    top = ax.get_ylim()[1]
    for bars, means, deviations in drawn:
        for bar, value, dev in zip(bars, means, deviations):
            y = min(value + dev + .012 * top, .965 * top)
            ax.text(bar.get_x() + bar.get_width()/2, y, fmt(value, metric),
                    ha='center', va='bottom', fontsize=7.2, color='#161616',
                    clip_on=False, zorder=5)
    if legend:
        handles = [Patch(facecolor=COLORS[name], edgecolor='#161616', label=name)
                   for name in SERIES]
        ax.legend(handles=handles, loc='upper center', bbox_to_anchor=(.5, 1.18),
                  ncol=2, fontsize=7.8, frameon=True, fancybox=False,
                  edgecolor='#d4d4d8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--csv', default='results/space_saving_budget_comparison.csv')
    parser.add_argument('--outdir', default='results/space_saving_budget_comparison')
    parser.add_argument('--fixed-counters', type=int, default=1000)
    parser.add_argument('--dpi', type=int, default=200)
    args = parser.parse_args()

    with open(args.csv, encoding='utf-8', newline='') as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        raise SystemExit('Empty CSV')
    fixed_name = f'Space-Saving (fixed-{args.fixed_counters})'
    global SERIES, COLORS
    SERIES = ['Space-Saving (alpha-specific)', fixed_name]
    COLORS[fixed_name] = COLORS.pop('Space-Saving (fixed-1000)', '#00a0d2')
    algorithms = {r['algorithm'] for r in rows}
    if algorithms != set(SERIES):
        raise SystemExit(f'Expected exactly {SERIES}; got {sorted(algorithms)}')

    alphas = sorted({float(r['alpha']) for r in rows})
    grouped = aggregate(rows)
    repeats = {int(r['repeat']) for r in rows}
    expected_pairs = len(alphas) * len(repeats) * len(SERIES)
    if len(rows) != expected_pairs:
        raise SystemExit(f'Expected {expected_pairs} rows, got {len(rows)}')
    for alpha in alphas:
        for repeat in repeats:
            pair = [r for r in rows if float(r['alpha']) == alpha and int(r['repeat']) == repeat]
            if {r['algorithm'] for r in pair} != set(SERIES):
                raise SystemExit(f'Missing paired Space-Saving result for alpha={alpha}, repeat={repeat}')
            if len({r['seed'] for r in pair}) != 1:
                raise SystemExit(f'Paired runs use different seeds at alpha={alpha}, repeat={repeat}')

    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)

    summary_fields = [
        'alpha','algorithm','runs','space_saving_counters','runtime_mean_ms','runtime_std_ms',
        'precision_mean','precision_std','recall_mean','recall_std','memory_bytes',
        'allocator_live_delta_mean_bytes'
    ]
    with (outdir / 'summary_by_alpha.csv').open('w', encoding='utf-8', newline='') as handle:
        writer = csv.DictWriter(handle, fieldnames=summary_fields)
        writer.writeheader()
        for alpha in alphas:
            for series in SERIES:
                sample = grouped[(alpha, series)]
                def vals(field): return [float(r[field]) for r in sample]
                def sd(field): return stdev(vals(field)) if len(sample) > 1 else 0.0
                writer.writerow({
                    'alpha': f'{alpha:g}', 'algorithm': series, 'runs': len(sample),
                    'space_saving_counters': sample[0]['space_saving_counters'],
                    'runtime_mean_ms': f'{mean(vals("runtime_ms")):.4f}',
                    'runtime_std_ms': f'{sd("runtime_ms"):.4f}',
                    'precision_mean': f'{mean(vals("precision")):.5f}',
                    'precision_std': f'{sd("precision"):.5f}',
                    'recall_mean': f'{mean(vals("recall")):.5f}',
                    'recall_std': f'{sd("recall"):.5f}',
                    'memory_bytes': f'{mean(vals("memory_bytes")):.0f}',
                    'allocator_live_delta_mean_bytes': f'{mean(vals("allocator_live_delta_bytes")):.0f}',
                })

    plt.rcParams.update({'font.family':'DejaVu Sans', 'axes.spines.top':False,
                         'axes.spines.right':False})
    fig, axes = plt.subplots(2, 2, figsize=(16.5, 10.0))
    for ax, (metric, title, ylabel, stem) in zip(axes.flat, PANELS):
        draw_panel(ax, metric, title, ylabel, alphas, grouped, legend=False)
        single, single_ax = plt.subplots(figsize=(9.3, 5.4))
        draw_panel(single_ax, metric, title, ylabel, alphas, grouped, legend=True)
        single.suptitle(
            f'α-specific Theorem-7 policy vs fixed {args.fixed_counters} counters | '
            f'N={int(rows[0]["events"]):,}, A={int(rows[0]["alphabet"]):,}, k={rows[0]["topk"]}',
            fontsize=9, y=.985)
        single.subplots_adjust(left=.12, right=.975, bottom=.15, top=.77)
        for ext in ('png','svg'):
            single.savefig(outdir / f'space_saving_{stem}.{ext}', dpi=args.dpi,
                           facecolor='white', bbox_inches='tight')
        plt.close(single)

    legend_handles = [Patch(facecolor=COLORS[name], edgecolor='#161616', label=name)
                      for name in SERIES]
    fig.legend(handles=legend_handles, loc='upper center', bbox_to_anchor=(.5, .955),
               ncol=2, fontsize=9, frameon=True, fancybox=False, edgecolor='#d4d4d8')
    fig.suptitle('Space-Saving Counter Budget Policy Comparison', fontsize=15, weight='bold', y=.995)
    fig.text(.5, .025,
             f'Paired input streams | α-specific theory policy vs fixed {args.fixed_counters} counters | '
             'fixed budget is a project-selected diagnostic, not a paper parameter',
             ha='center', fontsize=8, color='#52525b')
    fig.subplots_adjust(left=.075, right=.985, bottom=.11, top=.87, hspace=.50, wspace=.23)
    for ext in ('png','svg'):
        fig.savefig(outdir / f'space_saving_budget_comparison.{ext}', dpi=args.dpi,
                    facecolor='white')
    plt.close(fig)
    print(f'Wrote paired Space-Saving plots and summary to {outdir}')


if __name__ == '__main__':
    main()
