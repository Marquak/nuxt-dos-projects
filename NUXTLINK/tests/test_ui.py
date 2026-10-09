"""Real Tk widget tests with an in-memory DOS listing; never opens a serial port.

Run under an X display (e.g. xvfb-run python3 -m unittest discover -s tests).
Without a display these tests skip; the protocol tests still run normally.
"""
import os
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
import tkinter as tk

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'linux'))
from nuxtlink import App
from link import Entry, Metadata, Cancelled
from transfers import Job, Plan


def meta(size=0, attr=32):
    return Metadata(size, 0x5c41, 0x8900, attr)


class UITests(unittest.TestCase):
    def setUp(self):
        try:
            self.root = tk.Tk()
        except tk.TclError as exc:
            self.skipTest('Tk display unavailable: ' + str(exc))
        self.temp = tempfile.TemporaryDirectory()
        self.folder = Path(self.temp.name)
        for directory in (self.folder, self.folder/'SUB'):
            directory.mkdir(exist_ok=True)
            (directory/'BIG.BIN').write_bytes(b'b'*1000)
            (directory/'SMALL.BIN').write_bytes(b's'*20)
        self.errors = []
        self.root.report_callback_exception = lambda *exc: self.errors.append(exc)
        with patch('nuxtlink.ports', return_value=[]):
            self.app = App(self.root, {'local': str(self.folder)}, self.folder/'profiles.json')
        self.root.update()

    def tearDown(self):
        for timer in self.root.tk.splitlist(self.root.tk.call('after','info')):
            self.root.after_cancel(timer)
        self.root.destroy()
        self.temp.cleanup()
        self.assertEqual(self.errors, [], 'Tk callback errors')

    def connect_fixture(self):
        a = self.app
        a.client = SimpleNamespace(drive='C:\\', cancelled=threading.Event(), retries=0)
        self.info = dict(com=1,baud=38400,flow='none',peer='none',block_size=128,
                         msr=0,fifo=1,crc_errors=0,overruns=0,parity_errors=0,
                         framing_errors=0,breaks=0,flow_timeouts=0,
                         free_bytes=10485760,total_bytes=31981568)
        self.listing = [Entry('SMALL.BIN',meta(20)), Entry('BIG.BIN',meta(1000)),
                        Entry('SUB',meta(0,16)), Entry('HIDDEN.SYS',meta(40,7)),
                        Entry('NXLCACHE',meta(0,16),True)]
        a.client.listing = lambda path: self.listing
        a.client.diagnostics = lambda: self.info
        a.fill('\\', self.listing)
        a.display_diagnostics(self.info)
        a.update_controls()

    def make_plan(self):
        a = self.app
        a.plan = Plan('upload', str(self.folder), '\\', 'C:\\', jobs=[
            Job(str(self.folder/'BIG.BIN'), '\\BIG.BIN', meta(1000), 'send'),
            Job(str(self.folder/'SMALL.BIN'), '\\SMALL.BIN', meta(20), 'send')])
        a.fill_plan()
        return a.plan

    def names(self, tree):
        return [tree.set(row,'name') for row in tree.get_children()]

    def settle(self):
        deadline = time.monotonic()+3
        while self.app.busy and time.monotonic()<deadline:
            self.root.update()
            time.sleep(.01)
        self.assertFalse(self.app.busy, 'worker did not complete')
        self.root.update()

    def test_each_pane_keeps_its_sort_across_directories_and_refresh(self):
        self.connect_fixture()
        a = self.app
        a.sort_table(a.local_tree,'size',True)
        a.sort_table(a.tree,'name',True)
        a.local_navigate(self.folder/'SUB')
        self.assertEqual(self.names(a.local_tree), ['BIG.BIN','SMALL.BIN'])
        a.local_navigate(self.folder)
        self.assertEqual(self.names(a.local_tree), ['SUB','BIG.BIN','SMALL.BIN'])
        a.navigate('\\SUB')
        self.settle()
        self.assertEqual(self.names(a.tree), ['SUB','NXLCACHE [resume cache]','SMALL.BIN','HIDDEN.SYS','BIG.BIN'])
        a.refresh()
        self.settle()
        self.assertEqual(a.tree._sort, ('name',True))
        self.assertEqual(a.local_tree._sort, ('size',True))
        self.assertIn('▼', a.tree.heading('name','text'))
        a.sort_table(a.tree,'size',False)
        a.fill('\\',self.listing)
        self.assertEqual(self.names(a.tree)[2:], ['SMALL.BIN','HIDDEN.SYS','BIG.BIN'])

    def test_filters_preserve_sort_and_limit_selection_and_actions(self):
        self.connect_fixture()
        a = self.app
        self.assertTrue(a.send_button.instate(['disabled']))
        self.assertTrue(a.delete_button.instate(['disabled']))
        a.local_tree.selection_set(a.local_tree.get_children())
        a.selection_changed()
        self.assertFalse(a.send_button.instate(['disabled']))
        a.local_filter.set('big')
        self.assertEqual(self.names(a.local_tree), ['BIG.BIN'])
        self.assertEqual(len(a.local_tree.selection()), 1)
        self.assertIn('1,000 file bytes',a.local_summary.get())
        a.remote_filter.set('hidden')
        self.assertEqual(self.names(a.tree), ['HIDDEN.SYS'])
        a.tree.selection_set(a.tree.get_children())
        a.selection_changed()
        self.assertFalse(a.rename_button.instate(['disabled']))
        a.remote_filter.set('cache')
        a.tree.selection_set(a.tree.get_children())
        a.selection_changed()
        self.assertTrue(a.delete_button.instate(['disabled']))
        self.assertTrue(a.get_button.instate(['disabled']))
        a.remote_filter.set('nothing matches')
        self.assertIn('0 / 5 entries',a.remote_summary.get())

    def test_pending_settings_do_not_claim_to_be_applied(self):
        self.connect_fixture()
        a = self.app
        self.assertTrue(a.apply_button.instate(['disabled']))
        a.speed.set('115200')
        self.assertIn('Settings edited',a.connection_hint.get())
        self.assertFalse(a.apply_button.instate(['disabled']))
        a.busy=True;a.update_controls()
        self.assertTrue(a.apply_button.instate(['disabled']))
        a.busy=False;self.info['baud']=115200;a.display_diagnostics(self.info)
        self.assertIn('Connected',a.connection_hint.get())
        self.assertTrue(a.apply_button.instate(['disabled']))

    def test_live_queue_filters_and_progress_survive_filter_changes(self):
        self.connect_fixture()
        a = self.app;plan=self.make_plan();plan.status='Running'
        plan.jobs[0].status='Running';a.busy=True
        a.events.put(('progress',(0,'Running',500,1000)))
        a.poll()
        self.assertEqual(float(a.progress['value']),50)
        self.assertAlmostEqual(float(a.queue_progress['value']),100*500/1020)
        a.queue_filter.set('Pending');a.fill_plan()
        self.assertAlmostEqual(float(a.queue_progress['value']),100*500/1020)
        plan.jobs[0].status='Complete'
        a.events.put(('progress',(0,'Complete',1000,1000)));a.poll()
        self.assertNotIn(0,a.rows)
        a.queue_filter.set('Failed');a.fill_plan()
        plan.jobs[1].status='Failed';plan.jobs[1].reason='CRC verification failed'
        a.events.put(('progress',(1,'Failed',0,20)));a.poll()
        self.assertIn(1,a.rows)
        a.queue_tree.selection_set(a.rows[1])
        with patch('nuxtlink.messagebox.showinfo') as show:
            a.show_job()
            self.assertIn('CRC verification failed',show.call_args.args[1])
            self.assertIn(str(self.folder/'SMALL.BIN'),show.call_args.args[1])
        a.events.put(('error',Cancelled('Stopped')));a.poll()
        self.assertFalse(a.busy)
        self.assertLess(float(a.queue_progress['value']),100)

    def test_clear_queue_and_disconnect_do_not_remove_files(self):
        self.connect_fixture()
        a=self.app;self.make_plan()
        a.clear_queue()
        self.assertIsNone(a.plan)
        self.assertEqual(float(a.queue_progress['value']),0)
        self.assertTrue((self.folder/'BIG.BIN').exists())
        a.events.put(('done',('disconnected','')));a.poll()
        self.assertFalse(a.tree.get_children())
        self.assertEqual(a.remote_frame.cget('text'),'DOS files')
        self.assertIn('Disconnected',a.connection_hint.get())
        self.assertTrue(a.get_button.instate(['disabled']))

    def test_finished_report_and_progress_survive_automatic_refresh(self):
        self.connect_fixture()
        a=self.app;plan=self.make_plan()
        for job in plan.jobs:job.status='Complete'
        plan.status='Complete';a.busy=True
        a.events.put(('progress',(1,'Complete',20,20)))
        a.events.put(('done',('executed',str(self.folder/'last-report.txt'))))
        a.poll();self.settle()
        self.assertIn('Complete',a.status.get())
        self.assertIn('Open report',a.status.get())
        self.assertEqual(float(a.progress['value']),100)
        self.assertEqual(float(a.queue_progress['value']),100)
        self.assertFalse(a.report_button.instate(['disabled']))
        self.assertTrue(a.start_button.instate(['disabled']))

    def test_compact_layout_keeps_controls_and_rows_visible(self):
        self.connect_fixture();a=self.app;self.make_plan()
        self.root.geometry('1024x720');self.root.update()
        a.browsers.sashpos(0,a.browsers.winfo_width()//2)
        a.arrange_panes();self.root.update()
        for widget in (a.local_tree,a.tree,a.queue_tree):
            self.assertGreater(widget.winfo_height(),65)
        a.advanced_open.set(True);a.toggle_advanced();self.root.update()
        for widget in (a.connect_button,a.ftdi_button,a.diag_button,a.cancel_button,a.delete_button):
            self.assertTrue(widget.winfo_viewable())
            self.assertLessEqual(widget.winfo_rootx()+widget.winfo_width(),self.root.winfo_rootx()+self.root.winfo_width())
            self.assertLessEqual(widget.winfo_rooty()+widget.winfo_height(),self.root.winfo_rooty()+self.root.winfo_height())


if __name__=='__main__':
    unittest.main()
