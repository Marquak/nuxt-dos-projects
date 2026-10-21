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
dos_dir="$game_dir/DOSBOX"
mkdir -p "$dos_dir"
cp "$game_dir/NUXTRIS.EXE" "$dos_dir/NUXTRIS.EXE"
conf=$(mktemp /tmp/nuxtris-sp.XXXXXX.conf)
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
