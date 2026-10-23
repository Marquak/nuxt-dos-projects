#!/usr/bin/env python3
"""Slink-XT serial roundtrip on a disposable MS-DOS test disk."""
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

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'linux'))
from link import (Client, SerialPort, Metadata, RemoteError, bsum, wire_path,
                  PUT_BEGIN, PUT_STATUS, PUT_ZERO_RLE, PUT_LZ4, PUT_VERIFY, PUT_END)
from lz4_codec import packet as lz4_packet


def main():
    image_name = os.environ.get('SLINKXT_DOS_IMAGE')
    source_image = Path(image_name) if image_name else None
    if source_image is None or not source_image.is_file():
        raise SystemExit('Set SLINKXT_DOS_IMAGE to a raw DOS image with its FAT partition at byte offset 8704')
    with tempfile.TemporaryDirectory(prefix='slinkxt-qemu-') as temp:
        folder = Path(temp)
        image = folder / 'dos.img'
        shutil.copyfile(source_image, image)
        autoexec = folder / 'AUTOEXEC.BAT'
        autoexec.write_bytes(b'@ECHO OFF\r\nPATH C:\\DOS\r\nC:\\SLINKXT\\SLINK COM1\r\n')
        test_com = Path(__file__).resolve().parents[1] / 'SLINK.COM'
        subprocess.run(['mmd', '-i', str(image)+'@@8704', '::/SLINKXT'], check=True)
        for local, target in ((test_com, '::/SLINKXT/SLINK.COM'),
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
            assert hello.startswith('SLINKXT/1') and info['block_size'] == 4096
            import random
            rng = random.Random(313)
            cases = {
                'ZEROS.BIN': b'\0' * 131072,
                'MIXED.BIN': (b'\0' * 4096 + rng.randbytes(4096) + b'ABCD' * 1024) * 6 + b'END!',
                'SOLID.BIN': rng.randbytes(8193),
            }
            for name, contents in cases.items():
                source = folder / ('source-' + name)
                dest = folder / ('readback-' + name)
                source.write_bytes(contents)
                path = '\\SLINKXT\\' + name
                started = time.monotonic()
                upload = client.upload(source, path, overwrite=True)
                download = client.download(path, dest, overwrite=True)
                assert dest.read_bytes() == contents, name
                assert sum(upload['codec_blocks'].values()) == upload['compressed_blocks'] + upload['raw_blocks'], name
                assert sum(download['codec_blocks'].values()) == download['compressed_blocks'] + download['raw_blocks'], name
                assert client.hash(path)[1] == upload['bsum16'], name
                if name == 'ZEROS.BIN':
                    assert download['compressed_blocks'] and download['download_payload_bytes'] < len(contents) // 20
                if name == 'SOLID.BIN':
                    assert not download['compressed_blocks']
                print(f'PASS {name}: {len(contents)} bytes, upload payload '
                      f"{upload['upload_payload_bytes']} bytes, download payload "
                      f"{download['download_payload_bytes']} bytes, roundtrip {time.monotonic()-started:.1f}s", flush=True)
            data = b'ABCD' * 1024
            checksum = bsum(data)
            path = '\\SLINKXT\\LZ4.BIN'
            meta = Metadata(len(data), 0, 0, 32)
            client.request(PUT_BEGIN, meta.pack()+struct.pack('<HB', checksum, 1)+wire_path(path))
            assert client.scan(PUT_STATUS) == (0, 0)
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
            checksum = bsum(data)
            path = '\\SLINKXT\\BADRLE.BIN'
            meta = Metadata(len(data), 0, 0, 32)
            client.request(PUT_BEGIN, meta.pack()+struct.pack('<HB', checksum, 1)+wire_path(path))
            assert client.scan(PUT_STATUS) == (0, 0)
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
            # The assembly decoder is called only after bounded validation.
            data = b'A' * 128
            checksum = bsum(data)
            path = '\\SLINKXT\\BADLZ4.BIN'
            meta = Metadata(len(data), 0, 0, 32)
            client.request(PUT_BEGIN, meta.pack()+struct.pack('<HB', checksum, 1)+wire_path(path))
            assert client.scan(PUT_STATUS) == (0, 0)
            bad_blocks = (
                (128, b'\x00\x00\x00'),       # zero match offset
                (128, b'\xf0'),               # missing literal extension
                (128, b'\xf0\xff\xff'),     # truncated extended length
                (5, b'\x10A\x01\x00\x00'),  # final sequence has no literals
            )
            for size, raw in bad_blocks:
                try:
                    client.request(PUT_LZ4, struct.pack('<HH', size, 0)+raw)
                except RemoteError:
                    pass
                else:
                    raise AssertionError(f'DOS accepted malformed LZ4: {raw!r}')
            packed = lz4_packet(data)
            assert packed is not None
            client.request(PUT_LZ4, packed)
            assert client.scan(PUT_VERIFY) == (len(data), checksum)
            client.request(PUT_END, struct.pack('<H', checksum))
            print('PASS malformed LZ4 rejected; valid block then committed', flush=True)
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
