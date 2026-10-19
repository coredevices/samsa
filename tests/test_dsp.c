/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"
/* One ARMv7E-M DSP instruction with r0-r3 and xPSR in, r0-r3 and xPSR out. */
typedef struct {
    const char *name;
    uint16_t first, second;
    uint32_t in[4], xpsr, out[4], out_xpsr;
} Case;
#define T 0x01000000u
#define GE(x) ((uint32_t)(x) << 16)
#define QF 0x08000000u
static void check(const Case *k, size_t count) {
    for (size_t i = 0; i < count; ++i, ++k) {
        Fixture f;
        init(&f);
        CODE(&f, k->first, k->second);
        memcpy(f.c.r, k->in, sizeof(k->in));
        samsa_set_xpsr(&f.c, k->xpsr);
        SamsaStop stop = samsa_run(&f.c, 1, NULL);
        bool ok = stop == SAMSA_STOP_BUDGET && !memcmp(f.c.r, k->out, sizeof(k->out)) &&
                  samsa_get_xpsr(&f.c) == k->out_xpsr;
        if (!ok)
            fprintf(stderr, "%s: stop %u r0-r3 %08x %08x %08x %08x xpsr %08x\n", k->name, stop,
                    f.c.r[0], f.c.r[1], f.c.r[2], f.c.r[3], samsa_get_xpsr(&f.c));
        CHECK(ok);
    }
}
#define CHECK_CASES(table) check((table), sizeof(table) / sizeof((table)[0]))
static void rejected(uint16_t first, uint16_t second) {
    Fixture f;
    init(&f);
    CODE(&f, first, second);
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_UNDEF);
}
static void parallel(void) {
    // clang-format off
    static const Case cases[] = {
        /* Signed and unsigned lanes set GE per byte; Q and NZCV are untouched. */
        {"sadd16", 0xfa91, 0xf002, {0, 0x7fff8000, 0x00018000, 0}, 0xf8000000 | GE(3),
         {0x80000000, 0x7fff8000, 0x00018000, 0}, 0xf8000000 | T | GE(12)},
        {"uadd8", 0xfa81, 0xf042, {0, 0xff80017f, 0x01800181, 0}, 0,
         {0x00000200, 0xff80017f, 0x01800181, 0}, T | GE(13)},
        {"usub8", 0xfac1, 0xf042, {0, 0x10203040, 0x20203041, 0}, 0,
         {0xf00000ff, 0x10203040, 0x20203041, 0}, T | GE(6)},
        {"ssub8", 0xfac1, 0xf002, {0, 0x80017f00, 0x01ff8001, 0}, GE(9),
         {0x7f02ffff, 0x80017f00, 0x01ff8001, 0}, T | GE(6)},
        {"sasx", 0xfaa1, 0xf002, {0, 0x00050003, 0x00040002, 0}, 0,
         {0x0007ffff, 0x00050003, 0x00040002, 0}, T | GE(12)},
        {"usax", 0xfae1, 0xf042, {0, 0x0001ffff, 0x00020001, 0}, 0,
         {0x00000001, 0x0001ffff, 0x00020001, 0}, T | GE(15)},
        /* Saturating and halving forms leave GE and Q alone. */
        {"qadd16", 0xfa91, 0xf012, {0, 0x7ff08010, 0x0020ff00, 0}, GE(5),
         {0x7fff8000, 0x7ff08010, 0x0020ff00, 0}, T | GE(5)},
        {"uqsub8", 0xfac1, 0xf052, {0, 0x10ff0580, 0x20010680, 0}, 0,
         {0x00fe0000, 0x10ff0580, 0x20010680, 0}, T},
        {"uqadd8 r3,r3,r1", 0xfa83, 0xf351, {0, 0x20ff0102, 0, 0xf0102030}, 0,
         {0, 0x20ff0102, 0, 0xffff2132}, T},
        {"uqasx", 0xfaa1, 0xf052, {0, 0xfff00005, 0x00200010, 0}, 0,
         {0xffff0000, 0xfff00005, 0x00200010, 0}, T},
        {"shadd8", 0xfa81, 0xf022, {0, 0x80807f01, 0x80ff7f02, 0}, GE(15),
         {0x80bf7f01, 0x80807f01, 0x80ff7f02, 0}, T | GE(15)},
        {"uhsub16", 0xfad1, 0xf062, {0, 0x00010000, 0x00000001, 0}, 0,
         {0x0000ffff, 0x00010000, 0x00000001, 0}, T},
        {"shasx", 0xfaa1, 0xf022, {0, 0x7fff8000, 0x7fff8000, 0}, 0,
         {0xffff8000, 0x7fff8000, 0x7fff8000, 0}, T},
        /* SEL takes Rn bytes where GE is set. */
        {"sel", 0xfaa1, 0xf082, {0, 0x11223344, 0xaabbccdd, 0}, GE(5),
         {0xaa22cc44, 0x11223344, 0xaabbccdd, 0}, T | GE(5)},
    };
    // clang-format on
    CHECK_CASES(cases);
    rejected(0xfab1, 0xf002); /* op1 011 */
    rejected(0xfa91, 0xf032); /* prefix 11 */
    rejected(0xfa91, 0xf072);
    rejected(0xfa9d, 0xf002); /* SP and PC operands */
    rejected(0xfa91, 0xfd02);
    rejected(0xfa91, 0xf00f);
    rejected(0xfaaf, 0xf082);
}
static void saturating(void) {
    // clang-format off
    static const Case cases[] = {
        /* qadd/qsub r0, r1, r2 compute r1 +/- r2; the D forms double r2 first. */
        {"qadd", 0xfa82, 0xf081, {0, 0x7fffffff, 1, 0}, 0,
         {0x7fffffff, 0x7fffffff, 1, 0}, T | QF},
        {"qadd exact", 0xfa82, 0xf081, {0, 5, 0xfffffffd, 0}, 0xf0000000 | GE(9),
         {2, 5, 0xfffffffd, 0}, 0xf0000000 | T | GE(9)},
        {"qsub", 0xfa82, 0xf0a1, {0, 0x80000000, 1, 0}, 0,
         {0x80000000, 0x80000000, 1, 0}, T | QF},
        {"qdadd", 0xfa82, 0xf091, {0, 0, 0x40000000, 0}, 0,
         {0x7fffffff, 0, 0x40000000, 0}, T | QF},
        {"qdadd doubling only", 0xfa82, 0xf091, {0, 0xffffffff, 0x40000000, 0}, 0,
         {0x7ffffffe, 0xffffffff, 0x40000000, 0}, T | QF},
        {"qdsub", 0xfa82, 0xf0b1, {0, 0x7fffffff, 0xc0000000, 0}, 0,
         {0x7fffffff, 0x7fffffff, 0xc0000000, 0}, T | QF},
        {"qdsub exact", 0xfa82, 0xf0b1, {0, 10, 3, 0}, QF,
         {4, 10, 3, 0}, T | QF},
    };
    // clang-format on
    CHECK_CASES(cases);
    rejected(0xfa8f, 0xf081);
    rejected(0xfa82, 0xf0c1); /* bits 7:6 = 11 */
}
int main(void) {
    parallel();
    saturating();
    return 0;
}
