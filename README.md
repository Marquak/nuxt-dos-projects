# NuXT DOS projects

Games, demos and utilities for my NuXT. A bit of gaming, some AdLib tunes,
and seeing what we can get out of a V20 and an 8087.

My setup is a NuXT 2.0 with a V20 at about 10 MHz, an Intel 8087, Trident VGA
with 512 KB VRAM, a reproduction AdLib and DOS 6.22. Some of these also run
on a stock 8088/8086. Others take advantage of the V20's extra instructions.
The code was written with Codex. This collection grew out of trying things
on the real machine and fixing what didn't work. The test notes say what was
checked for each version.

These are the **current versions as of 2026-10-09**. Source, ready-to-run DOS
programs, build scripts and instructions are included. Bring your own DOS.
There are no commercial OS files, BIOS ROMs or complete games from other
publishers in here.

| Project | Current release | DOS files to copy | Purpose |
| --- | --- | --- | --- |
| [NUXMENU](NUXMENU/) | 1.2 | `MENU.EXE` and `NUI.EXE` together | Mouse/keyboard launcher, submenus, file explorer and themes |
| [ORBIT87](ORBIT87/) | 2.0 | `ORBITV20.EXE` for V20; `ORBIT87.EXE` for 8088 | Five-scene VGA/8087 demo with AdLib techno |
| [STARFIELD](STARFIELD/) | 1.0 | `STAR.EXE` | Endless silent star flight with optional 8087 setup |
| [MATRIX](MATRIX/) | 2.0 | `MATRIX.EXE` | Silent digital rain and optional 8087 palette glow |
| [INVADERS](INVADERS/) | 1.0 | `INVADERS.COM` | EGA/VGA arcade game with AdLib effects |
| [NUXTLINK](NUXTLINK/) | 3.1, current UI fixes | `dos/NUXFER.COM` or `NUXFER31.COM` | Verified serial transfers and Linux GUI |
| [SUNSET](SUNSET/) | 3.1 | `SUNSET.EXE` | Endless neon road/palm demo and original synthwave score |
| [DUNE2FIX](DUNE2FIX/) | 1.1 | Generate locally from your own executables | Dune II XMS fix and manual-check removal |
| [NUXTDASH](NUXTDASH/) | 1.3 | `DASH.COM` | CPU, 8087, memory, video, BIOS and drive information |

ORBIT87 and ORBITV20 share one source tree and are both built by its build
script. No separate V20 project is missing. Each program's README.TXT contains
its controls, switches, supported video modes and hardware caveats.

## Run

Copy the listed files to a writable DOS folder and launch the program there.
For NuXTLink, also run `sh NUXTLINK/linux/launch.sh` on Linux; it needs Python 3
and Tkinter. Keep your existing NUXMENU.DAT when updating the menu.

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
music are offered under the [MIT license](LICENSE). Third-party runtime code
retains its own license: see [THIRD_PARTY.md](THIRD_PARTY.md) and each OWLIC.TXT.
Research references are retained in the project documentation. External music
and pictures used as inspiration are not bundled.

DUNE2FIX contains only a patching script, tests, hashes and instructions. You
must supply your own matching Dune II executables. Their copyright is not
covered by this repository's license. Names of hardware and other games are
used descriptively; this is an independent hobby project, not an official
Monotech, Microsoft, Westwood, Taito or other vendor release.
