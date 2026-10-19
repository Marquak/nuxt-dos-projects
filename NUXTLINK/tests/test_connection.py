"""Baud-aware fake UART: replies at the wrong baud really are unreadable here."""
import sys, struct, threading, unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'linux'))
from link import *

class Clock:
    value=0.
    def now(self): return self.value
    def sleep(self,seconds): self.value+=max(seconds,.001)
class Event(threading.Event):
    def __init__(self,clock): super().__init__(); self.clock=clock
    def wait(self,seconds): self.clock.sleep(seconds); return self.is_set()

class BaudPort:
    def __init__(self,clock,baud=9600):
        self.clock=clock;self.server=(baud,'none','none',128);self.baud=9600
        self.flow=self.peer='none';self.pending=0;self.old=None;self.deadline=0
        self.answers=[];self.writes=[];self.drop={};self.reject=False;self.token=0
        self.hello_loss=0;self.confirm_replies=0
    def configure(self,baud,flow='none',peer='none'): self.baud,self.flow,self.peer=baud,flow,peer
    def discard(self): self.answers.clear()
    def poll(self):
        if self.pending and self.clock.now()>=self.deadline:self.server=self.old;self.pending=0
    def info(self):
        baud,flow,peer,block=self.server
        return struct.pack('<8B6I2IHHB',8,1,BAUDS.index(baud),FLOWS.index(flow),PEERS[peer],1,0xb0,3,
                           0,0,0,0,0,0,1000000,2000000,4096,block,self.pending)+b'\\NUXFER.COM\0'
    def write(self,data,timeout):
        self.poll();kind,seq=data[2:4];payload=data[6:-2];self.writes.append((self.baud,kind))
        if self.baud!=self.server[0]:return
        rate=self.baud;response=ACK
        if kind==HELLO:
            if self.hello_loss:self.hello_loss-=1;return
            payload=b'NUXTLINK/8 C:\\ file server'
        elif kind==INFO:payload=self.info()
        elif kind==SETTINGS:
            if self.reject:response=ERROR;payload=b'Required CTS line is low'
            else:
                i,flow,peer,block,self.token=struct.unpack('<BBBHI',payload)
                self.old=self.server;self.server=(BAUDS[i],FLOWS[flow],next(k for k,v in PEERS.items() if v==peer),block)
                self.pending=2;self.deadline=self.clock.now()+12
        elif kind==CONFIRM:
            if self.drop.get('all_confirm'):return
            if struct.unpack('<I',payload)[0]!=self.token:raise AssertionError('wrong token')
            self.pending=0;payload=self.info();self.confirm_replies+=1
        else:raise AssertionError(kind)
        if self.drop.get(kind,0):self.drop[kind]-=1;return
        self.answers.append((rate,encode(response,seq,payload)))
    def read(self,timeout):
        self.poll();self.clock.sleep(timeout)
        while self.answers:
            rate,data=self.answers.pop(0)
            if rate==self.baud:return data
        return b''

class Connections(unittest.TestCase):
    def setUp(self):
        self.clock=Clock();self.clock.value=0
        self.patches=[patch('link.time.monotonic',self.clock.now),patch('link.time.sleep',self.clock.sleep)]
        for p in self.patches:p.start()
    def tearDown(self):
        for p in reversed(self.patches):p.stop()
    def make(self,baud=9600):
        p=BaudPort(self.clock,baud);c=Client(p,9600);c.cancelled=Event(self.clock);c.stopping=Event(self.clock);return p,c
    def test_first_hello_lost_then_successful_9600_bootstrap(self):
        p,c=self.make();p.hello_loss=1
        hello,info=c.establish(115200,'rtscts','both',512)
        self.assertEqual(p.writes[:2],[(9600,HELLO)]*2)
        self.assertEqual(info['baud'],115200);self.assertEqual(c.block_size,512)
        self.assertEqual(p.server,(115200,'rtscts','both',512))
    def test_existing_session_and_legacy_manual_rates(self):
        for rate in (115200,38400,57600,1200):
            with self.subTest(rate=rate):
                p,c=self.make(rate);hello,info=c.establish(115200)
                self.assertIn((rate,HELLO),p.writes);self.assertEqual(info['baud'],115200)
    def test_lost_proposal_ack_still_confirms_known_token(self):
        p,c=self.make();p.drop[SETTINGS]=1
        info=c.apply_settings(115200,'rtscts','none',512)
        self.assertEqual(info['baud'],115200);self.assertEqual(p.pending,0)
    def test_lost_confirm_ack_retries_same_command(self):
        p,c=self.make();p.drop[CONFIRM]=1
        c.compress_uploads=False
        self.assertEqual(c.apply_settings(57600)['baud'],57600)
        self.assertEqual(p.confirm_replies,2);self.assertEqual(c.attempts,5)
        self.assertFalse(c.compress_uploads)
    def test_lost_both_confirm_replies_uses_committed_diagnostics(self):
        p,c=self.make();p.drop[CONFIRM]=2
        self.assertEqual(c.apply_settings(57600)['baud'],57600);self.assertEqual(p.pending,0)
    def test_unconfirmed_switch_rolls_back(self):
        p,c=self.make();p.drop['all_confirm']=True
        with self.assertRaisesRegex(LinkError,'recovered their previous settings'):
            c.apply_settings(115200)
        self.assertEqual(p.baud,9600);self.assertEqual(p.server[0],9600)
    def test_explicit_rejection_keeps_old_settings(self):
        p,c=self.make();p.reject=True
        with self.assertRaises(RemoteError):c.apply_settings(115200,'rtscts')
        self.assertEqual(p.server[0],9600);self.assertEqual(p.baud,9600)
    def test_cancel_discovery_restores_retry_settings(self):
        p,c=self.make();c.cancelled.set()
        with self.assertRaises(Cancelled):c.establish(115200)
        self.assertEqual(c.attempts,5);self.assertFalse(p.writes)
    def test_noop_1200_settings_refresh_timeout(self):
        p,c=self.make(1200);p.configure(1200)
        c.apply_settings(1200)
        self.assertGreater(c.timeout,5)
    def test_large_blocks_negotiate_with_lost_ack_and_slow_baud(self):
        for size in (2048,4096):
            with self.subTest(size=size):
                p,c=self.make();p.drop[CONFIRM]=1
                c.connect()
                info=c.apply_settings(1200,'none','none',size)
                self.assertEqual(info['block_size'],size)
                self.assertEqual(p.server,(1200,'none','none',size))
                self.assertGreater(c.timeout,(size+16)*10/1200)
                self.assertEqual(c.diagnostics()['max_payload'],4096)
    def test_large_block_switch_rolls_back_block_size_too(self):
        p,c=self.make();c.connect();c.apply_settings(115200,'none','none',2048)
        p.drop['all_confirm']=True
        with self.assertRaisesRegex(LinkError,'recovered their previous settings'):
            c.apply_settings(57600,'none','none',4096)
        self.assertEqual(c.block_size,2048)
        self.assertEqual(p.server,(115200,'none','none',2048))
    def test_failed_lock_does_not_restore_or_unlock_other_serial_owner(self):
        with patch('link.os.open',return_value=7),patch('link.os.close') as close, \
             patch('link.fcntl.flock',side_effect=BlockingIOError()), \
             patch('link.fcntl.ioctl') as ioctl,patch('link.termios.tcsetattr') as attrs:
            with self.assertRaises(BlockingIOError):SerialPort('/dev/fake',9600)
            ioctl.assert_not_called();attrs.assert_not_called();close.assert_called_once_with(7)
