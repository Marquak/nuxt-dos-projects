# Snapshot checks, 2026-10-12

This file covers preparation of the GitHub package. The TESTING/TESTS files
inside each project also retain earlier development results. Those historical
tests were not all repeated for this packaging pass.

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
