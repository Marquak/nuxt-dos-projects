#!/bin/sh
set -eu
cd "$(dirname "$0")"
${CC:-cc} -std=c99 -O3 -Wall -Wextra -Werror -fPIC -shared \
    bsum_native.c -o _bsum.so
