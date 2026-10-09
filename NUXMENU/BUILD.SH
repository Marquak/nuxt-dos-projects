#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${WATCOM:?Set WATCOM to Open Watcom}"
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH" INCLUDE="$WATCOM/h"
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM
wcl -q -cc -za99 -bt=dos -0 -ms -os -s -fpc -k2048 -w4 -we -fo="$build_dir/" -fe=MENU.EXE -fm="$build_dir/launcher.map" LAUNCH.C
wcl -q -cc -za99 -bt=dos -0 -ms -os -s -fpc -k4096 -w4 -we -fo="$build_dir/" -fe=NUI.EXE -fm="$build_dir/ui.map" UI.C DATA.C
if [ -n "${MENU_BUILD_REPORTS:-}" ]; then
 mkdir -p "$MENU_BUILD_REPORTS"
 cp "$build_dir/launcher.map" "$build_dir/ui.map" "$MENU_BUILD_REPORTS/"
fi
wc -c MENU.EXE NUI.EXE
