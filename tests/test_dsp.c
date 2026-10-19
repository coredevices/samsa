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
static void packing(void) {
    // clang-format off
    static const Case cases[] = {
        /* ssat16/usat16 saturate each halfword of r1 into r0. */
        {"ssat16 #8", 0xf321, 0x0007, {0, 0x0100ff00, 0, 0}, 0,
         {0x007fff80, 0x0100ff00, 0, 0}, T | QF},
        {"ssat16 #16", 0xf321, 0x000f, {0, 0x80007fff, 0, 0}, 0,
         {0x80007fff, 0x80007fff, 0, 0}, T},
        {"usat16 #8", 0xf3a1, 0x0008, {0, 0x0100ff00, 0, 0}, 0,
         {0x00ff0000, 0x0100ff00, 0, 0}, T | QF},
        {"usat16 #0", 0xf3a1, 0x0000, {0, 0x00010000, 0, 0}, 0,
         {0, 0x00010000, 0, 0}, T | QF},
        {"usat16 exact", 0xf3a1, 0x0008, {0, 0x00ff0012, 0, 0}, QF,
         {0x00ff0012, 0x00ff0012, 0, 0}, T | QF},
        /* pkhbt/pkhtb r0, r1, r2, shift */
        {"pkhbt lsl #8", 0xeac1, 0x2002, {0, 0x11112222, 0x00abcd00, 0}, 0,
         {0xabcd2222, 0x11112222, 0x00abcd00, 0}, T},
        {"pkhtb asr #8", 0xeac1, 0x2022, {0, 0x11112222, 0x80001234, 0}, 0,
         {0x11110012, 0x11112222, 0x80001234, 0}, T},
        {"pkhtb asr #32", 0xeac1, 0x0022, {0, 0x11112222, 0x80001234, 0}, 0,
         {0x1111ffff, 0x11112222, 0x80001234, 0}, T},
        /* Byte lanes 0 and 2 of the rotated operand, optionally added. */
        {"sxtb16 ror #8", 0xfa2f, 0xf091, {0, 0x8012fe34, 0, 0}, 0,
         {0xff80fffe, 0x8012fe34, 0, 0}, T},
        {"uxtb16", 0xfa3f, 0xf081, {0, 0x8012fe34, 0, 0}, 0,
         {0x00120034, 0x8012fe34, 0, 0}, T},
        {"sxtab16 ror #8", 0xfa21, 0xf092, {0, 0x00010001, 0x8012fe34, 0}, 0,
         {0xff81ffff, 0x00010001, 0x8012fe34, 0}, T},
        {"uxtab16 ror #24", 0xfa31, 0xf0b2, {0, 0xffff0001, 0x8012fe34, 0}, 0,
         {0x00fd0081, 0xffff0001, 0x8012fe34, 0}, T},
    };
    // clang-format on
    CHECK_CASES(cases);
    rejected(0xf321, 0x0017); /* SSAT16 bit 4 */
    rejected(0xf3a1, 0x0028); /* USAT16 bit 5 */
    rejected(0xf32d, 0x0007);
    rejected(0xead1, 0x2002); /* PKH S bit */
    rejected(0xeac1, 0x2012); /* PKH T bit */
    rejected(0xeac1, 0x2f02);
    rejected(0xeacd, 0x2002);
    rejected(0xfa2d, 0xf092);
    rejected(0xfa61, 0xf082); /* op1 011x extends */
}
static void multiplies(void) {
    // clang-format off
    static const Case cases[] = {
        /* r0 = r1 x r2 (+ r3); X swaps r2's halves. */
        {"smlad", 0xfb21, 0x3002, {0, 0x00030002, 0x00050004, 10}, 0,
         {33, 0x00030002, 0x00050004, 10}, T},
        {"smladx", 0xfb21, 0x3012, {0, 0x00030002, 0x00050004, 10}, 0,
         {32, 0x00030002, 0x00050004, 10}, T},
        {"smlad Q", 0xfb21, 0x3002, {0, 0x00010001, 0x00010001, 0x7fffffff}, 0,
         {0x80000001, 0x00010001, 0x00010001, 0x7fffffff}, T | QF},
        {"smuad Q", 0xfb21, 0xf002, {0, 0x80008000, 0x80008000, 0}, 0,
         {0x80000000, 0x80008000, 0x80008000, 0}, T | QF},
        {"smuadx", 0xfb21, 0xf012, {0, 0xffff0002, 0x00030004, 0}, 0xf0000000 | GE(3),
         {2, 0xffff0002, 0x00030004, 0}, 0xf0000000 | T | GE(3)},
        {"smlsd", 0xfb41, 0x3002, {0, 0x00030002, 0x00050004, 10}, 0,
         {3, 0x00030002, 0x00050004, 10}, T},
        {"smlsdx", 0xfb41, 0x3012, {0, 0x00030002, 0x00050004, 10}, 0,
         {8, 0x00030002, 0x00050004, 10}, T},
        {"smlsd Q", 0xfb41, 0x3002, {0, 0x7fff8000, 0x80008000, 0x10000}, 0,
         {0x80008000, 0x7fff8000, 0x80008000, 0x10000}, T | QF},
        {"smusd", 0xfb41, 0xf002, {0, 0x80000001, 0x7fff0001, 0}, 0,
         {0x3fff8001, 0x80000001, 0x7fff0001, 0}, T},
        {"smusdx", 0xfb41, 0xf012, {0, 0x00030002, 0x00050004, 0}, 0,
         {0xfffffffe, 0x00030002, 0x00050004, 0}, T},
        /* Word by halfword, keeping bits 47:16. */
        {"smlawb", 0xfb31, 0x3002, {0, 0x00010000, 0x0000ffff, 5}, 0,
         {4, 0x00010000, 0x0000ffff, 5}, T},
        {"smlawt Q", 0xfb31, 0x3012, {0, 0x80000000, 0x80000000, 0x7fffffff}, 0,
         {0xbfffffff, 0x80000000, 0x80000000, 0x7fffffff}, T | QF},
        {"smulwb", 0xfb31, 0xf002, {0, 0xffff0000, 3, 0}, 0,
         {0xfffffffd, 0xffff0000, 3, 0}, T},
        {"smulwt", 0xfb31, 0xf012, {0, 0x12345678, 0x00020000, 0}, 0,
         {0x2468, 0x12345678, 0x00020000, 0}, T},
        /* Most significant word, R rounds. */
        {"smmul", 0xfb51, 0xf002, {0, 0x40000000, 6, 0}, 0, {1, 0x40000000, 6, 0}, T},
        {"smmulr", 0xfb51, 0xf012, {0, 0x40000000, 6, 0}, 0, {2, 0x40000000, 6, 0}, T},
        {"smmul -1", 0xfb51, 0xf002, {0, 0xffffffff, 1, 0}, 0,
         {0xffffffff, 0xffffffff, 1, 0}, T},
        {"smmulr -1", 0xfb51, 0xf012, {0, 0xffffffff, 1, 0}, 0, {0, 0xffffffff, 1, 0}, T},
        {"smmla", 0xfb51, 0x3002, {0, 0x40000000, 6, 10}, 0, {11, 0x40000000, 6, 10}, T},
        {"smmlar", 0xfb51, 0x3012, {0, 0x40000000, 6, 10}, 0, {12, 0x40000000, 6, 10}, T},
        {"smmls", 0xfb61, 0x3002, {0, 0x40000000, 6, 10}, 0, {8, 0x40000000, 6, 10}, T},
        {"smmlsr", 0xfb61, 0x3012, {0, 0x40000000, 6, 10}, 0, {9, 0x40000000, 6, 10}, T},
        /* Sum of absolute byte differences, wrapping with the accumulator. */
        {"usad8", 0xfb71, 0xf002, {0, 0x01ff1080, 0xff00207f, 0}, 0,
         {526, 0x01ff1080, 0xff00207f, 0}, T},
        {"usada8", 0xfb71, 0x3002, {0, 0x01ff1080, 0xff00207f, 0xfffffff0}, 0,
         {0x1fe, 0x01ff1080, 0xff00207f, 0xfffffff0}, T},
    };
    // clang-format on
    CHECK_CASES(cases);
    rejected(0xfb21, 0x3022); /* op2 1x */
    rejected(0xfb21, 0x30c2); /* bits 7:6 */
    rejected(0xfb71, 0xf012); /* USAD8 with bit 4 */
    rejected(0xfb61, 0xf002); /* SMMLS without Ra */
    rejected(0xfb21, 0xd002);
    rejected(0xfb5d, 0xf002);
}
static void long_multiplies(void) {
    // clang-format off
    static const Case cases[] = {
        /* r3:r0 += r1 x r2 */
        {"smlalbb", 0xfbc1, 0x0382, {0xffffffff, 0x0000ffff, 0x00000002, 0}, 0,
         {0xfffffffd, 0x0000ffff, 0x00000002, 0}, T},
        {"smlalbt", 0xfbc1, 0x0392, {0xffffffff, 0x00000003, 0x00050000, 0}, 0,
         {0x0000000e, 0x00000003, 0x00050000, 1}, T},
        {"smlaltb", 0xfbc1, 0x03a2, {0, 0x80000000, 0x00008000, 0x7fffffff}, 0,
         {0x40000000, 0x80000000, 0x00008000, 0x7fffffff}, T},
        {"smlaltt", 0xfbc1, 0x03b2, {0, 0xffff0000, 0x00010000, 0}, 0xf8000000,
         {0xffffffff, 0xffff0000, 0x00010000, 0xffffffff}, 0xf8000000 | T},
        {"smlald", 0xfbc1, 0x03c2, {0xffffffff, 0x80008000, 0x80008000, 0x7fffffff}, 0,
         {0x7fffffff, 0x80008000, 0x80008000, 0x80000000}, T},
        {"smlaldx", 0xfbc1, 0x03d2, {10, 0x00030002, 0x00050004, 0}, 0,
         {32, 0x00030002, 0x00050004, 0}, T},
        {"smlsld", 0xfbd1, 0x03c2, {0, 0x00030002, 0x00050004, 0}, 0,
         {0xfffffff9, 0x00030002, 0x00050004, 0xffffffff}, T},
        {"smlsldx", 0xfbd1, 0x03d2, {10, 0x00030002, 0x00050004, 5}, 0,
         {8, 0x00030002, 0x00050004, 5}, T},
        /* UMAAL adds both halves of the destination to the product; it cannot carry out. */
        {"umaal", 0xfbe1, 0x0362, {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff}, 0,
         {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff}, T},
        {"umaal small", 0xfbe1, 0x0362, {7, 0x10000, 0x10000, 2}, 0,
         {9, 0x10000, 0x10000, 1}, T},
    };
    // clang-format on
    CHECK_CASES(cases);
    rejected(0xfbc1, 0x03e2); /* op2 111x */
    rejected(0xfbd1, 0x0382); /* SMLSL<x><y> does not exist */
    rejected(0xfbe1, 0x0372);
    rejected(0xfbc1, 0x0002); /* RdLo == RdHi */
    rejected(0xfbc1, 0xd382);
    rejected(0xfbc1, 0x0f82);
}
int main(void) {
    parallel();
    saturating();
    packing();
    multiplies();
    long_multiplies();
    return 0;
}
