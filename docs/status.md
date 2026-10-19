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
| 7: performance | Baseline benchmark recorded; optional cache work in progress |

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
