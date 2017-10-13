#include "common.S"

.syntax unified
.code 16

release_global:
    // function to release a list of resources to a global free list
    // as this is accessed by multiple cores we need to use exclusive access
    // interrupts must be already disabled
    // resource has next pointer as first word
    // arguments
    // r0 - global list
    // r2 - list start
    // r3 - list end
    // r4 - scratch
    // lr - normal return

    ldrex r4, [r0, #LIST_OFF_FREE]
    str r4, [r3] // store end->next = freelist
    strex r4, r4, [r0, #LIST_OFF_FREE]
    cbnz r4, rg_try_again
    mov pc, lr

rg_try_again:
    cpsid aif
    cpsie aif
    b release_global
