# Matrix 2.0

Fill the screen with falling green characters inspired by The Matrix. Choose
a resolution and colour depth, then let it run. The main mode is 640x480 in
16 colours; higher modes are there to try on faster video cards.

![Falling green characters with glow enabled.](SCREENSHOT.PNG)

*Falling green characters with glow enabled.*

- **Run:** copy `MATRIX.EXE` to DOS; start with `MATRIX`.
- **Hardware:** 8088/8086 or later with VGA/SVGA. An 8087 can calculate the
  glow curve; `BENCH`, `CHECK` and `FPUTEST` are included for diagnostics.
- **Full instructions:** [README.TXT](README.TXT).
- **Build:** `WATCOM=/path/to/watcom sh BUILD.SH` from this folder.

The source and DOS programs are here too. See [BUILDING.md](../BUILDING.md)
for tools and [THIRD_PARTY.md](../THIRD_PARTY.md) for license details. The
original project material is [MIT licensed](../LICENSE).
