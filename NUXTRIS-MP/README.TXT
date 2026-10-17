NUXTRIS MULTIPLAYER 0.6.4 - SERIAL AND LAN PREVIEW

Two live boards and the same piece sequence. Play a timed score race or
unlimited sudden death. The first top-out ends sudden death, and the higher
score wins. Either player can pause or resume both games. Completed results
with both names and scores are saved on both computers.

Run NXTNET.EXE for the menu. Set your name before starting. The host chooses
the mode and starting level; the joiner receives those choices. Up/down
selects, left/right changes a setting, Enter chooses it. A calibrated gamepad
can navigate the menu with its directions and first two buttons. Press J to
calibrate, or K to use the keyboard.

LAN CONNECTION

Use NXTNET.EXE on both DOS computers. Version 0.6.4 uses the same match
protocol as 0.6.1. Load a working Ethernet packet driver first. Set the
MTCPCFG environment variable to the full DOS path of your mTCP config file,
in AUTOEXEC.BAT or at the DOS prompt before starting the game, for example:
  SET MTCPCFG=C:\NET\MTCP.CFG
The file must exist at that path and contain PACKETINT, IPADDR and NETMASK.
PACKETINT (for example, 0x60) must match the software interrupt used when
you loaded the packet driver. Add GATEWAY when the peer is outside your
subnet. NuXTris uses UDP port 47587 and does not need an mTCP program to
stay resident.

In CONNECTION, set LINK to LAN. HOST GAME shows the host IP from MTCPCFG.
If its settings are missing or invalid, the menu shows IP ERROR with a red marker.
On the other DOS computer, choose JOIN GAME and set PEER IP to the address
shown on the host. The host learns the joiner's address. Either side can start
first; both wait until the other connects. Esc then Y cancels the wait.

The same LAN setup can be selected on the command line, for example:
  NXTNET /LAN /HOST /NAME:BOB
  NXTNET /LAN /JOIN /IP:192.168.50.8 /NAME:JON
Add /QUICK only if you want to skip the menu. A packet driver that is busy,
missing or incompatible produces a message in the menu. The LAN connection
uses small UDP packets, a CRC and full board snapshots to recover from lost
updates. A one-second silence suspends play until valid updates return.

DOSBOX-X LAN CONNECTION
On this Linux computer, run ./LANBOX.SH from this folder. It starts DOSBox-X
with NE2000 at 300h/IRQ10 and SLIRP, loads your NE2000.COM packet driver,
and opens the NuXTris menu set to join 192.168.50.8. On the NuXT, load its
working packet driver and run this release of NXTNET.EXE as LAN HOST. Choose
JOIN GAME in DOSBox-X and HOST GAME on the NuXT. Either can wait first.
Use ./LANBOX.SH 192.168.50.x if the NuXT address changes. The launcher
keeps DOSBox settings and scores in DOSBOX-LAN, separate from this folder.
It expects /home/amigo/Downloads/NE2000.COM; set NUXTRIS_DRIVER to another
full path if needed. NUXTRIS_CYCLES adjusts the default 3000 cycles. No
router port forwarding is required because DOSBox-X starts the connection.
The launcher is for this Linux machine; a packet driver is not bundled.

SERIAL CONNECTION

Set LINK to SERIAL on both sides, then choose each side's COM port, matching
baud rate and matching RTS/CTS or 3-WIRE setting. COM numbers may differ.
Default: COM1, 38400 baud, RTS/CTS. Close NuXTLink or any other program using
the adapter. On Linux, launch DOSBox-X through DOSBOXX.SH and its Python
helper. The script remembers the USB adapter; the game's /BRIDGE option keeps
its virtual COM settings aligned with the helper. See DOSBOXX.SH --help.

GAME CONTROLS

Arrows move; Down soft drops; Up, X, Space or Enter rotates clockwise; Z
rotates back. P pauses or resumes both games. Esc asks for confirmation to
leave the match. After both computers save the result, Enter or R requests a
rematch. Both players must agree. F1 changes theme, F2 sound, F3 link info.
The gamepad's first button rotates and its second button reverses rotation.

The host ends a timed match at the selected limit, or sooner if both players
top out. One topped-out player can watch while the other finishes. In sudden
death, the first top-out ends the match immediately, then the higher score
wins, including when the player who topped out has the higher score. Equal
scores are a draw. There are no garbage attacks. Local play runs from the
local PIT clock; rendering and link traffic do not set the fall rate.

FILES AND REQUIREMENTS

NXTMULT.CFG saves your name, connection choice, peer IP, rules, theme and
sound. NXTMULT.BAK is the recovery copy. NXTMATCH.TXT is the paired result
journal. These personal files are not included in the distribution ZIP.
Existing version-1 settings load as serial and save as version 2 on exit.

V20/V30 or 80186-compatible CPU, DOS 3.3+, VGA 640x480x16 and 640KB RAM.
No EMS or 8087 is needed. AdLib and a standard analog gameport are optional.
LAN requires a working Ethernet packet driver. Serial requires standard
COM1/COM2 BIOS addresses and dedicated IRQ4/IRQ3 respectively.

SOURCE AND LICENSE

The game and serial components remain MIT licensed. LAN.H adapts the packet
receiver and registration sequence from Michael B. Brutman's mTCP, which is
GPL-3.0-or-later. The combined NXTNET.EXE is therefore distributed under
GPL-3.0-or-later. GPL3.TXT, LICENSE.TXT, UPSTREAM.TXT and CREDITS.TXT contain
the terms and source credits. All source and BUILD.SH are included. Open
Watcom 2 can rebuild the game with WATCOM=/path/to/watcom sh BUILD.SH.

TEST STATUS

I confirmed a playable NuXT-to-DOSBox-X LAN match with 0.6.1.
A two-client DOSBox-X serial test also completed with matching scores
on both sides and no frame or UART errors. For 0.6.2, a routed DOSBox-X
LAN test reached live play after an unrelated large packet; zero game
frames were rejected or dropped. This diagnostic fix has not been tested
on the NuXT; it does not change match packets, timing or rendering.
Version 0.6.4 fixes a blank HOST GAME row in 0.6.3. The match protocol
and gameplay are unchanged.
No 86Box test was run.
