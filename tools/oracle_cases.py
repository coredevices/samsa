# SPDX-FileCopyrightText: 2026 Joshua Jun
# SPDX-License-Identifier: Apache-2.0
"""Valid ARMv7-M encoding families, independent of the interpreter decoder."""

import struct
from dataclasses import dataclass, field
from functools import partial

BASE = 0x1000
PC = 0x1100
RAM = 0x1800


@dataclass
class Case:
    code: tuple
    regs: dict = field(default_factory=dict)
    memory: tuple = ()
    steps: int = 1
    undefined: bool = False


def memory(seed):
    words = []
    seed ^= 0x9E3779B9
    for _ in range(16):
        seed = (seed * 1664525 + 1013904223) & 0xFFFFFFFF
        words.append(seed)
    return bytearray(struct.pack("<16I", *words) * 64)


def alu16(op, rng):
    return Case((0x4000 | op << 6 | rng.randrange(8) << 3 | rng.randrange(8),))


def add16(immediate, sub, rng):
    return Case(
        (
            0x1800
            | immediate << 10
            | sub << 9
            | rng.randrange(8) << 6
            | rng.randrange(8) << 3
            | rng.randrange(8),
        )
    )


def special16(op, rng):
    d, n = rng.choice([*range(13), 14]), rng.choice([*range(13), 14, 15])
    if op == 1:  # CMP cannot use PC; one operand must be high for this encoding.
        n = rng.choice([8, 9, 10, 11, 12, 14])
    return Case((0x4400 | op << 8 | (d & 8) << 4 | n << 3 | (d & 7),))


def memory16(kind, form, rng):
    offset = rng.randrange(32)
    base = RAM + rng.randrange(4)
    if form == "reg":
        return Case((0x5088 | kind << 9,), {1: base, 2: offset})
    if form == "sp":
        return Case((0x9000 | kind << 11 | offset,), {13: RAM})
    if form == "literal":
        return Case((0x4800 | rng.randrange(32, 128),))
    first = [0x6000, 0x6800, 0x7000, 0x7800, 0x8000, 0x8800][kind]
    return Case((first | offset << 6 | 8,), {1: base})


def extend16(op, rng):
    return Case((0xB200 | op << 6 | rng.randrange(8) << 3 | rng.randrange(8),))


def reverse16(op, rng):
    return Case((0xBA00 | op << 6 | rng.randrange(8) << 3 | rng.randrange(8),))


def address16(form, rng):
    imm = rng.randrange(128)
    return Case((form | imm,))


def branch16(form, rng):
    if form == "b":
        return Case((0xE000 | rng.randrange(128),))
    if form == "cond":
        return Case((0xD000 | rng.randrange(14) << 8 | rng.randrange(128),))
    if form == "cb":
        return Case(
            (0xB100 | rng.randrange(2) << 11 | rng.randrange(2) << 9 | rng.randrange(32) << 3,),
            {0: rng.choice([0, 1])},
        )
    return Case((0x4700 | (form == "blx") << 7,), {0: 0x1301})


def multiple16(load, stack, rng):
    registers = rng.randrange(1, 256)
    if stack:
        # Exclude PC in POP so data words remain arbitrary.
        return Case(((0xBC00 if load else 0xB400) | registers,), {13: RAM + 128})
    registers &= ~(1 << 7)  # Base not in list: no UNPREDICTABLE writeback.
    return Case(((0xCF00 if load else 0xC700) | registers | 1,), {7: RAM})


def dp32(op, form, alias, rng):
    n, d, m = 1, 0, 2
    if alias == "move":
        n = 15
    elif alias == "test":
        d = 15
    set_flags = alias == "test" or bool(rng.randrange(2))
    if form == "reg":
        amount, kind = rng.randrange(32), rng.randrange(4)
        return Case(
            (
                0xEA00 | op << 5 | int(set_flags) << 4 | n,
                amount // 4 << 12 | d << 8 | (amount & 3) << 6 | kind << 4 | m,
            )
        )
    imm = rng.randrange(4096)
    if 256 <= imm < 1024 and not (imm & 255):
        imm |= 1
    return Case(
        (
            0xF000 | (imm >> 11) << 10 | op << 5 | int(set_flags) << 4 | n,
            (imm >> 8 & 7) << 12 | d << 8 | (imm & 255),
        )
    )


def plain32(op, rng):
    imm = rng.randrange(4096)
    if op in [0, 10]:
        n = rng.choice([1, 13, 15])
        return Case((0xF200 | (imm >> 11) << 10 | op << 4 | n, (imm >> 8 & 7) << 12 | (imm & 255)))
    if op in [4, 12]:
        imm = rng.randrange(65536)
        return Case(
            (
                0xF200 | (imm >> 11 & 1) << 10 | op << 4 | imm >> 12,
                (imm >> 8 & 7) << 12 | (imm & 255),
            )
        )
    amount = rng.randrange(32)
    n = 15 if op == 22 and rng.randrange(2) else 1
    if op in [16, 18, 24, 26]:
        if op & 2:
            amount = rng.randrange(1, 32)
        imm = rng.randrange(32)
    elif op in [20, 28]:
        imm = rng.randrange(32 - amount)
    else:
        imm = rng.randrange(amount, 32)
    return Case((0xF200 | op << 4 | n, amount // 4 << 12 | (amount & 3) << 6 | imm))


def register32(kind, rng):
    d, n, m = rng.sample([*range(13), 14], 3)
    if kind < 8:
        return Case(
            (0xFA00 | kind << 4 | n, 0xF000 | d << 8 | m), {m: rng.choice([0, 1, 31, 32, 33, 255])}
        )
    if kind < 12:
        op = [0, 1, 4, 5][kind - 8]
        return Case((0xFA0F | op << 4, 0xF080 | d << 8 | m | rng.randrange(4) << 4))
    sub = kind - 12
    return Case(
        ((0xFAB0 if sub == 4 else 0xFA90) | m, 0xF080 | d << 8 | m | (0 if sub == 4 else sub) << 4)
    )


def multiply32(kind, rng):
    d, n, m, a = rng.sample([*range(13), 14], 4)
    if kind < 3:
        return Case(
            (0xFB00 | n, ((15 if kind == 0 else a) << 12) | d << 8 | m | (16 if kind == 2 else 0))
        )
    if kind < 7:
        return Case(([0xFB80, 0xFBA0, 0xFBC0, 0xFBE0][kind - 3] | n, a << 12 | d << 8 | m))
    return Case(
        ((0xFB90 if kind == 7 else 0xFBB0) | n, 0xF0F0 | d << 8 | m),
        {m: rng.choice([0, 1, 0xFFFFFFFF, 0x80000000, rng.getrandbits(32)])},
    )


def single32(op, form, rng):
    imm, base = rng.randrange(32), RAM + 64 + rng.randrange(4)
    t = rng.choice([0, *range(2, 13), 14] + ([13] if op in [4, 5] and form != "t" else []))
    overrides = {1: base}
    if form == "literal":
        up = rng.randrange(2)
        code = (0xF80F | op << 4 | up << 7, t << 12 | 64)
        address = PC + 4 + (64 if up else -64)
        overrides = {}
    elif form == "imm12":
        code, address = (0xF881 | op << 4, t << 12 | imm), base + imm
    elif form == "reg":
        amount = rng.randrange(4)
        code, address = (0xF801 | op << 4, t << 12 | 2 | amount << 4), base + (imm << amount)
        overrides[2] = imm
    else:
        mode = {
            "pre+": 0xF00,
            "pre-": 0xD00,
            "post+": 0xB00,
            "post-": 0x900,
            "negative": 0xC00,
            "t": 0xE00,
        }[form]
        code = (0xF801 | op << 4, t << 12 | mode | imm)
        address = (
            base
            if form.startswith("post")
            else base + (-imm if form in ["pre-", "negative"] else imm)
        )
    # A5.1.3: software must preserve SP[1:0] as zero in comparison inputs.
    patches = ((address - BASE, 4, rng.getrandbits(32) & ~3),) if t == 13 and op == 5 else ()
    return Case(code, overrides, patches)


def multiple32(load, db, wb, stack, rng):
    registers = rng.randrange(1, 1 << 13) | 3
    n = 13 if stack else 14
    return Case(
        (0xE800 | (256 if db else 128) | load << 4 | wb << 5 | n, registers), {n: RAM + 128}
    )


def dual32(load, pre, up, wb, literal, rng):
    imm = rng.randrange(16)
    n = 15 if literal else 1
    first = 0xE840 | pre << 8 | up << 7 | wb << 5 | load << 4 | n
    if literal:
        imm = 16 if up else 0
    return Case((first, 0x0200 | imm), {} if literal else {1: RAM + 64})


def exclusive32(size, clear, rng):
    if size == 4:
        load, store = (0xE851, 0x0F00), (0xE841, 0x0200)
    else:
        op = 0x40 if size == 1 else 0x50
        load, store = (0xE8D1, 0x0F0F | op), (0xE8C1, 0x0F02 | op)
    middle = (0xF3BF, 0x8F2F) if clear else (0xBF00,)
    return Case((*load, *middle, *store, *store), {1: RAM}, steps=4)


def table32(half, pc, rng):
    n = 15 if pc else 1
    index, offset = rng.randrange(8), rng.randrange(32)
    address = PC + 4 if pc else RAM
    return Case(
        (0xE8D0 | n, 0xF000 | half << 4 | 2),
        {1: RAM, 2: index},
        ((address + (index << half) - BASE, 2 if half else 1, offset),),
    )


def branch32(form, rng):
    offset = rng.randrange(-64, 64) * 2
    if form == "cond":
        value = offset & ((1 << 21) - 1)
        return Case(
            (
                0xF000 | (value >> 20) << 10 | rng.randrange(14) << 6 | (value >> 12 & 63),
                0x8000 | (value >> 18 & 1) << 13 | (value >> 19 & 1) << 11 | (value >> 1 & 2047),
            )
        )
    value = offset & ((1 << 25) - 1)
    sign = value >> 24
    j1, j2 = 1 ^ (value >> 23 & 1) ^ sign, 1 ^ (value >> 22 & 1) ^ sign
    return Case(
        (
            0xF000 | sign << 10 | (value >> 12 & 1023),
            (0xD000 if form == "bl" else 0x9000) | j1 << 13 | j2 << 11 | (value >> 1 & 2047),
        )
    )


def base_in_list(load, rng):
    return Case(((0xC800 if load else 0xC000) | rng.randrange(1, 256) | 1,), {0: RAM})


def alu_pc(add, rng):
    return Case((0x4487 if add else 0x4687,), {0: 0x100 if add else 0x1300})


def pc_load(form, rng):
    if form == "ldr":
        return Case((0xF8D1, 0xF000), {1: RAM}, ((RAM - BASE, 4, 0x1301),))
    if form == "pop16":
        return Case((0xBD01,), {13: RAM}, ((RAM + 4 - BASE, 4, 0x1301),))
    return Case((0xE8B1, 0x8001), {1: RAM}, ((RAM + 4 - BASE, 4, 0x1301),))


def it_case(mask, wide, rng):
    instruction = (0xF101, 0x0001) if wide else (0x3001,)
    slots = 4 - ((mask & -mask).bit_length() - 1)
    return Case((0xBF00 | rng.randrange(14) << 4 | mask, *instruction * slots), steps=1 + slots)


def fixed(code, rng):
    return Case(code)


def undefined(wide, rng):
    # UDF's immediate field is random; the encoding is architecturally undefined.
    if wide:
        imm = rng.randrange(65536)
        return Case((0xF7F0 | imm >> 12, 0xA000 | (imm & 4095)), undefined=True)
    return Case((0xDE00 | rng.randrange(256),), undefined=True)


def undefined_class(first, second):
    """A5 table holes only; no invalid-operand UNPREDICTABLE encodings."""
    if first < 0xE800:
        if first & 0xFF00 in [0xB700, 0xB800] or first & 0xFFC0 == 0xBA80:
            return "A5.2 unallocated"
        if first & 0xFF00 == 0xDE00:
            return "UDF T1"
        return None
    if first & 0xFE00 == 0xEA00 and (first >> 5 & 15) in [5, 7, 9, 12, 15]:
        return "A5-22 unallocated"
    if first & 0xF800 == 0xF000 and not second & 0x8000:
        if not first & 0x200 and (first >> 5 & 15) in [5, 6, 7, 9, 12, 15]:
            return "A5-10 unallocated"
        if first & 0x200 and (first >> 4 & 31) not in [0, 4, 10, 12, 16, 18, 20, 22, 24, 26, 28]:
            return "A5-12 unallocated"
    if first & 0xFE00 == 0xF800 and (first >> 4 & 7) == 7:
        return "A5-9 undefined single-transfer space"
    if first & 0xFF00 == 0xFA00 and second >> 12 != 15:
        return "A5.3.12 fixed second-halfword prefix"
    if first & 0xFF80 == 0xFB00 and second & 0xC0:
        return "A5.3.16 fixed multiply bits"
    return None


def raw_undefined(wide, rng):
    while True:
        first, second = rng.getrandbits(16), rng.getrandbits(16)
        if (first >= 0xE800) == wide and undefined_class(first, second):
            return Case((first, second) if wide else (first,), undefined=True)


def multiply_hole(rng):
    return Case((0xFB00 | rng.randrange(128), rng.getrandbits(16) | 0xC0), undefined=True)


def catalog():
    result = []

    def add(name, function, *args):
        result.append((name, partial(function, *args)))

    for op, name in enumerate(
        [
            "and",
            "eor",
            "lsl",
            "lsr",
            "asr",
            "adc",
            "sbc",
            "ror",
            "tst",
            "rsb",
            "cmp",
            "cmn",
            "orr",
            "mul",
            "bic",
            "mvn",
        ]
    ):
        add(f"16.{name}.reg", alu16, op)
    for immediate in range(2):
        for sub in range(2):
            add(
                f"16.{'sub' if sub else 'add'}.{'imm3' if immediate else 'reg3'}",
                add16,
                immediate,
                sub,
            )
    for op, name in enumerate(["add", "cmp", "mov"]):
        add(f"16.{name}.high", special16, op)
    for kind, name in enumerate(["str", "strh", "strb", "ldrsb", "ldr", "ldrh", "ldrb", "ldrsh"]):
        add(f"16.{name}.reg", memory16, kind, "reg")
    for kind, name in enumerate(["str", "ldr", "strb", "ldrb", "strh", "ldrh"]):
        add(f"16.{name}.imm", memory16, kind, "imm")
    for kind, name in enumerate(["str", "ldr"]):
        add(f"16.{name}.sp", memory16, kind, "sp")
    add("16.ldr.literal", memory16, 0, "literal")
    for op, name in enumerate(["sxth", "sxtb", "uxth", "uxtb"]):
        add(f"16.{name}", extend16, op)
    for op, name in [(0, "rev"), (1, "rev16"), (3, "revsh")]:
        add(f"16.{name}", reverse16, op)
    for form, name in [
        (0xA000, "adr"),
        (0xA800, "add.sp"),
        (0xB000, "add.sp.wb"),
        (0xB080, "sub.sp"),
    ]:
        add(f"16.{name}", address16, form)
    for form in ["b", "cond", "cb", "bx", "blx"]:
        add(f"16.branch.{form}", branch16, form)
    for load in range(2):
        for stack in range(2):
            add(f"16.multiple.{load}.{stack}", multiple16, load, stack)
    for form in ["reg", "imm"]:
        for op, name in [
            (0, "and"),
            (1, "bic"),
            (2, "orr"),
            (3, "orn"),
            (4, "eor"),
            (8, "add"),
            (10, "adc"),
            (11, "sbc"),
            (13, "sub"),
            (14, "rsb"),
        ]:
            add(f"32.{name}.{form}", dp32, op, form, "normal")
        for op, name in [(2, "mov"), (3, "mvn")]:
            add(f"32.{name}.{form}", dp32, op, form, "move")
        for op, name in [(0, "tst"), (4, "teq"), (8, "cmn"), (13, "cmp")]:
            add(f"32.{name}.{form}", dp32, op, form, "test")
    for op, name in [
        (0, "addw"),
        (10, "subw"),
        (4, "movw"),
        (12, "movt"),
        (16, "ssat.lsl"),
        (18, "ssat.asr"),
        (24, "usat.lsl"),
        (26, "usat.asr"),
        (20, "sbfx"),
        (28, "ubfx"),
        (22, "bfi.bfc"),
    ]:
        add(f"32.{name}", plain32, op)
    for kind, name in enumerate(
        [
            "lsl",
            "lsls",
            "lsr",
            "lsrs",
            "asr",
            "asrs",
            "ror",
            "rors",
            "sxth",
            "uxth",
            "sxtb",
            "uxtb",
            "rev",
            "rev16",
            "rbit",
            "revsh",
            "clz",
        ]
    ):
        add(f"32.{name}.register", register32, kind)
    for kind, name in enumerate(
        ["mul", "mla", "mls", "smull", "umull", "smlal", "umlal", "sdiv", "udiv"]
    ):
        add(f"32.{name}", multiply32, kind)
    for op, name in [
        (0, "strb"),
        (1, "ldrb"),
        (2, "strh"),
        (3, "ldrh"),
        (4, "str"),
        (5, "ldr"),
        (17, "ldrsb"),
        (19, "ldrsh"),
    ]:
        for form in ["imm12", "reg", "pre+", "pre-", "post+", "post-", "negative", "t"]:
            add(f"32.{name}.{form}", single32, op, form)
        if op & 1:
            add(f"32.{name}.literal", single32, op, "literal")
    for load in range(2):
        for db in range(2):
            for wb in range(2):
                for stack in range(2):
                    add(f"32.multiple.{load}.{db}.{wb}.{stack}", multiple32, load, db, wb, stack)
    for load in range(2):
        for pre, up, wb in [(0, 0, 1), (0, 1, 1), (1, 0, 0), (1, 1, 0), (1, 0, 1), (1, 1, 1)]:
            add(f"32.dual.{load}.{pre}.{up}.{wb}", dual32, load, pre, up, wb, False)
    for up in range(2):
        add(f"32.ldrd.literal.{up}", dual32, 1, 1, up, 0, True)
    for size in [1, 2, 4]:
        for clear in range(2):
            add(f"32.exclusive.{size}.{clear}", exclusive32, size, clear)
    for half in range(2):
        for pc in range(2):
            add(f"32.table.{half}.{pc}", table32, half, pc)
    for form in ["b", "cond", "bl"]:
        add(f"32.branch.{form}", branch32, form)
    for form in ["ldr", "pop16", "ldm"]:
        add(f"pc-load.{form}", pc_load, form)
    for mask in range(1, 16):
        for wide in range(2):
            add(f"it.{mask}.{wide}", it_case, mask, wide)
    for code, name in [
        ((0xF3EF, 0x8000), "mrs"),
        ((0xF380, 0x8800), "msr"),
        ((0xF3BF, 0x8F4F), "dsb"),
        ((0xF3BF, 0x8F5F), "dmb"),
        ((0xF3BF, 0x8F6F), "isb"),
        ((0xF3BF, 0x8F2F), "clrex"),
        ((0xF891, 0xF040), "pld"),
        ((0xF991, 0xF040), "pli"),
    ]:
        add(f"32.{name}", fixed, code)
    for hint in range(5):
        add(f"16.hint.{hint}", fixed, (0xBF00 | hint << 4,))
        add(f"32.hint.{hint}", fixed, (0xF3AF, 0x8000 | hint))
    for wide in range(2):
        add(f"udf.{wide}", undefined, wide)
    for wide in range(2):
        add(f"raw.undefined.{wide}", raw_undefined, bool(wide))
    add("32.multiply.reserved", multiply_hole)
    for load in range(2):
        add(f"16.base-in-list.{load}", base_in_list, load)
    for add_to_pc in range(2):
        add(f"16.alu-pc.{add_to_pc}", alu_pc, add_to_pc)
    return result


def sequence_dp(rng, flags=None):
    op = rng.choice([0, 1, 2, 3, 4, 8, 10, 11, 13, 14])
    case = dp32(op, rng.choice(["reg", "imm"]), "normal", rng)
    first, second = case.code
    n, d = rng.randrange(8), rng.randrange(8)
    first, second = (first & ~15) | n, (second & ~0xF00) | d << 8
    if first & 0xFE00 == 0xEA00:
        second = (second & ~15) | rng.randrange(8)
    if flags is not None:
        first = (first & ~16) | int(flags) << 4
    return first, second


def sequence_alu(kind, rng):
    code = []
    for _ in range(7):
        if kind == "narrow" or (kind == "mixed" and rng.randrange(2)):
            code.extend(alu16(rng.randrange(16), rng).code)
        else:
            code.extend(sequence_dp(rng, flags=True if kind == "carry" else None))
    code.extend((0xF3EF, 0x8300))  # mrs r3,APSR observes the final NZCVQ.
    return Case(tuple(code), steps=8)


def sequence_it(mask, rng):
    cond = rng.randrange(14)
    slots = 4 - ((mask & -mask).bit_length() - 1)
    code = [0x4288, 0xBF00 | cond << 4 | mask]  # cmp r0,r1; it...
    for _ in range(slots):
        # Explicit S bits and compare/test instructions may change later conditions.
        if rng.randrange(2):
            code.extend(sequence_dp(rng, flags=True))
        else:
            code.extend(alu16(rng.choice([0, 5, 6, 8, 10, 11]), rng).code)
    code.extend((0xF3EF, 0x8300))
    return Case(tuple(code), steps=3 + slots)


def sequence_memory(rng):
    kind, offset = rng.randrange(3), rng.randrange(16)
    store, load = [(0x6008, 0x680A), (0x8008, 0x880A), (0x7008, 0x780A)][kind]
    return Case(
        (
            0x2000 | rng.randrange(256),  # movs r0,#imm
            0x3000 | rng.randrange(256),  # adds r0,#imm
            0x2800 | rng.randrange(256),  # cmp r0,#imm
            store | offset << 6,
            load | offset << 6,
            0x4050,  # eors r0,r2
            0xF3EF,
            0x8300,  # mrs r3,APSR
            0xBF00,
        ),
        {1: RAM},
        steps=8,
    )


def sequence_apsr(rng):
    return Case(
        (
            *sequence_dp(rng, flags=True),
            0xF381,
            0x8800,  # msr APSR_nzcvq,r1 overwrites all pending flags.
            0x4148,  # adcs r0,r1
            0xF3EF,
            0x8300,
            0xF301,
            rng.randrange(32),  # ssat r0,#sat,r1 preserves NZCV and sets sticky Q.
            0xF3EF,
            0x8400,
            0xF381,
            0x8800,
            0xF3EF,
            0x8500,
        ),
        steps=8,
    )


def sequence_catalog():
    result = [
        (f"sequence.alu.{kind}", partial(sequence_alu, kind))
        for kind in ["narrow", "mixed", "carry"]
    ]
    result += [(f"sequence.it.{mask}", partial(sequence_it, mask)) for mask in range(1, 16)]
    result += [("sequence.memory", sequence_memory), ("sequence.apsr", sequence_apsr)]
    return result
