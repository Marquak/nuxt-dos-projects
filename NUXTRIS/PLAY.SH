#!/bin/sh
set -eu
game_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
command -v dosbox-x >/dev/null 2>&1 || {
    echo 'DOSBox-X is not installed.' >&2
    exit 1
}
cycles=${NUXTRIS_CYCLES:-3000}
case $cycles in *[!0-9]*|'') echo 'NUXTRIS_CYCLES must be positive.' >&2; exit 2;; esac
if [ "$cycles" -eq 0 ]; then echo 'NUXTRIS_CYCLES must be positive.' >&2; exit 2; fi
for name in NUXTRIS SP_TRIS MP_TRIS; do
    if [ ! -f "$game_dir/$name.EXE" ]; then
        echo "$name.EXE is missing from $game_dir" >&2
        exit 1
    fi
done
dos_dir="$game_dir/DOSBOX"
mkdir -p "$dos_dir"
for name in NUXTRIS SP_TRIS MP_TRIS; do
    cp "$game_dir/$name.EXE" "$dos_dir/$name.EXE"
done
conf=$(mktemp /tmp/nuxtris-selector.XXXXXX.conf)
trap 'rm -f "$conf"' EXIT HUP INT TERM
cat > "$conf" <<CONFIG
[sdl]
output=surface
[dosbox]
machine=svga_s3
memsize=4
[cpu]
core=normal
cycles=fixed $cycles
[autoexec]
mount c "$dos_dir"
c:
NUXTRIS
exit
CONFIG
dosbox-x -conf "$conf" -fastlaunch
