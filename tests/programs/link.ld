/* SPDX-FileCopyrightText: 2026 Joshua Jun */
/* SPDX-License-Identifier: Apache-2.0 */
ENTRY(program)
SECTIONS {
  . = 0x1000;
  .text : { KEEP(*(.entry)) *(.text*) *(.rodata*) }
  .data : { *(.data*) }
  .bss (NOLOAD) : { *(.bss*) *(COMMON) }
  /DISCARD/ : { *(.comment) *(.ARM.exidx*) *(.ARM.extab*) }
}
PROVIDE(host_step = 0x3001);
ASSERT(SIZEOF(.bss) == 0, "Fixtures must not require a loader to zero BSS")
