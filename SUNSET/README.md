# Sunset 3.2: Neon Coast

Cruise toward a neon sunset while palm trees pass by and an AdLib synthwave
track plays. Start at 320x200 on the NuXT. Higher resolutions are there to try
on faster machines.

![The demo running at 320x200 in 256 colours.](SCREENSHOT.PNG)

*The demo running at 320x200 in 256 colours.*

- **Run:** copy `SUNSET.EXE` to DOS; start with `SUNSET`.
- **Hardware:** V20/80186 or later with VGA/SVGA. The 8087 prepares the
  perspective table at startup; AdLib plays the soundtrack.
- **Full instructions:** [README.TXT](README.TXT).
- **Build:** `WATCOM=/path/to/watcom sh BUILD.SH` from this folder.

Source and DOS builds are included. See [BUILDING.md](../BUILDING.md) for the
toolchain and [THIRD_PARTY.md](../THIRD_PARTY.md) for license details. The
original project material is [MIT licensed](../LICENSE).

The FURNACE folder contains editable instrument auditions; MUSIC.C contains
the full score. No external music recordings are included.
