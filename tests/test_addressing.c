/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"
static void thumb16(void) {
    for (unsigned op = 0; op < 8; ++op) {
        Fixture f;
        init(&f);
        CODE(&f, 0x5008u | (op << 9) | (2u << 6));
        f.c.r[1] = 0x1100;
        f.c.r[2] = 8;
        f.c.r[0] = 0x8765abcd;
        uint32_t v = 0x8765abcd;
        memcpy(f.mem + 0x108, &v, 4);
        EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
        if (op == 3)
            EQ(f.c.r[0], 0xffffffcd);
        if (op == 4)
            EQ(f.c.r[0], v);
        if (op == 5)
            EQ(f.c.r[0], 0xabcd);
        if (op == 6)
            EQ(f.c.r[0], 0xcd);
        if (op == 7)
            EQ(f.c.r[0], 0xffffabcd);
        if (op < 3) {
            unsigned size = op == 0 ? 4 : op == 1 ? 2 : 1;
            uint32_t got = 0;
            memcpy(&got, f.mem + 0x108, size);
            EQ(got, v & (UINT32_MAX >> (32 - size * 8)));
        }
    }
    const uint16_t instructions[] = {0x7008, 0x7808, 0x8008, 0x8808, 0x9000, 0x9800};
    for (unsigned i = 0; i < 6; ++i) {
        Fixture f;
        init(&f);
        CODE(&f, instructions[i]);
        f.c.r[1] = 0x1101;
        f.c.r[13] = 0x1104;
        f.c.r[0] = 0x12345678;
        memset(f.mem + 0x100, 0x87, 16);
        EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
        if (i == 1)
            EQ(f.c.r[0], 0x87);
        if (i == 3)
            EQ(f.c.r[0], 0x8787);
        if (i == 5)
            EQ(f.c.r[0], 0x87878787);
        if (i == 0)
            EQ(f.mem[0x101], 0x78);
        if (i == 2)
            EQ(f.mem[0x102], 0x56);
        if (i == 4)
            EQ(word(&f, 0x104), 0x12345678);
    }
    Fixture f;
    init(&f);
    CODE(&f, 0x1c88, 0x1e8a, 0xa901, 0xb001, 0xb081);
    f.c.r[1] = 5;
    samsa_run(&f.c, 2, NULL);
    EQ(f.c.r[0], 7);
    EQ(f.c.r[2], 3);
    samsa_run(&f.c, 3, NULL);
    EQ(f.c.r[1], 0x2004);
    EQ(f.c.r[13], 0x2000);
    const uint16_t ext[] = {0xb208, 0xb248, 0xb288, 0xb2c8};
    const uint32_t expect[] = {0xffff80fe, 0xfffffffe, 0x80fe, 0xfe};
    for (unsigned i = 0; i < 4; ++i) {
        init(&f);
        CODE(&f, ext[i]);
        f.c.r[1] = 0x123480fe;
        samsa_run(&f.c, 1, NULL);
        EQ(f.c.r[0], expect[i]);
    }
    init(&f);
    CODE(&f, 0xb908);
    f.c.r[0] = 1;
    samsa_run(&f.c, 1, NULL);
    EQ(f.c.r[15], 0x1006);
    for (unsigned i = 0; i < 5; ++i) {
        init(&f);
        CODE(&f, 0xbf00u | (i << 4));
        samsa_set_xpsr(&f.c, 0xf8000000);
        samsa_run(&f.c, 1, NULL);
        EQ(samsa_get_xpsr(&f.c) >> 27, 31);
    }
    init(&f);
    CODE(&f, 0xde00);
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_UNDEF);
    init(&f);
    CODE(&f, 0x4685);
    f.c.r[0] = 0x1103;
    samsa_run(&f.c, 1, NULL);
    EQ(f.c.r[13], 0x1100);
}
static void thumb32(void) {
    /* Every scalar access width, imm12 / pre / post / T / register. */
    const unsigned widths[] = {1, 2, 4};
    for (unsigned w = 0; w < 3; ++w)
        for (unsigned load = 0; load < 2; ++load)
            for (unsigned form = 0; form < 5; ++form) {
                Fixture f;
                init(&f);
                unsigned kind = w * 2 + load;
                uint16_t first = (uint16_t)(0xf800u | (kind << 4) | 1u | (form == 0 ? 128u : 0));
                const uint16_t second[] = {8, 0x0f08, 0x0b08, 0x0e08, 2};
                CODE(&f, first, second[form]);
                f.c.r[0] = 0x12345678;
                f.c.r[1] = form == 2 ? 0x1108 : 0x1100;
                f.c.r[2] = 8;
                uint32_t value = 0x8765abcd;
                memcpy(f.mem + 0x108, &value, 4);
                EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
                uint32_t mask = UINT32_MAX >> (32 - widths[w] * 8);
                if (load)
                    EQ(f.c.r[0], value & mask);
                else
                    EQ(word(&f, 0x108) & mask, 0x12345678 & mask);
                EQ(f.c.r[1], form == 1 ? 0x1108 : form == 2 ? 0x1110 : 0x1100);
            }
    for (unsigned half = 0; half < 2; ++half)
        for (unsigned form = 0; form < 5; ++form) {
            Fixture f;
            init(&f);
            unsigned kind = 1 + half * 2;
            uint16_t first = (uint16_t)(0xf900u | (kind << 4) | 1u | (form == 0 ? 128u : 0));
            const uint16_t second[] = {8, 0x0f08, 0x0b08, 0x0e08, 2};
            CODE(&f, first, second[form]);
            f.c.r[1] = form == 2 ? 0x1108 : 0x1100;
            f.c.r[2] = 8;
            f.mem[0x108] = 0x80;
            f.mem[0x109] = 0x87;
            EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
            EQ(f.c.r[0], half ? 0xffff8780 : 0xffffff80);
        }
    /* Positive and negative literals, all load sizes and signs. */
    const uint16_t base[] = {0xf81f, 0xf83f, 0xf85f, 0xf91f, 0xf93f};
    const uint32_t expected[] = {0x80, 0x8780, 0x12348780, 0xffffff80, 0xffff8780};
    for (unsigned k = 0; k < 5; ++k)
        for (unsigned up = 0; up < 2; ++up) {
            Fixture f;
            init(&f);
            f.c.r[15] = 0x1020;
            uint16_t words[] = {(uint16_t)(base[k] | (up ? 128u : 0)), 8};
            memcpy(f.mem + 0x20, words, 4);
            uint32_t value = 0x12348780;
            memcpy(f.mem + (up ? 0x2c : 0x1c), &value, 4);
            EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
            EQ(f.c.r[0], expected[k]);
        }
    Fixture f;
    init(&f);
    CODE(&f, 0xe901, 0x0005);
    f.c.r[1] = 0x1110;
    f.c.r[0] = 77;
    f.c.r[2] = 88;
    samsa_run(&f.c, 1, NULL);
    EQ(f.c.r[1], 0x1110);
    EQ(word(&f, 0x108), 77);
    CODE(&f, 0xe931, 0x0005);
    f.c.r[15] = 0x1000;
    f.c.r[0] = 0;
    f.c.r[2] = 0;
    samsa_run(&f.c, 1, NULL);
    EQ(f.c.r[0], 77);
    EQ(f.c.r[2], 88);
    EQ(f.c.r[1], 0x1108);
}
static void dual_modes(void) {
    for (unsigned load = 0; load < 2; ++load) {
        for (unsigned up = 0; up < 2; ++up) {
            for (unsigned mode = 0; mode < 3; ++mode) {
                Fixture f;
                init(&f);
                bool pre = mode != 0, wb = mode != 1;
                uint16_t first = (uint16_t)(0xe840u | 1u | (load << 4) | (wb ? 32u : 0) |
                                            (up ? 128u : 0) | (pre ? 256u : 0));
                CODE(&f, first, 0x0202);
                f.c.r[1] = 0x1120;
                f.c.r[0] = 111;
                f.c.r[2] = 222;
                unsigned offset = pre ? (up ? 0x128 : 0x118) : 0x120;
                uint32_t values[] = {333, 444};
                memcpy(f.mem + offset, values, 8);
                EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
                if (load) {
                    EQ(f.c.r[0], 333);
                    EQ(f.c.r[2], 444);
                } else {
                    EQ(word(&f, offset), 111);
                    EQ(word(&f, offset + 4), 222);
                }
                EQ(f.c.r[1], wb ? (up ? 0x1128 : 0x1118) : 0x1120);
            }
        }
    }
    for (unsigned up = 0; up < 2; ++up) {
        Fixture f;
        init(&f);
        f.c.r[15] = 0x1020;
        uint16_t words[] = {(uint16_t)(0xe95fu | (up ? 128u : 0)), 0x0204};
        memcpy(f.mem + 0x20, words, 4);
        uint32_t values[] = {123, 456};
        memcpy(f.mem + (up ? 0x34 : 0x14), values, 8);
        EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
        EQ(f.c.r[0], 123);
        EQ(f.c.r[2], 456);
    }
}
int main(void) {
    thumb16();
    thumb32();
    dual_modes();
    return 0;
}
