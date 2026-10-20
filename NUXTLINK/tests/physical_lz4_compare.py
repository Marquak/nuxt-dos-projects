#!/usr/bin/env python3
"""A/B upload timing against a running NuXT Link 4 serial server.

Creates a fresh DOS folder, verifies each upload by downloading it again,
then removes only this run's files and folder on complete success.
"""
import argparse
import hashlib
import json
import secrets
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'linux'))
from link import (Client, SerialPort, PUT_COMPRESSED, PUT_DATA, PUT_LZ4,
                  PUT_ZERO_RLE, checksums)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--device', required=True)
    parser.add_argument('--source', type=Path, action='append', required=True)
    parser.add_argument('--pattern', choices=('AB', 'ABBA'), default='AB')
    parser.add_argument('--slack', type=int, default=256)
    parser.add_argument('--verify', choices=('download', 'dos-hash'), default='download')
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    if not 0 < args.slack <= 512:
        parser.error('--slack must be 1..512')
    for source in args.source:
        if not source.is_file():
            parser.error(f'No such source file: {source}')
    folder = '\\LZ4' + secrets.token_hex(2).upper()
    port = SerialPort(args.device, 9600)
    client = Client(port, 9600)
    created = []
    finished = False
    results = []
    try:
        hello, info = client.establish(115200, 'rtscts', 'both', 4096,
                                      notify=lambda text: print(text, flush=True))
        print(hello, 'settings', info['baud'], info['flow'], info['block_size'], flush=True)
        client.mkdir(folder)
        print('Created DOS test folder', folder, flush=True)
        original_request = client.request
        commands = {PUT_DATA: 'raw', PUT_COMPRESSED: 'heatshrink',
                    PUT_ZERO_RLE: 'zero-run', PUT_LZ4: 'lz4'}
        counts = {}

        def counted_request(kind, payload=b'', **kwargs):
            if kind in commands:
                label = commands[kind]
                counts[label] = counts.get(label, 0) + 1
            return original_request(kind, payload, **kwargs)

        client.request = counted_request
        with tempfile.TemporaryDirectory(prefix='nux-lz4-readback-') as temp:
            for source_index, source in enumerate(args.source):
                _, source_crc, source_hash = checksums(source)
                for run_index, label in enumerate(args.pattern):
                    policy = 'baseline' if label == 'A' else 'lz4-slack'
                    client.lz4_slack_bytes = 0 if label == 'A' else args.slack
                    path = folder + f'\\F{source_index}{run_index}.BIN'
                    destination = Path(temp) / f'F{source_index}{run_index}.BIN'
                    counts.clear()
                    before_retries = client.retries
                    start = time.monotonic()
                    upload = client.upload(source, path, overwrite=False)
                    elapsed = time.monotonic() - start
                    created.append(path)
                    upload_counts = dict(counts)
                    if args.verify == 'download':
                        download = client.download(path, destination, overwrite=False)
                        actual_hash = hashlib.sha256(destination.read_bytes()).hexdigest()
                        if actual_hash != source_hash or download['sha256'] != source_hash:
                            raise RuntimeError(f'SHA-256 readback differs for {path}')
                    else:
                        _, actual_crc = client.hash(path)
                        if actual_crc != source_crc:
                            raise RuntimeError(f'DOS disk CRC differs for {path}')
                    row = {'source': str(source), 'dos_path': path, 'policy': policy,
                           'slack_bytes': client.lz4_slack_bytes,
                           'upload_seconds': round(elapsed, 3),
                           'upload_payload_bytes': upload['upload_payload_bytes'],
                           'retries': client.retries - before_retries,
                           'codec_blocks': upload_counts, 'sha256': source_hash,
                           'verification': args.verify}
                    results.append(row)
                    print(json.dumps(row, sort_keys=True), flush=True)
                    args.report.write_text(json.dumps(results, indent=2) + '\n')
        finished = True
    finally:
        if finished:
            for path in created:
                client.delete(path, client.stat(path))
            client.delete(folder, client.stat(folder))
            print('Removed test files and folder', folder, flush=True)
        else:
            print('Incomplete test files retained in', folder, 'for inspection', flush=True)
        port.close()


if __name__ == '__main__':
    main()
