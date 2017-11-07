#pragma once
#include "config.h"
#include "../app/needle.h"
#include <stddef.h>
#include <stdint.h>


// no need to protect against false sharing on a single cpu
#ifndef SMP
#define CACHE_ALIGN
typedef struct kern_desc * volatile kern_desc_aligned_ptr;
#elif defined _MSC_VER
typedef __declspec(align(CACHE_SIZE)) struct kern_desc * volatile kern_desc_aligned_ptr;
#else
typedef struct kern_desc * volatile kern_desc_aligned_ptr __attribute__((aligned(CACHE_SIZE)));
#endif

struct kern_task;

struct page {
    char data[4096];
};

struct kern_desc {
    kern_desc_aligned_ptr next;
    struct kern_desc * volatile prev;
    struct kern_task * volatile task;
    void *pad;
};

struct kern_mem {
    kern_desc_aligned_ptr free;
    kern_desc_aligned_ptr top;
    
    struct kern_desc *begin;
    struct kern_desc *end;
    struct page *pages;
};

struct platform_cpu {
	void *align;
};

struct kern_cpu {
    struct kern_mem *kernel_mem;
    struct kern_mem *user_mem;

	struct kern_desc *free_first;
	struct kern_desc *free_last;
	size_t free_count;

    struct kern_task *running;
	struct kern_task *ready;
	struct kern_task *asleep;

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
    struct kern_task *receiver;
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

struct task_memory {
    struct kern_desc *first;
    size_t quota;
    size_t count;
};

struct os_task {
	// used for:
	// native - register set + user stack
	// hosted - OS thread handle
	void *align;
};

struct kern_task {
	struct kern_task *next, *prev;
	struct kern_cpu *cpu;
    struct task_memory mem;
	struct kern_task *creating_task;
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

struct stacks {
    void *user;
    void *svc;
};

struct stacks init_kernel();
struct kern_task *init_task(struct kern_cpu *c);
int start_initial_task(struct kern_cpu *c);

void kern_lock(struct kern_cpu *c);
void kern_unlock(struct kern_cpu *c);
struct kern_task *kern_current_task();
struct kern_desc *kern_alloc(struct kern_mem *p);
void kern_free(struct kern_mem *p, struct kern_desc *first, struct kern_desc *last);

void os_start_task(struct kern_cpu *c, struct kern_task *t, ndl_task_fn fn, void *udata);
void os_close_task(struct kern_task *t);
void os_switch(struct kern_cpu *c, struct kern_task *t);
ndl_tick_t kern_tick();

void release_desc(struct kern_cpu *c, struct kern_desc *d);
struct kern_desc *verify_page(struct kern_task *t, void *pg);
void transfer_page(struct kern_task *from, struct kern_task *to, struct kern_desc *d);

struct page *kern_alloc_page(struct kern_task *t);
int kern_free_page(struct kern_task *t, void *pg);
int kern_fork(struct kern_task *t);
int kern_start(struct kern_task *t, ndl_task_fn fn, void *udata);
int kern_pipe(struct kern_task *t, int chan, ndl_dispatch_fn fn, void *udata);
int kern_close(struct kern_task *t, int chan);
int kern_send(struct kern_task *t, int chan, int cmd, ndl_obj_t obj);
int kern_recv(struct kern_task *t, uint32_t mask, ndl_tick_t wakeup, struct ndl_message *r);
int kern_move(struct kern_task *t, ndl_obj_t obj);
int kern_dispatch(struct kern_task *t);

void free_task(struct kern_cpu *c, struct kern_task *t);
void schedule_next(struct kern_cpu *c);

extern void app_main(void*);


static inline void *desc_to_page(struct kern_mem *p, struct kern_desc *d) {
	size_t idx = d - p->begin;
	return p->pages + idx;
}

static inline struct kern_desc *page_to_desc(struct kern_mem *p, void *page) {
	struct page *pg = (struct page*) page;
	size_t idx = pg - p->pages;
	return p->begin + idx;
}

