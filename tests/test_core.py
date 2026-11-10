import os
import subprocess
import unittest
from unittest.mock import patch
from tux_steward.core import ACTIONS, CATALOG, execute, unavailable

class ActionsTest(unittest.TestCase):
    def test_unique_ids(self):
        self.assertEqual(len(CATALOG),len(ACTIONS))
    def test_mutation_requires_confirmation(self):
        for a in ACTIONS:
            if a.changes:
                with self.subTest(a=a.id), self.assertRaises(PermissionError): execute(a.id)
    def test_admin_actions_are_changes(self):
        self.assertTrue(all(a.changes for a in ACTIONS if a.admin))
    @patch('tux_steward.core.shutil.which',return_value='/usr/bin/tool')
    @patch('tux_steward.core.subprocess.run')
    def test_failed_command_preserves_error(self,run,which):
        run.return_value=subprocess.CompletedProcess([],7,'partial','denied')
        r=execute('storage'); self.assertEqual(r['code'],7); self.assertIn('denied',r['output'])
        self.assertFalse(run.call_args.kwargs['shell'])
    @patch('tux_steward.core.shutil.which',return_value=None)
    def test_missing_tool_does_not_execute(self,which):
        with patch('tux_steward.core.subprocess.run') as run:
            with self.assertRaises(RuntimeError): execute('storage')
            run.assert_not_called()
    @patch.dict(os.environ,{'XDG_SESSION_TYPE':'wayland'})
    def test_wayland_gate(self):
        self.assertIn('X11',unavailable(CATALOG['input']))
    @patch('tux_steward.core.shutil.which',return_value='/usr/bin/tool')
    @patch('tux_steward.core.subprocess.run',side_effect=subprocess.TimeoutExpired('df',1))
    def test_timeout_is_not_success(self,run,which):
        self.assertEqual(execute('storage')['code'],124)
    @patch('tux_steward.core.shutil.which',return_value='/usr/bin/tool')
    @patch('tux_steward.core.subprocess.run')
    def test_admin_uses_pkexec(self,run,which):
        run.return_value=subprocess.CompletedProcess([],0,'','')
        execute('apt-clean',True)
        self.assertEqual(run.call_args.args[0],('pkexec','apt-get','clean'))
    def test_real_diagnostic(self):
        r=execute('storage'); self.assertEqual(r['code'],0); self.assertIn('Filesystem',r['output'])

if __name__=='__main__': unittest.main()
