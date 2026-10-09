import os,sys,pty,struct,termios,tempfile,threading,unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'linux'))
from link import *
from profiles import Profiles
from ftdi import LatencySession
from operations import plan_delete,execute_delete
from transfers import Plan,Job,execute

class V3Tests(unittest.TestCase):
    def test_old_server_reports_upgrade_required(self):
        class Port:
            def write(self,data,timeout):self.data=encode(ACK,data[3],b'NUXTLINK/2 C:\\ file server')
            def read(self,timeout):data=self.data;self.data=b'';return data
        with self.assertRaisesRegex(VersionError,'matching NuXT Link 3'):
            Client(Port()).connect()

    def test_frame_limit_and_split_large_crc(self):
        for size in (0,127,128,129,255,256,511,512):
            payload=bytes(i%256 for i in range(size));wire=encode(PUT_DATA,9,payload)
            parser=Parser()
            for pos in range(0,len(wire),13):parser.feed(wire[pos:pos+13])
            self.assertEqual(parser.pop(),(PUT_DATA,9,payload))
        with self.assertRaises(ValueError):encode(PUT_DATA,9,b'x'*513)

    def test_cancellation_finishes_current_reply(self):
        event=threading.Event()
        class Port:
            def write(self,data,timeout):self.data=encode(ACK,data[3],b'committed');event.set()
            def read(self,timeout):data=self.data;self.data=b'';return data
        client=Client(Port());client.cancelled=event
        self.assertEqual(client.request(DELETE,b'test'),b'committed')
        with self.assertRaises(Cancelled):client.request(GET_DATA)

    def test_flow_reconfigure_restores_termios(self):
        master,slave=pty.openpty();before=termios.tcgetattr(slave)
        try:
            port=SerialPort(os.ttyname(slave),38400)
            for baud in BAUDS:
                port.configure(baud,'rtscts')
                attrs=termios.tcgetattr(slave)
                self.assertTrue(attrs[2]&termios.CRTSCTS)
                self.assertEqual(attrs[4],getattr(termios,'B'+str(baud)))
            port.configure(115200,'none');self.assertFalse(termios.tcgetattr(slave)[2]&termios.CRTSCTS)
            port.close();self.assertEqual(termios.tcgetattr(slave),before)
        finally:os.close(master);os.close(slave)

    def test_profiles_roundtrip_and_validation(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'settings.json';store=Profiles(path)
            store.save('NuXT',dict(baud=115200,flow='rtscts',peer='both',block=512))
            self.assertEqual(Profiles(path).items['NuXT']['peer'],'both')
            with self.assertRaises(ValueError):store.save('Bad',dict(baud=123))
            store.delete('NuXT');self.assertEqual(Profiles(path).items,{})

    def test_latency_restore_preserves_external_change(self):
        with patch('ftdi.read_latency',return_value=16),patch('ftdi.apply_latency',return_value=True) as apply:
            session=LatencySession('/dev/test');session.apply(2);apply.assert_called_once_with('/dev/test',2,False)
            with patch('ftdi.read_latency',return_value=4):session.restore()
            self.assertEqual(apply.call_count,1)
        with patch('ftdi.read_latency',return_value=16),patch('ftdi.apply_latency',return_value=True) as apply:
            session=LatencySession('/dev/test');session.apply(2)
            with patch('ftdi.read_latency',return_value=2):session.restore()
            self.assertEqual(apply.call_args.args,('/dev/test',16,False))

    def test_failed_job_gets_failed_status(self):
        class ClientStub:
            cancelled=threading.Event()
            def download(self,*args,**kwargs):raise LinkError('corrupt whole-file checksum')
            def abort(self):pass
        with tempfile.TemporaryDirectory() as folder:
            plan=Plan('download',folder,'\\','C:\\',jobs=[Job('\\A',folder+'/A',Metadata(1,0,0,32),'receive')])
            with self.assertRaises(LinkError):execute(ClientStub(),plan)
            self.assertEqual(plan.jobs[0].status,'Failed')
            self.assertEqual(plan.jobs[0].reason,'corrupt whole-file checksum')
            self.assertEqual(plan.status,'Interrupted')

    def test_recursive_delete_order_and_cancel(self):
        class Fake:
            drive='C:\\'
            def __init__(self):self.cancelled=threading.Event();self.deleted=[]
            def diagnostics(self):return {'program':'\\NUXFER.COM'}
            def listing(self,path):
                if path=='\\':return [Entry('TREE',Metadata(0,0,0,16))]
                if path=='\\TREE':return [Entry('FILE.BIN',Metadata(12,1,2,7)),Entry('EMPTY',Metadata(0,0,0,16))]
                return []
            def delete(self,path,meta,protected=False):self.deleted.append(path)
            def abort(self):pass
        with tempfile.TemporaryDirectory() as folder:
            c=Fake();plan=plan_delete(c,'\\',['TREE'],folder)
            self.assertEqual(plan.jobs[-1].source,'\\TREE')
            def progress(i,j,d,t):
                if d:c.cancelled.set()
            with self.assertRaises(Cancelled):execute_delete(c,plan,progress,protected=True)
            self.assertEqual(len(c.deleted),1)
            c.cancelled.clear();execute_delete(c,plan,protected=True)
            self.assertEqual(len(c.deleted),3);self.assertEqual(plan.status,'Complete')
            with self.assertRaises(ValueError):plan_delete(c,'\\',['NUXFER.COM'],folder)

if __name__=='__main__':unittest.main()
