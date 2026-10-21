NUXTRIS MULTIPLAYER 1.0

Play head-to-head Tetris on two DOS computers over a serial cable or LAN.
Both players get the same piece order. The host chooses one of three modes:

MARATHON   The level rises every 10 cleared lines, up to level 19. The first
           player to top out loses.
SPRINT 40  The first player to clear 40 lines wins. The selected starting
           level stays fixed. A top-out also ends the match; the survivor wins.
B-TYPE     Both players start with the same random pile at height 0 through 5.
           Every 25 lines raises the level by one, up to level 9. Play keeps
           going at level 9 until someone tops out. The survivor wins.

This B-Type is an endurance variation. The original Game Boy B-Type ends after
25 lines instead of continuing to another level. Both boards show score,
lines, level and the shared elapsed game time. Either player can pause both
boards. A Tetris briefly freezes play and displays its player's name on both
screens. The full-screen effect lasts about 2.2 seconds, then play resumes.

Run NXTNET.EXE to use the menu. Set your name, connection and, on the host,
mode, starting level and B-Type height. The joiner receives the host's rules.
Up/down selects, left/right changes a setting, and Enter chooses it. A
calibrated gamepad can navigate the menu. Press J to calibrate or K to use
the keyboard. The game-over screen names the winner and says whether the win
came from reaching 40 lines or surviving a top-out. Both PCs save matching
results with both names, scores and rules.

LAN CONNECTION

Use this NXTNET.EXE on both DOS computers. Match wire version 4 cannot join
an older NuXTris multiplayer build. Load your Ethernet packet driver first.
Set MTCPCFG to the full path of your mTCP config file before starting:
  SET MTCPCFG=C:\NET\MTCP.CFG
The file needs PACKETINT, IPADDR and NETMASK; PACKETINT must match the packet
driver's software interrupt, such as 0x60. Add GATEWAY if the other PC is
outside your subnet. NuXTris uses UDP port 47587. No mTCP program needs to
remain resident.

In CONNECTION, set LINK to LAN. HOST GAME shows the host IP. An invalid or
missing network config displays IP ERROR. On the other PC, choose JOIN GAME
and enter that host IP. Either side may start waiting first. Esc then Y
cancels a wait.

Command-line examples:
  NXTNET /LAN /HOST /NAME:BOB /SPRINT
  NXTNET /LAN /JOIN /IP:192.168.50.8 /NAME:JON
  NXTNET /LAN /HOST /BTYPE /LEVEL:2 /HEIGHT:3
The host can select /MARATHON, /SPRINT or /BTYPE, /LEVEL:0..9, and, for
B-Type, /HEIGHT:0..5. /QUICK skips the menu. A busy, missing or incompatible
packet driver produces a message in the menu. Packets carry a CRC and full
board snapshots, allowing play to recover from a lost update. A one-second
silence suspends play until valid updates return.

DOSBOX-X LAN CONNECTION

On this Linux computer, run ./LANBOX.SH from this folder. It starts DOSBox-X
with an NE2000 adapter and opens the NuXTris menu set to join 192.168.50.8.
On the NuXT, load its working packet driver and run this release of
NXTNET.EXE as LAN HOST. Use ./LANBOX.SH 192.168.50.x if the NuXT address
changes. The launcher keeps its DOSBox-X settings and scores in DOSBOX-LAN.
It expects /home/amigo/Downloads/NE2000.COM; set NUXTRIS_DRIVER to another
full path if needed. NUXTRIS_CYCLES adjusts the default 3000 cycles. No
router port forwarding is needed because DOSBox-X starts the connection.

SERIAL CONNECTION

Set LINK to SERIAL on both sides. Match COM port, baud rate and RTS/CTS or
3-WIRE settings; COM numbers can differ. Defaults: COM1, 38400, RTS/CTS.
Close NuXTLink or anything else using the adapter. On Linux, launch DOSBox-X
through DOSBOXX.SH and its Python helper. DOSBOXX.SH --help explains the
adapter and mode presets. /BRIDGE aligns the virtual COM settings with the helper.

CONTROLS

Arrows or A/D move; Down or S soft drops; Up, W, X, Space or Enter rotates
clockwise; Z rotates back. P pauses or resumes both games. Esc asks before
leaving a match. After both PCs save the result, Enter or R asks for a
rematch; both players must agree. F1 changes the theme, F2 sound and F3 link
info. The gamepad's first two buttons rotate in either direction. Gamepad
Up does nothing during play but still works in menus.

FILES AND REQUIREMENTS

NXTMULT.CFG stores your name, connection and game choices. NXTMULT.BAK is its
recovery copy. Version 1 or 2 settings load automatically; their previous
mode maps to Marathon, while name, level, connection and other preferences
are retained. Saving writes version 3. NXTMATCH.TXT is the paired result
journal. Keep these personal files when updating the game. Do not replace a
running EXE; exit it before copying the new one in binary mode.

V20/V30 or 80186-compatible CPU, DOS 3.3+, VGA 640x480x16 and 640 KB RAM.
AdLib and a standard analog gameport are optional. LAN needs a working
Ethernet packet driver. Serial needs standard COM1/COM2 BIOS addresses and
dedicated IRQ4/IRQ3 respectively.

SOURCE, LICENSE AND TESTING

Coded with GPT-6 Codex, with design direction and NuXT testing by Marquak.
The game and serial components are MIT licensed. LAN.H adapts Michael B.
Brutman's mTCP packet receiver and registration sequence under
GPL-3.0-or-later, so the combined NXTNET.EXE uses GPL-3.0-or-later.
GPL3.TXT, LICENSE.TXT, UPSTREAM.TXT and CREDITS.TXT give the terms and
credits. Open Watcom 2 builds the program with WATCOM=/path/to/watcom
sh BUILD.SH.

Version 1.0 passed packet validation and two-client DOSBox-X serial
matches in all three modes. The Sprint goal, B-Type 25-line level step and
level-9 cap, survivor win rule, identical result journals and old-settings
migration were checked. A DOSBox-X LAN smoke test exchanged valid new-wire
snapshots. These are emulator checks; the new modes still need a physical
NuXT-to-PC playtest. The previous 0.6.6 release is preserved separately.
