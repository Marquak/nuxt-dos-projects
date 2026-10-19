#!/usr/bin/env python3
"""Use ONLY a disposable DOS drive: creates/overwrites NXCTEST files."""
import random
import struct
import sys
import tempfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'linux'))
from link import *
from heatshrink_codec import encode as compress, packet
from integration_v3 import FaultSettings
from integration import rejected


def run(port, baud=9600):
    transport=FaultSettings(port);client=Client(transport,baud)
    print(client.connect(),flush=True);client.mkdir('\\NXCTEST')
    rng=random.Random(8733)
    with tempfile.TemporaryDirectory() as folder:
        src,dst=Path(folder)/'source.bin',Path(folder)/'readback.bin'
        # Raw and compressed blocks alternate, with a raw short tail and >64K.
        data=(b'\0'*4096+rng.randbytes(4096)+b'ABCD'*1024)*6+b'NX\xff\0!'
        src.write_bytes(data);meta=Metadata(len(data),0x5c41,0x645c,7)
        for size in (2048,4096):
            client.apply_settings(115200,'rtscts','none',size)
            client.sequence=254
            path=f'\\NXCTEST\\C{size}.BIN'
            transport.arm(PUT_COMPRESSED,'drop')
            result=client.upload(src,path,meta=meta,overwrite=True)
            assert result['compressed_blocks'] and result['raw_blocks'],result
            assert result['upload_payload_bytes']<len(data)*.6,result
            client.download(path,dst,overwrite=True)
            assert dst.read_bytes()==data and local_metadata(dst)==meta
            print(f'PASS compressed/raw {size}: lost ACK, sequence wrap, readback and RHSA/date verification; '
                  f"{len(data)} file bytes / {result['upload_payload_bytes']} payload bytes",flush=True)
        # Start raw at 2048, resume compressed with 4096, then finish raw again.
        path='\\NXCTEST\\RESUME.BIN'
        client.compress_uploads=False
        client.apply_settings(115200,'rtscts','none',2048)
        def cancel_first(done,total):
            if done>=2048:client.cancelled.set()
        try:client.upload(src,path,cancel_first,meta=meta,overwrite=True)
        except Cancelled:pass
        else:raise AssertionError('Did not cancel')
        client.cancelled.clear();client.compress_uploads=True
        client.apply_settings(115200,'rtscts','none',4096)
        def cancel_second(done,total):
            if done>=6144:client.cancelled.set()
        try:client.upload(src,path,cancel_second,meta=meta,overwrite=True)
        except Cancelled:pass
        else:raise AssertionError('Did not cancel')
        client.cancelled.clear();client.compress_uploads=False
        result=client.upload(src,path,meta=meta,overwrite=True)
        assert result['resumed_bytes']==6144 and result['compressed_blocks']==0,result
        client.download(path,dst,overwrite=True);assert dst.read_bytes()==data
        client.compress_uploads=True
        print('PASS raw/compressed/raw resume at 2048 and 6144, changing block size',flush=True)

        # Decode errors must leave file offset/rolling CRC intact. Follow each
        # group of errors with a valid packet and verify committed bytes.
        good=b'\0'*4096;checksum=crc(good);path='\\NXCTEST\\BAD.BIN'
        meta=Metadata(len(good),0x5c41,0x645c,32)
        client.request(PUT_BEGIN,meta.pack()+struct.pack('<HB',checksum,1)+wire_path(path))
        assert client.scan(PUT_STATUS)==(0,0xffff)
        valid=packet(good);header=valid[:4];packed=valid[4:]
        bad=(b'',struct.pack('<HH',0,checksum)+packed,
             struct.pack('<HH',4097,checksum)+packed,
             header+packed[:-1], header+packed+b'\0',
             struct.pack('<HH',4096,checksum^1)+packed,
             struct.pack('<HH',1,0xffff)+b'\xff\xff')
        for payload in bad:rejected(lambda:client.request(PUT_COMPRESSED,payload))
        transport.arm(PUT_COMPRESSED,'request')
        client.request(PUT_COMPRESSED,valid)
        rejected(lambda:client.request(PUT_END,struct.pack('<H',checksum)))
        transport.arm(PUT_VERIFY,'drop')
        assert client.scan(PUT_VERIFY)==(len(good),checksum)
        transport.arm(PUT_END,'drop')
        assert client.request(PUT_END,struct.pack('<H',checksum))==struct.pack('<H',checksum)
        client.download(path,dst,overwrite=True);assert dst.read_bytes()==good
        print('PASS invalid decoded length, truncation, trailing bytes, padding and decoded CRC rejected without advancing; corrupt frame retry',flush=True)

        for size in (128,256,512):
            client.apply_settings(115200,'none','none',size)
            src.write_bytes(b'\0'*(size+1));path=f'\\NXCTEST\\S{size}.BIN'
            result=client.upload(src,path,overwrite=True)
            assert result['compressed_blocks']==1 and result['raw_blocks']==1,result
            client.download(path,dst,overwrite=True)
            assert dst.read_bytes()==src.read_bytes()
        print('PASS compressed small blocks, raw one-byte tails, no-flow transmission',flush=True)
        # An interrupted readback must preserve the previous destination. A
        # fully staged upload then resumes with zero new payload bytes.
        client.apply_settings(115200,'rtscts','none',4096)
        path='\\NXCTEST\\VERIFY.BIN';src.write_bytes(b'original')
        client.upload(src,path,overwrite=True)
        old_meta=client.stat(path);old_crc=crc(src.read_bytes())
        data=b'New staging contents\0'*500;src.write_bytes(data)
        meta=local_metadata(src);checksum=crc(data)
        client.request(PUT_BEGIN,meta.pack()+struct.pack('<HB',checksum,1)+wire_path(path))
        assert client.scan(PUT_STATUS)==(0,0xffff)
        for i in range(0,len(data),4096):client.request(PUT_DATA,data[i:i+4096])
        status=struct.unpack('<IIHB',client.request(PUT_VERIFY))
        assert status[0]==8192 and status[1]==len(data) and not status[3]
        client.abort()
        assert client.hash(path)==(old_meta,old_crc)
        result=client.upload(src,path,meta=meta,overwrite=True)
        assert result['resumed_bytes']==len(data) and result['upload_payload_bytes']==0
        client.download(path,dst,overwrite=True);assert dst.read_bytes()==data
        src.write_bytes(b'')
        client.upload(src,'\\NXCTEST\\EMPTY.BIN',overwrite=True)
        client.download('\\NXCTEST\\EMPTY.BIN',dst,overwrite=True)
        assert dst.read_bytes()==b''
        print('PASS cancelled readback preserves old destination, fully staged resume, empty-file verification',flush=True)
    client.apply_settings(9600,'none','none',128)
    print(f'PASS compression integration, {transport.injected} deliberate frame faults recovered',flush=True)


if __name__=='__main__':
    port=SerialPort(sys.argv[1],9600)
    try:run(port)
    finally:port.close()
