#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${WATCOM:?Set WATCOM to the Open Watcom directory}"
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH"
export INCLUDE="$WATCOM/h"
wasm -q -0 -fp0 -fpi87 -fo=math87.o MATH87.ASM
wcl -q -cc -za99 -bt=dos -0 -ms -ot -s -fpc -k8192 -w4 -fe=ORBIT87.EXE -fm=ORBIT87.MAP MAIN.C VIDEO.C MATH.C MUSIC.C math87.o
wcl -q -cc -za99 -bt=dos -1 -dV20_BUILD -ms -ot -s -fpc -k8192 -w4 -fe=ORBITV20.EXE -fm=ORBITV20.MAP MAIN.C VIDEO.C MATH.C MUSIC.C math87.o
rm -f MAIN.o VIDEO.o MATH.o MUSIC.o MAIN.obj VIDEO.obj MATH.obj MUSIC.obj math87.o
