# ORBIT 87 2.1

A five-scene space demo with an AdLib techno soundtrack. The 8087 handles
its live geometry when present. There are separate builds for the V20 and
stock 8088/8086 machines.

![Neon Ocean scene. ORBIT87 and ORBITV20 share the same visuals.](SCREENSHOT.PNG)

*Neon Ocean scene. ORBIT87 and ORBITV20 share the same visuals.*

- **Run:** use `ORBITV20 /AUTO` on a V20, or `ORBIT87 /AUTO` on an 8088/8086.
- **Hardware:** VGA/SVGA; optional 8087 and AdLib. Both CPU builds share this source tree.
- **Full instructions:** [README.TXT](README.TXT).
- **Build:** `WATCOM=/path/to/watcom sh BUILD.SH` from this folder.

Source and DOS builds are included. See [BUILDING.md](../BUILDING.md) for the
toolchain and [THIRD_PARTY.md](../THIRD_PARTY.md) for license details. The
original project material is [MIT licensed](../LICENSE).

Version 2.1 fixes the statistics toggle while paused and runs visibly faster
on my NuXT after reducing repeated drawing and calculations.
Emulator measurements and their limits are in [TESTING.md](TESTING.md).
