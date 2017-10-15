#pragma once

//#define SMP
#define NUM_CORES 1
#define CACHE_SIZE 64
#define CACHE_SHIFT 4
#define PTRSZ 4

#define NUM_KERNEL 1024
#define NUM_USER 4096
#define NUM_SHARED 4096

#define NUM_CORE_FREE_USER 32

#define FAST_MEM   0x40300000U
#define STACK_TOP  0x40310000U
#define MAIN_MEM   0x80000000U

#define TICK_COUNT_ADDR 0x44E0503CU

