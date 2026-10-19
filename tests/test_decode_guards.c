/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"
#include "vectors/excluded.h"
int main(void) {
    for (unsigned first = 0xfb00; first < 0xfb80; ++first) {
        for (unsigned fixed = 1; fixed < 4; ++fixed) {
            Fixture f;
            init(&f);
            CODE(&f, first, 0xfe37u | (fixed << 6));
            f.c.r[0] = 0x12345678;
            f.c.r[7] = 3;
            f.c.r[14] = 0xabcdef01;
            uint32_t registers[16];
            memcpy(registers, f.c.r, sizeof(registers));
            samsa_set_xpsr(&f.c, 0xf8000000);
            EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_UNDEF);
            CHECK(!memcmp(registers, f.c.r, sizeof(registers)));
            EQ(samsa_get_xpsr(&f.c) >> 27, 31);
        }
    }
    Fixture f;
    init(&f);
    CODE(&f, 0xfb10, 0xfef7);
    f.c.r[0] = 15;
    f.c.r[7] = 3;
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_UNDEF);
    EQ(f.c.r[14], 0);
    init(&f);
    CODE(&f, 0xfb91, 0xf0f2, 0xfbb1, 0xf3f2);
    f.c.r[1] = 15;
    f.c.r[2] = 3;
    EQ(samsa_run(&f.c, 2, NULL), SAMSA_STOP_BUDGET);
    EQ(f.c.r[0], 5);
    EQ(f.c.r[3], 5);
    for (unsigned i = 0; i < sizeof(excluded) / sizeof(excluded[0]); ++i) {
        init(&f);
        CODE(&f, excluded[i].first, excluded[i].second);
        SamsaStop stop = samsa_run(&f.c, 1, NULL);
        if (stop != SAMSA_STOP_UNDEF)
            fprintf(stderr, "accepted DSP %s\n", excluded[i].name);
        EQ(stop, SAMSA_STOP_UNDEF);
    }
    /* All A5-30 coprocessor first-halfword allocations, including FP. */
    for (unsigned first = 0xec00; first <= 0xffff; ++first) {
        if (first >= 0xf000 && first < 0xfc00)
            continue;
        for (unsigned bits = 0; bits < 4; ++bits) {
            init(&f);
            CODE(&f, first, bits * 0x5555u);
            EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_UNDEF);
        }
    }
    for (unsigned selector = 1; selector < 256; ++selector) {
        init(&f);
        CODE(&f, 0xf3ef, 0x8000u | selector);
        EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_UNDEF);
        init(&f);
        CODE(&f, 0xf380, 0x8800u | selector);
        EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_UNDEF);
        init(&f);
        CODE(&f, 0xf380, 0x8c00u | selector);
        EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_UNDEF);
    }
    for (unsigned low = 0x60; low <= 0x73; ++low) {
        init(&f);
        CODE(&f, 0xb600u | low);
        EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_UNDEF);
    }
    init(&f);
    CODE(&f, 0xb650);
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_UNDEF);
    init(&f);
    CODE(&f, 0xb658);
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_UNDEF);
    return 0;
}
