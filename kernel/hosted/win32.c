#include "../kernel.h"
#include <Windows.h>
#include <stdbool.h>
#include <assert.h>

struct win32_task {
	bool valid;
	bool yielded;
	HANDLE thread;
	HANDLE yield;
	ndl_task_fn fn;
	void *udata;
};

struct win32_cpu {
	CRITICAL_SECTION core_lock;
	HANDLE schedule;
};

static struct kernel kernel;
static CRITICAL_SECTION global_lock;
static DWORD tls_task;

struct kern_task *kern_current_task() {
	return (struct kern_task*) TlsGetValue(tls_task);
}

#define PAGE_ALIGN(P) (((uintptr_t) (P) + (PGSZ) - 1) & ~((PGSZ)-1))

static void init_mem_pool(struct kern_mem *p, size_t sz) {
	size_t num_pages = (sz / (PGSZ + sizeof(struct kern_desc))) - 1;
	char *data = (char*) VirtualAlloc(NULL, sz, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	p->begin = (struct kern_desc*) data;
	p->end = p->begin + num_pages;
	p->pages = (struct page*) PAGE_ALIGN(p->end);
	p->free = NULL;
	p->top = p->begin;
}

struct kern_desc *kern_alloc(struct kern_mem *p) {
	EnterCriticalSection(&global_lock);
	struct kern_desc *ret = p->free;
	if (ret) {
		p->free = ret->next;
	} else if (p->top < p->end) {
		ret = p->top;
		p->top = ret + 1;
	} else {
		ret = NULL;
	}
	LeaveCriticalSection(&global_lock);
	return ret;
}

void kern_free(struct kern_mem *p, struct kern_desc *first, struct kern_desc *last) {
	EnterCriticalSection(&global_lock);
	last->next = p->free;
	p->free = first;
	LeaveCriticalSection(&global_lock);
}

void kern_lock(struct kern_cpu *c) {
	struct win32_cpu *wc = (struct win32_cpu*) &c->platform;
	EnterCriticalSection(&wc->core_lock);
}

void kern_unlock(struct kern_cpu *c) {
	struct win32_cpu *wc = (struct win32_cpu*) &c->platform;
	LeaveCriticalSection(&wc->core_lock);
}

static DWORD WINAPI main_thread(void *param) {
	struct kern_task *t = (struct kern_task*) param;
	struct core *c = t->core;
	struct win32_cpu *wc = (struct win32_cpu*) &c->os;
	TlsSetValue(tls_task, t);
	app_main();
	free_task(c, t);
	SetEvent(wc->schedule);
	return 0;
}

static DWORD WINAPI task_thread(void *param) {
	struct kern_task *t = (struct kern_task*) param;
	struct core *c = t->core;
	struct win32_task *wt = (struct win32_task*) &t->os;
	struct win32_cpu *wc = (struct win32_cpu*) &c->os;
	TlsSetValue(tls_task, t);
	wt->fn(wt->udata);
	free_task(c, t);
	SetEvent(wc->schedule);
	return 0;
}

int main() {
	SetProcessDEPPolicy(PROCESS_DEP_ENABLE);
	init_mem_pool(&kernel.kernel_mem, KERN_MEM_SIZE);
	init_mem_pool(&kernel.user_mem, USER_MEM_SIZE);
	InitializeCriticalSection(&global_lock);

	struct kern_desc *dc = kern_alloc(&kernel.kernel_mem);
	struct core *c = (struct core*) desc_to_page(&kernel.kernel_mem, dc);
	memset(c, 0, sizeof(*c));
	c->kernel = &kernel;

	struct win32_cpu *wc = (struct win32_cpu*) &c->os;
	InitializeCriticalSection(&wc->core_lock);
	wc->schedule = CreateEvent(NULL, FALSE, FALSE, NULL);

	struct kern_desc *dt = kern_alloc(&kernel.kernel_mem);
	struct kern_task *t = (struct kern_task*) desc_to_page(&kernel.kernel_mem, dt);
	memset(t, 0, sizeof(*t));
	t->mem.quota = 1024;
	t->core = c;
	t->next = NULL;
	t->prev = NULL;

	c->ready = t;
	c->running = t;
	
	struct win32_task *wt = (struct win32_task*) &t->os;
	wt->valid = true;
	wt->yield = CreateEvent(NULL, FALSE, FALSE, NULL);
	wt->thread = CreateThread(NULL, 64*1024, &main_thread, t, 0, NULL);
	for (;;) {
		WaitForSingleObject(wc->schedule, INFINITE);
		acquire_core_lock(c);
		schedule_next(c);
		release_core_lock(c);
	}
}

void os_resume_task(struct core *c, struct kern_task *t) {
	struct win32_task *wt = (struct win32_task*) &t->os;
	if (wt->yielded) {
		wt->yielded = false;
		SetEvent(wt->yield);
	} else {
		ResumeThread(wt->thread);
	}
}

void os_yield(struct core *c) {
	struct kern_task *t = c->running;
	struct win32_task *wt = (struct win32_task*) &t->os;
	struct win32_cpu *wc = (struct win32_cpu*) &c->os;
	assert(wt->valid);
	wt->yielded = true;
	SetEvent(wc->schedule);
	release_core_lock(c);
	WaitForSingleObject(wt->yield, INFINITE);
	acquire_core_lock(c);
}

void os_setup_task(struct core *c, struct kern_task *t, ndl_task_fn fn, void *udata) {
	struct win32_task *wt = (struct win32_task*) &t->os;
	assert(!wt->valid);
	wt->fn = fn;
	wt->udata = udata;
	wt->valid = true;
	wt->yield = CreateEvent(NULL, FALSE, FALSE, NULL);
	wt->thread = CreateThread(NULL, 64*1024, &task_thread, t, CREATE_SUSPENDED, NULL);
}

void os_close_task(struct kern_task *t) {
	struct win32_task *wt = (struct win32_task*) &t->os;
	assert(wt->valid);
	CloseHandle(wt->thread);
	wt->valid = false;
}

ndl_tick_t kern_tick() {
	uint64_t ticks = GetTickCount64();
	return (ndl_tick_t) NDL_MS_TO_TICK(ticks);
}
