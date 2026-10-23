"""Heatshrink 12/4 blocks; leave marginal savings raw to spare DOS decoding time."""
from bsum_codec import bsum
import ctypes
import struct
from pathlib import Path

_library = None


def encode(data):
    global _library
    if not 1 <= len(data) <= 4096:
        raise ValueError('Compression block must contain 1..4096 bytes')
    if _library is None:
        try:
            library = ctypes.CDLL(str(Path(__file__).with_name('_heatshrink.so')))
        except OSError as exc:
            raise RuntimeError('Upload compression needs its Linux helper. Run '
                               'sh linux/build_codec.sh, or turn off Compress uploads.') from exc
        library.slx_heatshrink_encode.argtypes = [ctypes.c_char_p, ctypes.c_size_t,
                                               ctypes.c_void_p, ctypes.c_size_t]
        library.slx_heatshrink_encode.restype = ctypes.c_size_t
        _library = library
    output = ctypes.create_string_buffer(4608)
    size = _library.slx_heatshrink_encode(data, len(data), output, len(output))
    if not size:
        raise RuntimeError('Heatshrink could not encode this block')
    return output.raw[:size]


def packet(data):
    packed = encode(data)
    # Four bytes describe decoded length/checksum. Require >=25% AND >=32 bytes
    # saved overall; this is deliberately conservative on an XT-class CPU.
    if len(data) - len(packed) - 4 < max(32, (len(data)+3)//4):
        return None
    return struct.pack('<HH', len(data), bsum(data)) + packed
