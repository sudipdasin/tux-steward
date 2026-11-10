import os
import sys
import time
import unittest
from tux_steward.core import CATALOG, matches
from tux_steward.runner import OUTPUT_LIMIT, run_command

class RunnerTests(unittest.TestCase):
    def run_python(self,script,**kw):
        return run_command((sys.executable,'-u','-c',script),**kw)
    def test_output_arrives_before_completion(self):
        events=[]; start=time.monotonic()
        r=self.run_python("import time; print('first'); time.sleep(.3); print('last')",on_output=lambda s:events.append((time.monotonic()-start,s)))
        self.assertEqual(r['code'],0)
        self.assertIn('first',events[0][1])
        self.assertGreater(events[-1][0]-events[0][0],.2)
        self.assertIn('last',r['output'])
    def test_error_and_exit_code(self):
        r=self.run_python("import sys; print('failure',file=sys.stderr); sys.exit(7)")
        self.assertEqual(r['code'],7); self.assertIn('failure',r['output'])
    def test_output_is_bounded(self):
        r=self.run_python("print('x'*1000000)")
        self.assertTrue(r['truncated']); self.assertLessEqual(len(r['output']),OUTPUT_LIMIT)
    def test_timeout_preserves_partial_output(self):
        r=self.run_python("import time; print('before'); time.sleep(10)",timeout=.15)
        self.assertEqual(r['code'],124); self.assertIn('before',r['output'])
        self.assertIn('partially completed',r['output']); self.assertLess(r['seconds'],2)
    def test_child_holding_pipe_does_not_hang(self):
        r=self.run_python("import subprocess,sys; subprocess.Popen([sys.executable,'-c','import time; time.sleep(10)']); print('parent done')",timeout=.2)
        self.assertEqual(r['code'],124); self.assertLess(r['seconds'],2)
    def test_deadline_applies_after_stdout_closes(self):
        r=self.run_python("import os,time; os.close(1); os.close(2); time.sleep(10)",timeout=.15)
        self.assertEqual(r['code'],124)
    def test_split_utf8_and_invalid_bytes(self):
        r=self.run_python("import os,time; os.write(1,b'\\xe2'); time.sleep(.05); os.write(1,b'\\x82\\xac\\xff')")
        self.assertEqual(r['output'],'€�')
    def test_callback_error_reaps_process(self):
        def broken(chunk): raise RuntimeError('callback failed')
        with self.assertRaisesRegex(RuntimeError,'callback failed'):
            self.run_python("import time; print('hello'); time.sleep(10)",on_output=broken)
    def test_literal_arguments(self):
        r=run_command((sys.executable,'-c','import sys; print(sys.argv[1])','$(touch /tmp/nope); echo unsafe'))
        self.assertEqual(r['output'].strip(),'$(touch /tmp/nope); echo unsafe')
    def test_invalid_timeout(self):
        with self.assertRaises(ValueError): self.run_python('pass',timeout=0)
    def test_search(self):
        self.assertTrue(matches(CATALOG['storage'],'STORAGE usage'))
        self.assertFalse(matches(CATALOG['storage'],'wine'))
        self.assertTrue(matches(CATALOG['storage'],'  '))
