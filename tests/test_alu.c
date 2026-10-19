/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include "../src/alu.h"
#include "harness.h"
int main(void) {
    unsigned cy = 1;
    EQ(shift(0x80000001, 0, 0, &cy), 0x80000001);
    EQ(cy, 1);
    EQ(shift(0x80000001, 0, 32, &cy), 0);
    EQ(cy, 1);
    EQ(shift(0x80000001, 0, 33, &cy), 0);
    EQ(cy, 0);
    EQ(shift(0x80000001, 1, 32, &cy), 0);
    EQ(cy, 1);
    EQ(shift(0x80000001, 1, 33, &cy), 0);
    EQ(cy, 0);
    EQ(shift(0x80000001, 2, 32, &cy), UINT32_MAX);
    EQ(cy, 1);
    EQ(shift(0x80000001, 2, 255, &cy), UINT32_MAX);
    EQ(cy, 1);
    EQ(shift(0x80000001, 3, 32, &cy), 0x80000001);
    EQ(cy, 1);
    EQ(shift(0x80000001, 3, 33, &cy), 0xc0000000);
    EQ(cy, 1);
    EQ(shift(2, 4, 1, &cy), 0x80000001);
    EQ(cy, 0);
    cy = 1;
    EQ(imm_expand(0xff, &cy), 255);
    EQ(cy, 1);
    EQ(imm_expand(0x1ff, &cy), 0x00ff00ff);
    EQ(cy, 1);
    EQ(imm_expand(0x2ff, &cy), 0xff00ff00);
    EQ(cy, 1);
    EQ(imm_expand(0x3ff, &cy), UINT32_MAX);
    EQ(cy, 1);
    EQ(imm_expand(0x400, &cy), 0x80000000);
    EQ(cy, 1);
    SamsaCpu c;
    samsa_reset(&c);
    EQ(add_c(&c, UINT32_MAX, 0, 1, true), 0);
    EQ(samsa_get_xpsr(&c) >> 28, 6);
    EQ(add_c(&c, 0x7fffffff, 0, 1, true), 0x80000000);
    EQ(samsa_get_xpsr(&c) >> 28, 9);
    EQ(add_c(&c, 0, ~1u, 1, true), UINT32_MAX);
    EQ(samsa_get_xpsr(&c) >> 28, 8);
    EQ(add_c(&c, 0x80000000, ~1u, 1, true), 0x7fffffff);
    EQ(samsa_get_xpsr(&c) >> 28, 3);
    for (uint32_t state = 0; state < 256; ++state) {
        samsa_set_xpsr(&c, (state << 24) | 0x0000a800);
        uint32_t x = samsa_get_xpsr(&c);
        EQ(x & 0xf800fc00, (state << 24 & 0xf8000000) | 0xa800);
    }
    return 0;
}
