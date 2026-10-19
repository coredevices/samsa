/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include "alu.h"
#include <string.h>
static const SamsaRegion *region(SamsaCpu *c, uint32_t a, unsigned size, unsigned kind) {
    if (a > UINT32_MAX - (size - 1u))
        return NULL;
#if SAMSA_CACHE_ENTRIES
    unsigned cached = c->_cache.region[kind];
    if (cached && cached <= c->num_regions) {
        const SamsaRegion *r = &c->regions[cached - 1u];
        if (a >= r->guest_base && size <= r->size && a - r->guest_base <= r->size - size)
            return r;
    }
#else
    (void)kind;
#endif
    for (unsigned i = 0; i < c->num_regions; ++i) {
        const SamsaRegion *r = &c->regions[i];
        if (a >= r->guest_base && size <= r->size && a - r->guest_base <= r->size - size) {
#if SAMSA_CACHE_ENTRIES
            c->_cache.region[kind] = (uint16_t)(i + 1u);
#endif
            return r;
        }
    }
    return NULL;
}
static bool intersects(const SamsaCpu *c, uint32_t a, unsigned size) {
    for (unsigned i = 0; i < c->num_regions; ++i) {
        const SamsaRegion *r = &c->regions[i];
        if (r->size &&
            (a >= r->guest_base ? a - r->guest_base < r->size : r->guest_base - a < size))
            return true;
    }
    return false;
}
static bool fault(SamsaCpu *c, uint32_t a) {
    c->fault_addr = a;
    c->_stop = SAMSA_STOP_MEMFAULT;
    return false;
}
bool aligned(SamsaCpu *c, uint32_t a, unsigned alignment) {
    if (!(a & (alignment - 1u)))
        return true;
    c->fault_addr = a;
    return usage(c);
}
bool mem_fetch(SamsaCpu *c, uint32_t a, uint32_t *halfword) {
    const SamsaRegion *r = region(c, a, 2, 2);
    if (!r || !(r->perms & SAMSA_X))
        return fault(c, a);
    if (halfword) {
        uint16_t h;
        memcpy(&h, r->host + (a - r->guest_base), sizeof(h));
        *halfword = h;
    }
    return true;
}
#if SAMSA_CACHE_ENTRIES
bool mem_exec(SamsaCpu *c, uint32_t a, bool wide) {
    const SamsaRegion *r = region(c, a, wide ? 4 : 2, 2);
    if (r)
        return r->perms & SAMSA_X ? true : fault(c, a);
    return mem_fetch(c, a, NULL) && (!wide || mem_fetch(c, a + 2u, NULL));
}
#endif
bool mem_read(SamsaCpu *c, uint32_t a, unsigned size, unsigned perm, uint32_t *v) {
    const SamsaRegion *r = region(c, a, size, perm == SAMSA_X ? 2 : 0);
    *v = 0;
    if (r) {
        if (!(r->perms & perm))
            return fault(c, a);
        memcpy(v, r->host + (a - r->guest_base), size);
        return true;
    }
    /* Bus is data-only: execute permission must come from a region. */
    if (perm != SAMSA_X && !intersects(c, a, size) && a <= UINT32_MAX - (size - 1u) && c->bus &&
        c->bus->read && c->bus->read(c, a, size, v)) {
        *v &= UINT32_MAX >> ((4u - size) * 8u);
        return true;
    }
    return fault(c, a);
}
bool mem_write(SamsaCpu *c, uint32_t a, unsigned size, uint32_t v) {
    const SamsaRegion *r = region(c, a, size, 1);
    if (r) {
        if (!(r->perms & SAMSA_W))
            return fault(c, a);
        memcpy(r->host + (a - r->guest_base), &v, size);
#if SAMSA_CACHE_ENTRIES
        if (r->perms & SAMSA_X)
            invalidate_code(c, a, size);
#endif
        return true;
    }
    if (!intersects(c, a, size) && a <= UINT32_MAX - (size - 1u) && c->bus && c->bus->write &&
        c->bus->write(c, a, size, v))
        return true;
    return fault(c, a);
}
