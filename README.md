# NuXT DOS projects

Games, demos and utilities I've been making for my NuXT. There are a few games
to play, some AdLib music, and plenty of experiments with old PC hardware.

All the coding for these projects was done with GPT-6 Codex. I came up with
the ideas, steered the design and tested the programs on my NuXT. Third-party
code is credited in the relevant project folders.

My setup is a NuXT 2.0 with a V20 at about 10 MHz, an Intel 8087, Trident VGA
with 512 KB VRAM, a reproduction AdLib and DOS 6.22. Some of these also run
on a stock 8088/8086. Others take advantage of the V20's extra instructions.
I try these on the real machine and keep working on the parts that don't feel
right. Each project's test notes say what has been checked.

These are the **current versions as of 2026-10-21**. Source, ready-to-run DOS
programs, build scripts and instructions are included. Bring your own DOS.
There are no commercial OS files, BIOS ROMs or complete games from other
publishers in here.

| Project | Current release | DOS files to copy | Purpose |
| --- | --- | --- | --- |
| [NUXMENU](NUXMENU/) | 1.5 | `MENU.EXE` and `NUI.EXE` together | Organise DOS games and programs into menus and launch them. |
| [ORBIT87](ORBIT87/) | 2.1 | `ORBITV20.EXE` for V20; `ORBIT87.EXE` for 8088 | An animated space demo with an AdLib techno soundtrack. |
| [STARFIELD](STARFIELD/) | 1.0 | `STAR.EXE` | Fly through an endless field of stars with adjustable speed and density. |
| [MATRIX](MATRIX/) | 2.0 | `MATRIX.EXE` | Falling green characters inspired by The Matrix. |
| [INVADERS](INVADERS/) | 2.0 | `INVADERS.COM` | Shoot the advancing aliens in an XT take on Space Invaders. |
| [NUXTROIDS](NUXTROIDS/) | 1.3 | `NUXTROID.EXE` | Fly a spaceship, dodge asteroids and blast them into smaller pieces. |
| [NUXTRIS](NUXTRIS/) | SP 1.3 / MP 1.0 | `NUXTRIS.EXE`, `SP_TRIS.EXE`, `MP_TRIS.EXE` together | Play Tetris alone or with a friend over serial or LAN. |
| [NUXTLINK](NUXTLINK/) | 4.1 | `NUXFER.COM` | Move files between Linux and DOS over serial, back up folders, and sync its clock. |
| [3D Checkers 88](CHECKERS88/) | 1.2.1 | `CHECK88.EXE` | Play checkers against the computer or a friend on a rotating 3D board, with classical AdLib music. |
| [SUNSET](SUNSET/) | 3.2 | `SUNSET.EXE` | Watch a car cruise towards a neon sunset to an AdLib synthwave soundtrack. |
| [DUNE2FIX](DUNE2FIX/) | 1.1 | Generate locally from your own executables | Fix the XMS startup error and skip the manual-word check in your US 1.07 copy of Dune II. |
| [NUXTDASH](NUXTDASH/) | 1.3 | `DASH.COM` | Check CPU speed, memory, video, ports, BIOS information and drives. |
| [TEXTMODE](TEXTMODE/) | 1.0 | `TEXTMODE.COM`; optional `IRC50.BAT` | Switch DOS between 25 and 50 text rows without ANSI.SYS. |

ORBIT87 and ORBITV20 are built from the same source. NuXTris opens its separate
single-player or multiplayer program from one menu.
Each project's README.TXT has its controls, video modes and hardware details.

[Browse the screenshot gallery](SCREENSHOTS.md). TEXTMODE is a small
command-line utility; its page has usage examples.

## Run

Copy the listed files to a writable DOS folder and launch the program there.
For NuXTris, copy all three EXEs to the same DOS folder. For multiplayer,
copy the three files to both computers. LAN play needs a packet driver and
`MTCPCFG` set to the full DOS path of your mTCP config file, for example
`SET MTCPCFG=C:\NET\MTCP.CFG`. See the [connection guide](NUXTRIS/README.md).
For NuXT Link, also run `sh NUXTLINK/linux/launch.sh` on Linux; it needs Python 3
and Tkinter. Version 4.1 uses serial wire protocol v8 and works with the 4.0
DOS server.

Keep your existing NUXMENU.DAT when updating the menu, and your NuXTris scores
and settings files when updating the game.

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

Most original code, documentation, art, fonts and music are offered under the
[MIT license](LICENSE). NuXTris Multiplayer includes a packet-driver adapter
derived from mTCP and is distributed under GPL-3.0-or-later. Other third-party
code and compiler runtimes keep their own licenses. Details are in
[THIRD_PARTY.md](THIRD_PARTY.md) and the notices in each project folder.
Research references are retained in the project documentation. Checkers includes
credited Mutopia music arrangements under their original licenses; see its
[MUSIC/SOURCES.TXT](CHECKERS88/MUSIC/SOURCES.TXT).

DUNE2FIX contains a patching script, tests, hashes, instructions and a setup
screenshot. You must supply your own matching Dune II executables. The game
and its screenshot are not covered by this repository's MIT license. Names
of hardware and other games are used descriptively. This is an independent
hobby project, not an official release from Monotech or any other vendor.

---

I'm not affiliated with the NuXT project or Monotech in any way. I'm just a big
fan and a hobbyist who enjoys tinkering with this machine. Check out
[Monotech's website](https://monotech.fwscart.com/) for the NuXT and their other projects.

GPT-6 Codex also ran automated checks and emulator tests. Hardware testing
was done on my NuXT.
