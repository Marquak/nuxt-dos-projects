NUXTDASH 1.3
============
A small DOS hardware dashboard for the NuXT. Copy DASH.COM and run DASH.
No installation, configuration file or runtime library is needed.

KEYS
----
1-6 / F1-F6       Select a page
Left/Right       Previous/next page
PgUp/PgDn/Tab     Cycle pages
R                Refresh, including a new CPU-speed sample
S                Refresh and save all six pages to a numbered text report
Esc / Q          Restore the screen and return to DOS
DASH /?          Command-line help

NEW IN 1.3
----------
System BIOS version on Overview, BIOS-reported VRAM under Memory/Video,
and a sixth page for XTIDE ROM information and drives. Reports include all six.

SYSTEM BIOS VERSION
-------------------
Reads the " BIOS, Version " POST banner used by Sergey Kiselev's 8088 BIOS
and Monotech firmware from F8000-FFFFF. Shows the version found in your ROM,
separately from the XTIDE version and the standard ROM date field. An unknown
or malformed banner is reported as unrecognized, with no guessed version.

VIDEO RAM
---------
Tries VBE function 4F00h, then the documented Trident INT 10h AX=1200h,
BL=12h query when a Trident VGA ROM signature is present. Reports the BIOS
value in KiB and the source. Unsupported queries show unknown. No VRAM pattern
writes, bank switching, mode changes or chipset-register writes are used.
This reports firmware's installed-memory value, not a physical RAM test.
The Trident fallback supports old VGA BIOSes without VBE; the reference
profile remains separately labelled and is never substituted for detection.

DRIVES
------
Recognizes XTIDE's XUB212 ROM header and reads its title, revision, first
controller configuration, command/control I/O bases, IRQ and controller count.
Tested against the documented r638 layout. Other layouts are not guessed.
ROM configuration does not establish that the controller or a drive responds.

Uses INT 13h AH=08h for geometry/configured floppy type and AH=41h/48h or
AH=15h for hard-disk capacity. Falls back to a labelled CHS estimate. Capacity
is whole MiB; geometry may be BIOS-translated. No sector reads, sector writes,
resets or direct ATA commands. DOS partitions are not enumerated. Drive model,
serial number and SMART data are not available through these BIOS queries.
A BIOS/controller can itself hang on a query if malfunctioning; Dash cannot
impose a timeout inside a BIOS interrupt handler.

Floppies are BIOS-configured types, not inserted-media capacities. A Gotek
appears as the floppy drive it emulates. Unsupported queries are labelled.

NEW IN 1.2
----------
Overview now shows an approximate NEC V20/V30 CPU speed and an actual x87
instruction-response probe, separately from the BIOS coprocessor flag.
Memory/Video shows DOS-managed UMB total, used, free and largest free block.

CPU SPEED
---------
The estimate is measured at startup, on R and before saving a report. Use R
after changing turbo speed. It is not continuously sampled while idle.

A short NEC-compatible CPU check selects the V20/V30 timing path. Intel CPUs
show N/A rather than a speed calculated from the wrong instruction timings.
The check does not distinguish V20 from V30 or related compatible NEC CPUs.

The measurement times unrolled AAM instructions over four BIOS timer ticks,
about 0.22 seconds after synchronization with the next tick. It uses NEC's
15-clock AAM timing plus an approximate loop overhead. It assumes the normal
18.2065 Hz BIOS clock. It does not reprogram the PIT, write chipset ports,
disable interrupts, change turbo settings or install an interrupt handler.

Treat the MHz number as approximate. Interrupt handlers, bus waits, memory
refresh and instruction fetching can lower the estimate. Modified timer
software and emulator timing can make it inaccurate. Timeouts and implausible
results display N/A. The user confirmed turbo-speed changes on the NuXT; the display
is not forced to one of the expected 4.77/7.16/9.55 MHz settings.

8087 / x87 PROBE
---------------
Two no-WAIT FNSTCW stores start with different sentinel words. Identical
returned control words establish an instruction response. No FNINIT, FWAIT,
exception clearing or arithmetic is performed. Control/status/tag and stack
register preservation was tested with a non-default control word in QEMU.

On a V20/8088 NuXT this checks for the 8087 response. On later systems it can
be another x87, and an emulator can provide the response. It does not identify
the chip model or certify that every arithmetic instruction works. The BIOS
flag remains visible so disagreement with the probe can be spotted. An empty
8087 socket still needs a physical-hardware check; the absent response was
covered by instruction-level tests.

UPPER MEMORY
------------
Requires DOS 5+ UMB-link services and DOS-compatible memory control blocks.
Reports memory exposed to DOS by its UMB driver, not physical RAM-bank size.
Without a driver, 'None exposed by DOS' does not mean upper RAM is absent.
An unavailable service or invalid memory chain is reported separately.

The scan temporarily links UMBs if needed, walks the DOS memory-block chain,
and restores the original link state. Allocation strategy is not changed.
It does not call an XMS provider or read/write-test unused upper addresses.
Reserved SC gaps and excluded-memory blocks are omitted. Used space includes
MCB headers; the largest free block excludes its header. UMB values round to
the nearest KiB, so separately rounded numbers can differ by 1 KiB. Linked
UMBs containing the running program, if any, count as used.

Tested with MS-DOS 6.22 and EMM386 UMBs, with and without DOSKEY loaded high.
Other DOS implementations and memory managers may use different layouts;
physical NuXT USE!UMBS/DOSMAX accounting remains a user hardware test.

PAGES
-----
1 Overview: DOS version, conventional memory, largest free DOS block, ROM
  date/model, system BIOS version, equipment word, BIOS FPU flag, current drive, CPU speed, probe.
2 Memory/Video: requested program allocation, UMB statistics, video BIOS
  interface, startup/current mode and page, text buffer, display code and BIOS-reported VRAM.
3 Ports: COM1-COM4 and LPT1-LPT3 addresses from the BIOS data area.
4 Interrupts: twelve current interrupt-handler addresses.
5 NuXT reference: manual defaults, explicitly not detected settings.
6 Drives: XTIDE ROM revision/configuration, BIOS disk geometry/capacity
  and configured floppy types. First two hard disks and floppy drives shown.

The supplied NuXT/V20/512 KiB video profile stays labelled as a reference.
UART model/FIFO/baud, physical IRQ wiring and chipset are not probed.
The largest free DOS block is not total free memory. It excludes DASH and
follows the existing DOS allocation/UMB policy; it is rounded down to KiB.

REPORTS
-------
S writes DASH00.TXT through DASH99.TXT in the current directory. Existing
files are never overwritten. DOS 3+ is required for safe create-new files.
All six pages are included, except the live clock/status row. Write failure
attempts to delete an incomplete report; a close failure may leave the file.
Normal browsing does not write files. No firmware or configuration writes.

DISPLAY / MEMORY
----------------
8086/8088 instruction set plus original-8087 control-word store opcodes.
DOS 2+ dashboard API target; DOS 5+ needed for UMB reporting.
About 22 KiB program allocation, plus the DOS environment.

80-column modes 2, 3 and 7 preserve the active page, first 25 rows and cursor.
Additional rows in 43/50-line modes are left alone. A graphics/40-column start
switches to mode 3; the old mode/page is restored, but its contents are lost.
Original CGA can show snow: direct text writes are not retrace synchronized.
While idle, HLT waits for interrupts. Only the status row updates each second.
BIOS tick reads do not consume the midnight flag. CPU timing is on demand.

BUILD
-----
Run sh BUILD.SH on Linux with GNU binutils (as, ld, objcopy).
DASH.S is GNU assembler Intel syntax, not MASM/TASM/NASM source.
COM.LD places code at 0100h and leaves buffers/stack out of the disk file.
An RWX warning for the intermediate ELF is expected for a real-mode COM.
TESTS.TXT records what was tested and the remaining limits.

REFERENCES
----------
User-supplied Monotech NuXT 2.0 manual (May 2020), hardware reference only.
Sergey Kiselev's 8088 BIOS, CPU identification/reference review:
https://github.com/skiselev/8088_bios/blob/master/src/cpu.inc
NEC V Series instruction manual, CVTBD/AAM timing, table 2-8:
https://datasheets.chipdb.org/NEC/V20-V30/U11301EJ5V0UMJ1.PDF
Intel AP-113, original-8087 control-store synchronization:
https://www.ardent-tool.com/CPU/docs/Intel/808x/8087/appnotes/AP-113.pdf
Ralf Brown's Interrupt List, DOS UMB linking and memory-block layouts:
https://fd.lod.bz/rbil/interrup/dos_kernel/2158.html
https://fd.lod.bz/rbil/interrup/dos_kernel/2152.html

BIOS POST version banner:
https://github.com/skiselev/8088_bios/blob/master/src/messages.inc
Trident BIOS memory query (TRIDENT.TXT / VGADOC3):
https://bearstrong.net/tekst97/data/programmering/vgadoc3/trident/
VBE 2.0 specification, controller information TotalMemory:
https://www.ele.uva.es/~jesman/BigSeti/ftp/Perifericos/Vesa/vbe_2_0.pdf
XTIDE r638 ROMVARS and controller-type definitions:
https://www.xtideuniversalbios.org/export/638/xtideuniversalbios/trunk/XTIDE_Universal_BIOS/Inc/RomVars.inc
https://www.xtideuniversalbios.org/export/638/xtideuniversalbios/trunk/XTIDE_Universal_BIOS/Inc/DeviceIDE.inc
BIOS disk geometry and EDD parameter interfaces:
https://fd.lod.bz/rbil/interrup/bios/1308.html
https://fd.lod.bz/rbil/interrup/io_disk/1348.html

No third-party source code is bundled. Implementation uses the documented
interfaces/instructions. The owner confirmed the final 1.3 build on the NuXT.
