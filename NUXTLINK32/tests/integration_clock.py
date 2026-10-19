"""Clock/transfer check. Use ONLY a disposable DOS emulator drive and clock.

Changes the DOS date/time and creates CLOCKCHK.BIN. Call run(Client(...))
with an already connected compiled server; no physical port is opened here.
"""
import datetime
from pathlib import Path
import struct
import tempfile
import time
from link import CLOCK, HASH, Client, Metadata, RemoteError, wire_path
from integration import FaultPort


def run(c):
    def stamp(year, month, day, hour=12, minute=34, second=56, hundredth=0):
        return struct.pack('<H6B',year,month,day,hour,minute,second,hundredth)
    def reject(payload):
        try:c.request(CLOCK,payload)
        except RemoteError:return
        raise AssertionError('Malformed clock request accepted')
    for y,m,d in [(1980,1,1),(2000,2,29),(2024,2,29),(2099,12,31)]:
        actual=c._clock_request(stamp(y,m,d))
        assert abs((actual-datetime.datetime(y,m,d,12,34,56)).total_seconds())<1,actual
    print('PASS DOS date/time readback: 1980, leap years, 2099',flush=True)
    before=c.read_clock()
    for bad in [b'x',bytes(7),bytes(9),stamp(1979,1,1),stamp(2100,1,1),stamp(2026,0,1),stamp(2026,13,1),
                stamp(2026,1,0),stamp(2026,4,31),stamp(2026,2,29),stamp(2026,1,1,24),
                stamp(2026,1,1,12,60),stamp(2026,1,1,12,0,60),stamp(2026,1,1,12,0,0,100)]:
        reject(bad)
    after=c.read_clock();assert 0 <= (after-before).total_seconds() < 5,(before,after)
    print('PASS invalid fields and lengths rejected before changing clock',flush=True)
    c._clock_request(stamp(2024,2,29,23,59,59,80));time.sleep(.4)
    actual=c.read_clock();assert actual.date()==datetime.date(2024,3,1),actual
    print('PASS DOS midnight and leap-day rollover',flush=True)
    # Drop the first ACK. The same set request must replay its old ACK instead
    # of resetting the DOS clock three seconds backwards when retried.
    original=c.transport;p=FaultPort(original);c.transport=p
    try:
        p.arm(CLOCK,'drop');retries=c.retries;actual=c.sync_clock()
        assert c.retries==retries+1
        assert abs((actual-datetime.datetime.now()).total_seconds())<2,actual
    finally:c.transport=original
    print('PASS lost clock ACK recovery without resetting clock on retry',flush=True)
    with tempfile.TemporaryDirectory() as tmp:
        local=Path(tmp);src=local/'SOURCE.BIN';dst=local/'GOT.BIN'
        data=bytes(range(256))*4;src.write_bytes(data)
        meta=Metadata(len(data),0x5c41,0x6540,7)
        c.upload(src,'\\CLOCKCHK.BIN',meta=meta,overwrite=True)
        c.request(HASH,wire_path('\\CLOCKCHK.BIN'))
        reject(stamp(2026,1,1));reject(b'');c.abort()
        c.sync_clock();c.download('\\CLOCKCHK.BIN',dst)
        assert dst.read_bytes()==data
        assert c.stat('\\CLOCKCHK.BIN')==meta
    assert c.diagnostics()['pending']==0
    print('PASS active-operation guard, CRC transfer and unchanged file timestamps/attributes after sync',flush=True)
