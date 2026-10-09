# Neon Coast 3.1 validation

This release replaces the soundtrack with **Horizon Runner**. The renderer,
projection routines and artwork match the 3.0 baseline byte for byte. MAIN.C
changes only its version and soundtrack labels. The user has reported that
3.0 runs substantially better on the real NuXT at 320x200/256.

## Build and resources

Open Watcom targets V20/80186 (`-1 -ms -ot -fpc`); FASTLINE.ASM targets
80186 and MATH87.ASM retains original-8087 instructions (`-0 -fp0 -fpi87`).
SUNSET.EXE is 126,382 bytes. DGROUP is 63,184 bytes including the 8,192-byte
stack, within the 64 KiB segment limit. The previous DGROUP was 63,008 bytes.
Far artwork, framebuffer allocations and low-memory fallbacks are unchanged.
Only SUNSET.EXE is required on DOS.

## Music checks

The host suite passes 164 checks over two complete loops, including:

- 104 BPM fractional tempo, loop phase and alternating lead answers.
- All six melodic channels, five rhythm voices and key-off ordering.
- Bass brightness, live lead bends, pad ducking and hardware-kick sweeps.
- Continuous loop transitions, pause, mute, no-card and silent operation.
- BIOS clock division, counter wrap and bounded interrupt work.

The final host suite also passes AddressSanitizer and UndefinedBehaviorSanitizer.
Review found and fixed a potentially out-of-range arpeggio index. The final
music has a peak of 24 OPL writes per IRQ, compared with 22 previously.
The two-loop run emitted 53,464 writes over 442.96 seconds (including initial
setup), about 121 per second. This is greater average I/O than the previous
score, whose comparable run was about 70 writes/second.

The music ISR retains the 72.8 Hz service rate, ISA-safe register delays and
18.2 Hz BIOS chaining. Effects use fixed tables, bounded loops and a register
cache; no floating point, division, allocation or DOS calls occur in the ISR.

The final DOS MUSIRQ test passes at 1,500 DOSBox cycles: BIOS and animation
clocks each advance 36 ticks, pause/mute operate, INT 08h is restored, INT 1Ch
is left owned by its original handler, and the PIT divisor returns to normal.
Silent mode also advances its clock and restores the interrupt correctly.

## Regression and executable checks

All 12 isolated DOSBox cases passed: 8087 and integer projection, artwork,
packed video at all three resolutions, all planar modes, sparse tiles and
mode transitions, cached 320 compositor, and music IRQ behaviour. The full
run was followed by the final bounds fix, sanitized host checks, and a fresh
DOS music IRQ run. Rendering/projection code did not change after that suite.

The final actual EXE ran 100 frames and returned to DOS with sound enabled;
a second run used /SILENT. Both reported the cached 320 renderer, 8087=1,
no overflow frames and full eight-palm detail.

DOSBox normal core, fixed 3,000 cycles, S3 VGA, 320x200/256, 100 frames:

|Build|Elapsed BIOS ticks|Rendering ticks|Final detail|
|---|---:|---:|---|
|3.0, old soundtrack|226|225|3: eight palms|
|3.1, Horizon Runner|227|226|3: eight palms|
|3.1, silent|225|224|3: eight palms|

These are emulator comparisons of time-driven/adaptive rendering, not
identical frame inputs or a prediction of real V20 performance. The 100-frame
run covers the introduction; it is not a benchmark of every musical section.
The full two-loop host run establishes the register-write bound across them.

## Audio and Furnace

A private DOS 6.22/QEMU image runs MUSIC.C through a real-mode test player,
using QEMU's AdLib device and WAV backend. The capture covers one complete
96-bar arrangement plus 12 bars of the next loop. Boot silence is removed
for the MP3 preview; no EQ, reverb, stereo widening or sample overlay is added.
This is emulated OPL2 output, not a recording of the physical AdLib card.
The final capture has zero clipped PCM samples and a peak of -0.71 dBFS.
The nominal loop transition retains audio; 25 ms windows around it have
RMS levels between -34.39 and -11.73 dBFS. The host trace independently
confirms that score wrap does not reset the chip or interrupt the clock.

Furnace 0.6.8.3 imports and renders FURNACE/AUDITION.S3M. Its text export
confirms one YM3812/OPL2, six two-operator instruments and zero samples.
The SBI files contain the same static patches in the documented SBI layout.
AUDITION.S3M is a patch test phrase, not an editable full-song score; MUSIC.C
holds the arrangement and live effects. The bank README lists macro values.

Direct listening is unavailable in this agent session. Signal-level checks
and tracker inspection supplement the musical composition; they do not
substitute for the user's listening judgment or a real AdLib test.

## Research

The original musical reference remains the user's YouTube timestamp, near
"1000 Years" by Karl Casey / White Bat Audio. The subsequently supplied MP3
covers a different part of the playlist; the user confirmed retaining the
original timestamp. Horizon Runner uses newly composed notes and patches;
no reference audio, melody transcription or third-party instrument bank is
embedded in the executable.

Primary technical references consulted:

- [Furnace OPL instrument guide](https://github.com/tildearrow/furnace/blob/master/doc/4-instrument/fm-opl.md):
  operator levels, envelopes, waveforms and modulation controls.
- [Furnace OPL effects](https://github.com/tildearrow/furnace/blob/master/doc/7-systems/opl.md):
  live operator/pitch changes and hardware rhythm mode.
- [AdPlug for 8088/86](https://github.com/mills32/Adplug-for-8088-86):
  existing XT-class FM-player approaches and CPU constraints.
- [Nepper](https://github.com/Kagamma/nepper): DOS OPL tracker/player reference.
- [Furnace instrument importer](https://github.com/tildearrow/furnace/blob/master/src/engine/fileOpsIns.cpp):
  SBI container layout for the exported patches.

Private development captures and emulator images are not bundled. No 86Box
test was run and no physical disk or user emulator setup was changed during
that validation pass. The owner later reported liking the soundtrack on the
real NuXT. See the repository VALIDATION.md for the packaging checks.
