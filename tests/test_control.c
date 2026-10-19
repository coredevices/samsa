/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"
static bool truth(unsigned cond, unsigned flags) {
    bool n = (flags & 8) != 0, z = (flags & 4) != 0, c = (flags & 2) != 0, v = (flags & 1) != 0;
    bool values[] = {z,  !z,      c,       !c,     n,      !n,           v,
                     !v, c && !z, !c || z, n == v, n != v, !z && n == v, z || n != v};
    return values[cond];
}
static void branches(void) {
    for (unsigned flags = 0; flags < 16; ++flags)
        for (unsigned cond = 0; cond < 14; ++cond) {
            Fixture f;
            init(&f);
            CODE(&f, 0xd002u | (cond << 8));
            samsa_set_xpsr(&f.c, flags << 28);
            samsa_run(&f.c, 1, NULL);
            EQ(f.c.r[15], truth(cond, flags) ? 0x1008 : 0x1002);
            init(&f);
            CODE(&f, 0xf000u | (cond << 6), 0x8002);
            samsa_set_xpsr(&f.c, flags << 28);
            samsa_run(&f.c, 1, NULL);
            EQ(f.c.r[15], truth(cond, flags) ? 0x1008 : 0x1004);
        }
    Fixture f;
    init(&f);
    CODE(&f, 0xf7ff, 0xbffe);
    samsa_run(&f.c, 1, NULL);
    EQ(f.c.r[15], 0x1000);
    init(&f);
    CODE(&f, 0xd1fe);
    samsa_run(&f.c, 1, NULL);
    EQ(f.c.r[15], 0x1000);
    init(&f);
    CODE(&f, 0xf47f, 0xaffe);
    samsa_run(&f.c, 1, NULL);
    EQ(f.c.r[15], 0x1000);
    init(&f);
    CODE(&f, 0x4488, 0x4588);
    f.c.r[8] = 7;
    f.c.r[1] = 3;
    samsa_run(&f.c, 2, NULL);
    EQ(f.c.r[8], 10);
    CHECK(samsa_get_xpsr(&f.c) & (1u << 29));
    init(&f);
    CODE(&f, 0x4248);
    f.c.r[1] = 1;
    samsa_run(&f.c, 1, NULL);
    EQ(f.c.r[0], UINT32_MAX);
    EQ(samsa_get_xpsr(&f.c) >> 28, 8);
    init(&f);
    CODE(&f, 0x4778); /* BX pc checks the even PC+4 exchange target. */
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_USAGE);
    const SamsaHook pc_hook = {0x1004, 0x1006};
    f.c.hooks = &pc_hook;
    f.c.num_hooks = 1;
    EQ(samsa_run(&f.c, 10, NULL), SAMSA_STOP_HOOK);
    EQ(f.c.r[15], 0x1004);
    const unsigned op[] = {2, 3, 4, 7};
    const uint32_t expected[] = {0, 0, UINT32_MAX, 0x80000001};
    for (unsigned i = 0; i < 4; ++i) {
        init(&f);
        CODE(&f, 0x4008u | (op[i] << 6));
        f.c.r[0] = 0x80000001;
        f.c.r[1] = 32;
        samsa_run(&f.c, 1, NULL);
        EQ(f.c.r[0], expected[i]);
        CHECK(samsa_get_xpsr(&f.c) & (1u << 29));
    }
}
static void system_ops(void) {
    for (unsigned hint = 0; hint < 5; ++hint) {
        Fixture f;
        init(&f);
        CODE(&f, 0xf3af, 0x8000u | hint);
        samsa_set_xpsr(&f.c, 0xf8000000);
        EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
        EQ(samsa_get_xpsr(&f.c) >> 27, 31);
    }
    for (unsigned barrier = 4; barrier <= 6; ++barrier) {
        Fixture f;
        init(&f);
        CODE(&f, 0xf3bf, 0x8f0fu | (barrier << 4));
        EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
    }
    const uint16_t bad[][2] = {{0xfa91, 0xf032}, {0xfb21, 0xf022}, {0xef00, 0x0000},
                               {0xf3ef, 0x8014}, {0xf380, 0x8814}, {0xf3af, 0x8100},
                               {0xf7f0, 0xa000}};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        Fixture f;
        init(&f);
        code(&f, bad[i], 2);
        EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_UNDEF);
        EQ(f.c.fault_pc, 0x1000);
    }
}
static void it_resume(void) {
    Fixture f;
    init(&f);
    CODE(&f, 0xbf04, 0xffff, 0xffff, 0x2107, 0xbe00); /* both EQ slots fail */
    samsa_run(&f.c, 2, NULL);
    EQ(f.c.r[15], 0x1006);
    CHECK(f.c.itstate);
    EQ(samsa_run(&f.c, 2, NULL), SAMSA_STOP_BKPT);
    EQ(f.c.r[1], 0);
    EQ(f.c.itstate, 0);
    init(&f);
    CODE(&f, 0xbf04, 0x2107, 0x2208, 0xbe00);
    samsa_set_xpsr(&f.c, 1u << 30);
    const SamsaHook hook = {0x1004, 0x1006};
    f.c.hooks = &hook;
    f.c.num_hooks = 1;
    EQ(samsa_run(&f.c, 10, NULL), SAMSA_STOP_HOOK);
    CHECK(f.c.itstate);
    EQ(f.c.r[1], 7);
    f.c.num_hooks = 0;
    EQ(samsa_run(&f.c, 10, NULL), SAMSA_STOP_BKPT);
    EQ(f.c.r[2], 8);
    EQ(f.c.itstate, 0);
    for (unsigned mask = 1, slots = 4; mask <= 8; mask <<= 1, --slots) {
        init(&f);
        CODE(&f, 0xbfe0u | mask, 0x3001, 0x3001, 0x3001, 0x3001);
        f.c.r[0] = UINT32_MAX;
        samsa_set_xpsr(&f.c, 0xf8000000);
        EQ(samsa_run(&f.c, slots + 1, NULL), SAMSA_STOP_BUDGET);
        EQ(f.c.r[0], slots - 1);
        EQ(f.c.itstate, 0);
        EQ(samsa_get_xpsr(&f.c) >> 27, 31);
    }
    init(&f);
    CODE(&f, 0xbf04, 0xbe42, 0x2107, 0xbe00);
    EQ(samsa_run(&f.c, 10, NULL), SAMSA_STOP_BKPT);
    EQ(f.c.stop_info, 0x42);
    EQ(f.c.r[15], 0x1004);
    EQ(f.c.itstate, 8);
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
    EQ(f.c.r[1], 0);
    EQ(f.c.itstate, 0);
    f.c.r[1] = 7;
    Fixture other;
    init(&other);
    CODE(&other, 0x2009);
    samsa_run(&other.c, 1, NULL);
    EQ(f.c.r[1], 7);
    EQ(other.c.r[0], 9);
}
int main(void) {
    branches();
    system_ops();
    it_resume();
    return 0;
}
