# NuXT DOS projects

Games, demos and utilities for my NuXT. A bit of gaming, some AdLib tunes,
and seeing what we can get out of a V20 and an 8087.

My setup is a NuXT 2.0 with a V20 at about 10 MHz, an Intel 8087, Trident VGA
with 512 KB VRAM, a reproduction AdLib and DOS 6.22. Some of these also run
on a stock 8088/8086. Others take advantage of the V20's extra instructions.
The code was written with Codex. This collection grew out of trying things
on the real machine and fixing what didn't work. The test notes say what was
checked for each version.

These are the **current versions as of 2026-10-13**. Source, ready-to-run DOS
programs, build scripts and instructions are included. Bring your own DOS.
There are no commercial OS files, BIOS ROMs or complete games from other
publishers in here.

| Project | Current release | DOS files to copy | Purpose |
| --- | --- | --- | --- |
| [NUXMENU](NUXMENU/) | 1.4 | `MENU.EXE` and `NUI.EXE` together | Organise DOS games and programs into menus and launch them. |
| [ORBIT87](ORBIT87/) | 2.0 | `ORBITV20.EXE` for V20; `ORBIT87.EXE` for 8088 | An animated space demo with an AdLib techno soundtrack. |
| [STARFIELD](STARFIELD/) | 1.0 | `STAR.EXE` | Fly through an endless field of stars. No sound or text. |
| [MATRIX](MATRIX/) | 2.0 | `MATRIX.EXE` | Falling green characters inspired by The Matrix. |
| [INVADERS](INVADERS/) | 2.0 | `INVADERS.COM` | Shoot the advancing aliens in an XT take on Space Invaders. |
| [NUXTROIDS](NUXTROIDS/) | 1.3 | `NUXTROID.EXE` | Fly a spaceship, dodge asteroids and blast them into smaller pieces. |
| [NUXTRIS](NUXTRIS/) | 1.2 | `NUXTRIS.EXE` | Clear rows of falling blocks in endless play or a 25-line challenge. |
| [NUXTLINK](NUXTLINK/) | 3.2.1 | `NUX321.COM` | Move files between Linux and DOS, back up a DOS drive, and sync its clock over serial. |
| [SUNSET](SUNSET/) | 3.2 | `SUNSET.EXE` | A car cruising towards a neon sunset with an AdLib synthwave soundtrack. |
| [DUNE2FIX](DUNE2FIX/) | 1.1 | Generate locally from your own executables | Fix the XMS startup error and skip the manual-word check in your US 1.07 copy of Dune II. |
| [NUXTDASH](NUXTDASH/) | 1.3 | `DASH.COM` | Check CPU speed, memory, video, ports, BIOS information and drives. |

ORBIT87 and ORBITV20 share one source tree and are both built by its build
script. Each program's README.TXT contains
its controls, switches, supported video modes and hardware caveats.

## Run

Copy the listed files to a writable DOS folder and launch the program there.
For NuXTLink, also run `sh NUXTLINK/linux/launch.sh` on Linux; it needs Python 3
and Tkinter. Keep your existing NUXMENU.DAT when updating the menu, and NUXTRIS.DAT/BAK
when updating NuXTris. For NuXTLink, 3.2.1 is the current release. Update both
the DOS server and the complete Linux folder, then restart both.

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

---

I'm not affiliated with the NuXT project or Monotech in any way. I'm just a big
fan and a hobbyist who enjoys tinkering with this machine. Check out
[Monotech's website](https://monotech.fwscart.com/) for the NuXT and their other projects.
