#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${WATCOM:?Set WATCOM to Open Watcom}"
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH"
export INCLUDE="$WATCOM/h"
wasm -q -0 -fp0 -fpi87 -fo=math87.o MATH87.ASM
wcl -q -cc -za99 -bt=dos -0 -ms -os -s -fpc -k4096 -w4 -fe=VIDTEST.EXE tests/VIDTEST.C VIDEO.C
wcl -q -cc -za99 -bt=dos -0 -ms -ot -s -fpc -k4096 -w4 -fe=MATHTEST.EXE tests/MATHTEST.C MATH.C math87.o
wcl -q -cc -za99 -bt=dos -0 -ms -ot -s -fpc -k4096 -w4 -fe=MATMUL.EXE tests/MATMUL.C
wcl -q -cc -za99 -bt=dos -0 -ms -ot -s -fpc -k4096 -w4 -fe=MUSIRQ.EXE tests/MUSIRQ.C MUSIC.C
wcl -q -cc -za99 -bt=dos -0 -ms -ot -s -fpc -k8192 -w4 -fe=SCENETST.EXE tests/SCENETEST.C VIDEO.C MATH.C MUSIC.C math87.o
wcl -q -cc -za99 -bt=dos -0 -ms -ot -s -fpc -k8192 -w4 -fe=PAGETEST.EXE tests/PAGETEST.C VIDEO.C MATH.C MUSIC.C math87.o
for cpu in 0 1; do
 wcl -q -cc -za99 -bt=dos -$cpu -ms -ot -s -fpc -k8192 -w4 -fe=BENCH$cpu.EXE tests/REVIEWBENCH.C VIDEO.C MATH.C MUSIC.C math87.o
done
rm -f math87.o REVIEWBENCH.o VIDTEST.o MATHTEST.o MATMUL.o MUSIRQ.o SCENETEST.o PAGETEST.o VIDEO.o MATH.o MUSIC.o
printf '%s\n' 'DOS test executables built. None is needed to run ORBIT87.EXE.'
