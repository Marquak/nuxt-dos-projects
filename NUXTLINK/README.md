# NuXT Link 3.1

A way to get files to and from the NuXT without swapping cards or floppies.
It has a Linux file-manager GUI, folder transfers and file-level drive backups,
with CRC verification, DOS timestamps and attributes preserved.

The DOS server targets 8088/8086 and NEC V20. The Linux client needs Python
3.10+ and Tkinter. No pip packages are required.

## Upgrade and start

**Linux UI refresh, 8 October 2026:** replace the complete `linux/` folder and
relaunch `sh linux/launch.sh`. Keep `interface.py` alongside `nuxtlink.py`.
Your existing DOS 3.1 server and saved connection profiles remain compatible;
there is no DOS update for this UI refresh.

Each file pane remembers its own sort column and direction when navigating,
refreshing or filtering, with folders first and an arrow on the active heading.
Click **Filter** to search names in the current folder; **Hide** clears the
filter. Enter a path and press Enter to navigate. In a file list, **F5** refreshes,
**Alt+Up** opens the parent, **Ctrl+L** focuses its path and **Ctrl+A** selects
visible entries. Hidden/system files remain included.

**Connection options** reveals peer checks, packet size, FTDI latency and the
DOS-port reminder. Edited link settings are marked pending until **Apply
changes** succeeds. **Diagnostics** still shows the individual error counters,
FIFO and handshake lines. Hover over options for explanations.

The queue shows source, destination and bytes. Local paths in its table are
relative to the preview's local root; double-click a job for complete paths,
metadata and any error/skip reason. Separate file and queue progress bars show
byte progress; completion/verification is indicated by job status. Folder-only
and deletion queues use completed job counts for overall progress. Cancel,
clear and resume retain their existing behaviour.

For the original **DOS 3.0 to 3.1** upgrade, replace the DOS program; Linux
protocol-v3 clients are compatible. The DOS patch addresses receive overruns during uploads by using a
short 8086 assembly receive loop and holding RTS low during CRC checks and
console output. The owner confirmed successful 115200 uploads on the NuXT after this fix;
other machines and cables still need their own checks.

Use 57600 for copying `NUXFER31.COM` to the NuXT alongside your existing server.
Disconnect the Linux client, exit the old server with Esc, then start:

```dos
NUXFER31 COM1 115200 /FLOW:RTSCTS /PEER:BOTH /BLOCK:512
```

Select those same settings in Linux and reconnect. Counters start at zero;
compare **UART overruns** before and after an upload. Lower to 57600 if the
counter increases. Old totals do not clear when baud or block size changes.
`dos/NUXFER.COM` and `NUXFER31.COM` in this package are identical; the alternate name
lets you stage this update without overwriting a running program.

When updating from v1/v2, update **both** DOS and Linux to protocol v3.
Exit the old server before replacing its executable.
This source package does not include a floppy image. Copy the server using
your existing transfer method or a DOS data floppy.

On DOS:

```dos
COPY A:\NUXFER31.COM C:\NUXFER31.COM
C:\NUXFER31 COM1 38400
```

Use the actual directory where you keep the server. It always shares the **whole
drive containing NUXFER.COM**. For example, launching `C:\TOOLS\NUXFER` while the
prompt is `A:\` shares `C:\`. Launching the copy on the floppy shares `A:\`.
The Linux browser starts at that drive's root; there is no other-drive selector.

On Linux, from the extracted package:

```sh
sh linux/launch.sh
```

Choose the serial device and matching speed, then **Connect**. A USB-to-RS-232
adapter and null-modem cable are required for the real machine. For 86Box, enter
the PTY displayed by its COM1 virtual console (for example `/dev/pts/3`). Your
86Box settings are independent of this package.

## Connection settings and full-handshake cables

The ordinary DOS bootstrap remains `NUXFER COM1 38400`. Choose the desired baud,
flow and optional peer checks in the GUI, then Connect. The client first tries
the selected baud, then 38400 if different, and negotiates the selected settings
on both ends. It does not scan every speed. The DOS port must already be the one
physically connected to the cable.

For the full-handshake cable, use **flow rtscts**, initially **peer none**. The
optional peer choices `dsr`, `dcd`, `both` require those readiness inputs before
transmitting. Enable them only with the corresponding wires connected. For a
basic three-wire cable use **flow none / peer none**. CTS/DSR/DCD status is visible
in Diagnostics; an unconnected status input is not a reason to fail three-wire
operation.

While connected and idle, **Apply changes** updates DOS and Linux baud, flow,
peer checks and block size together. A switch is acknowledged at the old baud,
confirmed at the new baud, and rolls back on the server after about 12 seconds
if not confirmed. A lost final acknowledgement is checked before recovery.
A cable/port failure can prevent recovery confirmation; the error then asks you
to check the DOS console rather than claiming the connection is healthy.

COM-port selection is local to the DOS server (`COM1..COM4` or idle key C).
The GUI's DOS-port profile field is a reminder; once connected it displays the
actual port. Selecting another COM socket requires moving the cable. Manual
DOS baud/flow changes require matching/reconnecting the client. GUI-negotiated
changes keep both ends coordinated.

DOS examples:

```dos
NUXFER COM1 115200 /FLOW:RTSCTS /PEER:NONE /BLOCK:128
NUXFER COM1 38400 /FLOW:NONE /PEER:NONE
```

`/PEER:` accepts `NONE`, `DSR`, `DCD`, `BOTH`. `/BLOCK:` accepts `128`, `256`,
`512`. Defaults are COM1, 38400, no flow control, no required peer lines, 128.
The 128-byte default is suitable for the original three-wire setup; larger
blocks are optional. Packet CRC and whole-file verification always apply.

Linux examples:

```sh
sh linux/launch.sh --device /dev/ttyUSB0 --baud 115200 --flow rtscts --latency 2 --connect
sh linux/launch.sh --profile NuXT --connect
sh linux/launch.sh --baud 38400 --flow none --peer none --block 128
```

Additional options: `--local PATH`, `--remote '\DOS'`, `--com COM1`. Use
`--help` for all options. Device paths under `/dev/serial/by-id/` are preferred
when available. **Save profile** stores device, baud, flow, peer checks, block
size, latency, DOS-port reminder and local/remote folders in
`~/.config/nuxtlink/profiles.json` (or the XDG_CONFIG_HOME equivalent).

## FTDI latency

The default GUI preference is **2 ms**; `0` leaves the adapter unchanged.
On connection, a writable FTDI setting is applied automatically. Otherwise,
**Apply FTDI latency** requests system administrator authentication through
PolicyKit (`pkexec`). A denied request is reported without changing the setting.
PTYs and non-FTDI devices continue to work without this feature.

Only the selected adapter's Linux `latency_timer` is changed and read back.
The previous value is restored on normal disconnect/close, using authentication
again if required. If another program changed it in the meantime, that new value
is respected. A crash may leave the runtime value in place until it is manually
changed or the adapter is unplugged; no EEPROM or permanent udev rule is changed.
Keep the adapter connected until normal disconnect if restoration is wanted.

## Browser and queue controls

The left pane is the local folder and the right pane is the shared DOS drive.
Double-click folders to navigate; double-click files to prepare a transfer.
Transfers first appear in the queue for review. Download selection uses the
current local folder. Upload selection accepts files and folders from the local
pane. Columns can be sorted; right-click offers the relevant actions.

On DOS: **New folder** creates an 8.3 directory; **Rename / move** accepts an
8.3 name or a root-relative destination path and never overwrites an existing
entry. **Delete** builds a recursive preview, then **Start / resume** asks for
confirmation with file/folder counts and protected-file counts. Confirming that
preview authorizes clearing R/H/S bits as needed for those selected entries.
Files changed since the preview are rejected; nonempty directories are not
silently removed. Completed deletions are permanent. Cancellation stops between
entries, and a saved report records what actually completed.

The shared root, running server and its containing directories, and `NX$CACHE`
cannot be deleted or renamed through these commands. Existing unrelated cache
contents are never treated as discardable partials. A deletion preview with
unreadable or excluded entries is rejected before execution.

The queue filter shows All, Pending, Failed or Completed. **Clear queue** never
deletes data or staging files. **DOS partial files** is the separate explicit
cleanup action for owned incomplete uploads. **Activity log** is collapsible.
**Test / diagnostics** reads the live DOS UART/error counts and adapter latency;
it does not send a destructive test file.

## Files and folders

Double-click a folder, use **Parent**, or enter a path. Hidden, system and
read-only entries are always shown. Select multiple entries to download them.
**Upload selection** includes subfolders and empty directories.

Choose how to handle existing files: **Skip**, **Overwrite**, or **Rename**.
Every operation first creates a **Transfer queue / preview**. Review its paths,
name changes and errors, then press **Start / resume**. Two Linux names
that map to the same DOS 8.3 name receive distinct `~1`, `~2`, etc. names in the
preview. Name mappings never silently overwrite another source in the queue.
Directories merge for Skip/Overwrite; Rename gives an ordinary uploaded folder
a new name. Backup restore merges directories so its hierarchy stays intact.

The progress bar shows the current file; status shows its position in the queue,
bytes, measured transfer rate, ETA and link retries. The queue summary includes
overall byte progress, ETA, completed jobs and failures. **Cancel queue** retains
incomplete files and stops before the next job. **Clear queue** removes only the
listed jobs while idle, leaving completed files and resumable partials intact.
**Retry failed / pending** resumes the unfinished jobs; successful ones stay done. Reconnect and resume the current queue, or repeat the same
operation after restarting the client. Completed matching files are verified and
skipped when **Verify and skip unchanged contents** is enabled with Overwrite.
No automatic deletion or mirroring of destination-only files is performed.

## Backups and restore

**Backups → Back up entire drive** starts at the shared drive root and includes hidden and
system files, such as `IO.SYS`, `MSDOS.SYS`, and `CONFIG.SYS`, plus empty folders.
Choose a dedicated Linux folder. Reusing it performs an incremental backup by
default: files with matching size/content CRC are checked locally and skipped;
changed files transfer again. Previous backup SHA-256 records also detect local
corruption. DOS reads/checksums still take time, but unchanged file contents do
not cross the serial cable. Uncheck the verification option to copy every file.

The backup manifest is `.nuxtlink/backup.json`. It records every planned path,
raw DOS metadata, successful file CRC-16 and local SHA-256, completion status,
errors, exclusions and files missing since the previous backup. An absent source
file is reported; its previous local copy is retained. Incomplete/unreadable
entries are reported explicitly rather than labelled as a complete backup.

**Verify local backup** checks files against the manifest and produces
`.nuxtlink/verification.txt` and `verification.json`. **Backups → Restore backup here**
validates local file SHA-256 values before building a restore preview for the
current DOS folder. Choose Overwrite to replace existing files, or Skip/Rename as
needed. Damaged or incomplete backup entries are listed as issues and excluded
from the restore. Move a backup as a complete directory, including hidden files.

This is a **file-level** backup. It does not copy partitions, boot sectors,
volume labels, deleted files or free space. Restoring DOS system files alone is
not a promise of a bootable disk; prepare/system-transfer a boot disk with DOS
as appropriate before restoring its files.

## Exact DOS metadata

All successful transfers preserve raw DOS **last-modified date/time** and the
**read-only, hidden, system and archive** bits, including a cleared archive bit.
DOS dates use local wall-clock time with two-second resolution. Raw unset or
invalid DOS dates are retained too. DOS has no Unix ownership/permissions or
standard per-file creation/access timestamps comparable to modern filesystems.

Linux files receive the corresponding modification time and read-only state.
The complete DOS fields live in hidden `.nuxtlink/files.json` sidecars and, when
supported, `user.nuxtlink.dos` extended attributes. Backups additionally contain
them in the manifest. **Keep the `.nuxtlink` folders** when moving/archiving files
if you want a lossless restore on filesystems that omit extended attributes.
New Linux files get archive=on and a read-only bit derived from their mode.
Editing a downloaded file updates the exported DOS modification time while
retaining its saved DOS attribute bits.

Directory attributes and timestamps are restored after child transfers.
DOS 3.31/6.22 lacks a handle API for setting directory dates: the server validates
a local FAT12/FAT16 volume, locates the directory, and changes only its four
last-modified timestamp bytes. It flushes DOS buffers and verifies the resulting
metadata. It never writes allocation tables or boot sectors. Unsupported volume
formats or failed metadata updates are errors, not successful transfers.
The FAT root itself has no directory timestamp entry to preserve.

## Recovery and disk space

Uploads are staged inside an owned hidden `NX$CACHE` folder on the shared DOS
drive. Resume compares source metadata, whole-file CRC and the retained prefix.
Downloads retain checked partials under the destination's `.nuxtlink/partials`.
Existing destination data is left in place until the new file verifies. An
explicit overwrite stages the old DOS file as a temporary `.BAK` during commit.

The GUI's **DOS partial files** button shows incomplete uploads and lets you
reclaim their space. Only staging files with valid NuXT Link records are removed.
Interrupted replacement backups and unrecognized records are protected. If a
power failure occurs between the final renames, a `.BAK` may remain in `NX$CACHE`;
the next attempt recovers a missing original, or reports that inspection is
needed if both versions exist. No original backup is silently discarded.

The owned DOS resume cache is visible in the browser, but excluded from drive
backups and listed as an exclusion in the report. An unrelated existing
`NX$CACHE` without the ownership marker is never taken over. Cache files consume
space until resumed or discarded. Overwrites require room for the staged copy
alongside the old file. Linux partials can be removed with the client stopped if
you intend to abandon the corresponding downloads.

## Hardware and limits

The supplied NuXT 2.0 manual identifies a **16C552**, containing two FIFO UARTs.
The rear RS-232 port normally uses COM1 at 3F8h; COM2 serves the onboard serial
mouse conversion. Server COM addresses come from the BIOS data area. The UART
is polled, with a FIFO enabled if detected. Packet headers/bodies are captured
by a bounded 8086 assembly loop; interrupts stay enabled and no DOS/BIOS calls
occur inside that loop. CRC calculation runs after reception with RTS low.
Escape is checked between packets; at 1200 baud a 512-byte packet takes about
4.3 seconds to finish, and a stalled packet has a one-second inter-byte timeout. On exit the saved baud, line-control,
modem-control and interrupt-enable registers are restored; the FIFO is disabled.
**Esc** exits; idle keys **B**, **F**, **D**, **C**, **S** change baud, toggle
RTS/CTS, toggle required DSR+DCD, cycle present COM ports, or print status.

Supported speeds: **1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200**.
All use **8N1**. Flow control is selectable: **none** for a three-wire cable,
or **rtscts** for crossed RTS/CTS. Software XON/XOFF is not used. Start at 38400
with the NuXT's FIFO UART, or 9600 with an older UART, and lower the speed if
retries or UART errors increase.
Maximum speeds depend on the adapter, cable and CPU. No graphics hardware or
sound card is needed.

Target: DOS **3.31+**, local **FAT12/FAT16**, conventional memory only, 8086
instructions. DOS paths use 8.3 components, CP437 names and at most 62 bytes after
the drive prefix. Unsupported names/paths, symlinks, special Linux files and
unreadable directories are reported. Files below 2 GiB use 32-bit lengths;
ordinary tens-of-megabytes files fit that protocol limit, subject to free space.
The disk and DOS environment must remain available during transfers.

## Build and test

`dos/NUXFER.COM` and `NUXFER31.COM` are the same compiled server.
Build from source with Open Watcom:

```sh
export WATCOM=/path/to/watcom
sh dos/BUILD.SH
```

The build uses `-0 -mt -l=com`: 8086 instructions, tiny model, COM executable.
The compiler/runtime are not bundled. See the
[Open Watcom compiler guide](https://openwatcom.org/ftp/manuals/current/cguide.pdf)
for tiny-model details. The earlier assembly v1 implementation is superseded.

```sh
python3 -m unittest discover -s tests -v
```

`tests/integration.py /dev/pts/N [baud]` tests the real compiled server; run it
**only on a disposable emulator disk** because it creates `C:\NXLTEST` (or the
same folder on the shared drive) and overwrites its test files. See `TESTING.md`
for the validation performed on this release, additional v3 integration tests,
and `PROTOCOL.md` for the wire format.

## License

Project-authored source is MIT licensed; see [../LICENSE](../LICENSE).
Open Watcom runtime terms are retained in [OWLIC.TXT](OWLIC.TXT).
See [../THIRD_PARTY.md](../THIRD_PARTY.md) for provenance and source links.
