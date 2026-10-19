#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${WATCOM:?Set WATCOM to your Open Watcom installation}"
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH"
export INCLUDE="$WATCOM/h"
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM
for unit in CRITICAL CRC HSDEC; do
    wasm -q -0 -mt -fo="$build_dir/$unit.o" "$unit.ASM"
done
wcl -q -cc -bt=dos -0 -mt -l=dos -os -s -fpc -k4096 -w4 -we -dNET_BUILD \
    -fo="$build_dir/" -fe="$build_dir/NUXNET.EXE" \
    NUXFER.C NET.C DIRMETA.C "$build_dir/CRITICAL.o" "$build_dir/CRC.o" \
    "$build_dir/HSDEC.o"
cp "$build_dir/NUXNET.EXE" ../NUXNET.EXE
wc -c ../NUXNET.EXE
