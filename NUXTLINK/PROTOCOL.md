# NuXT Link 4 serial protocol (wire version 8)

`NUXFER.COM` answers HELLO with `NUXTLINK/8`; the matching Linux client rejects wire-v6 servers. This release uses serial only. The 4.0 DOS server also speaks version 8 and works with this client. Earlier LAN files are not included.

Each frame is `NX`, command byte, sequence byte, little-endian payload length, payload, and little-endian CRC16/CCITT-FALSE over the command through payload. The CRC starts at `FFFFh`; the polynomial is `1021h`. The serial payload limit is 4096 bytes. ACK is `80h`, and ERROR is `FFh` with ASCII error text. The client sends one request at a time and retries the same sequence after a timeout. DOS replays the saved response to an identical request, so retransmission cannot write a block twice. Sequence numbers wrap at 256.

The original v6 file and control commands are retained, including directory listing, metadata, DOS clock, settings negotiation, staged uploads, resume scans, pause, and whole-file CRC checks. Uploads still accept raw `11h` and heatshrink `14h` blocks. Wire version 8 adds:

| Command | Request | Response |
|---|---|---|
| `16h` PUT_ZERO_RLE | decoded length u16, decoded CRC16 u16, zero-run bytes | empty ACK after decoding, checking the block and writing it to staging |
| `17h` PUT_LZ4 | decoded length u16, decoded CRC16 u16, raw LZ4 block bytes | empty ACK after bounded decoding, checking, and writing |
| `25h` GET_PACKED | empty | mode u8, decoded length u16, then raw or zero-run bytes |

GET_PACKED mode 0 contains raw bytes; mode 1 contains zero-run data. The decoded length must be 1 through the negotiated block size, remain within the file, and equal the actual decoded bytes. A raw response uses at most 4093 file bytes because the three-byte mode/length header shares the 4096-byte payload. The server sends mode 1 only when the encoded form saves at least 35 bytes; otherwise it sends mode 0. The client checks the frame CRC, decodes the block, rolls the whole-file CRC over decoded bytes, and verifies the final CRC against DOS. Linux also rereads the staged download before committing it.

Zero-run encoding is byte oriented. A control byte with bit 7 set emits `(control & 7Fh) + 1` zero bytes. A control byte with bit 7 clear introduces that many following literal bytes. Each count is 1 through 128. The encoder uses zero runs of at least three bytes; shorter runs remain literals. The decoder rejects truncated literals, output beyond the declared length, and an output shorter than the declared length. Uploads also check a CRC16 of the decoded block before writing. Raw LZ4 upload blocks are independently compressed and bounded to 4096 decoded bytes, so matches can only refer to bytes within the current output block. Linux normally sends the shortest eligible raw, zero-run, heatshrink, or LZ4 packet. At 115200 baud and 4096-byte blocks, it can prefer an LZ4 packet up to 512 bytes larger than the heatshrink winner because the NuXT decodes LZ4 faster. Zero-run and raw winners are not overridden.

Every upload begins with the intended file size, DOS metadata, and whole-file CRC. DOS writes to a private staging file. The client checks any retained prefix before resume. After transfer, DOS closes and reopens staging, scans it from disk, compares its CRC, then commits the file. Downloads check the retained Linux prefix before resume. An empty file is handled without data blocks. Settings changes are confirmed at the new speed or rolled back after a timeout. Both sides retain partial data after an interrupted transfer.

The protocol has no authentication or encryption. It is intended for a local serial cable. Paths are root-relative, NUL-terminated CP437 with DOS-safe names; DOS adds the drive containing `NUXFER.COM`. See the source for the complete command set and error handling.
