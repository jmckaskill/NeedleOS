#include "common.S"

.syntax unified
.code 16


kern_free_page:
    // arguments
    // r0 - page to release
    // return
    // r0 - returned error code
    // registers
    // r1 - core data
    // r2 - task
    // r3/r4/r12 - scratch

    // check that the input pointer points to a whole page
    mov r3, #(PGSZ-1)
    and r3, r0, r3
    cbnz r3, invalid_value

    // map the page to the descriptor
    ldr r3, =USER_HEAP
    ldr r4, =USER_DESC
    sub r0, r0, r3
    lsr r0, #(PAGE_SHIFT - DESC_SHIFT)
    add r0, r0, r4
    // r0 now contains the descriptor

    // check that it's a valid page in this section
    ldr r4, =USER_DESC_END
    cmp r0, r4
    bhs invalid_value

    // check that this task owns the page
    ldr r1, =CORE_DATA
    ldr r2, [r1, #CORE_OFF_RUNNING_TASK]
    ldr r3, [r0, #DESC_OFF_TASK]
    cmp r3, r2
    bne invalid_value
    
    // kern_move the page from the task list to the core list
    // disable interrupts as an interrupt may try and kill
    // the interrupt
    cpsie aif

    // remove the page from the task list

    // if (task->pages == desc) task->pages = desc->next
    // r0 - desc
    // r2 - task
    // r3 - desc->next
    // r4 - task->free
    ldr r3, [r0, #DESC_OFF_NEXT]
    ldr r4, [r2, #TASK_OFF_PAGES]
    cmp r0, r4
    bne clear_next_pointer
    str r3, [r2, #TASK_OFF_PAGES]

clear_next_pointer:
    // if (desc->next) desc->next->prev = desc->prev
    // r0 - desc
    // r3 - desc->next
    // r4 - desc->prev
    ldr r4, [r0, #DESC_OFF_PREV]
    cbz r3, clear_prev_pointer
    str r4, [r3, #DESC_OFF_PREV]    

clear_prev_pointer:
    cbz r4, clear_task_pointer
    str r3, [r4, #DESC_OFF_NEXT]

clear_task_pointer:
    // this task no longer owns it
    // desc->task = 0
    mov r4, #0
    ldr r4, [r0, #DESC_OFF_TASK]

    // add the task to the core freelist
    // desc->next = core->free_start
    // core->free_start = desc
    // r0 - desc
    // r1 - core
    // r2 - core->free_start
    // r3 - core->free_end
    // r4 - core->free_count
    ldr r2, [r1, #CORE_OFF_USER_FIRST]
    str r2, [r0, #DESC_OFF_NEXT]
    str r0, [r1, #CORE_OFF_USER_FIRST]

    // if (core->free_start == 0) core->free_end = desc
    cbnz r2, rel_inc_page_count
    str r0, [r1, #CORE_OFF_USER_LAST]

rel_inc_page_count:
    // core->free_count++
    ldr r4, [r1, #CORE_OFF_USER_COUNT]
    add r4, r4, #1

    // if (core->free_count > NUM_CORE_FREE_USER) release to global pool
    cmp r4, #NUM_CORE_FREE_USER
    blo rel_set_page_count

    ldr r0, =(GLOBAL_DATA + GLOBAL_LIST_USER)
    // r2 - list start (already)
    // r3 - list end (already)
    mov ip, lr
    bl release_global    
    mov lr, ip

    // clear the core memory list
    mov r4, #0
    ldr r4, [r1, #CORE_OFF_USER_FIRST]
    str r4, [r1, #CORE_OFF_USER_LAST]

rel_set_page_count:
    str r4, [r1, #CORE_OFF_USER_COUNT]

    // now return
    cpsid aif
    mov r0, #0
    subs pc, lr, #0

invalid_value:
    mov r0, #NDL_EINVAL
    subs pc, lr, #0
