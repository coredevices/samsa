/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include <stdint.h>
static uint32_t crc32(const uint8_t *p, unsigned n) {
    uint32_t crc = UINT32_MAX;
    while (n--) {
        crc ^= *p++;
        for (unsigned j = 0; j < 8; ++j)
            crc = (crc >> 1) ^ ((0u - (crc & 1u)) & 0xedb88320u);
    }
    return ~crc;
}
static uint32_t machine(uint32_t state, uint32_t input) {
    for (unsigned i = 0; i < 73; ++i) {
        switch (state & 15u) {
        case 0:
            input += 17;
            break;
        case 1:
            input ^= input << 3;
            break;
        case 2:
            input -= 91;
            break;
        case 3:
            input *= 7;
            break;
        case 4:
            input ^= 0xdeadbeef;
            break;
        case 5:
            input = (input >> 7) | (input << 25);
            break;
        case 6:
            input += state;
            break;
        case 7:
            input -= state;
            break;
        case 8:
            input ^= input >> 13;
            break;
        case 9:
            input += 0x12345678;
            break;
        case 10:
            input *= 0x12345;
            break;
        case 11:
            input = ~input;
            break;
        case 12:
            input /= 17;
            break;
        case 13:
            input |= input << 11;
            break;
        case 14:
            input ^= state * 33;
            break;
        default:
            input += 31;
            break;
        }
        state = input ^ (state * 1664525u + 1013904223u);
    }
    return input;
}
#ifdef __arm__
__attribute__((section(".entry")))
#endif
void program(uint32_t *out, uint32_t seed) {
    uint32_t values[24], state = seed;
    for (unsigned i = 0; i < 24; ++i) {
        state = state * 1664525u + 1013904223u;
        values[i] = state;
    }
    for (unsigned i = 1; i < 24; ++i) {
        uint32_t v = values[i];
        unsigned j = i;
        while (j && values[j - 1] > v) {
            values[j] = values[j - 1];
            --j;
        }
        values[j] = v;
    }
    for (unsigned i = 0; i < 24; ++i)
        *out++ = values[i];
    uint8_t bytes[65], copy[65];
    for (unsigned i = 0; i < 64; ++i)
        bytes[i] = (uint8_t)(33 + (seed + i) % 90);
    bytes[64] = 0;
    unsigned len = 0;
    do {
        copy[len] = bytes[len];
    } while (bytes[len++]);
    *out++ = len - 1;
    *out++ = crc32(copy, 64);
    *out++ = machine(seed, seed);
    volatile uint64_t x = UINT64_C(0x81234567abcdef01) ^ seed, y = UINT64_C(0x1020304) + seed;
    uint64_t q = x / y, r = x % y, prod = x * y;
    *out++ = (uint32_t)q;
    *out++ = (uint32_t)(q >> 32);
    *out++ = (uint32_t)r;
    *out++ = (uint32_t)(prod >> 32);
    volatile int64_t sx = -(int64_t)(x >> 1), sy = (int64_t)y;
    int64_t sq = sx / sy, sr = sx % sy;
    *out++ = (uint32_t)sq;
    *out++ = (uint32_t)((uint64_t)sq >> 32);
    *out++ = (uint32_t)sr;
    volatile float a = (float)(seed % 127 + 1) / 8.0f, b = 3.25f;
    volatile double d = (double)a / (double)b;
    union {
        float f;
        uint32_t u;
    } fbits;
    union {
        double d;
        uint64_t u;
    } dbits;
    fbits.f = (a + b) * b;
    *out++ = fbits.u;
    dbits.d = d;
    *out++ = (uint32_t)dbits.u;
    *out++ = (uint32_t)(dbits.u >> 32);
    double root = 1.0;
    for (unsigned i = 0; i < 16; ++i)
        root = (root + (double)a / root) * 0.5;
    dbits.d = root;
    *out++ = (uint32_t)dbits.u;
    *out++ = (uint32_t)(dbits.u >> 32);
}
