# Castle Wolfenstein: unofficial CW patch

If you've tried running the 1984 IBM PC *Castle Wolfenstein* on a fast :P DOS machine, you know it can get away from you. This patch fixes that. Movement, speech, and sound effects now run off hardware timers instead of CPU speed, so the game plays at a sensible pace. It also fixes scrambled character graphics on some VGA BIOSes, stops the false tamper detection on newer DOS versions, and adds Ctrl-X to drop back to DOS.

You end up with a single new `CW.EXE`. Your game's other data files still need to be in the same folder. This is the original CGA-style game, not the 16-colour EGA remake, and no game files are included here.

## Applying the patch

You'll need Python 3.10 or newer, on Linux or Windows. From the patcher folder, point the script at the folder with your original `CW.EXE`:

```text
python3 apply_cw7.py "/path/to/CastleWo"
```

On Windows, use `py -3` if that's how you run Python. You can also give it the full path to `CW.EXE` instead of the folder.

Here's what the patcher does: it checks your input file, builds the complete new EXE, then renames your old `CW.EXE` to `CW.BAK` and puts the new one in its place. If `CW.BAK` already exists, it stops rather than overwrite it. If the install fails partway, it puts your old file back. To undo the patch, move the patched `CW.EXE` out of the way and rename `CW.BAK` back to `CW.EXE`. It's a good idea to keep your save files somewhere separate anyway.

To play, change to the game folder in DOS and type `CW`. The defaults are `/T8 /V2`, with the quicker original-style action countdown.

- `/T2` to `/T9` adjusts movement speed. Higher numbers are slower.
- `/V1` to `/V4` adjusts speech speed. Again, higher is slower.
- `/A` makes the chest, search, key, and vest countdowns track real elapsed time more closely.
- The older `/F` and `/S` shortcuts still work and map to `/T3` and `/T5`.
- Ctrl-X exits immediately, without saving your current game.

## Credit

The fix for the DOS 3.x and later anti-tamper checks builds on [Nemesis/DivByZero's Castle Wolfenstein research and Python patch on VOGONS](https://www.vogons.org/viewtopic.php?t=106714). Many thanks to them for tracking down the false "You're Caught" checks and sharing the fix.

Their patch deals with those checks only. It doesn't remove the separate disk copy protection. For the protected input listed below, this patcher first converts it to the verified HDD-ready variant, using the differences between the two supplied files. The two changes are separate things.

## What changed under the hood

- **Main loop timing:** The update loop is now paced against the BIOS clock, and `/T2` to `/T9` pick the interval. If a PC is too slow to finish an update in time, the patch can't speed it up.
- **Sound and effects:** Several waits that used to be counted in CPU cycles now use the PIT timer. Speech delay is calibrated at startup, and `/V1` to `/V4` tune the result.
- **Counters:** Chest, search, key, and vest counters keep the quicker update-based behaviour by default. `/A` switches them to elapsed BIOS ticks.
- **Joystick:** The analog axes are now measured against PIT time rather than a fixed number of port reads. The existing controls are unchanged.
- **Graphics:** The game's custom character set is reinstalled after any video-mode change, which fixes scrambled walls and figures on some later VGA BIOSes.
- **Tamper checks:** The old interrupt-vector tamper checks are bypassed. Ctrl-X and the normal exit path both restore temporary hooks, the previous video mode, and the speaker state before quitting.
- **Not a recompile:** This patches a verified EXE rather than rebuilding it. A small 8086-compatible helper is built into the MZ load image, so there's no second DOS program to load.

## Supported files

The patcher only accepts these two exact 83,840-byte eXoDOS `CastleWo` variants, installed as `CW.EXE`:

| Variant | SHA-256 |
| --- | --- |
| HDD-ready `CW.EXE` | `635c4afd97d670a8c1d47d8a855d886f8431c2bc5c03a8c6f9cff31484800c22` |
| Earlier protected `CW.EX_` | `a50eaa2b9ce97b55c65d552c86f6b6f1b2e6ac1416dcaa8265bdef11075f7db9` |

Despite the different extension, `CW.EX_` is a complete EXE, not a compressed file. To patch it, copy it into a separate game folder and rename it `CW.EXE` first.

Both inputs produce the same 87,936-byte patched `CW.EXE`, with SHA-256 `3f84a1fa160b3344deffdff3a2391234be2b1188323a205b7b952b3e552833f8`. If you give it any other modified EXE, it will refuse and show you the file's size and hash. The helper relies on fixed game entry points and offsets, so supporting a different HDD mod would take its own binary review, not just a relaxed check.

`CW7HELP.BIN` holds only the new helper code, with no game data. The 8086 assembly source is included as `cw7.s`. The patcher and helper were written with GPT-6 Codex for the owner's NuXT project. This is an unofficial patch and isn't affiliated with Muse Software.

Coded with ChatGPT-6 Codex and tested on humans, lol

## Testing

Both supported inputs were tested and produce byte-identical CW output. That output passed startup, gameplay, graphics, and clean-exit checks on MS-DOS 6.22 under QEMU during development, played it on a NEC V20 @ 9.55Mhz with a SVGA Card, plus DOSBox and it works well. No other binary variants have been tested, if any others exist?
