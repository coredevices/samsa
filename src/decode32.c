/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include "alu.h"
static bool dp(SamsaCpu *c, unsigned op, unsigned n, unsigned d, uint32_t b, unsigned cy,
               bool flags, uint32_t pc) {
    uint32_t a = reg(c, n, pc), v;
    bool test = d == 15 && flags && (op == 0 || op == 4 || op == 8 || op == 13);
    if (d == 15 && !test)
        return undef(c);
    if (n == 15 && op != 2 && op != 3)
        return undef(c);
    switch (op) {
    case 0:
        v = a & b;
        break;
    case 1:
        v = a & ~b;
        break;
    case 2:
        v = n == 15 ? b : a | b;
        break;
    case 3:
        v = n == 15 ? ~b : a | ~b;
        break;
    case 4:
        v = a ^ b;
        break;
    case 8:
        v = add_c(c, a, b, 0, flags);
        goto result;
    case 10:
        v = add_c(c, a, b, (c->_flags & C) != 0, flags);
        goto result;
    case 11:
        v = add_c(c, a, ~b, (c->_flags & C) != 0, flags);
        goto result;
    case 13:
        v = add_c(c, a, ~b, 1, flags);
        goto result;
    case 14:
        v = add_c(c, ~a, b, 1, flags);
        goto result;
    default:
        return undef(c);
    }
    if (flags)
        nzc(c, v, cy);
result:
    if (!test)
        put(c, d, v);
    return true;
}
static bool control(SamsaCpu *c, uint16_t h, uint16_t l, uint32_t pc) {
    unsigned form = l & 0xd000u;
    if (form == 0x9000u || form == 0xd000u) {
        unsigned s = h >> 10 & 1u, j1 = l >> 13 & 1u, j2 = l >> 11 & 1u;
        uint32_t offset = (s << 24) | ((1u ^ j1 ^ s) << 23) | ((1u ^ j2 ^ s) << 22) |
                          ((h & 1023u) << 12) | ((l & 2047u) << 1);
        if (!branch(c, pc + 4u + sx(offset, 25), false))
            return false;
        if (form == 0xd000u)
            c->r[14] = (pc + 4u) | 1u;
        return true;
    }
    if (form != 0x8000u)
        return undef(c); /* BLX immediate is A/R profile only. */
    unsigned cond = h >> 6 & 15u;
    if (cond < 14) {
        if (c->itstate)
            return usage(c);
        uint32_t offset = ((h >> 10 & 1u) << 20) | ((l >> 11 & 1u) << 19) | ((l >> 13 & 1u) << 18) |
                          ((h & 63u) << 12) | ((l & 2047u) << 1);
        if (condition(c, cond))
            c->r[15] = pc + 4u + sx(offset, 21);
        return true;
    }
    if (h == 0xf3efu && (l & 0xf0ffu) == 0x8000u) {
        unsigned d = l >> 8 & 15u;
        if (d == 13 || d == 15)
            return undef(c);
        put(c, d, (uint32_t)c->_flags << 24 | (uint32_t)c->_ge << 16);
        return true;
    }
    if ((h & 0xfff0u) == 0xf380u && (l & 0xf3ffu) == 0x8000u && (l & 0xc00u)) {
        /* APSR_nzcvq (mask 10), with the DSP extension's APSR_g (01, 11). */
        unsigned n = h & 15u;
        if (n == 13 || n == 15)
            return undef(c);
        if (l & 0x800u)
            c->_flags = (uint8_t)(c->r[n] >> 24 & 0xf8u);
        if (l & 0x400u)
            c->_ge = (uint8_t)(c->r[n] >> 16 & 15u);
        return true;
    }
    if (h == 0xf3afu && (l & 0xff00u) == 0x8000u)
        return true;
    if (h == 0xf3bfu && (l & 0xff00u) == 0x8f00u) {
        unsigned op = l >> 4 & 15u;
        if (op == 2) {
            c->_exclusive_size = 0;
            return true;
        }
        if (op >= 4 && op <= 6)
            return true;
    }
    return undef(c);
}
static bool plain(SamsaCpu *c, uint16_t h, uint16_t l, uint32_t pc) {
    unsigned op = h >> 4 & 31u, n = h & 15u, d = l >> 8 & 15u;
    unsigned imm12 = (h >> 10 & 1u) << 11 | (l >> 12 & 7u) << 8 | (l & 255u);
    unsigned amount = (l >> 12 & 7u) * 4u + (l >> 6 & 3u);
    unsigned cy = (c->_flags & C) != 0;
    uint32_t v, mask;
    if (d == 15)
        return undef(c);
    if (op == 0 || op == 10) {
        v = n == 15 ? (pc + 4u) & ~3u : c->r[n];
        put(c, d, op == 0 ? v + imm12 : v - imm12);
        return true;
    }
    if (op == 4 || op == 12) {
        v = n << 12 | imm12;
        put(c, d, op == 4 ? v : (c->r[d] & 0xffffu) | (v << 16));
        return true;
    }
    if (n == 15 || n == 13 || d == 13) {
        if (!(op == 22 && n == 15 && d != 13))
            return undef(c);
    }
    if (op == 20 || op == 28) {
        unsigned width = (l & 31u) + 1u;
        if ((l & 0x20u) || amount + width > 32)
            return undef(c);
        mask = UINT32_MAX >> (32u - width);
        v = (c->r[n] >> amount) & mask;
        put(c, d, op == 20 ? sx(v, width) : v);
        return true;
    }
    if (op == 22) {
        unsigned msb = l & 31u;
        if ((l & 0x20u) || msb < amount)
            return undef(c);
        mask = (UINT32_MAX >> (31u - msb + amount)) << amount;
        put(c, d, (c->r[d] & ~mask) | ((n == 15 ? 0 : c->r[n] << amount) & mask));
        return true;
    }
    if (op == 16 || op == 18 || op == 24 || op == 26) {
        bool sign = op < 24, asr = (op & 2u) != 0;
        if ((l & 0x20u) || (asr && !amount))
            return undef(c); /* *SAT16 is DSP. */
        v = shift(c->r[n], asr ? 2 : 0, amount, &cy);
        unsigned bits = (l & 31u) + (sign ? 1u : 0u);
        int64_t value = (int64_t)(v & 0x7fffffffu) - (int64_t)(v >> 31) * INT64_C(0x80000000);
        int64_t lo = sign ? -(INT64_C(1) << (bits - 1u)) : 0;
        int64_t hi = sign ? (INT64_C(1) << (bits - 1u)) - 1 : (INT64_C(1) << bits) - 1;
        if (value < lo) {
            value = lo;
            c->_flags |= Q;
        }
        if (value > hi) {
            value = hi;
            c->_flags |= Q;
        }
        put(c, d, (uint32_t)value);
        return true;
    }
    return undef(c);
}
static bool dual(SamsaCpu *c, uint16_t h, uint16_t l, uint32_t pc) {
    unsigned n = h & 15u, t = l >> 12, t2 = l >> 8 & 15u;
    uint32_t base = n == 15 ? (pc + 4u) & ~3u : c->r[n], addr, v;
    if ((h & 0xffe0u) == 0xe840u) { /* STREX / LDREX word */
        bool load = (h & 16u) != 0;
        unsigned d = t2;
        if (n == 15 || (t == 13 || t == 15) ||
            (load ? d != 15 : (d == 13 || d == 15) || d == n || d == t))
            return undef(c);
        addr = base + (l & 255u) * 4u;
        if (!aligned(c, addr, 4))
            return false;
        if (load) {
            if (!mem_read(c, addr, 4, SAMSA_R, &c->r[t]))
                return false;
            c->exclusive_addr = addr;
            c->_exclusive_size = 4;
        } else {
            bool success = c->_exclusive_size == 4 && c->exclusive_addr == addr;
            c->_exclusive_size = 0;
            if (success && !mem_write(c, addr, 4, c->r[t]))
                return false;
            put(c, d, success ? 0 : 1);
        }
        return true;
    }
    if ((h & 0xffe0u) == 0xe8c0u) {
        bool load = (h & 16u) != 0;
        unsigned op = l >> 4 & 15u;
        if (load && t == 15 && t2 == 0 && op <= 1) {
            if (!last_it(c))
                return usage(c);
            unsigned m = l & 15u;
            if ((m == 13 || m == 15))
                return undef(c);
            /* Table branch uses unaligned PC, unlike literals. */
            addr = reg(c, n, pc) + (c->r[m] << op);
            if (!mem_read(c, addr, op ? 2 : 1, SAMSA_R, &v))
                return false;
            return branch(c, pc + 4u + 2u * v, false);
        }
        if ((op != 4 && op != 5) || t2 != 15 || n == 15 || (t == 13 || t == 15))
            return undef(c);
        unsigned size = op == 4 ? 1 : 2, d = l & 15u;
        if (load ? d != 15 : (d == 13 || d == 15) || d == n || d == t)
            return undef(c);
        addr = base;
        if (!aligned(c, addr, size))
            return false;
        if (load) {
            if (!mem_read(c, addr, size, SAMSA_R, &c->r[t]))
                return false;
            c->exclusive_addr = addr;
            c->_exclusive_size = (uint8_t)size;
        } else {
            bool success = c->_exclusive_size == size && c->exclusive_addr == addr;
            c->_exclusive_size = 0;
            if (success && !mem_write(c, addr, size, c->r[t]))
                return false;
            put(c, d, success ? 0 : 1);
        }
        return true;
    }
    bool pre = (h & 256u) != 0, up = (h & 128u) != 0, wb = (h & 32u) != 0, load = (h & 16u) != 0;
    if ((!pre && !wb) || (t == 13 || t == 15) || (t2 == 13 || t2 == 15) || (load && t == t2) ||
        (n == 15 && (!load || wb)) || (wb && (n == t || n == t2)))
        return undef(c);
    uint32_t offset = (l & 255u) * 4u, next = up ? base + offset : base - offset;
    addr = pre ? next : base;
    if (!aligned(c, addr, 4))
        return false;
    if (load) {
        uint32_t a, b;
        if (!mem_read(c, addr, 4, SAMSA_R, &a) || !mem_read(c, addr + 4u, 4, SAMSA_R, &b))
            return false;
        c->r[t] = a;
        c->r[t2] = b;
    } else if (!mem_write(c, addr, 4, c->r[t]) || !mem_write(c, addr + 4u, 4, c->r[t2]))
        return false;
    if (wb)
        put(c, n, next);
    return true;
}
static bool single(SamsaCpu *c, uint16_t h, uint16_t l, uint32_t pc) {
    unsigned op = h >> 4 & 31u, n = h & 15u, t = l >> 12;
    unsigned kind = op & 7u;
    bool load = (kind & 1u) != 0, sign = (op & 16u) != 0;
    if (kind >= 6 || (sign && (!load || kind == 5)))
        return undef(c);
    unsigned size = kind < 2 ? 1 : kind < 4 ? 2 : 4;
    if (load && size < 4 && t == 15)
        return true; /* PLD/PLI and unallocated hints. */
    if ((!load && t == 15) || (t == 15 && !last_it(c)))
        return undef(c);
    uint32_t base = n == 15 ? (pc + 4u) & ~3u : c->r[n], addr, next = base, offset;
    bool wb = false;
    if (n == 15) {
        if (!load)
            return undef(c);
        offset = l & 4095u;
        addr = h & 128u ? base + offset : base - offset;
    } else if (op & 8u)
        addr = base + (l & 4095u);
    else if (!(l & 0x800u)) {
        if (l & 0xfc0u)
            return undef(c);
        unsigned m = l & 15u;
        if ((m == 13 || m == 15))
            return undef(c);
        addr = base + (c->r[m] << (l >> 4 & 3u));
    } else {
        bool pre = (l & 0x400u) != 0, up = (l & 0x200u) != 0;
        wb = (l & 0x100u) != 0;
        if ((!pre && !wb) || (wb && n == t))
            return undef(c);
        offset = l & 255u;
        next = up ? base + offset : base - offset;
        addr = pre ? next : base;
    }
    if (load) {
        uint32_t value;
        if (t == 15 && !aligned(c, addr, 4))
            return false;
        if (!mem_read(c, addr, size, SAMSA_R, &value))
            return false;
        if (sign)
            value = sx(value, size * 8u);
        if (t == 15) {
            if (!branch(c, value, true))
                return false;
        } else
            put(c, t, value);
    } else if (!mem_write(c, addr, size, c->r[t]))
        return false;
    if (wb)
        put(c, n, next);
    return true;
}
static bool data_reg(SamsaCpu *c, uint16_t h, uint16_t l) {
    unsigned n = h & 15u, d = l >> 8 & 15u, m = l & 15u;
    unsigned op = h >> 4 & 15u, sub = l >> 4 & 15u, cy = (c->_flags & C) != 0;
    if ((l >> 12) != 15 || d == 13 || d == 15 || m == 13 || m == 15)
        return undef(c);
    uint32_t v;
    if (op < 8 && sub == 0) {
        if (n == 13 || n == 15)
            return undef(c);
        v = shift(c->r[n], op >> 1, c->r[m] & 255u, &cy);
        put(c, d, v);
        if (op & 1u)
            nzc(c, v, cy);
        return true;
    }
    if ((sub & 12u) == 8 && (op == 0 || op == 1 || op == 4 || op == 5)) {
        /* With Rn, these are the ARMv7E-M SXTAH/UXTAH/SXTAB/UXTAB, which
           Cortex-M4 builds of real apps use. */
        if (n == 13)
            return undef(c);
        v = shift(c->r[m], 3, (sub & 3u) * 8u, &cy);
        unsigned bits = op < 2 ? 16 : 8;
        v &= UINT32_MAX >> (32u - bits);
        v = op & 1u ? v : sx(v, bits);
        put(c, d, n == 15 ? v : c->r[n] + v);
        return true;
    }
    if ((op == 9 || op == 11) && n == m && sub >= 8 && sub <= 11) {
        v = c->r[m];
        if (op == 11) {
            if (sub != 8)
                return undef(c);
            unsigned count = 0;
            while (count < 32 && !(v & 0x80000000u)) {
                ++count;
                v <<= 1;
            }
            v = count;
        } else if (sub == 10) {
            uint32_t r = 0;
            for (unsigned i = 0; i < 32; ++i) {
                r = r << 1 | (v & 1u);
                v >>= 1;
            }
            v = r;
        } else {
            v = ((v & 0xff00ffu) << 8) | ((v >> 8) & 0xff00ffu);
            if (sub == 8)
                v = v << 16 | v >> 16;
            if (sub == 11)
                v = sx(v & 0xffffu, 16);
        }
        put(c, d, v);
        return true;
    }
    return undef(c);
}
static bool multiply(SamsaCpu *c, uint16_t h, uint16_t l) {
    unsigned n = h & 15u, m = l & 15u, d = l >> 8 & 15u, a = l >> 12;
    if (n == 13 || n == 15 || m == 13 || m == 15 || d == 13 || d == 15)
        return undef(c);
    if ((h & 0xfff0u) == 0xfb00u && (l & 0xe0u) == 0) {
        if ((l & 16u) && a == 15)
            return undef(c);
        if (a == 13)
            return undef(c);
        uint32_t product = c->r[n] * c->r[m], acc = a == 15 ? 0 : c->r[a];
        put(c, d, l & 16u ? acc - product : acc + product);
        return true;
    }
    if ((h & 0xfff0u) == 0xfb10u && (l & 0xc0u) == 0) {
        /* ARMv7E-M SMLA<x><y>/SMUL<x><y>, which Cortex-M4 builds of real
           apps use for fixed-point math. */
        if (a == 13)
            return undef(c);
        int32_t x = (int16_t)(l & 32u ? c->r[n] >> 16 : c->r[n]);
        int32_t y = (int16_t)(l & 16u ? c->r[m] >> 16 : c->r[m]);
        int64_t v = (int64_t)(x * y) + (a == 15 ? 0 : (int32_t)c->r[a]);
        if (v != (int32_t)v)
            c->_flags |= Q;
        put(c, d, (uint32_t)v);
        return true;
    }
    unsigned op = h >> 4 & 7u;
    if ((h & 0xffd0u) == 0xfb90u && (l & 0xf0f0u) == 0xf0f0u) {
        uint32_t x = c->r[n], y = c->r[m], v = 0;
        if (y) {
            if (op == 3)
                v = x / y;
            else {
                bool neg = ((x ^ y) >> 31) != 0;
                x = x >> 31 ? 0u - x : x;
                y = y >> 31 ? 0u - y : y;
                v = x / y;
                if (neg)
                    v = 0u - v;
            }
        }
        put(c, d, v);
        return true;
    }
    if (!(h & 128u) || (l & 0xf0u) || (op != 0 && op != 2 && op != 4 && op != 6) || a == 13 ||
        a == 15 || a == d)
        return undef(c);
    uint32_t x = c->r[n], y = c->r[m];
    uint64_t product = (uint64_t)x * y;
    if (!(op & 2u)) { /* Signed product via unsigned two's-complement arithmetic. */
        if (x >> 31)
            product -= (uint64_t)y << 32;
        if (y >> 31)
            product -= (uint64_t)x << 32;
    }
    if (op & 4u)
        product += (uint64_t)c->r[d] << 32 | c->r[a];
    c->r[a] = (uint32_t)product;
    put(c, d, (uint32_t)(product >> 32));
    return true;
}
enum {
    T32_SHIFTED,
    T32_CONTROL,
    T32_PLAIN,
    T32_MODIFIED,
    T32_MULTIPLE,
    T32_DUAL,
    T32_SINGLE,
    T32_REGISTER,
    T32_MULTIPLY,
    T32_UNDEFINED,
};
uint8_t classify32(uint16_t h, uint16_t l) {
    if ((h & 0xfe00u) == 0xea00u)
        return T32_SHIFTED;
    if ((h & 0xf800u) == 0xf000u)
        return l & 0x8000u ? T32_CONTROL : h & 0x200u ? T32_PLAIN : T32_MODIFIED;
    if ((h & 0xfe40u) == 0xe800u)
        return T32_MULTIPLE;
    if ((h & 0xfe40u) == 0xe840u)
        return T32_DUAL;
    if ((h & 0xfe00u) == 0xf800u)
        return T32_SINGLE;
    if ((h & 0xff00u) == 0xfa00u)
        return T32_REGISTER;
    return (h & 0xff00u) == 0xfb00u ? T32_MULTIPLY : T32_UNDEFINED;
}
bool execute32(SamsaCpu *c, unsigned op, uint16_t h, uint16_t l, uint32_t pc) {
    switch (op) {
    case T32_SHIFTED: {
        if (l & 0x8000u)
            return undef(c);
        unsigned amount = (l >> 12 & 7u) * 4u + (l >> 6 & 3u), type = l >> 4 & 3u;
        unsigned m = l & 15u, cy = (c->_flags & C) != 0;
        if (m == 15)
            return undef(c);
        if (!amount && type) {
            if (type == 3)
                type = 4, amount = 1;
            else
                amount = 32;
        }
        uint32_t b = shift(c->r[m], type, amount, &cy);
        return dp(c, h >> 5 & 15u, h & 15u, l >> 8 & 15u, b, cy, (h & 16u) != 0, pc);
    }
    case T32_CONTROL:
        return control(c, h, l, pc);
    case T32_PLAIN:
        return plain(c, h, l, pc);
    case T32_MODIFIED: {
        unsigned imm = (h >> 10 & 1u) << 11 | (l >> 12 & 7u) << 8 | (l & 255u),
                 cy = (c->_flags & C) != 0;
        if ((imm & 0xc00u) == 0 && (imm & 0x300u) && !(imm & 255u))
            return undef(c);
        uint32_t b = imm_expand(imm, &cy);
        return dp(c, h >> 5 & 15u, h & 15u, l >> 8 & 15u, b, cy, (h & 16u) != 0, pc);
    }
    case T32_MULTIPLE: {
        unsigned n = h & 15u, mode = h >> 7 & 3u;
        bool load = (h & 16u) != 0, wb = (h & 32u) != 0;
        if ((mode != 1 && mode != 2) || (l & 0x2000u) || (!load && (l & 0x8000u)) ||
            (load && (l & 0xc000u) == 0xc000u) || (wb && (l & (1u << n))))
            return undef(c);
        unsigned count = 0;
        for (unsigned i = 0; i < 16; ++i)
            count += (l >> i) & 1u;
        if (count < 2)
            return undef(c);
        return multiple(c, n, l, load, mode == 2, wb);
    }
    case T32_DUAL:
        return dual(c, h, l, pc);
    case T32_SINGLE:
        return single(c, h, l, pc);
    case T32_REGISTER:
        return data_reg(c, h, l);
    case T32_MULTIPLY:
        return multiply(c, h, l);
    default:
        return undef(c);
    }
}

bool decode32(SamsaCpu *c, uint16_t h, uint16_t l, uint32_t pc) {
    return execute32(c, classify32(h, l), h, l, pc);
}
