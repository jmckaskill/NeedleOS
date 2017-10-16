#include "kernel.h"

void schedule_next(struct core *c) {
	if (c->ready) {
		c->running = c->ready;
		os_resume_task(c, c->running);
	}
}

