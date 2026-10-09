#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${WATCOM:?Set WATCOM to Open Watcom}"
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH"
export INCLUDE="$WATCOM/h"
wcl -q -cc -za99 -bt=dos -0 -ms -os -s -fpc -k4096 -w4 -fe=MATRIX.EXE -fm=MATRIX.MAP MATRIX.C FPU.C
rm -f MATRIX.o MATRIX.obj FPU.o FPU.obj
