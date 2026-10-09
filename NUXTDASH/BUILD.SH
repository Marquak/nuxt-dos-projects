#!/bin/sh
# Reproducible Linux build with GNU binutils; no DOS runtime library required.
set -eu
cd "$(dirname "$0")"
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM
as --32 -o "$build_dir/dash.o" DASH.S
ld -m elf_i386 -T COM.LD -o "$build_dir/dash.elf" "$build_dir/dash.o"
objcopy -O binary -j .text -j .data "$build_dir/dash.elf" DASH.COM
wc -c DASH.COM
