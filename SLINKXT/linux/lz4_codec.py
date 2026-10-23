"""Optional raw LZ4 block encoder for Linux-to-DOS uploads.

This uses the system liblz4 encoder. Blocks are independently compressed, so
the DOS side needs only the current output block as its match dictionary.
"""
import ctypes
import ctypes.util
import struct
from bsum_codec import bsum

_library = None
_checked = False


def available():
    global _library, _checked
    if not _checked:
        _checked = True
        name = ctypes.util.find_library('lz4')
        if not name:
            return False
        try:
            library = ctypes.CDLL(name)
        except OSError:
            return False
        library.LZ4_compress_default.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                                                  ctypes.c_int, ctypes.c_int]
        library.LZ4_compress_default.restype = ctypes.c_int
        _library = library
    return True


def packet(data):
    if not data or not available():
        return None
    # A raw LZ4 block has no file header or checksum. Slink-XT supplies both.
    capacity = len(data) + len(data) // 255 + 16
    output = ctypes.create_string_buffer(capacity)
    size = _library.LZ4_compress_default(data, output, len(data), capacity)
    if size <= 0:
        return None
    # Avoid spending DOS decode cycles for a marginal wire saving. Benchmark
    # this gate on real hardware before declaring a permanent speed policy.
    if len(data) - size - 4 < max(64, (len(data) + 15) // 16):
        return None
    return struct.pack('<HH', len(data), bsum(data)) + output.raw[:size]
