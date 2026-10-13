#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${WATCOM:?Set WATCOM to your Open Watcom installation}"
export WATCOM
for project in NUXMENU ORBIT87 STARFIELD MATRIX INVADERS SUNSET NUXTDASH NUXTROIDS NUXTRIS; do
    printf '\nBuilding %s\n' "$project"
    (cd "$project" && sh BUILD.SH)
done
(cd NUXTLINK && sh dos/BUILD.SH)
cp NUXTLINK/dos/NUXFER.COM NUXTLINK/NUX321.COM
cp NUXTLINK/dos/NUXFER.COM NUXTLINK/NUXFER32.COM
cp NUXTLINK/dos/NUXFER.COM NUXTLINK/NUXFER.COM
