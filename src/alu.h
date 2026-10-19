/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#ifndef SAMSA_ALU_H
#define SAMSA_ALU_H
#include "samsa/samsa.h"
#if !defined(__BYTE_ORDER__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "Samsa requires a little-endian host"
#endif
#define N 128u
#define Z 64u
#define C 32u
#define V 16u
#define Q 8u
uint32_t sx(uint32_t x, unsigned bits);
uint32_t shift(uint32_t x, unsigned type, unsigned amount, unsigned *carry);
uint32_t imm_expand(unsigned imm, unsigned *carry);
uint32_t add_c(SamsaCpu *c, uint32_t a, uint32_t b, unsigned carry, bool flags);
void nz(SamsaCpu *c, uint32_t value);
void nzc(SamsaCpu *c, uint32_t value, unsigned carry);
bool condition(const SamsaCpu *c, unsigned cond);
bool mem_read(SamsaCpu *c, uint32_t addr, unsigned size, unsigned perm, uint32_t *v);
bool mem_write(SamsaCpu *c, uint32_t addr, unsigned size, uint32_t value);
bool aligned(SamsaCpu *c, uint32_t addr, unsigned alignment);
bool target_special(const SamsaCpu *c, uint32_t addr);
bool branch(SamsaCpu *c, uint32_t addr, bool exchange);
bool decode16(SamsaCpu *c, uint16_t h, uint32_t pc, bool in_it);
bool decode32(SamsaCpu *c, uint16_t h, uint16_t l, uint32_t pc);
bool multiple(SamsaCpu *c, unsigned rn, uint32_t list, bool load, bool db, bool wb);
static inline uint32_t reg(const SamsaCpu *c, unsigned r, uint32_t pc) {
    return r == 15 ? pc + 4u : c->r[r];
}
static inline bool undef(SamsaCpu *c) { c->_stop = SAMSA_STOP_UNDEF; return false; }
static inline bool usage(SamsaCpu *c) { c->_stop = SAMSA_STOP_USAGE; return false; }
static inline bool last_it(const SamsaCpu *c) { return (c->itstate & 7u) == 0; }
#endif
