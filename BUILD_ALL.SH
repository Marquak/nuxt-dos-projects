#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${WATCOM:?Set WATCOM to your Open Watcom installation}"
export WATCOM
for project in NUXMENU ORBIT87 STARFIELD MATRIX INVADERS SUNSET NUXTDASH NUXTROIDS NUXTRIS CHECKERS88; do
    printf '\nBuilding %s\n' "$project"
    (cd "$project" && sh BUILD.SH)
done
(cd TEXTMODE && sh BUILD.SH)
(cd NUXTLINK && sh dos/BUILD.SH)
(cd NUXTLINK && sh linux/build_codec.sh)
