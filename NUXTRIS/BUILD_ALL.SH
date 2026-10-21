#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${WATCOM:?Set WATCOM to your Open Watcom directory}"
(cd SOURCE/SP && ./BUILD.SH)
(cd SOURCE/MP && ./BUILD.SH)
(cd SOURCE/LAUNCH && ./BUILD.SH)
cp SOURCE/SP/NUXTRIS.EXE SP_TRIS.EXE
cp SOURCE/MP/NXTNET.EXE MP_TRIS.EXE
cp SOURCE/LAUNCH/NUXTRIS.EXE NUXTRIS.EXE
