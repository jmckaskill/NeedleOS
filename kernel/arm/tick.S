#include "common.S"

.syntax unified
.code 16

current_tick:
    // no arguments
    // return
    // r0 - current tick
    // registers
    // r12 - scratch

    ldr r12, =TICK_COUNT_ADDR
    ldr r0, [r12]
    subs pc, lr, #0

