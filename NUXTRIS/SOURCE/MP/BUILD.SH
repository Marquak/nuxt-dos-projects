#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${WATCOM:?Set WATCOM to your Open Watcom directory}"
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH"
export INCLUDE="$WATCOM/h"
flags=
name=NXTNET
case "${1:-}" in --test) flags=-dLINKTEST; name=NETTEST ;; --menu-test) flags=-dMENUTEST; name=MENUTEST ;; "") ;; *) exit 2 ;; esac
wasm -q -1 -fo=render.obj RENDER.ASM
wcl -q -cc -za99 -bt=dos -1 -ms -ot -oi -s -fpc -k8192 -w4 $flags \
  -fe="$name.EXE" -fm="$name.MAP" MPLAY.C render.obj
rm -f MPLAY.o MPLAY.obj render.obj
