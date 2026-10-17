# Dune II fixes for NuXT

If the US 1.07 release of Dune II gives the XMS startup error with a UMB-only
memory manager, this patch lets the game start. It also skips the manual-word
check. Both changes have been tested on my NuXT.

![Dune II setup running after the XMS fix. Original game files are required.](SCREENSHOT.PNG)

*Dune II setup running after the XMS fix. Original game files are required.*

The startup error looks like this:

```text
*** Himem.Sys Error (80) *** Run setup and remove XMS and HMA usage.
```

The setup and game query XMS independently of the saved "use extended memory"
setting. This patch makes their XMS installation checks report no manager, so
they follow their existing conventional-memory paths. It does not unload UMBs,
modify CONFIG.SYS, add a TSR or consume additional resident memory.

## Use your own game files

No game executables or data are included. Use your original SETUP.EXE and
DUNE2.EXE as inputs, even if you already have the older XMS-only patch.
On Linux (Python 3.10+):

```sh
python3 patch.py /path/to/your/DUNE2 --check
python3 patch.py /path/to/your/DUNE2 --output /path/to/new/dune2-patched
```

Choose a new output directory. The script checks both inputs first and refuses
unknown or modified versions, so it never guesses where to patch. It leaves
your originals alone. If a write fails partway through, remove the incomplete
output directory and try again after freeing some space.

Copy SETUPNX.EXE and DUNE2FX.EXE from that new directory to your DOS DUNE2 folder,
keeping all your existing game data and original executables. Boot normally:

```dos
C:
CD \DUNE2
SETUPNX
DUNE2FX
```

Select AdLib music/effects, no digitized sound, and no extended memory if offered.
SETUPNX writes the game's normal DUNE.CFG; both original/patched programs share
that configuration. Use DUNE2FX to play. To undo the workaround, launch the
original executables again or delete SETUPNX.EXE and DUNE2FX.EXE.

## Supported files and exact change

| Input | Size | SHA-256 |
| --- | ---: | --- |
| SETUP.EXE | 90,848 | `ea286f1c6d1aa017c31c04443c1e4642613078c3661a8c705c18ee4701cda935` |
| DUNE2.EXE | 367,760 | `e4bc94e900508b156c12be704cc8d12c7b52432b07fe3a3b42f5ceae976ce54d` |

At SETUP file offset 0x8C33, and DUNE2 offsets 0x25368 and 0x2EB67, replace
`CD 2F` (INT 2Fh) with `30 C0` (XOR AL,AL). These are AX=4300h XMS installation
probes followed by a comparison against AL=80h.

The game also changes `74 07` to `90 90` at 0x17248 and `75 69` to `EB 69`
at 0x1724F. That skips the whole manual-check block after missions 1 and 7,
including its screen setup, through the existing continuation at 0x172BA.

Both XMS fixes in the game remain applied. Seven bytes change in DUNE2.EXE
and two in SETUP.EXE; executable sizes and all other bytes stay unchanged. Output hashes are verified by the tool. This is not a general patch for every Dune II release.

## Validation and references

The exact original setup reproduced error 80h in QEMU/MS-DOS 6.22 with a
controlled XMS stub advertising a manager but rejecting functions. Patched
setup reached its menu and patched game reached the main menu under that same
stub. The owner then confirmed the fix works on a real NuXT with USE!UMBS and
DOSMAX. This is startup compatibility validation, not a complete playthrough.

The combined build was also checked with the same XMS stub in QEMU. Targeted
instruction tests verified that the old build entered the manual check after
missions 1/7 and the new build skipped it, while retaining both XMS fixes. The
owner confirmed the combined build works on the NuXT. This wasn't a complete
campaign playthrough.

The included tests use synthetic fixtures, not commercial game bytes:

```sh
python3 -m unittest discover -s . -v
```

Relevant interfaces and saved-config layout:
- [Microsoft XMS specification](https://jnz.dk/swag/FAQ/0053.PAS.html)
- [USE!UMBS documentation](https://retrocmp.de/hardware/above-plus/use!umbs.txt)
- [OpenDUNE config layout](https://github.com/OpenDUNE/OpenDUNE/blob/master/src/config.h)

The manual-check gate was identified using
[Segra's Dune II reverse-engineering post](https://forum.dune2k.com/topic/20156-dune-2-concrete-bug-fix/)
and matched against this exact US executable. Posted addresses from other
versions must not be applied blindly. No unrelated gameplay patches are added.

The patcher/tests are MIT licensed. Dune II and its executable code retain
their respective owners' rights. Do not include generated game copies when
publishing this repository.
