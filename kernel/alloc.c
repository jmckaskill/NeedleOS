#include "kernel.h"
#include <assert.h>

struct page *kern_alloc_page(struct kern_task *t) {
	struct core *c = t->core;
    struct kernel *k = c->kernel;
	
	if (t->creating_task) {
		t = t->creating_task;
	}

    if (t->mem.count >= t->mem.quota) {
        return NULL;
    }

    struct kern_desc *d = c->mem.first;
    if (d) {
        struct kern_desc *next = d->next;
        c->mem.first = d->next;
        if (!next) {
            c->mem.last = NULL;
        }
        c->mem.count--;
    } else {
        d = kern_alloc(&k->user_mem);
        if (!d) {
            return NULL;
        }
    }

    struct kern_desc *next = t->mem.first;
    d->next = next;
    d->prev = NULL;
	d->task = t;
    t->mem.first = d;
    t->mem.count++;

	if (next) {
		next->prev = d;
	}

	return desc_to_page(&k->user_mem, d);
}

void release_desc(struct core *c, struct kern_desc *d) {
	struct kernel *k = c->kernel;

	d->prev = NULL;
	d->task = NULL;

	// add to the core list
	struct kern_desc *next = c->mem.first;
	d->next = c->mem.first;
	c->mem.first = d;
	if (!next) {
		c->mem.last = d;
	}
	c->mem.count++;

	// see if we want to release to the global free list
	if (c->mem.count > 32) {
		kern_free(&k->user_mem, c->mem.first, c->mem.last);
		c->mem.first = NULL;
		c->mem.last = NULL;
		c->mem.count = 0;
	}
}

struct kern_desc *verify_page(struct kern_task *t, void *pg) {
	struct core *c = t->core;
	struct kernel *k = c->kernel;
	
	// check that the argument is an actual page pointer
	if ((uintptr_t) pg & ((PGSZ)-1)) {
		return NULL;
	}

	// in the user memory pool
	struct kern_desc *d = page_to_desc(&k->user_mem, pg);
	if (d < k->user_mem.begin || d >= k->user_mem.end) {
		return NULL;
	}

	// and assigned to this task
	if (d->task != t) {
		return NULL;
	}

	return d;
}

int kern_free_page(struct kern_task *t, void *pg) {
	struct core *c = t->core;
	struct kernel *k = c->kernel;

	if (t->creating_task) {
		t = t->creating_task;
	}

	struct kern_desc *d = verify_page(t, pg);
	if (!d) {
		return NDL_EINVAL;
	}
	
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

	release_desc(c, d);

	return 0;
}

void transfer_page(struct kern_task *from, struct kern_task *to, struct kern_desc *d) {
	struct core *c = from->core;
	struct kernel *k = c->kernel;

	assert(from->core == to->core);

	// remove from the source page list
	if (d->next) {
		d->next->prev = d->prev;
	}
	if (d->prev) {
		d->prev->next = d->next;
	}
	if (from->mem.first == d) {
		from->mem.first = d->next;
	}
	from->mem.count--;

	// add to the target page list
	d->next = to->mem.first;
	d->prev = NULL;
	d->task = to;
	if (d->next) {
		d->next->prev = d;
	}
	to->mem.count++;
}
