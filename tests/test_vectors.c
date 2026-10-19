/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"
static uint32_t next(uint32_t *s) {
    *s = *s * 1664525u + 1013904223u;
    return *s;
}
static void replay(const char *name) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/tests/vectors/%s.bin", SAMSA_SOURCE_DIR, name);
    FILE *file = fopen(path, "rb");
    CHECK(file);
    uint8_t magic[8];
    uint32_t count;
    EQ(fread(magic, 1, 8, file), 8);
    CHECK(!memcmp(magic, "SAMSV001", 8));
    EQ(fread(&count, 4, 1, file), 1);
    CHECK(count >= 100000);
    for (uint32_t i = 0; i < count; ++i) {
        uint16_t code_words[2];
        uint32_t seed, expected[4];
        EQ(fread(code_words, 2, 2, file), 2);
        EQ(fread(&seed, 4, 1, file), 1);
        EQ(fread(expected, 4, 4, file), 4);
        Fixture f;
        init(&f);
        code(&f, code_words, 2);
        for (unsigned j = 0; j < 15; ++j)
            f.c.r[j] = next(&seed);
        f.c.r[13] &= ~3u;
        uint32_t initial[16];
        memcpy(initial, f.c.r, sizeof(initial));
        samsa_set_xpsr(&f.c, next(&seed) & 0xf8000000u);
        SamsaStop stop = samsa_run(&f.c, 1, NULL);
        if (f.c.r[0] != expected[0] || (samsa_get_xpsr(&f.c) & 0xf8000000u) != expected[1] ||
            f.c.r[15] != expected[2] || stop != expected[3]) {
            fprintf(stderr, "%s vector %u %04x %04x: r0 %08x/%08x flags %08x/%08x\n", name, i,
                    code_words[0], code_words[1], f.c.r[0], expected[0],
                    samsa_get_xpsr(&f.c) & 0xf8000000u, expected[1]);
            exit(1);
        }
        CHECK(!memcmp(initial + 1, f.c.r + 1, 14 * 4));
    }
    EQ(fgetc(file), EOF);
    fclose(file);
}
int main(void) {
    replay("spec");
    replay("unicorn");
    return 0;
}
