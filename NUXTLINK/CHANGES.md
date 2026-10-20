# NuXT Link 4.1 (19 October 2026)

- At 115200 baud with 4096-byte blocks, choose LZ4 over heatshrink when the LZ4 packet is no more than 512 bytes larger. This reduced real NuXT upload time for an ordinary executable and mixed disk-image data while preserving CRC checks and staged readback. Other link settings keep the smaller-packet choice.
- Keep serial wire version 8. The 4.0 DOS server is compatible with the 4.1 Linux client; the new DOS binary changes its startup version label only.

## Earlier 4.0 release

- Promote the tested Fast serial build to the main NuXT Link release. The DOS program is now `NUXFER.COM`; serial wire version 8 is unchanged from the 0.3.2 preview.
- Import serial profiles from the Fast preview and the earlier combined serial/LAN client into a new profile store. Both earlier stores remain untouched.
- The earlier LAN build used wire version 6 and needed its own Linux client. It is not included in the 4.1 release.
- Pass a physical NuXT interrupted upload and resume across a serial reconnect, then download and compare all 128 KiB byte for byte.

## Earlier 0.3.2 preview

- Scan up to 8 KB per verification request using the existing 4 KB DOS buffer. This halves the request count for large file readback and hash checks without changing the serial protocol or conventional-memory buffer size.

## Earlier 0.3.1 preview

- Keep the previous Linux destination available until the downloaded file's metadata is saved. Use a hard link where supported or copy it on filesystems without hard links. If metadata saving fails, restore the old file and retain the verified staging file for retry.
- Show recorded link counters without adding simultaneous UART framing and break flags into a misleading error count.
- Correct the DOS startup banner to identify the matching 0.3.1 preview. The serial wire format remains version 8.

## Earlier 0.3 preview

- Sped up the 8088-compatible DOS LZ4 decoder for repeated bytes and nonoverlapping matches without increasing its 4 KB request buffer.
- On the NuXT at 115200 baud, `MATRIX.IMG` uploaded in 61.0 seconds and a 256 KB mixed-data file in 23.8 seconds. Both passed DOS staging readback and a separate file CRC query. QEMU roundtrips, boundary cases, and malformed packets passed. The Linux GUI and other file types still need hands-on checks.
- Corrected the connection-test fixture to report the current wire version. The client and DOS server still use wire version 8, so this preview does not require a protocol migration.

## Earlier 0.2 preview

- Added optional raw LZ4 upload blocks with a bounded 8088-compatible DOS decoder and a per-block Linux encoder choice. The matching Linux and DOS programs now use serial wire version 8.
- The codec passed QEMU upload, staging readback, and download checks.

## Earlier 0.1 preview

- Forked NuXT Link into a separate serial-only preview with the familiar Linux file manager and queue. It uses `NUXFAST.COM` and serial wire version 7, leaving the main `NUXFER.COM` installation alone.
- Added DOS zero-run compression for downloads. Linux chooses zero-run, heatshrink, or raw upload blocks according to which saves space.
- Kept staged upload readback, frame and whole-file CRC16, resume checks, DOS metadata, and date/time sync. The queue now reports compression for downloads as well as uploads.
- First launch reads existing serial profiles for convenience, then saves future edits in a separate profile directory.
- Built the DOS binary and completed focused QEMU roundtrip and malformed-block checks. Physical NuXT speed and reliability still need measurement.
