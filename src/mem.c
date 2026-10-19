/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include "alu.h"
#include <string.h>
static const SamsaRegion *region(SamsaCpu *c, uint32_t a, unsigned size) {
    if (a > UINT32_MAX - (size - 1u)) return NULL;
    for (unsigned i = 0; i < c->num_regions; ++i) {
        const SamsaRegion *r = &c->regions[i];
        if (a >= r->guest_base && size <= r->size && a - r->guest_base <= r->size - size)
            return r;
    }
    return NULL;
}
static bool fault(SamsaCpu *c, uint32_t a) {
    c->fault_addr = a; c->_stop = SAMSA_STOP_MEMFAULT; return false;
}
bool aligned(SamsaCpu *c, uint32_t a, unsigned alignment) {
    if (!(a & (alignment - 1u))) return true;
    c->fault_addr = a; return usage(c);
}
bool mem_read(SamsaCpu *c, uint32_t a, unsigned size, unsigned perm, uint32_t *v) {
    const SamsaRegion *r = region(c, a, size);
    *v = 0;
    if (r) {
        if (!(r->perms & perm)) return fault(c, a);
        memcpy(v, r->host + (a - r->guest_base), size);
        return true;
    }
    /* Bus is data-only: execute permission must come from a region. */
    if (perm != SAMSA_X && a <= UINT32_MAX - (size - 1u) && c->bus && c->bus->read &&
        c->bus->read(c, a, size, v)) return true;
    return fault(c, a);
}
bool mem_write(SamsaCpu *c, uint32_t a, unsigned size, uint32_t v) {
    const SamsaRegion *r = region(c, a, size);
    if (r) {
        if (!(r->perms & SAMSA_W)) return fault(c, a);
        memcpy(r->host + (a - r->guest_base), &v, size); return true;
    }
    if (a <= UINT32_MAX - (size - 1u) && c->bus && c->bus->write &&
        c->bus->write(c, a, size, v)) return true;
    return fault(c, a);
}
