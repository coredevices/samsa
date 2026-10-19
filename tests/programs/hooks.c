/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
#include <stdint.h>
extern uint32_t host_step(uint32_t depth);
__attribute__((section(".entry"))) uint32_t guest_callback(uint32_t depth) {
    if (!depth)
        return 7;
    return host_step(depth - 1) + 1;
}
