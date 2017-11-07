#include "common.S"

.global svc_alloc_page
.global svc_release_page
.global svc_send
.global svc_recv
.global svc_create_channel
.global svc_close_channel
.global svc_create_task
.global svc_start_task
.global svc_transfer
.global svc_dispatch

.align 5 // 32 bytes
.code 16

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
    b.w irq_exception
fiq:
    b.w fiq

irq_exception:
    push {r0-r12}
    pop {r0-r12}
    subs pc, lr, #4

#define SYSNO r7

svc_exception:
    // arguments
    // r0 = syscall number
    // r1-r3 = syscall arguments
    // return - r0
    // for calling into C code
    // r0 = task structure
    // r1-r3 = syscall arguments
    // return - r0
    push {SYSNO}
    mov SYSNO, r0
    mov r0, sp
sys_0_up:
    cmp SYSNO, #6
    bhs sys_6_up
sys_0_5:
    cmp SYSNO, #3
    bhs sys_3_5
sys_0_2:
    cmp SYSNO, #1
    beq sys_1
    bhs sys_2
sys_0:
    bl svc_alloc_page
    b sys_ret
sys_1:
    bl svc_release_page
    b sys_ret
sys_2:
    bl svc_send
    b sys_ret
sys_3_5:
    cmp SYSNO, #4
    beq sys_4
    bhs sys_5
sys_3:
    bl svc_recv
    b sys_ret
sys_4:
    ldr r0, =TICK_COUNT_ADDR
    ldr r0, [r0]
    b sys_ret
sys_5:
    bl svc_create_channel
    b sys_ret
sys_6_up:
    cmp SYSNO, #9
    bhs sys_9_up
sys_6_8:
    cmp SYSNO, #7
    beq sys_7
    bhs sys_8
sys_6:
    bl svc_close_channel
    b sys_ret
sys_7:
    bl svc_create_task
    b sys_ret
sys_8:
    bl svc_start_task
    b sys_ret
sys_9_up:
    cmp SYSNO, #10
    beq sys_10
    bhs sys_11_up
sys_9:
    bl svc_transfer
    b sys_ret
sys_10:
    bl svc_dispatch
    b sys_ret
sys_11_up:
    mov r0, #NDL_ENOSVC // 11+
sys_ret:
    pop {SYSNO}
    subs pc, lr, #0


