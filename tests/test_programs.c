/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"
#include "programs/suite.c"
static void load(uint8_t *p, size_t size, const char *name) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/tests/programs/%s.bin", SAMSA_SOURCE_DIR, name);
    FILE *file = fopen(path, "rb");
    CHECK(file);
    CHECK(fread(p, 1, size, file) > 0);
    CHECK(!ferror(file));
    CHECK(feof(file));
    fclose(file);
}
static SamsaStop handler(SamsaCpu *cpu, SamsaStop why, void *ctx) {
    CHECK(why == SAMSA_STOP_HOOK);
    unsigned *depth = ctx;
    ++*depth;
    uint32_t arg = samsa_arg(cpu, 0), result = 0;
    EQ(samsa_call(cpu, 0x1001, &arg, 1, &result, handler, ctx), SAMSA_STOP_RETURN);
    samsa_return32(cpu, result);
    return SAMSA_STOP_BUDGET;
}
int main(void) {
    uint8_t *memory = calloc(1, 65536);
    CHECK(memory);
    SamsaRegion region = {0x1000, 65536, memory, SAMSA_R | SAMSA_W | SAMSA_X};
    SamsaCpu c;
    samsa_reset(&c);
    c.regions = &region;
    c.num_regions = 1;
    c.r[13] = 0x11000;
    load(memory, 65536, "suite");
    samsa_invalidate(&c, 0x1000, 65536);
    for (unsigned seed = 0; seed < 32; ++seed) {
        uint32_t expected[39] = {0}, args[] = {0xe000, seed}, ret;
        program(expected, seed);
        SamsaStop stop = samsa_call(&c, 0x1001, args, 2, &ret, NULL, NULL);
        if (stop != SAMSA_STOP_RETURN)
            fprintf(stderr, "program stop %u pc=%08x addr=%08x seed=%u\n", stop, c.fault_pc,
                    c.fault_addr, seed);
        EQ(stop, SAMSA_STOP_RETURN);
        CHECK(!memcmp(expected, memory + 0xd000, sizeof(expected)));
    }
    memset(memory, 0, 65536);
    load(memory, 65536, "hooks");
    samsa_invalidate(&c, 0x1000, 65536);
    const SamsaHook hook = {0x3000, 0x3002};
    c.hooks = &hook;
    c.num_hooks = 1;
    uint32_t arg = 4, ret;
    unsigned depth = 0;
    EQ(samsa_call(&c, 0x1001, &arg, 1, &ret, handler, &depth), SAMSA_STOP_RETURN);
    EQ(ret, 11);
    EQ(depth, 4);
    EQ(c.r[13], 0x11000);
    free(memory);
    return 0;
}
