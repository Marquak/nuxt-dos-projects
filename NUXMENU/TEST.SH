#!/bin/sh
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
: "${WATCOM:?Set WATCOM to Open Watcom}"
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH" INCLUDE="$WATCOM/h"
run=$(mktemp -d "${TMPDIR:-/tmp}/nuxtmenu-test.XXXXXX")
wcl -q -cc -bt=dos -0 -ms -os -s -fpc -k2048 -w4 -we \
 "-i=$source_dir" "-fo=$run/" "-fe=$run/TEST.EXE" "$source_dir/tests/DATATEST.C" "$source_dir/DATA.C"
wcl -q -cc -za99 -bt=dos -0 -ms -os -s -fpc -k4096 -w4 -we \
 "-i=$source_dir" "-fo=$run/" "-fe=$run/UI.EXE" "$source_dir/tests/UITEST.C" "$source_dir/DATA.C"
cat > "$run/RUN.BAT" <<'BAT'
@echo off
TEST.EXE > RESULT.TXT
if errorlevel 1 goto failed
UI.EXE
if errorlevel 1 goto failed
echo PASS > STATUS.TXT
goto done
:failed
echo FAIL > STATUS.TXT
:done
exit
BAT
cat > "$run/dosbox.conf" <<CONF
[sdl]
output=surface
waitonerror=false
[cpu]
cycles=fixed 3000
[serial]
serial1=disabled
serial2=disabled
serial3=disabled
serial4=disabled
[mixer]
nosound=true
[midi]
mididevice=none
[autoexec]
mount c "$run"
c:
RUN.BAT
exit
CONF
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout -k 5 30 dosbox-x -defaultconf -fastlaunch -silent -exit -nopromptfolder -nogui -conf "$run/dosbox.conf" > "$run/console.txt" 2>&1
cat "$run/RESULT.TXT" "$run/UIRESULT.TXT"
printf 'Test reports: %s\n' "$run"
[ "$(tr -d '\r\n ' < "$run/STATUS.TXT")" = PASS ]
