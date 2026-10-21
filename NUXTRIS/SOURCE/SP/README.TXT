NUXTRIS 1.3

A single-player block-dropping game for DOS on the NuXT. Play in VGA
640x480 with a keyboard or a two-button analog gamepad. The large fading
TETRIS! celebration and sound from the earlier release are still here.

Run NUXTRIS.EXE on DOS and choose a mode, starting level, and optional
B-Type pile height in the menu. On Linux, ./PLAY.SH opens the game in
DOSBox-X and keeps emulator scores under DOSBOX. Each new game draws a
fresh seven-piece order.

MARATHON   Keep going until you top out. Speed rises every 10 cleared lines
           and stops rising at level 19.
SPRINT 40  Clear 40 lines as quickly as you can. Speed stays at the level
           you chose. A top-out ends the attempt without a best time.
B-TYPE     Start with a random pile at height 0 through 5. Speed rises one
           level every 25 lines, up to level 9. Keep going until top-out.

This B-Type is a custom endurance mode. The original Game Boy B-Type ends
after 25 lines. The menu lets you choose starting level 0 through 9 for all
three modes. The time display counts active play, excluding pauses and the
TETRIS! celebration. Sprint best times are ranked by the fastest finish;
Marathon and B-Type high scores are ranked by points.

CONTROLS

Left/Right or A/D move. Down or S soft-drops. Up, W, X, Space, or Enter
rotates clockwise; Z rotates the other way. P or Esc pauses, and Q returns
to the menu while paused. F1 changes the theme, F2 changes sound, J
calibrates a gamepad, and K returns to the keyboard. In play, gamepad
left/right move, down soft-drops, and its two buttons rotate; gamepad up
is unused. The original single-player TETRIS! animation and fanfare have
not been shortened or replaced.

Use H on the menu to view records. Left/Right cycles through Marathon,
Sprint, B-Type, and Old Type B tables. That last table preserves scores
from the previous 25-line B game, separate from the new endurance mode.
Names may be up to eight letters or digits.

SCORES AND UPDATING

Replace NUXTRIS.EXE while the game is closed. Keep NUXTRIS.DAT and
NUXTRIS.BAK in the same folder as before. Version 1 scores and preferences
are imported automatically: old Type A scores become Marathon scores,
and old Type B scores appear in the Old Type B table. If Type B was selected,
the menu selects the new B-Type and keeps your level and height. The first
successful save writes the new format and keeps the prior valid file as
NUXTRIS.BAK. If the primary file is corrupt, the game tries the backup.

The game keeps ten records per table. Marathon and B-Type save a score on
top-out if it reaches the table. Sprint saves a best time only after all 40
lines are cleared. The result screen still shows the score if a Sprint
attempt ends early.

REQUIREMENTS AND BUILD

NEC V20/V30 or 80186-compatible CPU, DOS 3.3 or later, and VGA mode 12h.
An AdLib-compatible card and standard analog gameport are optional. Run
NUXTRIS /? for sound and calibration switches. The game uses integer
coordinates and its own VGA, sound and input routines.

Open Watcom 2 builds the game with WATCOM=/path/to/watcom sh BUILD.SH.
WATCOM=/path/to/watcom sh BUILD.SH --test makes CHECK.EXE. Run that only
in a disposable DOS directory; it creates test files and screen captures.

Coded with GPT-6 Codex, with design direction and NuXT testing by Marquak.
The game code is MIT licensed. CORE.H adapts olzhasar/sdl-tetris rule code;
see CREDITS.TXT, UPSTREAM.TXT, LICENSE.TXT, and OWLIC.TXT.

Version 1.3 passed 144 focused checks in DOSBox-X, including old-save
migration, mode goals and level changes, score ranking, screen drawing and
the original TETRIS! cleanup. Physical NuXT playtesting of this exact build
is still pending. The previous 1.2 release is preserved separately.
