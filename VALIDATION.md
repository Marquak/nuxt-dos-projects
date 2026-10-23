# Snapshot checks, 2026-10-23

This file covers preparation of the GitHub package. The TESTING/TESTS files
inside each project also retain earlier development results. Those historical
tests were not all repeated for this packaging pass. Sections below the current
release notes document earlier publication passes.

## Slink-XT 1.0

`SLINKXT/SLINK.COM` is the 8088-compatible DOS server and the matching Linux
client is under `SLINKXT/linux`. The 1.0 rebuild passed 90 host checks with
21 display-only Tk checks skipped in the headless run. A disposable QEMU
MS-DOS roundtrip passed raw, zero-run and LZ4 transfers, checksum and staging
readback, and malformed compressed-block rejection. The owner also completed
four-file upload and download queues on the physical NuXT/SvarDOS at 115200
baud, RTS/CTS and 4096-byte blocks with zero reported failures. See
[SLINKXT/README.md](SLINKXT/README.md) for timings and limits. NuXT Link 4.1
remains in this repository as the deprecated predecessor.

## Combined NuXTris release

The current `NUXTRIS` folder contains one menu and two games: `NUXTRIS.EXE`,
`SP_TRIS.EXE` 1.3, and `MP_TRIS.EXE` 1.0. The older separate multiplayer folder
and single-player 1.2 files were removed. The selector opens one game at a
time and returns to its menu after the game exits. Existing player scores,
settings and match history were not included in the repository. Keep personal
files when installing this update.

The local release package passed DOSBox-X launcher, return, missing-file and
VGA checks. SP 1.3 passed 144 focused checks; MP 1.0 is the tested 0.7.0
gameplay promoted with a corrected help label. Its prior serial and LAN
checks are in `NUXTRIS/SOURCE/MP/TESTS.TXT`. This exact combined package still
needs a physical NuXT launch check. The repository files were compared with
the local release and the checksum inventory was regenerated.

The first NuXT launch reported `CANNOT OPEN GAME FOLDER` after finding the
child EXE. The selector now strips the trailing backslash before DOS `CHDIR`
and uses the current folder directly when that is where the child resides.
Focused DOSBox-X checks passed from a different starting directory, with a
forced `CHDIR` failure, and with the MP EXE missing. The replacement launcher
still needs a physical NuXT retest.

## NuXT Link 4.1 LZ4 upload choice

The physical NuXT A/B tests at 115200 baud and 4096-byte blocks found faster verified uploads when Linux favored LZ4 over slightly smaller heatshrink packets. See [NUXTLINK/TESTING.md](NUXTLINK/TESTING.md) for times, file checks, and the separate 57600-baud comparison. The DOS wire protocol remains version 8.

## NuXT Link 4.0 serial promotion

The 0.3.2 Fast preview became the main serial release, `NUXTLINK/NUXFER.COM`,
with its matching Linux client and wire version 8. The earlier 3.5.2 UDP LAN
package used a wire-v6 client and is not included in the current checkout.
Personal profiles and transfer reports are not in this package.
The new serial client imports serial profiles from the Fast preview and the
older combined client into a separate store without editing those sources.

The owner completed four-file queues in both directions on the NuXT using
the 0.3.2 preview. Codex then interrupted a 128 KiB upload after 16 KiB on
the physical NuXT, reconnected, resumed at the exact offset, downloaded the
file, and compared it byte for byte. The 4.0 DOS rebuild targets 8086; 74 of
95 host tests passed, with 21 Tk tests skipped for lack of a display. A
disposable MS-DOS 6.22/QEMU run passed zero, mixed, random and floppy-image
roundtrips plus LZ4 and malformed-frame checks. The rebuilt 4.0 banner has
not yet had a separate physical startup check.

## NuXTris Multiplayer 0.6.2 and README review

Published the tested multiplayer preview as a separate folder from the
single-player game. The source, executable, build scripts, DOS guide, license
notices and development test notes match the local 0.6.2 release. Player
settings, match logs and USB-adapter settings were left out. The README gives
the full `MTCPCFG` path example and explains how its `PACKETINT` value relates
to the loaded packet driver. The program itself was not changed for this
publication pass.

The owner reported smooth NuXT-to-DOSBox-X LAN play with 0.6.1. Version 0.6.2
was checked in DOSBox-X after a diagnostic counter fix, as recorded in
The historical 0.6.2 tests are retained in repository history. This earlier
upload did not claim a fresh NuXT hardware run.
The repository's project introductions were edited for clearer, more natural
wording without changing their binaries or technical requirements.

## NuXTLink 3.3.3 and 3D Checkers 88 1.2.1

Published the tested local releases with source, build scripts and credits.
NuXTLink now has one DOS binary, NUXFER.COM (server version 3.3.1), alongside
the 3.3.3 Linux client. Older binary aliases have been removed. Both ends use
wire protocol v6; update both when upgrading from the previous public version.

The final local NuXTLink review passed 86 host/Tk tests; all 86 also passed
again from the publication copy on an isolated virtual display. Its TESTING.md records
prior DOS integration and assembly checks. Checkers' TESTING.TXT records its
host and DOSBox-X checks, including 1.2.1 input and redraw regression checks.
These emulator suites were not repeated for publication. This packaging pass
checks source/binary parity and the repository's complete checksum inventory.
Checkers screenshots and music attribution accompany the source and executable.

## TEXTMODE, NuXT Menu 1.5 and Orbit87 2.1

Added TEXTMODE 1.0, source, reproducible binutils build and an IRCjr wrapper.
DOSBox-X checks verified 80x50 with an 8x8 font, 80x25 with an 8x16 font,
invalid/missing arguments and whitespace. The owner confirmed it works on
the NuXT. FTP text conversion caused extra carriage returns in the batch;
Binary transfer is documented. The utility is 287 bytes and is not resident.

Updated NuXT Menu to 1.5 and both Orbit87 builds to 2.1 using the tested local
sources and binaries. The menu development checks passed 179 checks, and
Orbit's page tests passed 89 checks; their project notes retain details and
limits. These historical emulator suites were not repeated for publication.
The owner reported visibly faster Orbit animation on the NuXT.

Other published executables match the current local releases. Existing
screenshots and newer repository documentation were preserved. Multiplayer
Tetris and unrelated local projects remain unpublished. Publication checks
cover file parity, TEXTMODE rebuild, manifest hashes and package integrity.

## NuXTLink 3.2.1 update and description cleanup

3.2.1 is now the current NuXTLink release. The DOS server, Linux client and
source include Sync DOS Date/Time, with NUX321.COM as the recommended DOS name.
NUXFER.COM and NUXFER32.COM remain identical aliases for existing launchers.

Development checks passed 61 host/Tk tests and focused MS-DOS 6.22/QEMU clock
checks, including invalid dates, leap-day rollover, lost replies and transfers
with preserved metadata after syncing. Physical NuXT clock sync and RTC
persistence have not yet been confirmed. See NUXTLINK/TESTING.md for details.

For publication, source and binaries were compared with the tested local
release, and checksums were refreshed. No emulator reruns were needed.
The collection table and all eleven project introductions were edited for
clarity. These documentation edits do not change the other programs.

## NuXT Menu 1.4 update

Updated the menu source, executables, preview and documentation. This includes
Favourites as a submenu, joystick navigation, expanded settings, clock and
screensavers, remembered menu positions, and F10-only exit. Esc goes back or
cancels; it cannot exit from Home.

The assembly launcher replaces the old C supervisor and unloads the UI before
launching programs. A same-boot MS-DOS 6.22/QEMU comparison measured 2,816 bytes
of overhead for an EXE and 5,936 for a batch file, versus 31,184 with the older
launcher. These figures depend on DOS, environment size and drivers.

Development validation passed 38 data checks and 75 UI/input checks in DOSBox-X.
Focused MS-DOS 6.22/QEMU checks covered EXE/COM/BAT launch and return, the DOS
prompt, saved settings, favourites, screensaver wake behaviour, missing joystick,
and F10/Esc handling. See NUXMENU/TESTING.TXT for details. The owner subsequently
reported this release working well on the physical NuXT so far; this is not
exhaustive validation of every setting or device combination.

Publication files match the tested local release. Snapshot checksums were
refreshed. No additional emulator runs were needed for publication.

## NuXTris 1.2 update

Updated the single-player executable, source and documentation. This fixes
Backspace after a full eight-character score name and limits menu/name-entry
redraws to changed text and cursor pixels. Gameplay, sound and saves are unchanged.
Multiplayer is not part of this update.

Development checks passed in DOSBox-X: 129 checks, including 24 new name-entry
and UI regression checks. Incremental screens matched the reference renderer's
four-plane framebuffer hashes. These checks do not establish physical NuXT
performance. See NUXTRIS/TESTS.TXT for details.

Publication files were checked against the tested local release and snapshot
checksums were refreshed. No additional emulator runs were needed.

## Sunset 3.2 update

Updated the executable, source, renderer tests and documentation. The 320x200
renderer batches dirty-mask reads and row copies in assembly, with no extra
framebuffer memory or changes to the artwork and soundtrack.

Development checks passed: 32 renderer cases, 164 music checks and a 120-frame
release run with AdLib. Fixed-scene DOSBox-X rendering time fell from 277 to
264 ticks in 256 colours and 478 to 466 in 16 colours. Sampled frames matched
3.1 byte for byte. These are emulator measurements, not physical NuXT results.
See SUNSET/TESTING.md for the setup and limitations.

For publication, an isolated rebuild matched the release executable and all
snapshot checksums were verified. No additional emulator runs were needed.

## NuXTroids 1.3 update

Updated the executable, renderer, source and test notes from the current local
release. All other published app sources and binaries were already current.
Multiplayer NuXTris is excluded from this update.

The release records 67 checks at 640x480 and 29 renderer checks each at 320x200
and 800x600. A controlled 16-rock workload improved from 93 to 84 BIOS ticks
with AdLib in the same emulator configuration. These are not physical NuXT
frame rates. The larger mask cache trades RAM for fewer VGA writes.

For publication, a fresh isolated build matched the release executable and
all repository checksums were verified. No extra emulator tests were run.

## October 11 publication

NuXTroids 1.2 and NuXTris 1.1 were added with source, binaries, build scripts,
licenses and test notes. The owner called both single-player games complete.
NuXT Link 3.2 replaces 3.1; Invaders 2.0's prepared update is also included.
Other published program sources/binaries were compared against current outputs
and were already current. Repository-specific documentation is retained.

Fresh isolated builds of the four changed/added programs match their released
binaries. The snapshot verifier checks every published file. Prior development
validation is recorded in each project; it was not repeated in emulators just
for uploading. Link's 54 host/Tk tests and compiled DOS QEMU integration passed
during today's implementation. Its new connection logic still needs physical
NuXT confirmation. No 86Box testing was used.

NuXTris's upstream MIT notice and compiler runtime licenses are retained.
Commercial game executables, OS disks, user saves and transfer records are
excluded. DUNE2FIX remains a patcher requiring the owner's original game.

## Invaders 2.0 local update, 2026-10-11

- Isolated GNU binutils rebuild matches the 10,549-byte COM exactly.
- 86 QEMU checks passed in each resolution, plus real emulated keyboard
  short-tap tests, silent mode and operation without AdLib hardware.
- DOSBox's emulated gameport passed a scripted virtual-controller run through
  calibration, movement, firing, pause and resume. Physical gameport timing
  and NuXT performance remain untested for this update.
- Source, executable, README and test notes are synchronized with outputs.
  No 86Box tests were run. Full details are in INVADERS/TESTS.TXT.

## Dash 1.3 and Dune II 1.1 update

The owner confirmed both final builds work well on the NuXT before this upload.

- NUXTDASH rebuilt from the prepared source in an isolated directory using
  GNU binutils. DASH.COM matched the tested 10,428-byte binary exactly.
- DUNE2FIX: all 10 synthetic unit tests passed, including both retained XMS
  edits, manual-check branch guards and refusing file-size changes.
- The updated public patcher generated SETUPNX.EXE and DUNE2FX.EXE from the
  owner's original files. Both matched the already-tested local builds exactly.
  Commercial inputs and outputs stayed outside the repository.
- The earlier QEMU and targeted instruction tests are recorded in the project
  documentation. No emulator testing was repeated for this upload, and no
  86Box tests were run.
- Snapshot checksums were refreshed for the new project and patcher update.

The checks below describe the earlier collection packaging pass.

## Rebuilds

Open Watcom C/C++ 2.0 beta, Linux x86-64 tools built Oct 5 2026 06:25:14;
GNU binutils 2.47 for INVADERS. All builds ran from an isolated copy of the
staged source tree, with no files read from older project source directories.
The compiler installation was supplied separately through WATCOM.

All nine main DOS binaries rebuilt byte-for-byte identical to the current
working releases:

- NUXMENU: MENU.EXE and NUI.EXE.
- ORBIT87: ORBIT87.EXE and ORBITV20.EXE.
- STARFIELD: STAR.EXE.
- MATRIX: MATRIX.EXE.
- INVADERS: INVADERS.COM.
- NUXTLINK: dos/NUXFER.COM; NUXFER31.COM is an identical alternate name.
- SUNSET: SUNSET.EXE.

MATRIX's distributed BENCH/CHECK/FPUTEST executables also reproduced exactly.
ORBIT and STARFIELD test sources compiled successfully. STARFIELD's historical
CHECK.EXE did not reproduce from the supplied test source (32-byte size
difference), so that optional diagnostic binary is omitted. Build it with
STARFIELD/TEST.SH when needed. The main STAR.EXE matched exactly.

## Tests run for this snapshot

- DUNE2FIX: 7 unit tests passed, including unknown-version rejection, opcode
  guards, output-checksum checks and refusing existing output directories.
- DUNE2FIX also generated byte-identical copies of the two executables the
  owner already tested. Commercial inputs and outputs stayed outside this
  repository and are not in the archive.
- NUXTLINK: 24 protocol/transfer/settings tests passed. Seven Tk UI tests were
  skipped because this run had no display. Historical GUI test results are
  retained in NUXTLINK/TESTING.md; no live serial device was used here.
- NUXMENU: 29 configuration/CRC/recovery checks passed in DOSBox.
- SUNSET: 164 host music checks passed with a peak of 24 OPL writes per IRQ;
  all seven DOS test executables compiled. The full graphics test matrix was
  not rerun for packaging.

Python used for this pass: 3.14.7. No 86Box runs were made.

## Hardware evidence

The owner has reported these projects working on the NuXT during development,
including the Dune II XMS fix, NuXTLink serial transfers, Matrix, starfield and
V20 demos. These reports apply to that machine and the tested modes, not every
possible DOS/video/controller combination. Emulator checks cannot establish
real V20 bus timing or serial flow-control behaviour on other hardware.

## Content checks

The staged collection excludes transfer caches, serial-adapter identifiers,
user menu/save files, private media, disk/ROM images, toolchain distributions
and commercial game executables. DUNE2FIX ships a patcher and synthetic tests
only. Project licenses and Open Watcom notices are included. Main executable
hashes identify the current files; SHA256SUMS covers the full staged snapshot.
