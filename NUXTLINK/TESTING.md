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

# Linux UI refresh validation - 8 October 2026

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

# Original NuXT Link 3.1 validation - 6 October 2026

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

# NuXT Link 3.0 validation - 6 October 2026

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

The attached FTDI adapter was set to a runtime latency of **2 ms**, with
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
