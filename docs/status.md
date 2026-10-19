<!-- SPDX-FileCopyrightText: 2026 Joshua Jun -->
<!-- SPDX-License-Identifier: Apache-2.0 -->
# Milestone status

| Milestone | Evidence / remaining work |
| --- | --- |
| 1: skeleton | Budget loop, memory, public API, CMake and initial tests pass |
| 2: Thumb-16 | A5 family checklist, directed pitfalls and full-state vectors pass |
| 3: Thumb-32 | Integer families, aliases, reserved/DSP/coprocessor guards pass |
| 4: oracle | 100,000 Unicorn ALU vectors and 100,000 full-state cases across 301 families; four documented manual hint forms |
| 5: embedding | Hooks, stops, bus, AAPCS slots, bounded calls, four callback levels, compiled libgcc soft float pass |
| 6: portability | Local Linux GCC/Clang and macOS GCC/Clang pass; host sanitizers, ARM cross-compiles and size checks pass |
| 7: performance | Optional predecode/region cache, guest-store coherence and host invalidation; before/after benchmarks recorded |

Cached and uncached host builds are validated separately. Additional sanitizer
runs cover the minimum and maximum supported cache sizes. The core retains
compact eager flags; no lazy flag representation is used.

Local host validation uses AppleClang 21 and Homebrew GCC 16 on macOS arm64,
and GCC 13 / Clang 18 in a disposable Ubuntu 24.04 arm64 container. The Dockerfile
is in `tools/ci/`; it mounts source read-only and builds in temporary storage.
ARM GNU cross-compilation checks Cortex-M4 Thumb and ARM7TDMI ARM with `-Os
-Wall -Wextra -Werror`. Compiler versions can change size totals.

The coverage checklist tracks encoding families, not every possible operand and
state combination. UNPREDICTABLE inputs are excluded from oracle comparisons;
see the vector metadata and disputes file. No remote CI run or physical hardware
execution is claimed. CI is configured and will run when the repository is
published; publishing has not been requested.

With ARM GCC 15.2.1 at `-Os`, object text totals are 7,042 bytes without a
cache and 7,476 bytes with 64 entries for Cortex-M4 Thumb. ARM7TDMI ARM totals
are 10,552 and 11,184 bytes respectively. These are core object totals, excluding
embedder libc/libgcc routines and final linker overhead. No writable global
storage is emitted. The 16 KiB target is enforced by the portability script.
