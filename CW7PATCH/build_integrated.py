"""Place the timing helper before a verified MZ image in one executable."""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path


PATCHED_GAME_SHA256 = "4f1738bbab102daaf1dc38ef3ccab2ccda35fe4d0c25cf25c92c9e93654d35ad"
PREFIX_BYTES = 0x1000
GAME_SEGMENT_SHIFT = PREFIX_BYTES // 16


def word(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def set_word(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<H", data, offset, value)


def integrate(source: bytes, helper: bytes, synthetic: bool = False) -> bytes:
    if not synthetic and hashlib.sha256(source).hexdigest() != PATCHED_GAME_SHA256:
        raise ValueError("Expected the exact CW7-patched Castle Wolfenstein image")
    if len(helper) <= 0x100 or len(helper) > 0x0c00:
        raise ValueError("Helper must fit below the reserved startup stack")
    if helper[:0x100] != bytes(0x100):
        raise ValueError("Helper's first instruction must be at offset 0100h")
    if len(source) < 32 or word(source, 0) != 0x5A4D:
        raise ValueError("Expected an MZ executable")
    header_size = word(source, 8) * 16
    if header_size < 32 or header_size > len(source):
        raise ValueError("Invalid MZ header length")
    pages, last = word(source, 4), word(source, 2)
    declared_size = (pages - 1) * 512 + (last or 512)
    if declared_size > len(source):
        raise ValueError("MZ load image extends past the file")
    if word(source, 26) != 0:
        raise ValueError("MZ overlay is not supported")
    if word(source, 22) != 0 or word(source, 20) != 0x000E:
        raise ValueError("Expected the original 0000:000E entry point")
    if not synthetic and (header_size != 512 or len(source) != 83840):
        raise ValueError("Unexpected Castle Wolfenstein image layout")
    if not synthetic and source[declared_size:] != bytes(100):
        raise ValueError("Unexpected bytes after Castle Wolfenstein's MZ load image")

    header = bytearray(source[:header_size])
    body = bytearray(source[header_size:declared_size])
    trailer = source[declared_size:]
    relocation_count = word(header, 6)
    relocation_offset = word(header, 24)
    if relocation_offset + relocation_count * 4 > header_size:
        raise ValueError("Relocation table extends past the header")
    if not synthetic and relocation_count != 1:
        raise ValueError("Unexpected Castle Wolfenstein relocation count")
    for index in range(relocation_count):
        entry = relocation_offset + 4 * index
        location = word(header, entry + 2) * 16 + word(header, entry)
        if location + 2 > len(body):
            raise ValueError("Relocation points outside the game image")
        old_value = word(body, location)
        if old_value + GAME_SEGMENT_SHIFT > 0xFFFF:
            raise ValueError("Relocated segment would overflow")
        set_word(body, location, old_value + GAME_SEGMENT_SHIFT)
        set_word(header, entry + 2, word(header, entry + 2) + GAME_SEGMENT_SHIFT)

    set_word(header, 14, 0)       # Initial stack in the helper prefix.
    set_word(header, 16, 0x0FF0)
    set_word(header, 18, 0)       # No obsolete MZ checksum.
    set_word(header, 20, 0x0100)  # Helper entry point.
    set_word(header, 22, 0)
    image = helper.ljust(PREFIX_BYTES, b"\0") + body
    result = header + image + trailer
    new_load_size = declared_size + PREFIX_BYTES
    set_word(result, 2, new_load_size % 512)
    set_word(result, 4, (new_load_size + 511) // 512)
    return bytes(result)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("helper", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--synthetic", action="store_true", help="internal disposable probe image")
    args = parser.parse_args()
    result = integrate(args.source.read_bytes(), args.helper.read_bytes(), args.synthetic)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(result)
    print(f"Wrote {args.output} ({len(result)} bytes, SHA-256 {hashlib.sha256(result).hexdigest()})")


if __name__ == "__main__":
    main()
