NUXT LINK 3.1 - DOS RECEIVE-OVERRUN PATCH
========================================
This is a DATA floppy, not a boot disk.
dos/NUXFER.COM and NUXFER31.COM are identical. Use the alternate name to keep
an old server available while copying the new one.

Existing NuXT Link 3 Linux clients work with this patch unchanged.
When upgrading from version 1/2, update both DOS and Linux.

Upload NUXFER31.COM using 57600, then disconnect Linux and exit the old
DOS server with Esc. On DOS, from the folder containing the update:

  NUXFER31 COM1 115200 /FLOW:RTSCTS /PEER:BOTH /BLOCK:512

Select 115200, rtscts, peer both, block 512 in Linux and reconnect.
For a 3-wire cable use /FLOW:NONE /PEER:NONE and a suitable lower baud.

The patch uses a short 8086 receive loop and holds RTS low during CRC
checking, disk work and console output. File CRC, resume validation and
DOS timestamps/attributes remain unchanged. It is compatible with V20.

The four UART counts mean overrun / parity / framing / break.
They are cumulative until you exit/restart, even when settings change.
Press S while idle after an upload and check whether overruns increased.
Use 57600 if errors continue. Emulator tests cannot prove real NuXT timing.

Start at 57600 instead by replacing 115200 in the command above and in
Linux. Supported baud rates: 1200 2400 4800 9600 19200 38400 57600 115200.
/FLOW:NONE or RTSCTS. /PEER:NONE, DSR, DCD, BOTH. /BLOCK:128, 256, 512.
Idle keys: B baud, F flow, D peer, C COM, S status. Esc exits.
At very low baud, Esc can wait for the current packet to finish.
The program shares the ENTIRE drive containing the running executable.

See README.md, CHANGES.md and TESTING.md in this folder for details.

LINUX UI REFRESH - 8 OCTOBER 2026
The ZIP now includes a revised Linux interface. Replace the full linux/
folder (including interface.py), then run sh linux/launch.sh as before.
It remembers each file pane's sorting across folders, offers name filters,
and shows source/destination plus separate file and queue progress.
This UI update does not require replacing the DOS executable again.
