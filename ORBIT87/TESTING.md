# ORBIT 87 2.1 validation

Review completed 2026-10-15. Tests use DOSBox-X 2026.10.01, normal core,
S3 VGA. No 86Box testing was used.

## Changes and correctness

- Statistics toggling now updates the footer on each page instead of
  rebuilding both scenes. In 2.0, toggling F while paused erased the animation.
- Footer updates redraw changed character cells and erase a shorter string's
  tail. Each page keeps its own cache. Text is bounded before the separate
  sound-status label, including at 640x480.
- Visible line endpoints bypass the clipping routine. Pages wholly inside a
  video aperture bypass repeated per-line bank calculations. Clipped and
  bank-crossing lines retain the existing safe paths.
- Nebula ring traversal uses compare/wrap instead of repeated modulus
  operations. Colour boundaries are calculated once per frame. The ring
  explicitly resets if its capacity changes with detail.
- Unchanged palette values are cached; entering a scene invalidates the cache.

MATH.C, MATH87.ASM and MUSIC.C are unchanged from 2.0. The original 8087
instructions and the existing assembly pixel kernels are retained.

VIDTEST matched the reference C renderer at 640x480, 800x600 and 1024x768,
including clipping extremes, bank boundaries and drawing after CRC readback.
Frame and post-readback CRCs are the same as the 2.0 values below.

PAGETEST passed **89 checks, zero failures**: page isolation, retained
geometry versus a complete rebuild, all five scenes, paused footer toggles,
shorter/unchanged text, and nebula ring bounds across detail changes.
The new footer tests caught an overlap at 640x480 during development;
bounding the footer before the sound-status label fixed it.

## Measured scene workloads

`tests/REVIEWBENCH.C` runs 60 frames per scene at maximum detail, 640x480,
with 8087 maths, retained geometry and palette updates. Scene five also
renders its scrolling credits. It measures rendering work without normal
frame pacing or page-flip waits. Both versions ran at fixed 3000 DOSBox-X
cycles with the normal core, and the same input sequence.

| Build | Act | 2.0 BIOS ticks | 2.1 BIOS ticks | Frame CRC32, both versions |
|---|---:|---:|---:|---|
| 8088 | 1 | 123 | 77 | `0ced5185` |
| 8088 | 2 | 59 | 43 | `9ac46a8b` |
| 8088 | 3 | 114 | 83 | `5bcb4fbe` |
| 8088 | 4 | 79 | 48 | `cb2c4aeb` |
| 8088 | 5 | 204 | 192 | `bff6d413` |
| V20 | 1 | 112 | 70 | `0ced5185` |
| V20 | 2 | 56 | 40 | `9ac46a8b` |
| V20 | 3 | 107 | 77 | `5bcb4fbe` |
| V20 | 4 | 72 | 43 | `cb2c4aeb` |
| V20 | 5 | 193 | 183 | `bff6d413` |

The combined workload uses about 24% fewer ticks in either build. The
V20-targeted nebula workload improves most (about 40% fewer ticks), while
its text-heavy finale improves only about 5%. These are emulator workloads,
not whole-demo FPS measurements or predictions for the physical NuXT.

The owner subsequently compared the old and new demos on the NuXT and
reported that the new version is visibly faster. No numerical hardware FPS
measurement was taken.

## Release checks and sizes

- ORBIT87.EXE: 41,376 bytes, 85,440-byte linked memory image.
- ORBITV20.EXE: 40,800 bytes, 84,864-byte linked memory image.
- Both retain an 8,192-byte stack and DGROUP below 64 KiB.
- Final V20 executable: `/QUICK /LOOP /TICKS:650 /REPORT /HUD` completed all
  five scenes, looped back to the first, and returned with zero line overflow.
- Final generic executable: `/QUICK /NO87 /SILENT /REPORT` completed all five
  scenes and returned with zero line overflow.
- These fast headless graphics runs used the silent path. A separate explicit
  OPL2 configuration without DOSBox-X's `-silent` switch detected AdLib.
  MUSIRQ checked 36/36 BIOS/music ticks, 15 note triggers, pause/mute, the
  silent clock, restored vectors and restoration of the normal PIT divisor.
  The final V20 executable also completed a 90-tick run with 36 musical
  triggers and zero line overflow.

Native Trident display-start code is unchanged. The owner previously
confirmed working double buffering without flicker with 2.0. The new 2.1
renderer work still needs a physical NuXT check; DOSBox-X checks the generic
VGA/VBE paths and memory contents.

## Reproduction and references

`WATCOM=/path/to/watcom sh BUILDTEST.SH` also builds BENCH0.EXE and BENCH1.EXE
from REVIEWBENCH.C. Each writes BENCH.TXT; preserve the report before running
the other executable. Use fixed 3000 cycles for comparisons. CRC-heavy
VIDTEST/PAGETEST can run at higher cycles. None of the test executables is
needed to run the demo.

Reviewed [Little Game Engine's graphics source](https://github.com/mills32/Little-Game-Engine-for-VGA-EGA/blob/master/lt_gfx.c)
for selective updates and page-buffer design. No third-party source was
copied. Hardware references and the original 2.0 validation follow.

---

# Historical ORBIT 87 2.0 validation

## Builds and target

Open Watcom v2 builds two self-contained DOS executables:

- ORBIT87.EXE: 8086/8088-compatible, `-0 -ms -ot -fpc`, 40,766 bytes.
- ORBITV20.EXE: V20/V30 or 80186-compatible, `-1 -ms -ot -fpc`, 40,286 bytes.

Both use an 8,192-byte stack and original 8087 kernels assembled with
`-0 -fp0 -fpi87`. The linked images are about 83 KB; DGROUP including
stack remains below 64 KB. No implicit compiler floating point, FPU
emulator, FSIN, FCOS, or 287/387-only instructions are used.

The V20 build permits immediate shifts, IMUL/PUSH, LEAVE and PUSHA/POPA.
The hand-written rasterizer and 8087 kernels are shared with the generic
build. NEC's manual confirms the additional instruction forms; compiling
for 80186 does not establish cycle timings on a real V20.

## Renderer optimisation

The second pass combines shallow-line pixels into a mask for each video
byte and uses REP STOSB for horizontal interiors. Partial edge bytes still
load VGA latches. Steeper lines retain the old assembly loop. Text-cell
clearing uses the new span routine when the span fits one video bank.

Independent exact-assembly tests exercised 28,392 cases, checked pixel
masks against Bresenham and verified ES/BP/DS/stack preservation. A random
shallow-line sample reduced VRAM reads+writes from 5,120,110 to 1,075,206
(79%). An aligned 640-pixel horizontal line needs 80 writes instead of
1,280 read/write accesses.

Fixed 3,000-cycle DOSBox tests, v1 versus v2 assembly:

| Mode | Mixed lines | Shallow lines | Horizontal spans |
|---|---:|---:|---:|
| 640x480 | 1.115x | 1.132x | 4.658x |
| 800x600 | 1.117x | 1.130x | 5.679x |
| 1024x768 | 1.124x | 1.138x | 5.568x |

These ratios describe workloads, not whole-demo or physical NuXT FPS.

C/v1/v2 full-plane CRCs matched over clipping extremes, every slope,
single pixels, reversed endpoints, banks, all colours, scaled opaque
text and drawing after CRC readback. Final integrated VIDTEST also matched:

| Mode | Frame CRC32 | After-readback CRC32 |
|---|---|---|
| 640x480 | `44ceb9d3` | `b8ff6be6` |
| 800x600 | `dc7750d0` | `aca06cf1` |
| 1024x768 | `7e05086c` | `7fdcfb6d` |

The compiler-target comparison used identical final renderer sources:

| 100 frames | 8088 build | V20 build | CRC32 |
|---|---:|---:|---|
| Crystal | 95 ticks | 89 ticks | `30a8f929` |
| Ocean | 187 ticks | 174 ticks | `3605d1fa` |

The V20-targeted build improved these DOSBox workloads by about 7%.
Actual V20 speed remains a hardware measurement.

## Page buffering and flicker

For the TVGA9000i (sequencer chip ID 43h), mode 12h and 800x600 can use
per-plane page bases 0 and 65536 on a 512 KB card. CRTC 1Eh bit 7 enables
extended display addressing and bit 5 supplies display-start bit 16.
Other mode bits are preserved; byte addressing is explicit. Banking uses
new-mode sequencer 0Eh with its XOR 2 convention. An all-four-plane alias
probe establishes that reading/writing the two pages accesses distinct
memory. Failed probes leave the single-page renderer available.

Frame construction has three line lists: one history for each screen
page plus one free list for new geometry. Old lines are erased only on
the page being prepared. A completed page is presented at retrace;
interrupts stay enabled during waits, with a short locked recheck/write
for the native register update. Original extended mode state and bank 0
are restored before returning to text mode.

Generic VBE page flipping requires enough scanlines, independent memory
and accepted display-start calls. VBE errors restoring the original
start cause clean startup failure. Plain DOSBox mode 12h aliases its
banks and rejects display-start services: it correctly stays single-page.
Two 1024x768/16 pages do not fit the NuXT's 512 KB; that mode remains single
page there. Larger generic cards may have enough memory.

`tests/PAGETEST.C`: 75 checks passed in DOSBox S3, zero failures. Tests cover
640x480 single-page fallback, 800x600 VBE page independence, drawing and
clearing in both directions, and all five scenes over four consecutive
frames. Each retained frame matches a clear-and-rebuild reference CRC;
the opposite page remains unchanged. An extra moving marker exposes stale
history across the two-frame page reuse interval.

Native Trident display addressing is corroborated by the Xorg Trident
driver, but **native scanout has not been tested on the actual card**.
The owner will test that path. `/SINGLE` provides a direct comparison.

## Music and timers

PULSE DRIVE is an original 112-bar, 140 BPM FM score, approximately 192 seconds.
It uses a swept melodic kick plus drum click, acid bass timbre changes,
chord stabs, two hook phrases, hats/snare/toms/crash, breakdown, build,
drop, peak and outro. No scene switch restarts the song. Visual kick
accents follow a decaying envelope from actual kick triggers.

PIT divisor 16384 produces 72.8 IRQs/second. Every fourth IRQ chains the
saved INT 08h; other IRQs issue their own single PIC EOI. Animation time
stays 18.2 Hz. Pause stops song/animation time while BIOS chaining continues.
Exit restores the standard DOS PIT divisor and original INT 08h; INT 1Ch is
untouched. The sound handler performs no DOS/BIOS/FPU calls or division.
Cached register writes and precomputed pitches bound observed sound work
to 14 OPL register updates in one IRQ.

Host MUSTEST: 141 checks passed, including exact rational tempo, all voices,
pitch sweep, four-on-floor groove, breakdown and build density, scene
continuity, mute/pause/silent/no-card clocks, BIOS divider, restart,
32-bit clock wrap and final release.

MUSIRQ passed with both compiler targets. BIOS/music clocks advanced 36/36
ticks, BIOS time continued paused, silent mode advanced 12/12, original
interrupt vectors returned, and PIT counters demonstrated the fast
16384 divisor followed by the normal 65536 divisor. The final V20 math
checks also passed 341,764 FPU checks and 169,105 forced-integer checks,
zero errors; empty FPU stack/status 0000 and restored control word.

A complete QEMU score recording advanced 3496 BIOS and 3496 music ticks,
with 4512 audible note/drum triggers. The final waveform peaked at
-2.08 dBFS with zero clipped samples; decoded MP3 also had zero clipping.
Output-carrier headroom was adjusted without changing FM modulator levels.
Numerical audio QA verifies signal output; it is not a listening assessment
or a measurement of the physical AdLib's analogue output.

## Integrated runs

DOSBox completed all five shortened acts at 640, 800 and 1024, with no line
buffer overflows; 800/1024 on its larger S3 card used VBE page flipping.
Forced integer and silent paths also returned normally. Final V20 builds
completed both FPU and `/NO87 /SILENT` shortened shows.

QEMU/MS-DOS 6.22 exercised all five final generic-build scenes, direct
selection, next act, mute, statistics and exit. Paused screenshots were
byte-identical across a delay; resuming changed the image. Captures of
all scenes were visually inspected. The generic integration build also
completed the full track and returned to DOS with 3491 frames, 4512 musical
triggers and no line overflow. QEMU is much faster than a V20.

The final 40,286-byte V20 executable completed the full show on
MS-DOS 6.22 in QEMU: 3,496 active ticks, 3,491 frames, 4,512 musical
triggers, all five acts and zero line overflows; the DOS prompt returned.
Final generic page tests again passed all 75 checks and MUSIRQ passed.
A final V20 `/QUICK /LOOP /TICKS:650 /SILENT` run crossed the end of all
five acts, restarted the score/show, and exited at exactly 650 ticks
with 642 frames and zero line overflows.
A combined test harness first hit its 160-second wall limit during the
CRC-heavy page test; rerunning that diagnostic alone at maximum DOSBox
cycles completed normally. This was a harness time allowance issue.

No 86Box tests were run. No 86Box settings, serial settings or physical
drives were changed. Emulator disks are private development fixtures.

## Rebuilding tests

`WATCOM=/path/to/watcom sh BUILDTEST.SH` builds VIDTEST, MATHTEST, MATMUL,
MUSIRQ, SCENETST and PAGETEST. Test executables write reports in their
current directory and are not runtime dependencies.

```sh
cc -std=c99 -O2 -Wall -Wextra -Werror -DHOST -x c MUSIC.C tests/MUSTEST.C -o mustest
./mustest
```

## Primary references

- [NEC V20/V30 User's Manual](https://www.dosdays.co.uk/media/nec/NEC-V20-V30-Users-Manual.pdf)
- [Xorg Trident display-start and chip identification](https://raw.githubusercontent.com/NetBSD/xsrc/trunk/external/mit/xf86-video-trident/dist/src/trident_driver.c)
- [Xorg TVGA mode-register setup](https://raw.githubusercontent.com/NetBSD/xsrc/trunk/external/mit/xf86-video-trident/dist/src/tvga_dac.c)
- [Xorg TVGA bank switching](https://raw.githubusercontent.com/NetBSD/xsrc/trunk/external/mit/xf86-video-trident/dist/src/trident_bank.c)
- [Intel 8087 documentation](https://www.pcjs.org/documents/manuals/intel/8087/)
- [IBM VGA/XGA technical reference](https://bitsavers.org/pdf/ibm/pc/cards/IBM_VGA_XGA_Technical_Reference_Manual_May92.pdf)
- [Yamaha YM3812 datasheet](https://www.ardent-tool.com/datasheets/Yamaha_YM3812.pdf)
