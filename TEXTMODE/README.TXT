TEXTMODE 1.0
============
Switch VGA text between 80x25 and 80x50 without ANSI.SYS.
287-byte assembly COM program. Nothing stays resident.

INSTALL
Copy TEXTMODE.COM and IRC50.BAT to C:\NET\MTCP.
IRC50.BAT expects IRCJR.EXE there too. Edit its paths if needed.
Your existing SET MTCPCFG=... setting is still required for IRCjr.

If ANSI.SYS was added just for changing rows, comment out that DEVICE or
DEVICEHIGH line in CONFIG.SYS and reboot. Keep your existing Vidsync line.
There is no need to load a second copy of Vidsync.

USE
  TEXTMODE 50       80 columns, 50 rows
  TEXTMODE 25       80 columns, 25 rows

From C:\NET\MTCP, run IRC50 with the arguments normally passed to IRCJR.
It switches to 50 rows, launches IRCjr, then restores 25 rows on return.
Alt+X exits IRCjr. Interrupting the batch or resetting the PC can prevent
its final restore command; run TEXTMODE 25 manually if necessary.
The wrapper forwards up to nine arguments, as supported by MS-DOS batches.

DETAILS
Requires a VGA-compatible BIOS reporting a VGA display combination code.
Both settings use 400 scanlines, mode 3 and ROM fonts. The screen is cleared
when changing mode. 50 rows uses 8x8 characters; 25 rows uses 8x16 characters.
No ANSI driver, direct hardware register writes, FPU or TSR required.
Uses 8086 instructions, which also run on the NuXT's V20.

Normal INT 10h calls pass through installed hooks such as Vidsync. Vidsync
only calibrates when the BIOS mode number changes, and both row counts are
mode 3. This program does not force another calibration. Removing ANSI may
help your setup, but LCD alignment needs a physical NuXT test.

Exit codes: 0 success, 1 invalid arguments, 2 VGA not detected,
3 BIOS geometry did not match the requested mode.

BUILD
On Linux with GNU binutils installed:
  sh BUILD.SH
Source is TEXTMODE.S, GNU assembler Intel syntax.

CHECKED
DOSBox-X, VGA and 8086 CPU setting, without ANSI.SYS:
80x50/8x8 font, 80x25/8x16 font, invalid/missing arguments, surrounding
spaces, and unchanged geometry after an invalid argument. The BIOS data
area was read independently to check results. No 86Box tests.
I also confirmed the IRCjr wrapper works on my NuXT.
The automated checks did not emulate the physical LCD/Vidsync combination.

REFERENCES
Vidsync source, reviewed for INT 10h hook behaviour:
https://github.com/jhhoward/Vidsync/blob/main/vidsync.s
FreeDOS SCREEN/font reference:
https://home.mnet-online.de/willybilly/fdhelp-108a/en/hhstndrd/cnfigsys/screen.htm
BIOS API reference:
https://fd.lod.bz/rbil/
Local mTCP IRCjr SCREEN.CPP was reviewed for BIOS row-count detection.
No source code was copied from these projects.

Coding: ChatGPT / Codex. Emulator checks: Codex.
Physical NuXT testing: I confirmed IRC50 works on 2026-10-15.
Serial transfer preserved the batch; FTP text conversion introduced extra
carriage returns. Use Binary transfer mode for BAT/CFG/TXT as well as COM.

IRC50 irc.example.net #channel forwards those arguments to IRCjr.
To save a favourite server/channel, replace the %1 through %9 on the IRCJR
line with your server and #channel. Optional port: -port 6668 before server.
