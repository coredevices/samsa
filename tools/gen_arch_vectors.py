#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Joshua Jun
# SPDX-License-Identifier: Apache-2.0
"""Generate full-state, sparse-memory oracle vectors for each ARMv7-M family."""

import argparse
import hashlib
import json
import random
import struct
from collections import Counter
from pathlib import Path

from gen_vectors import initial
from oracle_cases import BASE, PC, catalog, memory, sequence_catalog

# DDI 0403E.b A7-560 (WFE), A7-562 (YIELD); Samsa treats both as NOP.
HINT_DISPUTES = {(0xBF10,), (0xBF20,), (0xF3AF, 0x8001), (0xF3AF, 0x8002)}


def generate(count, seed, output, suite="families"):
    import unicorn
    from unicorn import arm_const

    uc = unicorn.Uc(unicorn.UC_ARCH_ARM, unicorn.UC_MODE_THUMB | unicorn.UC_MODE_MCLASS)
    uc.ctl_set_cpu_model(arm_const.UC_CPU_ARM_CORTEX_M3)
    uc.mem_map(BASE, 4096)
    pristine = uc.context_save()
    ids = [getattr(arm_const, f"UC_ARM_REG_R{i}") for i in range(13)]
    ids += [arm_const.UC_ARM_REG_SP, arm_const.UC_ARM_REG_LR, arm_const.UC_ARM_REG_PC]
    events = []

    def interrupt(engine, number, _):
        events.append(number)
        engine.emu_stop()

    uc.hook_add(unicorn.UC_HOOK_INTR, interrupt)
    families = sequence_catalog() if suite == "sequences" else catalog()
    counts = Counter()
    manual_overrides = Counter()
    rng = random.Random(seed)
    data = bytearray(b"SAMSA002" + struct.pack("<IH", count, len(families)))
    for name, _ in families:
        encoded = name.encode()
        data.extend(bytes([len(encoded)]) + encoded)
    for index in range(count):
        family = index % len(families)
        name, make = families[family]
        case, case_seed = make(rng), rng.getrandbits(32)
        regs, flags = initial(case_seed)
        overrides = {13: 0x1900, **case.regs}
        for register, value in overrides.items():
            regs[register] = value
        before = memory(case_seed)
        encoded = struct.pack(f"<{len(case.code)}H", *case.code)
        before[PC - BASE : PC - BASE + len(encoded)] = encoded
        for offset, size, value in case.memory:
            before[offset : offset + size] = value.to_bytes(size, "little")
        uc.context_restore(pristine)
        uc.mem_write(BASE, bytes(before))
        uc.ctl_remove_cache(BASE, BASE + 4096)
        for reg_id, value in zip(ids, [*regs, PC], strict=True):
            uc.reg_write(reg_id, value)
        uc.reg_write(arm_const.UC_ARM_REG_XPSR, flags | 0x01000000)
        events.clear()
        error = None
        try:
            # IT sequences run to their linear end, counting skipped slots too.
            uc.emu_start(PC | 1, PC + len(encoded), count=1 if case.steps == 1 else 0)
        except unicorn.UcError as exc:
            error = exc
        actual = [uc.reg_read(reg_id) for reg_id in ids]
        xpsr = uc.reg_read(arm_const.UC_ARM_REG_XPSR) & 0xFF00FC00
        stop = 0
        if case.undefined:
            if not events and (
                error is None
                or error.errno not in [unicorn.UC_ERR_INSN_INVALID, unicorn.UC_ERR_EXCEPTION]
            ):
                raise RuntimeError(f"{name}: UDF did not trap")
            if actual[:15] != regs:
                raise RuntimeError(f"{name}: UDF changed general registers")
            # No exception entry is modeled: report the faulting instruction.
            actual[15], stop = PC, 5
            xpsr = flags | 0x01000000
        elif (
            case.code in HINT_DISPUTES
            and error is not None
            and error.errno == unicorn.UC_ERR_INSN_INVALID
            and not events
        ):
            if actual[:15] != regs:
                raise RuntimeError(f"{name}: unsupported hint changed registers")
            actual[15] = PC + len(encoded)
            xpsr = flags | 0x01000000
            manual_overrides[name] += 1
        elif error is not None or events:
            raise RuntimeError(f"case {index} {name} {case.code}: {error}, interrupts={events}")
        after = bytes(uc.mem_read(BASE, 4096))
        changes = [
            (offset, value)
            for offset, (old, value) in enumerate(zip(before, after, strict=True))
            if old != value
        ]
        regmask = sum(1 << r for r in overrides)
        data.extend(
            struct.pack(
                "<IHBBHH", case_seed, family, len(case.code), case.steps, regmask, len(case.memory)
            )
        )
        data.extend(encoded)
        for register in sorted(overrides):
            data.extend(struct.pack("<I", overrides[register]))
        for offset, size, value in case.memory:
            data.extend(struct.pack("<HBI", offset, size, value))
        data.extend(struct.pack("<16IIBBH", *actual, xpsr, stop, 0, len(changes)))
        for offset, value in changes:
            data.extend(struct.pack("<HB", offset, value))
        counts[name] += 1
        if (index + 1) % 10000 == 0:
            print(f"{index + 1}/{count}", flush=True)
    temporary = output.with_suffix(".tmp")
    temporary.write_bytes(data)
    temporary.replace(output)
    output.with_suffix(".json").write_text(
        json.dumps(
            {
                "producer": "unicorn",
                "suite": suite,
                "manual_hint_overrides": manual_overrides,
                "version": unicorn.__version__,
                "cpu": "Cortex-M3",
                "count": count,
                "seed": seed,
                "families": counts,
                "sha256": hashlib.sha256(data).hexdigest(),
                "normalization": "UDF traps become UNDEF at fault_pc without exception entry.",
            },
            indent=2,
        )
        + "\n"
    )
    print(f"{output}: {count} vectors, {len(families)} families")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--count", type=int)
    parser.add_argument("--suite", choices=["families", "sequences"], default="families")
    parser.add_argument("--seed", type=lambda value: int(value, 0), default=0x41524D37)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    sequence = args.suite == "sequences"
    count = args.count if args.count is not None else 10000 if sequence else 100000
    output = args.output or Path(f"tests/vectors/{'sequences' if sequence else 'architecture'}.bin")
    if count < len(sequence_catalog() if sequence else catalog()):
        parser.error("count must cover every family")
    generate(count, args.seed, output, args.suite)


if __name__ == "__main__":
    main()
