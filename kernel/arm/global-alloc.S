#include "../config.h"

#define DESC_OFF_NEXT 0
#define DESC_SIZE CACHE_SIZE
#define POOL_OFF_FREE 0
#define POOL_OFF_BEGIN CACHE_SIZE
#define POOL_OFF_END (CACHE_SIZE+PTRSZ)

#define RESULT r0
#define LOOP_COUNT r3
#define POOL r1
#define END r2
#define NEXT r2
#define TEMP r2

.syntax unified
.global kern_alloc
.thumb
.thumb_func

kern_alloc:
    // function to obtain a resource from a global free list
    // as this is accessed by multiple cores we need to use exclusive access
    // interrupts must be already disabled
    // arguments
    // r0 - pool
    // r1/r2/r3 - scratch
    // lr - normal return
    // returns
    // r0 - found pointer
    // the function will exception return NDL_ENOMEM on error

    mov LOOP_COUNT, #10
    mov POOL, r0

ag_start:
    ldrex RESULT, [POOL, #POOL_OFF_FREE]
    cbz RESULT, ag_new_resource
    ldr NEXT, [RESULT, #DESC_OFF_NEXT]
    strex TEMP, NEXT, [POOL, #POOL_OFF_FREE]
    cbnz TEMP, ag_new_resource

    // got the resource successfully from the global free list
    mov pc, lr

ag_new_resource:
    ldr END, [POOL, #POOL_OFF_END]
    ldrex RESULT, [POOL, #POOL_OFF_BEGIN]
    cmp RESULT, END
    bhs ag_no_mem

    // store the new top back
    add NEXT, RESULT, #DESC_SIZE
    strex TEMP, NEXT, [POOL, #POOL_OFF_BEGIN]
    cbnz TEMP, ag_again

    // got the resource successfully from the new list
    mov pc, lr

ag_again:
    cpsid aif
    cpsie aif
    subs LOOP_COUNT, LOOP_COUNT, #1
    blo ag_no_mem
    b ag_start

ag_no_mem:
    mov r0, #0
    mov pc, lr


