<!-- SPDX-FileCopyrightText: 2026 Joshua Jun -->
<!-- SPDX-License-Identifier: Apache-2.0 -->
# Oracle disputes and contract normalization

Unicorn version: 2.1.4, `UC_MODE_THUMB | UC_MODE_MCLASS`, Cortex-M3 CPU model.
The original 100,000 ALU vectors agree with both Samsa and the independent model.
The full-state corpus compares all registers, NZCVQ/ITSTATE, and all mapped
memory across 301 named families. It retains these exact manual-derived hint
expectations when Unicorn returns `UC_ERR_INSN_INVALID`:

| Encoding | Expected behavior | ARM reference | Reason |
| --- | --- | --- | --- |
| `BF10` YIELD T1 | Advance PC by 2, preserve state | DDI 0403E.b A7-562 | Unicorn Cortex-M3 rejects this valid hint |
| `F3AF 8001` YIELD T2 | Advance PC by 4, preserve state | DDI 0403E.b A7-562 | Unicorn Cortex-M3 rejects this valid hint |
| `BF20` WFE T1 | Advance PC by 2, preserve state | DDI 0403E.b A7-560 | Unicorn Cortex-M3 rejects this valid hint; Samsa specifies NOP |
| `F3AF 8002` WFE T2 | Advance PC by 4, preserve state | DDI 0403E.b A7-560 | Unicorn Cortex-M3 rejects this valid hint; Samsa specifies NOP |

These four operand-free encodings form the complete allow-list in
`gen_arch_vectors.py`. The generator records the exact number of manual
expectations per family in `architecture.json`; they are not silent oracle
agreements. Any other error in a valid instruction case stops generation.

The DSP corpus (`dsp.bin`) runs on Unicorn's Cortex-M4 model and has one
further manual-derived expectation, counted per family as
`manual_smlad_q_overrides` in `dsp.json`:

| Encoding | Expected behavior | ARM reference | Reason |
| --- | --- | --- | --- |
| SMLAD/SMLADX T1 with Ra | Q unchanged when only the product sum overflows | DDI 0403E.b SMLAD pseudocode | Unicorn 2.1.4 (QEMU 5.0) also sets Q on the intermediate sum |

The pseudocode adds both products and the accumulator as unbounded integers and
sets Q only if that final result does not fit in 32 bits. The two products only
overflow on their own when all four halfwords are -32768, so the generator
recognizes exactly that input (`smlad_q_dispute`) and keeps the Q value the
preceding MSR wrote. All result registers still come from Unicorn. `test_dsp`
asserts the same case directly.

For architecturally undefined encodings, a Unicorn undefined-instruction error
or exception/debug event is normalized to Samsa's UNDEF stop at the instruction,
without exception entry. General registers must remain unchanged. This is the
library's exception-free stop contract, not an oracle claim about NVIC behavior.

Random raw encodings enter this comparison only after classification into an
unallocated space in the A5 tables. Invalid-register/other UNPREDICTABLE inputs
are excluded. In particular, values loaded into SP preserve zero low bits as
required by A5.1.3; unaligned values are not allow-listed as oracle disagreements.
