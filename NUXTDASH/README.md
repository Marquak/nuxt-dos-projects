# NuXT Dash 1.3

Check what's in your NuXT and what DOS can see. Dash shows CPU speed, memory,
video, ports, BIOS information and drives. It's handy for checking the turbo
speed or seeing how much upper memory is free.

Copy **DASH.COM** to your DOS machine and run `DASH`. No installation or
configuration file needed. This version is working well on my NuXT.

## What's in it

- Overview: DOS version, approximate V20/V30 speed, 8087 response, system BIOS
  version and ROM date. The system BIOS and XTIDE versions are separate.
- Memory/Video: conventional memory, DOS-managed UMB usage and BIOS-reported VRAM.
- Ports: COM and LPT addresses recorded by the BIOS.
- Interrupts: current interrupt-handler addresses.
- NuXT reference: manual defaults, clearly labelled as reference information.
- Drives: XTIDE revision and controller configuration, BIOS disk geometry and
  capacity, and the configured floppy types. A Gotek shows its emulated type.

Use **1-6** or **F1-F6** to pick a page, arrows/Tab to cycle, **R** to refresh,
**S** to save all six pages, and **Esc/Q** to exit. CPU speed is sampled on
startup and refresh, so press R after changing turbo. Reports use numbered
names and won't overwrite an existing report.

## A few limits

The CPU speed is an estimate for NEC V20/V30-compatible timing. The 8087 check
looks for an instruction response; it isn't a full arithmetic test. UMB totals
need a DOS UMB driver. Without one, Dash cannot measure the physical upper RAM.

VRAM comes from VBE or the Trident BIOS query, without writing test patterns.
Drives use BIOS information calls, not direct ATA commands. Disk model/serial
numbers and DOS partitions aren't listed. Unknown BIOS versions or unsupported
queries stay marked as unknown. A broken BIOS call can still hang in firmware.

About 22 KiB of program memory. DOS 2+ for the dashboard, DOS 3+ for report
creation, DOS 5+ for UMB reporting. The program uses 8086 instructions plus
original-8087 control-word stores for the coprocessor check.

## Build and tests

On Linux with GNU binutils (`as`, `ld`, `objcopy`):

```sh
sh BUILD.SH
```

No Open Watcom or C runtime required. [README.TXT](README.TXT) has the detailed
probe behaviour and technical references. [TESTS.TXT](TESTS.TXT) records the
QEMU and instruction-level checks. No 86Box testing was used for this update.
The owner also confirmed the final 1.3 build works on the NuXT.
