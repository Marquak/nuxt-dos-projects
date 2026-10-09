# NuXT Link 3.1  -  Linux UI refresh, 8 October 2026

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

# NuXT Link 3.1  -  original DOS receive-overrun fix

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
