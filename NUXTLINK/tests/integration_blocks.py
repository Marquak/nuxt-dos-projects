#!/usr/bin/env python3
"""Large-block checks. Use ONLY a disposable DOS drive: overwrites NXBTEST."""
import sys, struct, tempfile
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'linux'))
from link import *
from integration_v3 import FaultSettings


def run(port, baud=9600):
    transport = FaultSettings(port)
    client = Client(transport, baud)
    client.compress_uploads = False
    print(client.connect(), flush=True)
    client.mkdir('\\NXBTEST')
    with tempfile.TemporaryDirectory() as folder:
        src, dst = Path(folder)/'src.bin', Path(folder)/'dst.bin'
        # All byte values, marker bytes and a tail crossing both 4 KiB and 64 KiB.
        data = (bytes(range(256))*257 + b'NX\0\xff')[:65573]
        src.write_bytes(data)
        meta = Metadata(len(data), 0x5c41, 0x645c, 32)
        for size in (2048, 4096):
            info = client.apply_settings(115200, 'rtscts', 'none', size)
            assert info['max_payload'] == 4096 and client.block_size == size
            client.sequence = 252  # Cross sequence wrap during a transfer.
            path = f'\\NXBTEST\\B{size}.BIN'
            transport.arm(PUT_DATA, 'drop')  # Replay must not append twice.
            client.upload(src, path, meta=meta, overwrite=True)
            transport.arm(GET_DATA, 'response')  # Retry must not skip a block.
            client.download(path, dst, overwrite=True)
            assert dst.read_bytes() == data and local_metadata(dst) == meta
            print(f'PASS {size}: 65573-byte upload/readback, CRC, metadata, '
                  'sequence wrap, lost write ACK and corrupt read reply', flush=True)
        # Resume at a non-4K offset, including a short final frame.
        def cancel(done, total):
            if done >= 2048: client.cancelled.set()
        client.apply_settings(115200, 'rtscts', 'none', 2048)
        path = '\\NXBTEST\\RESUME.BIN'
        try: client.upload(src, path, cancel, meta=meta, overwrite=True)
        except Cancelled: pass
        else: raise AssertionError('Upload did not cancel')
        client.cancelled.clear()
        client.apply_settings(115200, 'rtscts', 'none', 4096)
        result = client.upload(src, path, meta=meta, overwrite=True)
        assert result['resumed_bytes'] == 2048, result
        client.apply_settings(115200, 'rtscts', 'none', 2048)
        try: client.download(path, dst, cancel, overwrite=True)
        except Cancelled: pass
        else: raise AssertionError('Download did not cancel')
        client.cancelled.clear()
        client.apply_settings(115200, 'rtscts', 'none', 4096)
        result = client.download(path, dst, overwrite=True)
        assert result['resumed_bytes'] == 2048 and dst.read_bytes() == data
        print('PASS upload/download resume at 2048 after switching to 4096', flush=True)
        # A rejected oversized header must not overwrite buffers or poison framing.
        port.write(b'NX' + struct.pack('<BBH', PUT_DATA, 200, 4097), 3)
        assert client.stat(path) == meta
        for size in (128,256,512):
            assert client.apply_settings(115200,'rtscts','none',size)['block_size']==size
        print('PASS oversized header rejected; all smaller sizes still selectable', flush=True)
    client.apply_settings(9600, 'none', 'none', 128)
    print(f'PASS {transport.injected} deliberate packet faults recovered', flush=True)


if __name__ == '__main__':
    port = SerialPort(sys.argv[1], 9600)
    try: run(port)
    finally: port.close()
