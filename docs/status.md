<!-- SPDX-FileCopyrightText: 2026 Joshua Jun -->
<!-- SPDX-License-Identifier: Apache-2.0 -->
# Milestone status

| Milestone | Evidence / remaining work |
| --- | --- |
| 1: skeleton | Budget loop, memory, public API, CMake and initial tests pass locally |
| 2: Thumb-16 | Families implemented; direct tests and model vectors pass; encoding audit still needs completion |
| 3: Thumb-32 | Requested integer families implemented; direct and libgcc integration tests pass; exhaustive encoding audit remains |
| 4: oracle | 100,000 checked-in Unicorn 2.1.4 ALU vectors pass and match the independent model; sampler expansion needed |
| 5: embedding | Hooks, stops, bus, AAPCS slots, bounded calls, four callback levels, libgcc soft float pass locally |
| 6: portability | GCC ARM cross-compiles, host ASan/UBSan, and 100,000 libFuzzer runs pass; remote Linux/macOS gcc/clang CI pending |
| 7: performance | Benchmark provided; optimizations deferred until milestones 1-6 are complete |

No remote CI run or hardware execution is claimed. The library is a substantial
initial implementation awaiting oracle and per-encoding conformance validation.
