#!/bin/sh
# Linux build: GNU binutils (as, ld and objcopy). No DOS compiler required.
set -eu
cd "$(dirname "$0")"
build_tmp=$(mktemp -d)
trap 'rm -rf "$build_tmp"' EXIT HUP INT TERM
as --32 -o "$build_tmp/textmode.o" TEXTMODE.S
ld --build-id=none -m elf_i386 -Ttext 0x100 -e _start \
  -o "$build_tmp/textmode.elf" "$build_tmp/textmode.o"
objcopy -O binary -j .text "$build_tmp/textmode.elf" TEXTMODE.COM
