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
.venv/bin/python tools/gen_arch_vectors.py            # full-state family cases
ruff check tools
ruff format --check tools
```

`test_vectors` requires and replays both checked-in corpora, `spec.bin` and
`unicorn.bin`, with 100,000 cases each. Unicorn 2.1.4 and the independent model
agree exactly on this sample, and both corpora pass on Samsa under ASan/UBSan.
The oracle CI job regenerates Unicorn vectors before running tests; ordinary
host CI does not install Python dependencies.

The initial sandbox blocked package downloads. After permission escalation
became available, installation succeeded. Unicorn also required an approved run
outside the macOS sandbox: inside it, native initialization stopped with SIGILL
in `init_cache_info` during `uc_mem_map`, before guest execution. Running the
same generator with escalation succeeded. This was an oracle-runtime restriction,
not an instruction-semantics disagreement.

The original sampler covers ALU forms. The full-state sampler additionally
covers memory/control instructions, IT masks, exclusive sequences, multiply/divide,
saturation, bitfields, high registers, SP transfers with defined aligned values,
and random encodings classified into unallocated spaces by A5. There are 100,000
full-state records across 301 named families. Four YIELD/WFE forms unsupported by
Unicorn use exact manual-derived expectations, counted separately in metadata.
All other valid cases require oracle execution; no whole families are skipped.

The replay compares all registers and mapped memory, both continuously and with
one-instruction budgets. See [the full-state format](vector-format.md) and
[the oracle disputes](../tests/vectors/disputes.md). Unclassified random encodings
remain in sanitizer/fuzzer testing, where no architectural equivalence is claimed.

`tools/build_excluded.py` regenerates the directed table of 90 excluded DSP
instructions using ARM GNU assembler. The core never executes or links those
instructions. Guard tests also reject every coprocessor first-halfword allocation
and unsupported MRS/MSR selectors, and check reserved multiply encoding bits.

Files use little-endian fixed-width integers: magic `SAMSV001`, count (u32),
then 24-byte records `(first:u16, second:u16, seed:u32, r0:u32, apsr:u32,
pc:u32, stop:u32)`. Each seed initializes r0-r14 with the LCG
`s = s * 1664525 + 1013904223`; SP is then aligned down to four bytes,
and the next word gives NZCVQ. The generator
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
and require the Cortex-M4 object text total below 16 KiB, with caching both
disabled and enabled (64 entries). These totals exclude
embedder libc/libgcc dependencies and linker overhead. Both builds passed
locally. Local Linux GCC/Clang container tests and native macOS GCC/Clang tests
also pass. The configured GitHub Actions workflow has not run remotely.

## Benchmark

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DSAMSA_BUILD_BENCH=ON
cmake --build build-release
build-release/samsa_bench tests/programs/suite.bin
```

This runs compiled programs and counts actual guest instructions. Record baseline
and later optimized results in commit messages. The baseline precedes the optional cache optimization. Compare identical
compiler, build type, workload and host; sanitizer throughput is not comparable.

The host CI matrix tests cache sizes 0 and 64. Sanitizers and libFuzzer also run
with 64 entries. Directed cache tests cover host invalidation, overlapping guest
stores, permissions on both halfwords, mapping changes, collisions, PC zero,
and independent CPUs. Local sanitizer runs additionally test sizes 1 and 1024.
When replacing a guest program in host memory, invalidate before reusing a CPU.

For reproducible local Linux checks on another host:

```sh
docker build -t samsa-ci -f tools/ci/Dockerfile .
docker run --rm --mount type=bind,src="$PWD",dst=/src,readonly samsa-ci sh -c '
  cmake -S /src -B /tmp/build -DCMAKE_C_COMPILER=clang -DSAMSA_SANITIZE=ON -DSAMSA_CACHE_ENTRIES=64 &&
  cmake --build /tmp/build --parallel && ctest --test-dir /tmp/build --output-on-failure
'
```

The implementation retains compact eager NZCVQ flags. The measured optimization
changes instruction dispatch, fetch and region lookup; benchmark commit messages
record cache sizes and before/after measurements. Cache benefit depends on the
workload and host, so it is optional.
