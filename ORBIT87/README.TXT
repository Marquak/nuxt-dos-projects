ORBIT 87 2.1
A NuXT space odyssey
===================

An original five-act VGA demo with live 8087 mathematics and an original
AdLib techno score. Made for the NEC V20 NuXT, its Trident VGA, and an 8087.

For the NuXT, copy ORBITV20.EXE to DOS and type:

    ORBITV20 /AUTO

This build uses the V20's 80186-compatible instructions. For an original
8088/8086, use ORBIT87.EXE instead. Both have identical features and
options. Only your chosen EXE is needed: no graphics or music data files.

Without /AUTO, press ENTER for 640 x 480, 16 colours. The complete show
lasts about 3 minutes 12 seconds and exits to DOS when finished.

CHANGES IN 2.1

F now updates only the statistics footer, so toggling it while paused
keeps the scene intact. Statistics redraw only changed character cells.
The line renderer skips repeated clipping/bank calculations when the
visible page already proves they are unnecessary. Nebula trails no longer
use repeated divisions to walk the ring, and unchanged palettes are cached.

LAUNCH OPTIONS

    ORBIT87 /AUTO              Start immediately in 640x480/16.
    ORBIT87 /800               Select 800x600/16 in the launch menu.
    ORBIT87 /1024 /AUTO        Start immediately in 1024x768/16.
    ORBIT87 /SILENT            No sound; do not probe the AdLib.
    ORBIT87 /NO87              Skip the 8087 probe; integer maths only.
    ORBIT87 /LOOP              Repeat until ESC.
    ORBIT87 /DETAIL:1          Start with lower geometric complexity.
    ORBIT87 /HUD               Show optional performance information.
    ORBIT87 /SINGLE            Disable double buffering for comparison.
    ORBIT87 /?                 List every option.

The examples also work with ORBITV20. Options can be combined.
All three display modes use 16 colours.
At the launch menu: ENTER uses the selected mode; 1/2/3 selects a mode
and starts; S toggles sound; ESC quits.

DURING THE SHOW

    ESC or Q    Exit and return to DOS.
    SPACE       Pause/resume the animation and music together.
    M           Mute/unmute while the show keeps running.
    N           Next act (wraps from the finale to the first act).
    1 to 5      Jump directly to an act.
    F           Toggle the optional statistics line.

THE FIVE ACTS

1. GRAVITY GARDEN
   Orbiting particles leave fading trails around a softened central
   gravitational field. The 8087 calculates square roots and forces,
   then projects the moving particles into the camera view.

2. CRYSTAL OBSERVATORY
   Three coloured armillary rings surround a breathing wireframe cube
   and a counter-rotating crystal. The 8087 transforms and projects the
   vertices every frame.

3. NEON OCEAN
   Two travelling waves interfere across a perspective mesh beneath
   an electronic sun. Musical accents increase the ripple amplitude.
   The 8087 handles the live 3D projection.

4. MATHEMATICAL NEBULA
   A Lorenz attractor evolves continuously, leaving a coloured trail
   through a slowly moving viewpoint. The 8087 integrates its equations
   and projects each new point. Older projected points form a flowing
   screen-space trail.

5. WE ARE STILL HERE
   The orbital field returns around the NuXT name and scrolling credits,
   accompanied by the peak and outro of the continuous techno score.

MUSIC - PULSE DRIVE

One original 112-bar, 140 BPM track builds throughout the show: a swept
low kick, rolling acid-style FM bass, offbeat hats, snare, chord stabs,
arpeggios, a developing hook, breakdown, snare-roll build and main drop.
A second answering hook arrives later, followed by the peak and fade.
Uses six OPL2 melodic voices plus the five rhythm instruments. This is
synthesized AdLib music, with no samples or external tracker file.

The first four acts occupy 24, 24, 16 and 32 bars; the finale lasts 16.
Automatic scene changes follow the score position. N and 1..5 change
visuals without restarting the track. /LOOP restarts the whole track
when the show wraps. /QUICK deliberately shortens the visual schedule.

A 72.8 Hz timer drives pitch sweeps and the sequencer independently of
rendering. The normal 18.2 Hz BIOS clock is preserved by chaining every
fourth IRQ. Exit restores INT 08h and the standard DOS PIT divisor.
Pause freezes the show but keeps the BIOS clock running. Mute leaves
the song position and kick-driven visual accents running. No DOS, BIOS
or floating-point operations occur in the sound interrupt handler.
Missing AdLib hardware automatically selects silence. No PC speaker.

HARDWARE AND VIDEO

DOS, an 8086/8088/NEC V20 or later CPU, and VGA are required. ORBIT87.EXE
targets original 8086 instructions; ORBITV20.EXE requires a V20/V30 or
80186-compatible CPU. Do not run ORBITV20.EXE on an original 8088/8086. The optional coprocessor kernels
use original 8087 instructions, including the required WAIT protocol.
No FSIN, FCOS, 287/387-only instructions, or FPU emulator is needed.
Without a detected 8087, the entire show uses integer calculations.

640x480/16 is standard VGA BIOS mode 12h and is the intended target.
800x600/16 and 1024x768/16 try a compatible banked VBE mode, then the
Trident TVGA9000 native BIOS modes used by Matrix. These optional modes
fit the NuXT's 512 KB VGA card. Their actual timings and speed depend
on the card and display. No VESA TSR is needed for the native fallback.
All acts stay in the selected mode, avoiding repeated monitor resyncs.

Video memory is checked at startup. Unsupported modes return to DOS
with an explanation. Use /640 if a higher mode is unavailable.

At 640x480 or 800x600, a TVGA9000i with 512 KB automatically gets two
VRAM pages. The complete next frame is drawn offscreen and then shown
at vertical retrace. No erase-and-rebuild interval is exposed during
normal animation. The footer says DOUBLE BUFFERED when enabled.
The program checks chip ID and proves the pages do not alias before
using this path. No VESA TSR is needed on the native Trident path.

Two 1024x768/16 pages do not fit 512 KB, so the NuXT uses a single page
at that resolution. Other cards may use VBE page flipping if the BIOS
and memory checks allow it. Otherwise single-page drawing is retained.
/SINGLE forces that original path. ORBIT.LOG (/REPORT) records which
path was selected. The owner confirmed double buffering without flicker
on the NuXT with 2.0. Version 2.1 retains that display-start code; its new
renderer work has been checked in DOSBox-X and awaits a NuXT test.

Both executables are about 40 KB. Their linked memory images are about
84 KB, including an 8 KB stack, plus DOS/runtime allocation overhead.
A normal 640 KB DOS configuration has ample space. No RAM framebuffer
or full-screen RAM-to-VGA copy is needed for double buffering.

PERFORMANCE

The V20 draws with 8086 assembly Bresenham lines and VGA write mode 3.
There is no VGA port write per pixel. Long lines are split at video-bank
boundaries without changing Bresenham's error state. Only old geometry
is erased; there is no whole-screen copy or clear for every frame.
Colour buckets reduce VGA register changes. Glyphs use byte masks.
Shallow lines combine pixels that share a VGA byte. Horizontal interiors
use REP STOSB, with latch reads only for partial edge bytes. Text clearing
uses the same span routine. Double buffering retains a separate geometry
history for each video page, so only that page's old lines are erased.

The crystal mesh and ocean grid are cached. Small assembly multiplication
and rounding helpers replace general 32-bit arithmetic in hot paths.
AdLib register writes are cached and pitch words are prepared in advance.

Animation aims for at most 18.2 updates per second. If several frames
take too long, the number of particles/grid vertices drops automatically.
Music and scene timing continue at the same rate. Actual NuXT frame
rates depend on the video mode and real hardware.

The second renderer pass, measured at fixed 3000 DOSBox cycles, is about
11-14% faster on mixed/shallow line tests and 4.7-5.7 times faster on the
horizontal-span test than v1. Shallow-line video-memory accesses fell
79% in an independent instruction-level test. Complete framebuffer
checksums remained identical. These are workload comparisons, not NuXT
frame-rate predictions.

The optional V20 compiler target improved the 100-frame crystal/ocean
tests by about 7% over the 8088-compatible build, again with identical
images. Both retain the hand-optimized assembly loops and original 8087
kernels. Real V20 gains remain a hardware measurement.
The 2.1 review reduced total rendering time for five identical 60-frame
scene workloads by about 24% against 2.0 in DOSBox-X (fixed 3000 cycles).
Both builds produced the same image checksums as before. This is a scene
workload measurement, not a promised NuXT FPS increase. The soundtrack,
scene content and animation timing are unchanged.
See TESTING.md for validation details and remaining hardware checks.

DIAGNOSTIC OPTIONS

    /SCENE:1..5     Start at the specified act.
    /DETAIL:1..3    Initial detail; 3 is the default.
    /QUICK          Shorten each act to about six seconds for testing.
    /TICKS:1..65000  Stop after that many active BIOS ticks.
    /REPORT         Write ORBIT.LOG after the show exits.
    /REF            Use the slower reference C line renderer.

Normally the demo writes no files. /REPORT explicitly enables the log.

SOURCE AND BUILD

Source and test sources are included. Build both executables on Linux
with Open Watcom v2:

    WATCOM=/path/to/watcom sh BUILD.SH

MATH87.ASM is assembled with -0 -fp0 -fpi87. The -fpi87 flag is essential
to emit actual coprocessor instructions rather than emulation interrupts.
C uses -0 (ORBIT87) or -1 (ORBITV20), with -ms -ot -fpc. Thus no
accidental compiler floating-point code can
enter the integer fallback. No floating point runs in an interrupt.

The graphics, composition, font and demo code are original work for these
NuXT projects. OWLIC.TXT contains the Open Watcom runtime licence.
No 86Box testing was performed and no 86Box settings were changed.
