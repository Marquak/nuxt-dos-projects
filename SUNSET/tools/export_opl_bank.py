#!/usr/bin/env python3
"""Export the demo's six melodic OPL2 patches for Furnace (.sbi import).

The SBI containers carry the static instrument parameters. MUSIC.C owns
note-dependent envelopes, ducking, detuning, echo and hardware percussion.
Layout follows Furnace's documented SoundBlaster instrument importer:
https://github.com/tildearrow/furnace/blob/master/src/engine/fileOpsIns.cpp
"""
from pathlib import Path
import re
import struct

root = Path(__file__).resolve().parents[1]
source = (root / 'MUSIC.C').read_text()
block = source.split('patches[6][11]={', 1)[1].split('\n};', 1)[0]
patches = [[int(n, 0) for n in re.findall(r'0x[0-9a-fA-F]+|\d+', row)]
           for row in re.findall(r'\{([^{}]+)\}', block)]
assert len(patches) == 6 and all(len(p) == 11 for p in patches)
names = ['Echo', 'Pulse Bass', 'Lead', 'Pad Low', 'Pad Mid', 'Pad High']
files = ['ECHO', 'BASS', 'LEAD', 'PADLOW', 'PADMID', 'PADHIGH']
# Representative main-section note levels, including the DOS mix headroom.
carrier = [33, 10, 19, 29, 31, 33]
out = root / 'FURNACE'
out.mkdir(exist_ok=True)
for i, (name, filename, patch) in enumerate(zip(names, files, patches)):
    assert all(0 <= v <= 255 for v in patch)
    assert patch[8] <= 3 and patch[9] <= 3  # YM3812 waveform limit
    patch[3] = carrier[i]
    if i == 1:
        patch[2] = 22  # Bass macro starts bright, then fades to TL 39.
    data = b'SBI\x1a' + ('Horizon ' + name).encode().ljust(32, b'\0')
    data += bytes(patch) + bytes(5)  # Melodic voice + reserved extension.
    assert len(data) == 52
    (out / (filename + '.SBI')).write_bytes(data)
print('Exported six OPL2 SBI instruments:', out)

# A tiny Scream Tracker 3 AdLib module lets Furnace audition the static
# patches together. This is a test phrase, not the soundtrack's score.
header = bytearray(96)
header[:28] = b'Horizon OPL2 patch audition'.ljust(28, b'\0')
header[28:30] = b'\x1a\x10'
struct.pack_into('<6H', header, 32, 2, 6, 1, 0, 0x1320, 1)
header[44:48] = b'SCRM'
header[48:52] = bytes([64, 6, 104, 48])
header[64:96] = bytes(list(range(16, 22)) + [255] * 26)
song = header + bytes([0, 255])
song += struct.pack('<7H', *[7 + i * 5 for i in range(6)], 37)
for filename in files:
    sbi = (out / (filename + '.SBI')).read_bytes()
    ins = bytearray(80)
    ins[0] = 2  # AdLib melodic, not PCM.
    ins[16:27] = sbi[36:47]
    ins[28] = 64
    struct.pack_into('<I', ins, 32, 8363)
    ins[48:76] = sbi[4:32]
    ins[76:80] = b'SCRI'
    song += ins
assert len(song) == 592
pattern = bytearray()
notes = [0x50, 0x20, 0x50, 0x40, 0x44, 0x47]
for row in range(64):
    if row < 48 and row % 8 == 0:
        ch = row // 8
        pattern += bytes([0x20 | ch, notes[ch], ch + 1])
    elif row < 48 and row % 8 == 6:
        pattern += bytes([0x20 | (row // 8), 254, 0])
    elif row == 48:
        for ch in (3, 4, 5):
            pattern += bytes([0x20 | ch, notes[ch], ch + 1])
    elif row == 60:
        for ch in (3, 4, 5):
            pattern += bytes([0x20 | ch, 254, 0])
    pattern += b'\0'
song += struct.pack('<H', len(pattern)) + pattern
(out / 'AUDITION.S3M').write_bytes(song)
