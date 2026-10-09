# Matrix 2.0 validation

The delivered executable was built with Open Watcom v2, `-0 -ms -os -fpc`,
without warnings. All CPU instructions target 8086; explicit x87 code in
FPU.C uses only original 8087 instructions. The link map reports a 51,200-byte
memory image, including a 4,096-byte stack and a 36,272-byte DGROUP.

## Drawing performance and correctness

DOSBox 0.74-3, normal core, fixed 3,000 cycles, S3 VGA emulation. The same
32,000 changing glyphs and five rain shades were submitted to each version:

| Resolution/depth | v1 BIOS ticks | v2 BIOS ticks | Drawing throughput |
| --- | ---: | ---: | ---: |
| 640x480 /16 | 365 | 35 | 10.43x |
| 640x480 /256 | 1034 | 92 | 11.24x |

Setup, palette initialization, lookup generation and checksum readback are
outside the timed region. Every pass changes both glyph and shade, so the
comparison does not just benefit from skipping an unchanged screen.
The animation's existing 18.2-Hz update pacing is separate from this result.
DOSBox cycles do not model a 10-MHz V20 exactly; physical NuXT FPS is unmeasured.

Complete video-memory CRC32 matched v1 in both benchmarks:
`eee28556` (16 colours) and `8d443dc7` (256 colours).
A separate workload covering queued replacements, duplicates, clipping and
bank boundaries matched v1 in all six modes:

| Mode | CRC32 |
| --- | --- |
| 640x480/16 | cb62a4ff |
| 640x480/256 | d3768f13 |
| 800x600/16 | ffbd7204 |
| 800x600/256 | 091b6c86 |
| 1024x768/16 | 1ea36146 |
| 1024x768/256 | 34f9c981 |

`BUILDTEST.SH` builds the included benchmark and checksum test sources.
They output DRAW.TXT and CHECK.TXT. Run them in DOS; CHECK intentionally
attempts all six modes and records unsupported modes rather than assuming
that every card has enough VRAM. CHECK is intended for emulators and cards
that support VGA-compatible readable windows. The main program additionally
checks planar writes/readback and rejects a broken advertised mode.

Bank calculations were independently reviewed over 1-64KB windows, padded
and odd pitches, and glyphs spanning a bank. Bulk clears and packed rows are
clipped to each window boundary. Direct VGA access rejects VBE modes marked
non-VGA-compatible; planar probes select readable and writable windows.

## Coprocessor and fallback

The optional module generates a 64-entry curve from rounded square roots.
The 8087 FSQRT path and integer square-root path return identical bytes.
Only those cached bytes are read during animation; the x87 never participates
in drawing, palette I/O, banking or interrupts.

- Host sanitizer tests checked all 1,025 integer-root inputs (0-1024),
  table range, symmetry and monotonic half-wave. Leak detection was disabled
  because of the sandbox's ptrace restrictions; this module uses no heap.
- DOSBox and QEMU passed detection, identical-table and control-word tests.
- Disassembly confirms a memory FNSTSW probe with no FWAIT before presence
  is established. It contains no FSIN, FNSTSW AX or other 287/387-only forms.
- A separate test copy replaced just the three probe ESC instructions with
  NOPs, simulating an absent coprocessor leaving probe memory unchanged.
  Automatic detection returned 0 and produced the integer table. A calibrated
  instruction trace saw 6,294 instructions and no ESC/FWAIT reached in that
  fallback path. Production code was unchanged by this fault-injection test.
- `/NO87` bypasses both detection and all x87 arithmetic. QEMU keyboard tests
  exercised that actual command-line path in the complete executable.

Actual operation with an empty 8087 socket remains a hardware test. DOSBox's
attempted FPU-off setting and QEMU's CPU-feature mask still executed x87, so
those were not counted as proof of physical absence handling.
Detection initializes a responding x87. The table routine restores the control
word and leaves its own operand stack empty; no FPU state is kept at runtime.

Intel AP-113 (1981), printed page 25, documents the original 8087's special
memory/bus synchronization for FNSTSW/FNSTCW. Intel's 1987 Hardware Reference,
Figure 5-4, gives the no-WAIT detection sequence covering 8086/8088 plus 8087:
[AP-113](https://www.ardent-tool.com/CPU/docs/Intel/808x/8087/appnotes/AP-113.pdf),
[Hardware Reference](https://www.bitsavers.org/components/intel/80386/1987_80386_Hardware_Reference_Manual.pdf).

## Complete executable

QEMU/FreeDOS keyboard checks passed automatic x87 selection, the default
640x480/16 animation, pause/resume, toggling glow while paused, exact restoration
of the previous palette when re-enabled, menu return and DOS exit.
`/NO87 /NOGLOW` reports integer fallback, starts, and allows G to enable glow.
Screenshots were inspected. No 86Box testing or user emulator-setting changes.

The default QEMU VBE backend still misrepresents its higher planar modes;
our probe rejects them. DOSBox S3 handled both higher planar modes correctly.
Native Trident higher modes and physical NuXT performance still need hardware
verification. 1024x768/256 emulator success does not make that mode fit in 512 KB.

## Video references

The single-pass planar write uses VGA write mode 0, Set/Reset value 0 and
Enable Set/Reset equal to the complement of the foreground colour. Selected
planes receive the font byte; background planes are forced to zero, allowing
one CPU write per scanline. See the original [IBM VGA/XGA Technical Reference](https://bitsavers.org/pdf/ibm/pc/cards/IBM_VGA_XGA_Technical_Reference_Manual_May92.pdf), pages 2-79/2-80.

[VBE2.0](https://www.ecsdump.net/wp-content/uploads/2020/12/vbe20.pdf) documents
window attributes, granularity and non-VGA mode flags. Native mode numbers
come from the [Trident TVGA9000i datasheet](https://www.dosdays.co.uk/media/trident/Trident_TVGA9000i.pdf), page 12, and the owner's NuXT 2.0 manual.
