#!/bin/sh
set -eu
cd "$(dirname "$0")"
tmp=$(mktemp ./_heatshrink.so.XXXXXX)
trap 'rm -f "$tmp"' EXIT HUP INT TERM
${CC:-cc} -std=c99 -O2 -Wall -Wextra -Werror -Wno-implicit-fallthrough -fPIC -shared \
    -I../third_party/heatshrink heatshrink_wrap.c \
    ../third_party/heatshrink/heatshrink_encoder.c -o "$tmp"
chmod 755 "$tmp"
mv "$tmp" _heatshrink.so
