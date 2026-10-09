#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${WATCOM:?Set WATCOM to Open Watcom}"
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH"
export INCLUDE="$WATCOM/h"
wasm -q -0 -fp0 -fpi87 -fo=math87.obj MATH87.ASM
wcl -q -cc -za99 -bt=dos -1 -ms -ot -s -fpc -k4096 -w4 -fe=CHECK.EXE CHECK.C math87.obj
rm -f CHECK.o math87.obj
