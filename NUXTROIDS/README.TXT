NuXTroids 1.3
=============
A little wireframe asteroid shooter for the NuXT. Turn, thrust, coast, and
try not to fly straight into the rock you just split in half.

Only NUXTROID.EXE is needed to play. There are no external graphics or sound
files. The rest of this folder is the readme, source, build and test notes.

STARTING IT
-----------
NUXTROID          640x480, 16 greyscale shades, AdLib if detected
NUXTROID /J       Same, with joystick calibration before the title
NUXTROID /8 /J    800x600, with joystick calibration
NUXTROID /L       320x200 for extra speed
NUXTROID /V       Explicitly select 640x480
NUXTROID /M       Bright monochrome objects over the dim grey background
NUXTROID /S       Silent; does not probe or write to the AdLib
NUXTROID /N       Integer geometry; does not probe or use the 8087
NUXTROID /?       Show options

Options can be combined. All modes use planar 16-colour graphics.
640x480 is the default. 800x600 is optional and has a larger playfield.
It needs compatible VBE support or the NuXT's Trident BIOS mode 5Bh.
Use /L if the larger modes feel slow on your machine.

CONTROLS
--------
Left / Right, or A / D    Rotate
Up, or W                 Thrust
Down, or S               Brake to a stop
Space                    Start, fire, or start again after game over
P                        Pause / resume
M                        Mute / unmute the detected AdLib
J                        Calibrate or recalibrate the joystick
K                        Disable joystick input
Escape                   Quit to DOS; cancel while calibrating

Hold Space for continuous fire. Brief taps are latched and queued for a
short time if the gun is cooling down. There are six shot slots.
/S disables AdLib hardware for the whole run, so M cannot enable it later.
Keyboard controls stay available when a joystick is in use.

JOYSTICK / GAMEPAD
-----------------
Standard PC gameport joystick 1, at port 201h. Analog-compatible gamepads
work too. This does not implement proprietary digital controller protocols.

Use /J at launch, or press J in the game. Follow the three prompts:
1. Centre the stick and press button 1.
2. Hold fully up-left and press button 1.
3. Hold fully down-right and press button 1.
Release the button between steps. Calibration lasts for this session.

Left / right rotates. Up or button 2 thrusts. Down brakes. Button 1 starts
the game and fires. The centre has a dead zone. Reads have a time limit;
a disconnected or unresponsive controller falls back to keyboard control.
An AdLib card does not supply a gameport. A separate gameport is fine.

PLAYING
-------
Three ships to start. Large rocks split into two medium rocks; medium rocks
split into two small ones. Scores are 20, 50 and 100 respectively. Clear the
field to advance. Later waves add rocks and gradually increase their speed.

You get another ship every 5000 points, up to five in reserve. A new ship
blinks while temporarily protected. Both shots and ships wrap at the edges.
The best score lasts until you leave the program; no files are written.

The faded planets and stars change position between waves. They are scenery
and do not collide with anything. The AdLib supplies laser, impact, thrust,
explosion and two-tone heartbeat effects. There is no PC-speaker soundtrack.

HARDWARE AND SPEED
------------------
Target: NEC V20, optional original Intel 8087, VGA and optional AdLib at 388h.
This is not an 8088 build. It uses V20-compatible 80186 instructions.
No 286 protected mode, 386 instructions, XMS or EMS are required.

The 8087 calculates the rotated ship and asteroid outlines once at startup.
Without it, integer calculations build equivalent tables. Gameplay then uses
cached vertices and fixed-point motion; putting floating-point calculations
in the pixel loop would cost more than it saves here.

Version 1.3 prepares horizontally aligned asteroid masks at startup. It
combines overlapping source fragments and, for interior movement, merges
the old and new outlines in RAM before writing only their pixel difference.
Unchanged bits cancel without accessing VGA. Screen-edge asteroids now use
a wrapped cache blitter instead of redrawing all their individual lines.
Ship controls, collisions, firing, rotation and sound timing are unchanged.

The cache now uses about 291 KiB at 640x480/800x600, plus tables and heap
bookkeeping. Startup takes longer. If a shape cannot be allocated it uses
the existing line renderer, so low free conventional RAM can reduce speed.
The 320x200 mode prepares eight alignments; larger modes need only four.
There is still no full-screen copy. The sparse RAM merge still reads VGA
latches for each changed byte, but avoids redundant erase/draw accesses.

The line renderer and gameport reader are assembly. Moving outlines are
updated individually with XOR drawing, preserving the static background.
There is no full-screen copy each frame. This is not double buffering;
overlapping outlines can briefly cancel where they share exactly the same
pixels. Score updates change only the affected glyph pixels, using batched
VGA byte writes. Unchanged labels and digits stay on screen. Decimal score
formatting avoids general printf conversion and software 32-bit division.
This removes unnecessary whole-line redraw work on every asteroid hit.

Physics uses a 36.4 Hz timer. Rendering can skip intermediate frames when
needed. Catch-up is bounded so an overloaded machine stays responsive.
The exit report lists frames, simulation ticks, and dropped catch-up ticks.
These are diagnostics, not a measured average frame rate.

DOSBox tests looked playable at 640x480 and 800x600, but the NuXT's ISA VGA
bus is the real test. The Trident path reuses the mode setup from our other
NuXT demos. The user has confirmed the game runs on the NuXT. The user also confirmed the 1.1 score-update hitch is fixed. The owner reported lag in later waves in 1.2; version 1.3 needs a new
physical NuXT playtest.
No 86Box testing was used.

The program expects normal BIOS timer timing on entry and restores that
rate, the original keyboard/timer interrupt vectors, and the original BIOS
video mode/page on exit. It is a foreground DOS game, not a TSR.

BUILDING
--------
Open Watcom C/C++ 2.0 with its DOS target libraries and WASM assembler:

  WATCOM=/path/to/watcom sh BUILD.SH

Output: NUXTROID.EXE. The linker also produces NUXTROID.MAP.
The game is C plus two original assembly modules; no extra libraries beyond
the compiler's DOS C runtime are needed. FPU.ASM targets the original 8087.

Optional DOS checks:
  WATCOM=/path/to/watcom sh BUILD.SH --test
  CHECK 0       320x200
  CHECK 1       640x480
  CHECK 2       800x600
  CHECK 1 PLAY  Controlled 640x480 workload timing; writes PERFRUN.TXT

The test build writes CHECKS.TXT. Its FPU check expects an emulated FPU to be
present. CHECK.EXE is not needed by the game. See TESTS.TXT for coverage.

SOURCE REFERENCES
-----------------
Existing Matrix, Starfield, Orbit87 and Invaders code in this project was
used for the video, 8087, timing, input and AdLib groundwork. The ship,
asteroids, planet outlines and effects are original. No Atari assets.

Quick comparison/reference checks included:
https://github.com/lclarkmichalek/asteroids
https://github.com/EyeOfMidas/canvas-asteroids
https://fd.lod.bz/rbil/
FreeDOS INFOPlus PAGE_07.PAS (bounded gameport detection reference).

Those are references, not bundled dependencies or copied game code.
Original source is under the included MIT licence. The executable also
contains unmodified Open Watcom runtime code under OWLIC.TXT, not MIT.
Use of the covered Watcom portions constitutes acceptance of that licence.
Upstream runtime source: https://github.com/open-watcom/open-watcom-v2

AI-written, with emulator checks;
actual NuXT playtesting is still the useful final step.

ADDITIONAL RENDERING REFERENCES (1.3)
-----------------------------------
Michael Abrash, Graphics Programming Black Book, chapters 26 and 45/46:
https://www.phatcode.net/res/224/files/html/ch45/45-03.html
Little Game Engine sprite code:
https://github.com/mills32/Little-Game-Engine-for-VGA-EGA/blob/master/lt_spr.c
These informed the review. No external source was copied into this update.
