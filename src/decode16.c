/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include "alu.h"
bool decode16(SamsaCpu *c, uint16_t h, uint32_t pc, bool it) {
    unsigned d = h & 7u;
    if ((h & 0xf800u) == 0x2000u) { c->r[h >> 8 & 7u] = h & 255u; if (!it) nz(c, h & 255u); return true; }
    if ((h & 0xf800u) == 0x3000u) { d = h >> 8 & 7u; c->r[d] = add_c(c, c->r[d], h & 255u, 0, !it); return true; }
    if ((h & 0xf800u) == 0xe000u) return branch(c, pc + 4u + sx((h & 2047u) << 1, 12), false);
    if ((h & 0xff00u) == 0x4700u) { if (h & 7u) return undef(c); uint32_t a = reg(c, h >> 3 & 15u, pc); if (h & 128u) c->r[14] = (pc + 2u) | 1u; return branch(c, a, true); }
    if ((h & 0xff00u) == 0xbe00u) { c->stop_info = (uint8_t)h; c->_stop = SAMSA_STOP_BKPT; return false; }
    return undef(c);
}
bool multiple(SamsaCpu *c, unsigned rn, uint32_t list, bool load, bool db, bool wb) {
    (void)c; (void)rn; (void)list; (void)load; (void)db; (void)wb; return false;
}
