#!/bin/sh
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
: "${WATCOM:?Set WATCOM to Open Watcom}"
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH" INCLUDE="$WATCOM/h"
run=$(mktemp -d "${TMPDIR:-/tmp}/nuxtmenu-test.XXXXXX")
wcl -q -cc -bt=dos -0 -ms -os -s -fpc -k2048 -w4 -we \
 "-i=$source_dir" "-fo=$run/" "-fe=$run/TEST.EXE" "$source_dir/tests/DATATEST.C" "$source_dir/DATA.C"
cat > "$run/RUN.BAT" <<'BAT'
@echo off
TEST.EXE > RESULT.TXT
if errorlevel 1 goto failed
echo PASS > STATUS.TXT
goto done
:failed
echo FAIL > STATUS.TXT
:done
BAT
cat > "$run/dosbox.conf" <<CONF
[sdl]
output=surface
[cpu]
cycles=fixed 3000
[mixer]
nosound=true
[midi]
mididevice=none
[autoexec]
mount c "$run"
c:
call RUN.BAT
exit
CONF
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout -k 5 30 dosbox -conf "$run/dosbox.conf" > "$run/console.txt" 2>&1
cat "$run/RESULT.TXT"
printf 'Test reports: %s\n' "$run"
[ "$(tr -d '\r\n ' < "$run/STATUS.TXT")" = PASS ]
