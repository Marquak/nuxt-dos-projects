STARFIELD 1.0 - NuXT star flight

Copy STAR.EXE to DOS and run STAR. No other files are needed.
Silent, endless blue-white stars flying outward from the centre of a
black screen. Stars brighten and grow as they approach. No onscreen text.
Targets the NEC V20 / 80186 or later, VGA, DOS 3.0 or later.

QUICK START
  STAR                         Launch menu; ENTER starts 640x480/16.
  STAR /AUTO                   Start that default immediately.
  STAR /320 /256 /AUTO          Low-resolution VGA option.
  STAR /800 /16 /AUTO           Higher resolution, 16 colours.
  STAR /1024 /16 /AUTO          Highest NuXT resolution.
  STAR /AUTO /STARS:256         Denser starfield.
  STAR /AUTO /NO87              Bypass the coprocessor entirely.

MENU
  1 = 320x200       2 = 640x480
  3 = 800x600       4 = 1024x768
  C = toggle 16/256 colours
  D = cycle density (32, 64, 128, 256 stars)
  ENTER starts; ESC or Q exits to DOS.

IN FLIGHT
  SPACE             Pause/resume.
  + or =            Faster travel.
  -                 Slower travel.
  ESC or Q          Return to menu; with /AUTO, exit to DOS.
Speed changes do not change the star count or video mode.

OPTIONS
  /320 /640 /800 /1024   Resolution (choose one).
  /16 /256              Actual video colour depth (choose one).
  /AUTO                 Skip the menu; exit directly to DOS.
  /NO87                 No 8087 detection or floating-point instructions.
  /STARS:n              16..256; default 128.
  /SPEED:n              2..64; default 12 depth units per BIOS tick.
  /FRAMES:n             Stop after 1..65000 updates, for testing.
  /REPORT               Write STAR.LOG after flight (overwrites old log).
  /?                    Command help.
Ordinary runs do not create files or change DOS configuration.

MATRIX BASE / SPEED
Uses the VGA, banked VBE and native TVGA9000 backend from our MATRIX demo.
The renderer is new: only 8-pixel spans touched by old/new star positions
are recomposed. There is no whole-screen erase or background decoding.
Overlapping stars use the brightest star and do not erase each other.
Planar writes are grouped into four plane passes; packed VGA copies use
unrolled assembly word writes. Signed projection multiplies use a small
assembly kernel. Tables cache scanline addresses and perspective factors.

This is sparse compositing, not hardware page flipping. Changes are not
an atomic screen swap. A black background and tiny updates minimize the
visible drawing interval. Animation is paced at up to 18.2 updates/sec;
actual hardware speed is unmeasured until tested on the NuXT.

8087
An automatically detected 8087 builds both perspective lookup tables at
startup. These contain focal length / depth, pre-scaled for fast integer
projection. The integer fallback gives identical table values.
Animation itself uses integer assembly: repeatedly making the 8087 divide
for every star would add unnecessary work. Having an 8087 therefore does
not increase steady-state FPS compared with /NO87.
Uses original 8087 instructions only, saves/restores the control word and
leaves its numerical stack empty. Detection initializes the FPU. No sound
hardware, timer interrupt handler or DOS resident component is installed.
The CPU build uses V20-compatible 80186 instructions; not for stock 8088.

VIDEO
320x200/16 and /256: standard VGA modes 0Dh and 13h.
640x480/16: standard VGA mode 12h (main target).
Other modes use compatible VBE, or the native Trident TVGA9000 BIOS.
The NuXT with 512 KB VRAM supports the offered modes except 1024x768/256,
which requires at least 768 KB and is only for other cards/emulators.
All modes use a 16-shade blue-white palette; /256 selects packed 8bpp
storage, not 256 distinct star shades. 320x200 projection compensates for
its non-square pixels on a 4:3 display. Higher modes need a compatible BIOS.

BUILD
  WATCOM=/path/to/watcom sh BUILD.SH
Open Watcom 2, DOS small model, -1 CPU, software floating point. The optional
8087 module is assembled separately with -0 -fp0 -fpi87. EXE is about 20 KB;
loaded image including static workspace and 4 KB stack is about 78 KB.
  WATCOM=/path/to/watcom sh TEST.SH
builds CHECK.EXE for perspective and video readback checks. It is not needed
to run STAR.EXE. See TESTING.TXT for test coverage and limitations.
