MATRIX - NuXT digital rain, version 2.0

Run MATRIX.EXE. It is self-contained and silent.
Press ENTER twice for the main target: 640 x 480, 16 colours.

Launch menu:
  Resolution: 1 = 640 x 480, 2 = 800 x 600, 3 = 1024 x 768
  Colours:    1 = 16,      2 = 256

During rain:
  Space       Pause/resume (including the glow animation).
  G           Toggle the gentle green glow.
  Esc or Q    Return to the menu.
At the resolution menu: Esc or Q exits to DOS.

OPTIONS
  MATRIX              Detect an 8087/x87; use it when available.
  MATRIX /NO87        Skip detection and use integer maths only.
  MATRIX /NOGLOW      Start with the palette glow switched off.
  MATRIX /?           Show command help.
Options can be combined. The menu reports the selected maths path.

Green ASCII characters, bright leading glyphs, fading trails,
independent column speeds and occasional changing characters.
No sound hardware or timer interrupt hooks are used.

OPTIMISATION AND THE 8087
Version 2 draws each planar glyph with eight VGA writes, down from 32.
Changes are batched by shade and screen row; repeated changes to the
same cell are combined. Precomputed row addresses and small assembly
blitters remove per-pixel address calculations. Packed-colour modes
use an expansion lookup table and word writes.

Fixed-speed DOSBox drawing benchmarks improved about 10.4x in 16 colours
and 11.2x in 256 colours, with matching complete video-memory checksums.
These measure the drawing routine, not actual NuXT frame rates.
Animation still advances at up to 18.2 updates per second; optimisation
helps a slow machine keep up with that pacing.

An optional 8087 calculates a 64-entry glow curve using square roots
once at startup. During animation, palette updates use that table and
integer arithmetic. The CPU fallback computes exactly the same curve.
This is a small visual use of the 8087, not the source of the drawing
speedup. The program does not require a coprocessor.

Detection uses Intel's non-waiting 8087 memory-status probe. /NO87 is
available to bypass the probe entirely. The assembly uses original
8087 instructions, with no 287/387-only instructions such as FSIN.
The code and CPU fallback remain 8086/8088/NEC V20 compatible.

VIDEO SUPPORT
DOS and a VGA-compatible card are required. The executable's memory
image is about 50 KB including a 4 KB stack, plus DOS/runtime overhead.
640 x 480/16 uses ordinary VGA BIOS mode 12h and direct planar writes.
The other modes are optional experiments and will be slower.

Higher modes try banked VESA/VBE first, then the Trident TVGA9000
native BIOS modes when a Trident video ROM is detected. The native
fallback targets the NuXT's 512 KB card; no VESA TSR is required there.
On other cards, a compatible VESA BIOS/driver may be needed.
Unsupported selections return to the menu with an explanation.

The NuXT manual lists 640 x 480 and 800 x 600 in 16/256 colours, and
1024 x 768 in 16 colours with 512 KB VRAM. 1024 x 768 in 256 colours
needs at least 768 KB and is offered for testing other VGA cards.
The BIOS determines the refresh rate of the higher modes.

BUILD AND TEST
Build with Open Watcom v2:
  WATCOM=/path/to/watcom sh BUILD.SH
The build targets 8086 instructions, small model and software floating
point. Optional 8087 code is isolated in FPU.C as explicit instructions.
All source and the original font from our DOS projects are included.

BUILDTEST.SH optionally builds three DOS test utilities:
  BENCH.EXE    Times 32,000 glyph draws in each 640 x 480 colour depth.
               Writes DRAW.TXT with ticks and video-memory checksums.
  CHECK.EXE    Checks all six modes; writes CHECK.TXT checksums.
  FPUTEST.EXE  Checks detection and matching glow tables; FPUTEST.TXT.
Tests need not be copied to the NuXT to use MATRIX.EXE.
See TESTING.md for results, expected checksums and validation limits.

No delivery disk image was created. Replace MATRIX.EXE in your usual
DOS directory. The program creates no save or configuration files.
