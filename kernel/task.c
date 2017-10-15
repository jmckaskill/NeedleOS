#include "kernel.h"
#include <string.h>

int create_task(struct core *c) {
	struct kernel *k = c->kernel;
	struct task *t = c->running;

	if (t->creating_task) {
		return NDL_EINPROGRESS;
	}

	struct desc *dt = global_alloc(&k->kernel_mem);
	if (!dt) {
		return NDL_ENOMEM;
	}
	struct task *nt = (struct task*) desc_to_page(&k->kernel_mem, dt);
	memset(nt, 0, sizeof(*nt));
	nt->mem.quota = 1024;

	t->creating_task = nt;

	return 0;
}

void free_task(struct core *c, struct task *t) {
	struct kernel *k = c->kernel;

	for (struct desc *d = t->mem.first; d != NULL;) {
		struct desc *next = d->next;

		acquire_core_lock(c);
		do_release_page(c, d);
		release_core_lock(c);

		d = next;
	}

	struct desc *dt = page_to_desc(&k->kernel_mem, t);

	acquire_core_lock(c);

	// remove from the core list
	if (t->next) {
		t->next->prev = t->prev;
	}
	if (t->prev) {
		t->prev->next = t->prev;
	}
	if (c->tasks == t) {
		c->tasks = t->next;
	}

	// free the task memory itself
	os_close_task(t);
	global_release(&k->kernel_mem, dt, dt);

	release_core_lock(c);
}

int start_task(struct core *c, ndl_task_fn fn, void *udata) {
	struct kernel *k = c->kernel;
	struct task *t = c->running;
	struct task *nt = t->creating_task;

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
	nt->next = c->tasks;
	nt->prev = NULL;
	if (nt->next) {
		nt->next->prev = nt;
	}
	c->tasks = nt;

	// this will switch immediately to the new thread
	c->running = nt;
	os_start_task(c, nt, fn, udata);
	return 0;
}
