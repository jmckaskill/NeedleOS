#include "kernel.h"

struct page *alloc_page(struct core *c) {
    struct kernel *k = c->kernel;
    struct task *t = c->running;
	
	if (t->creating_task) {
		t = t->creating_task;
	}

    if (t->mem.count >= t->mem.quota) {
        return NULL;
    }

    acquire_core_lock(c);

    struct desc *d = c->mem.first;
    if (d) {
        struct desc *next = d->next;
        c->mem.first = d->next;
        if (!next) {
            c->mem.last = NULL;
        }
        c->mem.count--;
    } else {
        d = global_alloc(&k->user_mem);
        if (!d) {
            return NULL;
        }
    }

    struct desc *next = t->mem.first;
    d->next = next;
    d->prev = NULL;
	d->task = t;
    t->mem.first = d;
    t->mem.count++;

	if (next) {
		next->prev = d;
	}

    release_core_lock(c);
	return desc_to_page(&k->user_mem, d);
}

void do_release_page(struct core *c, struct desc *d) {
	struct kernel *k = c->kernel;

	d->prev = NULL;
	d->task = NULL;

	// add to the core list
	struct desc *next = c->mem.first;
	d->next = c->mem.first;
	c->mem.first = d;
	if (!next) {
		c->mem.last = d;
	}
	c->mem.count++;

	// see if we want to release to the global free list
	if (c->mem.count > 32) {
		global_release(&k->user_mem, c->mem.first, c->mem.last);
		c->mem.first = NULL;
		c->mem.last = NULL;
		c->mem.count = 0;
	}
}

int release_page(struct core *c, void *pg) {
	struct kernel *k = c->kernel;
	struct task *t = c->running;

	if (t->creating_task) {
		t = t->creating_task;
	}

	// check that the argument is an actual page pointer
	if ((uintptr_t) pg & ((PGSZ)-1)) {
		return NDL_EINVAL;
	}

	// in the user memory pool
	struct desc *d = page_to_desc(&k->user_mem, pg);
	if (d < k->user_mem.begin || d >= k->user_mem.end) {
		return NDL_EINVAL;
	}

	// and assigned to this task
	if (d->task != t) {
		return NDL_EINVAL;
	}

	acquire_core_lock(c);
	
	// remove from the task's page list
	if (d->next) {
		d->next->prev = d->prev;
	}
	if (d->prev) {
		d->prev->next = d->next;
	}
	if (t->mem.first == d) {
		t->mem.first = d->next;
	}

	t->mem.count--;

	do_release_page(c, d);

	release_core_lock(c);

	return 0;
}
