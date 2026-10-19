"""Clock protocol validation without touching the host or a real DOS clock."""
import datetime
import struct
import sys
import unittest
from pathlib import Path
from unittest.mock import Mock, patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'linux'))
from link import Client, CLOCK, LinkError, RemoteError, VersionError

STAMP = datetime.datetime(2026, 10, 13, 21, 45, 12, 340000)
PAYLOAD = struct.pack('<H6B', 2026, 10, 13, 21, 45, 12, 34)

class ClockTests(unittest.TestCase):
    def client(self, **kwargs):
        c = Client(None)
        c.request = Mock(**kwargs)
        return c

    def test_local_wall_time_and_fresh_readback(self):
        c = self.client(return_value=PAYLOAD)
        with patch('link.datetime.datetime', wraps=datetime.datetime) as dt:
            dt.now.return_value = STAMP
            self.assertEqual(c.sync_clock(), STAMP)
            dt.now.assert_called_once_with()
        self.assertEqual([call.args for call in c.request.call_args_list], [(CLOCK, PAYLOAD), (CLOCK, b'')])
        self.assertTrue(all(call.kwargs == {'cancellable':False} for call in c.request.call_args_list))

    def test_out_of_range_linux_date_never_sent(self):
        c = self.client()
        for year in (1979, 2100):
            with patch('link.datetime.datetime', wraps=datetime.datetime) as dt:
                dt.now.return_value = STAMP.replace(year=year)
                with self.assertRaises(ValueError): c.sync_clock()
        c.request.assert_not_called()

    def test_bad_readback_rejected(self):
        for data in (b'',PAYLOAD[:-1],PAYLOAD+b'x',struct.pack('<H6B',2026,2,30,12,0,0,0),
                     struct.pack('<H6B',2100,1,1,0,0,0,0),struct.pack('<H6B',2026,1,1,0,0,0,100)):
            with self.subTest(data=data), self.assertRaises(LinkError):
                self.client(return_value=data).read_clock()

    def test_wrong_clock_not_reported_as_synced(self):
        wrong=struct.pack('<H6B',2026,10,12,21,45,12,34)
        for responses in ([wrong], [PAYLOAD,wrong]):
            with patch('link.datetime.datetime', wraps=datetime.datetime) as dt:
                dt.now.return_value=STAMP
                with self.assertRaises(LinkError):self.client(side_effect=responses).sync_clock()

    def test_old_server_explains_upgrade(self):
        with self.assertRaisesRegex(VersionError,'NUXFER.COM'):
            self.client(side_effect=RemoteError('Unknown command')).read_clock()
        with self.assertRaisesRegex(RemoteError,'active operation'):
            self.client(side_effect=RemoteError('Finish active operation')).read_clock()

    def test_delayed_cached_ack_compensates_elapsed_time(self):
        later=struct.pack('<H6B',2026,10,13,21,45,17,34)
        with patch('link.datetime.datetime', wraps=datetime.datetime) as dt, patch('link.time.monotonic',side_effect=[10,15]):
            dt.now.return_value=STAMP
            self.assertEqual(self.client(side_effect=[PAYLOAD,later]).sync_clock(),STAMP+datetime.timedelta(seconds=5))
