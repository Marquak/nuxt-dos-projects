#!/usr/bin/env python3
"""Verify the packaged snapshot's files and SHA-256 hashes. Standard library only."""
import hashlib
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
failures = []
expected = set()
for line in (root/'SHA256SUMS').read_text().splitlines():
    digest, name = line.split('  ', 1)
    path = root/name
    expected.add(name)
    if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
        failures.append(name)
actual = {p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file()
          and p.name != 'SHA256SUMS' and '.git' not in p.relative_to(root).parts
          and '__pycache__' not in p.relative_to(root).parts}
extra = actual - expected
if failures or extra:
    for name in failures: print('Missing or changed:', name)
    for name in sorted(extra): print('Not in snapshot:', name)
    raise SystemExit(1)
print(f'OK: {len(expected)} snapshot files match SHA256SUMS.')
