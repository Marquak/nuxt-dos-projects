SLINK-XT - SERIAL LINK XT

Version 1.0. NuXT serial roundtrips passed.

Copy SLINK.COM in binary mode to a DOS folder named
SLINKXT. Exit any running server before replacing its COM file.

  C:\SLINKXT\SLINK.COM COM1

Use your own drive and COM port. The server serves its executable drive.
It starts at 9600 baud for discovery. Esc exits. The matching Linux
client is started with sh linux/launch.sh after building its helpers.
Default Linux settings: 115200 baud, RTS/CTS, 4096-byte blocks.
Use Flow: none for a three-wire cable.

Each frame and complete file uses a 16-bit BSUM checksum. Uploads are
staged and reread before commit. Downloads are verified on Linux.
Interrupted files can resume. This checksum is faster but weaker than
the CRC used by earlier NuXTLink releases.

This serial-only protocol needs matching Slink-XT programs on both ends.
See README.md and PROTOCOL.md on Linux for details and source credits.
Program code written with GPT-6 Codex. Jim Leonard is credited for
the adapted 8088 LZ4 decoder; see JIM-ORIGINAL-LICENSE.TXT.
