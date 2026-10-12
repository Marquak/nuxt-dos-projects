#!/bin/sh
# Host music checks plus isolated V20/8087 DOS test builds and DOSBox-X runs.
# Usage: WATCOM=/path/to/watcom ./TEST.SH [--build-only] [work-directory]
# Every generated file stays in a fresh run directory below work-directory
# (default: ${TMPDIR:-/tmp}/sunset-tests). No QEMU or 86Box is required.
set -eu

source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build_only=0
work_parent=${TMPDIR:-/tmp}/sunset-tests
have_directory=0
for arg in "$@"; do
    case "$arg" in
        --build-only) build_only=1 ;;
        --help|-h)
            printf '%s\n' 'Usage: WATCOM=/path/to/watcom ./TEST.SH [--build-only] [work-directory]'
            printf '%s\n' 'CC and DOSBOX may select the host compiler and DOSBox-X executable.'
            printf '%s\n' 'SUNSET_TEST_TIMEOUT sets the per-case timeout in seconds (default 180).'
            exit 0 ;;
        --*) printf 'Unknown option: %s\n' "$arg" >&2; exit 2 ;;
        *)
            if [ "$have_directory" -ne 0 ]; then
                printf '%s\n' 'Only one work-directory may be supplied.' >&2
                exit 2
            fi
            work_parent=$arg; have_directory=1 ;;
    esac
done

: "${WATCOM:?Set WATCOM to the Open Watcom installation}"
WATCOM=$(CDPATH= cd -- "$WATCOM" && pwd)
export WATCOM
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH"
export INCLUDE="$WATCOM/h"
host_cc=${CC:-cc}
dosbox=${DOSBOX:-dosbox-x}
test_timeout=${SUNSET_TEST_TIMEOUT:-180}
case "$test_timeout" in
    ''|*[!0-9]*|0) printf '%s\n' 'SUNSET_TEST_TIMEOUT must be a positive integer.' >&2; exit 2 ;;
esac
for program in "$host_cc" wcc wcl wasm; do
    command -v "$program" >/dev/null 2>&1 || {
        printf 'Required program not found: %s\n' "$program" >&2; exit 2;
    }
done
if [ "$build_only" -eq 0 ]; then
    for program in "$dosbox" timeout; do
        command -v "$program" >/dev/null 2>&1 || {
            printf 'Required program not found: %s (or use --build-only)\n' "$program" >&2
            exit 2
        }
    done
fi

mkdir -p -- "$work_parent"
work_parent=$(CDPATH= cd -- "$work_parent" && pwd)
case "$work_parent/" in
    "$source_dir/"*) printf '%s\n' 'Choose a work-directory outside the SUNSET source directory.' >&2; exit 2 ;;
esac
# DOSBox's mount syntax quotes host paths but cannot escape a literal quote.
case "$work_parent" in
    *'"'*) printf '%s\n' 'The work-directory cannot contain a double quote.' >&2; exit 2 ;;
esac
run_dir=$(mktemp -d "$work_parent/run.XXXXXX")
mkdir "$run_dir/BIN" "$run_dir/objects"
printf 'Test output: %s\n' "$run_dir"

"$host_cc" -std=c99 -O2 -Wall -Wextra -Werror -DHOST -x c \
    "$source_dir/MUSIC.C" "$source_dir/tests/MUSTEST.C" \
    -o "$run_dir/music-host"
"$run_dir/music-host" "$run_dir/music-registers.csv" >"$run_dir/HOST.TXT"
cat "$run_dir/HOST.TXT"

# Compile in the private directory, using explicit object paths. The source
# directory receives neither object files nor linker scratch files.
cd "$run_dir/objects"
wasm -q -0 -fp0 -fpi87 -fo=math87.o "$source_dir/MATH87.ASM"
wasm -q -1 -fo=fastline.o "$source_dir/FASTLINE.ASM"
for unit in MATH ART VIDEO PLANAR TILES FAST320 MUSIC; do
    wcc -q -za99 -bt=dos -1 -ms -ot -s -fpc -w4 \
        "-i=$source_dir" "-fo=$run_dir/objects/$unit.o" "$source_dir/$unit.C"
done
for unit in MATHTEST ARTTEST VIDTEST TILETEST MUSIRQ PLANETST FASTTEST; do
    wcc -q -za99 -bt=dos -1 -ms -ot -s -fpc -w4 \
        "-i=$source_dir" "-fo=$run_dir/objects/$unit.o" "$source_dir/tests/$unit.C"
done
link_test() {
    name=$1; shift
    wcl -q -bt=dos -1 -ms -s -fpc -k8192 \
        "-fe=$run_dir/BIN/$name.EXE" "$name.o" "$@"
}
link_test MATHTEST MATH.o math87.o fastline.o
link_test ARTTEST ART.o
link_test VIDTEST VIDEO.o PLANAR.o
link_test TILETEST TILES.o FAST320.o fastline.o VIDEO.o PLANAR.o
link_test FASTTEST TILES.o FAST320.o fastline.o VIDEO.o PLANAR.o
link_test MUSIRQ MUSIC.o
link_test PLANETST VIDEO.o PLANAR.o
printf '%s\n' 'Built seven DOS test executables with V20/80186 C and original-8087 assembly.'
if [ "$build_only" -eq 1 ]; then
    printf 'DOS executables: %s/BIN\n' "$run_dir"
    printf '%s\n' 'Run MATHTEST, MATHTEST /NO87, ARTTEST, VIDTEST 0/1/2, PLANETST, TILETEST 0/1, TILETEST, FASTTEST, and MUSIRQ in DOS.'
    exit 0
fi

run_case() {
    case_name=$1; executable=$2; argument=$3; log_name=$4; cycles=$5; sound=$6
    case_dir=$run_dir/$case_name
    mkdir "$case_dir"
    # Branch before redirecting: some DOSBox shells create a redirection
    # target even when an IF condition is false. RESULT therefore contains
    # an explicit status, rather than relying on marker-file existence.
    {
        printf '%s\r\n' '@echo off'
        printf 'C:\\BIN\\%s.EXE %s\r\n' "$executable" "$argument"
        printf '%s\r\n' 'if errorlevel 1 goto failed' 'echo PASS>RESULT.TXT' \
            'goto done' ':failed' 'echo FAIL>RESULT.TXT' ':done'
    } >"$case_dir/RUNTEST.BAT"
    cat >"$case_dir/dosbox.conf" <<EOF
[sdl]
fullscreen=false
output=surface
[dosbox]
machine=svga_s3
memsize=4
[cpu]
core=normal
cputype=auto
cycles=fixed $cycles
[mixer]
nosound=true
[sblaster]
sbtype=$sound
oplmode=opl2
oplrate=44100
[midi]
mididevice=none
[autoexec]
mount c "$run_dir"
c:
cd $case_name
call RUNTEST.BAT
exit
EOF
    if SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
        timeout -k 5 "$test_timeout" "$dosbox" -conf "$case_dir/dosbox.conf" \
        >"$case_dir/dosbox.log" 2>&1; then
        status=$(tr -d '\r\n ' <"$case_dir/RESULT.TXT" 2>/dev/null || true)
        if [ "$status" = PASS ] && [ -s "$case_dir/$log_name" ]; then
            printf 'PASS %s: %s/%s\n' "$case_name" "$case_dir" "$log_name"
            return 0
        fi
    fi
    printf 'FAIL %s: inspect %s\n' "$case_name" "$case_dir" >&2
    if [ -f "$case_dir/$log_name" ]; then cat "$case_dir/$log_name" >&2; fi
    return 1
}

# Fresh emulator sessions also isolate video-mode state and every report.
# The IRQ case uses a deliberately slower emulated CPU; the long exhaustive
# pixel/projection suites use a faster setting to keep test runs practical.
dos_failures=0
run_case MATH87 MATHTEST '' MATHTEST.TXT 100000 none || dos_failures=$((dos_failures+1))
run_case MATHINT MATHTEST /NO87 MATHTEST.TXT 100000 none || dos_failures=$((dos_failures+1))
run_case ART ARTTEST '' ARTTEST.TXT 100000 none || dos_failures=$((dos_failures+1))
run_case VIDEO320 VIDTEST 2 VIDTEST.TXT 100000 none || dos_failures=$((dos_failures+1))
run_case PLANAR PLANETST '' PLANETST.TXT 100000 none || dos_failures=$((dos_failures+1))
run_case VIDEO640 VIDTEST 0 VIDTEST.TXT 100000 none || dos_failures=$((dos_failures+1))
run_case VIDEO800 VIDTEST 1 VIDTEST.TXT 100000 none || dos_failures=$((dos_failures+1))
run_case FAST320 FASTTEST '' FASTTEST.TXT 100000 none || dos_failures=$((dos_failures+1))
run_case TILE640 TILETEST 0 TILETEST.TXT 100000 none || dos_failures=$((dos_failures+1))
run_case TILE800 TILETEST 1 TILETEST.TXT 100000 none || dos_failures=$((dos_failures+1))
# One process switches 640 -> text -> 800, covering mode-settle regressions.
run_case TILEBOTH TILETEST '' TILETEST.TXT 100000 none || dos_failures=$((dos_failures+1))
run_case IRQ MUSIRQ '' MUSIRQ.TXT 1500 sbpro2 || dos_failures=$((dos_failures+1))
printf 'DOS cases: 12 run, %s failed. Reports: %s\n' "$dos_failures" "$run_dir"
[ "$dos_failures" -eq 0 ]
