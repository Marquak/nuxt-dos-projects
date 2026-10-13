#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${WATCOM:?Set WATCOM to Open Watcom}"
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH" INCLUDE="$WATCOM/h"
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM
as --32 -o "$build_dir/launcher.o" LAUNCH.S
ld -m elf_i386 -T LAUNCH.LD -o "$build_dir/launcher.elf" "$build_dir/launcher.o"
objcopy -O binary -j .text "$build_dir/launcher.elf" "$build_dir/launcher.bin"
python3 - "$build_dir/launcher.bin" <<'PYBUILD'
import struct,sys
from pathlib import Path
body=Path(sys.argv[1]).read_bytes()
size=32+len(body)
# A nonzero MAXALLOC avoids DOS's special load-high placement. The launcher
# immediately shrinks its PSP block to the image plus its 512-byte stack.
header=struct.pack('<16H',0x5a4d,size%512,(size+511)//512,0,2,0,65535,0,len(body),0,0,0,28,0,0,0)
Path('MENU.EXE').write_bytes(header+body)
PYBUILD
wcl -q -cc -za99 -bt=dos -0 -ms -os -s -fpc -k4096 -w4 -we -fo="$build_dir/" -fe=NUI.EXE -fm="$build_dir/ui.map" UI.C DATA.C
if [ -n "${MENU_BUILD_REPORTS:-}" ]; then
 mkdir -p "$MENU_BUILD_REPORTS"
 cp "$build_dir/ui.map" "$build_dir/launcher.elf" "$MENU_BUILD_REPORTS/"
fi
wc -c MENU.EXE NUI.EXE
