/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include "samsa/samsa.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s tests/programs/suite.bin\n", argv[0]);
        return 1;
    }
    uint8_t *mem = calloc(1, 65536);
    if (!mem)
        return 1;
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        free(mem);
        return 1;
    }
    size_t bytes = fread(mem, 1, 65536, f);
    int bad = ferror(f);
    fclose(f);
    if (!bytes || bad) {
        free(mem);
        return 1;
    }
    SamsaRegion region = {0x1000, 65536, mem, SAMSA_R | SAMSA_W | SAMSA_X};
    const SamsaHook done = {SAMSA_RETURN_PC, UINT32_MAX};
    SamsaCpu c;
    uint64_t total = 0;
    clock_t begin = clock();
    for (unsigned i = 0; i < 1000; ++i) {
        samsa_reset(&c);
        c.regions = &region;
        c.num_regions = 1;
        c.r[0] = 0xe000;
        c.r[1] = i;
        c.r[13] = 0x11000;
        c.r[14] = SAMSA_RETURN_PC | 1u;
        c.r[15] = 0x1000;
        c.hooks = &done;
        c.num_hooks = 1;
        uint32_t n;
        SamsaStop stop = samsa_run(&c, 1000000, &n);
        total += n;
        if (stop != SAMSA_STOP_HOOK) {
            fprintf(stderr, "stop %u at %08x\n", stop, c.fault_pc);
            free(mem);
            return 1;
        }
    }
    double seconds = (double)(clock() - begin) / CLOCKS_PER_SEC;
    printf("%llu instructions in %.3fs: %.0f instructions/s\n", (unsigned long long)total, seconds,
           total / seconds);
    free(mem);
    return 0;
}
