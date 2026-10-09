#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${WATCOM:?Set WATCOM to Open Watcom}"
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH"
export INCLUDE="$WATCOM/h"
wasm -q -0 -fp0 -fpi87 -fo=math87.o MATH87.ASM
wasm -q -1 -fo=fastline.o FASTLINE.ASM
wcl -q -cc -za99 -bt=dos -1 -ms -ot -s -fpc -k8192 -w4 -fe=SUNSET.EXE -fm=SUNSET.MAP MAIN.C ART.C TILES.C FAST320.C VIDEO.C PLANAR.C MATH.C MUSIC.C math87.o fastline.o
rm -f MAIN.o ART.o TILES.o FAST320.o VIDEO.o PLANAR.o MATH.o MUSIC.o math87.o fastline.o
