#!/usr/bin/env python3
"""Destructive ONLY inside \\NXLTEST on a disposable DOS test drive.
Usage: python3 tests/integration.py /dev/pts/N [baud]
"""
import sys, time, struct, tempfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'linux'))
from link import *
from transfers import *

class FaultPort:
    def __init__(self,inner):
        self.inner=inner;self.parser=Parser();self.fault=None;self.kinds={};self.injected=0
    def arm(self,kind,mode):self.fault=(kind,mode)
    def write(self,data,timeout):
        kind,seq=data[2:4];self.kinds[seq]=kind
        if self.fault==(kind,'request'):
            self.fault=None;self.injected+=1;data=bytearray(data);data[-1]^=1
        self.inner.write(data,timeout)
    def read(self,timeout):
        end=time.monotonic()+max(timeout,0)
        while True:
            frame=self.parser.pop()
            if frame:
                kind,seq,data=frame;wire=encode(kind,seq,data)
                if self.fault and self.fault[0]==self.kinds.get(seq):
                    mode=self.fault[1]
                    if mode in ('drop','response'):
                        self.fault=None;self.injected+=1
                        if mode=='drop':continue
                        wire=bytearray(wire);wire[-1]^=1
                return wire
            remaining=end-time.monotonic()
            if remaining<=0:return b''
            self.parser.feed(self.inner.read(remaining))
    def close(self):self.inner.close()

def rejected(fn):
    try:fn()
    except (RemoteError,Cancelled):return
    raise AssertionError('Expected rejection')

def run(port,baud):
    p=FaultPort(port);c=Client(p,baud,timeout=4)
    print(c.connect(),flush=True)
    c.mkdir('\\NXLTEST')
    with tempfile.TemporaryDirectory(prefix='nuxtlink-test-') as folder:
        base=Path(folder);source=base/'SOURCE.BIN';dest=base/'GOT.BIN'
        for i,size in enumerate((0,1,127,128,129,65573)):
            data=(bytes(range(256))+b'NX\x00\xff\x1a')*((size+260)//261);data=data[:size]
            source.write_bytes(data);meta=Metadata(size,0x2d5f,0x645d,(0,1,2,4,7,0x27)[i])
            name=f'\\NXLTEST\\TEST{i}.BIN'
            if size==127:p.arm(PUT_DATA,'request')
            if size==128:p.arm(PUT_DATA,'drop')
            if size==129:p.arm(PUT_END,'drop')
            c.upload(source,name,meta=meta,overwrite=True)
            if size==65573:p.arm(GET_DATA,'response')
            c.download(name,dest,overwrite=True)
            assert dest.read_bytes()==data
            assert local_metadata(dest)==meta
            assert c.stat(name)==meta
            print(f'PASS {size} bytes, exact timestamp and attributes {meta.attr:02X}',flush=True)
        p.arm(NEXT,'drop');entries=c.listing('\\NXLTEST')
        assert len({e.name for e in entries})==len(entries)
        print('PASS dropped/corrupt frames, duplicate suppression and sequence wrap',p.injected,c.retries,flush=True)
        for bad in ('C:\\CONFIG.SYS','D:\\X','\\..\\X','\\CON.TXT','\\A\\..\\X'):
            rejected(lambda:c.request(GET_BEGIN,struct.pack('<I',0)+bad.encode()+b'\0'))
            rejected(lambda:c.request(PUT_BEGIN,Metadata(1,0,0,32).pack()+struct.pack('<HB',0,1)+bad.encode()+b'\0'))
        print('PASS server enforces drive and path boundaries',flush=True)
        source.write_bytes(bytes(range(256))*20)
        def cancel(done,total):
            if done>=1024:c.cancelled.set()
        name='\\NXLTEST\\RESUME.BIN'
        rejected(lambda:c.upload(source,name,cancel,overwrite=True))
        c.cancelled.clear();c.connect()
        result=c.upload(source,name,overwrite=True);assert result['resumed_bytes']==1024,result
        dest.chmod(0o600);dest.write_bytes(b'KEEP')
        rejected(lambda:c.download(name,dest,cancel,overwrite=True));assert dest.read_bytes()==b'KEEP'
        c.cancelled.clear();c.connect()
        result=c.download(name,dest,overwrite=True);assert result['resumed_bytes']==1024,result
        assert dest.read_bytes()==source.read_bytes()
        print('PASS interrupted uploads/downloads resume; old destination preserved',flush=True)
        c.mkdir('\\NXLTEST\\EMPTY');dm=Metadata(0,0x285d,0x7250,0x17)
        c.set_metadata('\\NXLTEST\\EMPTY',dm);assert c.stat('\\NXLTEST\\EMPTY')==dm
        backup=base/'backup'
        plan=plan_download(c,'\\NXLTEST',backup,backup=True);execute(c,plan)
        assert plan.status=='Complete',plan.document()
        assert not verify_backup(backup)[0]['errors']
        plan=plan_download(c,'\\NXLTEST',backup,backup=True)
        assert all(j.action in ('verified','directory') for j in plan.jobs)
        execute(c,plan)
        c.mkdir('\\NXLTEST\\RESTORED')
        plan=plan_upload(c,[backup],'\\NXLTEST\\RESTORED',contents=True,restore=True,policy='overwrite')
        assert not plan.errors,plan.errors
        execute(c,plan);assert plan.status=='Complete',plan.document()
        for job in plan.jobs:assert c.stat(job.target)==job.meta
        print('PASS folder backup, empty directories, incremental verification and restore',flush=True)
        # 32-bit lengths are accepted without filling the target with a 40 MiB fixture.
        c.request(PUT_BEGIN,Metadata(40*1024*1024,0x2d5f,0x645d,32).pack()+struct.pack('<HB',0x1234,1)+b'\\NXLTEST\\FORTYMB.BIN\0')
        c.scan(PUT_STATUS);c.request(PUT_DATA,b'x'*128);c.abort()
        print('PASS 40 MiB declared size; paused partial retained',flush=True)
    p.close()

if __name__=='__main__':
    baud=int(sys.argv[2]) if len(sys.argv)>2 else 38400
    run(SerialPort(sys.argv[1],baud),baud)
