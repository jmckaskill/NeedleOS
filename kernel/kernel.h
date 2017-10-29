#pragma once
#include "config.h"
#include "../needle.h"
#include <stddef.h>
#include <stdint.h>


// no need to protect against false sharing on a single cpu
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

struct platform_cpu {
	void *align;
};

struct cpu {
    struct kernel *kernel;
    struct task *running;
    struct core_pool mem;
	struct task *ready;
	struct task *asleep;

	// last item is platform specific and takes
	// up the rest of the page
	struct platform_cpu platform;
};

struct msg {
	struct msg *next;
    int cmd;
    ndl_obj_t obj;
};

struct queue {
    struct msg *first, *last;
};

#define NUM_MSGS 128

struct chan {
	volatile long ref;
    struct task *receiver;
    uint32_t rx_mask;
    ndl_dispatch_fn fn;
    void *udata;
    struct queue high_priority;
    struct queue normal_priority;
    struct msg *free;
    struct msg *used;
    struct msg msgs[NUM_MSGS];
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
	struct cpu *cpu;
    struct task_pool mem;
	struct task *creating_task;
    struct chan *rx[32];
    struct chan *tx[32];
    uint32_t rx_mask;
	uint32_t ready_mask;
    uint32_t dispatch_mask;
	struct ndl_message *recv;
	
	// last item is OS/platform specific and takes
	// up the rest of the page
    struct os_task os;
};

struct desc *global_alloc(struct kernel_pool *p);
void global_release(struct kernel_pool *p, struct desc *first, struct desc *last);
void os_start_task(struct cpu *c, struct task *t, ndl_task_fn fn, void *udata);
void os_close_task(struct task *t);
void os_switch(struct cpu *c, struct task *t);
ndl_tick_t current_tick();

void release_desc(struct cpu *c, struct desc *d);
struct desc *verify_page(struct task *t, void *pg);
void transfer_page(struct task *from, struct task *to, struct desc *d);

struct page *alloc_page(struct task *t);
int release_page(struct task *t, void *pg);
int start_initial_task(struct cpu *c);
int create_task(struct task *t);
int start_task(struct task *t, ndl_task_fn fn, void *udata);
int create_channel(struct task *t, int chan, ndl_dispatch_fn fn, void *udata);
int send_msg(struct task *t, int chan, int cmd, ndl_obj_t obj);
int recv_msg(struct task *t, uint32_t mask, ndl_tick_t wakeup, struct ndl_message *r);
int transfer(struct task *t, ndl_obj_t obj);

void free_task(struct cpu *c, struct task *t);
void schedule_next(struct cpu *c);

extern void app_main(void*);


static inline void *desc_to_page(struct kernel_pool *p, struct desc *d) {
	size_t idx = d - p->begin;
	return p->pages + idx;
}

static inline struct desc *page_to_desc(struct kernel_pool *p, void *page) {
	struct page *pg = (struct page*) page;
	size_t idx = pg - p->pages;
	return p->begin + idx;
}

