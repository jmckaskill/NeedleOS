#pragma once
#include "config.h"
#include "../needle.h"
#include <stddef.h>
#include <stdint.h>


// no need to protect against false sharing on a single core
#ifndef SMP
#define CACHE_ALIGN
typedef struct desc * volatile aligned_desc_ptr;
#elif defined _MSC_VER
typedef __declspec(align(CACHE_SIZE)) struct desc * volatile aligned_desc_ptr;
#else
typedef struct desc * volatile aligned_desc_ptr __attribute__((aligned(CACHE_SIZE)));
#endif

struct task;

struct page {
    char data[4096];
};

struct desc {
    aligned_desc_ptr next;
    struct desc * volatile prev;
    struct task * volatile task;
    void *pad;
};

struct kernel_pool {
    aligned_desc_ptr free;
    aligned_desc_ptr top;
    
    struct desc *begin;
    struct desc *end;
    struct page *pages;
};

struct kernel {
    struct kernel_pool kernel_mem;
    struct kernel_pool user_mem;
};

struct core_pool {
    struct desc *first;
    struct desc *last;
    size_t count;
};

struct os_core {
	void *align;
};

struct core {
    struct kernel *kernel;
    struct task *running;
    struct core_pool mem;
	struct task *tasks;

	// last item is OS/platform specific and takes
	// up the rest of the page
	struct os_core os;
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

static_assert(sizeof(struct chan) < PGSZ, "overlarge channel");

struct task_pool {
    struct desc *first;
    size_t quota;
    size_t count;
};

struct os_task {
	// used for:
	// native - register set + user stack
	// hosted - OS thread handle
	void *align;
};

struct task {
	struct task *next, *prev;
    struct task_pool mem;
	struct task *creating_task;
    struct chan *rx[32];
    struct chan *tx[32];
    uint32_t rx_mask;
    uint32_t dispatch_mask;
	
	// last item is OS/platform specific and takes
	// up the rest of the page
    struct os_task os;
};

extern struct kernel KERNEL;
extern struct core CORE;
extern struct desc DESCRIPTORS;
extern struct page PAGES;

#ifdef __arm__
#define acquire_core_lock(c) __asm__ volatile ("cpsid aif")
#define release_core_lock(c) __asm__ volatile ("dsb st\t\ncpsie aif")
#else
extern void acquire_core_lock(struct core *c);
extern void release_core_lock(struct core *c);
#endif

struct desc *global_alloc(struct kernel_pool *p);
void global_release(struct kernel_pool *p, struct desc *first, struct desc *last);
void os_start_task(struct core *c, struct task *t, ndl_task_fn fn, void *udata);
void os_close_task(struct task *t);
void os_pause_task(struct task *t);
void os_resume_task(struct task *t);

struct page *alloc_page(struct core *c);
void do_release_page(struct core *c, void *pg);
int release_page(struct core *c, void *pg);
int create_task(struct core *c);
int start_task(struct core *c, ndl_task_fn fn, void *udata);
void free_task(struct core *c, struct task *t);
void schedule_next(struct core *c);

extern void app_main();


static inline void *desc_to_page(struct kernel_pool *p, struct desc *d) {
	size_t idx = d - p->begin;
	return p->pages + idx;
}

static inline struct desc *page_to_desc(struct kernel_pool *p, void *page) {
	struct page *pg = (struct page*) page;
	size_t idx = pg - p->pages;
	return p->begin + idx;
}

