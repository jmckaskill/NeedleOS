#pragma once
#include "config.h"
#include "../needle.h"
#include <stddef.h>
#include <stdint.h>


// no need to protect against false sharing on a single core
#ifdef SMP
#define CACHE_ALIGN __attribute__((aligned(CACHE_SIZE)))
#else
#define CACHE_ALIGN
#endif

struct task;

struct page {
    char data[4096];
};

struct desc {
    struct desc * volatile next;
    struct desc * volatile prev;
    struct task * volatile task;
    void *pad;
} CACHE_ALIGN;

struct kernel_pool {
    struct desc * volatile free CACHE_ALIGN;
    struct desc * volatile top CACHE_ALIGN;
    
    struct desc *begin;
    struct desc *end;
    struct page *pages;
};

struct kernel {
    struct kernel_pool kernel;
    struct kernel_pool user;
    struct kernel_pool shared;
};

struct core_pool {
    struct desc *first;
    struct desc *last;
    size_t count;
};

struct core {
    struct kernel *kernel;
    struct task *running;
    struct core_pool user;
    struct core_pool shared;
};

struct msg {
    int cmd;
    ndl_obj_t obj;
};

struct queue {
    struct msg *first, *last;
};

struct chan {
    struct task *receiver;
    uint32_t rx_mask;
    ndl_dispatch_fn fn;
    void *udata;
    struct queue high_priority;
    struct queue normal_priority;
    struct msg *free;
    struct msg *used;
    struct msg msgs[128];
};

struct task_pool {
    struct desc *first;
    size_t quota;
    size_t count;
};

struct registers {
    uintptr_t data[64];
};

struct task {
    struct task_pool user;
    struct task_pool shared;
    struct chan *rx[32];
    struct chan *tx[32];
    uint32_t rx_mask;
    uint32_t dispatch_mask;
    struct registers regs;
    char stack[1];
};

extern struct kernel KERNEL;
extern struct core CORE;
extern struct desc DESCRIPTORS;
extern struct page PAGES;

#define acquire_core_lock() __asm__ volatile ("cpsid aif")
#define release_core_lock() __asm__ volatile ("dsb st\t\ncpsie aif")

struct desc *global_alloc(struct kernel_pool *p);
struct page *alloc_page();
int release_page(void *pg);




