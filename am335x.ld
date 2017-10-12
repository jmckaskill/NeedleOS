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
    _irq_stack  = 0x40300000;
    _user_stack = 0x402fd000;
    _core_data = 0x402ff000;
    _global_page_list = 0x402fe000;
    _global_shared_list = 0x402fe020;
}

