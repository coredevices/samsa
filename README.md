<!-- SPDX-FileCopyrightText: 2026 Joshua Jun -->
<!-- SPDX-License-Identifier: Apache-2.0 -->
# Samsa

A small, embeddable C11 interpreter for little-endian ARMv7-M Thumb code.
Named after Gregor Samsa from Kafka's *The Metamorphosis*: code wakes up on
an unfamiliar CPU.

Samsa supplies execution, memory permissions, hooks, and host/guest calls.
The embedder supplies the bytes, memory map, stack, and host functions.
There is no loader, allocator, OS, exception controller, FPU, ARM/A32, or DSP
extension. The core uses only the four permitted standard headers and has
no global state. Each CPU is independent.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Use `-DSAMSA_SANITIZE=ON` for ASan/UBSan, `-DSAMSA_BUILD_TESTS=OFF` for embedding,
and `-DSAMSA_BUILD_BENCH=ON` for the compiled-program benchmark. Link target
`samsa`, include `<samsa/samsa.h>`. Installation exports `samsa::samsa`.
No Python or ARM tools are needed to replay checked-in test data.

```c
uint8_t memory[4096] = { 0x07, 0x20, 0x00, 0xbe }; /* movs r0,#7; bkpt #0 */
SamsaRegion ram = { 0x1000, sizeof memory, memory, SAMSA_R | SAMSA_W | SAMSA_X };
SamsaCpu cpu;
samsa_reset(&cpu);
cpu.regions = &ram;
cpu.num_regions = 1;
cpu.r[15] = 0x1000;
cpu.r[13] = 0x2000;
SamsaStop why = samsa_run(&cpu, 100, NULL); /* BKPT, r0 == 7 */
```

See [API and embedding](docs/api.md), [architecture choices](docs/behavior.md),
[test-data regeneration](docs/testing.md), and [encoding coverage](tests/coverage.md).
The implementation follows the encodings and instruction semantics in
[ARM DDI 0403E.b, A5 and A7](https://documentation-service.arm.com/static/5f8fef3af86e16515cdbf816).
No emulator source was used.

## Current validation

The integer instruction families are implemented and tested with hand-written
cases, native-versus-guest C programs (including libgcc soft float and four
nested callbacks), 100,000 Unicorn ALU vectors, 100,000 independent model vectors,
and 100,000 full-state cases across 301 named encoding families. Four YIELD/WFE
forms use explicit manual-derived expectations because Unicorn rejects them;
the metadata counts these separately. The full-state replay compares every
register and mapped byte, both continuously and across instruction budgets.
Reserved encodings, excluded DSP/FPU classes, and unsupported system-register
selectors have directed rejection tests.

Local tests pass with GCC and Clang on macOS and Linux, including ASan/UBSan.
Bounded random testing covers 300,000 cases, plus 100,000 libFuzzer runs.
GCC cross-compiles warning-free for Cortex-M4 Thumb and ARM7TDMI ARM.
The CPU occupies 120 bytes on the tested 64-bit host; the portability script
checks Cortex-M4 object text against the 16 KiB target. See
[validation status](docs/status.md) for evidence and limits. No remote CI run
or physical hardware execution is claimed.

Apache-2.0. Copyright 2026 Joshua Jun.
