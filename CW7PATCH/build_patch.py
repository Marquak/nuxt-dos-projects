"""Patch only the two verified 1984 Castle Wolfenstein DOS EXE variants."""

from __future__ import annotations

import hashlib


# Both files were 83,840 bytes in the owner's archive. The first is already
# HDD-ready; the second has the earlier disk-check bytes. Normalize the latter
# before applying the gameplay patch so both routes produce identical CW7.EXE.
HDD_READY_SHA256 = "635c4afd97d670a8c1d47d8a855d886f8431c2bc5c03a8c6f9cff31484800c22"
PROTECTED_SHA256 = "a50eaa2b9ce97b55c65d552c86f6b6f1b2e6ac1416dcaa8265bdef11075f7db9"
PATCHED_GAME_SHA256 = "4f1738bbab102daaf1dc38ef3ccab2ccda35fe4d0c25cf25c92c9e93654d35ad"

PROTECTION_VARIANT = {
    0x04C6: (bytes.fromhex("cd 13"), bytes.fromhex("90 90")),
    0x04D9: (bytes.fromhex("89 46 02"), bytes.fromhex("b8 00 10")),
    0x168C: (bytes.fromhex("cd 13"), bytes.fromhex("90 90")),
    0x16F0: (bytes.fromhex("74"), bytes.fromhex("eb")),
    0x6211: (
        bytes.fromhex("3d a5 a5 75 0b 2e f7 06 f9 5f 00 10"),
        bytes.fromhex("b8 00 10 2e a3 f9 5f b8 a5 a5 09 c0"),
    ),
    0x6291: (bytes.fromhex("8b ec"), bytes.fromhex("89 e5")),
    0x6331: (bytes.fromhex("cd"), bytes.fromhex("90")),
    0x6338: (bytes.fromhex("13"), bytes.fromhex("90")),
    0x73DA: (bytes.fromhex("74 04"), bytes.fromhex("90 90")),
}

# Patch points use the original file offsets. Their exact old bytes are
# checked so a different image layout can never be silently patched.
CW7_PATCHES = {
    0x0276: (b"\x8b\xe5\xcb", b"\xcd\x66\x90"),
    0x0528: (b"\x75\x11", b"\x90\x90"),
    0x0539: (b"\x74", b"\xeb"),
    0x1FEA: (b"\x74", b"\xeb"),
    0x293B: (b"\x7f", b"\xeb"),
    0x2078: (b"\x83\x7e\x02\x00", b"\xcd\x66\x90\x90"),
    0x2098: (b"\x83\x7e\x02\x00", b"\xcd\x66\x90\x90"),
    0x2718: (b"\x81\x7e\x14\x10\x27", b"\xcd\x66\x90\x90\x90"),
    0x4377: (b"\x81\x7e\x02\x10\x27", b"\xcd\x66\x90\x90\x90"),
    0x44DC: (b"\x83\x7e\x04\x00", b"\xcd\x66\x90\x90"),
    0x4509: (b"\x83\x7e\x04\x00", b"\xcd\x66\x90\x90"),
    0x4B21: (b"\x83\x7e\x06\x00", b"\xcd\x66\x90\x90"),
    0x669A: (b"\x81\x7e\x06\xd0\x07", b"\xcd\x66\x90\x90\x90"),
    0x733A: (b"\xe2\xfe", b"\xcd\x66"),
    0x7343: (b"\x50\x50", b"\xcd\x66"),
    0x7378: (b"\xe2\xfe", b"\xcd\x66"),
    0x73A7: (b"\xa2\x89\x9e", b"\xcd\x66\x90"),
    0x7467: (b"\xa3\x90\x9e", b"\xcd\x66\x90"),
    0x746F: (b"\xa3\x90\x9e", b"\xcd\x66\x90"),
    0x7600: (b"\x81\x7e\x14\xe8\x03", b"\xcd\x66\x90\x90\x90"),
    0x761C: (b"\x81\x7e\x12\xd8\x0e", b"\xcd\x66\x90\x90\x90"),
    0x7A49: (b"\xe9\x17\xfb", b"\xcd\x66\x90"),
    0x7661: (b"\x30\xe4\x48", b"\xcd\x66\x90"),
    0x70CC: (b"\x55\xc6\x06", b"\xcd\x66\xc3"),
    0x7E3F: (b"\x83\x7e\x02\x00", b"\xcd\x66\x90\x90"),
    0x7E5F: (b"\x83\x7e\x02\x00", b"\xcd\x66\x90\x90"),
    0x627D: (b"\xcd\x16", b"\xcd\x66"),
    0x136B5: (
        b"Press Ctrl-R to reverse controls",
        b"Ctrl-R reverse / Ctrl-Q monitor ",
    ),
    0x136D6: (
        b"Press Ctrl-Q to select monitor type",
        b"Press Ctrl-X to return to DOS      ",
    ),
}


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def apply_edits(data: bytes, edits: dict[int, tuple[bytes, bytes]]) -> bytes:
    result = bytearray(data)
    for offset, (old, new) in edits.items():
        if len(old) != len(new):
            raise ValueError(f"Length-changing edit at 0x{offset:04X}")
        if result[offset:offset + len(old)] != old:
            raise ValueError(f"Unexpected bytes at 0x{offset:04X}")
        result[offset:offset + len(old)] = new
    return bytes(result)


def patch_game(source: bytes) -> tuple[bytes, str]:
    digest = sha256(source)
    if digest == PROTECTED_SHA256:
        normalized = apply_edits(source, PROTECTION_VARIANT)
        if sha256(normalized) != HDD_READY_SHA256:
            raise ValueError("Protected variant normalization failed")
        variant = "verified protected CW.EX_"
    elif digest == HDD_READY_SHA256:
        normalized = source
        variant = "verified HDD-ready CW.EXE"
    else:
        raise ValueError(
            f"Unsupported CW.EXE variant: {len(source)} bytes, SHA-256 {digest}. "
            "No output written. A different HDD mod needs its own review."
        )
    patched = apply_edits(normalized, CW7_PATCHES)
    if sha256(patched) != PATCHED_GAME_SHA256:
        raise ValueError("Patched game hash mismatch")
    return patched, variant
