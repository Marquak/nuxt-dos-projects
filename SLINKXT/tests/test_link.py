import unittest,sys,os,pty,termios
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1] / 'linux'))
from link import *
def sum_reference(data):
 s=0
 for byte in data:s=((s>>1)|(s<<15))&0xffff;s=(s+byte)&0xffff
 return s
class ProtocolTests(unittest.TestCase):
 def test_bsum(self):self.assertEqual(bsum(b'123456789'),sum_reference(b'123456789'))
 def test_parser_split_binary(self):
  payload=bytes(range(128));wire=encode(PUT_DATA,255,payload);p=Parser()
  for i,b in enumerate(wire):
   p.feed(bytes([b]));frame=p.pop()
   if i<len(wire)-1:self.assertIsNone(frame)
  self.assertEqual(frame,(PUT_DATA,255,payload))
 def test_parser_recovers_crc_noise_length(self):
  p=Parser();bad=bytearray(encode(GET_DATA,1,b'bad'));bad[-1]^=1
  p.feed(b'noiseNX\x01\x00\xff\xff'+bad+encode(ACK,3,b'SX\x00\xff'))
  self.assertEqual(p.pop(),(ACK,3,b'SX\x00\xff'));self.assertGreater(p.bad_sum,0)
 def test_device_names_and_paths(self):
  for name in ['CON','con.txt','LPT1.COM','COM0','AUX','NUL.X','CLOCK$','../A','A/B','C:X','A..B','A.','TOOLONG99.EXE','A.LONG','A B']:
   with self.assertRaises(ValueError,msg=name):valid_name(name)
  for name in ['DASH.COM','A','FILE~1.EXE',"TEST-1.BIN",'FOO$.TXT']:
   self.assertEqual(valid_name(name),name)
 def test_pty_transport_and_restore(self):
  master,slave=pty.openpty();path=os.ttyname(slave);before=termios.tcgetattr(slave)
  serial=SerialPort(path,38400);serial.write(b'\x00\xffTEST',1);self.assertEqual(os.read(master,20),b'\x00\xffTEST')
  os.write(master,b'\x00\xffBACK');self.assertEqual(serial.read(1),b'\x00\xffBACK')
  serial.close();self.assertEqual(termios.tcgetattr(slave),before)
  os.close(slave);os.close(master)
 def test_all_baud_rates(self):
  for baud in BAUDS:
   master,slave=pty.openpty();serial=SerialPort(os.ttyname(slave),baud)
   attrs=termios.tcgetattr(slave)
   self.assertEqual(attrs[4],getattr(termios,'B'+str(baud)))
   self.assertEqual(attrs[5],getattr(termios,'B'+str(baud)))
   self.assertFalse(attrs[2] & (termios.PARENB|termios.CSTOPB|termios.CRTSCTS))
   serial.close();os.close(slave);os.close(master)
 def test_retry_discards_truncated_false_header(self):
  class Port:
   def __init__(self):self.count=0;self.reply=b''
   def write(self,data,timeout):
    self.count+=1
    self.reply=b'SX\x80\x00\x80\x00' if self.count==1 else encode(ACK,data[3],b'ok')
   def read(self,timeout):
    data=self.reply;self.reply=b'';return data
  p=Port();c=Client(p,timeout=.01)
  self.assertEqual(c.request(HELLO),b'ok');self.assertEqual(c.retries,1)
if __name__=='__main__':unittest.main(verbosity=2)
