#!/usr/bin/env python3
"""Destructive integration tests: use ONLY a disposable DOS drive (V3TEST)."""
import sys,time,tempfile,struct,argparse
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'linux'))
from link import *
from operations import *
from integration import FaultPort,rejected

class FaultSettings(FaultPort):
    def configure(self,*args):self.inner.configure(*args)

def run(port,baud=38400,handshake=False):
    p=FaultSettings(port);c=Client(p,baud)
    print(c.connect(),flush=True)
    # Previous suite may have left a paused upload; HELLO has closed it.
    initial=c.diagnostics();server=initial['program'];print('Initial',initial,flush=True)
    for baud,flow,peer,size in [(115200,'rtscts','both' if handshake else 'none',512),(9600,'none','none',128),(57600,'rtscts','dsr' if handshake else 'none',256),(38400,'none','none',128)]:
        info=c.apply_settings(baud,flow,peer,size)
        assert (info['baud'],info['flow'],info['peer'],info['block_size'])==(baud,flow,peer,size)
        assert not c.diagnostics()['pending']
        print('PASS negotiated',baud,flow,peer,size,flush=True)
    # Lost final confirmation acknowledgement must recover the committed settings.
    p.arm(CONFIRM,'drop')
    info=c.apply_settings(115200,'rtscts','none',512)
    assert info['baud']==115200 and c.diagnostics()['pending']==0
    print('PASS lost final settings ACK recovered',flush=True)
    with tempfile.TemporaryDirectory() as folder:
        root=Path(folder);src=root/'src.bin';dst=root/'dst.bin'
        data=bytes(range(256))*33+b'NX\0\xff';src.write_bytes(data)
        c.mkdir('\\V3TEST');c.mkdir('\\V3TEST\\SUB')
        meta=Metadata(len(data),0x2d5f,0x645d,7)
        c.upload(src,'\\V3TEST\\SUB\\DATA.BIN',meta=meta,overwrite=True)
        c.download('\\V3TEST\\SUB\\DATA.BIN',dst)
        assert dst.read_bytes()==data and local_metadata(dst)==meta
        print('PASS 512-byte frames and buffered readback CRC/metadata',flush=True)
        c.rename('\\V3TEST\\SUB\\DATA.BIN','\\V3TEST\\SUB\\RENAMED.BIN',meta)
        assert c.stat('\\V3TEST\\SUB\\RENAMED.BIN')==meta
        rejected(lambda:c.rename(server,'\\OTHER.COM'))
        rejected(lambda:c.delete('\\V3TEST\\SUB\\RENAMED.BIN',meta))
        rejected(lambda:c.delete('\\V3TEST\\SUB\\RENAMED.BIN',Metadata(meta.size+1,meta.date,meta.time,meta.attr),protected=True))
        rejected(lambda:c.delete(server,c.stat(server),protected=True))
        plan=plan_delete(c,'\\',['V3TEST'],root)
        assert len(plan.jobs)==3 and plan.jobs[0].source.endswith('RENAMED.BIN')
        def cancel(index,job,done,total):
            if done and job.status=='Complete':c.cancelled.set()
        try:execute_delete(c,plan,cancel,protected=True)
        except Cancelled:pass
        else:raise AssertionError('deletion did not cancel')
        assert sum(j.status=='Complete' for j in plan.jobs)==1
        c.cancelled.clear();execute_delete(c,plan,protected=True)
        assert plan.status=='Complete',plan.document()
        assert all(j.status=='Complete' for j in plan.jobs)
        print('PASS protected deletion, stale preview rejection, rename, cancel/resume recursive delete',flush=True)
        for payload in [b'',b'\x02'+bytes(9)+b'\\X\0',bytes(10)+b'\\..\\X\0']:
            rejected(lambda:c.request(DELETE,payload))
        print('PASS malformed/path-escape deletion requests rejected',flush=True)
    # Unconfirmed change rolls back without a client confirmation.
    old=c.diagnostics();c.request(SETTINGS,struct.pack('<BBBHI',0,0,0,128,123456))
    time.sleep(13.5)
    p.configure(old['baud'],old['flow'],old['peer'])
    actual=c.diagnostics();assert actual['baud']==old['baud'] and not actual['pending']
    print('PASS unconfirmed settings rollback',flush=True)
    c.apply_settings(38400,'none','none',128)
    

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('port');parser.add_argument('baud',type=int,nargs='?',default=38400)
    parser.add_argument('--handshake',action='store_true',help='also exercise required DSR/DCD; needs full wiring')
    args=parser.parse_args();port=SerialPort(args.port,args.baud)
    try:run(port,args.baud,args.handshake)
    finally:port.close()
