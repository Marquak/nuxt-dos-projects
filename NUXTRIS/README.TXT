NUXTRIS 1.2

Fixed Backspace after an eight-character high-score name. Menus and name
entry now redraw only changed text/cursor pixels. Game rules, timing,
sound and existing score/settings files are unchanged.

UPDATING: replace NUXTRIS.EXE. Keep your NUXTRIS.DAT and NUXTRIS.BAK.

A bit of old-school block dropping for the NuXT.

640x480, 16 colours. Coloured wireframe pieces, a next-piece window, and a
big fading TETRIS! with a short original fanfare when you clear four lines.
The on-screen logo uses the requested Cyrillic letters, drawn directly so
it works with an ordinary DOS code page. DOS filename: NUXTRIS.EXE.

RUN IT
  NUXTRIS
  NUXTRIS /J       Calibrate a joystick at launch
  NUXTRIS /A       AdLib effects
  NUXTRIS /P       PC speaker effects
  NUXTRIS /B       Both (first-run default)
  NUXTRIS /S       Silent
  NUXTRIS /?      Show command-line help

Only NUXTRIS.EXE is needed to play. Put it in a writable folder on your CF
card if you want to keep your scores. No installer, external art, music
files, mouse driver, UMB driver or sound driver required.

NEEDED
  NEC V20/V30 or 80186-compatible CPU; DOS 3.3 or newer.
  VGA with standard BIOS mode 12h, 640x480 in 16 colours.
  Optional original AdLib/OPL2-compatible card at port 388h.
  Optional standard analog PC gameport joystick/gamepad on port 201h.
  Target system: NuXT V20 around 10 MHz, DOS 6.22, Trident VGA.

This build uses 80186 instructions. It is not an 8088/8086 executable.
Board movement and scoring use integer cell coordinates. Video uses standard
VGA mode 12h.

TWO GAME TYPES
  A: Endless. Clear lines and keep going until the stack reaches the top.
     Speed increases every ten cleared lines, up to level 19.
  B: Clear 25 lines. Pick a starting speed and a starting height from 0-5.
     Each height adds two rows of scattered blocks. Speed stays at the
     chosen level. There is always at least one hole in each starting row.

The menu offers starting levels 0-9 for both types. Each type has its own
10-entry score table. Tables record an eight-character name, score, level,
height, and whether you completed Type B. Ranking is by score.

CONTROLS
  Left/right       Move; hold for delayed auto-repeat
  Down             Soft drop: descend faster while held, one cell at a time
  Up, X, Space     Rotate clockwise
  Z                Rotate anticlockwise
  Enter            Start from menu; also rotates clockwise during play
  P or Esc         Pause during play
  Enter/P/Esc      Resume from pause
  Q                Return to menu while paused; abandon this round
  F1               Cycle white, red, green and amber frame/text colours
  F2               Cycle silent, AdLib, speaker and both
  J                Calibrate joystick; game is suspended while calibrating
  K                Disable joystick; keyboard stays active
  H                High scores, from the main menu
  Esc              Exit, from the main menu

In the menu, up/down selects an option and left/right changes it.
The theme colours change only the frame and interface. Piece colours stay
fixed. Pause hides the middle of the board with a small resume/menu prompt.

JOYSTICK/GAMEPAD
  Calibrate with the stick centred, then up-left, then down-right. Press
  button 1 at each position, releasing it between steps. Return the stick
  to centre and release the button after the last step.
  Left/right = move, down = soft drop, up = clockwise rotation.
  Button 1 = clockwise rotation; button 2 = anticlockwise rotation.
  Button 1 starts/confirms in menus. Button 2 goes back in menus, or ends
  the round when paused. Use P/Esc on the keyboard to pause.
  Names can be entered with the keyboard, or left/right to position the
  cursor and up/down to select a character; button 1 saves.

Works with gamepads that present standard joystick axes/buttons. USB pads
plugged into a modern PC do not directly work on a DOS gameport. Proprietary
SideWinder/Gravis digital protocols and four-button protocols are not
implemented. Calibration is per launch. Missing/disconnected hardware has a
bounded timeout and the keyboard remains usable.

RULES AND SCORE
  A 10x20 well, seven pieces, one next-piece preview, no hold or hard drop.
  A shuffled seven-piece bag prevents very long piece droughts.
  Rotation is about a simple pivot, with no modern wall kicks or lock delay.
  A failed downward move locks the piece. Moving sideways or rotating does
  not reset gravity. Down never jumps straight to the bottom.
  One/two/three/four lines: 40/100/300/1200 points times (level + 1).
  Each successful soft-drop cell adds one point.
  Soft-drop points appear on the score display when the piece locks.
  Line-clear bonuses appear when the clear finishes. Scoring itself and
  saved totals are unchanged; only the display update is deferred.
  Four-line celebrations suspend dropping for about 1.5 seconds, then play
  continues automatically. Other clears use a short row flash.

Inspired by Electronika 60 Tetris, with the requested modern conveniences
and A/B challenge modes. This is our version, not an exact historical rules
recreation. No commercial graphics, music or game executable is included.

SCORES AND SETTINGS
  NUXTRIS.DAT stores both tables, last name, theme, sound and game options.
  NUXTRIS.BAK keeps the previous valid save. NUXTRIS.NEW is a temporary file
  used while saving. Files live beside the executable on normal DOS 3.3+
  launches, including when you start it from another current directory.
  Files have a version tag, CRC and range checks. A corrupt/truncated DAT
  can recover from BAK. A save failure is reported on screen; previous
  valid files are preserved where possible. Do not include your DAT/BAK
  when sharing the game unless you also want to share your scores.
  Scores are saved after name entry; menu settings are saved on clean exit.
  Eight-character names use A-Z, 0-9 and spaces. Enter accepts the name.
  Backspace removes the preceding letter, including after a full name.
  Extra typing at the end of a full name is ignored; use arrows to edit.
  Esc from name entry accepts the current name too. Abandoned rounds do
  not qualify for the table.

SOUND
  Landing: short blip. Line clear: rising notes. Four lines: longer fanfare
  with AdLib harmony and an optional PC speaker melody.
  There is no continuous background music. If AdLib is absent, /A falls
  back to speaker; Both still plays its speaker part. Sound timing uses
  the game clock and does not block keyboard input.

UNDER THE HOOD
  Rule code adapted from the MIT-licensed olzhasar/sdl-tetris project.
  See CREDITS.TXT and UPSTREAM.TXT for the exact upstream revision/licence.
  Direct planar VGA drawing with V20/80186 assembly lines and masked clears.
  Ordinary movement erases/draws only changed cells. While a piece is being
  lowered, soft-drop points do not reformat or redraw the score panel.
  Landing updates only changed glyph pixels with batched VGA byte writes.
  Other counters use the same glyph updates and bounded decimal conversion.
  Menu selections update only the arrow and changed labels. Name editing
  updates only changed letters and the underline cursor. Screen changes
  and returning from calibration rebuild the appropriate screen.
  Palette-only theme changes and celebration fades avoid repainting.
  Pause and entering/leaving the celebration still rebuild the scene.
  Fixed 36.4 Hz game timing, with the BIOS timer chained at its normal rate.
  On exit the keyboard/timer handlers and previous video mode are restored.

TESTED
  See TESTS.TXT. Version 1.2: focused DOSBox-X regression checks. Earlier
  builds were tested in DOSBox/QEMU and on my physical NuXT.
  These new changes still need a physical check; emulator cycles are not
  a V20 performance measurement.

BUILD FROM SOURCE
  Open Watcom 2, Linux host:
    WATCOM=/path/to/watcom sh BUILD.SH
  Developer regression executable:
    WATCOM=/path/to/watcom sh BUILD.SH --test
  Run CHECK.EXE in a disposable directory. It makes TSTSAVE.* test files.
  CHECK /SHOW also runs an automated visual/sound/score-entry demonstration.
  The release EXE has no test/showcase switch.

Source and game-specific assets are MIT licensed; see LICENSE.TXT and
UPSTREAM.TXT. Open Watcom runtime terms are in OWLIC.TXT.
