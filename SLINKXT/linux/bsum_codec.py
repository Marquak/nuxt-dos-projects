"""BSUM 1.1-compatible rolling BSD checksum for Slink-XT."""
import ctypes
from pathlib import Path

_native = ctypes.CDLL(str(Path(__file__).with_name('_bsum.so')))
_native.slx_bsum.argtypes = (ctypes.c_char_p, ctypes.c_size_t, ctypes.c_uint16)
_native.slx_bsum.restype = ctypes.c_uint16


def bsum(data: bytes, initial: int = 0) -> int:
    return int(_native.slx_bsum(data, len(data), initial))
