#!/bin/sh
set -eu
src=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
: "${WATCOM:?Set WATCOM to Open Watcom}"
sh "$src/BUILD.SH" --test
run=$(mktemp -d /tmp/checkers88-test.XXXXXX)
cp "$src/TEST88.EXE" "$run/"
cat > "$run/dosbox.conf" <<CONF
[sdl]
output=surface
waitonerror=false
[cpu]
cycles=fixed 12000
[serial]
serial1=disabled
serial2=disabled
serial3=disabled
serial4=disabled
[mixer]
nosound=false
[sblaster]
sbtype=sb1
oplmode=opl2
[midi]
mididevice=none
[autoexec]
mount c "$run"
c:
TEST88
exit
CONF
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout -k 5 60 dosbox-x -defaultconf -fastlaunch -silent -exit -nopromptfolder -nogui -conf "$run/dosbox.conf" > "$run/console.txt" 2>&1
cat "$run/RESULT.TXT"
printf 'Test files: %s\n' "$run"
grep -q 'checks, 0 failures' "$run/RESULT.TXT"
