/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"
static uint32_t random_word(uint32_t *s) {
    *s = *s * 1664525u + 1013904223u;
    return *s;
}
int main(void) {
    Fixture f;
    uint32_t s = 0x53414d53;
    for (unsigned i = 0; i < 300000; ++i) {
        init(&f);
        for (unsigned j = 0; j < 15; ++j)
            f.c.r[j] = random_word(&s);
        samsa_set_xpsr(&f.c, random_word(&s) & 0xf8000000u);
        uint16_t h = i < 65536 ? (uint16_t)i : (uint16_t)random_word(&s),
                 l = (uint16_t)random_word(&s);
        CODE(&f, h, l, 0xbe00);
        uint32_t n;
        SamsaStop stop = samsa_run(&f.c, 3, &n);
        CHECK(n <= 3);
        CHECK(stop <= SAMSA_STOP_USAGE);
    }
    return 0;
}
