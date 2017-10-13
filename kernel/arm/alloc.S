#include "common.S"

.syntax unified
.code 16

alloc_page:
    // no arguments
    // return
    // r0 - returned page
    // registers
    // r1 - core data
    // r2 - task
    // r3/r4/r12 - scratch

    // check the quota
    ldr r1, =CORE_DATA
    ldr r2, [r1, #CORE_OFF_RUNNING_TASK]
    ldr r3, [r2, #TASK_OFF_PAGE_QUOTA]
    ldr r4, [r2, #TASK_OFF_PAGE_COUNT]
    cmp r3, r4
    bhs no_mem // count >= quota

    // increment the page count
    add r4, r4, #1
    str r4, [r2, #TASK_OFF_PAGE_COUNT]

    // mask interrupts so we can reliably move a page
    // from the freelist to the task list
    // this is to avoid reentrancy issues with the irq handler
    cpsid aif

    // see if we have a page in the core freelist
    // r0 - newly grabbed desc
    // r1 - core
    // r2 - task
    // r3/r4 - scratch
    ldr r0, [r1, #CORE_OFF_USER_FIRST]
    cbz r0, alloc_global_page

    // update core free start to the desc->next pointer
    ldr r3, [r0, #DESC_OFF_NEXT]
    str r3, [r1, #CORE_OFF_USER_FIRST]

    // check to see if we need to zero core free end
    cbnz r3, all_set_free_count
    str r3, [r1, #CORE_OFF_USER_LAST]

all_set_free_count:
    // update core free count
    ldr r3, [r1, #CORE_OFF_USER_COUNT]
    sub r3, r3, #1
    str r3, [r1, #CORE_OFF_USER_COUNT]

// Return code
// We have the new page descriptor in r0
// Interrupts are still masked
add_page_to_task:
    // page->next = task freelist
    ldr r3, [r2, #TASK_OFF_PAGES]
    str r3, [r0, #DESC_OFF_NEXT]

    // page->prev = 0
    mov r4, #0
    str r4, [r1, #DESC_OFF_PREV]

    // task freelist -> prev = page
    cbz r3, set_task_pages_head
    str r0, [r3, #DESC_OFF_PREV]

set_task_pages_head:
    // set task list head
    str r0, [r2, #TASK_OFF_PAGES]

    // we've added the page into the task list so can reenable interrupts
    cpsie aif

    // Mark this page as ours. That way we can check it on release
    str r2, [r0, #DESC_OFF_TASK]

    // return the new page data
    ldr r1, =USER_DESC
    ldr r2, =USER_HEAP
    sub r0, r0, r1
    lsl r0, #(PAGE_SHIFT-DESC_SHIFT)
    add r0, r0, r2

    subs pc, lr, #0

// Option 2/3 - grab a free page from the global page list
alloc_global_page:
    ldr r1, =(GLOBAL_DATA + GLOBAL_LIST_USER)
    mov r2, #PGSZ
    mov ip, lr
    bl alloc_global
    mov lr, ip

    ldr r1, =CORE_DATA
    ldr r2, [r1, #CORE_OFF_RUNNING_TASK]
    b add_page_to_task

no_mem:
    mov r0, #NDL_ENOMEM
    subs pc, lr, #0
