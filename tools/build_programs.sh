#!/bin/sh
# SPDX-FileCopyrightText: 2026 Joshua Jun
# SPDX-License-Identifier: Apache-2.0
set -eu
cd "$(dirname "$0")/.."
cc=${ARM_CC:-arm-none-eabi-gcc}
objcopy=${ARM_OBJCOPY:-arm-none-eabi-objcopy}
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT HUP INT TERM
for name in suite hooks; do
  entry=program
  if [ "$name" = hooks ]; then entry=guest_callback; fi
  "$cc" -std=c11 -mcpu=cortex-m3 -mthumb -mfloat-abi=soft -Os -fPIE \
    -ffreestanding -fno-builtin -ffp-contract=off -nostdlib -Wall -Wextra -Werror \
    -Wl,-T,tests/programs/link.ld,-e,"$entry",--build-id=none \
    "tests/programs/$name.c" -lgcc -o "$build/$name.elf"
  "$objcopy" -O binary "$build/$name.elf" "tests/programs/$name.bin"
  chmod 644 "tests/programs/$name.bin"
done
