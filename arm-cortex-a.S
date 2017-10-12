.syntax unified

.global ndl_svc_exception
.global ndl_irq_exception
.global _start
.global _irq_stack
.global _user_stack
.global _core_data
.global _global_page_list
.global _global_shared_list
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

#define WORD_SHIFT 2

#define CORE_OFF_TASK (0 << WORD_SHIFT)
#define CORE_OFF_FREE_PAGES (1 << WORD_SHIFT)
#define CORE_OFF_FREE_SHARED (2 << WORD_SHIFT)
#define CORE_OFF_PAGE_DATA (3 << WORD_SHIFT)
#define CORE_OFF_SHARED_DATA (4 << WORD_SHIFT)

#define TASK_OFF_PAGES (0 << WORD_SHIFT)
#define TASK_OFF_SHARED (1 << WORD_SHIFT)
#define TASK_OFF_PAGE_QUOTA (2 << WORD_SHIFT)
#define TASK_OFF_PAGE_COUNT (3 << WORD_SHIFT)

#define DESC_OFF_NEXT (0 << WORD_SHIFT)
#define DESC_OFF_PREV (1 << WORD_SHIFT)
#define DESC_OFF_TASK (2 << WORD_SHIFT)
#define DESC_SZ_SHIFT (WORD_SHIFT+2) // 16 bytes

#define PAGE_SZ_SHIFT 14 // 4 KB

#define NDL_ENOSVC -2
#define NDL_ENOQUOTA -3
#define NDL_ECONTENTION -4

.code 32

.section .init
_start:
    // setup stacks
    cpsid aif, #MODE_IRQ
    ldr sp, =_irq_stack

    cpsid aif, #MODE_SYS
    ldr sp, =_user_stack

    // set the exception base address
    ldr r0, =exception_table
    mcr p15, 0, r0, c12, c0, 0

    // take exceptions in thumb mode
    mrc p15, 0, r0, c1, c0, 0 // Read SCTLR
    orr r0, r0, #SCTLR_THUMB_EXCEPTIONS
    mcr p15, 0, r0, c1, c0, 0 // Write SCTLR

    // call main as thumb
    ldr r0, =main
    orr r0, r0, #BRANCH_TO_THUMB
    bx r0

.code 16

.align 5 // 32 bytes
exception_table:
reset:
    b.w reset
undefined_instruction:
    b.w undefined_instruction
software_interrupt:
    b.w svc_exception
prefetch_abort:
    b.w prefetch_abort
data_abort:
    b.w data_abort
reserved:
    b.w reserved
irq:
    b.w ndl_irq_exception
fiq:
    b.w fiq

svc_exception:
    // r0 = syscall number
    // r1-r4 = syscall arguments
    cpsie aif
    // binary search
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
    mov r0, #NDL_ENOSVC
svc_ret:
    movs pc, lr

alloc_page:
    // r1 - core data
    // r2 - task
    // r0 - returned page
    // r3/r4 - scratch
    // check the quota
    ldr r1, =_core_data
    ldr r2, [r1, #CORE_OFF_TASK]
    ldr r3, [r2, #TASK_OFF_PAGE_QUOTA]
    ldr r4, [r2, #TASK_OFF_PAGE_COUNT]
    mov r0, #NDL_ENOQUOTA
    cmp r3, r4
    bhs svc_ret // count >= quota

    // increment the page count
    add r4, r4, #1
    str r4, [r2, #TASK_OFF_PAGE_COUNT]

    // mask interrupts so we can reliably move a page
    // from the freelist to the task list
    cpsid aif

    // see if we have a page in the core freelist (unlocked)
    ldr r0, [r1, #CORE_OFF_FREE_PAGES]
    cbz r0, alloc_global_page

    // update core freelist to the desc->next pointer
    ldr r3, [r0, #DESC_OFF_NEXT]
    str r3, [r1, #CORE_OFF_FREE_PAGES]

add_page_to_task:
    // page->next = task freelist
    // page->prev = 0
    ldr r3, [r2, #TASK_OFF_PAGES]
    mov r4, #0
    str r3, [r0, #DESC_OFF_NEXT]
    str r4, [r1, #DESC_OFF_PREV]

    // task freelist -> prev = page
    cbz r3, set_task_pages_head
    str r0, [r3, #DESC_OFF_PREV]

set_task_pages_head:
    // set task list head
    str r0, [r2, #TASK_OFF_PAGES]

    // we've added the page into the task list so can reenable interrupts
    cpsie aif

    // return the new page data
alloc_page_return:
    cpsie aif
    ldr r3, [r2, #CORE_OFF_PAGE_DATA]
    lsl r0, #PAGE_SZ_SHIFT
    add r0, r3

    movs pc, lr

alloc_global_page:
    // see if we have a page in the global list
    // as this is accessed by multiple cores we need to use exclusive access
    // r0 - returned page/desc
    // r1 - global page list
    // r2 - task data
    // r3 - loops left
    // r4 - scratch
    ldr r1, =_global_page_list
    mov r3, #10
alloc_global_page_again:
    ldrex r0, [r1]
    ldr r4, [r0, #DESC_OFF_NEXT]
    strex r4, r4, [r1]
    cbnz r4, alloc_global_page_fail
    b add_page_to_task

alloc_global_page_fail:
    subs r3, r3, #1
    bgt alloc_global_page_again
    mov r0, #NDL_ECONTENTION
    movs pc, lr


release_page:
send:
recv:
current_tick:
create_channel:
close_channel:
set_dispatch_fn:
create_task:
cancel_task:
start_task:
set_task_priority:
transfer_channel:
transfer_shared:
dispatch:
alloc_shared:
release_shared: