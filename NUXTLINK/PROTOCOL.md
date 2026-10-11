# NuXT Link protocol 3

V3 is not compatible with v2 or v1. Both clients and servers must be upgraded.

## Frames and transport

512-byte maximum payload; selected transfer blocks default to 128 bytes. Frame: `NX`, kind u8, sequence u8, payload length
u16 LE, payload, CRC16 u16 LE. CRC16/CCITT-FALSE uses polynomial 1021h and seed
FFFFh over kind/sequence/length/payload. Binary data is never escaped or converted.
ACK=80h, ERROR=FFh with an ASCII description. Stop-and-wait: retry the exact
request/sequence. The server compares the complete last request and caches its
response, so duplicate writes, reads, final commits and directory advancement
are replayed without being repeated. Sequence numbers wrap at 256.

The client has five attempts per command, with bounded timeouts scaled for baud.
The server uses a one-second inter-byte receive timeout and closes active file
handles after about 30 seconds idle. Staging survives timeout, Escape, HELLO and
CANCEL; uncommitted destinations are not exposed under the final filename.

## Version 3.2 connection discovery

Wire protocol remains v3. The server now defaults to 9600/none/none/128.
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
| 01 | HELLO | empty | `NUXTLINK/3 C:\ file server` (drive varies); pauses active file |
| 02 | LIST_FIRST | path (root allowed) | meta + nameZ; empty means end |
| 03 | LIST_NEXT | empty | same as LIST_FIRST |
| 04 | STAT | path | meta |
| 05 | MKDIR | path | empty; existing directory allowed |
| 06 | METADATA | meta + path | empty, after setting and verifying metadata |
| 07 | HASH_BEGIN | path | meta; begins disk-only whole-file scan |
| 08 | HASH_STEP | empty | scan; ready=1 closes the file |
| 10 | PUT_BEGIN | meta + wholeCRC u16 + overwrite u8 (0/1) + path | retained prefix length u32 |
| 13 | PUT_STATUS | empty | scan of retained prefix; ready=1 enables PUT_DATA |
| 11 | PUT_DATA | 1..512 binary bytes | empty |
| 12 | PUT_END | wholeCRC u16 | matching wholeCRC; committed only after length, CRC and metadata validation |
| 20 | GET_BEGIN | resume offset u32 + path | meta; begins prefix scan |
| 24 | GET_STATUS | empty | scan; ready=1 enables GET_DATA |
| 21 | GET_DATA | empty | up to the selected block size (128/256/512 bytes) |
| 22 | GET_END | empty | whole-file CRC after source metadata recheck |
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
Only block sizes 128,256,512 are accepted. SETTINGS is idle-only and rejects
required modem inputs that are already low. The server finishes transmitting
the old-speed ACK, waits three BIOS ticks, applies the new settings, and awaits
CONFIRM for 218 ticks. It rejects unrelated commands while pending. An
unconfirmed switch restores old settings and invalidates the duplicate cache.
A confirmed response can be replayed by the duplicate-request mechanism.
The client probes after a lost confirmation ACK before deciding to recover;
this avoids blindly restoring its old baud after the server already committed.

INFO layout is Python struct `<8B6I2IHHB`, followed by the running executable's
root-relative CP437 path and NUL (45 fixed bytes plus pathZ):

- u8: version=3, COM number, baud index, flow, peer mask, FIFO available, MSR, MCR.
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
