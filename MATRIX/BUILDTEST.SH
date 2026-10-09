#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${WATCOM:?Set WATCOM to Open Watcom}"
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH"
export INCLUDE="$WATCOM/h"
for test_name in BENCH CHECK FPUTEST; do
  wcl -q -cc -za99 -bt=dos -0 -ms -os -s -fpc -k4096 -w4 -fe="$test_name.EXE" "tests/$test_name.C" FPU.C
  rm -f "$test_name.o" "$test_name.obj" FPU.o FPU.obj
done
