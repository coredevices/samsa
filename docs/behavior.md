<!-- SPDX-FileCopyrightText: 2026 Joshua Jun -->
<!-- SPDX-License-Identifier: Apache-2.0 -->
# Architectural behavior

The target is ARMv7-M Thumb, Cortex-M3 default settings, little-endian guest and
host. Compilation fails on an unknown or big-endian host byte order. No ARM/A32,
VFP, ARMv7E-M DSP, exception/NVIC/MPU, privileged registers, or binary format is
implemented. Unknown encodings stop with UNDEF. Many UNPREDICTABLE combinations
also stop with UNDEF; this is a deterministic policy, not a promise to reproduce
hardware's behavior on UNPREDICTABLE inputs. The remaining reserved-bit audit is
tracked in the coverage checklist.

* PC operands read the instruction address plus four. Literals/ADR align that
  value down to four bytes. Table branches use the unaligned PC operand.
* ALU PC writes (16-bit high-register ADD/MOV) clear bit zero without exchange.
  BX/BLX and loads/POP/LDM to PC check bit zero. An even target stops with USAGE
  unless it is a hook or active call sentinel, checked before fetching. No
  EXC_RETURN processing exists. BLX immediate is not an ARMv7-M instruction.
* Ordinary scalar word/halfword accesses allow unaligned addresses. Multiple,
  dual, stack, and exclusive accesses require their architectural alignment.
  Loads to PC require a word-aligned address. A failing access reports its
  starting address. A transfer must fit within a single region or be serviced
  wholly by an unmapped-data bus access; partial region transfers are not split.
* Instruction writes to SP clear its low two bits, as required by ARMv7-M.
* UDIV/SDIV return zero for a zero divisor. `INT_MIN / -1` produces `INT_MIN`
  without relying on overflowing signed host arithmetic.
* ITSTATE survives budget/hook stops and advances for skipped instructions.
  Implicit 16-bit flag updates are suppressed inside IT; compare/test operations
  still update flags. Explicit 32-bit S bits update flags. Branches are checked
  against the last IT slot when executed.
* NOP/YIELD/WFE/WFI/SEV/DBG and DMB/DSB/ISB have no side effects. There are no host
  memory-ordering guarantees. PLD/PLI do not touch memory or fault.
* MRS supports APSR only (SYSm zero); MSR supports APSR_nzcvq only (mask `10`,
  SYSm zero). NZCVQ are the only modeled APSR flags. Other selectors and CPS
  stop with UNDEF. Q is sticky for SSAT/USAT and writable through MSR/accessors.
* LDREX/B/H remember an exact address and access width. STREX/B/H succeed only
  for that address and width, and clear the monitor whether they succeed or
  fail. CLREX clears it. Ordinary stores do not clear the monitor. There is no
  global monitor or DMA/coherency model; bus callbacks are responsible for their
  own host-side synchronization.
* Thumb-16 LDM with its base in the list suppresses writeback. STM stores the
  original base when it is the lowest register; other base-in-list STM forms
  are rejected. Thumb-32 writeback with base in the list is rejected.

No source code from QEMU, Unicorn, or other emulators is included. Encodings and
semantics were implemented from ARM DDI 0403E.b A5/A7. Unicorn is optional tooling
only, imported by the vector generator, and is never linked with the library.
