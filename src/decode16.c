/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include "alu.h"
bool multiple(SamsaCpu *c, unsigned rn, uint32_t list, bool load, bool db, bool wb) {
    unsigned count = 0;
    for (unsigned i = 0; i < 16; ++i)
        count += (list >> i) & 1u;
    if (!count || rn == 15)
        return undef(c);
    if ((list & 0x8000u) && !last_it(c))
        return usage(c);
    uint32_t base = c->r[rn], addr = db ? base - count * 4u : base, v;
    if (!aligned(c, addr, 4))
        return false;
    for (unsigned i = 0; i < 16; ++i) {
        if (!(list & (1u << i)))
            continue;
        if (load) {
            if (!mem_read(c, addr, 4, SAMSA_R, &v))
                return false;
            if (i == 15) {
                if (!branch(c, v, true))
                    return false;
            } else
                c->r[i] = v;
        } else if (!mem_write(c, addr, 4, i == rn ? base : c->r[i]))
            return false;
        addr += 4;
    }
    if (wb)
        c->r[rn] = db ? base - count * 4u : addr;
    return true;
}
static uint32_t reverse(uint32_t x, unsigned op) {
    uint32_t r = ((x & 0xff00ffu) << 8) | ((x >> 8) & 0xff00ffu);
    if (op == 0)
        return r << 16 | r >> 16;
    if (op == 1)
        return r;
    return sx(r & 0xffffu, 16);
}
bool decode16(SamsaCpu *c, uint16_t h, uint32_t pc, bool it) {
    unsigned d = h & 7u, n = h >> 3 & 7u, m = h >> 6 & 7u;
    unsigned cy = (c->_flags & C) != 0;
    uint32_t a, b, v, addr;
    if (h < 0x1800u) {
        unsigned type = h >> 11, amount = h >> 6 & 31u;
        if (type && !amount)
            amount = 32;
        v = shift(c->r[n], type, amount, &cy);
        put(c, d, v);
        if (!it)
            nzc(c, v, cy);
        return true;
    }
    if (h < 0x2000u) {
        b = h & 0x400u ? m : c->r[m];
        put(c, d, add_c(c, c->r[n], h & 0x200u ? ~b : b, (h >> 9) & 1u, !it));
        return true;
    }
    if (h < 0x4000u) {
        d = h >> 8 & 7u;
        b = h & 255u;
        switch (h >> 11 & 3u) {
        case 0:
            put(c, d, b);
            if (!it)
                nz(c, b);
            break;
        case 1:
            (void)add_c(c, c->r[d], ~b, 1, true);
            break;
        case 2:
            put(c, d, add_c(c, c->r[d], b, 0, !it));
            break;
        case 3:
            put(c, d, add_c(c, c->r[d], ~b, 1, !it));
            break;
        }
        return true;
    }
    if ((h & 0xfc00u) == 0x4000u) {
        a = c->r[d];
        b = c->r[n];
        bool flags = !it;
        switch (h >> 6 & 15u) {
        case 0:
            v = a & b;
            break;
        case 1:
            v = a ^ b;
            break;
        case 2:
        case 3:
        case 4:
        case 7:
            v = shift(a, (h >> 6 & 15u) == 7 ? 3 : (h >> 6 & 15u) - 2u, b & 255u, &cy);
            put(c, d, v);
            if (flags)
                nzc(c, v, cy);
            return true;
        case 5:
            put(c, d, add_c(c, a, b, cy, flags));
            return true;
        case 6:
            put(c, d, add_c(c, a, ~b, cy, flags));
            return true;
        case 8:
            nz(c, a & b);
            return true;
        case 9:
            put(c, d, add_c(c, ~b, 0, 1, flags));
            return true;
        case 10:
            (void)add_c(c, a, ~b, 1, true);
            return true;
        case 11:
            (void)add_c(c, a, b, 0, true);
            return true;
        case 12:
            v = a | b;
            break;
        case 13:
            v = a * b;
            break;
        case 14:
            v = a & ~b;
            break;
        default:
            v = ~b;
            break;
        }
        put(c, d, v);
        if (flags)
            nz(c, v);
        return true;
    }
    if ((h & 0xfc00u) == 0x4400u) {
        d |= h >> 4 & 8u;
        n = h >> 3 & 15u;
        a = reg(c, d, pc);
        b = reg(c, n, pc);
        switch (h >> 8 & 3u) {
        case 0:
            v = a + b;
            break;
        case 1:
            (void)add_c(c, a, ~b, 1, true);
            return true;
        case 2:
            v = b;
            break;
        default:
            if ((h & 7u) || (n == 15 && (h & 128u)) || !last_it(c))
                return undef(c);
            if (!branch(c, b, true))
                return false;
            if (h & 128u)
                c->r[14] = (pc + 2u) | 1u;
            return true;
        }
        if (d == 15)
            return branch(c, v, false);
        put(c, d, v);
        return true;
    }
    if ((h & 0xf800u) == 0x4800u) {
        d = h >> 8 & 7u;
        addr = ((pc + 4u) & ~3u) + (h & 255u) * 4u;
        return mem_read(c, addr, 4, SAMSA_R, &c->r[d]);
    }
    if ((h & 0xf000u) == 0x5000u) {
        unsigned op = h >> 9 & 7u, size = op == 0 || op == 4              ? 4
                                          : op == 1 || op == 5 || op == 7 ? 2
                                                                          : 1;
        addr = c->r[n] + c->r[m];
        if (op < 3)
            return mem_write(c, addr, size, c->r[d]);
        if (!mem_read(c, addr, size, SAMSA_R, &v))
            return false;
        put(c, d, op == 3 ? sx(v, 8) : op == 7 ? sx(v, 16) : v);
        return true;
    }
    if ((h & 0xe000u) == 0x6000u || (h & 0xf000u) == 0x8000u) {
        unsigned size = h < 0x8000u ? (h & 0x1000u ? 1 : 4) : 2;
        addr = c->r[n] + (h >> 6 & 31u) * size;
        if (h & 0x800u)
            return mem_read(c, addr, size, SAMSA_R, &c->r[d]);
        return mem_write(c, addr, size, c->r[d]);
    }
    if ((h & 0xf000u) == 0x9000u) {
        d = h >> 8 & 7u;
        addr = c->r[13] + (h & 255u) * 4u;
        if (h & 0x800u)
            return mem_read(c, addr, 4, SAMSA_R, &c->r[d]);
        return mem_write(c, addr, 4, c->r[d]);
    }
    if ((h & 0xf000u) == 0xa000u) {
        c->r[h >> 8 & 7u] = (h & 0x800u ? c->r[13] : (pc + 4u) & ~3u) + (h & 255u) * 4u;
        return true;
    }
    if ((h & 0xff00u) == 0xb000u) {
        v = (h & 127u) * 4u;
        c->r[13] += h & 128u ? 0u - v : v;
        return true;
    }
    if ((h & 0xf500u) == 0xb100u) {
        if (it)
            return usage(c);
        if ((c->r[d] != 0) == ((h & 0x800u) != 0))
            c->r[15] = pc + 4u + (h >> 3 & 31u) * 2u + (h >> 9 & 1u) * 64u;
        return true;
    }
    if ((h & 0xff00u) == 0xb200u) {
        v = c->r[n];
        switch (h >> 6 & 3u) {
        case 0:
            v = sx(v & 0xffffu, 16);
            break;
        case 1:
            v = sx(v & 255u, 8);
            break;
        case 2:
            v &= 0xffffu;
            break;
        default:
            v &= 255u;
            break;
        }
        put(c, d, v);
        return true;
    }
    if ((h & 0xf600u) == 0xb400u) {
        bool pop = (h & 0x800u) != 0;
        return multiple(c, 13, (h & 255u) | (h & 256u ? (pop ? 0x8000u : 0x4000u) : 0), pop, !pop,
                        true);
    }
    if ((h & 0xff00u) == 0xba00u) {
        unsigned op = h >> 6 & 3u;
        if (op == 2)
            return undef(c);
        put(c, d, reverse(c->r[n], op));
        return true;
    }
    if ((h & 0xff00u) == 0xbe00u) {
        c->stop_info = (uint8_t)h;
        c->_stop = SAMSA_STOP_BKPT;
        return false;
    }
    if ((h & 0xff00u) == 0xbf00u) {
        if (!(h & 15u))
            return true;
        if (it || (h >> 4 & 15u) == 15u ||
            ((h >> 4 & 15u) == 14u && ((h & 15u) & ((h & 15u) - 1u))))
            return undef(c);
        c->itstate = (uint8_t)h;
        return true;
    }
    if ((h & 0xf000u) == 0xc000u) {
        n = h >> 8 & 7u;
        bool load = (h & 0x800u) != 0;
        if (!load && (h & (1u << n)) && (h & ((1u << n) - 1u)))
            return undef(c);
        return multiple(c, n, h & 255u, load, false, !load || !(h & (1u << n)));
    }
    if ((h & 0xf000u) == 0xd000u) {
        unsigned cond = h >> 8 & 15u;
        if (cond == 15) {
            c->stop_info = (uint8_t)h;
            c->_stop = SAMSA_STOP_SVC;
            return false;
        }
        if (cond == 14)
            return undef(c);
        if (it)
            return usage(c);
        if (condition(c, cond))
            c->r[15] = pc + 4u + sx((h & 255u) * 2u, 9);
        return true;
    }
    if ((h & 0xf800u) == 0xe000u)
        return branch(c, pc + 4u + sx((h & 2047u) << 1, 12), false);
    return undef(c);
}
