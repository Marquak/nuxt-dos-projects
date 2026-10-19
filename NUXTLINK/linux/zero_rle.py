"""Small zero-run codec for NuXT Link Fast file blocks.

Control bytes with bit 7 set repeat zero 1..128 times. Other controls
introduce 1..128 literal bytes. The DOS encoder uses the same format.
"""
import binascii
import struct


def encode(data: bytes) -> bytes:
    out = bytearray()
    pos = 0
    length = len(data)
    while pos < length:
        run = 0
        while pos + run < length and data[pos + run] == 0 and run < 128:
            run += 1
        if run >= 3:
            out.append(0x80 | (run - 1))
            pos += run
            continue
        start = pos
        pos += max(run, 1)
        while pos < length and pos - start < 128:
            if data[pos:pos + 3] == b'\0\0\0':
                break
            pos += 1
        out.append(pos - start - 1)
        out.extend(data[start:pos])
    return bytes(out)


def decode(data: bytes, expected: int) -> bytes:
    if not 0 <= expected <= 4096:
        raise ValueError('Invalid decoded block length')
    out = bytearray()
    pos = 0
    while pos < len(data):
        control = data[pos]
        pos += 1
        count = (control & 0x7f) + 1
        if len(out) + count > expected:
            raise ValueError('Zero-run block exceeds expected length')
        if control & 0x80:
            out.extend(b'\0' * count)
        else:
            if pos + count > len(data):
                raise ValueError('Truncated zero-run literal')
            out.extend(data[pos:pos + count])
            pos += count
    if len(out) != expected:
        raise ValueError('Short zero-run block')
    return bytes(out)


def upload_packet(data: bytes) -> bytes:
    if not 1 <= len(data) <= 4096:
        raise ValueError('Invalid upload block length')
    return struct.pack('<HH', len(data), binascii.crc_hqx(data, 0xffff)) + encode(data)
