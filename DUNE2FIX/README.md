# Dune II XMS compatibility patch for NuXT

A small patching tool for a specific pair of DOS Dune II executables. It fixes
startup failure with UMB-only memory managers that advertise an XMS interface:

```text
*** Himem.Sys Error (80) *** Run setup and remove XMS and HMA usage.
```

The setup and game query XMS independently of the saved "use extended memory"
setting. This patch makes their XMS installation checks report no manager, so
they follow their existing conventional-memory paths. It does not unload UMBs,
modify CONFIG.SYS, add a TSR or consume additional resident memory.

## Use your own game files

No game executables or data are included. On Linux (Python 3.10+):

```sh
python3 patch.py /path/to/your/DUNE2 --check
python3 patch.py /path/to/your/DUNE2 --output /path/to/new/dune2-patched
```

The output directory must not exist. Both inputs are validated before anything
is created. Unknown versions, modified files and duplicate case variants are
rejected rather than patched by guessing. The originals are never overwritten.
If disk writing fails partway, remove the incomplete new output directory and
retry with adequate space.

Copy SETUPNX.EXE and DUNENX.EXE from that new directory to your DOS DUNE2 folder,
keeping all your existing game data and original executables. Boot normally:

```dos
C:
CD \DUNE2
SETUPNX
DUNENX
```

Select AdLib music/effects, no digitized sound, and no extended memory if offered.
SETUPNX writes the game's normal DUNE.CFG; both original/patched programs share
that configuration. Use DUNENX to play. To undo the workaround, launch the
original executables again or delete the two NX executables.

## Supported files and exact change

| Input | Size | SHA-256 |
| --- | ---: | --- |
| SETUP.EXE | 90,848 | `ea286f1c6d1aa017c31c04443c1e4642613078c3661a8c705c18ee4701cda935` |
| DUNE2.EXE | 367,760 | `e4bc94e900508b156c12be704cc8d12c7b52432b07fe3a3b42f5ceae976ce54d` |

At SETUP file offset 0x8C33, and DUNE2 offsets 0x25368 and 0x2EB67, replace
`CD 2F` (INT 2Fh) with `30 C0` (XOR AL,AL). These are AX=4300h XMS installation
probes followed by a comparison against AL=80h. Six bytes change in total;
all other bytes and executable sizes remain unchanged. Output hashes are
verified by the tool. This is not a general patch for every Dune II release.

## Validation and references

The exact original setup reproduced error 80h in QEMU/MS-DOS 6.22 with a
controlled XMS stub advertising a manager but rejecting functions. Patched
setup reached its menu and patched game reached the main menu under that same
stub. The owner then confirmed the fix works on a real NuXT with USE!UMBS and
DOSMAX. This is startup compatibility validation, not a complete playthrough.

The included tests use synthetic fixtures, not commercial game bytes:

```sh
python3 -m unittest discover -s . -v
```

Relevant interfaces and saved-config layout:
- [Microsoft XMS specification](https://jnz.dk/swag/FAQ/0053.PAS.html)
- [USE!UMBS documentation](https://retrocmp.de/hardware/above-plus/use!umbs.txt)
- [OpenDUNE config layout](https://github.com/OpenDUNE/OpenDUNE/blob/master/src/config.h)

The patcher/tests are MIT licensed. Dune II and its executable code retain
their respective owners' rights. Do not include generated game copies when
publishing this repository.
