#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${WATCOM:?Set WATCOM to your Open Watcom directory}"
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH"
export INCLUDE="$WATCOM/h"
flag=
name=NUXTRIS
case "${1:-}" in
    "") ;;
    --test) flag=-dLAUNCH_TEST; name=CHECK ;;
    *) echo 'Usage: BUILD.SH [--test]' >&2; exit 2 ;;
esac
wasm -q -1 -fo=render.obj RENDER.ASM
wcl -q -cc -za99 -bt=dos -1 -ms -ot -oi -s -fpc -k2048 -w4 $flag \
    -fe="$name.EXE" -fm="$name.MAP" LAUNCH.C render.obj
rm -f LAUNCH.o LAUNCH.obj render.obj
