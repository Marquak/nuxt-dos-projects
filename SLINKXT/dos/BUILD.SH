#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${WATCOM:?Set WATCOM to your Open Watcom installation}"
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH"
export INCLUDE="$WATCOM/h"
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM
wasm -q -0 -mt -fo="$build_dir/CRITICAL.o" CRITICAL.ASM
wasm -q -0 -mt -fo="$build_dir/UART.o" UART.ASM
for unit in BSUM HSDEC UARTTX JIMRAW; do
    wasm -q -0 -mt -fo="$build_dir/$unit.o" "$unit.ASM"
done
wcl -q -cc -bt=dos -0 -mt -l=com -os -s -fpc -k4096 -w4 -we \
    -fo="$build_dir/" -fe="$build_dir/SLINK.COM" \
    SLINK.C DIRMETA.C LZ4VALID.C \
    "$build_dir/CRITICAL.o" "$build_dir/UART.o" "$build_dir/BSUM.o" \
    "$build_dir/HSDEC.o" "$build_dir/UARTTX.o" "$build_dir/JIMRAW.o"
cp "$build_dir/SLINK.COM" ../SLINK.COM
wc -c ../SLINK.COM
