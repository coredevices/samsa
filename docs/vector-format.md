<!-- SPDX-FileCopyrightText: 2026 Joshua Jun -->
<!-- SPDX-License-Identifier: Apache-2.0 -->
# Full-state oracle records

`gen_arch_vectors.py` generates `architecture.bin` using the factories in
`oracle_cases.py`. It selects each family in turn, randomizes valid operands and
initial state, and executes on Unicorn's Cortex-M3 model. `architecture.json`
records the producer/version, seed, family counts, manual expectations, and hash.
The `--suite sequences` profile produces `sequences.bin`/`.json`: 10,000 sequences
across 20 families. These compose narrow/wide ALU operations, carry dependencies,
all IT masks with changing flags, memory operations, MRS/MSR and sticky Q.
The `--suite dsp` profile produces `dsp.bin`/`.json` on Unicorn's Cortex-M4
model: 20,000 cases across 93 families covering every ARMv7E-M DSP instruction,
the APSR_g MSR masks, and reserved DSP encodings. Each valid case first runs
`msr APSR_nzcvqg, r12` with random r12, so NZCVQ and GE inputs vary, and
operands mix random words with saturation and sign boundaries.

Replay needs no Python, assembler, or emulator library. `test_arch_vectors` checks
all sixteen registers, NZCVQ, GE and ITSTATE, the stop/payload, and every mapped byte.
Each case runs both with one continuous budget and with one-instruction budgets
to exercise ITSTATE and exclusive-monitor resumption. Cache-enabled builds repeat
both modes with the previous replay's instruction cache retained, while restoring
architectural state and memory. This checks cached execution against the same
oracle expectations as initial decoding. A failing replay
prints the vector index, family, instruction halfwords, and changed state.

The byte order is little-endian, with no native C padding. The header is magic
`SAMSA002`, case count (u32), family count (u16), then a family-name table. Each
name is length (u8) followed by UTF-8 bytes.

Each record contains, in order:

1. Initial seed (u32), family index (u16), code halfword count (u8), instruction
   budget (u8), initial register override mask (u16), memory patch count (u16).
2. Instruction halfwords (u16 each), followed by override values (u32 each) for
   set mask bits in ascending register order.
3. Initial memory patches: offset (u16), byte size (u8), value (u32).
4. Expected r0-r15 (sixteen u32), masked xPSR (u32), stop (u8), payload (u8),
   expected memory change count (u16).
5. Memory changes, each an offset (u16) and expected byte (u8).

Initial r0-r14 and flags use the same seed LCG as the original ALU format. SP is
word-aligned; factories normally override it with a valid stack address. Guest
memory spans `[0x1000,0x2000)`, and instructions start at `0x1100`. Memory starts
as a repeated 64-byte block generated from `seed XOR 0x9e3779b9`, taking sixteen
successive LCG words. Instructions and initial patches are applied afterward.
Sparse expected changes preserve a compact file while replay still compares
all 4 KiB. The xPSR comparison mask is `0xff0ffc00` (NZCVQ, GE, T, split
ITSTATE). GE is always zero in the Cortex-M3 corpora.

See [oracle disputes](../tests/vectors/disputes.md) for the exact four hint
limitations, the SMLAD Q override, and undefined-stop normalization. No broad instruction-family
exceptions or UNPREDICTABLE encodings are included in the comparison.
