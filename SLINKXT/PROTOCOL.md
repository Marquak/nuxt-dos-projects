# Slink-XT serial protocol (wire version 1)

The DOS `SLINK.COM` server answers HELLO with `SLINKXT/1` and the drive containing its executable. The Linux client requires that greeting. Slink-XT has a separate wire identity and does not accept NuXTLink frames.

Each frame is `SX`, a command byte, a sequence byte, a little-endian 16-bit payload length, the payload, and a little-endian 16-bit BSUM of the command through payload. BSUM starts at zero. For each byte, rotate the 16-bit accumulator right by one bit and add the byte modulo 65536. The maximum payload is 4096 bytes. ACK is `80h`; ERROR is `FFh` followed by ASCII error text.

The Linux client keeps one request in flight. On timeout it retries the same sequence and packet. DOS caches the previous response to an identical request, so a retry does not write data twice. The sequence byte wraps at 256.

Uploads may send raw, zero-run, heatshrink, or raw LZ4 blocks. Each compressed upload block declares its decoded length and BSUM; DOS checks the decoded bytes before staging them. LZ4 blocks are independently compressed with no cross-block dictionary. DOS downloads use raw or zero-run blocks. The declared decoded length must fit the negotiated block size and remaining file length.

An upload begins with file metadata and the expected whole-file BSUM. After all blocks arrive, DOS closes and rereads its staging file, compares the complete checksum, and only then commits it. Linux verifies a retained upload prefix before resuming. Downloads similarly verify any retained prefix; Linux rereads the staged file before committing it. The Linux side also calculates SHA-256 for its local manifest and reports.

Paths are root-relative, NUL-terminated CP437 DOS names. DOS adds the drive containing `SLINK.COM`. The server manages resumable upload files under its private `\SL$CACHE` directory. Serial settings are negotiated and confirmed at the new speed, with rollback if confirmation times out. See source for command payload layouts and error handling.
