#pragma once

#include "../needle.h"
#include "config.h"
#include <assert.h>

struct page_desc {
    struct page_desc *next, *prev;
    int task;
} NDL_ALIGNED(CACHE_LINE_SIZE);

struct scheduler {
};

struct message {
    int cmd;
    void *buf;
};

struct channel {

};

static_assert(sizeof(struct channel) < PGSZ, "channel size");

struct kernel_task {
    int id;
    struct ndl_task *user;
};

struct core {
    // unlocked data - can only be accessed from this core
    struct scheduler scheduler;
    struct page_desc *free_pages;
    struct channel *interprocess;
    struct kernel_task *current_task;
} NDL_ALIGNED(CACHE_LINE_SIZE);

struct mempool {
    struct page_desc *desc;
    char* volatile next;
    char* top;
} NDL_ALIGNED(CACHE_LINE_SIZE);

struct kernel {
    struct core cores[NUM_CORES];
    struct mempool mem_shared;
    struct mempool mem_kernel;
    struct mempool mem_user;
};

extern struct kernel ndl_kernel;

uintptr_t ndl_do_syscall(int num, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4);
struct core *ndl_current_core();

