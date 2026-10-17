# 3D Checkers 88

Play checkers against the computer or a friend on the same PC. Turn the board
to a view you like, choose a colour theme and put on some classical AdLib music.
It follows the usual forced-capture and king rules, with undo and a choice of
AI difficulty.

![The checkers board](DEPTH.PNG)

## Run

Copy `CHECK88.EXE` to a writable DOS folder and run `CHECK88`. It saves your
settings in `CHECK88.CFG`. The music is built in; no separate player is needed.

Runs on DOS 3.3+ with a V20/V30 or 80186-compatible CPU and VGA 640x480 in
16 colours. Use a keyboard, DOS mouse or gameport joystick. The game also
supports an 8087 for board projection and AdLib music and effects, with separate
volume controls.

Use WASD to rotate and tilt the board, R to restore the default view, and F1
for help. A piece with only one legal destination highlights it, then waits
for confirmation. The full controls and settings are in [README.TXT](README.TXT).

## Source and credits

Version 1.2.1. Source and an Open Watcom build script are included; see the
collection's [build instructions](../BUILDING.md) and [test notes](TESTING.TXT).

Game code is MIT licensed, with the Open Watcom runtime covered by
[OWLIC.TXT](OWLIC.TXT). The classical music arrangements retain their original
licenses, including their converted data in the executable. See
[MUSIC/SOURCES.TXT](MUSIC/SOURCES.TXT) for credits, licenses and source files,
and [CREDITS.TXT](CREDITS.TXT) for programming references.
