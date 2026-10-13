# NuXTroids 1.3

An Asteroids-style game for the NuXT. Turn, thrust and shoot the rocks into
smaller pieces while trying not to hit them. Faded planets and stars sit behind
the action, with AdLib effects and a two-note heartbeat.

Copy **NUXTROID.EXE** to DOS and run `NUXTROID`. That's the only runtime file.
The default is 640x480 with 16 greyscale shades. `/L` selects 320x200, `/8`
selects 800x600 on compatible hardware, and `/M` makes the objects monochrome.
Use `/J` for gameport calibration or `/S` for silence.

Arrow keys turn, thrust and brake; Space starts and fires. P pauses, M mutes,
J calibrates, K disables the joystick, and Esc returns to DOS. Keyboard
controls remain available with a joystick. The best score lasts for the session.

Built for V20/80186-compatible instructions, VGA and an optional original 8087.
The 8087 prepares rotated outlines at startup; integer tables handle gameplay.
Without it, the game builds the tables with integer math. AdLib is optional.

Version 1.3 prepares aligned asteroid masks at startup and combines the old
and new outlines before drawing. Unchanged pixels cancel out, reducing VGA
writes as rocks move, rotate and cross screen edges. Rules and sound are unchanged.
The larger cache uses more RAM; the line renderer remains a fallback if allocation
fails. Later-wave performance still needs checking on the NuXT.
Emulator timings are documented separately and aren't physical V20 benchmarks.

[README.TXT](README.TXT) has all controls, video limits and build instructions.
[TESTS.TXT](TESTS.TXT) records the regression and performance checks.
Build with `WATCOM=/path/to/watcom sh BUILD.SH`; add `--test` for CHECK.EXE.

The game, artwork and effects are original project material under
[MIT](LICENSE.TXT). No Atari assets are included. The linked Open Watcom
runtime has separate terms in [OWLIC.TXT](OWLIC.TXT).
