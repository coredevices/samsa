#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Joshua Jun
# SPDX-License-Identifier: Apache-2.0
"""Generate deterministic ALU vectors; Unicorn is an offline, optional dependency.

The spec producer is an independent integer model, never an oracle substitute.
Random unclassified halfwords belong in the sanitizer/fuzzer corpus, not in a
comparison corpus that silently includes UNPREDICTABLE instructions.
"""

import argparse
import hashlib
import json
import random
import struct
from pathlib import Path

MASK = 0xFFFFFFFF
BASE = 0x1000


def initial(seed):
    values = []
    for _ in range(16):
        seed = (seed * 1664525 + 1013904223) & MASK
        values.append(seed)
    values[13] &= ~3  # Cortex-M SP writes discard the low two bits.
    return values[:15], values[15] & 0xF8000000


def signed(x):
    return x - (1 << 32) if x & (1 << 31) else x


def shift(x, kind, amount, carry):
    if kind == 5:
        if x < 1024:
            low = x & 255
            return [low, low * 0x10001, low * 0x1000100, low * 0x1010101][x >> 8], carry
        return shift(128 | (x & 127), 3, x >> 7, carry)
    if not amount:
        return x, carry
    if kind == 0:
        return (x << amount) & MASK, (x >> (32 - amount)) & 1
    if kind == 1:
        return x >> amount, (x >> (amount - 1)) & 1
    if kind == 2:
        return (signed(x) >> amount) & MASK, (x >> (amount - 1)) & 1
    if kind == 4:
        return (carry << 31) | (x >> 1), x & 1
    amount %= 32
    result = ((x >> amount) | (x << ((32 - amount) % 32))) & MASK
    return result, result >> 31


def sample(rng):
    """Return (halfwords, operation, operands, update_flags, shift_kind, amount)."""
    kind = rng.randrange(6)
    if kind == 0:
        operation = rng.choice([0, 1, 8, 10, 11, 12, 13, 14, 15])
        # T1 ALU: Rd=0, Rm=1. Map onto common model operations.
        names = {
            0: "and",
            1: "eor",
            8: "tst",
            10: "cmp",
            11: "cmn",
            12: "orr",
            13: "mul",
            14: "bic",
            15: "mvn",
        }
        return (0x4008 | operation << 6, 0), names[operation], (0, 1), True, 0, 0
    if kind == 1:
        operation = rng.choice(["add", "sub", "mov", "cmp"])
        imm = rng.randrange(256)
        h = {"add": 0x3000, "sub": 0x3800, "mov": 0x2000, "cmp": 0x2800}[operation]
        return (h | imm, 0), operation, (0, -imm - 1), True, 0, 0
    if kind == 2:
        shift_type = rng.randrange(3)
        amount = rng.randrange(32)
        h = shift_type << 11 | amount << 6 | 8
        return (h, 0), "shift", (1, 1), True, shift_type, amount or (32 if shift_type else 0)
    if kind == 3:
        op = rng.choice([0, 1, 2, 3, 4, 8, 10, 11, 13, 14])
        names = {
            0: "and",
            1: "bic",
            2: "orr",
            3: "orn",
            4: "eor",
            8: "add",
            10: "adc",
            11: "sbc",
            13: "sub",
            14: "rsb",
        }
        amount = rng.randrange(32)
        typ = rng.randrange(4)
        flags = bool(rng.randrange(2))
        h = 0xEA01 | op << 5 | int(flags) << 4
        low = (amount >> 2) << 12 | (amount & 3) << 6 | typ << 4 | 2
        if not amount and typ:
            amount, typ = (1, 4) if typ == 3 else (32, typ)
        return (h, low), names[op], (1, 2), flags, typ, amount
    if kind == 4:
        op = rng.choice([0, 1, 2, 3, 4, 8, 10, 11, 13, 14])
        names = {
            0: "and",
            1: "bic",
            2: "orr",
            3: "orn",
            4: "eor",
            8: "add",
            10: "adc",
            11: "sbc",
            13: "sub",
            14: "rsb",
        }
        imm = rng.randrange(4096)
        if 256 <= imm < 1024 and not (imm & 255):
            imm |= 1
        flags = bool(rng.randrange(2))
        h = 0xF001 | op << 5 | int(flags) << 4 | ((imm >> 11) << 10)
        low = (imm >> 8 & 7) << 12 | (imm & 255)
        return (h, low), names[op], (1, -imm - 1), flags, 5, 0
    imm = rng.randrange(0x10000)
    top = bool(rng.randrange(2))
    h = (0xF2C0 if top else 0xF240) | (imm >> 12) | ((imm >> 11 & 1) << 10)
    low = (imm >> 8 & 7) << 12 | (imm & 255)
    return (h, low), "movt" if top else "mov", (0, -imm - 1), False, 0, 0


def model(item, regs, flags):
    (h, _), op, (an, bn), update, typ, amount = item
    a, b = regs[an], regs[bn] if bn >= 0 else -bn - 1
    carry = flags >> 29 & 1
    b, shift_carry = shift(b, typ, amount, carry)
    if op == "shift":
        a = regs[1]
        value, shift_carry = shift(a, typ, amount, carry)
    elif op == "movt":
        value = (a & 65535) | (b << 16)
    elif op in ("add", "adc", "sub", "sbc", "cmp", "cmn", "rsb"):
        x, y, cy = a, b, 0
        if op in ("sub", "sbc", "cmp"):
            y, cy = b ^ MASK, carry if op == "sbc" else 1
        elif op == "rsb":
            x, y, cy = b, a ^ MASK, 1
        elif op == "adc":
            cy = carry
        total = x + y + cy
        value = total & MASK
        overflow = int(not -(1 << 31) <= signed(x) + signed(y) + cy < (1 << 31))
        if update:
            flags = (flags & 0x08000000) | (int(total > MASK) << 29) | overflow << 28
    else:
        value = {
            "and": lambda: a & b,
            "tst": lambda: a & b,
            "bic": lambda: a & ~b,
            "eor": lambda: a ^ b,
            "orr": lambda: a | b,
            "orn": lambda: a | ~b,
            "mvn": lambda: ~b,
            "mul": lambda: a * b,
            "mov": lambda: b,
        }[op]() & MASK
    if update:
        flags = (flags & 0x38000000) | (value & 0x80000000) | (int(value == 0) << 30)
        if op in ("and", "tst", "bic", "eor", "orr", "orn", "mvn", "mov", "shift"):
            flags = (flags & ~(1 << 29)) | (shift_carry << 29)
    r0 = regs[0] if op in ("cmp", "cmn", "tst") else value
    return r0, flags, BASE + (4 if h >> 11 >= 29 else 2), 0


def unicorn_producer():
    # Imports stay here: spec generation and ordinary C tests need no Unicorn.
    import unicorn
    from unicorn import arm_const

    uc = unicorn.Uc(unicorn.UC_ARCH_ARM, unicorn.UC_MODE_THUMB | unicorn.UC_MODE_MCLASS)
    uc.mem_map(BASE, 4096)
    ids = [getattr(arm_const, f"UC_ARM_REG_R{i}") for i in range(13)]
    ids += [arm_const.UC_ARM_REG_SP, arm_const.UC_ARM_REG_LR]

    def execute(item, regs, flags):
        h, low = item[0]
        data = struct.pack("<HH", h, low)
        uc.mem_write(BASE, data)
        # Invalidate translated blocks after changing bytes at a fixed address.
        uc.ctl_remove_cache(BASE, BASE + 4)
        for reg_id, value in zip(ids, regs, strict=True):
            uc.reg_write(reg_id, value)
        uc.reg_write(arm_const.UC_ARM_REG_XPSR, flags | 0x01000000)
        uc.emu_start(BASE | 1, BASE + 4, count=1)
        actual = [uc.reg_read(reg_id) for reg_id in ids]
        if actual[1:] != regs[1:]:
            raise RuntimeError(f"unexpected register side effect for {h:04x} {low:04x}")
        return (
            actual[0],
            uc.reg_read(arm_const.UC_ARM_REG_XPSR) & 0xF8000000,
            uc.reg_read(arm_const.UC_ARM_REG_PC),
            0,
        )

    return execute, unicorn.__version__


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--producer", choices=["unicorn", "spec"], default="unicorn")
    parser.add_argument("--count", type=int, default=100000)
    parser.add_argument("--seed", type=lambda x: int(x, 0), default=0x53414D53)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.count < 1:
        parser.error("count must be positive")
    producer, version = (model, "ARM-DDI-0403E.b integer model")
    if args.producer == "unicorn":
        producer, version = unicorn_producer()
    rng = random.Random(args.seed)
    output = args.output or Path(f"tests/vectors/{args.producer}.bin")
    data = bytearray(b"SAMSV001" + struct.pack("<I", args.count))
    for _ in range(args.count):
        item, seed = sample(rng), rng.getrandbits(32)
        regs, flags = initial(seed)
        result = producer(item, regs, flags)
        data.extend(struct.pack("<HHIIIII", *item[0], seed, *result))
    output.write_bytes(data)
    output.with_suffix(".json").write_text(
        json.dumps(
            {
                "producer": args.producer,
                "version": version,
                "count": args.count,
                "seed": args.seed,
                "sha256": hashlib.sha256(data).hexdigest(),
                "scope": "Valid ALU encodings only; random raw encodings use sanitizer tests.",
            },
            indent=2,
        )
        + "\n"
    )
    print(f"{output}: {args.count} vectors ({args.producer})")


if __name__ == "__main__":
    main()
