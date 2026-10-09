HORIZON RUNNER - FURNACE INSTRUMENT BANK

Open AUDITION.S3M in Furnace for a short test phrase that plays all six
patches separately, then the three chord voices together. It uses one
YM3812/OPL2, no samples. Save As .fur if you want to edit it in Furnace's
native format. This is a patch audition, not the full soundtrack.

The six SBI instruments are the melodic patches from SUNSET 3.1.
In Furnace, make a song using Yamaha YM3812 (OPL2), then use Load instrument
in the Instruments window to load the .SBI files. Choose the OPL2 drums
variant when arranging a complete tune with hardware percussion.

ECHO.SBI     Quiet lead echo / arpeggio
BASS.SBI     Pulsing FM bass
LEAD.SBI     Main melody
PADLOW.SBI   Low chord voice
PADMID.SBI   Middle chord voice
PADHIGH.SBI  High chord voice

The carrier levels include the demo's output headroom. They are deliberately
not all equally loud. Start the bass in octave 2, lead in octave 5, and pads
around octave 4 (middle C is MIDI 60).

SBI stores a static patch, not the arrangement or tracker macros. The demo
adds the following effects in MUSIC.C:
- Bass modulator total level: 22,25,29,33,36,38,39 (main section).
- Lead F-number scoop: -12,-7,-3,0,0 relative to its tuned pitch.
- Three-sixteenth-note echo on a separate voice.
- Chord ducking: add 12,9,6,3,1,0,0 to carrier attenuation after a kick.
- Pad detune: -2/0/+2 F-number units on the low/middle/high voice.
- Bass drum pitch: MIDI 53,45,38,32,29,28, one step per music interrupt.

The demo services the OPL2 at about 72.8 Hz. Bass, lead and ducking macros
advance every second interrupt; drum pitch advances every interrupt.
MUSIC.C remains the authoritative full score; these are editable starting
patches, not an importable copy of the whole song. Use two-operator OPL2
features and waveforms 0..3 for the NuXT's original AdLib.

Regenerate this bank from the source with:
  python3 tools/export_opl_bank.py

Official Furnace OPL instrument guide:
https://github.com/tildearrow/furnace/blob/master/doc/4-instrument/fm-opl.md
