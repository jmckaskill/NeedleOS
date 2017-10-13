#include "common.S"

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
    // r12 = scratch
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
