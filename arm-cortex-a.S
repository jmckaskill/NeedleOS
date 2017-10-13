#include "config.h"

.syntax unified

.global _start
.global init_board
.global main

#define MODE_USER 16
#define MODE_IRQ 18
#define MODE_SVC 19
#define MODE_SYS 31
#define FIQ_MASKED 64
#define IRQ_MASKED 128
#define ALL_MASKED (FIQ_MASKED|IRQ_MASKED)
#define MODE_THUMB (1 << 5)
#define SCTLR_THUMB_EXCEPTIONS (1 << 30)
#define BRANCH_TO_THUMB 1

#define PTRSZ 4
#define PTR_SHIFT 2

#define LIST_OFF_HEAD 0
#define LIST_OFF_NEXT CACHE_SIZE
#define LIST_OFF_END (LIST_OFF_NEXT+PTRSZ)
#define LIST_SIZE (CACHE_SIZE*2)

#define GLOBAL_LIST_KERNEL (0)
#define GLOBAL_LIST_USER (1*LIST_SIZE)
#define GLOBAL_LIST_SHARED (2*LIST_SIZE)

#define TASK_OFF_PAGES (0*PTRSZ)
#define TASK_OFF_SHARED (1*PTRSZ)
#define TASK_OFF_PAGE_QUOTA (2*PTRSZ)
#define TASK_OFF_PAGE_COUNT (3*PTRSZ)

#define DESC_OFF_NEXT (0*PTRSZ)
#define DESC_OFF_PREV (1*PTRSZ)
#define DESC_OFF_TASK (2*PTRSZ)
#define DESC_SHIFT CACHE_SHIFT
#define DESC_SIZE CACHE_SIZE

#define CORE_OFF_RUNNING_TASK (0*PTRSZ)
#define CORE_OFF_FREE_PAGES (1*PTRSZ)
#define CORE_OFF_FREE_SHARED (2*PTRSZ)

#define PGSZ 4096
#define SEGMENT (16*1024*1024)
#define PAGE_SHIFT 14 // 4 KB

#define TASK_SIZE PGSZ

#define NDL_EINVAL -1
#define NDL_ENOSVC -2
#define NDL_ENOMEM -3

#define ALIGN(SZ, X) (((X) + (SZ) - 1) &~ (SZ))

#define GLOBAL_DATA FAST_MEM
#define CORE_DATA (GLOBAL_DATA + PGSZ)

#define KERNEL_DESC MAIN_MEM
#define USER_DESC (KERNEL_DESC + (NUM_KERNEL*DESC_SIZE))
#define SHARED_DESC (USER_DESC + (NUM_USER*DESC_SIZE))
#define DESC_END (SHARED_DESC + (NUM_SHARED*DESC_SIZE))

#define KERNEL_HEAP ALIGN(PGSZ, SHARED_DESC)
#define USER_HEAP (KERNEL_HEAP + (NUM_KERNEL*PGSZ))
#define SHARED_HEAP (USER_HEAP + (NUM_USER*PGSZ))
#define HEAP_END (SHARED_HEAP + (NUM_SHARED*PGSZ))

#define IRQ_STACK_TOP STACK_TOP
#define MAIN_STACK_TOP (STACK_TOP - PGSZ)

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

    str r0, [r1, #(GLOBAL_LIST_KERNEL + LIST_OFF_HEAD)]
    str r0, [r1, #(GLOBAL_LIST_USER + LIST_OFF_HEAD)]
    str r0, [r1, #(GLOBAL_LIST_SHARED + LIST_OFF_HEAD)]

    str r2, [r1, #(GLOBAL_LIST_KERNEL + LIST_OFF_NEXT)]
    str r3, [r1, #(GLOBAL_LIST_KERNEL + LIST_OFF_END)]
    str r3, [r1, #(GLOBAL_LIST_USER + LIST_OFF_NEXT)]
    str r4, [r1, #(GLOBAL_LIST_USER + LIST_OFF_END)]
    str r4, [r1, #(GLOBAL_LIST_SHARED + LIST_OFF_NEXT)]
    str r5, [r1, #(GLOBAL_LIST_SHARED + LIST_OFF_END)]

    // clear the page descriptors task pointers
clear_next_descriptor:
    str r0, [r2, #DESC_OFF_TASK]
    add r2, #DESC_SIZE
    cmp r2, r5
    blt clear_next_descriptor

    bl init_board
    b main
loop:
    b loop

.align 5 // 32 bytes
.code 16
exception_table:
reset:
    b.w reset
undefined_instruction:
    b.w undefined_instruction
software_interrupt:
    cpsie.n aif
    b.n svc_exception
prefetch_abort:
    b.w prefetch_abort
data_abort:
    b.w data_abort
reserved:
    b.w reserved
irq:
    b.w irq
fiq:
    b.w fiq

svc_exception:
    // r0 = syscall number
    // r1-r4 = syscall arguments
    // binary search
    // this is preferred to a jump table as it makes better use of the branch predictor
    // end node is a 3 way test
    // max is 16 -> 3 * 8 covers this
sys_0_up:
    cmp r0, #12
    bhs sys_12_up
sys_0_11:
    cmp r0, #6
    bhs sys_6_11
sys_0_5:
    cmp r0, #3
    bhs sys_3_5
sys_0_2:
    cmp r0, #1
    blo alloc_page // 0
    beq release_page // 1
    b send // 2
sys_3_5:
    cmp r0, #4
    blo recv // 3
    beq current_tick // 4
    b create_channel // 5
sys_6_11:
    cmp r0, #9
    bhs sys_9_11
sys_6_8:
    cmp r0, #7
    blo close_channel // 6
    beq set_dispatch_fn // 7
    b create_task // 8
sys_9_11:
    cmp r0, #10
    blo cancel_task // 9
    beq start_task // 10
    b set_task_priority // 11
sys_12_up:
    cmp r0, #15
    bhi sys_16_up
sys_12_15:
    cmp r0, #13
    blo transfer_channel // 12
    beq transfer_shared // 13
    b dispatch // 14 
sys_16_up:
    cmp r0, #16
    blo alloc_shared // 15
    beq release_shared // 16

no_service:
    mov r0, #NDL_ENOSVC
    subs pc, lr, #0

get_global_resource:
    // function to obtain a resource from a global free list
    // as this is accessed by multiple cores we need to use exclusive access
    // arguments
    // r1 - global free list
    // r2 - resource size
    // r3 - scratch
    // returns
    // r0 - found pointer
    // the function will jump to no_mem on error

    ldrex r0, [r1, #LIST_OFF_HEAD]
    cbz r0, get_new_resource
    ldr r3, [r1, #LIST_OFF_HEAD]
    strex r3, r3, [r1, #LIST_OFF_HEAD]
    cbnz r3, get_new_resource

    // got the resource successfully from the global free list
    mov pc, lr

get_new_resource:
    ldr r3, [r1, #LIST_OFF_END]
    ldrex r0, [r1, #LIST_OFF_NEXT]
    cmp r0, r3
    bhs no_mem

    // store the new top back
    add r3, r0, r2
    strex r3, r3, [r1, #LIST_OFF_NEXT]
    cbnz r3, get_resource_again

    // got the resource successfully from the new list
    mov pc, lr

get_resource_again:
    cpsid aif
    cpsie aif
    b get_global_resource

no_mem:
    mov r0, #NDL_ENOMEM
    subs pc, lr, #0



alloc_page:
    // no arguments
    // r0 - returned page
    // r1 - core data
    // r2 - task
    // r3/r4 - scratch
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
    ldr r0, [r1, #CORE_OFF_FREE_PAGES]
    cbz r0, alloc_global_page

    // update core freelist to the desc->next pointer
    ldr r3, [r0, #DESC_OFF_NEXT]
    str r3, [r1, #CORE_OFF_FREE_PAGES]


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
    ldr r1, =PAGE_DESC
    ldr r2, =PAGE_HEAP
    sub r0, r0, r1
    lsl r0, #(PAGE_SHIFT-DESC_SHIFT)
    add r0, r0, r2

    subs pc, lr, #0



// Option 2/3 - grab a free page from the global page list
alloc_global_page:
    ldr r1, =(GLOBAL_DATA + GLOBAL_OFF_PAGES)
    mov r2, #PGSZ
    mov r4, lr
    bl get_global_resource

    mov lr, r4
    ldr r1, =CORE_DATA
    ldr r2, [r1, #CORE_OFF_RUNNING_TASK]



release_page:
    // r0 - page to release
    // on return
    // r0 - returned error code
    // r1 - core data
    // r2 - task
    // r3/r4 - scratch

    // check that the input pointer points to a whole page
    mov r3, #(PGSZ-1)
    and r3, r0, r3
    cbnz r3, invalid_value

    // map the page to the descriptor
    ldr r3, =PAGE_HEAP
    ldr r4, =PAGE_DESC
    sub r0, r0, r3
    lsr r0, #(PAGE_SHIFT - DESC_SHIFT)
    add r0, r0, r4
    // r0 now contains the descriptor

    // check that it's a valid page in this section
    ldr r4, =PAGE_DESC_END
    cmp r0, r4
    bhs invalid_value

    // check that this task owns the page
    ldr r1, =CORE_DATA
    ldr r2, [r1, #CORE_OFF_RUNNING_TASK]
    ldr r3, [r0, #DESC_OFF_TASK]
    cmp r3, r2
    bne invalid_value
    
    // transfer the page from the task list to the core list
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
    // desc->next = core->free
    // r1 - core
    // r3 - core->free
    ldr r3, [r1, #CORE_OFF_FREE_PAGES]
    str r3, [r0, #DESC_OFF_NEXT]

    // core->free = desc
    str r0, [r1, #CORE_OFF_FREE_PAGES]

    // now return
    cpsid aif
    mov r0, #0
    subs pc, lr, #0

invalid_value:
    mov r0, #NDL_EINVAL
    subs pc, lr, #0

send:
recv:

current_tick:
    ldr r1, =TICK_COUNT_ADDR
    ldr r0, [r1]
    subs pc, lr, #0

create_task:

create_channel:
close_channel:
set_dispatch_fn:
cancel_task:
start_task:
set_task_priority:
transfer_channel:
transfer_shared:
dispatch:
alloc_shared:
release_shared:


