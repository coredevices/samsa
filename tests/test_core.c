/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"
int main(void) {
    Fixture f; uint32_t n; init(&f);
    /* movs r0,#0; adds r0,#1; b .-2 */
    CODE(&f, 0x2000, 0x3001, 0xe7fd);
    EQ(samsa_run(&f.c, 5, &n), SAMSA_STOP_BUDGET); EQ(n, 5); EQ(f.c.r[0], 2);
    EQ(samsa_run(&f.c, 2, &n), SAMSA_STOP_BUDGET); EQ(f.c.r[0], 3);
    init(&f); CODE(&f, 0xbe42);
    EQ(samsa_run(&f.c, 5, &n), SAMSA_STOP_BKPT); EQ(n, 1); EQ(f.c.stop_info, 0x42);
    f.c.r[15] = 0; EQ(samsa_run(&f.c, 1, &n), SAMSA_STOP_MEMFAULT); EQ(n, 0);
    init(&f); CODE(&f, 0x4700); f.c.r[0] = 0x1000;
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_USAGE);
    init(&f); f.region.perms = SAMSA_R;
    EQ(samsa_run(&f.c, 1, NULL), SAMSA_STOP_MEMFAULT);
    EQ(samsa_run(&f.c, 0, &n), SAMSA_STOP_BUDGET); EQ(n, 0);
    CHECK(sizeof(SamsaCpu) < 128);
    return 0;
}
