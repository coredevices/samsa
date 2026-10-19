/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include "samsa/samsa.h"
#include <string.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    SamsaCpu cpu;
    uint8_t memory[1024];
    samsa_reset(&cpu);
    memset(memory, 0, sizeof(memory));
    size_t state_bytes = sizeof(cpu.r) - sizeof(cpu.r[15]);
    if (size < state_bytes + 4)
        return 0;
    memcpy(cpu.r, data, state_bytes);
    data += state_bytes;
    size -= state_bytes;
    samsa_set_xpsr(&cpu, (uint32_t)*data++ << 24);
    --size;
    if (size > sizeof(memory))
        size = sizeof(memory);
    memcpy(memory, data, size);
    SamsaRegion region = {0x1000, sizeof(memory), memory, SAMSA_R | SAMSA_W | SAMSA_X};
    cpu.regions = &region;
    cpu.num_regions = 1;
    cpu.r[15] = 0x1000;
    uint32_t n = 0;
    (void)samsa_run(&cpu, 100, &n);
    if (n > 100)
        __builtin_trap();
    return 0;
}
