# NuXT Link 3.5.2 - 17 October 2026

- Restore the direct packet-driver UDP LAN protocol (wire v6) with 1440-byte
  file blocks. The Linux LAN client and DOS `NUXNET.EXE` must be updated together.
- Keep the wait-for-any-client behavior, DOS and connected-client IP display,
  resume, frame CRC16, whole-file CRC16, and DOS staging readback.
- Saved LAN profiles from the interim TCP releases load with the fixed
  1440-byte block size. Serial `NUXFER.COM` is unchanged.

---

# NuXT Link 3.4.3 - 17 October 2026

- Let `NUXNET` wait for a client instead of requiring the Linux IP on the DOS
  command line. It displays the configured DOS IP from `MTCPCFG` and shows the
  connected client's IP. Enter the DOS IP in the Linux client.
- Use 1464-byte LAN blocks to fill a standard Ethernet MTU without IP
  fragmentation. Batch raw DOS upload writes in the existing 4 KiB buffer and
  build packet headers in place to avoid two full-frame copies.
- LAN transfers remain stop-and-wait. A larger speed increase needs a
  pipelined protocol with bounded retries and receive buffering. Serial
  `NUXFER.COM` is unchanged.

---

# NuXT Link 3.4.2 - 17 October 2026

- Show the upload's DOS readback check and final save in the Linux progress
  area after transfer bytes reach 100%. Downloads show local verification.
- Show the configured Linux peer IP on the LAN DOS screen when it connects,
  then `Disconnected` when the Linux client closes the connection normally.
- Confirmed the existing DOS date/time button also works over LAN.

---

# NuXT Link 3.4.1 - 17 October 2026

- LAN blocks are now 1440 bytes, still within a standard 1500-byte Ethernet
  MTU. This reduces per-block acknowledgements; real NuXT speed needs testing.
- Grey out serial settings and their labels in LAN mode. Old saved LAN profiles
  are updated to the new fixed block size when loaded.

---

# NuXT Link 3.4.0 - 17 October 2026

- Add `NUXNET.EXE` as a separate DOS LAN server. It uses the packet driver and
  the static IP settings in `MTCPCFG`; its only command-line argument is the
  Linux peer's IP. The DOS server accepts packets only from that peer.
- Add Serial/LAN selection and a DOS IP field to the Linux file manager and
  saved profiles. Existing serial profiles remain valid.
- Keep the v6 file protocol, staged upload readback, resume and metadata checks.
  LAN carries one frame per IPv4/UDP datagram using 1024-byte blocks; serial
  remains in `NUXFER.COM` with its existing settings.
- LAN compression defaults off when selected; users can enable it. The local
  QEMU DOS roundtrip verified browse, mixed raw/compressed upload and an exact
  download. No physical NuXT speed claim is made.

---

# NuXT Link 3.3.3 - 16 October 2026

- Fix reconnecting after the last DOS folder was renamed, removed or became
  unavailable. Open the drive root and report the fallback instead of closing
  an otherwise working connection. Serial failures still report normally.
  Existing queued paths stay unchanged.
- Final focused review: 86 host/Tk tests passed. DOS remains 3.3.1, wire v6.

---

# NuXT Link 3.3.2 - 16 October 2026

- Fix Disconnect and window close after the DOS server exits. Stop trying to
  renegotiate DOS settings during cleanup. Leave a running server at its current
  speed; Connect already searches for it. A pause request gets one short attempt.
- Allow Disconnect during a busy operation. Interrupt serial waits, including
  low DSR/DCD, blocked writes and settings recovery, before closing the port in
  the worker. Queue entries and resumable files are retained.
- Always release the serial descriptor even when restoring its settings raises
  `termios.error`. Detach the connection once so cleanup errors cannot cause a
  repeated disconnect loop on window close. FTDI restoration is still attempted.
- Linux-only update. DOS 3.3.1 and wire protocol v6 are unchanged.

---

# NuXT Link 3.3.1 - 16 October 2026

- Show completed-file Time and Compression columns in the Linux queue, with
  numeric sorting and full details on double-click. Save results in reports.
  Time includes active attempts, retries and verification, excluding pauses.
  Resumed-upload ratios describe the final newly transferred portion only.
- Fix missing DOS upload readback: close/reopen and hash the staged file in
  bounded, cancellable steps before replacing the original destination. Both
  ends check the readback length/CRC. Earlier releases verified received data
  but did not actually reread the staged file, despite the documentation.
- Require the readback before commit, including after resume. Use protocol v6
  so an older client cannot silently omit the new check. Update both ends.
- Check the initial staging-file seek result. Clear old failure messages when
  retrying a job, retain completion metrics in the combined queue report, and
  avoid redrawing queue rows whose cells have not changed.

---

# NuXT Link 3.3 - 16 October 2026

- Add heatshrink compression for uploads, with a saved on/off toggle and raw
  fallback when a block saves less than 25% or 32 bytes. Show payload savings
  in completed queue entries and reports; label speed as file KiB/s.
- Decode independent blocks in bounded assembly using the existing DOS disk
  buffer. Check decoded length/CRC before writing. Retain file readback,
  metadata verification, duplicate replay and resume across setting changes.
- Replace the C CRC byte loop with table-driven assembly, keeping the same
  polynomial and checksum values for frames, files and resume records.
- Fill the empty transmit FIFO in bursts of up to 16 bytes. Check configured
  peer signals per byte, retain timeout/ESC handling and final UART drain,
  and use single-byte writes without a FIFO.
- Use wire protocol v5. Update DOS and Linux together. Keep the NUXTLINK32
  folder and NUXFER.COM filename so existing launchers still work.
- Include the upstream heatshrink encoder, its ISC license, a Linux helper
  and build script. No proprietary FastLynx code was copied.

---

# NuXT Link 3.2.2 - 16 October 2026

- Correct the Linux speed display to use worker timestamps, so delayed UI
  updates cannot create a false burst when processed together. DOS is unchanged
  by this display fix; restart the Linux client after the current transfer.
- Add selectable 2048- and 4096-byte transfer blocks to DOS, Linux, saved
  profiles and command-line options. Existing profiles keep their chosen size.
- Negotiate the selected size with the existing confirmed-settings exchange.
  Uploads and downloads use it; resume can switch sizes at any byte offset.
- Keep all CRC, duplicate-request, whole-file and metadata verification.
- Scale transfer timeouts to the selected block and baud rate. Count time
  transmitting a reply as activity, including 4 KiB replies at 1200 baud.
- Use wire protocol v4. Update DOS and Linux together; no older-version
  compatibility or fallback. Smaller block options remain available.
- Use NUXFER.COM as the single DOS binary; remove versioned aliases and
  duplicate release builds. Retain the NUXTLINK32 folder for Linux launchers.
  DOS frame buffers grow by 10.5 KiB; COM grows by 116 bytes.

Based on the local throughput investigation, especially the cost of repeated
stop-and-wait exchanges. No third-party protocol or proprietary code added.
Hardware speed measurements remain for the NuXT test.

---

# NuXT Link 3.2.1 - 13 October 2026

- Add Sync DOS Date/Time beside Diagnostics. Sends Linux local date/time while
  idle, reads the DOS clock back and reports the result in the status bar/log.
- Validate calendar/time fields before changing the DOS clock. Preserve retry
  safety, reject sync during file operations, and explain when an older DOS
  server needs updating. File timestamps and transfer verification are unchanged.
- Include NUX321.COM for uploading beside a running older server. Existing
  folder paths and binary aliases remain available.

Original implementation using documented DOS APIs. References checked:
[FreeDOS clock source](https://github.com/FDOS/kernel/blob/master/kernel/systime.c),
[RBIL set date](https://fd.lod.bz/rbil/interrup/dos_kernel/212b.html), and
[RBIL set time](https://fd.lod.bz/rbil/interrup/dos_kernel/212d.html).
No external source was copied and no runtime dependency was added.

# NuXT Link 3.2 - 11 October 2026

- Start at 9600, find the server, then negotiate the chosen transfer settings.
  Normal disconnect returns the server to 9600. Manual settings and servers
  left at other speeds remain discoverable.
- Recover a lost settings-proposal reply using its confirmation token. Retry
  confirmation safely, check committed settings and retain timed rollback.
- Move DOS status printing before the confirmation reply so a three-wire
  client cannot race console output. Invalidate cached replies when a failed
  settings transmission abandons its pending state.
- Append upload/download previews from multiple folders. Paths and policies
  belong to each batch. Duplicate targets fail without replacing the queue.
  Resume keeps completed work; backup manifests remain separate.
- Reapply earlier directory metadata when later batches write into those
  directories. Check stale source metadata before replacing downloaded files
  or uploading changed files with old timestamps.
- Failed serial-port open attempts no longer restore or release another
  instance's port settings. Validate HELLO drive names before accepting them.
- Limit routine progress notifications to roughly 12 per second while keeping
  job transitions and final updates. No checksums or verification were removed.

FileZilla's per-item local/remote paths were a design reference for the queue.
This implementation is original Python, with no copied FileZilla code or new
runtime dependency. References: [queue source](https://fossies.org/linux/FileZilla/src/interface/queue.cpp),
[serial open behaviour](https://pyserial.readthedocs.io/en/latest/pyserial_api.html),
and the [FreeDOS networking archive](https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/net/).
The client continues to use Python's standard-library termios transport.

Older release notes follow.

---

# NuXT Link 3.1 — Linux UI refresh, 8 October 2026

- Each file pane retains its own sort column and direction across folder changes,
  refreshes and filters. Folders remain first in either direction; numeric sizes
  sort numerically. Column arrows show the active sort.
- More compact layout with collapsible connection options and on-demand local/DOS
  name filters. Selection counts and file sizes appear below each browser.
- Upload, download, rename and delete controls follow the current selection.
  Hidden/system entries stay visible; the internal resume cache cannot be selected
  for transfer/deletion through these controls.
- Source, destination and size columns replace the ambiguous single queue path.
  Full paths, metadata and job reasons are available by double-click or Enter.
  Status colours supplement labels; queue filters follow live status changes.
- Separate file and overall progress, activity during scans, and a completion
  summary that survives the automatic DOS directory refresh.
- Pending connection changes are identified until applied. Detailed serial
  diagnostics remain available. Tooltips explain flow, peer checks and FTDI.
- File-list shortcuts: F5 refresh, Alt+Up parent, Ctrl+L path, Ctrl+A visible entries.

Transfer/protocol modules, CRC and file verification, DOS metadata handling,
profile format, FTDI implementation and all DOS binaries are unchanged.
Replace the entire Linux folder, including the new `interface.py` layout module.

---

# NuXT Link 3.1 — original DOS receive-overrun fix

Protocol remains v3; the Linux client is unchanged. The existing file formats,
CRC checks, verified resume and metadata checks remain unchanged too.

The owner's V20 upload test showed rising receive-overrun counts at 115200 for
128/256/512-byte blocks, while framing errors remained at two. This establishes
lost incoming bytes; it does not establish a cable fault. The old receive loop
called settings/timer/status helpers for every byte, and the status helper
looped over four error bits even when no error was present. This is a likely
service-rate bottleneck on a V20. A smaller packet does not reduce the incoming
byte rate within that packet.

Changes:

- A 79-byte 8086 assembly routine receives headers and bodies with a short
  data-ready loop. It checks every byte for UART errors and has a bounded
  inter-byte timeout. It preserves ES/BP and leaves interrupts enabled.
- The C wrapper rejects partial/error frames, accounts for UART faults, and
  clears damaged receive FIFO contents. Frame CRC still runs before dispatch.
- The common no-error status path no longer loops through all error counters.
- RTS goes low before frame CRC checking, between-packet keyboard handling,
  console output and disk I/O.
  Baud-switch console output finishes before receive readiness is raised.
- Keyboard handling also runs between packets; long low-baud packets may delay
  Escape until the packet completes (about 4.3 seconds for 512 bytes at 1200).
- The DOS banner identifies 3.1; the Linux client and protocol stay at version 3.

The UART's FIFO is 16 bytes and its RTS output is software-controlled; it does
not automatically throttle a transmitter when the receive FIFO fills. At
115200 8N1, 16 bytes arrive in about 1.39 ms. These facts were checked against
[TI's TL16C552 datasheet](https://www.ti.com/lit/ds/symlink/tl16c552.pdf).
[FTDI's hardware description](https://ftdichip.com/wp-content/uploads/2020/08/DS_FT232R.pdf)
confirms that the adapter handles enabled handshaking in hardware.

The code and emulator tests do not prove the real NuXT now sustains 115200.
Keep 57600 as the fallback and compare fresh overrun counts during an upload.
The 3.0 binaries and user's renamed NUXFER3.COM are retained separately.

---

# NuXT Link 3.0

The DOS server and Linux client must both be upgraded. Version 2 backups,
metadata sidecars, and owned resume records remain compatible. The wire protocol
is version 3 and rejects a mismatched server before file operations.

## Review and changes

- Cancellation previously left a job displayed as Running. Cancelled and failed
  jobs now receive explicit states. Cancellation finishes an in-flight CRC-checked
  reply before stopping, so a completed delete/rename is not mistaken for an
  operation that never happened. Queue clearing removes UI jobs only.
- Downloads previously called DOS for every 128 bytes. A 4 KiB read-ahead buffer
  reduces those calls while the existing rolling whole-file CRC still processes
  exactly the transmitted bytes. Duplicate requests replay the cached response
  without advancing either the disk buffer or CRC again.
- Framing now accepts up to 512 bytes. Transfer blocks are selectable at 128,
  256 or 512 bytes, with the conservative 128-byte default retained. Request
  timeouts account for the largest frame at the selected baud, including 1200.
- RTS/CTS is optional. The polled DOS driver checks CTS before transmitting and
  lowers RTS while processing a received request, including disk I/O. DTR remains
  asserted while the server is running. Optional DSR/DCD checks gate transmission
  on both ends; they are not required for three-wire or five-wire cables.
- Connection changes use an old-settings proposal, a delayed switch, and a
  token-bearing confirmation at the new settings. The server restores the old
  settings if confirmation does not arrive in about 12 seconds. The client also
  checks for a committed switch after a lost confirmation reply before recovering.
- Diagnostics expose UART FIFO support, modem lines, CRC failures, individual
  UART errors, transmit/handshake timeouts, active settings and DOS disk space.
- New delete and rename requests are CRC-framed and duplicate-suppressed. Delete
  compares preview metadata, requires an explicit protected-file flag, and removes
  only one file or empty directory per command. Recursion lives in the Linux queue,
  allowing cancellation between entries. Root, the executing server and containing
  folders, and the resume cache are protected. Rename never overwrites a target.
- FTDI latency can be applied to one adapter with OS authorization and readback.
  Session restoration respects intervening changes made by other software. The
  helper does not open the serial port, modify EEPROM, or install global rules.
- Linux UI now shows local/DOS panes, an always-visible queue, status filters,
  cancel/clear/retry controls, recursive deletion previews, mkdir, rename/move,
  sorting, context menus, disk space, connection profiles, diagnostics and an
  optional activity log. Profiles contain paths/settings, not file contents.

Packet CRC16/CCITT-FALSE, whole-file CRC, local SHA-256 manifests, verified resume,
final metadata checks, staging and overwrite recovery remain in place. No check
was removed for speed. Larger blocks and latency tuning are options, not claims
of a measured improvement on the physical NuXT.
