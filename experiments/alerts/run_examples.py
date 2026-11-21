"""Reproduce deterministic examples using only the Python standard library."""
import csv
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent
SCENARIOS = ('steady', 'churn', 'returning', 'missed')


def read_csv(path):
    with path.open(newline='') as f:
        return list(csv.DictReader(f))


def summarize(name, rows):
    detected_by_rule = {rule: {r['source_ip'] for r in rows
                        if r['rule'] == rule and r['first_detection_packet']}
                        for rule in ('estimate', 'lower')}
    paired = detected_by_rule['estimate'] & detected_by_rule['lower']
    for rule in ('estimate', 'lower'):
        selected = [r for r in rows if r['rule'] == rule]
        eligible = [r for r in selected if r['true_threshold_packet']]
        detected = [r for r in eligible if r['first_detection_packet']]
        # Paired means BOTH rules detected this source, not either rule.
        both = [r for r in detected if r['source_ip'] in paired]
        yield dict(scenario=name, counters=4, threshold=5, rule=rule,
                   false_alert_sources=sum(int(r['false_alert']) for r in selected),
                   eligible_sources=len(eligible), detected_sources=len(detected),
                   missed_sources=len(eligible)-len(detected),
                   coverage=len(detected)/len(eligible) if eligible else '',
                   mean_delay_packets=sum(int(r['delay_packets']) for r in detected)/len(detected) if detected else '',
                   paired_detected_sources=len(both),
                   paired_mean_delay_packets=sum(int(r['delay_packets']) for r in both)/len(both) if both else '')


def main():
    results = ROOT / 'results'
    results.mkdir(exist_ok=True)
    summary = []
    for name in SCENARIOS:
        subprocess.run([str(ROOT/'build/alert_validation'), '--input', str(ROOT/'data'/f'{name}.txt'),
                        '--events', str(results/f'{name}_events.csv'),
                        '--sources', str(results/f'{name}_sources.csv')], check=True)
        summary.extend(summarize(name, read_csv(results/f'{name}_sources.csv')))
    with (results/'summary.csv').open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=list(summary[0]), lineterminator='\n')
        writer.writeheader()
        writer.writerows(summary)


if __name__ == '__main__':
    main()
