"""Real Tk widget tests with an in-memory DOS listing; never opens a serial port.

Run under an X display (e.g. xvfb-run python3 -m unittest discover -s tests).
Without a display these tests skip; the protocol tests still run normally.
"""
import os
import datetime
import sys
import tempfile
import threading
import time
import termios
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch, Mock
import tkinter as tk

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'linux'))
from slinkxt import App
from link import Entry, Metadata, Cancelled, Client, RemoteError, LinkError, INFO
from transfers import Job, Plan


def meta(size=0, attr=32):
    return Metadata(size, 0x5c41, 0x8900, attr)


class UITests(unittest.TestCase):
    def setUp(self):
        try:
            self.root = tk.Tk(); self.root.withdraw()
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
        with patch('slinkxt.ports', return_value=[]):
            self.app = App(self.root, {'local': str(self.folder)}, self.folder/'profiles.json')
        self.root.update()

    def tearDown(self):
        for timer in self.root.tk.splitlist(self.root.tk.call('after','info')):
            self.root.tk.call('after', 'cancel', timer)
        self.root.destroy()
        self.temp.cleanup()
        self.assertEqual(self.errors, [], 'Tk callback errors')

    def connect_fixture(self):
        a = self.app
        a.speed.set('38400'); a.flow.set('none'); a.block_size.set('128')
        a.client = SimpleNamespace(drive='C:\\', cancelled=threading.Event(), retries=0)
        self.info = dict(com=1,baud=38400,flow='none',peer='none',block_size=128,
                         msr=0,fifo=1,checksum_errors=0,overruns=0,parity_errors=0,
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

    def test_sync_clock_button_idle_only_and_reports_readback(self):
        a=self.app
        self.assertTrue(a.clock_button.instate(['disabled']))
        self.connect_fixture()
        calls=[]
        def sync():
            calls.append(True)
            return datetime.datetime(2026,10,13,21,45,12)
        a.client.sync_clock=sync
        a.busy=True;a.update_controls()
        self.assertTrue(a.clock_button.instate(['disabled']))
        a.clock_button.invoke();self.assertEqual(calls,[])
        a.busy=False;a.update_controls()
        a.clock_button.invoke()
        self.assertTrue(a.cancel_button.instate(['disabled']))
        self.settle()
        self.assertEqual(calls,[True])
        self.assertIn('2026-10-13 21:45:12',a.status.get())
        self.assertFalse(a.clock_button.instate(['disabled']))

    def test_compression_profile_and_busy_state(self):
        a=self.app
        self.assertTrue(a.compression.get())
        a.compression.set(False)
        values=a.profile_values()
        self.assertFalse(values['compression'])
        a.profiles.save('Raw',values)
        self.assertFalse(a.profiles.items['Raw']['compression'])
        a.busy=True;a.update_controls()
        self.assertTrue(a.compression_check.instate(['disabled']))
        a.busy=False;a.update_controls()
        self.assertFalse(a.compression_check.instate(['disabled']))

    def test_upload_readback_has_visible_progress_after_transfer_reaches_100_percent(self):
        self.connect_fixture();a=self.app;plan=self.make_plan()
        job=plan.jobs[0];job.status='Running';plan.status='Running'
        a.progress_event(0,job,1000,1000)
        a.phase_event(0,job,'Verifying on DOS',500,1000)
        a.poll()
        self.assertEqual(a.file_label.cget('text'),'Verify')
        self.assertEqual(a.file_percent.get(),'50%')
        self.assertIn('Verifying on DOS',a.status.get())
        a.phase_event(0,job,'Saving on DOS',0,0)
        a.poll()
        self.assertEqual(str(a.progress.cget('mode')),'indeterminate')
        a.progress_event(0,job,1000,1000)
        a.poll()
        self.assertEqual(a.file_label.cget('text'),'File')
        self.assertEqual(a.file_percent.get(),'100%')

    def test_completed_queue_shows_time_ratio_and_sorts_numerically(self):
        self.connect_fixture();a=self.app;plan=self.make_plan()
        for job,seconds,payload in zip(plan.jobs,(120,9),(100,10)):
            job.status='Complete';job.elapsed_seconds=seconds
            job.result={'resumed_bytes':0,'upload_payload_bytes':payload,'compressed_blocks':1,'raw_blocks':0}
        a.fill_plan()
        self.assertEqual(a.queue_tree.set('job:0','elapsed'),'2m 00s')
        self.assertEqual(a.queue_tree.set('job:0','compression'),'Unknown · 10.00:1 (90% saved)')
        a.sort_table(a.queue_tree,'elapsed',False)
        self.assertEqual(a.queue_tree.get_children()[0],'job:1')
        a.sort_table(a.queue_tree,'compression',False)
        self.assertEqual(a.queue_tree.get_children()[0],'job:1')
        a.queue_tree.selection_set('job:0')
        with patch('slinkxt.messagebox.showinfo') as show:
            a.show_job()
            self.assertIn('Transfer time: 2m 00s',show.call_args.args[1])
            self.assertIn('Unknown · 10.00:1 (90% saved)',show.call_args.args[1])
        a.queue_filter.set('Completed');a.fill_plan()
        self.assertEqual(a.queue_tree.set('job:0','elapsed'),'2m 00s')

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
        a.events.put(('progress',(0,'Running',500,1000,time.monotonic())))
        a.poll()
        self.assertEqual(float(a.progress['value']),50)
        self.assertAlmostEqual(float(a.queue_progress['value']),100*500/1020)
        a.queue_filter.set('Pending');a.fill_plan()
        self.assertAlmostEqual(float(a.queue_progress['value']),100*500/1020)
        plan.jobs[0].status='Complete'
        a.events.put(('progress',(0,'Complete',1000,1000,time.monotonic())));a.poll()
        self.assertNotIn(0,a.rows)
        a.queue_filter.set('Failed');a.fill_plan()
        plan.jobs[1].status='Failed';plan.jobs[1].reason='CRC verification failed'
        a.events.put(('progress',(1,'Failed',0,20,time.monotonic())));a.poll()
        self.assertIn(1,a.rows)
        a.queue_tree.selection_set(a.rows[1])
        with patch('slinkxt.messagebox.showinfo') as show:
            a.show_job()
            self.assertIn('CRC verification failed',show.call_args.args[1])
            self.assertIn(str(self.folder/'SMALL.BIN'),show.call_args.args[1])
        a.events.put(('error',Cancelled('Stopped')));a.poll()
        self.assertFalse(a.busy)
        self.assertLess(float(a.queue_progress['value']),100)

    def test_speed_uses_worker_time_when_progress_updates_arrive_together(self):
        self.connect_fixture()
        a=self.app;plan=self.make_plan()
        job=plan.jobs[0];job.meta=meta(32768);job.status='Running'
        # The worker records 8 KiB over one second. The GUI is stalled until
        # all three events are queued, then processes them in a single poll.
        with patch('slinkxt.time.monotonic',side_effect=[100.,100.5,101.]):
            for done in (0,4096,8192):a.progress_event(0,job,done,32768)
        with patch('slinkxt.time.monotonic',return_value=110.):a.poll()
        self.assertIn('8.0 file KiB/s',a.detail.get())
        self.assertIn('ETA 0m 3s',a.detail.get())
        self.assertEqual(a.started,100.)
        self.assertEqual(a.active_progress,(0,8192))

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

    def test_exit_after_cleanup_errors_releases_connection_once(self):
        self.connect_fixture();a=self.app
        client=a.client
        client.stopping=a.stop_event
        client.abort=Mock(return_value=False)
        client.transport=SimpleNamespace(close=Mock(side_effect=termios.error(5,'Input/output error')))
        latency=SimpleNamespace(restore=Mock(side_effect=OSError('Adapter gone')))
        a.latency_session=latency
        with patch.object(self.root,'destroy') as destroy,patch('slinkxt.messagebox.showerror') as error:
            a.disconnect();a.close_window();self.settle()
            destroy.assert_called_once();error.assert_not_called()
        self.assertIsNone(a.client);self.assertIsNone(a.latency_session)
        self.assertFalse(a.disconnecting)
        self.assertEqual(str(a.progress['mode']), 'determinate')
        client.abort.assert_called_once();client.transport.close.assert_called_once()
        latency.restore.assert_called_once()

    def test_disconnect_while_waiting_for_dead_peer_retains_queue(self):
        self.connect_fixture();a=self.app;plan=self.make_plan()
        class Port:
            interrupted=a.stop_event
            close=Mock()
            def write(self,data,timeout):pass
            def read(self,timeout):time.sleep(min(timeout,.01));return b''
        client=Client(Port(),timeout=40);client.drive='C:\\';client.cancelled=a.cancel_event
        a.client=client
        a.work('Reading DOS folder',lambda:client.request(INFO))
        self.assertFalse(a.connect_button.instate(['disabled']))
        started=time.monotonic()
        with patch('slinkxt.messagebox.showerror') as error:
            a.connect_button.invoke();self.settle()
            self.assertLess(time.monotonic()-started,1.5)
            error.assert_not_called()
        self.assertIs(a.plan,plan);self.assertIsNone(a.client)
        client.transport.close.assert_called_once()
        self.assertIn('Disconnected',a.connection_hint.get())
        self.assertFalse(a.connect_button.instate(['disabled']))
        self.assertTrue((self.folder/'BIG.BIN').exists())

    def test_close_during_discovery_interrupts_private_connection(self):
        a=self.app;a.device.set('/dev/fake')
        class Port:
            close=Mock()
            def __init__(self,path,baud,*,interrupted):self.interrupted=interrupted
            def configure(self,*args):pass
            def discard(self):pass
            def write(self,data,timeout):pass
            def read(self,timeout):time.sleep(min(timeout,.01));return b''
        with patch('slinkxt.SerialPort',Port),patch.object(self.root,'destroy') as destroy:
            a.connect();a.close_window();self.settle()
            destroy.assert_called_once()
        Port.close.assert_called_once()
        self.assertIsNone(a.client)

    def test_reconnect_when_previous_dos_folder_was_removed(self):
        self.connect_fixture();a=self.app;a.client=None
        a.remote='\\REMOVED';a.device.set('/dev/fake');a.latency.set('0')
        port=Mock()
        client=Mock(drive='C:\\')
        client.establish.return_value=('SLINKXT/1 C:\\ file server',self.info)
        client.listing.side_effect=[RemoteError('DOS error 3 (path missing)'),self.listing]
        with patch('slinkxt.SerialPort',return_value=port),patch('slinkxt.Client',return_value=client), \
             patch('slinkxt.LatencySession'),patch('slinkxt.messagebox.showerror') as error:
            a.connect();self.settle()
            error.assert_not_called()
        self.assertEqual([call.args[0] for call in client.listing.call_args_list],['\\REMOVED','\\'])
        self.assertIs(a.client,client);self.assertEqual(a.remote,'\\')
        self.assertEqual(a.location.get(),'C:\\')
        self.assertIn('previous folder',a.status.get().lower())
        port.close.assert_not_called()

    def test_preview_has_only_serial_connection_controls(self):
        self.root.deiconify(); self.root.update()
        a=self.app
        self.assertEqual(a.transport.get(),'serial')
        self.assertFalse(a.transport_box.winfo_ismapped())
        self.assertFalse(a.address_box.winfo_ismapped())
        self.assertTrue(a.port_box.winfo_ismapped())
        self.assertEqual(a.profile_values()['transport'],'serial')

    def test_reconnect_does_not_hide_a_serial_failure_with_folder_fallback(self):
        self.connect_fixture();a=self.app;a.client=None
        a.remote='\\REMOVED';a.device.set('/dev/fake');a.latency.set('0')
        port=Mock();client=Mock(drive='C:\\')
        client.establish.return_value=('SLINKXT/1 C:\\ file server',self.info)
        client.listing.side_effect=LinkError('No valid reply')
        with patch('slinkxt.SerialPort',return_value=port),patch('slinkxt.Client',return_value=client), \
             patch('slinkxt.LatencySession'),patch('slinkxt.messagebox.showerror') as error:
            a.connect();self.settle()
            error.assert_called_once()
        client.listing.assert_called_once_with('\\REMOVED')
        self.assertIsNone(a.client);port.close.assert_called_once()

    def test_finished_report_and_progress_survive_automatic_refresh(self):
        self.connect_fixture()
        a=self.app;plan=self.make_plan()
        for job in plan.jobs:job.status='Complete'
        plan.status='Complete';a.busy=True
        a.events.put(('progress',(1,'Complete',20,20,time.monotonic())))
        a.events.put(('done',('executed',str(self.folder/'last-report.txt'))))
        a.poll();self.settle()
        self.assertIn('Complete',a.status.get())
        self.assertIn('Open report',a.status.get())
        self.assertEqual(float(a.progress['value']),100)
        self.assertEqual(float(a.queue_progress['value']),100)
        self.assertFalse(a.report_button.instate(['disabled']))
        self.assertTrue(a.start_button.instate(['disabled']))

    def test_new_previews_append_and_navigation_never_retargets_jobs(self):
        self.connect_fixture();a=self.app
        one=Plan('upload',str(self.folder),'\\ONE','C:\\',jobs=[
            Job(str(self.folder/'BIG.BIN'),'\\ONE\\BIG.BIN',meta(1000),'send')])
        two=Plan('upload',str(self.folder/'SUB'),'\\TWO','C:\\',jobs=[
            Job(str(self.folder/'SUB/SMALL.BIN'),'\\TWO\\SMALL.BIN',meta(20),'send')])
        for plan in (one,two):a.events.put(('done',('prepared',plan)));a.poll()
        a.local_navigate(self.folder/'SUB');a.fill('\\OTHER',self.listing)
        self.assertEqual(len(a.plan.jobs),2)
        self.assertEqual(a.queue_tree.set('job:0','path'),'C:\\ONE\\BIG.BIN')
        self.assertEqual(a.queue_tree.set('job:1','source'),str(self.folder/'SUB/SMALL.BIN'))
        self.assertEqual(a.queue_tree.set('job:1','path'),'C:\\TWO\\SMALL.BIN')

    def test_mixed_queue_shows_correct_source_and_destination_sides(self):
        self.connect_fixture();a=self.app;one=self.make_plan()
        two=Plan('download',str(self.folder/'SUB'),'\\REMOTE','C:\\',jobs=[
            Job('\\REMOTE\\FILE.BIN',str(self.folder/'SUB/FILE.BIN'),meta(12),'receive')])
        a.events.put(('done',('prepared',two)));a.poll()
        self.assertEqual(a.queue_tree.set('job:0','path'),'C:\\BIG.BIN')
        self.assertEqual(a.queue_tree.set('job:2','source'),'C:\\REMOTE\\FILE.BIN')
        self.assertEqual(a.queue_tree.set('job:2','path'),str(self.folder/'SUB/FILE.BIN'))

    def test_conflicting_preview_does_not_clear_queue(self):
        self.connect_fixture();a=self.app;one=self.make_plan()
        two=Plan('upload',str(self.folder/'SUB'),'\\','C:\\',jobs=[
            Job(str(self.folder/'SUB/BIG.BIN'),'\\BIG.BIN',meta(1000),'send')])
        with patch('slinkxt.messagebox.showerror') as error:
            a.events.put(('done',('prepared',two)));a.poll()
            self.assertTrue(error.called)
        self.assertIs(a.plan,one);self.assertEqual(len(a.plan.jobs),2)
        self.assertFalse(a.busy)

    def test_compact_layout_keeps_controls_and_rows_visible(self):
        self.root.deiconify(); self.root.update()
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
