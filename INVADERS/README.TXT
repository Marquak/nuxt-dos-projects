XT INVADERS 1.0
==============
An original Space Invaders-style DOS game for 8088/NEC V20 machines.
320x200, 16-color EGA graphics; also runs on VGA. Optional AdLib/OPL2 effects.

RUN
---
Copy INVADERS.COM to your DOS machine and type:

    INVADERS

The program detects an AdLib-compatible card at 388h. If none is found,
it runs silently. No sound driver, BLASTER variable, or other files needed.
To skip sound detection and all AdLib port accesses:

    INVADERS /S

Command-line help: INVADERS /?

CONTROLS
--------
Left / Right arrow   Move (keypad 4/6 also work)
Space                Fire; hold for repeated shots. Start/restart at prompts.
Enter                Start/restart at prompts
P                    Pause / resume
M                    Toggle sound, if AdLib was detected
Esc                  Quit to DOS

HOW TO PLAY
-----------
Clear all 50 aliens before their formation reaches your ship's line.
Aliens reverse at the sides and descend. Their march accelerates as fewer
remain. Later waves also increase the pressure.

Top row scouts: 30 points; middle raiders: 20; bottom drones: 10.
The red bonus saucer is worth 100 points. Clearing a wave earns 100 more
and restores one life, up to a maximum of five. You start with three lives.
The four shields are destructible, including by your own shots, and are
rebuilt for each new wave. Respawning briefly makes the ship invulnerable;
the blinking ship indicates this protection.

Only one player shot can be active. Up to three enemy bolts can be active.
BEST is kept for the current session only; the game writes no score files.
Score saturates at 65535, and the wave display caps at 99.

HARDWARE
--------
CPU: 8088/8086 or NEC V20 and later. All code is assembled with .arch i8086.
DOS: 2.0+ API target. Tested under FreeDOS 1.3.
Video: EGA or VGA supporting BIOS mode 0Dh (320x200, 16 colors).
Sound: optional original AdLib, faithful OPL2 reproduction, or compatible
       FM hardware at I/O ports 388h/389h. No PCM/Sound Blaster DSP required.
Memory: under 8 KiB for the program block, plus DOS/environment overhead.

Your intended target: NuXT with NEC V20, Trident TVGA9000i/512 KiB, and the
planned original-design AdLib reproduction. Physical NuXT timing and sound
have not been tested here; emulator results are recorded in TESTS.TXT.

The engine redraws only changed sprites and HUD elements using EGA planar
XOR writes, rather than copying a full framebuffer every frame. The pacing
uses the standard approximately 18.2 Hz BIOS clock. Faster PCs retain that
rate; slow PCs drop simulation time instead of doing long catch-up loops.
The PIT is not reprogrammed, so BIOS/DOS timekeeping continues normally.

The keyboard IRQ is temporarily captured for real key-down/key-up tracking
and simultaneous movement/fire. The old handler is restored on normal exit.
BIOS keyboard shortcuts (including NuXT CPU-speed hotkeys) are not processed
while the game owns the keyboard. Set CPU speed before launching the game.

The original video mode and page are restored on exit. Old screen contents
are not saved. AdLib voices are keyed off on mute, pause and exit.

SOUND DESIGN
------------
Five FM channels provide player laser, alien-hit, formation-march,
player-explosion, and bonus-saucer effects. Effects run asynchronously;
there are no long sound loops blocking movement.

AdLib detection uses the OPL timer status test. Register writes wait with
six status-port reads after address writes and 35 after data writes, as
required for original OPL2 hardware. Port I/O is skipped entirely with /S.

FILES AND BUILDING
------------------
INVADERS.COM   Standalone ready-to-run game
GAME.S         Commented GNU assembler source, Intel syntax
ART.INC        Original pixel sprites and 5x7 font, stored as source bytes
COM.LD         Linker layout for DOS COM loading at 0100h
BUILD.SH       Linux build script using GNU binutils
README.TXT     This guide
TESTS.TXT      Test results and remaining hardware-validation limits

Build on Linux with GNU binutils (as, ld, objcopy):

    sh BUILD.SH

No external libraries or downloaded art/sound assets are used. GNU binutils
2.47 was used for the supplied binary. This is GNU assembler syntax; MASM,
TASM or NASM would need source adaptation. The intermediate ELF's RWX
segment warning is expected for a single-segment real-mode DOS program.

TECHNICAL REFERENCES
--------------------
Microsoft, MS-DOS Encyclopedia, BIOS video mode table:
https://www.pcjs.org/documents/books/mspl13/msdos/encyclopedia/appendix-o/

AdLib access/detection reference: id Software's published ID_SD.C:
https://github.com/id-Software/wolf3d/blob/master/WOLFSRC/ID_SD.C

All game code, pixel art, font and synthesized effect definitions in this
package were written for this project. No original Space Invaders ROM,
graphics, audio recordings, or other game data is required or included.
