#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${WATCOM:?Set WATCOM to Open Watcom}"
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH" INCLUDE="$WATCOM/h"
build_name=CHECK88
build_flag=
case "${1:-}" in
  --test) build_name=TEST88; build_flag=-dSELFTEST ;;
  "") ;;
  *) echo 'Usage: BUILD.SH [--test]' >&2; exit 2 ;;
esac
wasm -q -1 -fo=render.obj RENDER.ASM
wasm -q -0 -fp0 -fpi87 -fo=math87.obj MATH87.ASM
wcl -q -cc -za99 -bt=dos -1 -ms -ot -oi -s -fpc -k8192 -w4 \
  $build_flag -fe="$build_name.EXE" -fm="$build_name.MAP" GAME.C render.obj math87.obj
rm -f GAME.o GAME.obj render.obj math87.obj
