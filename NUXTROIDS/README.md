# NuXTroids 1.3

Turn, thrust and shoot your way through an Asteroids-style game on the NuXT.
The rocks break into smaller pieces as you hit them. Faded planets and stars
sit behind the action, with AdLib effects and a two-note heartbeat.

![Asteroids, the player ship and distant planets.](SCREENSHOT.PNG)

*Asteroids, the player ship and distant planets.*

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

Version 1.3 reduces the drawing work as rocks move and turn. It uses a larger
cache and falls back to the original line drawing if memory is tight. Later
wave performance still needs a check on the NuXT; emulator timings are in
[TESTS.TXT](TESTS.TXT).

[README.TXT](README.TXT) has all controls, video limits and build instructions.
[TESTS.TXT](TESTS.TXT) records the regression and performance checks.
Build with `WATCOM=/path/to/watcom sh BUILD.SH`; add `--test` for CHECK.EXE.

The game, artwork and effects are original project material under
[MIT](LICENSE.TXT). No Atari assets are included. The linked Open Watcom
runtime has separate terms in [OWLIC.TXT](OWLIC.TXT).
