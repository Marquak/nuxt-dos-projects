#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${WATCOM:?Set WATCOM to your Open Watcom directory}"
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH"
export INCLUDE="$WATCOM/h"
task_build_flag=
task_build_name=NUXTRIS
case "${1:-}" in
    --test) task_build_flag=-dSELFTEST; task_build_name=CHECK ;;
    "") ;;
    *) echo 'Usage: BUILD.SH [--test]' >&2; exit 2 ;;
esac
wasm -q -1 -fo=render.obj RENDER.ASM
wcl -q -cc -za99 -bt=dos -1 -ms -ot -oi -s -fpc -k4096 -w4 \
    $task_build_flag -fe="$task_build_name.EXE" -fm="$task_build_name.MAP" \
    GAME.C render.obj
rm -f GAME.o GAME.obj render.obj
