"""Read-only setup regression tests; never opens a serial device. MIT, 2026 FaiyuetCik."""
import hashlib
import json
from pathlib import Path
import queue
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import patch
import easyread_setup as setup

class PayloadTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        self.addCleanup(patch.stopall)
        patch.object(setup,'ROOT',self.root).start()
        names=['firmware/'+n+'.bin' for n in ['boot_app0','bootloader','partitions','firmware','full']]
        names+=['tools/esptool/esptool.exe','tools/esptool/_internal/example.dll']
        self.manifest={'target':'Seeed Studio reTerminal Sticky','version':'2026-10-10.1','files':{}}
        for name in names:
            path=self.root/name; path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(b'fixture')
            self.manifest['files'][name]=hashlib.sha256(b'fixture').hexdigest()
        self.save()
    def save(self): (self.root/'payload.json').write_text(json.dumps(self.manifest),encoding='utf-8')
    def test_valid(self): self.assertEqual(setup.check_payload()['version'],'2026-10-10.1')
    def test_empty_list(self):
        self.manifest['files']={}; self.save()
        with self.assertRaises(setup.PayloadError): setup.check_payload()
    def test_missing_required_record(self):
        del self.manifest['files']['firmware/full.bin']; self.save()
        with self.assertRaises(setup.PayloadError): setup.check_payload()
    def test_missing_file(self):
        (self.root/'firmware/full.bin').unlink()
        with self.assertRaises(setup.PayloadError): setup.check_payload()
    def test_damaged_dll(self):
        (self.root/'tools/esptool/_internal/example.dll').write_bytes(b'corrupt')
        with self.assertRaises(setup.PayloadError): setup.check_payload()
    def test_bad_hash(self):
        self.manifest['files']['firmware/full.bin']='wrong'; self.save()
        with self.assertRaises(setup.PayloadError): setup.check_payload()
    def test_escaped_path(self):
        self.manifest['files']['../outside.bin']=hashlib.sha256(b'fixture').hexdigest(); self.save()
        with self.assertRaises(setup.PayloadError): setup.check_payload()
    def test_wrong_target(self):
        self.manifest['target']='other board'; self.save()
        with self.assertRaises(setup.PayloadError): setup.check_payload()
    def test_wrong_version(self):
        self.manifest['version']='other version'; self.save()
        with self.assertRaises(setup.PayloadError): setup.check_payload()
    def test_invalid_json(self):
        (self.root/'payload.json').write_text('{')
        with self.assertRaises(setup.PayloadError): setup.check_payload()

class FlowTests(unittest.TestCase):
    def test_progress_v5(self): self.assertEqual(setup.progress_value('Writing at 0x10000 [====] 42.5%'),42.5)
    def test_progress_legacy(self): self.assertEqual(setup.progress_value('(42 %)'),42)
    def test_progress_invalid(self):
        self.assertIsNone(setup.progress_value('999.0%')); self.assertIsNone(setup.progress_value('ordinary log'))
    def test_first_install_address(self):
        steps=list(setup.flash_steps('COM99',False))
        self.assertEqual(len(steps),1); self.assertEqual(steps[0][1][-2],'0x0')
    def test_upgrade_addresses(self):
        steps=list(setup.flash_steps('COM99',True))
        self.assertEqual([s[0] for s in steps],['layout','writing'])
        self.assertIn('verify-flash',steps[0][1]); self.assertEqual(steps[1][1][-2],'0x10000')
    def test_upgrade_stops_on_mismatch(self):
        fake=SimpleNamespace(stdout=iter(['mismatch\n']),returncode=1,wait=lambda:1)
        context=unittest.mock.MagicMock(); context.__enter__.return_value=fake
        owner=SimpleNamespace(events=queue.Queue())
        with patch.object(setup,'check_payload'),patch.object(setup.subprocess,'Popen',return_value=context) as run:
            setup.Setup.worker(owner,'COM99',True)
            self.assertEqual(run.call_count,1)
            self.assertIn('verify-flash',run.call_args.args[0])
        events=list(owner.events.queue)
        self.assertEqual(events[-1],('finish','failed'))
    def test_broken_payload_never_spawns(self):
        owner=SimpleNamespace(events=queue.Queue())
        with patch.object(setup,'check_payload',side_effect=setup.PayloadError('damaged')),patch.object(setup.subprocess,'Popen') as run:
            setup.Setup.worker(owner,'COM99',False); run.assert_not_called()
        self.assertEqual(list(owner.events.queue)[-1],('finish','broken'))
    def test_success_flow_uses_mock_processes(self):
        fake=SimpleNamespace(stdout=iter(['ok\n']),returncode=0,wait=lambda:0)
        context=unittest.mock.MagicMock(); context.__enter__.return_value=fake
        owner=SimpleNamespace(events=queue.Queue())
        with patch.object(setup,'check_payload'),patch.object(setup.subprocess,'Popen',return_value=context) as run:
            setup.Setup.worker(owner,'COM99',True)
            self.assertEqual(run.call_count,2)
        self.assertEqual(list(owner.events.queue)[-1],('finish','done'))

class UiTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.addCleanup(patch.stopall)
        patch.dict(setup.os.environ,{'LOCALAPPDATA':self.temp.name}).start()
        patch.object(setup.list_ports,'comports',return_value=[]).start()
        self.app=setup.Setup(); self.app.ui.withdraw(); self.addCleanup(self.app.ui.destroy)
    def test_no_ports(self): self.assertEqual(self.app.status_key,'none')
    def test_language_switch(self):
        self.app.change_language(1)
        self.assertEqual(self.app.install.cget('text'),'Install EasyREAD')
    def test_one_espressif_candidate(self):
        with patch.object(setup.list_ports,'comports',return_value=[SimpleNamespace(vid=0x303a,device='COM99',description='fixture')]):
            self.app.refresh(); self.assertEqual(self.app.portbox.current(),0)
    def test_multiple_candidates_require_choice(self):
        with patch.object(setup.list_ports,'comports',return_value=[SimpleNamespace(vid=0x303a,device=f'COM{i}',description='fixture') for i in [98,99]]):
            self.app.refresh(); self.assertEqual(self.app.portbox.current(),-1)
    def test_confirmation_required(self):
        with patch.object(setup.messagebox,'showinfo') as alert:
            self.app.start(); self.assertFalse(self.app.busy); alert.assert_called_once()
    def test_runtime_terms_required(self):
        with patch.object(setup.list_ports,'comports',return_value=[SimpleNamespace(vid=0x303a,device='COM99',description='fixture')]):
            self.app.refresh()
        self.app.confirmed.set(True)
        with patch.object(setup.messagebox,'showinfo') as alert,patch.object(setup.threading,'Thread') as thread:
            self.app.start(); self.assertFalse(self.app.busy); thread.assert_not_called()
            self.assertEqual(alert.call_args.args[1],self.app.t('accept'))
    def test_close_is_blocked_while_busy(self):
        self.app.busy=True
        with patch.object(setup.messagebox,'showinfo') as alert:
            self.app.close(); alert.assert_called_once(); self.assertTrue(self.app.ui.winfo_exists())
    def test_log_failure_does_not_block_finish(self):
        self.app.logfile=Path(self.temp.name)/'missing'/'log.txt'
        self.app.events.put(('line','Writing [====] 42.5%\n'))
        self.app.events.put(('finish','done'))
        with patch.object(setup.messagebox,'showinfo'): self.app.poll()
        self.assertFalse(self.app.busy); self.assertEqual(self.app.status_key,'done')

if __name__=='__main__': unittest.main(verbosity=2)
