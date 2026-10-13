NUXT LINK 3.2.1
===============
Copy NUX321.COM beside your existing server. Disconnect Linux, exit the
old DOS server with Esc, then start from its folder:

  NUX321 COM1

Default startup is 9600 baud, no flow/peer requirements, 128-byte blocks.
In Linux keep your desired transfer baud and click Connect. The client
finds the server and switches both ends to your chosen settings.
Normal Disconnect returns the server to 9600 for the next connection.
Manual baud and flow options still work; the client searches other rates
if the server is already running with different settings.

Replace the complete Linux folder, including transfer_queue.py, then run:
  sh linux/launch.sh
Saved profiles remain compatible. For all fixes, update both sides.
NUXFER.COM, NUXFER32.COM and NUX321.COM are identical; protocol remains v3.

SYNC DOS DATE/TIME: click the button beside Diagnostics while connected
and idle. Sends Linux local time and date, then checks the DOS clock.
Update both DOS and Linux sides. Existing file timestamps are unchanged.
Manual sync only; normal DOS clock resolution and serial delay apply.

FULL HANDSHAKE: select rtscts; peer both needs DSR and DCD wired.
THREE WIRES: select flow none and peer none.
Supported baud: 1200 2400 4800 9600 19200 38400 57600 115200.
/FLOW:NONE or RTSCTS. /PEER:NONE, DSR, DCD, BOTH. /BLOCK:128, 256, 512.
Example manual startup:
  NUX321 COM1 115200 /FLOW:RTSCTS /PEER:BOTH /BLOCK:512
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
