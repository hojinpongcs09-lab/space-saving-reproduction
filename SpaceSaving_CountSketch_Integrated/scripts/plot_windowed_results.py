#!/usr/bin/env python3
"""Plot the block-windowed Space-Saving experiment CSV."""
import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--csv", required=True)
    parser.add_argument("--outdir", required=True)
    args = parser.parse_args()

    source = Path(args.csv)
    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    df = pd.read_csv(source)

    metrics = [
        "precision", "recall", "certification_rate", "order_certification_rate",
        "detection_coverage", "avg_detection_delay_packets", "runtime_ms", "memory_bytes",
        "observed_window_fraction", "full_window_query_rate", "aligned_tie_recall",
    ]
    group_cols = ["scenario", "alpha", "method", "blocks"]
    summary = df.groupby(group_cols)[metrics].agg(["mean", "std"]).reset_index()
    summary.columns = [
        "_".join(str(part) for part in col if str(part) != "") if isinstance(col, tuple) else col
        for col in summary.columns
    ]
    summary.to_csv(outdir / "summary_by_condition.csv", index=False)

    labels = {
        "Cumulative-SpaceSaving": "Cumulative SS",
        "Periodic-Reset-SpaceSaving": "Periodic reset",
        "Naive-Block-Merge": "Naive block merge",
        "Bound-Aware-Block-Merge": "Bound-aware merge",
    }
    plot_metrics = [
        ("precision", "Top-k Precision", "rate"),
        ("recall", "Top-k Recall", "rate"),
        ("certification_rate", "Certified Top-k Membership Rate", "rate"),
        ("order_certification_rate", "Certified Returned-Order Rate", "rate"),
        ("runtime_ms", "Update + Query Runtime (ms)", "ms"),
        ("memory_bytes", "Persistent Algorithm Memory (bytes)", "bytes"),
        ("observed_window_fraction", "Mean fraction of W represented", "fraction"),
        ("aligned_tie_recall", "Tie-aware Recall at full W boundaries only", "rate"),
    ]
    scenarios = sorted(df["scenario"].unique())
    for scenario in scenarios:
        part = df[df["scenario"] == scenario]
        for metric, title, ylabel in plot_metrics:
            fig, ax = plt.subplots(figsize=(8.5, 5.2))
            for method in labels:
                method_rows = part[part["method"] == method]
                for alpha in sorted(method_rows["alpha"].unique()):
                    subset = method_rows[method_rows["alpha"] == alpha].groupby("blocks")[metric].mean()
                    if subset.empty:
                        continue
                    xs = [0.5 if method == "Periodic-Reset-SpaceSaving" else 1 if int(b) == 0 else int(b) for b in subset.index]
                    order = sorted(range(len(xs)), key=lambda i: xs[i])
                    xs = [xs[i] for i in order]
                    ys = [float(subset.iloc[i]) for i in order]
                    ax.plot(xs, ys, marker="o", label=f"{labels[method]}, α={alpha:g}")
            ax.set_title(f"{title} — {scenario.title()} Traffic")
            ax.set_xlabel("Baseline / Number of blocks")
            ax.set_ylabel(ylabel)
            if metric == "avg_detection_delay_packets":
                ax.set_yscale("symlog", linthresh=1000)
            ax.set_xticks([0.5, 1, 2, 4, 8], ["Periodic", "Cumulative", "B=2", "B=4", "B=8"])
            ax.grid(True, alpha=0.25)
            ax.legend(fontsize=7, ncol=2)
            fig.tight_layout()
            fig.savefig(outdir / f"{metric}_{scenario}.png", dpi=180)
            fig.savefig(outdir / f"{metric}_{scenario}.svg")
            plt.close(fig)

        if scenario != "static":
            for metric, title, ylabel in [
                ("detection_coverage", "Query-sampled Top-k Entry Coverage", "coverage"),
                ("avg_detection_delay_packets", "Query-sampled Top-k Entry Detection Delay", "packets"),
            ]:
                fig, ax = plt.subplots(figsize=(8.5, 5.2))
                for method in labels:
                    method_rows = part[part["method"] == method]
                    for alpha in sorted(method_rows["alpha"].unique()):
                        subset = method_rows[method_rows["alpha"] == alpha].groupby("blocks")[metric].mean()
                        if subset.empty:
                            continue
                        xs = [0.5 if method == "Periodic-Reset-SpaceSaving" else 1 if int(b) == 0 else int(b) for b in subset.index]
                        order = sorted(range(len(xs)), key=lambda i: xs[i])
                        xs = [xs[i] for i in order]
                        ys = [float(subset.iloc[i]) for i in order]
                        ax.plot(xs, ys, marker="o", label=f"{labels[method]}, α={alpha:g}")
                ax.set_title(title)
                ax.set_xlabel("Baseline / Number of blocks")
                ax.set_ylabel(ylabel)
                if metric == "avg_detection_delay_packets":
                    ax.set_yscale("symlog", linthresh=1000)
                ax.set_xticks([0.5, 1, 2, 4, 8], ["Periodic", "Cumulative", "B=2", "B=4", "B=8"])
                ax.grid(True, alpha=0.25)
                ax.legend(fontsize=7, ncol=2)
                fig.tight_layout()
                fig.savefig(outdir / f"{metric}_{scenario}.png", dpi=180)
                fig.savefig(outdir / f"{metric}_{scenario}.svg")
                plt.close(fig)

    print(f"Wrote plots and summary to {outdir}")


if __name__ == "__main__":
    main()
