/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"
int main(void) {
    const uint16_t tests[] = {0xf011, 0xf091, 0xf111, 0xf1b1};
    const uint16_t shifted[] = {0xea11, 0xea91, 0xeb11, 0xebb1};
    const uint32_t flags[] = {2, 2, 0, 8};
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned immediate = 0; immediate < 2; ++immediate) {
            Fixture f;
            init(&f);
            CODE(&f, immediate ? tests[i] : shifted[i], immediate ? 0x0f03 : 0x0f02);
            f.c.r[0] = 42;
            f.c.r[1] = 1;
            f.c.r[2] = 3;
            samsa_set_xpsr(&f.c, 1u << 29);
            EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
            EQ(f.c.r[0], 42);
            EQ(samsa_get_xpsr(&f.c) >> 28, flags[i]);
        }
    const uint16_t moves[] = {0xf04f, 0xf06f, 0xea4f, 0xea6f};
    for (unsigned i = 0; i < 4; ++i) {
        Fixture f;
        init(&f);
        CODE(&f, moves[i], i < 2 ? 3 : 1);
        f.c.r[1] = 3;
        EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
        EQ(f.c.r[0], i & 1 ? 0xfffffffcu : 3u);
    }
    for (unsigned sub = 0; sub < 2; ++sub) {
        Fixture f;
        init(&f);
        CODE(&f, sub ? 0xf2af : 0xf20f, 0x0004);
        EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
        EQ(f.c.r[0], sub ? 0x1000 : 0x1008);
    }
    /* Register ASR/ROR with S, including amounts greater than 32. */
    Fixture f;
    init(&f);
    CODE(&f, 0xfa51, 0xf002, 0xfa71, 0xf302);
    f.c.r[1] = 0x80000001;
    f.c.r[2] = 33;
    samsa_run(&f.c, 1, NULL);
    EQ(f.c.r[0], UINT32_MAX);
    EQ(samsa_get_xpsr(&f.c) >> 28, 10);
    samsa_run(&f.c, 1, NULL);
    EQ(f.c.r[3], 0xc0000000);
    EQ(samsa_get_xpsr(&f.c) >> 28, 10);
    init(&f);
    CODE(&f, 0xfa0f, 0xf091, 0xfa5f, 0xf2b1);
    f.c.r[1] = 0x123480fe;
    samsa_run(&f.c, 2, NULL);
    EQ(f.c.r[0], 0x3480);
    EQ(f.c.r[2], 0x12);
    /* Saturate with signed/unsigned and both immediate shift directions. */
    for (unsigned sign = 0; sign < 2; ++sign)
        for (unsigned asr = 0; asr < 2; ++asr) {
            init(&f);
            CODE(&f, (sign ? 0xf301 : 0xf381) | (asr ? 32u : 0), 0x0040u | (sign ? 7u : 8u));
            f.c.r[1] = 0x1000;
            samsa_set_xpsr(&f.c, 0xf0000000);
            EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
            EQ(f.c.r[0], sign ? 127u : 255u);
            EQ(samsa_get_xpsr(&f.c) >> 27, 31);
        }
    /* LSL/LSR/ASR/ROR/MOV/MVN aliases of shifted-register data processing. */
    const uint16_t shifts[] = {0x0041, 0x0051, 0x0061, 0x0071, 0x0001};
    const uint32_t expected[] = {2, 0x40000000, 0xc0000000, 0xc0000000, 0x80000001};
    for (unsigned i = 0; i < 5; ++i) {
        init(&f);
        CODE(&f, 0xea4f, shifts[i]);
        f.c.r[1] = 0x80000001;
        EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
        EQ(f.c.r[0], expected[i]);
    }
    init(&f);
    CODE(&f, 0xf3af, 0x80f0);
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET); /* DBG */
    return 0;
}
