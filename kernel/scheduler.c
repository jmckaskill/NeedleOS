#include "kernel.h"

void schedule_next(struct core *c) {
	c->running = c->tasks;
	if (c->running) {
		os_resume_task(c->running);
	}
}

