# Building on Linux

## Requirements

- Open Watcom C/C++ 2.0, Linux x86-64 host tools (`wcl`, `wcc`, `wasm`, `wlink`):
  https://github.com/open-watcom/open-watcom-v2/releases
- GNU binutils with i386 support (`as`, `ld`, `objcopy`) for INVADERS.
- Python 3.10+ for the host utilities and DUNE2FIX; Tkinter for NuXTLink.
- Optional: Pillow to regenerate SUNSET procedural artwork; Furnace to edit
  the included OPL instrument audition. Neither is needed to compile/run DOS binaries.
- Optional tests: DOSBox, a host C compiler, Xvfb for headless GUI tests, and
  Unicorn for the separate NuXTLink UART emulator test. See each test guide.

Use a toolchain location without spaces. Set WATCOM to the installation root:

```sh
export WATCOM=/path/to/watcom
sh BUILD_ALL.SH
```

This rebuilds all seven original projects, including both ORBIT executables,
and refreshes NuXTLink's alternate-name binary. DUNE2FIX is a Python script;
it does not compile or distribute Dune II. Builds replace the binaries inside
this checkout. Use a copy if you want to retain the snapshot byte-for-byte.

For a single project:

```sh
(cd ORBIT87 && sh BUILD.SH)
(cd NUXTLINK && sh dos/BUILD.SH)
```

After a NuXTLink-only build, `dos/NUXFER.COM` is authoritative; copy it to
`NUXTLINK/NUXFER31.COM` if you also use the alternate-name distribution file.
Build scripts select the required CPU target; do not replace them with generic
compiler defaults. V20 does not support arbitrary 286/386 instructions, and
8087 assembly must remain compatible with the original coprocessor.

## Tests

```sh
python3 -m unittest discover -s DUNE2FIX -v
python3 -m unittest discover -s NUXTLINK/tests -v
(cd NUXMENU && sh TEST.SH)
(cd SUNSET && sh TEST.SH --build-only)
```

NuXTLink GUI tests skip when Tk cannot open a display. For a headless GUI run,
use `xvfb-run -a python3 -m unittest discover -s NUXTLINK/tests -v`.
Integration tests require a running DOS server and a disposable test drive;
they can create/delete remote files and are not part of ordinary unit tests.
SUNSET's full `TEST.SH` runs additional isolated DOSBox tests; `--build-only`
still runs host music checks and builds DOS test executables.

MATRIX/BUILDTEST.SH, ORBIT87/BUILDTEST.SH and STARFIELD/TEST.SH build their DOS
validation programs. Follow their TESTING documents to run the workloads.
Historical timings describe the original test environments, not performance
promises for other machines. No commercial OS or emulator hard-disk image is
included; supply your own lawful DOS environment if using QEMU.

## Generated content

SUNSET/ART.H and the other generated art headers are included so normal builds
need no Python imaging library. `python3 SUNSET/tools/make_art.py` regenerates
the art and previews; `python3 SUNSET/tools/export_opl_bank.py` regenerates the
Furnace instrument bank. MATRIX and ORBIT fonts and INVADERS sprite bytes are
included as source. See THIRD_PARTY.md for tool/runtime license information.
