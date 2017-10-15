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

#define SYSNO r0

svc_exception:
    // r0 = syscall number
    // r1-r3 = syscall arguments
    // binary search with 3 way test end node
    // this is preferred to a jump table as it makes better use of the branch predictor
    // max is 10 -> 3*2*2 = 12
sys_0_up:
    cmp SYSNO, #6
    bhs sys_6_up
sys_0_5:
    cmp SYSNO, #3
    bhs sys_3_5
sys_0_2:
    cmp SYSNO, #1
    blo do_alloc_page // 0
    beq release_page // 1
    b send // 2
sys_3_5:
    cmp SYSNO, #4
    blo recv // 3
    beq current_tick // 4
    b create_channel // 5
sys_6_up:
    cmp SYSNO, #9
    bhs sys_9_up
sys_6_8:
    cmp SYSNO, #7
    blo close_channel // 6
    beq create_task // 7
    b start_task // 8
sys_9_up:
    cmp SYSNO, #10
    blo transfer // 9
    b dispatch // 10
    mov r0, #NDL_ENOSVC // 11+
    subs pc, lr, #0

do_alloc_page:
    mov r0, sp
    mov r12, #MODE_SYS
    msr CPSR_c, r12
    push {lr}
    bl alloc_page
    pop {lr}
    subs pc, lr, #0
