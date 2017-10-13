#include "common.S"

.syntax unified
.code 16

alloc_global:
    // function to obtain a resource from a global free list
    // as this is accessed by multiple cores we need to use exclusive access
    // interrupts must be already disabled
    // resource has next pointer as first word
    // arguments
    // r1 - global list
    // r2 - resource size
    // r3/r4 - scratch
    // ip - exception return
    // lr - normal return
    // returns
    // r0 - found pointer
    // the function will exception return NDL_ENOMEM on error

    ldrex r0, [r1, #LIST_OFF_FREE]
    cbz r0, ag_new_resource
    ldr r3, [r0] // load res->next
    strex r3, r3, [r1, #LIST_OFF_FREE]
    cbnz r3, ag_new_resource

    // got the resource successfully from the global free list
    mov pc, lr

ag_new_resource:
    ldr r3, [r1, #LIST_OFF_END]
    ldrex r0, [r1, #LIST_OFF_TOP]
    cmp r0, r3
    bhs ag_no_mem

    // store the new top back
    add r3, r0, r2
    strex r3, r3, [r1, #LIST_OFF_TOP]
    cbnz r3, ag_again

    // got the resource successfully from the new list
    mov pc, lr

ag_again:
    cpsid aif
    cpsie aif
    b alloc_global

ag_no_mem:
    mov r0, #NDL_ENOMEM
    mov lr, ip
    subs pc, lr, #0

