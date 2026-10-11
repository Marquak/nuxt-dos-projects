# XT Invaders 2.0

A little Space Invaders-style game for an XT. EGA or VGA, gameport controls,
AdLib effects and a single COM file. The aliens now march to a four-note
thump, with a different wireframe backdrop each wave.

- **Run:** `INVADERS` for 320x200; `INVADERS /V` for 640x480. Both use 16 colours.
- **Joystick/gamepad:** add `/J` to calibrate, or press J at the title.
- **Controls:** arrows and Space, or gameport left/right and button 1. P or
  button 2 pauses; M toggles sound; Escape quits.
- **Hardware:** 8088/8086 or later; EGA/VGA; optional AdLib and standard
  analog-compatible PC gameport. No external assets or audio driver.
- **Full instructions:** [README.TXT](README.TXT).
- **Build:** `sh BUILD.SH` with GNU binutils.

Version 2.0 improves fire-tap handling, doubles the player/laser update rate,
reduces redraw work and adds six original wireframe backgrounds. VGA uses
crisp doubled sprites in a centred 640x400 playfield within 640x480.

Tested in QEMU and DOSBox, including an emulated gameport controller. See
[TESTS.TXT](TESTS.TXT) for coverage and hardware-validation limits.
Project-authored material is MIT licensed; see [../LICENSE](../LICENSE) and
[../THIRD_PARTY.md](../THIRD_PARTY.md) for runtime/dependency terms.
