#!/usr/bin/env python3
"""Create paper-style, value-labelled bar charts for Figure 6 Space-Saving only.

The y-values come exclusively from measured CSV rows. This is NOT a recreation
of the paper's CountSketch or Probabilistic-InPlace comparison series.
"""
import argparse
import csv
from pathlib import Path
from statistics import mean, stdev

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Patch
from matplotlib.ticker import FuncFormatter, MultipleLocator


METRICS = (
    ('runtime_ms', '(a) Run Time for Top-{k} on Synthetic Data', 'Run Time (ms)', 'runtime'),
    ('precision', '(b) Precision for Top-{k} on Synthetic Data', 'Precision', 'precision'),
    ('recall', '(c) Recall for Top-{k} on Synthetic Data', 'Recall', 'recall'),
    ('memory_bytes', '(d) Space Used for Top-{k} on Synthetic Data', 'Space Used (Bytes)', 'memory'),
)
BAR_COLOR = '#27272a'  # paper-style black bars for Space-Saving


def number_label(metric, value):
    if metric == 'memory_bytes':
        return f'{value:,.0f}'
    if metric in ('precision', 'recall'):
        return f'{value:.2f}'
    return f'{value:,.2f}'


def alpha_label(alpha):
    return f'{alpha:g}'


def panel(ax, metric, title, ylabel, alphas, groups, k):
    means = [mean(float(r[metric]) for r in groups[a]) for a in alphas]
    stds = [stdev(float(r[metric]) for r in groups[a]) if len(groups[a]) > 1 else 0.0
            for a in alphas]
    xpos = list(range(len(alphas)))
    bars = ax.bar(xpos, means, width=0.67, color=BAR_COLOR,
                  edgecolor='black', linewidth=0.45, zorder=3)
    if any(v > 0 for v in stds):
        ax.errorbar(xpos, means, yerr=stds, fmt='none', ecolor='#666666',
                    elinewidth=1, capsize=3, zorder=4)

    ax.set_title(title.format(k=k), fontsize=12, fontweight='bold', pad=12)
    ax.set_xlabel('Zipf Alpha', fontsize=10, fontweight='bold', labelpad=7)
    ax.set_ylabel(ylabel, fontsize=10, fontweight='bold')
    ax.set_xticks(xpos, [alpha_label(a) for a in alphas])
    ax.set_xlim(-0.6, len(alphas) - 0.4)
    ax.tick_params(axis='both', labelsize=9)
    ax.yaxis.grid(True, linestyle='-', color='#d4d4d8', alpha=0.55,
                  linewidth=0.7, zorder=0)
    ax.set_axisbelow(True)
    ax.legend(handles=[Patch(facecolor=BAR_COLOR, edgecolor='black', label='Space-Saving')],
              loc='upper right', fontsize=9, frameon=True, framealpha=1,
              edgecolor='#d4d4d8', fancybox=False)

    upper = max((v + s for v, s in zip(means, stds)), default=1)
    if metric in ('precision', 'recall'):
        ax.set_ylim(0, max(1.17, upper * 1.16))
        ax.yaxis.set_major_locator(MultipleLocator(0.2))
        ax.yaxis.set_major_formatter(FuncFormatter(lambda val, _: f'{val:.1f}' if val <= 1.001 else ''))
    else:
        ax.set_ylim(0, max(upper * 1.26, 1))
        if metric == 'memory_bytes':
            ax.yaxis.set_major_formatter(FuncFormatter(lambda val, _: f'{val:,.0f}'))
        else:
            ax.yaxis.set_major_formatter(FuncFormatter(lambda val, _: f'{val:,.0f}'))

    ylim = ax.get_ylim()[1]
    for rect, value, sd in zip(bars, means, stds):
        ax.text(rect.get_x() + rect.get_width()/2,
                min(value + sd + 0.017 * ylim, 0.97 * ylim),
                number_label(metric, value),
                ha='center', va='bottom', fontsize=8.4,
                color='#18181b', fontweight='semibold', clip_on=False, zorder=5)
    return means


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--csv', default='results/figure6_space_saving.csv')
    ap.add_argument('--outdir', default='results')
    ap.add_argument('--dpi', type=int, default=190)
    args = ap.parse_args()

    with open(args.csv, newline='', encoding='utf-8') as file:
        rows = list(csv.DictReader(file))
    if not rows:
        raise SystemExit('No measurements in CSV')
    if len({(r['topk'], r['mode']) for r in rows}) != 1:
        raise SystemExit('Cannot combine different topk or experiment modes in one plot')
    out = Path(args.outdir)
    out.mkdir(parents=True, exist_ok=True)
    alphas = sorted({float(r['alpha']) for r in rows})
    groups = {a: [r for r in rows if float(r['alpha']) == a] for a in alphas}
    k = rows[0]['topk']
    mode = rows[0]['mode']
    counters = {r['counters'] for r in rows}
    if mode == 'fixed' and len(counters) == 1:
        setup = f'fixed M={int(next(iter(counters))):,} counters'
    elif mode == 'calibrate':
        setup = 'offline oracle-calibrated counter budgets'
    else:
        setup = f'mode={mode}'

    plt.rcParams.update({'font.family': 'DejaVu Sans', 'axes.spines.top': False,
                         'axes.spines.right': False})
    fig, axs = plt.subplots(2, 2, figsize=(15.8, 9.6))
    for ax, (metric, title, ylabel, filename) in zip(axs.flat, METRICS):
        panel(ax, metric, title, ylabel, alphas, groups, k)
        single, single_ax = plt.subplots(figsize=(8.6, 5.15))
        panel(single_ax, metric, title, ylabel, alphas, groups, k)
        single.suptitle(f'Space-Saving only | {setup}', fontsize=10, y=.99)
        single.subplots_adjust(left=.13, right=.97, bottom=.13, top=.86)
        for suffix in ('png', 'svg'):
            single.savefig(out / f'figure6_{filename}.{suffix}', dpi=args.dpi,
                           facecolor='white', bbox_inches='tight')
        plt.close(single)

    fig.suptitle(f'Figure 6 Reproduction — Space-Saving Only | Top-{k} | {setup}',
                 fontsize=15, fontweight='bold', y=.988)
    fig.text(.5, .012,
             'Measured values from experiment CSV; CountSketch and Probabilistic-InPlace are intentionally excluded.',
             ha='center', fontsize=9, color='#52525b')
    fig.subplots_adjust(left=.072, right=.978, bottom=.09, top=.905,
                        hspace=.44, wspace=.27)
    for suffix in ('png', 'svg'):
        fig.savefig(out / f'figure6_space_saving.{suffix}', dpi=args.dpi,
                    facecolor='white')
    plt.close(fig)
    print(f'Wrote paper-style, value-labelled Figure 6 bar charts to: {out.resolve()}')


if __name__ == '__main__':
    main()
