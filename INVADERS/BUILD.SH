#!/bin/sh
set -eu
cd "$(dirname "$0")"
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM
as --32 -o "$build_dir/game.o" GAME.S
ld -m elf_i386 -T COM.LD -o "$build_dir/game.elf" "$build_dir/game.o"
objcopy -O binary -j .text -j .data "$build_dir/game.elf" INVADERS.COM
wc -c INVADERS.COM
