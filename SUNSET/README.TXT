NEON COAST 3.2
Endless sunset drive for the NuXT / V20 / 8087 / AdLib

RUN
  SUNSET

Default: 320x200, 256 colours. This is the recommended fast mode.
Only SUNSET.EXE is required on DOS. Artwork and music are embedded.
There is no text during the drive and no automatic ending.

LAUNCH MENU
  1         Select 320x200
  2         Select 640x480
  3         Select 800x600
  C         Toggle 16 / 256 colours
  S         Toggle sound
  ENTER     Start the selected mode
  ESC       Quit

WHILE RUNNING
  ESC or Q  Return to DOS
  SPACE     Pause/resume both the drive and soundtrack
  M         Mute/unmute; the arrangement continues while muted

COMMAND LINE
  SUNSET /AUTO                  Start 320x200 / 256 immediately
  SUNSET /320 /16 /AUTO          320x200 / 16 colours
  SUNSET /640 /16 /AUTO          640x480 / 16 colours
  SUNSET /800 /16 /AUTO          800x600 / 16 colours
  SUNSET /640 /256 /AUTO         Previous high-resolution colour mode
  SUNSET /800 /256 /AUTO         Highest-resolution colour mode
  SUNSET /AUTO /DETAIL:1         Fewer/simpler palms
  SUNSET /AUTO /SILENT           No sound
  SUNSET /AUTO /NO87             Force integer projection
  SUNSET /?                     Show help

All three resolutions support both colour depths. /640 and /800 retain
256 colours unless /16 is also given. The running screen has no HUD,
messages, logos or credits. An ordinary launch runs until you quit.

VIDEO MODES
  Resolution   Colours  VRAM used   Hardware path
  320x200      256       64,000 B   Standard VGA mode 13h, linear
  320x200       16       32,000 B   Standard mode 0Dh, four planes
  640x480      256      307,200 B   Trident 5Dh or VBE 101h, banked
  640x480       16      153,600 B   Standard VGA mode 12h, four planes
  800x600      256      480,000 B   Trident 5Eh or VBE 103h, banked
  800x600       16      240,000 B   Trident 5Bh or compatible VBE 102h

The 16-colour modes are actual four-plane modes, not 256-colour modes with
most colours unused. A custom neon palette and fixed ordered dithering
retain the sunset gradients. The foreground car still covers lines behind
it. Tail-light palette pulsing is used in 256-colour modes only.

320x200 was chosen over 320x240 because standard mode 13h offers a simple,
fast linear framebuffer. It uses the usual non-square DOS VGA pixels;
the scene is scaled for a 4:3 display. The high-resolution modes remain
available for testing and appearance. Fewer colours do not automatically
mean faster rendering: the 16-colour converter needs extra CPU work, even
though it writes fewer VRAM bytes. Start with 320x200 / 256 for speed.

HARDWARE
  NEC V20/V30 or an 80186-compatible instruction set is required.
  This executable intentionally does not support the original 8088/8086.
  Target: NuXT, 10 MHz V20, original 8087, Trident TVGA9000i/512 KB, AdLib.
  Standard VGA handles both 320 modes and 640x480/16. Other combinations
  require the supported Trident BIOS or a compatible VBE BIOS.
  The 8087 is detected automatically and builds the perspective depth
  table at startup. V20 assembly uses that table while drawing. General
  projection and low-memory fallback retain the original 8087 routines.
  /NO87 builds the table with integer maths and also bypasses detection.
  AdLib-compatible OPL2 at port 388h is detected automatically. Without it
  the demo continues silently. A normal 640 KB DOS setup is recommended.
  Tested with DOS 6.22 and DOSBox. No runtime sound driver is needed.

HORIZON RUNNER - THE SOUNDTRACK
  An original 104 BPM synthwave track, 96 bars / about 3 minutes 42 seconds
  per arrangement. The requested direction is the chapter at 1:23:44 in:
    https://www.youtube.com/watch?v=YrRhEWiHBjU&t=5024s
  The playlist identifies this area as 1000 Years, Karl Casey / White Bat
  Audio. This is a newly composed FM tune, not a transcription or sample.

  Three-note sustained chords, syncopated bass with changing FM brightness,
  a melodic lead with pitch scoops and chip vibrato, a separate echo voice,
  pitch-swept bass drum, snare flams, hats, tom fills and cymbal accents.
  The chords dip around each kick, then swell back. A breakdown, build,
  answering theme and turnaround give the loop a longer musical shape.
  The lead answer changes on alternate loops. No fade or stop at the seam.

  All sound comes from the original mono YM3812/OPL2: six melodic voices
  plus its five hardware percussion voices. No samples or OPL3 required.
  A 72.8 Hz interrupt keeps music independent of rendering; BIOS time stays
  at its normal rate. Effects use small lookup tables and cached writes.
  No FPU work, allocation, division or DOS calls occur in the interrupt.

  FURNACE/AUDITION.S3M opens in Furnace and auditions the six static
  melodic patches, separately and as a chord. The accompanying .SBI files
  can be imported individually. These are editable patch starting points;
  MUSIC.C contains the full score and its live macros. See that folder's
  README.TXT for the macro settings. Furnace is optional on Linux and is
  not needed to run the demo on DOS.

PERFORMANCE / DETAIL
  /DETAIL:3 (default) has eight palms; /DETAIL:2 and :1 have six.
  Distant palms use four fronds, middle-distance palms six, and nearby
  palms up to eight. /DETAIL:1 caps nearby fronds at six.
  Fixed palm vectors are precomputed, avoiding repeated sine/multiply
  work every frame. Position and perspective are still calculated live.
  Sustained slow rendering or tile exhaustion may reduce detail further.

  At 320x200, a decoded background and reusable frame stay in RAM. This
  costs about 127 KiB, avoiding repeated background decompression and
  per-pixel tile lookups. V20 assembly draws the lines and finds changed
  spans. Only completed spans are copied to VGA, preserving car occlusion.
  Version 3.2 batches dirty-mask reads and row copies in assembly, avoiding
  repeated far-pointer setup and short C-to-assembly calls. No extra RAM
  buffer, skipped scanlines or reduced detail. At fixed full detail in a
  DOSBox-X comparison, 120 frames took 264 rendering ticks versus 277 before
  in 256 colours, and 466 versus 478 in 16 colours. Actual NuXT gains may differ.
  Higher resolutions, or insufficient RAM for the cache, use sparse tiles.

  The 8087 prepares scale/depth coefficients once; each frame uses fast
  fixed-point assembly multiplies. All supported depths/scales were checked
  against exact projection: the cached result differs by at most one pixel.
  Screen coordinates are calculated once per vertex and reused by edges.

  One VRAM frame is used; regional/plane updates are not atomic page flips.
  Existing assembly RLE, video copies and planar conversion remain.
  No FPU work, allocation or DOS calls occur inside the music interrupt.

  In the fixed-3000-cycle DOSBox comparison, 100 frames at 320x200/256
  took 226 ticks versus 414 for version 2.0, about 1.8 times the throughput.
  The new build also retained eight palms instead of reducing to six.
  This is an emulator comparison, not a measured NuXT speed promise.
  Version 3.1 keeps the 3.0 renderer and artwork. With the new music,
  the same 100-frame comparison took 227 ticks versus 226 for 3.0;
  both retained eight palms. Real NuXT performance remains to be checked.

DIAGNOSTICS
  /REPORT writes SUNSET.LOG after exit: selected mode/colours, 8087 use,
  renderer, clock ticks, frames, VRAM bytes, tile usage, detail and notes.
  /TICKS:n stops after n animation ticks (about 18.2 per second).
  /FRAMES:n stops after n rendered frames. Both accept 1..65000.
  These optional test limits are absent during a normal endless launch.

BUILD / TEST (Linux)
  Set WATCOM to an Open Watcom installation, then:
    sh BUILD.SH
  C targets V20/80186 with -1 -ms -ot -fpc. FASTLINE.ASM targets -1.
  MATH87.ASM uses -0 -fp0 -fpi87 for the original 8087 instruction set.
  Generated ART.H, COLOR16.H and FRONDS.H are included. Python is needed
  only to regenerate those assets: install Pillow, then run
    python3 tools/make_art.py
  Reusable host and DOSBox regression checks:
    sh TEST.SH
  Tests write into a new directory outside the source folder.
  See TESTING.md for results and hardware limitations.

All artwork and music were created for this demo.
Open Watcom runtime license: OWLIC.TXT.
