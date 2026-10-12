# NuXT DOS projects

Games, demos and utilities for my NuXT. A bit of gaming, some AdLib tunes,
and seeing what we can get out of a V20 and an 8087.

My setup is a NuXT 2.0 with a V20 at about 10 MHz, an Intel 8087, Trident VGA
with 512 KB VRAM, a reproduction AdLib and DOS 6.22. Some of these also run
on a stock 8088/8086. Others take advantage of the V20's extra instructions.
The code was written with Codex. This collection grew out of trying things
on the real machine and fixing what didn't work. The test notes say what was
checked for each version.

These are the **current versions as of 2026-10-11**. Source, ready-to-run DOS
programs, build scripts and instructions are included. Bring your own DOS.
There are no commercial OS files, BIOS ROMs or complete games from other
publishers in here.

| Project | Current release | DOS files to copy | Purpose |
| --- | --- | --- | --- |
| [NUXMENU](NUXMENU/) | 1.2 | `MENU.EXE` and `NUI.EXE` together | Organise and launch DOS programs from menus, with a file browser for adding them. |
| [ORBIT87](ORBIT87/) | 2.0 | `ORBITV20.EXE` for V20; `ORBIT87.EXE` for 8088 | Watch an animated space demo set to an AdLib techno soundtrack. |
| [STARFIELD](STARFIELD/) | 1.0 | `STAR.EXE` | Fly through an endless field of stars in a silent animated demo. |
| [MATRIX](MATRIX/) | 2.0 | `MATRIX.EXE` | Fill the screen with falling green characters inspired by The Matrix. |
| [INVADERS](INVADERS/) | 2.0 | `INVADERS.COM` | Defend Earth from descending alien waves in a Space Invaders-style game. |
| [NUXTROIDS](NUXTROIDS/) | 1.3 | `NUXTROID.EXE` | Pilot a spaceship, dodge asteroids and shoot them into smaller pieces. |
| [NUXTRIS](NUXTRIS/) | 1.1 | `NUXTRIS.EXE` | Fit falling blocks together and clear complete rows in a Tetris-style game. |
| [NUXTLINK](NUXTLINK/) | 3.2 | `NUXFER32.COM` | Transfer files and folders between Linux and DOS over a serial cable, or back up a DOS drive. |
| [SUNSET](SUNSET/) | 3.1 | `SUNSET.EXE` | Watch a car cruise towards a neon sunset, past palm trees, with a synthwave soundtrack. |
| [DUNE2FIX](DUNE2FIX/) | 1.1 | Generate locally from your own executables | Patch your own Dune II files to avoid the XMS startup error and skip manual-word checks. |
| [NUXTDASH](NUXTDASH/) | 1.3 | `DASH.COM` | Inspect your DOS computer's processor, memory, video, ports, BIOS and drives. |

ORBIT87 and ORBITV20 share one source tree and are both built by its build
script. No separate V20 project is missing. Each program's README.TXT contains
its controls, switches, supported video modes and hardware caveats.

## Run

Copy the listed files to a writable DOS folder and launch the program there.
For NuXTLink, also run `sh NUXTLINK/linux/launch.sh` on Linux; it needs Python 3
and Tkinter. Keep your existing NUXMENU.DAT when updating the menu, and NUXTRIS.DAT/BAK
when updating NuXTris. Replace the complete NuXTLink Linux folder when upgrading.

The higher resolutions are partly for fun and testing. Don't expect a V20 to
fly at 1024x768, even with an 8087. Modes above standard VGA depend on the
card, video BIOS and available VRAM, so check the program's guide if you're
using different hardware.

## Build and verify

See [BUILDING.md](BUILDING.md) for the Linux/Open Watcom toolchain, project
builds and tests. [VALIDATION.md](VALIDATION.md) separates snapshot checks from
historical emulator tests and owner-reported hardware results.
[SNAPSHOT.json](SNAPSHOT.json) and `SHA256SUMS` identify this exact package.

## License and provenance

Project-authored code, documentation, procedural art, fonts and composed FM
music are offered under the [MIT license](LICENSE). Third-party code, including the adapted NuXTris rules and compiler runtimes,
retains its own license: see [THIRD_PARTY.md](THIRD_PARTY.md) and each OWLIC.TXT.
Research references are retained in the project documentation. External music
and pictures used as inspiration are not bundled.

DUNE2FIX contains only a patching script, tests, hashes and instructions. You
must supply your own matching Dune II executables. Their copyright is not
covered by this repository's license. Names of hardware and other games are
used descriptively; this is an independent hobby project, not an official
Monotech, Microsoft, Westwood, Taito or other vendor release.
