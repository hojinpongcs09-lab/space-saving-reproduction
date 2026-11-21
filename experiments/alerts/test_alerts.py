"""Hand-checkable alert regressions, including early false alert recovery."""
from pathlib import Path
import subprocess
import tempfile
import unittest
from run_examples import ROOT, read_csv, summarize


class AlertTests(unittest.TestCase):
    def run_case(self, name):
        with tempfile.TemporaryDirectory() as tmp:
            events, sources = Path(tmp)/'events.csv', Path(tmp)/'sources.csv'
            subprocess.run([str(ROOT/'build/alert_validation'), '--input', str(ROOT/'data'/f'{name}.txt'),
                            '--events', str(events), '--sources', str(sources)], check=True, capture_output=True)
            return read_csv(events), read_csv(sources)

    def test_steady_threshold(self):
        events, rows = self.run_case('steady')
        self.assertEqual([r['estimate_alert'] for r in events], ['0']*4+['1']*2)
        for r in rows:
            self.assertEqual((r['true_threshold_packet'], r['first_alert_packet'],
                              r['first_detection_packet'], r['delay_packets'], r['false_alert']),
                             ('5', '5', '5', '0', '0'))

    def test_churn_false_alerts_without_true_positives(self):
        events, rows = self.run_case('churn')
        # Four counters reach count 4 after 16 unique packets. The next four
        # new sources inherit 4 and falsely alert with estimate 5, lower 1.
        self.assertEqual([int(r['packet']) for r in events if r['estimate_alert']=='1'], [17,18,19,20])
        self.assertTrue(all(r['lower_alert']=='0' for r in events))
        summaries = list(summarize('churn', rows))
        self.assertEqual([r['false_alert_sources'] for r in summaries], [4,0])
        self.assertTrue(all(r['coverage']=='' and r['mean_delay_packets']=='' for r in summaries))

    def test_returning_source_delays(self):
        _, rows = self.run_case('returning')
        target = {r['rule']: r for r in rows if r['source_ip']=='192.0.2.1'}
        # Returning at packet 21 inherits min=5: c=6,e=5,lower=1.
        # Actual fifth occurrence is packet 24; lower reaches 5 at packet 25.
        self.assertEqual(target['estimate']['first_alert_packet'], '21')
        self.assertEqual(target['estimate']['false_alert'], '1')
        self.assertEqual(target['estimate']['first_detection_packet'], '24')
        self.assertEqual(target['estimate']['delay_packets'], '0')
        self.assertEqual(target['lower']['first_detection_packet'], '25')
        self.assertEqual(target['lower']['delay_packets'], '1')
        self.assertEqual(target['lower']['false_alert'], '0')

    def test_missed_is_not_zero_delay(self):
        _, rows = self.run_case('missed')
        target = {r['rule']: r for r in rows if r['source_ip']=='192.0.2.1'}
        self.assertEqual(target['estimate']['first_detection_packet'], '24')
        self.assertEqual(target['lower']['missed'], '1')
        self.assertEqual(target['lower']['delay_packets'], '')
        summaries = list(summarize('missed', rows))
        self.assertEqual([r['coverage'] for r in summaries], [1,0])
        self.assertEqual(summaries[1]['mean_delay_packets'], '')
        self.assertTrue(all(r['paired_mean_delay_packets']=='' for r in summaries))

    def test_invalid_input_and_threshold(self):
        with tempfile.TemporaryDirectory() as tmp:
            inp, events, sources = (Path(tmp)/x for x in ('input.txt','events.csv','sources.csv'))
            base = [str(ROOT/'build/alert_validation'), '--input', str(inp), '--events', str(events), '--sources', str(sources)]
            for content in ('', 'not-an-ip\n', '192.0.2.256\n', '192.0.2.1.\n', '\n'):
                inp.write_text(content)
                self.assertNotEqual(subprocess.run(base, capture_output=True).returncode, 0)
                self.assertFalse(events.exists())
            inp.write_text('192.0.2.1\n')
            for threshold in ('0','-1','5x'):
                self.assertNotEqual(subprocess.run(base+['--threshold',threshold], capture_output=True).returncode, 0)


if __name__ == '__main__':
    unittest.main()
