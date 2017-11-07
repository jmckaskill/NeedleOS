#include "kernel.h"
#include <string.h>


int kern_fork(struct kern_task *t) {
	struct kern_cpu *c = t->cpu;

	if (t->creating_task) {
		return NDL_EINPROGRESS;
	}

	struct kern_desc *dt = kern_alloc(c->kernel_mem);
	if (!dt) {
		return NDL_ENOMEM;
	}
	struct kern_task *nt = (struct kern_task*) desc_to_page(&k->kernel_mem, dt);
	memset(nt, 0, sizeof(*nt));
	nt->mem.quota = 1024;

	t->creating_task = nt;

	return 0;
}

struct kern_task *init_task(struct kern_cpu *c) {
	struct kern_desc *dt = kern_alloc(c->kernel_mem);
	if (!dt) {
		return NULL;
	}
	struct kern_task *nt = (struct kern_task*) desc_to_page(c->kernel_mem, dt);
	memset(nt, 0, sizeof(*nt));
	nt->mem.quota = 1024;
	nt->next = NULL;
	nt->prev = NULL;
	nt->cpu = c;

	c->ready = nt;
	return nt;
}

int kern_start(struct kern_task *t, ndl_task_fn fn, void *udata) {
	struct core *c = t->core;
	struct kernel *k = c->kernel;
	struct kern_task *nt = t->creating_task;

	if (!nt) {
		return NDL_ENEEDSTART;
	}

	t->creating_task = NULL;

	if (!fn) {
		// user request to cancel the started task
		free_task(c, t);
		return 0;
	}

	// add new task to this core's task list as it's now schedulable
	nt->next = c->ready;
	nt->prev = NULL;
	if (nt->next) {
		nt->next->prev = nt;
	}
	c->ready = nt;
	nt->core = c;

	os_setup_task(c, nt, fn, udata);

	// we'll run the task next time the scheduler is run
	return 0;
}

void free_task(struct core *c, struct kern_task *t) {
	struct kernel *k = c->kernel;

	for (struct kern_desc *d = t->mem.first; d != NULL;) {
		struct kern_desc *next = d->next;
		release_desc(c, d);
		d = next;
	}

	struct kern_desc *dt = page_to_desc(&k->kernel_mem, t);

	// remove from the core list
	if (t->next) {
		t->next->prev = t->prev;
	}
	if (t->prev) {
		t->prev->next = t->next;
	}
	if (c->ready == t) {
		c->ready = t->next;
	}
	if (c->asleep == t) {
		c->asleep = t->next;
	}

	// free the task memory itself
	os_close_task(t);
	kern_free(&k->kernel_mem, dt, dt);
}

