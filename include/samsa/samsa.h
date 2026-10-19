/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#ifndef SAMSA_SAMSA_H
#define SAMSA_SAMSA_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define SAMSA_R 1u
#define SAMSA_W 2u
#define SAMSA_X 4u
#define SAMSA_RETURN_PC UINT32_C(0xfffffffe)
typedef enum {
    SAMSA_STOP_BUDGET, SAMSA_STOP_HOOK, SAMSA_STOP_RETURN, SAMSA_STOP_SVC,
    SAMSA_STOP_BKPT, SAMSA_STOP_UNDEF, SAMSA_STOP_MEMFAULT, SAMSA_STOP_USAGE
} SamsaStop;
typedef struct { uint32_t guest_base, size; uint8_t *host; uint8_t perms; } SamsaRegion;
typedef struct { uint32_t start, end; } SamsaHook;
struct SamsaCpu;
typedef struct {
    bool (*read)(struct SamsaCpu *, uint32_t, unsigned, uint32_t *);
    bool (*write)(struct SamsaCpu *, uint32_t, unsigned, uint32_t);
} SamsaBus;
typedef struct SamsaCpu {
    uint32_t r[16];
    const SamsaRegion *regions;
    const SamsaHook *hooks;
    const SamsaBus *bus;
    void *user;
    uint32_t exclusive_addr, fault_pc, fault_addr;
    uint16_t num_regions, num_hooks;
    uint8_t itstate, stop_info;
    /* Internal state. Use xPSR accessors for flags. */
    uint8_t _flags, _exclusive_size, _stop, _calling;
} SamsaCpu;
/* Reset clears the entire struct, including configuration pointers. */
void samsa_reset(SamsaCpu *cpu);
SamsaStop samsa_run(SamsaCpu *cpu, uint32_t max_insns, uint32_t *executed);
uint32_t samsa_get_xpsr(const SamsaCpu *cpu);
void samsa_set_xpsr(SamsaCpu *cpu, uint32_t xpsr);
uint32_t samsa_arg(SamsaCpu *cpu, unsigned word_index);
uint64_t samsa_arg64(SamsaCpu *cpu, unsigned word_index);
void samsa_return32(SamsaCpu *cpu, uint32_t value);
void samsa_return64(SamsaCpu *cpu, uint64_t value);
typedef SamsaStop (*SamsaStopHandler)(SamsaCpu *, SamsaStop, void *);
/* Handler returns BUDGET to continue. Each call has a finite instruction limit. */
SamsaStop samsa_call_budget(SamsaCpu *, uint32_t fn, const uint32_t *args,
                          unsigned nargs, uint32_t *ret, SamsaStopHandler,
                          void *ctx, uint32_t max_insns);
SamsaStop samsa_call(SamsaCpu *, uint32_t fn, const uint32_t *args,
                   unsigned nargs, uint32_t *ret, SamsaStopHandler, void *ctx);
void samsa_invalidate(SamsaCpu *cpu, uint32_t addr, uint32_t len);
#ifdef __cplusplus
}
#endif
#endif
