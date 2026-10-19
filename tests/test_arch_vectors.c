/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"
static uint32_t next(uint32_t *s) {
    *s = *s * 1664525u + 1013904223u;
    return *s;
}
static unsigned byte(FILE *f) {
    int v = fgetc(f);
    CHECK(v != EOF);
    return (unsigned)v;
}
static uint32_t number(FILE *f, unsigned bytes) {
    uint32_t value = 0;
    for (unsigned i = 0; i < bytes; ++i)
        value |= (uint32_t)byte(f) << (i * 8);
    return value;
}
static void replay(const char *path, uint32_t minimum) {
    FILE *file = fopen(path, "rb");
    CHECK(file);
    char magic[8];
    EQ(fread(magic, 1, 8, file), 8);
    CHECK(!memcmp(magic, "SAMSA002", 8));
    uint32_t count = number(file, 4), families = number(file, 2);
    CHECK(count >= minimum && count >= families);
    CHECK(families < 512);
    char names[512][100];
    unsigned hits[512] = {0};
    for (unsigned i = 0; i < families; ++i) {
        unsigned n = byte(file);
        CHECK(n < 100);
        EQ(fread(names[i], 1, n, file), n);
        names[i][n] = 0;
    }
    for (uint32_t index = 0; index < count; ++index) {
        uint32_t seed = number(file, 4);
        unsigned family = number(file, 2), halfwords = byte(file), steps = byte(file);
        unsigned regmask = number(file, 2), patches = number(file, 2);
        CHECK(family < families);
        ++hits[family];
        CHECK(halfwords <= 16);
        CHECK(steps <= 8);
        Fixture f;
        init(&f);
        f.c.r[15] = 0x1100;
        uint32_t state = seed ^ 0x9e3779b9u, block[16];
        for (unsigned i = 0; i < 16; ++i)
            block[i] = next(&state);
        for (unsigned offset = 0; offset < 4096; offset += 64)
            memcpy(f.mem + offset, block, 64);
        for (unsigned r = 0; r < 15; ++r)
            f.c.r[r] = next(&seed);
        f.c.r[13] &= ~3u;
        samsa_set_xpsr(&f.c, next(&seed) & 0xf8000000u);
        for (unsigned i = 0; i < halfwords; ++i) {
            uint16_t h = (uint16_t)number(file, 2);
            memcpy(f.mem + 0x100 + 2 * i, &h, 2);
        }
        for (unsigned r = 0; r < 16; ++r)
            if (regmask & (1u << r))
                f.c.r[r] = number(file, 4);
        for (unsigned i = 0; i < patches; ++i) {
            unsigned offset = number(file, 2), size = byte(file);
            uint32_t value = number(file, 4);
            CHECK(size <= 4 && offset <= 4096 - size);
            memcpy(f.mem + offset, &value, size);
        }
        uint32_t expected[16];
        for (unsigned r = 0; r < 16; ++r)
            expected[r] = number(file, 4);
        uint32_t xpsr = number(file, 4);
        unsigned stop = byte(file), info = byte(file), changes = number(file, 2);
        uint8_t expected_memory[4096];
        memcpy(expected_memory, f.mem, 4096);
        for (unsigned i = 0; i < changes; ++i) {
            unsigned offset = number(file, 2);
            CHECK(offset < 4096);
            expected_memory[offset] = (uint8_t)byte(file);
        }
        SamsaCpu saved = f.c;
        uint8_t saved_memory[4096];
        memcpy(saved_memory, f.mem, 4096);
        for (unsigned mode = 0; mode < (SAMSA_CACHE_ENTRIES ? 4u : 2u); ++mode) {
#if SAMSA_CACHE_ENTRIES
            SamsaCpu prior = f.c;
#endif
            f.c = saved;
#if SAMSA_CACHE_ENTRIES
            if (mode >= 2)
                f.c._cache = prior._cache;
#endif
            bool split = (mode & 1u) != 0;
            memcpy(f.mem, saved_memory, 4096);
            SamsaStop actual = SAMSA_STOP_BUDGET;
            for (unsigned step = 0; step < (split ? steps : 1u); ++step) {
                actual = samsa_run(&f.c, split ? 1u : steps, NULL);
                if (actual != SAMSA_STOP_BUDGET)
                    break;
            }
            bool fail = actual != stop || f.c.stop_info != info ||
                        (samsa_get_xpsr(&f.c) & 0xff00fc00u) != xpsr ||
                        memcmp(expected, f.c.r, sizeof(expected)) ||
                        memcmp(expected_memory, f.mem, 4096);
            if (fail) {
                fprintf(stderr, "vector %u %s (mode %u): stop %u/%u xpsr %08x/%08x\n", index,
                        names[family], mode, actual, stop, samsa_get_xpsr(&f.c), xpsr);
                fprintf(stderr, " code:");
                for (unsigned i = 0; i < halfwords; ++i)
                    fprintf(stderr, " %02x%02x", f.mem[0x101 + 2 * i], f.mem[0x100 + 2 * i]);
                fprintf(stderr, "\n");
                for (unsigned r = 0; r < 16; ++r)
                    if (f.c.r[r] != expected[r])
                        fprintf(stderr, " r%u=%08x expected %08x\n", r, f.c.r[r], expected[r]);
                for (unsigned i = 0; i < 4096; ++i)
                    if (f.mem[i] != expected_memory[i]) {
                        fprintf(stderr, " memory %04x=%02x expected %02x\n", i + 0x1000, f.mem[i],
                                expected_memory[i]);
                        break;
                    }
                exit(1);
            }
        }
    }
    for (unsigned i = 0; i < families; ++i)
        CHECK(hits[i]);
    EQ(fgetc(file), EOF);
    fclose(file);
}
int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "usage: %s [full-state-vectors.bin]\n", argv[0]);
        return 2;
    }
    if (argc == 2)
        replay(argv[1], 1);
    else {
        replay(SAMSA_SOURCE_DIR "/tests/vectors/architecture.bin", 100000);
        replay(SAMSA_SOURCE_DIR "/tests/vectors/sequences.bin", 10000);
    }
    return 0;
}
