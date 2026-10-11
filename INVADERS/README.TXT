XT INVADERS 2.0
===============
A little Space Invaders-style game for the NuXT and other DOS PCs.
One COM file, EGA/VGA graphics, gameport controls and optional AdLib sound.

RUN
---
    INVADERS          320x200, 16 colours (EGA or VGA)
    INVADERS /V       640x480, 16 colours (VGA)
    INVADERS /V /J    VGA with joystick/gamepad calibration
    INVADERS /E /J    EGA with joystick/gamepad calibration
    INVADERS /S       Silent; skips all AdLib probing and writes
    INVADERS /?       Command-line help

Flags can be combined. /E is the default. VGA mode uses crisp doubled
sprites in a centred 640x400 playfield with 40 black scanlines above and
below. Gameplay and movement speed are the same in both modes.
Only INVADERS.COM is needed on the DOS machine. No assets, drivers or
BLASTER environment variable are required. AdLib is detected at port 388h;
without a compatible card the game runs silently.

CONTROLS
--------
Left / Right arrows  Move (keypad 4/6 also work)
Space                Fire; hold for repeat fire. Start/restart at prompts
Enter                Start/restart at prompts
P                    Pause / resume
M                    Toggle sound, if AdLib was detected
J                    Calibrate gameport at the title or game-over screen
Esc                  Quit; during calibration, cancel back to the title

Joystick / gamepad   Left/right on joystick 1's X axis
Button 1             Fire, start/restart, accept calibration position
Button 2             Pause / resume

Keyboard controls remain available when a joystick is enabled.

GAMEPORT SETUP
--------------
Use /J or press J at the title. Release the stick or D-pad to the centre,
then press Enter or button 1. Repeat with full left, then full right.
Release the confirmation button between steps. Calibration lasts for this
session. Escape cancels it. If calibration fails, keyboard play still works.

Supports the standard PC gameport at 201h and analog-compatible DOS
joysticks/gamepads, including pads that present their D-pad as X/Y axes.
It does not implement proprietary digital SideWinder/GrIP protocols or USB
host drivers. An adapter that emulates a standard PC gameport may work.
No gameport is accessed until /J or J enables it.

Only X is timed; unused Y and joystick-2 axes cannot stall the game.
Measurements use the PIT clock, with a 2 ms timeout and an extra loop limit.
Calibration uses median samples and a centre dead zone. Repeated missing
axis readings clear the controls and disable polling. Recalibrate from the
title/game-over screen after reconnecting. Real card/controller timing
still needs testing on the NuXT; emulator results are in TESTS.TXT.

WHAT CHANGED IN 2.0
-------------------
- A 36.4 Hz player/laser update, twice the previous input update rate.
- Keyboard presses are latched by IRQ1, so a quick tap between updates is
  retained. Holding fire reuses a free shot slot in the same update.
- A short fire-request buffer (about 165 ms) makes a tap just before the
  previous shot disappears more forgiving. Still only one shot at a time.
- Standard DOS joystick/gamepad support, calibration and button controls.
- VGA 640x480 / 16-colour mode alongside the original EGA mode.
- A four-note descending AdLib march, tied to alien movement. It accelerates
  as the fleet shrinks. These are newly synthesized FM thuds, not samples.
- Six original static wireframe backdrops: mountains, city, desert base,
  lunar outpost, spaceport and crystal valley. They cycle every six waves.
- Faster alien position/collision lookup and smaller score redraws.
- Stationary ships are no longer erased and redrawn unnecessarily.
- Each marching alien is erased and replaced individually, reducing fleet
  blanking. XOR sprites restore scenery behind them without a screen copy.

HOW TO PLAY
-----------
Clear all 50 aliens before they reach your ship's line. The formation
reverses and descends at the edges, and speeds up as fewer aliens remain.
Later waves increase the pressure.

Top scouts: 30 points; middle raiders: 20; bottom drones: 10.
The bonus saucer is worth 100. Clearing a wave earns 100 more and restores
one life, up to five. You start with three lives. Four destructible shields
are rebuilt each wave; your own shots also damage them. A blinking ship is
briefly protected after respawning. Up to three enemy bolts can be active.

BEST lasts for this session only. No save files are written. Score caps at
65535 and the wave counter at 99. Scenery is cosmetic, with no collisions.

HARDWARE AND TIMING
-------------------
CPU: 8088/8086, NEC V20 or later; assembled with .arch i8086.
DOS: 2.0+ API target; tested in FreeDOS 1.3 and DOSBox.
Video: EGA/VGA mode 0Dh, or VGA mode 12h for /V.
Sound: optional AdLib/OPL2-compatible hardware at 388h/389h.
Controller: optional standard gameport at 201h.
Memory: about 13 KiB for code/data/stack/PSP, plus environment/DOS overhead.

Rendering stays in assembly and only updates changed sprites and fields.
The VGA renderer expands bytes with a lookup table. The background uses
integer line drawing once at the start of each wave. It does not use the
8087: floating-point setup would not improve ongoing animation speed.

The PIT is temporarily set to about 36.4 Hz. Every second interrupt is
passed to the previous IRQ0 handler, preserving normal BIOS timekeeping.
Alien movement, enemy firing, respawn and wave delays retain their original
real-time cadence. Short delays can catch up by up to four updates; larger
stalls drop excess simulation time rather than running an unbounded backlog.

The program assumes the normal DOS BIOS PIT rate on entry. Use it from DOS,
not alongside another application that owns/reprograms the timer. Original
IRQ0/IRQ1 vectors, standard BIOS PIT timing, video mode and video page are
restored on normal exit. The previous screen contents are not saved.

IRQ1 tracks key-down/key-up directly. BIOS keyboard shortcuts, including
NuXT CPU-speed hotkeys, are not processed while the game owns the keyboard.
Choose the CPU speed before launch. AdLib voices stop on pause, mute and exit.
OPL2 writes keep the conservative six/35 status-read hardware delays.

FILES AND BUILDING
------------------
INVADERS.COM   Standalone game
GAME.S         GNU assembler source, Intel syntax
ART.INC        Original sprite and font bytes
WIRE.INC       Original wireframe line coordinates
COM.LD         Linker layout for COM loading at 0100h
BUILD.SH       Linux build script (GNU as, ld, objcopy)
README.TXT     This guide
TESTS.TXT      Verification and remaining hardware-test limits

Build: sh BUILD.SH
No external libraries, game ROMs, recorded sounds or downloaded art are used.
The intermediate ELF RWX warning is expected for a single-segment DOS COM.

TECHNICAL REFERENCES
--------------------
id Software's published ID_IN.C and ID_SD.C: gameport and OPL programming.
https://github.com/id-Software/wolf3d/blob/master/WOLFSRC/ID_IN.C
https://github.com/id-Software/wolf3d/blob/master/WOLFSRC/ID_SD.C

Original arcade fleet-sound sequencing, reviewed as a behaviour reference:
https://computerarcheology.com/Arcade/SpaceInvaders/Code.html

FreeDOS archive INFOPlus, PAGE_07.PAS: bounded gameport presence polling.
https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/util/system/infoplus/

IBM BIOS video modes, reproduced by PCjs:
https://www.pcjs.org/documents/books/mspl13/msdos/encyclopedia/appendix-o/

These were references, not copied source dependencies. Code, graphics and
FM effect definitions were authored for this project. No original Space
Invaders ROM, graphics or audio recordings are included.
