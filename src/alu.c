/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include "alu.h"
uint32_t sx(uint32_t x, unsigned bits) {
    uint32_t sign = UINT32_C(1) << (bits - 1u);
    return (x ^ sign) - sign;
}
void nz(SamsaCpu *c, uint32_t value) {
    c->_flags = (uint8_t)((c->_flags & (C | V | Q)) | (value >> 24 & N) | (!value ? Z : 0));
}
void nzc(SamsaCpu *c, uint32_t value, unsigned carry) {
    nz(c, value);
    c->_flags = (uint8_t)((c->_flags & ~C) | (carry ? C : 0));
}
uint32_t add_c(SamsaCpu *c, uint32_t a, uint32_t b, unsigned carry, bool flags) {
    uint32_t t = a + b, r = t + carry;
    if (flags) {
        nzc(c, r, t < a || r < t);
        c->_flags = (uint8_t)((c->_flags & ~V) | (((~(a ^ b) & (a ^ r)) >> 27) & V));
    }
    return r;
}
uint32_t shift(uint32_t x, unsigned type, unsigned n, unsigned *carry) {
    if (!n) return x;
    switch (type) {
    case 0:
        *carry = n <= 32 ? (x >> (32 - n)) & 1u : 0;
        return n < 32 ? x << n : 0;
    case 1:
        *carry = n <= 32 ? (x >> (n - 1)) & 1u : 0;
        return n < 32 ? x >> n : 0;
    case 2:
        *carry = n < 32 ? (x >> (n - 1)) & 1u : x >> 31;
        if (n >= 32) return 0u - (x >> 31);
        return (x >> n) | ((0u - (x >> 31)) << (32 - n));
    case 3:
        n &= 31u;
        if (n) x = (x >> n) | (x << (32 - n));
        *carry = x >> 31;
        return x;
    default: { /* RRX */
        uint32_t r = (*carry << 31) | (x >> 1);
        *carry = x & 1u;
        return r;
    }
    }
}
uint32_t imm_expand(unsigned imm, unsigned *carry) {
    uint32_t x = imm & 255u;
    if ((imm & 0xc00u) == 0) {
        switch (imm >> 8) {
        case 1: return x | x << 16;
        case 2: return x << 8 | x << 24;
        case 3: return x * UINT32_C(0x01010101);
        default: return x;
        }
    }
    return shift(128u | (imm & 127u), 3, imm >> 7, carry);
}
bool condition(const SamsaCpu *c, unsigned cond) {
    bool n = (c->_flags & N) != 0, z = (c->_flags & Z) != 0;
    bool cy = (c->_flags & C) != 0, v = (c->_flags & V) != 0, yes;
    switch (cond >> 1) {
    case 0: yes = z; break;
    case 1: yes = cy; break;
    case 2: yes = n; break;
    case 3: yes = v; break;
    case 4: yes = cy && !z; break;
    case 5: yes = n == v; break;
    case 6: yes = n == v && !z; break;
    default: return true;
    }
    return (cond & 1u) ? !yes : yes;
}
