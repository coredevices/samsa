/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#ifndef SAMSA_TEST_H
#define SAMSA_TEST_H
#include "samsa/samsa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
#define EQ(a,b) CHECK((a) == (b))
typedef struct { SamsaCpu c; SamsaRegion region; uint8_t mem[4096]; } Fixture;
static inline void init(Fixture *f) {
    memset(f, 0, sizeof(*f));
    f->region = (SamsaRegion){0x1000, sizeof(f->mem), f->mem, SAMSA_R | SAMSA_W | SAMSA_X};
    f->c.regions = &f->region; f->c.num_regions = 1;
    f->c.r[15] = 0x1000; f->c.r[13] = 0x2000;
}
static inline void code(Fixture *f, const uint16_t *p, size_t n) { memcpy(f->mem, p, n * 2); }
#define CODE(f, ...) do { const uint16_t p[] = {__VA_ARGS__}; code((f), p, sizeof(p)/2); } while (0)
static inline uint32_t word(const Fixture *f, unsigned offset) { uint32_t v; memcpy(&v, f->mem + offset, 4); return v; }
#endif
