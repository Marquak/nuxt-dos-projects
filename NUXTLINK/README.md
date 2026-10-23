# NuXT Link 4.1

NuXT Link moves files between Linux and DOS over a serial cable. The Linux file manager lets you browse both machines, queue transfers, back up DOS folders, and set the DOS clock. Transfers can resume and are checked before replacing the destination.

Version 4.1 is the serial release. Use its `NUXFER.COM` with the Linux client in this folder. Earlier NuXT Link versions are not included in this checkout.

NuXT Link 4.1 is now deprecated. [Slink-XT 1.0](../SLINKXT/) continues the serial file manager with a new protocol and matching `SLINK.COM`. Existing NuXT Link installations can stay on 4.1; the two programs are not wire-compatible.

![Earlier NuXT Link file manager with a sample transfer queue](SCREENSHOT.PNG)

## Start

Copy `NUXFER.COM` in binary mode to a new DOS folder, for example `C:\UTILS\NXLINK41`. Exit any running serial server before starting the new one:

```dos
C:\UTILS\NXLINK41\NUXFER.COM COM1
```

Use the path and COM port on your machine. The server shares the whole drive containing its executable and starts at 9600 baud for discovery. Esc exits it. If you transfer a replacement through NuXT Link, give it a temporary name while the old server is running, then exit the server before replacing the program.

On Linux, run:

```sh
sh linux/launch.sh
```

Choose the serial device and click **Connect**. The defaults are 115200 baud, RTS/CTS, 4096-byte blocks, and no required DSR/DCD signal. Choose **Flow: none** for a three-wire cable. The 4.0 DOS server also works with the 4.1 Linux client because both use wire version 8. The earlier 3.3.3 and 3.5.2 servers use wire version 6 and will be rejected with an upgrade message.

The client needs Python 3.10 or newer and Tkinter. The included `_heatshrink.so` is for x86-64 Linux; on another Linux architecture, build it with `sh linux/build_codec.sh`. If the system has `liblz4`, the client can also send LZ4 blocks. Without it, raw, zero-run and heatshrink uploads still work.

On first launch, serial profiles are imported from the Fast preview and the older combined client into `~/.config/nuxtlink-serial/profiles.json`. The old profile files remain untouched. If both sources contain the same name with different settings, the older one receives an ` (old serial)` suffix. LAN profiles are not imported.

## Transfers

Linux chooses raw, zero-run, heatshrink or LZ4 for each upload block when compression is enabled. At 115200 baud with 4096-byte blocks, it may choose a slightly larger LZ4 packet over heatshrink because LZ4 decodes faster on the NuXT. Other settings keep the smaller-packet choice. DOS uses a light zero-run encoder for downloads. Incompressible blocks go raw. Every frame has a CRC16, and both sides check the complete file. DOS rereads a staged upload before committing it; Linux verifies staged downloads and keeps the previous destination until the new file and its metadata are saved. File dates and attributes are preserved.

The queue reports each file's transfer time and compression ratio. The ratio counts acknowledged data payload, including encoding headers, but excludes frame headers, acknowledgements and retries. A compressed file can show more original bytes per second than the serial wire rate because fewer bytes actually travel across the cable.

The owner completed four-file queues in both directions on the NuXT at 115200 baud, including a mixed disk image and incompressible data, with zero reported issues. A separate physical check interrupted a 128 KiB upload, reconnected, resumed at the exact byte offset, downloaded the result and matched it byte for byte. Details and limits are in [TESTING.md](TESTING.md).

For a new PC or cable, start with a small file in both directions. If diagnostics show errors or timeouts, lower the baud rate or block size. Try an ordinary program, archive or mixed game file when comparing speeds; mostly empty floppy images compress unusually well.

## Source and credit

This release uses serial only. The older LAN program used a different wire protocol and is not included.

The code was written with GPT-6 Codex and tested by Codex and the owner. The protocol design was informed by FastLynx behavior seen in synthetic QEMU/Wine captures, but includes no FastLynx binary or copied source. See [PROTOCOL.md](PROTOCOL.md), [TESTING.md](TESTING.md), and [CHANGES.md](CHANGES.md). DOS source is GPL-3.0-or-later because it derives from the earlier combined NuXT Link source. The Linux client is MIT licensed; heatshrink is ISC licensed; the optional system `liblz4` is BSD-2-Clause licensed.
