<!-- SPDX-FileCopyrightText: 2026 Joshua Jun -->
<!-- SPDX-License-Identifier: Apache-2.0 -->
# Validation and regeneration

## Ordinary tests

The C test executables use only the checked-in binaries and a minimal harness.
`test_programs` compiles the same freestanding suite natively and compares output
words against guest execution for 32 seeds. It exercises sorting, CRC32, strings,
a switch state machine, signed/unsigned 64-bit division, floating-point add,
multiply, conversion, double division, and iterative square root. The separate
hook fixture reaches four nested hook/guest callback levels.

`test_random` executes 300,000 deterministic bounded instruction/state cases,
including every first halfword. It checks termination and memory safety under
sanitizers, not architectural equivalence. The fuzzer has the same purpose:

```sh
CC=clang cmake -S . -B build-fuzz -DSAMSA_BUILD_FUZZ=ON -DSAMSA_BUILD_TESTS=OFF
cmake --build build-fuzz
build-fuzz/samsa_fuzz -runs=100000 -max_len=1100
```

A compiler distribution with libFuzzer's runtime is required. Apple's command
line tools in the initial environment lack `libclang_rt.fuzzer_osx.a`. A cached
upstream LLVM runtime was linked separately for 100,000 local fuzz runs. CI uses
upstream Linux Clang directly.

## Vectors

```sh
python3 -m venv .venv  # Python >= 3.10
.venv/bin/pip install unicorn==2.1.4 ruff==0.14.0
.venv/bin/python tools/gen_vectors.py                 # Unicorn, 100,000 cases
.venv/bin/python tools/gen_vectors.py --producer spec # independent integer model
ruff check tools
ruff format --check tools
```

`test_vectors` always replays `spec.bin` and replays `unicorn.bin` when present.
A missing Unicorn corpus is printed explicitly. The oracle CI job generates it
before running tests; ordinary host CI does not install Python dependencies.
The initial checkout contains only the spec corpus, because installing Unicorn
was blocked by network/sandbox policy. No successful oracle result is claimed.

The sampler currently covers valid low-register Thumb-16 ALU/shift forms,
Thumb-32 shifted-register and modified-immediate ALU forms, and MOVW/MOVT. It does **not** yet provide
the requested class-by-class memory/control oracle coverage or random-halfword
architectural classification. Unclassified halfwords stay in safety testing so
UNPREDICTABLE instructions cannot silently become false correctness references.
Expansion is tracked in `tests/coverage.md`. Resolve disagreements against the
ARM manual before adding an entry to `tests/vectors/disputes.md`; never add a
blanket skip for an instruction family.

Files use little-endian fixed-width integers: magic `SAMSV001`, count (u32),
then 24-byte records `(first:u16, second:u16, seed:u32, r0:u32, apsr:u32,
pc:u32, stop:u32)`. Each seed initializes r0-r14 with the LCG
`s = s * 1664525 + 1013904223`; the next word gives NZCVQ. The generator
checks that the other registers remain unchanged. JSON records producer,
version, seed, count, and SHA-256. The replay verifies every other register too.
There is no dependency on C struct padding or Python at replay time.

## Compiled programs and portability

Put ARM GCC and binutils in PATH (or set ARM_CC, ARM_OBJCOPY, ARM_SIZE):

```sh
tools/build_programs.sh
tools/check_portability.sh
```

The fixture build is `-mcpu=cortex-m3 -mthumb -Os -fPIE -mfloat-abi=soft
-nostdlib`, linking libgcc. A linker script fixes the fixture entry at `0x1000`
and the external hook at `0x3001`. There is no ELF parsing in Samsa. Regenerating
with another compiler can change bytes; verify behavior instead of requiring
byte identity across GCC releases. Initial fixtures use GCC 15.2.1.

Portability checks compile all core sources with `-std=c11 -Os -ffreestanding
-Wall -Wextra -Werror` for Cortex-M4 Thumb and ARM7TDMI ARM, print size totals,
and require the Cortex-M4 object text total below 16 KiB. These totals exclude
embedder libc/libgcc dependencies and linker overhead. Both builds passed
locally. Linux/GCC and the CI matrix are configured but not yet run remotely.

## Benchmark

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DSAMSA_BUILD_BENCH=ON
cmake --build build-release
build-release/samsa_bench tests/programs/suite.bin
```

This runs compiled programs and counts actual guest instructions. Record baseline
and later optimized results in commit messages. No lazy-flag or decode-cache
optimization has been attempted: milestone 7 waits for oracle validation and
the complete correctness audit.
