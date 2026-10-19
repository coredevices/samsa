/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include "alu.h"
#include <string.h>
_Static_assert(sizeof(SamsaCpu) < 128, "CPU size budget exceeded");
void samsa_reset(SamsaCpu *c) { memset(c, 0, sizeof(*c)); }
uint32_t samsa_get_xpsr(const SamsaCpu *c) {
    return (uint32_t)c->_flags << 24 | UINT32_C(0x01000000) | (uint32_t)(c->itstate & 3u) << 25 |
           (uint32_t)(c->itstate & 0xfcu) << 8;
}
void samsa_set_xpsr(SamsaCpu *c, uint32_t x) {
    c->_flags = (uint8_t)(x >> 24 & 0xf8u);
    c->itstate = (uint8_t)((x >> 25 & 3u) | (x >> 8 & 0xfcu));
}
bool target_special(const SamsaCpu *c, uint32_t a) {
    a &= ~1u;
    if (c->_calling && a == SAMSA_RETURN_PC)
        return true;
    for (unsigned i = 0; i < c->num_hooks; ++i)
        if (a >= c->hooks[i].start && a < c->hooks[i].end)
            return true;
    return false;
}
bool branch(SamsaCpu *c, uint32_t a, bool exchange) {
    if (!last_it(c))
        return usage(c);
    if (exchange && !(a & 1u) && !target_special(c, a))
        return usage(c);
    c->r[15] = a & ~1u;
    return true;
}
SamsaStop samsa_run(SamsaCpu *c, uint32_t max, uint32_t *executed) {
    uint32_t count = 0;
    c->_stop = SAMSA_STOP_BUDGET;
    c->stop_info = 0;
    while (count < max) {
        uint32_t pc = c->r[15], h, l = 0;
        if (target_special(c, pc)) {
            c->_stop = c->_calling && pc == SAMSA_RETURN_PC ? SAMSA_STOP_RETURN : SAMSA_STOP_HOOK;
            break;
        }
        c->fault_pc = pc;
        if (!aligned(c, pc, 2) || !mem_read(c, pc, 2, SAMSA_X, &h))
            break;
        bool wide = (h >> 11) >= 29u;
        if (wide && !mem_read(c, pc + 2u, 2, SAMSA_X, &l))
            break;
        uint8_t it = c->itstate;
        c->r[15] = pc + (wide ? 4u : 2u);
        bool ok = true;
        if (!it || condition(c, it >> 4))
            ok = wide ? decode32(c, (uint16_t)h, (uint16_t)l, pc)
                      : decode16(c, (uint16_t)h, pc, it != 0);
        ++count;
        if (it)
            c->itstate = (it & 7u) ? (uint8_t)((it & 0xe0u) | ((it << 1) & 31u)) : 0;
        if (!ok) {
            if (c->_stop != SAMSA_STOP_SVC && c->_stop != SAMSA_STOP_BKPT) {
                c->r[15] = pc;
                c->itstate = it;
            }
            break;
        }
    }
    if (executed)
        *executed = count;
    return (SamsaStop)c->_stop;
}
uint32_t samsa_arg(SamsaCpu *c, unsigned i) {
    uint32_t v = 0;
    if (i < 4)
        return c->r[i];
    if (i - 4u > (UINT32_MAX - c->r[13]) / 4u) {
        c->fault_addr = c->r[13];
        c->_stop = SAMSA_STOP_MEMFAULT;
        return 0;
    }
    (void)mem_read(c, c->r[13] + (i - 4u) * 4u, 4, SAMSA_R, &v);
    return v;
}
uint64_t samsa_arg64(SamsaCpu *c, unsigned i) {
    if (i == UINT32_MAX) {
        usage(c);
        return 0;
    }
    i = (i + 1u) & ~1u;
    uint32_t lo = samsa_arg(c, i), hi = samsa_arg(c, i + 1u);
    return (uint64_t)hi << 32 | lo;
}
SamsaStop samsa_status(const SamsaCpu *c) { return (SamsaStop)c->_stop; }
void samsa_return32(SamsaCpu *c, uint32_t v) {
    c->r[0] = v;
    (void)branch(c, c->r[14], true);
}
void samsa_return64(SamsaCpu *c, uint64_t v) {
    c->r[1] = (uint32_t)(v >> 32);
    samsa_return32(c, (uint32_t)v);
}
SamsaStop samsa_call_budget(SamsaCpu *c, uint32_t fn, const uint32_t *args, unsigned nargs,
                            uint32_t *ret, SamsaStopHandler h, void *ctx, uint32_t budget) {
    uint32_t saved[16], xpsr = samsa_get_xpsr(c);
    uint8_t calling = c->_calling, stop = c->_stop, stop_info = c->stop_info;
    uint32_t fault_pc = c->fault_pc, fault_addr = c->fault_addr;
    memcpy(saved, c->r, sizeof(saved));
    c->_calling = 1;
    c->itstate = 0;
    c->_stop = SAMSA_STOP_BUDGET;
    SamsaStop result = SAMSA_STOP_USAGE;
    if (nargs && !args)
        goto done;
    uint32_t words = nargs > 4 ? nargs - 4u : 0;
    if (words > UINT32_MAX / 4u || c->r[13] < words * 4u)
        goto done;
    c->r[13] = (c->r[13] - words * 4u) & ~7u;
    for (unsigned i = 0; i < nargs; ++i) {
        if (i < 4)
            c->r[i] = args[i];
        else if (!mem_write(c, c->r[13] + (i - 4u) * 4u, 4, args[i])) {
            result = (SamsaStop)c->_stop;
            goto done;
        }
    }
    c->r[14] = SAMSA_RETURN_PC | 1u;
    if (!branch(c, fn, true))
        goto done;
    for (;;) {
        uint32_t n;
        result = samsa_run(c, budget, &n);
        budget -= n;
        if (result == SAMSA_STOP_RETURN) {
            if (ret)
                *ret = c->r[0];
            break;
        }
        if (!h ||
            (result != SAMSA_STOP_HOOK && result != SAMSA_STOP_SVC && result != SAMSA_STOP_BKPT))
            break;
        result = h(c, result, ctx);
        if (result != SAMSA_STOP_BUDGET)
            break;
        if (c->_stop == SAMSA_STOP_USAGE || c->_stop == SAMSA_STOP_MEMFAULT) {
            result = (SamsaStop)c->_stop;
            break;
        }
        if (!budget)
            break;
        /* A hook that consumes no guest instructions still consumes fuel. */
        if (!n)
            --budget;
    }
done:
    memcpy(c->r, saved, sizeof(saved));
    samsa_set_xpsr(c, xpsr);
    c->_calling = calling;
    c->_stop = stop;
    if (result == SAMSA_STOP_RETURN) {
        c->stop_info = stop_info;
        c->fault_pc = fault_pc;
        c->fault_addr = fault_addr;
    }
    return result;
}
SamsaStop samsa_call(SamsaCpu *c, uint32_t fn, const uint32_t *args, unsigned nargs, uint32_t *ret,
                     SamsaStopHandler h, void *ctx) {
    return samsa_call_budget(c, fn, args, nargs, ret, h, ctx, 1000000u);
}
void samsa_invalidate(SamsaCpu *c, uint32_t a, uint32_t len) {
    (void)c;
    (void)a;
    (void)len;
}
