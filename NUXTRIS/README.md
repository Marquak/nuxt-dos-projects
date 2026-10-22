# NuXTris

Play alone or against a friend on two DOS computers. The selector opens the
single-player game or the multiplayer game, then returns to the menu when you
quit. Put these three files in the same writable DOS folder and run `NUXTRIS`:

| File | Purpose |
| --- | --- |
| `NUXTRIS.EXE` | Game selector |
| `SP_TRIS.EXE` | Single-player 1.3 |
| `MP_TRIS.EXE` | Multiplayer 1.0 |

Single-player has Marathon, Sprint 40, and an endurance B-Type with a starting
pile height. Multiplayer has the same modes, a shared piece sequence, and
serial or LAN play. Only one game runs at a time. See [README.TXT](README.TXT)
for controls and setup, and the detailed [single-player](SOURCE/SP/README.TXT)
and [multiplayer](SOURCE/MP/README.TXT) guides.

For LAN play, load a packet driver on both DOS PCs and set `MTCPCFG` to the
full DOS path of your mTCP configuration file, such as
`SET MTCPCFG=C:\NET\MTCP.CFG`. It needs `PACKETINT`, `IPADDR`, and `NETMASK`.
`PACKETINT` must match the packet driver's software interrupt. The host's
address appears in the multiplayer menu. For serial play, use matching baud
and flow settings on each PC. [LANBOX.SH](LANBOX.SH) and [PLAY.SH](PLAY.SH)
start DOSBox-X on Linux.

Keep existing `NUXTRIS.DAT` and `NUXTRIS.BAK` for single-player scores and
settings. Multiplayer uses `NXTMULT.CFG`, `NXTMULT.BAK`, and `NXTMATCH.TXT`.
Copy these personal files into the new game folder when upgrading. They are
not part of this release. Replace all three executables together; the new
multiplayer wire version does not connect to older builds.

The selector and single-player game require DOS 3.3+, VGA, and a V20/V30 or
80186-compatible CPU. Multiplayer also requires the serial or network
hardware you choose. Build all three with Open Watcom 2 using
`WATCOM=/path/to/watcom sh BUILD_ALL.SH`. Sources, license notices, and
test notes are under [SOURCE](SOURCE/). The selector and single-player code
are MIT licensed. Multiplayer is GPL-3.0-or-later because its packet-driver
adapter adapts mTCP code. Coded with GPT-6 Codex, with game design and NuXT
testing by Marquak.

The selector, SP 1.3, and MP 1.0 passed the DOSBox-X checks described in
[TESTS.TXT](TESTS.TXT). The first physical NuXT run found the child EXEs but
could not open their folder. The selector now starts the games beside it
without changing directories. This passed focused DOSBox-X checks; it still
needs a NuXT retest.

![Single-player game](SCREENSHOT.PNG)
![Multiplayer game](MP-SCREENSHOT.PNG)
