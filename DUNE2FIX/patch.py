#!/usr/bin/env python3
"""Create XMS-disabled copies of two exact Dune II executables. Python 3.10+."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

# Exact versions tested on DOS 6.22 / NuXT. No game code or data is embedded.
SPECS = {
    'SETUP.EXE': {
        'size': 90848,
        'sha256': 'ea286f1c6d1aa017c31c04443c1e4642613078c3661a8c705c18ee4701cda935',
        'output': 'SETUPNX.EXE',
        'offsets': (0x8C33,),
        'patched_sha256': '991176dc3eb9240ed17e6bbfcd7861798d9756747c4d3a2ffd634a0ab0e235b1',
    },
    'DUNE2.EXE': {
        'size': 367760,
        'sha256': 'e4bc94e900508b156c12be704cc8d12c7b52432b07fe3a3b42f5ceae976ce54d',
        'output': 'DUNENX.EXE',
        'offsets': (0x25368, 0x2EB67),
        'patched_sha256': 'efc54a5bdcb195c632e8e482f7de8ec8af04be4eb84cb9c0e37101976962a3de',
    },
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def patch_bytes(data, spec):
    if len(data) != spec['size'] or digest(data) != spec['sha256']:
        raise ValueError('Unsupported executable version or modified file; nothing patched')
    result = bytearray(data)
    for offset in spec['offsets']:
        if result[offset:offset+2] != b'\xcd\x2f':
            raise ValueError('Expected XMS interrupt instruction not found')
        # INT 2Fh -> XOR AL,AL: report XMS absent to the following CMP AL,80h.
        result[offset:offset+2] = b'\x30\xc0'
    result = bytes(result)
    if digest(result) != spec['patched_sha256']:
        raise ValueError('Patched checksum mismatch')
    return result


def find_input(directory, name):
    matches = [p for p in directory.iterdir() if p.name.upper() == name and p.is_file()]
    if len(matches) != 1:
        raise ValueError(f'Need exactly one {name} in the input directory (case-insensitive)')
    return matches[0]


def prepare(directory):
    # Validate BOTH inputs completely before creating any output.
    return {spec['output']: patch_bytes(find_input(directory, name).read_bytes(), spec)
            for name, spec in SPECS.items()}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game_directory', type=Path, help='Your existing DOS Dune II folder')
    parser.add_argument('--output', type=Path, help='New output directory; must not already exist')
    parser.add_argument('--check', action='store_true', help='Validate inputs without writing files')
    args = parser.parse_args(argv)
    if not args.check and args.output is None:
        parser.error('--output is required unless --check is used')
    if args.check and args.output is not None:
        parser.error('Use either --check or --output, not both')
    try:
        files = prepare(args.game_directory)
        if args.check:
            print('Both executables match the tested versions; patch is applicable.')
            return 0
        # Never replace the originals, existing outputs, or an existing directory.
        args.output.mkdir()
        for name, data in files.items():
            with (args.output/name).open('xb') as stream:
                stream.write(data)
        report = {name: digest(data) for name, data in files.items()}
        (args.output/'SHA256.json').write_text(json.dumps(report, indent=2)+'\n')
        print(f'Created SETUPNX.EXE and DUNENX.EXE in {args.output}')
        print('Copy these two files into your DOS DUNE2 folder. Originals are unchanged.')
        return 0
    except (OSError, ValueError) as exc:
        print(f'Error: {exc}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
