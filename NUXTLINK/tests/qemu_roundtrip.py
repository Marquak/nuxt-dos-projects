#!/usr/bin/env python3
"""Serial v8 roundtrip on a disposable copy of the MS-DOS test disk."""
import os
import re
import select
import shutil
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT.parents[1]
sys.path.insert(0, str(ROOT / 'linux'))
from link import (Client, SerialPort, Metadata, RemoteError, crc, wire_path,
                  PUT_BEGIN, PUT_STATUS, PUT_ZERO_RLE, PUT_LZ4, PUT_VERIFY, PUT_END)
from lz4_codec import packet as lz4_packet


def main():
    source_image = Path(os.environ['NUXTLINK_DOS_IMAGE']) if os.environ.get('NUXTLINK_DOS_IMAGE') else WORK / 'work/nuxlink31/dos622.img'
    if not source_image.exists():
        raise SystemExit('Missing MS-DOS test image; set NUXTLINK_DOS_IMAGE to a raw image with its FAT partition at byte offset 8704')
    with tempfile.TemporaryDirectory(prefix='nuxfast-qemu-') as temp:
        folder = Path(temp)
        image = folder / 'dos.img'
        shutil.copyfile(source_image, image)
        autoexec = folder / 'AUTOEXEC.BAT'
        autoexec.write_bytes(b'@ECHO OFF\r\nPATH C:\\DOS\r\nC:\\NXTEST\\NUXFER COM1\r\n')
        for local, target in ((ROOT / 'NUXFER.COM', '::/NXTEST/NUXFER.COM'),
                              (autoexec, '::/AUTOEXEC.BAT')):
            subprocess.run(['mcopy', '-o', '-i', str(image)+'@@8704', str(local), target], check=True)
        process = subprocess.Popen([
            'qemu-system-i386', '-machine', 'pc', '-m', '16', '-accel', 'tcg', '-cpu', '486',
            '-drive', f'file={image},format=raw', '-display', 'none', '-monitor', 'none',
            '-serial', 'pty', '-nic', 'none', '-no-reboot'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        port = None
        try:
            log = b''
            deadline = time.monotonic() + 10
            match = None
            while time.monotonic() < deadline:
                ready, _, _ = select.select([process.stdout, process.stderr], [], [], .2)
                for stream in ready:
                    log += os.read(stream.fileno(), 4096)
                    match = re.search(rb'/dev/pts/\d+', log)
                    if match:
                        break
            if not match:
                raise RuntimeError(f'QEMU serial PTY not found: {log!r}')
            time.sleep(4)
            if process.poll() is not None:
                raise RuntimeError('QEMU exited before DOS server started')
            port = SerialPort(match.group().decode(), 9600)
            client = Client(port, 9600)
            hello, info = client.establish(115200, 'none', 'none', 4096)
            assert hello.startswith('NUXTLINK/8') and info['block_size'] == 4096
            import random
            rng = random.Random(313)
            cases = {
                'ZEROS.BIN': b'\0' * 131072,
                'MIXED.BIN': (b'\0' * 4096 + rng.randbytes(4096) + b'ABCD' * 1024) * 6 + b'END!',
                'SOLID.BIN': rng.randbytes(8193),
            }
            matrix = WORK / 'outputs/FLOPPIES/MATRIX.IMG'
            if matrix.exists():
                cases['MATRIX.IMG'] = matrix.read_bytes()
            for name, contents in cases.items():
                source = folder / ('source-' + name)
                dest = folder / ('readback-' + name)
                source.write_bytes(contents)
                path = '\\NXTEST\\' + name
                started = time.monotonic()
                upload = client.upload(source, path, overwrite=True)
                download = client.download(path, dest, overwrite=True)
                assert dest.read_bytes() == contents, name
                assert client.hash(path)[1] == upload['crc16'], name
                if name == 'ZEROS.BIN':
                    assert download['compressed_blocks'] and download['download_payload_bytes'] < len(contents) // 20
                if name == 'SOLID.BIN':
                    assert not download['compressed_blocks']
                print(f'PASS {name}: {len(contents)} bytes, upload payload '
                      f"{upload['upload_payload_bytes']} bytes, download payload "
                      f"{download['download_payload_bytes']} bytes, roundtrip {time.monotonic()-started:.1f}s", flush=True)
            data = b'ABCD' * 1024
            checksum = crc(data)
            path = '\\NXTEST\\LZ4.BIN'
            meta = Metadata(len(data), 0, 0, 32)
            client.request(PUT_BEGIN, meta.pack()+struct.pack('<HB', checksum, 1)+wire_path(path))
            assert client.scan(PUT_STATUS) == (0, 0xffff)
            packed = lz4_packet(data)
            assert packed is not None
            client.request(PUT_LZ4, packed)
            assert client.scan(PUT_VERIFY) == (len(data), checksum)
            client.request(PUT_END, struct.pack('<H', checksum))
            readback = folder / 'lz4-readback.bin'
            client.download(path, readback, overwrite=True)
            assert readback.read_bytes() == data
            print('PASS explicit LZ4 upload, DOS staging readback, and download', flush=True)
            # A malformed compressed packet must not advance the DOS staging file.
            data = bytes(128)
            checksum = crc(data)
            path = '\\NXTEST\\BADRLE.BIN'
            meta = Metadata(len(data), 0, 0, 32)
            client.request(PUT_BEGIN, meta.pack()+struct.pack('<HB', checksum, 1)+wire_path(path))
            assert client.scan(PUT_STATUS) == (0, 0xffff)
            try:
                client.request(PUT_ZERO_RLE, struct.pack('<HH', 128, checksum)+b'\xff\x80')
            except RemoteError:
                pass
            else:
                raise AssertionError('DOS accepted an oversized zero-run block')
            client.request(PUT_ZERO_RLE, struct.pack('<HH', 128, checksum)+b'\xff')
            assert client.scan(PUT_VERIFY) == (128, checksum)
            client.request(PUT_END, struct.pack('<H', checksum))
            print('PASS malformed zero-run rejected without advancing staging file', flush=True)
        finally:
            if port:
                port.close()
            process.terminate()
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()


if __name__ == '__main__':
    main()
