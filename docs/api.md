<!-- SPDX-FileCopyrightText: 2026 Joshua Jun -->
<!-- SPDX-License-Identifier: Apache-2.0 -->
# API and embedding

`include/samsa/samsa.h` is the only public header. Initialize a CPU with
`samsa_reset` **before** assigning configuration: reset clears the entire struct.
The embedder owns all storage and must keep regions, hooks, bus, and host memory
alive for the duration of execution. Region and hook counts are `uint16_t`.
Samsa never allocates memory. Core code needs freestanding `memcpy` and `memset`.

Guest addresses are `uint32_t`, host pointers are ordinary pointers. The library
never casts a guest address to a host pointer. Host pointers may implement an
identity mapping on a 32-bit host when the embedder provides valid accessible
objects. Region storage must be at least `size` bytes. Regions must not wrap the 32-bit guest address space. Use nonoverlapping regions
and do not place guest code at the reserved call-return sentinel `0xfffffffe`.
`r[15]` is the next fetch address, with bit zero clear. Set SP explicitly.

## Execution and stops

`samsa_run(cpu, max_insns, &executed)` stops at the first event or after the budget.
A skipped IT instruction consumes one instruction. A 32-bit instruction consumes
one instruction. A decoded instruction that faults also consumes one; a failing
fetch consumes none. A zero budget does not fetch or inspect hooks. Reaching a
hook or sentinel immediately after the last budgeted instruction is observed by
the next run with a nonzero budget.

| Stop | Meaning and resume state |
| --- | --- |
| BUDGET | PC/ITSTATE retained; call run again |
| HOOK | PC is in a configured `[start,end)` range; no fetch occurred |
| RETURN | Active `samsa_call` reached its reserved sentinel |
| SVC / BKPT | `stop_info` contains imm8; PC/ITSTATE advanced |
| UNDEF | Unsupported/undefined encoding; PC retained at `fault_pc` |
| MEMFAULT | Fetch/load/store failure; `fault_addr` and `fault_pc` identify it |
| USAGE | Misalignment, invalid exchange state, or forbidden IT branch; PC retained |

Faulting multiple transfers can already have accessed memory or updated some
registers. There is no instruction rollback or exception entry. Treat a fault as
terminal unless the embedder deliberately repairs the state. `fault_pc` records
the most recently attempted instruction and is meaningful for fault stops.

`get_xpsr`/`set_xpsr` handle NZCVQ and split ITSTATE fields. T is always reported
set. `samsa_status` returns the current stop, including a helper memory fault.
Other `_` fields are internal and must not be modified by embedders.

## Memory bus

Regions require X for instruction fetch, R for loads, and W for stores.
Unmapped data accesses may use `SamsaBus.read`/`.write`. Callbacks return false
for an access failure, or true with the low `size` bytes read/written. Sizes are
1, 2, or 4. Fetches never use the bus because it cannot grant X permission.
A region permission failure or transfer crossing a mapped region boundary never falls through to the bus. Mapping configuration
is trusted host input, and must not be changed concurrently with execution.
The guest may modify executable memory if its region also has W permission.

## Host functions and guest callbacks

A guest function pointer can target a hook range. Both even and odd exchange
targets in a hook range are accepted; range boundaries themselves are canonical
even byte addresses. Hook checks precede fetch permission checks. Hook ranges
need no backing region.

```c
static SamsaStop handle(SamsaCpu *cpu, SamsaStop why, void *ctx) {
    (void)ctx;
    if (why != SAMSA_STOP_HOOK) return why;
    uint32_t input = samsa_arg(cpu, 0);
    if (samsa_status(cpu) == SAMSA_STOP_MEMFAULT) return SAMSA_STOP_MEMFAULT;
    uint32_t args[] = { input };
    uint32_t result;
    /* This guest callback may itself hit hooks and call this handler again. */
    SamsaStop stop = samsa_call(cpu, 0x1201, args, 1, &result, handle, NULL);
    if (stop != SAMSA_STOP_RETURN) return stop;
    samsa_return32(cpu, result + 1);
    return samsa_status(cpu) == SAMSA_STOP_USAGE ? SAMSA_STOP_USAGE : SAMSA_STOP_BUDGET;
}

/* After reset and memory configuration: */
SamsaHook hooks[] = { { 0x3000, 0x3010 } };
cpu.hooks = hooks;
cpu.num_hooks = 1;
uint32_t args[] = { 42 }, result;
SamsaStop stop = samsa_call(&cpu, 0x1001, args, 1, &result, handle, NULL);
```

The handler returns BUDGET to resume, or another reason to abort. Only HOOK,
SVC, and BKPT are delivered to the handler. On a hook, advance the PC (usually
using a return helper). A stalled handler still consumes call fuel.

`samsa_call` allows one million instructions/zero-progress hook events;
`samsa_call_budget` supplies an explicit total limit. On any stop the caller's
registers, flags, ITSTATE, and enclosing call mode are restored. On success `ret`
receives r0 and the enclosing stop payload/fault diagnostics are restored as well.
On failure the failing call's diagnostics remain, but its registers are unwound;
a budgeted call cannot be resumed. Guest stack contents and other memory changes
persist. The local exclusive monitor is shared across calls, just like memory.
Nested calls use host stack frames driven by the embedder's handler; the core
does not recurse to interpret guest calls. The embedder bounds nesting depth.

Arguments are **AAPCS word slots**, not typed parameter indices. `samsa_arg(i)`
reads r0-r3 then stack words. `samsa_arg64(i)` rounds the word index up to an even
slot and reads the low word first. The embedder accounts for all preceding
parameter types; structure arguments, VFP PCS, and variadic classification are
outside this helper API. `samsa_call` accepts an already laid-out array of word
slots (including any alignment padding), puts excess words on an 8-byte-aligned
stack, and preserves the enclosing SP. Return helpers put values in r0/r1 and
exchange to LR, applying the same INVSTATE checks as guest load-to-PC operations.

There is no decode cache in this version. `samsa_invalidate` is an explicit no-op
reserved for cache-enabled implementations. Direct changes to region host memory
are visible at the next fetch.
