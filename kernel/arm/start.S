#include "common.S"

.syntax unified
.code 32

.section .init
_start:
    // setup stacks
    cpsid aif, #MODE_IRQ
    ldr sp, =IRQ_STACK_TOP

    cpsid aif, #MODE_SYS
    ldr sp, =MAIN_STACK_TOP

    // set the exception base address
    ldr r0, =exception_table
    mcr p15, 0, r0, c12, c0, 0

    // take exceptions in thumb mode
    mrc p15, 0, r0, c1, c0, 0 // Read SCTLR
    orr r0, r0, #SCTLR_THUMB_EXCEPTIONS
    mcr p15, 0, r0, c1, c0, 0 // Write SCTLR

    // register assignments
    // r0 - 0
    // r1 - global
    // r2-5, desc pointers

    // setup global memory pool pointers
    mov r0, #0
    ldr r1, =GLOBAL_DATA
    ldr r2, =KERNEL_DESC
    ldr r3, =USER_DESC
    ldr r4, =SHARED_DESC
    ldr r5, =DESC_END

    str r0, [r1, #(GLOBAL_LIST_KERNEL + LIST_OFF_FREE)]
    str r0, [r1, #(GLOBAL_LIST_USER + LIST_OFF_FREE)]
    str r0, [r1, #(GLOBAL_LIST_SHARED + LIST_OFF_FREE)]

    str r2, [r1, #(GLOBAL_LIST_KERNEL + LIST_OFF_TOP)]
    str r3, [r1, #(GLOBAL_LIST_KERNEL + LIST_OFF_END)]
    str r3, [r1, #(GLOBAL_LIST_USER + LIST_OFF_TOP)]
    str r4, [r1, #(GLOBAL_LIST_USER + LIST_OFF_END)]
    str r4, [r1, #(GLOBAL_LIST_SHARED + LIST_OFF_TOP)]
    str r5, [r1, #(GLOBAL_LIST_SHARED + LIST_OFF_END)]

    // clear the page descriptors task pointers
clear_top_descriptor:
    str r0, [r2, #DESC_OFF_TASK]
    add r2, #DESC_SIZE
    cmp r2, r5
    blt clear_top_descriptor

    bl init_board
    b main
loop:
    b loop
