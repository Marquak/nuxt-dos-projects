NUXT LINK 4.1 - SERIAL FILE MANAGER

Copy NUXFER.COM in binary mode to a new DOS folder. Exit any running
serial server before starting it. Do not replace a running COM file.

  C:\UTILS\NXLINK41\NUXFER.COM COM1

The server starts at 9600 baud for discovery, then negotiates the chosen
speed. Esc exits. It shares the entire drive containing its executable.

On Linux, run sh linux/launch.sh from this matching 4.1 package. Select
your serial device and Connect. Defaults: 115200 baud, RTS/CTS, 4096-byte
blocks. Use Flow: none for a three-wire cable. Python 3.10+ and Tkinter
are required. Wire version 8 does not connect to earlier wire-v6 servers.

Transfers use frame and whole-file CRC16, staged writes, readback checks,
and resume after interruption. Linux can send raw, zero-run, heatshrink or
LZ4 blocks; DOS sends raw or zero-run blocks. Dates and attributes are
preserved. The Linux queue shows time and compression for each file.

This release is serial only. Earlier LAN versions used a different client
and wire protocol; their files are not included here.

See README.md and TESTING.md on Linux for setup, results, source credits,
and licenses. Code written with GPT-6 Codex.
