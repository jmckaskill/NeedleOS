OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

SECTIONS {
    . = 0x402f0400;
    .init : { *(.init) }
    .text : { *(.text) }
    .data : { *(.data) }
    .rodata : { *(.rodata) }
    .bss : { *(.bss) }
    CORE        = 0x40300000;
    KERNEL      = 0x40301000;
    DESCRIPTORS = 0x80000000;
    PAGES       = 0x80010000;
}

