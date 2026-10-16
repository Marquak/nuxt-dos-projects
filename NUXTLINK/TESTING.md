# NuXT Link 3.3.3 final review, 16 October 2026

86 host/Tk tests passed on a private Xvfb display. The missing-folder reconnect
test first reproduced the failure against 3.3.2, then passed with root fallback.
A second test confirms a serial failure is not hidden by that fallback and the
failed connection's descriptor is closed.

The focused source review covered Linux reconnect/shutdown, queue paths and
retry state, compression framing, and DOS staged-file verification/commit and
cached replies. No other actionable bug was identified in those reviewed paths.
This is not a guarantee that every hardware or failure case has been exercised.
DOS source/binary is unchanged; previous DOS integration results below still
apply. No physical serial device or emulator was used for this final pass.

---

# NuXT Link 3.3.2 validation, 16 October 2026

84 host/Tk tests passed on a private Xvfb display. Eleven new tests cover silent
peers, low handshake signals, blocked writes, settings negotiation/recovery
interruption, discovery cancellation, cleanup EIO errors, descriptor release,
one-time window shutdown and keeping the queue after a busy disconnect.

The silent-peer test checks one CANCEL attempt with at most 0.5 seconds for
writing and 0.5 seconds for its reply, even with a 40-second transfer timeout.
Private PTY checks verify that a shutdown signal interrupts a waiting request
without retries. Tk tests reproduce cleanup errors and confirm the window
closes once, the spinner stops and the connection is cleared. These tests use
fake peers and private PTYs, not the user's serial adapter. Linux authorization
for FTDI restoration, if required, can still need user interaction.

Reviewed the [pySerial POSIX implementation](https://github.com/pyserial/pyserial/blob/master/serial/serialposix.py)
for I/O cancellation and descriptor cleanup. No code was copied and no new
dependency was added. DOS source/binary and wire protocol are unchanged from
3.3.1; no emulator or physical NuXT run was needed for this Linux-only fix.

---

# NuXT Link 3.3.1 validation, 16 October 2026

73 host/Tk tests passed. New checks cover active time accumulated across a retry
while excluding the pause, result persistence in combined queue reports,
resume-aware ratio arithmetic, raw/empty/already-staged files, completion-only
columns, numeric sorting and entry details. A bad staging-readback CRC is
rejected by the Linux client without sending PUT_END. The 1024x720 completed
queue layout was visually checked. The DOS server builds with Open Watcom,
8086 target and warnings as errors, to 26,740 bytes.

The updated server passed the existing raw/compressed integration suites on a
private MS-DOS 6.22/QEMU disk: 2048/4096-byte transfers, sequence wrap, metadata,
resume, raw fallback, malformed compressed blocks and deliberate packet faults.
New checks confirm commit is rejected without readback, lost readback/commit
ACKs replay safely, cancelling readback leaves the old destination intact,
fully staged uploads resume without sending payload again, and empty files
complete verification. There were nine injected frame faults across the raw
and compressed suites. No physical serial device, CF card or 86Box was used.

The review covered DOS UART/FIFO/CRC/decode boundaries, command/transfer state,
resume/commit paths and Linux framing, queue execution and reporting. This is
a focused bug review, not a claim that every possible fault was tested. Assembly
routines are unchanged from 3.3; their 422-case checks were not repeated.

Correction to historical notes below: releases through 3.3 checked the upload's
received rolling CRC, not an actual reread of DOS staging. That gap is fixed in
3.3.1. Downloads already had local readback verification.

---

# NuXT Link 3.3 validation, 16 October 2026

The 26,398-byte NUXFER.COM builds with Open Watcom targeting 8086, warnings as
errors. An independent link-map build matches the shipped binary exactly.
Code/data/BSS is 45,566 bytes; BSS remains unchanged at 18,912 bytes. The program
fits the tiny-model segment with its configured 4096-byte stack and PSP. Both
the file and linked group grew by 584 bytes compared with 3.2.2. Decompression
reuses the existing disk buffer instead of adding another 4 KiB history buffer.

69 host/Tk tests passed on a private Xvfb display, including native heatshrink
encoder round trips against an independent format reader, raw fallback,
compression profile settings and UI busy state. Existing connection retries,
rollback, CRC, queue, metadata, clock and speed-display tests passed. The
1024x720 layout was visually checked with connection options expanded.

422 Unicorn assembly cases passed against separately assembled copies of the
same release routines:

- CRC over 0..4100 bytes with four seeds, checking against Python's CRC-HQX.
- Heatshrink output from the upstream encoder, random/repetitive/all-byte
  patterns, overlapping references and initial zero history, including maximum
  distance 4096. Truncation, excess decoded length, trailing bytes and nonzero
  padding are rejected. Output canaries and saved registers/IF/DF are checked.
- FIFO fills from zero to 16 bytes on two COM bases; required CTS/DSR/DCD lines
  can stop a burst before its first byte or partway through. No bytes are sent
  while the required lines are low. Single-byte non-FIFO calls are covered.

On a private MS-DOS 6.22/FAT16 QEMU disk, the delivered server passed:

- Existing raw 65,573-byte upload/download tests at 2048 and 4096 bytes,
  sequence wrap, lost write ACKs, corrupt download replies, metadata checks,
  resume at byte 2048 with a changed block size and oversized-header rejection.
- Mixed compressed/raw 73,733-byte uploads at 2048 and 4096 bytes, lost compressed
  write ACKs, sequence wrap and exact readback with read-only/hidden/system
  attributes and timestamps. Synthetic data used 31,265 / 31,187 payload bytes
  respectively; these figures are compression ratios, not performance results.
- Raw/compressed/raw resume at offsets 2048 and 6144, changing block size.
- Invalid decoded sizes, truncated streams, extra bytes, nonzero padding and
  decoded CRC mismatches rejected without writing or advancing position. A
  valid block and commit afterward produce exact expected bytes.
- Compressed-frame CRC corruption recovered through retransmission. Seven
  deliberate packet faults across the raw/compressed suites recovered.
- Compressed 128/256/512-byte blocks with raw one-byte tails, and transmission
  with and without configured RTS/CTS flow control.

These checks validate behavior, not V20 performance, actual FTDI latency or
physical CTS propagation. NuXT throughput remains to be measured by the owner.
No real serial port, CF card, system clock or 86Box instance was used.

Reproduce host tests (build the helper first if needed):

```sh
sh linux/build_codec.sh
xvfb-run -a python3 -m unittest discover -s tests -v
```

For the assembly checks, assemble/link CRC.ASM, HSDEC.ASM and UARTTX.ASM into
standalone COM files named CRC.com, HSDEC.com and UARTTX.com with respective
entry symbols crc_fast_, hs_decode_ and uart_tx_fill_. Pass their directory to
`python3 tests/fast_emulation.py DIR` (Unicorn is a test-only dependency).

Boot the server on a disposable DOS disk for `tests/integration_blocks.py` and
`tests/integration_compression.py`, each with a serial PTY path argument.
They create/overwrite NXBTEST and NXCTEST and change serial settings. Never
point them at valuable DOS files. Earlier validation below is historical.

---

# Linux speed display follow-up, 16 October 2026

64 host/Tk tests passed. The new regression queues three worker updates over
one second, then processes them together after a simulated UI delay. The
meter correctly reports 8.0 KiB/s and a three-second ETA, independent of the
UI's processing time. This is a display correction only; the 3.2.2 DOS binary
is byte-identical, and DOS integration was not repeated.

The owner reports about 7.8-8 KiB/s with 2048-byte blocks and about 8.5 KiB/s
with 4096 on the same floppy image. The one-time startup reading near 40 KiB/s
is not a sustainable UART rate. This report motivated the timing fix; the
specific physical transfer was not independently instrumented.

---

# Larger transfer blocks: 3.2.2 validation, 16 October 2026

63 host/Tk tests passed on a private Xvfb display. Added checks cover split
frames through 4096 bytes, oversized-frame rejection, profile persistence,
large-block settings with lost confirmation ACKs, low-baud timeout scaling,
and rollback from 4096 to 2048. Previous transfer/queue/clock/UI checks passed.

The 25,814-byte DOS COM builds with Open Watcom targeting 8086, warnings as
errors. An independent map build matched the delivered binary exactly.
The linked code/data/BSS group is 44,982 bytes, leaving room for the configured
4096-byte stack and PSP within the tiny-model segment. Three expanded frame
buffers add 10,752 bytes compared with 3.2.1.

Focused checks on a disposable MS-DOS 6.22/QEMU disk passed:

- 65,573-byte binary upload/download at both 2048 and 4096, crossing 64 KiB,
  with exact data, CRC and DOS metadata verification.
- Sequence wrap, dropped write ACKs and corrupt read replies. Four deliberately
  injected packet faults recovered without duplicate writes or skipped data.
- Upload and download resume at byte 2048 after switching to 4096-byte blocks.
- Oversized declared payload rejection followed by a successful request.
- Settings changes through all five block sizes, then return to 9600/128.

Reproduce with tests/integration_blocks.py on a disposable DOS server only.
QEMU was chosen for actual DOS file I/O and protocol checks. Its serial backend
is not a physical FTDI adapter or a V20 timing model. No real UART, user CF card
or 86Box was accessed. Throughput, error counts and responsiveness on the NuXT
still need hardware testing. Historical reports below apply to older releases.

---

# Date/time sync update, 2026-10-13

NuXT Link 3.2.1 built with the existing Open Watcom 8086/tiny-model build,
warnings as errors. NUXFER.COM is 25,698 bytes, 732 bytes larger than 3.2.

61 host/Tk tests passed, including seven new clock/UI checks: local wall time,
invalid date/range/readback, old-server errors, elapsed retry time, fresh clock
verification, idle-only button state and status reporting. Layout was inspected
at 1024x720 on a private Xvfb display.

The compiled server passed focused checks on a disposable MS-DOS 6.22/QEMU disk:
1980/2099 date limits, leap years, invalid lengths/fields without clock mutation,
midnight leap-day rollover, a dropped clock ACK with cached retry (no repeated
clock reset), active-operation rejection and a binary upload/download with CRC
verification and unchanged DOS timestamps/attributes after sync. A fresh 9600
connection negotiated 115200 before the clock checks. DOS tick rounding can put
readback slightly before the requested fractional second; checks allow for it.

`tests/test_clock.py` and the added Tk test run in the ordinary host suite.
`tests/integration_clock.py` exposes run(client) for an already connected
compiled server on a disposable DOS disk. It changes the guest date/time and
creates CLOCKCHK.BIN; do not point it at a real working drive.

No real UART, Linux system clock, user CF card or 86Box configuration was changed.
Hardware RTC persistence and physical NuXT sync remain for the owner to check.
Earlier test reports below describe their respective releases.

# NuXT Link 3.2 validation - 11 October 2026

All **54 host tests passed**, including 10 Tk GUI tests on private Xvfb.
Connection tests model mismatched baud rates, lost initial replies, lost
proposal/confirmation replies, rollback, cancellation, explicit rejection,
legacy/manual rates and failed serial ownership. Queue tests cover multiple
roots, mixed directions, repeated filenames, atomic conflict rejection,
cancel/resume, stale metadata and separate backup manifests.
The GUI was inspected at 1180x820 and 1024x720 with connection options both
collapsed and expanded. The test display does not require Xvfb for normal use.

The **24,966-byte DOS COM** was compiled with Open Watcom, 8086 tiny model,
warnings as errors. A clean rebuild produced identical bytes.

The compiled server passed the existing integration suites on a disposable
MS-DOS 6.22/FAT16 QEMU disk, plus new connection and multiple-folder checks:

- Startup at 9600, negotiation, lost proposal ACK recovery, disconnect reset
  and reconnect to the requested speed.
- Binary boundary sizes, sequence wrap, corrupt/dropped requests and replies,
  verified resume and preservation of existing destination files.
- Exact file/directory DOS dates and attributes, backup/restore/incremental
  verification, empty directories and declared 40 MiB partial-transfer sizes.
- Settings at 9600/38400/57600/115200, all block sizes, none/RTSCTS, lost final
  ACK recovery, unconfirmed rollback, protected rename/delete and malformed
  requests.
- Multiple upload and download roots, identical filenames in different
  folders, and exact metadata after executing the combined queues.

Limits: QEMU's PTY does not simulate FTDI timing or physical baud mismatch;
baud-aware host tests cover the latter logically. QEMU ran a 486 CPU, not a
cycle-accurate NuXT. First-connect reliability and throughput still need a
real NuXT check. No physical serial adapter, FTDI setting, user DOS drive or
86Box configuration was accessed. No claim of a measured hardware speed gain.
The older tests below are historical, not additional current test counts.

---

# Linux UI refresh validation — 8 October 2026

All **31 host tests passed**: the existing 24 protocol/metadata/profile tests
and 7 new Tk GUI tests, run on a private Xvfb display with simulated DOS listings
and temporary files. New coverage includes independent sort retention through
directory changes/refreshes, filters and selection-sensitive actions, pending
connection settings, live queue filtering/progress, cancellation, non-destructive
queue clearing, disconnect state, automatic refresh preserving completion, and
compact layout visibility.

The interface was also visually inspected at 1180x820 and 1024x720, including
expanded connection settings. Normal usage needs only Python/Tkinter. GUI tests
skip if no X display is available; run them under a real display or Xvfb:

```sh
xvfb-run -a python3 -m unittest discover -s tests -v
```

SHA-256 checks confirmed that `link.py`, `transfers.py`, `operations.py`,
`profiles.py`, `ftdi.py` and the delivered DOS COM files did not change. No real
serial adapter, FTDI setting, DOS drive or 86Box machine was accessed. The older
DOS/emulator validation below was not repeated for this UI-only update.

---

# Original NuXT Link 3.1 validation — 6 October 2026

The 24,968-byte DOS COM builds with Open Watcom `-0 -mt -l=com`, warnings as
errors. It contains only 8086-targeted C/runtime code and `.8086` assembly.
The unchanged Linux client remains protocol v3 compatible.

Passed with the compiled server on an isolated MS-DOS 6.22/FAT16 QEMU disk:

- All existing transfer tests: binary boundary sizes and sequence wrap, exact
  timestamps/RHSA attributes, deliberately corrupt/dropped packets, verified
  resume, existing destination preservation, folder backup, incremental checks,
  SHA-256 verification, restore, empty-directory dates and 32-bit file lengths.
- V3 settings combinations at 9600/38400/57600/115200, all block sizes, none/RTSCTS
  and peer requirements; lost final settings ACK recovery and timed rollback.
- 512-byte CRC/metadata round-trip, protected rename/delete, stale preview
  rejection and interrupted/resumed recursive deletion.
- Incomplete header/body timeouts followed by successful fresh-frame recovery.
- 65,540-byte uploads/downloads at each of 128/256/512-byte block sizes, with
  requests split into seven-byte host writes and gaps; exact CRC/data/metadata.

The exact 79-byte assembly routine also passed **67 Unicorn CPU-emulation cases**:
zero/small/128/256/514-byte reads, four COM base addresses, incoming gaps, injected
overrun/parity/framing/break flags, partial and empty timeouts, BIOS tick wrap,
buffer bounds, exact returned byte counts and preservation of registers/IF/DF.
This is a functional port-I/O model, not a cycle-accurate V20 or FTDI simulation.

The existing **24 standard-library host tests** still pass. No GUI code changed.
No physical serial port, active transfer, 86Box configuration or real DOS drive
was touched. Hardware upload testing is still required to confirm whether this
fix resolves the owner's observed overruns at 115200.

Assembly test reproduction (optional test dependency: Unicorn 2.1.4):

```sh
wasm -q -0 -mt -fo=UART.o dos/UART.ASM
wlink option quiet format dos com name UARTTEST.COM file UART.o option start=uart_rx_
python3 tests/uart_emulation.py UARTTEST.COM
```

The section below records the **earlier 3.0** release's broader validation;
its FreeDOS and GUI runs were not repeated for 3.1.

---

# NuXT Link 3.0 validation — 6 October 2026

## Compiled DOS server

The delivered COM was built with Open Watcom `-0 -mt -l=com -os -s -fpc`
for 8086/8088/V20, with warnings treated as errors. It is 24,796 bytes.
Tests used private QEMU disks: FreeDOS on FAT12 and MS-DOS 6.22 on FAT16.
The user's 86Box configuration, CF card and DOS files were not changed.

Both DOS environments passed:

- Upload/download sizes 0, 1, 127, 128, 129 and 65,573 bytes, sequence wrap,
  binary framing-marker/control bytes, exact timestamps and attributes
  00h, 01h, 02h, 04h, 07h and 27h.
- Five injected dropped/corrupt requests/replies recovered with five retries;
  duplicate writes, commits, reads and directory replies are not repeated.
- Drive/path boundary checks, upload/download resume at byte 1024, original
  destination retained during incomplete replacement, recursive/incremental
  backup, SHA-256 verification, restore and empty-directory metadata.
- A declared 40 MiB file size with 128 bytes staged and paused. A full 40 MiB
  transfer was not run; the private hard disk is smaller than that.
- New settings combinations at 9600, 38400, 57600 and 115200, none/RTSCTS,
  none/DSR/both peer requirements, and 128/256/512-byte blocks.
- Lost final settings acknowledgement recovery and unconfirmed server rollback.
- An 8,452-byte binary transfer at 512-byte blocks, buffered readback CRC and
  exact metadata; rename of a read-only/hidden/system file with metadata intact.
- Protected-file confirmation enforcement, stale deletion-preview rejection,
  malformed/path-escape rejection, executing-server guards and recursive
  deletion cancellation/resumption.

The MS-DOS 6.22 test also passed client/server recovery after losing the initial
settings-proposal acknowledgement.

Additional FreeDOS tests passed lost rename/delete acknowledgement recovery
without repeating mutations, and rejection of settings changes during an active
upload. The Linux GUI connected to that real compiled server, negotiated RTSCTS,
created a folder, uploaded, renamed and downloaded a file, cancelled an upload,
cleared the queue without deleting data, executed a recursive deletion preview,
saved a profile and disconnected. GUI layout was inspected on a private Xvfb
display. These tests used temporary local files and disposable emulator disks.

## Host tests and reproduction

24 Python standard-library tests cover framing/CRC, retry parser recovery,
all eight baud rates and restored termios, RTSCTS configuration, DOS path/name
validation, metadata round trips without xattrs, unset DOS dates, local path
safety, 8.3 collision mapping, backup corruption rejection, incremental checking,
metadata-directory ownership, in-flight cancellation, failed-job status,
old-version upgrade errors, profile persistence, reversible latency changes and recursive deletion ordering.

```sh
python3 -m unittest discover -s tests -v
```

For integration tests, boot NUXFER on a **disposable** emulator disk:

```sh
python3 tests/integration.py /dev/pts/N 38400
python3 tests/integration_v3.py /dev/pts/N 38400
```

The first creates/overwrites NXLTEST; the second creates/deletes V3TEST and
changes the connection settings before restoring 38400/none/none/128.
Do not run either against valuable files. V3 tests include RTSCTS; add
`--handshake` only if DSR/DCD inputs are available. The server-side tests here
used QEMU UNIX serial sockets with a test transport; actual baud switching and
kernel settings were covered separately by PTY host tests.

## Limits and real adapter change

The attached FTDI adapter FTE201LV was set to a runtime latency of **2 ms**, with
successful readback, after the owner confirmed the backup had finished.
No EEPROM or persistent system rule was changed.

QEMU's socket backend presents asserted modem inputs: these tests validate
protocol, CRC/retry/recovery, DOS filesystem behaviour and settings state, but
**do not demonstrate physical CTS pauses, cable timing or NuXT throughput**.
Those need the owner's real NuXT/cable test. No 8086 CPU emulation was rerun for
this release; instruction compatibility relies on the 8086-targeted build and
previous real NuXT testing of the v2 base. No 86Box settings were changed.

FAT32/network drives, disk-image backup and power-loss-proof filesystem updates
remain outside scope. Interrupted overwrite recovery backups are protected and
can require inspection, as described in README.md. All CRC, whole-file and
metadata checks remain enabled.
