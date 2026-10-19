<!-- SPDX-FileCopyrightText: 2026 Joshua Jun -->
<!-- SPDX-License-Identifier: Apache-2.0 -->
# ARMv7-M encoding coverage

Source: ARM DDI 0403E.b A5.2/A5.3 and A7. `I` means the decoder family is
implemented; `D` means directed behavior assertions exist; `V` means sampled
integer-model ALU vectors. Compiled libgcc execution adds integration coverage,
not exhaustive operand enumeration. The original ALU corpus has 100,000 Unicorn
vectors and 100,000 independent-model vectors. `test_arch_vectors` adds 100,000
full-state records across 301 named encoding families, comparing all registers,
flags/ITSTATE, and memory in continuous and single-step modes. Exact YIELD/WFE
oracle limitations are listed in `vectors/disputes.md`, not silently skipped.
The factory names and counts in `vectors/architecture.json` provide the mapping
from table forms to executed oracle cases.

## A5.2: Thumb-16

| Encoding / aliases | State | Test(s) |
| --- | --- | --- |
| LSL/LSR/ASR immediate T1; MOV alias | I V | test_vectors, test_alu |
| ADD/SUB register T1 | I D | test_thumb16 arithmetic |
| ADD/SUB imm3 T1 | I D | test_addressing thumb16 |
| MOV imm8 T1 | I D V | test_core, test_vectors |
| CMP imm8 T1 | I D V | test_thumb16 arithmetic, test_vectors |
| ADD/SUB imm8 T2 | I D V | test_thumb16 arithmetic, test_vectors |
| AND/EOR/ORR/BIC/MVN register T1 | I V | test_vectors |
| LSL/LSR/ASR/ROR register T1 | I D | test_control branches; test_alu edge amounts |
| ADC/SBC T1 | I D | test_thumb16 arithmetic |
| TST register T1 | I V | test_vectors |
| RSB immediate #0 T1 | I D | test_control branches |
| CMP register T1; CMN register T1 | I V | test_vectors |
| MUL T1 | I V | test_vectors |
| High-register ADD/CMP/MOV T2 | I D | test_control branches; test_thumb16 misc; test_addressing SP MOV |
| BX/BLX register T1 | I D | test_core, test_thumb16 misc, test_embedding |
| LDR literal T1 | I D | test_thumb16 memory |
| STR/STRH/STRB register T1 | I D | test_addressing thumb16 |
| LDR/LDRH/LDRB register T1 | I D | test_addressing thumb16 |
| LDRSB register T1 | I D | test_thumb16 memory |
| LDRSH register T1 | I D | test_addressing thumb16 |
| STR/LDR word immediate T1 | I D | test_thumb16 memory |
| STRB/LDRB immediate T1 | I D | test_addressing thumb16 |
| STRH/LDRH immediate T1 | I D | test_thumb16 memory |
| STR/LDR SP-relative T2 | I D | test_embedding stack arguments; compiled suite |
| ADR T1 | I D | test_thumb16 memory |
| ADD SP-relative T1 | I D | test_addressing thumb16 |
| ADD/SUB SP immediate T2/T1 | I D | test_addressing thumb16 |
| CBZ/CBNZ T1 | I D | test_thumb16 misc; test_addressing thumb16 |
| SXTH/SXTB/UXTH/UXTB T1 | I D | test_addressing thumb16 |
| REV/REV16/REVSH T1 | I D | test_thumb16 misc |
| PUSH/POP T1 | I D | test_thumb16 memory; compiled suite |
| IT T1, NOP/YIELD/WFE/WFI/SEV hints T1 | I D | test_control it_resume (including AL); test_addressing thumb16 |
| STM/LDM T1 | I D | test_thumb16 memory, base/writeback hazards |
| B conditional T1 | I D | test_control all conditions and negative offset |
| UDF T1 | I D | test_addressing thumb16 |
| SVC T1 | I D | test_thumb16 misc |
| B unconditional T2 | I D | test_core |
| BKPT T1 | I D | test_core |
| CPS T1 | rejected D | test_thumb16 misc |
| SETEND / unallocated encodings | rejected D | test_decode_guards; test_arch_vectors raw.undefined.0 |

## A5.3: Thumb-32

| Table / encoding forms | State | Test(s) |
| --- | --- | --- |
| A5-10 modified immediate AND/BIC/ORR/ORN/EOR | I V | test_vectors; test_thumb32 AND/MOV constants |
| A5-10 modified immediate ADD/ADC/SBC/SUB/RSB | I V | test_vectors, test_thumb32 ADD flags |
| A5-10 MOV/MVN/TST/TEQ/CMN/CMP aliases | I D | test_aliases |
| A5-12 ADDW/SUBW | I D | test_thumb32 data_processing |
| A5-12 ADR add/sub | I D | test_aliases |
| A5-12 MOVW/MOVT | I D V | test_thumb32, test_vectors |
| A5-12 SSAT/USAT LSL/ASR | I D | test_aliases all four combinations |
| A5-12 SBFX/UBFX/BFI/BFC | I D | test_thumb32 data_processing |
| A5-13 B T3 conditional | I D | test_control all conditions and negative offset |
| A5-13 B T4; BL T1 | I D | test_thumb32 control; compiled programs |
| A5-13 MRS APSR / MSR APSR_nzcvq | I D | test_thumb32 control |
| A5-14 hints / DBG | I D | test_control system_ops; test_aliases DBG |
| A5-15 CLREX/DMB/DSB/ISB | I D | test_thumb32 exclusive; test_control system_ops |
| A5-16 STMIA/LDMIA T2 | I D | test_thumb32 memory |
| A5-16 STMDB/LDMDB T1 | I D | test_addressing thumb32 |
| A5-16 PUSH/POP aliases | I D | test_arch_vectors 32.multiple stack forms; compiled libgcc |
| A5-17 STRD/LDRD T1 immediate/literal, all index modes | I D | test_addressing dual_modes; compiled STRD with LR |
| A5-17 STREX/LDREX T1 | I D | test_thumb32 exclusive |
| A5-17 STREXB/LDREXB T1 | I D | test_thumb32 exclusive |
| A5-17 STREXH/LDREXH T1 | I D | test_thumb32 exclusive |
| A5-17 TBB/TBH T1 | I D | test_thumb32 control; compiled switch |
| A5-18 LDR imm12 T3 / imm8 T4 | I D | test_thumb32 memory: pre/post/writeback |
| A5-18 LDRT T1 / LDR shifted-register T2 | I D | test_thumb32 memory |
| A5-18 LDR literal T2 +/- | I D | test_addressing thumb32 |
| A5-19 LDRH/LDRSH imm12/imm8/literal/register | I D | test_addressing thumb32 |
| A5-19 LDRHT/LDRSHT | I D | test_addressing thumb32 |
| A5-20 LDRB/LDRSB imm12/imm8/literal/register | I D | test_addressing thumb32 |
| A5-20 LDRBT/LDRSBT | I D | test_addressing thumb32 |
| A5-19/20 PLD/PLI, unallocated memory hints | I D | test_thumb32 memory (unmapped address) |
| A5-21 STR/STRB/STRH imm12/imm8/register | I D | test_addressing thumb32 |
| A5-21 STRT/STRBT/STRHT | I D | test_addressing thumb32 |
| A5-22 shifted AND/BIC/ORR/ORN/EOR | I D V | test_thumb32, test_vectors |
| A5-22 shifted ADD/ADC/SBC/SUB/RSB | I D V | test_thumb32, test_vectors |
| A5-22 TST/TEQ/CMN/CMP aliases | I D | test_aliases |
| A5-23 MOV/MVN/LSL/LSR/ASR/ROR/RRX aliases | I D | test_aliases; test_thumb32 RRX |
| A5-24 register LSL/LSR/ASR/ROR, S variants | I D | test_aliases; test_thumb32 |
| A5-24 SXTH/SXTB/UXTH/UXTB with rotation | I D | test_aliases SXTH/UXTB; test_thumb32 SXTB/UXTH |
| A5-27 REV/REV16/REVSH/RBIT/CLZ | I D | test_thumb32 data_processing |
| A5-28 MUL/MLA/MLS | I D | test_thumb32 multiply_divide |
| A5-29 SMULL/UMULL/SMLAL/UMLAL | I D | test_thumb32 multiply_divide; libgcc |
| A5-29 SDIV/UDIV | I D | test_thumb32 zero divisor, signed overflow; compiled programs |
| A5-25/26/27/28/29 DSP extension entries | rejected D | test_decode_guards, 90 assembled DSP representatives |
| A5-30 coprocessor, A6 floating point | rejected D | test_decode_guards, all first-halfword allocations |

## Explicit architectural hazards

- [x] PC + 4 and aligned literal/ADR; PC-relative table branch.
- [x] ALU versus exchange PC writes; even exchange faults; hooks before fetch.
- [x] IT condition failure, implicit flag suppression, budget/hook resume, early branch rejection, unconditional BKPT.
- [x] Shift carry 0/32/>32, RRX, rotated versus replicated modified immediates.
- [x] Add carry/overflow and subtract NOT-borrow.
- [x] Thumb-16 base-in-list LDM/STM handling.
- [x] BL/BLX next-PC link semantics.
- [x] Ordinary unaligned scalar access; multiple/dual/stack misalignment faults.
- [x] Divide by zero and INT_MIN/-1.
- [x] Q flag saturation; APSR read/write and unsupported selector.
- [x] Local monitor success, failed second STREX, CLREX, byte/halfword width.
- [x] Nested callbacks with >=3 levels and outer register/flag restoration.
- [x] Every in-scope encoding family mapped to directed/oracle tests; representative invalid-register and reserved-bit guards.
- [x] Unicorn vectors generated/replayed, >=100,000 valid ALU cases.
- [x] Oracle sampler expanded to memory/control/data-processing classes, including full-state replay.
- [x] Explicit classified random UNDEF oracle comparisons (A5 table holes only).
- [x] Local macOS/Linux GCC/Clang validation available; remote CI awaits a published repository.
- [ ] Performance/cache work and final repeated matrix validation.
