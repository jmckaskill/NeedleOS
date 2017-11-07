
.syntax unified
.code 32

.global init_memory
.global init_kernel
.global shutdown
.global main

.section .init
_start:
    // initial mode is SYS with interrupts masked
    cpsid aif, #MODE_IRQ
    ldr sp, =(INIT_MEM_START + INIT_MEM_SIZE)
    blx init_memory

    // set the exception base address
    ldr r0, =exception_table
    mcr p15, 0, r0, c12, c0, 0

    // take exceptions in thumb mode
    mrc p15, 0, r0, c1, c0, 0 // Read SCTLR
    orr r0, r0, #SCTLR_THUMB_EXCEPTIONS
    mcr p15, 0, r0, c1, c0, 0 // Write SCTLR

    // once memory is setup we can create the initial task
    blx init_kernel
    // init_kernel returns the initial user stack in r0
    // and the initial svc stack in r1
    cspid aif, #MODE_SVC
    mov sp, r1

    cpsid aif, #MODE_SYS
    mov sp, r0
    blx main

    bx shutdown

