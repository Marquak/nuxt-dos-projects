# NuXT Link protocol 6

NuXT Link 3.3.1 uses v6. Both client and server must be upgraded together.
Older wire protocols are not supported; HELLO and INFO identify version 6.

## Frames and transport

4096-byte maximum payload; selected transfer blocks default to 128 bytes. Frame: `NX`, kind u8, sequence u8, payload length
u16 LE, payload, CRC16 u16 LE. CRC16/CCITT-FALSE uses polynomial 1021h and seed
FFFFh over kind/sequence/length/payload. Binary data is never escaped or converted.
ACK=80h, ERROR=FFh with an ASCII description. Stop-and-wait: retry the exact
request/sequence. The server compares the complete last request and caches its
response, so duplicate writes, reads, final commits and directory advancement
are replayed without being repeated. Sequence numbers wrap at 256.

The client has five attempts per command. The transfer timeout is
max(3 seconds, (max(512, selected block)+16)*10/baud + 1 second), allowing
8N1 wire time for the block and acknowledgement plus processing time.
SETTINGS retains its separate confirmation/rollback state machine.
The server uses a one-second inter-byte receive timeout and closes active file
handles after about 30 seconds idle. The idle timer is refreshed after a
successful reply, so a long low-baud transmission does not expire its own transfer. Staging survives timeout, Escape, HELLO and
CANCEL; uncommitted destinations are not exposed under the final filename.

## Version 3.2 connection discovery

The server defaults to 9600/none/none/128.
The Linux client probes 9600, the chosen baud, then remaining supported rates
with bounded HELLO retries and quiet gaps for stale partial frames to expire.
It opens without flow/peer gating, then negotiates requested settings.
Normal disconnect negotiates the bootstrap settings before closing.

A lost SETTINGS reply does not prove rejection: the client attempts CONFIRM
with its known token at the proposed baud, then checks/recovers as necessary.
Explicit ERROR replies do not trigger this switch. DOS prints confirmation
status before its ACK, with RTS low, and performs no console output after it.
A failed settings ACK invalidates the cached response when its pending state
is abandoned. Transfer framing, CRCs and commit verification are unchanged.

## Types

* `path`: NUL-terminated CP437 root-relative path, beginning with backslash.
  No drive prefix, relative segments, wildcard or DOS device names. Maximum
  62 bytes excluding the NUL. The server attaches the executable's drive.
* `meta`: size u32, packed DOS date u16, packed DOS time u16, attributes u8;
  all little-endian, 9 bytes. Attribute bits R=1,H=2,S=4,D=16,A=32. Directories
  have size zero. Raw DOS date/time words are preserved without conversion.
* `scan`: processed u32, prefix length u32, current CRC16 u16, ready u8 (11 bytes).
  Each scan command hashes at most 4096 bytes from disk, bounding command time
  on an XT and making long verification scans cancellable. CRC starts at FFFFh.

## Commands

| Hex | Name | Request | ACK payload |
|---|---|---|---|
| 01 | HELLO | empty | `NUXTLINK/6 C:\ file server` (drive varies); pauses active file |
| 02 | LIST_FIRST | path (root allowed) | meta + nameZ; empty means end |
| 03 | LIST_NEXT | empty | same as LIST_FIRST |
| 04 | STAT | path | meta |
| 05 | MKDIR | path | empty; existing directory allowed |
| 06 | METADATA | meta + path | empty, after setting and verifying metadata |
| 07 | HASH_BEGIN | path | meta; begins disk-only whole-file scan |
| 08 | HASH_STEP | empty | scan; ready=1 closes the file |
| 10 | PUT_BEGIN | meta + wholeCRC u16 + overwrite u8 (0/1) + path | retained prefix length u32 |
| 13 | PUT_STATUS | empty | scan of retained prefix; ready=1 enables PUT_DATA |
| 11 | PUT_DATA | 1..4096 binary bytes | empty |
| 14 | PUT_COMPRESSED | decoded size u16 + decoded CRC16 u16 + heatshrink bytes | empty |
| 15 | PUT_VERIFY | empty | scan of closed/reopened upload staging; repeat until ready |
| 12 | PUT_END | wholeCRC u16 | matching wholeCRC; committed only after length, CRC and metadata validation |
| 20 | GET_BEGIN | resume offset u32 + path | meta; begins prefix scan |
| 24 | GET_STATUS | empty | scan; ready=1 enables GET_DATA |
| 21 | GET_DATA | empty | up to the selected block size (128/256/512/2048/4096 bytes) |
| 22 | GET_END | empty | whole-file CRC after source metadata recheck |
| 43 | CLOCK (3.2.1+) | empty to read; 8-byte clock to set | 8-byte DOS clock readback |
| 30 | PAUSE/CANCEL | empty | empty; closes file handles, retains staging |
| 32 | INSPECT_PARTIALS | empty | discardable count u16 + protected count u16 |
| 31 | DISCARD_PARTIALS | empty | discarded count u16 + protected count u16 |

Listing includes all ordinary/hidden/system/read-only files and directories,
excluding dot entries and volume labels. Bit 80h in a **listing** metadata
attribute byte identifies the server-owned root `NX$CACHE`; it is not a DOS
attribute. The client strips it before interpreting metadata, and records the
cache as an explicit backup exclusion. Finish each listing before another
command that changes directory-search state.

One file operation/scan is active at a time. HELLO and PAUSE close any active
handle. The client verifies retained upload/download prefix CRC before continuing.
A download final CRC covers the complete file, including the disk-scanned prefix.
A completed Linux file also receives a locally recomputed SHA-256 for manifests.

## Compressed uploads (v5)

PUT_COMPRESSED has a decoded length (1..selected block size), a decoded
CRC16/CCITT-FALSE (seed FFFFh) and one independent heatshrink stream. Window
bits=12, lookahead bits=4; initial history is zero. Bits are MSB first: a 1 tag
precedes an 8-bit literal, a 0 tag precedes a 12-bit (distance-1) and 4-bit
(length-1). References may overlap. Last-byte padding must be zero; trailing
bytes, truncated input and references exceeding decoded length are rejected.
The entire payload, including the four-byte prefix, must fit 4096 bytes.

Decoding uses the existing disk scratch buffer as history: a block cannot
exceed the 4096-byte window, so no separate circular buffer is required.
Leading references into initial zero history are supported. DOS verifies the
decoded CRC before writing and updating the rolling file CRC and position.
Rejected compressed blocks do not write or advance position. Frame CRC still
covers the compressed payload; commit verifies the whole uncompressed file.

Linux uses PUT_COMPRESSED only if its payload saves at least max(32,ceil(raw
length/4)) bytes. Otherwise it uses PUT_DATA. Selection is per block; there is
no connection compression state, dictionary carried across packets, or change
to duplicate replay. Retained file lengths/resume offsets count decoded bytes.
GET_DATA remains uncompressed. Host reports count acknowledged payload bytes
once, excluding retries and framing; progress/rates count original file bytes.

## Upload readback (v6)

After the last data block, PUT_VERIFY first checks the received length and CRC,
closes/reopens the staging file, resets the readback CRC and scans at most
4096 bytes per request. Subsequent PUT_VERIFY requests continue the scan until
ready=1. The client checks processed length and CRC against the local file,
then issues PUT_END. No original destination is renamed during this scan.
PAUSE/HELLO/idle timeout can cancel verification; the staged data remains.

PUT_END requires a completed readback plus matching received size, readback
CRC and expected file CRC. It cannot bypass the verification step. Replay of
a lost scan reply or final commit ACK remains idempotent. A resumed upload,
including one already fully staged, must be verified again before committing.
Version 5 did not perform this reread; its earlier readback claims were wrong.

## Staging and commit

The DOS cache is accepted only with the exact `OWNER.ID` signature. Uploads use
six-hex-digit `.RSM` records, `.PRT` data and, during overwrite commits, `.BAK`
originals. A 96-byte record includes signature, requested metadata, whole-file
CRC, overwrite flag, final path, old attributes/commit state, and record CRC.
Names are allocated without replacing unrecognized cache files. The same file
identity resumes from actual retained length, followed by a prefix checksum scan.

For overwrite, the original is renamed into the cache before committing the
verified replacement. Failures attempt to roll back; interrupted backups remain
protected. A missing final file with a recognized `.BAK` is recovered on retry.
If final and `.BAK` both exist after interruption, the server reports the state
instead of guessing which should be removed. Cleanup only deletes recognized
partial pairs without a `.BAK`; it never deletes completed destination files.

Directory metadata uses DOS attribute APIs and a bounded FAT12/FAT16 directory
entry timestamp update; file metadata uses DOS handle functions 5701h and 4301h.
All metadata is checked via DOS again before success is acknowledged.

## Protocol 3 additions

| Hex | Name | Request | ACK payload |
| --- | --- | --- | --- |
| 40 | INFO | empty | diagnostics (below) |
| 41 | SETTINGS | baud index u8, flow u8, peer mask u8, block bytes u16, token u32 | identical 9-byte proposal at old settings |
| 42 | CONFIRM | token u32 | diagnostics at new settings |
| 50 | DELETE | protected flag u8 + expected meta + path | empty |
| 51 | RENAME | expected meta + source pathZ + destination pathZ | empty |

All values are little-endian. Baud index follows the eight documented rates.
Flow is 0=none or 1=RTS/CTS; peer mask is 00,20,80,A0 for none/DSR/DCD/both.
Only block sizes 128,256,512,2048,4096 are accepted. SETTINGS is idle-only and rejects
required modem inputs that are already low. The server finishes transmitting
the old-speed ACK, waits three BIOS ticks, applies the new settings, and awaits
CONFIRM for 218 ticks. It rejects unrelated commands while pending. An
unconfirmed switch restores old settings and invalidates the duplicate cache.
A confirmed response can be replayed by the duplicate-request mechanism.
The client probes after a lost confirmation ACK before deciding to recover;
this avoids blindly restoring its old baud after the server already committed.

INFO layout is Python struct `<8B6I2IHHB`, followed by the running executable's
root-relative CP437 path and NUL (45 fixed bytes plus pathZ):

- u8: version=6, COM number, baud index, flow, peer mask, FIFO available, MSR, MCR.
- six u32 counters: failed frame CRCs, overruns, parity errors, framing errors,
  break indications, transmit/handshake timeouts. Counters reset on server start.
- u32 free bytes, u32 total DOS-volume bytes.
- u16 maximum frame payload, u16 selected transfer block size.
- u8 settings state (0 settled; other values mean a transition is pending).

DELETE removes one file or empty directory. The client scans and orders recursive
operations deepest-first. The protected flag must be 1 to remove an R/H/S entry.
Regular-file metadata must still match all nine bytes; directory attributes/type
must match, while timestamps can change as children are removed. Root, running
server/ancestor paths, and the reserved cache tree are rejected. If clearing
attributes is necessary but deletion fails, the prior attributes are restored.
Malformed requests fail explicitly. RENAME validates both paths and metadata,
rejects existing destinations and moving a directory into itself, and verifies
result metadata. CRC framing and duplicate suppression cover both commands.

RTS/CTS is implemented by the polled DOS driver, not by assuming automatic flow
control hardware in the 16C552. RTS is low during request processing/disk I/O, frame CRC checking and console
output, and high while receiving. Since 3.1 an 8086 assembly routine captures
each header and body directly, avoiding per-byte C housekeeping. A UART fault
or inter-byte timeout discards the incomplete frame before command dispatch. Every transmit byte checks CTS when flow is enabled and
checks any required DSR/DCD bits. DTR is asserted while the server is running.
Three-wire mode does not gate on CTS or require peer-status signals by default.
Linux enables CRTSCTS only in that mode; optional peer checks gate its writes.

Downloads use 4 KiB DOS read-ahead. The rolling file CRC is advanced only for the
bytes in a new response; duplicate requests replay the cached response and do
not advance the file or CRC. Uploads retain the original staging/commit path.
The version 2 on-disk cache signatures and backup manifest formats are unchanged.

## Clock extension (3.2.1)

CLOCK (43h) uses `year u16 LE, month, day, hour, minute, second, hundredth`
(eight bytes total). This is local wall time, not UTC or a packed FAT timestamp.
The year must be 1980-2099; all calendar and time fields are validated before
any mutation. Empty requests read the clock; lengths other than zero/eight fail.
Requests are rejected during an active file operation or pending settings change.

Setting calls DOS INT 21h time/date services, time first to avoid an old clock
near midnight advancing the requested date between calls. Errors identify a
possible partial update. Readback checks date/time/date to avoid mixing dates
across midnight. BIOS-tick-based idle state is refreshed after changing the clock.
Duplicate requests replay their cached response without setting the clock again.
The client uses a fresh read after the set ACK, allowing elapsed retry time and
up to two seconds of clock/serial tolerance when checking the result. A lost or
invalid reply is not reported as success. An older server's Unknown command
response becomes an explicit upgrade message; other v3 commands are unchanged.
