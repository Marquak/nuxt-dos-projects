#!/bin/sh
# Run NuXTris LAN play in DOSBox-X with its NE2000/SLIRP adapter.
set -eu
game_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
peer_ip=${1:-192.168.50.8}
cycles=${NUXTRIS_CYCLES:-3000}
driver=${NUXTRIS_DRIVER:-/home/amigo/Downloads/NE2000.COM}

if ! printf '%s\n' "$peer_ip" | awk -F. 'NF==4 {
  for (i=1;i<=4;i++) if ($i !~ /^[0-9]+$/ || $i+0>255) exit 1;
  ok=1
} END { if (!ok) exit 1 }'; then
    echo "Give the NuXT's IPv4 address, for example: $0 192.168.50.8" >&2
    exit 2
fi
case $cycles in *[!0-9]*|'') echo 'NUXTRIS_CYCLES must be a positive number.' >&2; exit 2;; esac
if [ "$cycles" -eq 0 ]; then echo 'NUXTRIS_CYCLES must be positive.' >&2; exit 2; fi
if [ ! -f "$driver" ]; then
    echo "Packet driver not found: $driver" >&2
    echo 'Set NUXTRIS_DRIVER to your NE2000.COM path.' >&2
    exit 1
fi
if [ ! -f "$game_dir/NXTNET.EXE" ]; then
    echo "NXTNET.EXE not found in $game_dir" >&2
    exit 1
fi

dos_dir="$game_dir/DOSBOX-LAN"
mkdir -p "$dos_dir"
cp "$game_dir/NXTNET.EXE" "$dos_dir/NXTNET.EXE"
cat > "$dos_dir/MTCP.CFG" <<'EOF'
PACKETINT 0x60
IPADDR 10.0.2.15
NETMASK 255.255.255.0
GATEWAY 10.0.2.2
EOF

driver_dir=$(CDPATH= cd -- "$(dirname -- "$driver")" && pwd)
driver_file=$(basename -- "$driver")
conf=$(mktemp /tmp/nuxtris-lanbox.XXXXXX.conf)
trap 'rm -f "$conf"' EXIT HUP INT TERM
cat > "$conf" <<EOF
[sdl]
output=surface
[dosbox]
machine=svga_s3
memsize=4
[cpu]
core=normal
cycles=fixed $cycles
[ne2000]
ne2000=true
nicbase=300
nicirq=10
backend=slirp
[serial]
serial1=disabled
serial2=disabled
[autoexec]
mount c "$dos_dir"
mount d "$driver_dir"
c:
set MTCPCFG=C:\\MTCP.CFG
d:\\$driver_file 0x60 10 0x300
NXTNET /LAN /JOIN /IP:$peer_ip
EOF
echo "DOSBox-X joins NuXTris at $peer_ip. Start HOST GAME on the NuXT."
dosbox-x -conf "$conf" -fastlaunch
