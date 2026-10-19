/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"
static bool bus_read(SamsaCpu *c, uint32_t addr, unsigned size, uint32_t *out) {
    EQ(size, 4);
    if (addr != 0x4000)
        return false;
    *out = *(uint32_t *)c->user;
    return true;
}
static bool bus_write(SamsaCpu *c, uint32_t addr, unsigned size, uint32_t value) {
    EQ(size, 4);
    if (addr != 0x4000)
        return false;
    *(uint32_t *)c->user = value;
    return true;
}
static SamsaStop stalled(SamsaCpu *c, SamsaStop why, void *ctx) {
    (void)c;
    (void)why;
    (void)ctx;
    return SAMSA_STOP_BUDGET;
}
static bool crossing_bus(SamsaCpu *c, uint32_t addr, unsigned size, uint32_t *out) {
    (void)addr;
    (void)size;
    ++*(unsigned *)c->user;
    *out = 123;
    return true;
}
int main(void) {
    Fixture f;
    init(&f);
    const SamsaHook hook = {0x3000, 0x3010};
    f.c.hooks = &hook;
    f.c.num_hooks = 1;
    CODE(&f, 0x4780, 0xbe00);
    f.c.r[0] = 0x3000;
    EQ(samsa_run(&f.c, 10, NULL), SAMSA_STOP_HOOK);
    EQ(f.c.r[15], 0x3000);
    EQ(f.c.r[14], 0x1003);
    samsa_return32(&f.c, 123);
    EQ(samsa_run(&f.c, 10, NULL), SAMSA_STOP_BKPT);
    EQ(f.c.r[0], 123);
    init(&f);
    CODE(&f, 0x1840, 0x4770);
    f.c.r[0] = 123;
    f.c.r[15] = 0x1234;
    f.c.r[14] = 0x5679;
    samsa_set_xpsr(&f.c, 0xf8000800);
    f.c.stop_info = 0xab;
    f.c.fault_pc = 0xdead;
    f.c.fault_addr = 0xbeef;
    uint32_t args[] = {7, 11}, ret = 0;
    EQ(samsa_call(&f.c, 0x1001, args, 2, &ret, NULL, NULL), SAMSA_STOP_RETURN);
    EQ(ret, 18);
    EQ(f.c.r[0], 123);
    EQ(f.c.r[15], 0x1234);
    EQ(f.c.r[14], 0x5679);
    EQ(samsa_get_xpsr(&f.c) >> 27, 31);
    EQ(f.c.itstate, 8);
    EQ(f.c.stop_info, 0xab);
    EQ(f.c.fault_pc, 0xdead);
    EQ(f.c.fault_addr, 0xbeef);
    EQ(samsa_call(&f.c, 0x1000, args, 2, &ret, NULL, NULL), SAMSA_STOP_USAGE);
    /* msr APSR_g,r0 inside a call; the caller's GE bits are restored. */
    init(&f);
    CODE(&f, 0xf380, 0x8400, 0x4770);
    samsa_set_xpsr(&f.c, 0x00050000);
    uint32_t ge = 0x000a0000;
    EQ(samsa_call(&f.c, 0x1001, &ge, 1, NULL, NULL, NULL), SAMSA_STOP_RETURN);
    EQ(samsa_get_xpsr(&f.c), 0x01050000);
    init(&f);
    CODE(&f, 0x9800, 0x9901, 0x1840, 0x4770);
    uint32_t many[] = {1, 2, 3, 4, 5, 6};
    EQ(samsa_call(&f.c, 0x1001, many, 6, &ret, NULL, NULL), SAMSA_STOP_RETURN);
    EQ(ret, 11);
    EQ(f.c.r[13], 0x2000);
    f.c.r[0] = 1;
    f.c.r[1] = 2;
    f.c.r[2] = 3;
    f.c.r[3] = 4;
    EQ(samsa_arg64(&f.c, 1), UINT64_C(0x400000003));
    f.c.r[13] = 0x1ff8;
    memcpy(f.mem + 0xff8, many, 8);
    EQ(samsa_arg(&f.c, 4), 1);
    EQ(samsa_arg64(&f.c, 3), UINT64_C(0x200000001));
    init(&f);
    CODE(&f, 0xe7fe);
    EQ(samsa_call_budget(&f.c, 0x1001, NULL, 0, NULL, NULL, NULL, 10), SAMSA_STOP_BUDGET);
    f.c.hooks = &hook;
    f.c.num_hooks = 1;
    EQ(samsa_call_budget(&f.c, 0x3000, NULL, 0, NULL, stalled, NULL, 10), SAMSA_STOP_BUDGET);
    init(&f);
    const SamsaBus bus = {bus_read, bus_write};
    uint32_t device = 42;
    f.c.bus = &bus;
    f.c.user = &device;
    CODE(&f, 0x6808, 0x3001, 0x6008);
    f.c.r[1] = 0x4000;
    EQ(samsa_run(&f.c, 3, NULL), SAMSA_STOP_BUDGET);
    EQ(device, 43);
    f.c.r[15] = 0x4000;
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_MEMFAULT);
    init(&f);
    CODE(&f, 0x6808);
    f.c.r[1] = 0xfffffffe;
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_MEMFAULT);
    EQ(f.c.fault_addr, 0xfffffffe);
    EQ(f.c.fault_pc, 0x1000);
    init(&f);
    CODE(&f, 0x6808);
    f.c.r[1] = 0x1ffe;
    const SamsaBus boundary_bus = {crossing_bus, NULL};
    unsigned accesses = 0;
    f.c.bus = &boundary_bus;
    f.c.user = &accesses;
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_MEMFAULT);
    EQ(accesses, 0);
    f.c.r[1] = 0x4000;
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_BUDGET);
    EQ(accesses, 1);
    EQ(f.c.r[0], 123);
    return 0;
}
