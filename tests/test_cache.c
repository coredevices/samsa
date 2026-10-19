/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"
static void host_patches(void) {
    Fixture f;
    init(&f);
    CODE(&f, 0x2001); /* movs r0,#1 */
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
    const uint16_t wide[] = {0xf240, 0x0037}; /* movw r0,#55 */
    memcpy(f.mem, wide, sizeof(wide));
    samsa_invalidate(&f.c, 0x1000, sizeof(wide));
    f.c.r[15] = 0x1000;
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
    EQ(f.c.r[0], 55);
    f.mem[2] = 56;
    samsa_invalidate(&f.c, 0x1002, 1); /* Only the second halfword changed. */
    f.c.r[15] = 0x1000;
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
    EQ(f.c.r[0], 56);
    f.mem[2] = 57;
    samsa_invalidate(&f.c, 0, UINT32_MAX);
    f.c.r[15] = 0x1000;
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
    EQ(f.c.r[0], 57);
    f.mem[2] = 58;
    samsa_invalidate(&f.c, UINT32_MAX - 1u, 4); /* Wrapping range flushes all. */
    f.c.r[15] = 0x1000;
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
    EQ(f.c.r[0], 58);
}
static void guest_patches(void) {
    const uint16_t stores[] = {0x7011, 0x8011, 0x6011}; /* str[b/h] r1,[r2] */
    for (unsigned k = 0; k < 3; ++k) {
        Fixture f;
        init(&f);
        const uint16_t target[] = {0xf240, 0x0037, 0xbf00};
        memcpy(f.mem + 0x20, target, sizeof(target));
        memcpy(f.mem + 0x80, &stores[k], 2);
        f.c.r[15] = 0x1020;
        EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
        EQ(f.c.r[0], 55);
        f.c.r[1] = k == 2 ? 0xbf000038 : 0x38;
        f.c.r[2] = 0x1022;
        f.c.r[15] = 0x1080;
        EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
        f.c.r[15] = 0x1020;
        EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
        EQ(f.c.r[0], 56);
    }
    Fixture f;
    init(&f);
    /* An unaligned word store touches two cached narrow instructions. */
    const uint16_t targets[] = {0x2001, 0x2102, 0xbf00};
    memcpy(f.mem + 0x20, targets, sizeof(targets));
    CODE(&f, 0x6011);
    f.c.r[15] = 0x1020;
    EQ(samsa_run(&f.c, 2, NULL), SAMSA_STOP_BUDGET);
    f.c.r[1] = 0x00210920;
    f.c.r[2] = 0x1021;
    f.c.r[15] = 0x1000;
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
    f.c.r[15] = 0x1020;
    EQ(samsa_run(&f.c, 2, NULL), SAMSA_STOP_BUDGET);
    EQ(f.c.r[0], 1);
    EQ(f.c.r[1], 9);
}
static void permissions_and_hooks(void) {
    Fixture f;
    init(&f);
    CODE(&f, 0xf240, 0x0037);
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
    f.c.r[15] = 0x1000;
    f.region.perms = SAMSA_R | SAMSA_W;
    uint32_t count;
    EQ(samsa_run(&f.c, 1, &count), SAMSA_STOP_MEMFAULT);
    EQ(count, 0);
    EQ(f.c.fault_addr, 0x1000);
    const SamsaHook hook = {0x1000, 0x1004};
    f.c.hooks = &hook;
    f.c.num_hooks = 1;
    EQ(samsa_run(&f.c, 1, &count), SAMSA_STOP_HOOK);
    EQ(count, 0);
    f.c.num_hooks = 0;
    SamsaRegion regions[] = {{0x1000, 2, f.mem, SAMSA_X}, {0x1002, 2, f.mem + 2, SAMSA_R}};
    f.c.regions = regions;
    f.c.num_regions = 2;
    samsa_invalidate(&f.c, 0, UINT32_MAX);
    EQ(samsa_run(&f.c, 1, &count), SAMSA_STOP_MEMFAULT);
    EQ(count, 0);
    EQ(f.c.fault_addr, 0x1002);
    regions[1].perms |= SAMSA_X;
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
    regions[1].perms = SAMSA_R;
    f.c.r[15] = 0x1000;
    EQ(samsa_run(&f.c, 1, &count), SAMSA_STOP_MEMFAULT);
    EQ(count, 0);
    EQ(f.c.fault_addr, 0x1002);
}
static void mappings_and_instances(void) {
    Fixture a, b;
    init(&a);
    init(&b);
    CODE(&a, 0x2007);
    CODE(&b, 0x2009);
    a.region.guest_base = 0; /* Zero is a valid cache key. */
    a.c.r[15] = 0;
    for (unsigned i = 0; i < 2; ++i) {
        EQ(samsa_run(&a.c, 1, NULL), SAMSA_STOP_BUDGET);
        EQ(samsa_run(&b.c, 1, NULL), SAMSA_STOP_BUDGET);
        EQ(a.c.r[0], 7);
        EQ(b.c.r[0], 9);
        a.c.r[15] = 0;
        b.c.r[15] = 0x1000;
    }
    SamsaRegion changed = {0, sizeof(b.mem), b.mem, SAMSA_X};
    a.c.regions = &changed;
    samsa_invalidate(&a.c, 0, UINT32_MAX);
    EQ(samsa_run(&a.c, 1, NULL), SAMSA_STOP_BUDGET);
    EQ(a.c.r[0], 9);
    a.c.num_regions = 0;
    a.c.r[15] = 0;
    EQ(samsa_run(&a.c, 1, NULL), SAMSA_STOP_MEMFAULT);
}
static void cache_collisions(void) {
    Fixture f;
    init(&f);
    /* Addresses separated by 2048 bytes collide for every supported cache size. */
    CODE(&f, 0x2007);
    f.mem[0x800] = 9;
    f.mem[0x801] = 0x20;
    for (unsigned i = 0; i < 8; ++i) {
        f.c.r[15] = i & 1u ? 0x1800 : 0x1000;
        EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
        EQ(f.c.r[0], i & 1u ? 9 : 7);
    }
}
int main(void) {
    host_patches();
    guest_patches();
    permissions_and_hooks();
    mappings_and_instances();
    cache_collisions();
    return 0;
}
