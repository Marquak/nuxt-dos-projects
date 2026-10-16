3D CHECKERS 88 1.2.1

A game of checkers with a thick, beveled board, raised pieces and little crowns
for kings. Play against the computer or share the board with another player.

RUN IT
  CHECK88

Only CHECK88.EXE is needed to play. Copy it to a folder on your DOS drive.
The game creates CHECK88.CFG beside the executable to remember your options.
CHECK88.BAK is the previous settings file. Games are not saved between runs.

Requirements: DOS 3.3 or newer, a NEC V20/V30 or 80186-compatible processor,
and VGA 640x480 in 16 colours (standard mode 12h). No VESA driver needed.
This executable uses 80186 instructions and will not run on a stock 8088.
The executable is about 57 KB, with a minimum loaded program footprint of
about 85 KB including its stack and PSP, excluding DOS and the environment.

Optional: an 8087, AdLib-compatible OPL2 at port 388h, a DOS mouse driver,
and a standard gameport joystick/gamepad at port 201h. An 8087 is detected
at startup; integer projection is used when it is absent. No sound driver
is needed. A mouse needs an installed INT 33h driver such as CuteMouse.

PLAYING
  The red side starts at the front of the board and moves first.
  Men move one square diagonally forward onto a dark square.
  Jump an adjacent opponent onto the empty square beyond to capture it.
  Captures are compulsory. When several are available, choose any route;
  you do not have to choose the route that takes the most pieces.
  Keep jumping with the same piece until its chosen chain is complete.
  Reach the far row to become a king. Crowning ends that turn, including
  a capturing turn. Kings can move and jump diagonally in both directions.
  Men cannot move or capture backwards. There are no flying kings.

  Win by capturing every opposing piece or leaving the opponent unable
  to move. The game automatically draws on the third occurrence of a
  position with the same side to move, or after 40 turns by each side
  without a capture or a man moving. These automate the corresponding
  English/American checkers draw claims; this is not a tournament clock.

  A small gold mark identifies pieces that can move. Select one to see
  its legal destinations. The selected square is gold; the keyboard/gamepad
  cursor is white. If the selected piece has just one legal destination, the
  cursor goes there automatically, but Enter, Space or button 1 is still
  required to make the move. Each jump in a forced chain also needs your
  confirmation. Nothing plays itself just because there is only one choice.

  The previous turn's starting square has a small dim box; its destination
  has a short gold underline. These markers update after a capture leg.
  U undoes the last complete turn, including all jumps, or cancels a partial
  capture chain. Against the computer it takes back your turn and its reply
  together. Up to 128 individual turns are retained, including the exact
  position history needed for draw decisions. New Game clears undo history.

CONTROLS
  Arrow keys       Move between playable squares; up/down follows board rows
  Enter / Space    Select the piece or destination
  Mouse left       Click a piece, then its destination
  Esc / right click / gamepad button 2
                   Cancel selection, or open the menu if nothing can be
                   cancelled. Use U to take back a jump already started.
  Gamepad axes     Move the cursor; navigate menus
  Gamepad button 1 Select, move or confirm
  F1               How to play and controls
  F2               Change sound: off, AdLib, speaker or both
  W / S            Raise/lower the viewing angle
  A / D            Rotate the board, 15 degrees per step, through a full turn
  R                Return to the default view
  U                Undo turn (including the computer reply)
  M                Toggle background music
  [ / ]            Previous / next piece
  - / +            Lower / raise music volume
  F10              Exit to DOS, with confirmation

In menus use up/down to choose, left/right to change an option, and Enter
to activate it. Mouse and gamepad work here too. Tab or arrows changes
Yes/No confirmations; No is selected initially. Esc cancels a confirmation.
Esc resumes an existing game from the menu. Choose Exit or F10 to quit.
While the computer thinks, Esc opens the menu and F10 offers to exit.

OPTIONS
  Opponent         Computer or two local players using the same controls
  Difficulty       Easy, Normal or Hard
  Your colour      Red or Blue against the computer
  Sound            AdLib, PC speaker, both, or off
  Music and volume Track, previous/next, shuffle, music volume, AdLib FX volume
                   Each volume runs from 0 (muted) to 10. Speaker volume is
                   fixed; use Sound to enable/disable the speaker.
  Colours          Neon, Classic, Amber or Ice
  Undo last turn   Same as U during play
  Joystick         Calibrate, or disable joystick input

Opponent and Your colour take effect when you start a NEW GAME. Resume
keeps the current game's players. Other options can be changed mid-game.
The board stays in the same orientation for both players; it does not spin
after each turn. Your camera position survives new games and menu visits
within the current run. It resets when you relaunch the programme.
Opening the menu pauses play. Music can keep playing.

The computer uses iterative deepening and alpha-beta pruning. It examines
forced jump chains and extends captures at the search horizon. It values
pieces, kings, advancement, central squares and a back-row guard. Search
has both node and clock limits, retaining its last completed iteration.
Easy allows about 1 second, Normal 3 seconds and Hard 8 seconds. Simple
positions finish earlier. A single search node cannot be interrupted, so
these are approximate limits. This is a small opponent, not a master-level
engine. Search depth depends on the position and the machine's speed.

JOYSTICK / GAMEPAD
Choose Joystick in the menu. Centre the stick, then hold up-left, then
down-right, pressing and releasing button 1 at each step. Return to centre
and release the button after calibration. Keyboard and mouse remain active.
Calibration is for this run only; repeat after changing the CPU speed or
the timing dial on your gameport card. A disconnected stick times out.
Standard analog-compatible gamepads work. Proprietary digital protocols,
extra buttons and a second joystick are not used.

SOUND
Moves have a short click, captures a two-note effect, and kings/wins a
fanfare. Music uses separate AdLib channels and continues during drawing
and computer searches. Speaker sound is effects only. If AdLib is missing,
an AdLib sound request falls back to the PC speaker. Silent stays silent.
The playlist contains The Blue Danube (main theme), Fur Elise, Moonlight
Sonata (complete first movement), and Ode to Joy. The default starts with
Blue Danube; the last track and sound settings are saved. Tracks advance and
loop automatically; Shuffle picks a different next piece. Eight music voices
are available, with one separate voice for effects. No external files are
needed to play. These are FM renditions converted from Mutopia MIDI scores,
not recordings. Music sources, adaptations and licences: MUSIC/SOURCES.TXT.
The FX and FM instrument patch are original.

COMMAND LINE
  CHECK88 /A          AdLib sound
  CHECK88 /P          PC speaker effects
  CHECK88 /B          Both
  CHECK88 /S          Silent
  CHECK88 /NOMUSIC    Music off, keep effects
  CHECK88 /J          Calibrate joystick before the menu
  CHECK88 /NO87       Force integer perspective setup
  CHECK88 /?          Help

DRAWING AND COMPATIBILITY
The board, base, rings and crowns are projected at startup or on a camera
change, then cached. An 8087, when present, performs the perspective divisions; play does not continually
recalculate geometry. The search uses integers, which suit this task.
The VGA renderer uses our V20/80186 assembly line routines. XOR erasure
restores the underlying board and other objects, so a move redraws only
changed pieces and highlights. Text updates only changed characters.
Idle play and menus skip status formatting as well as drawing. Cursor moves
update highlights without rebuilding the piece counts. Help, confirmations
and joystick calibration sleep between input polls while IRQ music continues.
Menus, help and view changes are deliberate full-screen redraws.
There is no continuous full-screen animation or full-frame copy.
The board has a narrow top frame, beveled edges and shaded side faces.
Hidden side faces are skipped as you rotate it. These static details are
painted only when showing or rotating the board, never during normal moves.
Masked assembly spans fill the base a byte at a time; VGA write mode 3
paints its rim edges without disturbing neighbouring pixels. Piece shapes
and colours are unchanged. Existing CHECK88.CFG settings still work.

The title's '88' is the name, not an 8088 compatibility claim. The target
is the user's NuXT V20 with VGA and optional 8087/AdLib. DOSBox-X was used
for development checks. Real NuXT joystick timing, audio balance and AI
thinking times still need a physical test. No 86Box tests were run.

1.2.1 REVIEW UPDATE
Keyboard and mouse input from the same poll now perform only one action,
so selecting a piece cannot accidentally confirm its move at the same time.
Undo correctly shows NONE before a Blue player's first turn. Track changes
update their pending request atomically with respect to the music interrupt.
Shared precomputed ring offsets cut repeated setup math when rotating the
board. Board and piece captures match 1.2 exactly. Settings remain compatible.

BUILD
Open Watcom 2 on Linux:
  WATCOM=/path/to/watcom sh BUILD.SH

Rules and integration tests:
  WATCOM=/path/to/watcom sh TEST.SH
  gcc -x c -std=c99 -O2 HOSTTEST.C -o /tmp/check88-test
  /tmp/check88-test
  gcc -x c -std=c99 -O2 SOUNDTEST.C -o /tmp/check88-sound
  /tmp/check88-sound

The preconverted MUSIC.H is included. To regenerate it from the supplied
MIDI sources, run python3 MUSIC/CONVERT.PY before compiling.

TEST.SH uses DOSBox-X in an isolated temporary directory. TEST88.EXE is a
development build and is not needed to play. The release contains no test
or autoplay switch. Code and original assets: MIT licence, LICENSE.TXT. Third-party music has
separate licences listed in MUSIC/SOURCES.TXT.
See CREDITS.TXT for research and reused code, and OWLIC.TXT for runtime terms.
