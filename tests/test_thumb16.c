/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"
static void arithmetic(void) {
    Fixture f;
    init(&f); /* movs; adds; subs; cmp */
    CODE(&f, 0x20ff, 0x3001, 0x3801, 0x28ff, 0xbe00);
    EQ(samsa_run(&f.c, 10, NULL), SAMSA_STOP_BKPT);
    EQ(f.c.r[0], 255);
    CHECK(samsa_get_xpsr(&f.c) & (1u << 30));
    init(&f);
    CODE(&f, 0x1888, 0x1a89); /* adds r0,r1,r2; subs r1,r1,r2 */
    f.c.r[1] = 0x7fffffff;
    f.c.r[2] = 1;
    samsa_run(&f.c, 1, NULL);
    EQ(f.c.r[0], 0x80000000);
    EQ(samsa_get_xpsr(&f.c) >> 28, 9);
    samsa_run(&f.c, 1, NULL);
    EQ(f.c.r[1], 0x7ffffffe);
    init(&f);
    CODE(&f, 0x4148, 0x4188); /* adcs r0,r1; sbcs r0,r1 */
    f.c.r[0] = UINT32_MAX;
    f.c.r[1] = 1;
    samsa_set_xpsr(&f.c, 1u << 29);
    samsa_run(&f.c, 1, NULL);
    EQ(f.c.r[0], 1);
    CHECK(samsa_get_xpsr(&f.c) & (1u << 29));
    samsa_run(&f.c, 1, NULL);
    EQ(f.c.r[0], 0);
}
static void it_blocks(void) {
    Fixture f;
    init(&f); /* cmp r0,#0; ite eq; moveq r1,#7; movne r1,#8; bkpt */
    CODE(&f, 0x2800, 0xbf0c, 0x2107, 0x2108, 0xbe00);
    samsa_run(&f.c, 3, NULL);
    EQ(f.c.r[1], 7);
    CHECK(f.c.itstate);
    CHECK(samsa_get_xpsr(&f.c) & (1u << 30));
    EQ(samsa_run(&f.c, 2, NULL), SAMSA_STOP_BKPT);
    EQ(f.c.r[1], 7);
    EQ(f.c.itstate, 0);
    init(&f);
    CODE(&f, 0xbf04, 0xe000);
    samsa_set_xpsr(&f.c, 1u << 30);
    EQ(samsa_run(&f.c, 2, NULL), SAMSA_STOP_USAGE);
    init(&f);
    CODE(&f, 0xbf08, 0x3001);
    samsa_set_xpsr(&f.c, 1u << 30);
    f.c.r[0] = UINT32_MAX;
    samsa_run(&f.c, 2, NULL);
    EQ(f.c.r[0], 0);
    EQ(samsa_get_xpsr(&f.c) >> 28, 4);
}
static void memory(void) {
    Fixture f;
    init(&f); /* str r0,[r1]; ldr r2,[r1]; strh; ldrh; ldrsb */
    CODE(&f, 0x6008, 0x680a, 0x8008, 0x880b, 0x560c);
    f.c.r[0] = 0x123480fe;
    f.c.r[1] = 0x1101;
    EQ(samsa_run(&f.c, 4, NULL), SAMSA_STOP_BUDGET);
    EQ(f.c.r[2], 0x123480fe);
    EQ(f.c.r[3], 0x80fe);
    f.c.r[0] = 0;
    samsa_run(&f.c, 1, NULL);
    EQ(f.c.r[4], 0xfffffffe);
    init(&f);
    CODE(&f, 0x4801, 0xa101, 0xbf00, 0xbf00, 0x5678, 0x1234);
    samsa_run(&f.c, 2, NULL);
    EQ(f.c.r[0], 0x12345678);
    EQ(f.c.r[1], 0x1008);
    init(&f);
    CODE(&f, 0xb503, 0xbd0c);
    f.c.r[0] = 11;
    f.c.r[1] = 22;
    f.c.r[14] = 0x1011;
    samsa_run(&f.c, 2, NULL);
    EQ(f.c.r[2], 11);
    EQ(f.c.r[3], 22);
    EQ(f.c.r[15], 0x1010);
    EQ(f.c.r[13], 0x2000);
    init(&f);
    CODE(&f, 0xc103, 0xc90c);
    f.c.r[1] = 0x1100;
    EQ(samsa_run(&f.c, 1, NULL),
       SAMSA_STOP_UNDEF); /* base stored after lower register: unpredictable */
    init(&f);
    CODE(&f, 0xc003, 0xc80c);
    f.c.r[0] = 0x1100;
    f.c.r[1] = 44;
    samsa_run(&f.c, 1, NULL);
    f.c.r[0] = 0x1100;
    samsa_run(&f.c, 1, NULL);
    EQ(f.c.r[2], 0x1100);
    EQ(f.c.r[3], 44);
    EQ(f.c.r[0], 0x1108);
    init(&f);
    CODE(&f, 0xc803);
    f.c.r[0] = 0x1100;
    uint32_t w[] = {123, 456};
    memcpy(f.mem + 256, w, 8);
    samsa_run(&f.c, 1, NULL);
    EQ(f.c.r[0], 123);
    EQ(f.c.r[1], 456);
    init(&f);
    CODE(&f, 0xb401);
    f.c.r[13] = 0x1101;
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_USAGE);
}
static void misc(void) {
    Fixture f;
    init(&f);
    CODE(&f, 0xb248, 0xb289, 0xba0a, 0xba4b, 0xbacc); /* sxtb; uxth; rev; rev16; revsh */
    f.c.r[1] = 0x123480fe;
    samsa_run(&f.c, 5, NULL);
    EQ(f.c.r[0], 0xfffffffe);
    EQ(f.c.r[1], 0x80fe);
    EQ(f.c.r[2], 0xfe800000);
    EQ(f.c.r[3], 0xfe80);
    EQ(f.c.r[4], 0xfffffe80);
    init(&f);
    CODE(&f, 0xb100);
    samsa_run(&f.c, 1, NULL);
    EQ(f.c.r[15], 0x1004);
    init(&f);
    CODE(&f, 0x4780);
    f.c.r[0] = 0x1101;
    samsa_run(&f.c, 1, NULL);
    EQ(f.c.r[14], 0x1003);
    EQ(f.c.r[15], 0x1100);
    init(&f);
    CODE(&f, 0x4687);
    f.c.r[0] = 0x1100;
    samsa_run(&f.c, 1, NULL);
    EQ(f.c.r[15], 0x1100); /* mov pc,r0: no exchange */
    init(&f);
    CODE(&f, 0xdfab);
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_SVC);
    EQ(f.c.stop_info, 0xab);
    init(&f);
    CODE(&f, 0xb672);
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_UNDEF);
}
int main(void) {
    arithmetic();
    it_blocks();
    memory();
    misc();
    return 0;
}
