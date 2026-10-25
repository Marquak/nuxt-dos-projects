# Castle Wolfenstein: unofficial CW7 patch

This patch makes the 1984 IBM PC *Castle Wolfenstein* more comfortable to play
on faster DOS machines. Movement, speech and sound effects are paced using
hardware timers instead of relying on CPU speed. It also fixes scrambled
character graphics on some VGA BIOSes, avoids false tamper detection on newer
DOS versions, and adds Ctrl-X to return to DOS.

The result is one `CW.EXE`. The game's other data files are still needed in the
same folder. This is the original CGA-style game, not a 16-colour EGA remake.
No game executable or data is included here.

## Apply the patch

Install Python 3.10 or newer on Linux or Windows. Run this from the patcher
folder, pointing it at the folder containing your original `CW.EXE`:

```text
python3 apply_cw7.py "/path/to/CastleWo"
```

On Windows, use `py -3` if that is how Python is installed. You can also pass
the full path to `CW.EXE` instead of its folder.

The patcher verifies the input, prepares the complete new EXE, then renames
your old `CW.EXE` to `CW.BAK` and installs the new one as `CW.EXE`. It stops if
`CW.BAK` already exists and restores the old name if installation fails. To
undo the patch, move the patched `CW.EXE` away and rename `CW.BAK` to `CW.EXE`.
Keep your save data files separately.

In DOS, change to the game folder and type `CW`. The default settings are
`/T8 /V2` with the quicker original-style action countdown. `/T2` through
`/T9` adjust movement, with higher numbers slower. `/V1` through `/V4` adjust
speech, again with higher numbers slower. `/A` makes chest, search, key and
vest countdowns follow elapsed time more closely. The older `/F` and `/S`
shortcuts select `/T3` and `/T5`. Ctrl-X exits without saving the current game.

## Credit

The DOS 3.x and later anti-tamper fix draws on
[Nemesis/DivByZero's Castle Wolfenstein research and Python patch on VOGONS](https://www.vogons.org/viewtopic.php?t=106714).
Thanks to them for tracing the false "You're Caught" checks and publishing the
fix. Their patch addresses those checks; it does not remove the separate disk
copy protection. This patcher first converts the specific protected input
listed below to the verified HDD-ready variant, using the differences between
the two supplied files. The two changes are distinct.

## Technical changes

- The main update loop is paced against the BIOS clock. `/T2` through `/T9`
  select the interval. A PC too slow to complete an update cannot be sped up
  by the patch.
- Several CPU-counted sound and effect waits now use PIT timing. Speech delay
  is calibrated at startup; `/V1` through `/V4` adjust the result.
- Chest, search, key and vest counters keep the quicker update-based behaviour
  by default. `/A` uses elapsed BIOS ticks for those counters.
- The analog joystick axes are measured against PIT time instead of a fixed
  number of port reads. The game's existing controls remain.
- The game's custom character set is reinstalled after video-mode changes,
  preventing scrambled walls and figures on some later VGA BIOSes.
- The obsolete interrupt-vector tamper checks are bypassed. Ctrl-X and the
  normal return path restore temporary hooks, the previous video mode and
  speaker state before exiting.
- This is a patch of a verified EXE, not a recompile. An 8086-compatible helper
  is integrated into its MZ load image. It has no second DOS program to load.

## Supported files

Only these two exact 83,840-byte eXoDOS `CastleWo` variants are accepted when
installed as `CW.EXE`:

| Variant | SHA-256 |
| --- | --- |
| HDD-ready `CW.EXE` | `635c4afd97d670a8c1d47d8a855d886f8431c2bc5c03a8c6f9cff31484800c22` |
| Earlier protected `CW.EX_` | `a50eaa2b9ce97b55c65d552c86f6b6f1b2e6ac1416dcaa8265bdef11075f7db9` |

`CW.EX_` is a complete EXE with a different extension, not a compressed file.
To patch it, copy it into a separate game folder as `CW.EXE` first. Both inputs
produce the same 87,936-byte patched `CW.EXE`, SHA-256
`3f84a1fa160b3344deffdff3a2391234be2b1188323a205b7b952b3e552833f8`.
A differently modified EXE is rejected with its size and hash. The helper
depends on fixed game entry points and offsets, so any other HDD mod needs a
separate binary review rather than bypassing the check.

`CW7HELP.BIN` contains only the new helper code, not game data. Its 8086
assembly source is included as `cw7.s`. The patcher and helper were coded
with GPT-6 Codex for the owner's NuXT project. This is an unofficial patch,
not affiliated with Muse Software.

Both supported inputs were tested to produce byte-identical CW7 output. That
exact output passed MS-DOS 6.22/QEMU startup, gameplay, graphics and clean-exit
checks during development. The owner also played CW7 on the NuXT and reported
that it works. No unknown binary variant has been tested.
