#pragma once
#include "config.h"
#include <stddef.h>
#include <stdint.h>

struct task;

struct page {
    char data[4096];
};

struct desc {
    struct desc * volatile next;
    struct desc * volatile prev;
    struct task * volatile task;
};

struct kernel_pool {
    union {
        struct desc * volatile p;
        char pad[CACHE_SIZE];
    } free;

    union {
        struct desc * volatile p;
        char pad[CACHE_SIZE];
    } top;

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
    struct task *running;
    struct core_pool user;
    struct core_pool shared;
};

struct task_pool {
    struct desc *first;
    size_t quota;
    size_t count;
};

struct task {
    struct task_pool user;
    struct task_pool shared;
};

extern struct kernel KERNEL;
extern struct core CORE;
extern struct desc DESCRIPTORS;
extern struct page PAGES;

#define NDL_ENOMEM -2



