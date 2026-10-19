#!/bin/sh
# SPDX-FileCopyrightText: 2026 Joshua Jun
# SPDX-License-Identifier: Apache-2.0
set -eu
cd "$(dirname "$0")/.."
cc=${ARM_CC:-arm-none-eabi-gcc}
size=${ARM_SIZE:-arm-none-eabi-size}
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT HUP INT TERM
for entries in 0 64; do
  for cpu in cortex-m4 arm7tdmi; do
    mode=-mthumb
    if [ "$cpu" = arm7tdmi ]; then mode=-marm; fi
    directory="$build/$cpu-$entries"
    mkdir "$directory"
    echo "$cpu: SAMSA_CACHE_ENTRIES=$entries"
    for source in src/*.c; do
      "$cc" -std=c11 -Os -ffreestanding -Wall -Wextra -Werror -Iinclude \
        -DSAMSA_CACHE_ENTRIES="$entries" -mcpu="$cpu" "$mode" -c "$source" -o "$directory/$(basename "$source" .c).o"
    done
    "$size" -t "$directory/"*.o
    if [ "$cpu" = cortex-m4 ]; then
      text=$("$size" -t "$directory/"*.o | awk '/TOTALS/ { print $1 }')
      test "$text" -lt 16384
    fi
  done
done
