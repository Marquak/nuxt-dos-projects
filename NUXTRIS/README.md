# ИuXTяis 1.2

Fit the falling blocks together and clear rows. Play an endless game or try
to clear 25 lines, then save your score with your name. Coloured outline blocks,
a next-piece preview, and a short fanfare when you get a Tetris.

Copy **NUXTRIS.EXE** to a writable DOS folder and run `NUXTRIS`. Only the EXE is
needed. Keep your NUXTRIS.DAT and NUXTRIS.BAK when updating; those hold your
scores and settings.

- **Type A:** endless play, with speed increasing every ten lines.
- **Type B:** clear 25 lines at your chosen speed and starting height.
- Separate named score tables, white/red/green/amber interface themes, and
  AdLib, PC speaker, both or silent sound.
- Keyboard and standard analog gameport controls. `/J` calibrates at launch.

Left/right moves, Down soft-drops, Up/X/Space rotates clockwise, and Z rotates
the other way. P or Esc pauses. F1 changes the theme and F2 changes the sound.
The score display updates when a piece lands, avoiding extra redraws during
a soft drop.

Version 1.2 fixes Backspace after an eight-character score name. Menus and
name entry now redraw only changed text and cursor pixels. Gameplay, sound
and the save format are unchanged. All 129 checks passed in DOSBox-X; this
update still needs its physical NuXT check.

V20/V30 or an 80186-compatible CPU, DOS 3.3+, and VGA 640x480 in 16 colours.
The 8087 isn't needed. This is the completed single-player version; serial and
LAN multiplayer aren't implemented. Earlier builds have been played on the NuXT.

[README.TXT](README.TXT) has the rules, controls, saves and technical details.
[TESTS.TXT](TESTS.TXT) separates current checks from earlier test runs.
Build with `WATCOM=/path/to/watcom sh BUILD.SH`; add `--test` for CHECK.EXE.

Rules were adapted from **olzhasar/sdl-tetris**, with its MIT notice preserved
in [UPSTREAM.TXT](UPSTREAM.TXT). [CREDITS.TXT](CREDITS.TXT) records the exact
revision and changes. No SDL dependency or commercial game assets are included.
Project code/assets use [MIT](LICENSE.TXT); the Open Watcom runtime retains
its own terms in [OWLIC.TXT](OWLIC.TXT).
