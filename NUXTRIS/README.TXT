NUXTRIS GAME SELECTOR

Run NUXTRIS.EXE in DOS. Choose Single Player or Multiplayer with Up/Down
and Enter. Esc returns to DOS. When a game closes, the selector returns.

Keep these three files in the same DOS folder:
  NUXTRIS.EXE   Game selector
  SP_TRIS.EXE   Single-player NuXTris 1.3
  MP_TRIS.EXE   Multiplayer NuXTris 1.0

The selector uses the same VGA title and menu style as single-player
NuXTris. It keeps only its small menu program in memory while one game
runs. The two games are never loaded together. The selector is about
15 KB on disk and reserves about 18 KB of DOS memory.

Single-player offers Marathon, Sprint 40, and an endurance B-Type with
selectable starting pile height. Multiplayer offers the same three modes
for two DOS PCs over serial or LAN. In a multiplayer game, both players
receive the same piece sequence; the host sets the rules. See the SP and
MP readmes under SOURCE for controls, scoring, and connection setup.

Each game keeps its own files. SP_TRIS.EXE uses NUXTRIS.DAT and
NUXTRIS.BAK; MP_TRIS.EXE uses NXTMULT.CFG, NXTMULT.BAK, and NXTMATCH.TXT.
To bring forward existing scores or settings, copy those personal files
from the old game folders into this folder before launching. Do not copy
older EXEs over these three files. The SP game imports old score formats
automatically; the MP game imports old settings formats automatically.

For Linux DOSBox-X, ./PLAY.SH opens this menu and keeps emulator saves in
DOSBOX. ./LANBOX.SH also loads an NE2000 packet driver. Inside the MP menu,
select LAN and JOIN, then set the host PC's IP address. LANBOX.SH prints
that address as a reminder. On the NuXT, load its packet driver and set
MTCPCFG before choosing a LAN game. Serial play requires a working null
modem cable and matching port, baud, and flow settings at both ends.

The selector and SP game need a V20/V30 or 80186-compatible CPU, DOS 3.3
or later, and VGA. The MP game also uses the network or serial hardware
selected in its own menu. See SOURCE/SP/README.TXT and SOURCE/MP/README.TXT
for each game's detailed requirements.

To rebuild all three executables with Open Watcom 2, set WATCOM to your
Open Watcom directory and run ./BUILD_ALL.SH. The build script compiles the
separate source trees and copies their executables to this folder.

The selector and single-player code are MIT licensed. Multiplayer uses
GPL-3.0-or-later because its packet-driver adapter adapts mTCP code.
Source and license texts are included under SOURCE. Coded with GPT-6
Codex, with game design and NuXT testing by Marquak.

The selector's launch, return, missing-file display, and menu VGA output
passed DOSBox-X tests. The selector starts each game by its path beside
NUXTRIS.EXE without changing the DOS drive or directory. The SP 1.3
build passed 144 focused DOSBox-X checks.
MP 1.0 is the 0.7.0 preview gameplay promoted to a release; its prior
two-client DOSBox-X serial matches and LAN smoke test are documented in
SOURCE/MP/TESTS.TXT. The replacement selector still needs a physical NuXT retest.
