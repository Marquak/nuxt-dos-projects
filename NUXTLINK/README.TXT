NUXT LINK 3.3.3
===============
Linux-only connection fixes. DOS remains 3.3.1; no DOS update needed if current.
Restart the Linux client to use the fixes.
If the previous DOS folder is gone, Connect opens the drive root instead.
Queued source/destination paths are preserved.
Stop the DOS server before replacing NUXFER.COM. Use FTP in binary mode.
For a NuXTLink transfer, send it as NUXNEW.COM first, disconnect and exit
the server, then replace NUXFER.COM with that file. Run:

  NUXFER COM1

Default startup is 9600 baud, no flow/peer requirements, 128-byte blocks.
In Linux keep your desired transfer baud and click Connect. The client
finds the server and switches both ends to your chosen settings.
Disconnect pauses briefly and closes locally, even if DOS has exited.
It keeps a running server at its current speed; Connect finds it again.
Disconnect also stops stalled operations while preserving resumable files.
Manual baud and flow options still work; the client searches other rates
if the server is already running with different settings.

Replace the complete Linux folder, including transfer_queue.py, then run:
  sh linux/launch.sh
Saved profiles remain compatible.
NUXFER.COM is the single current DOS binary. Wire protocol is v6.
When upgrading from before 3.3.1, update DOS and Linux together.

LARGER BLOCKS: choose 2048 under Connection options, then Apply changes.
Try 4096 on the same file for comparison. Save your profile afterward.
Existing profiles keep their previous size. CRC and resume stay enabled.

COMPRESSION: Compress uploads is on by default; profiles save the choice.
Linux compresses useful blocks with heatshrink. DOS expands them in assembly.
Poorly compressible blocks stay raw. Downloads use faster assembly CRC and
FIFO transmission, without DOS compression work. All verification stays on.
Double-click completed uploads to see payload savings and block counts.
File KiB/s measures original file data, not compressed serial bytes.
If the Linux helper cannot load, run sh linux/build_codec.sh from this
release folder, or turn off Compress uploads. See README.md for details.

COMPLETED FILES: Time counts active work, including retries and verification,
excluding queue waits and pauses. Compression shows a ratio and % saved.
For resumed uploads the ratio covers the final resumed portion only.
Both appear in the queue, entry details and saved reports.

3.3.1 FIX: DOS rereads the staging file before replacing the destination.
Earlier versions checked received CRC but did not perform this disk readback.
Update both sides. The new verification is bounded and cancellable.

SYNC DOS DATE/TIME: click the button beside Diagnostics while connected
and idle. Sends Linux local time and date, then checks the DOS clock.
Update both DOS and Linux sides. Existing file timestamps are unchanged.
Manual sync only; normal DOS clock resolution and serial delay apply.

FULL HANDSHAKE: select rtscts; peer both needs DSR and DCD wired.
THREE WIRES: select flow none and peer none.
Supported baud: 1200 2400 4800 9600 19200 38400 57600 115200.
/FLOW:NONE or RTSCTS. /PEER:NONE, DSR, DCD, BOTH. /BLOCK:128, 256, 512, 2048, 4096.
Example manual startup:
  NUXFER COM1 115200 /FLOW:RTSCTS /PEER:BOTH /BLOCK:2048
Idle keys: B baud, F flow, D peer, C COM, S status. Esc exits.
The ENTIRE drive containing the running executable is shared.

QUEUE: prepare files, browse elsewhere and add more. Each addition keeps
its source and destination. Uploads and downloads can share the queue.
Duplicate target conflicts leave the old queue intact. Clear the queue
before preparing deletions. Cancel retains pending work and partials.

CRC/file verification, resumable transfers and DOS timestamps/attributes
remain enabled. UART counts are cumulative until server restart.
See README.md, CHANGES.md and TESTING.md for details and test limits.
This release does not include a floppy image.
