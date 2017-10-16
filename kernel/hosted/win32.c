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

struct win32_core {
	CRITICAL_SECTION core_lock;
	HANDLE schedule;
};

static struct kernel kernel;
static CRITICAL_SECTION global_lock;
static DWORD tls_task;

#define PAGE_ALIGN(P) (((uintptr_t) (P) + (PGSZ) - 1) & ~((PGSZ)-1))

static void init_mem_pool(struct kernel_pool *p, size_t sz) {
	size_t num_pages = (sz / (PGSZ + sizeof(struct desc))) - 1;
	char *data = (char*) VirtualAlloc(NULL, sz, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	p->begin = (struct desc*) data;
	p->end = p->begin + num_pages;
	p->pages = (struct page*) PAGE_ALIGN(p->end);
	p->free = NULL;
	p->top = p->begin;
}

struct desc *global_alloc(struct kernel_pool *p) {
	EnterCriticalSection(&global_lock);
	struct desc *ret = p->free;
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

void global_release(struct kernel_pool *p, struct desc *first, struct desc *last) {
	EnterCriticalSection(&global_lock);
	last->next = p->free;
	p->free = first;
	LeaveCriticalSection(&global_lock);
}

static void acquire_core_lock(struct core *c) {
	struct win32_core *wc = (struct win32_core*) &c->os;
	EnterCriticalSection(&wc->core_lock);
}

static void release_core_lock(struct core *c) {
	struct win32_core *wc = (struct win32_core*) &c->os;
	LeaveCriticalSection(&wc->core_lock);
}

static DWORD WINAPI main_thread(void *param) {
	struct task *t = (struct task*) param;
	struct core *c = t->core;
	struct win32_core *wc = (struct win32_core*) &c->os;
	TlsSetValue(tls_task, t);
	app_main();
	free_task(c, t);
	SetEvent(wc->schedule);
	return 0;
}

static DWORD WINAPI task_thread(void *param) {
	struct task *t = (struct task*) param;
	struct core *c = t->core;
	struct win32_task *wt = (struct win32_task*) &t->os;
	struct win32_core *wc = (struct win32_core*) &c->os;
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

	struct desc *dc = global_alloc(&kernel.kernel_mem);
	struct core *c = (struct core*) desc_to_page(&kernel.kernel_mem, dc);
	memset(c, 0, sizeof(*c));
	c->kernel = &kernel;

	struct win32_core *wc = (struct win32_core*) &c->os;
	InitializeCriticalSection(&wc->core_lock);
	wc->schedule = CreateEvent(NULL, FALSE, FALSE, NULL);

	struct desc *dt = global_alloc(&kernel.kernel_mem);
	struct task *t = (struct task*) desc_to_page(&kernel.kernel_mem, dt);
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

void os_resume_task(struct core *c, struct task *t) {
	struct win32_task *wt = (struct win32_task*) &t->os;
	if (wt->yielded) {
		wt->yielded = false;
		SetEvent(wt->yield);
	} else {
		ResumeThread(wt->thread);
	}
}

void os_yield(struct core *c) {
	struct task *t = c->running;
	struct win32_task *wt = (struct win32_task*) &t->os;
	struct win32_core *wc = (struct win32_core*) &c->os;
	assert(wt->valid);
	wt->yielded = true;
	SetEvent(wc->schedule);
	release_core_lock(c);
	WaitForSingleObject(wt->yield, INFINITE);
	acquire_core_lock(c);
}

void os_setup_task(struct core *c, struct task *t, ndl_task_fn fn, void *udata) {
	struct win32_task *wt = (struct win32_task*) &t->os;
	assert(!wt->valid);
	wt->fn = fn;
	wt->udata = udata;
	wt->valid = true;
	wt->yield = CreateEvent(NULL, FALSE, FALSE, NULL);
	wt->thread = CreateThread(NULL, 64*1024, &task_thread, t, CREATE_SUSPENDED, NULL);
}

void os_close_task(struct task *t) {
	struct win32_task *wt = (struct win32_task*) &t->os;
	assert(wt->valid);
	CloseHandle(wt->thread);
	wt->valid = false;
}


void *ndl_alloc_page() {
	struct task *t = (struct task*) TlsGetValue(tls_task);
	struct core *c = t->core;
	acquire_core_lock(c);
	void *ret = alloc_page(t);
	release_core_lock(c);
	return ret;
}

int ndl_release_page(void *pg) {
	struct task *t = (struct task*) TlsGetValue(tls_task);
	struct core *c = t->core;
	acquire_core_lock(c);
	int ret = release_page(t, pg);
	release_core_lock(c);
	return ret;
}

int ndl_send(int chan, int cmd, void *buf, int flags) {
	struct task *t = (struct task*) TlsGetValue(tls_task);
	struct core *c = t->core;
	acquire_core_lock(c);
	int ret = send_msg(t, chan, cmd, (uintptr_t) buf | flags);
	release_core_lock(c);
	return ret;
}

int ndl_recv(uint32_t mask, ndl_tick_t wakeup, struct ndl_message *r) {
	struct task *t = (struct task*) TlsGetValue(tls_task);
	struct core *c = t->core;
	acquire_core_lock(c);
	int ret = recv_msg(t, mask, wakeup, r);
	release_core_lock(c);
	return ret;
}

int ndl_dispatch();

ndl_tick_t current_tick() {
	uint64_t ticks = GetTickCount64();
	return (ndl_tick_t) NDL_MS_TO_TICK(ticks);
}

ndl_tick_t ndl_current_tick() {
	return current_tick();
}

int ndl_create_channel(ndl_dispatch_fn fn, void *udata) {
	struct task *t = (struct task*) TlsGetValue(tls_task);
	struct core *c = t->core;
	acquire_core_lock(c);
	int ret = create_channel(t, 0, fn, udata);
	release_core_lock(c);
	return ret;
}

int ndl_set_dispatch_fn(int chan, ndl_dispatch_fn fn, void *udata);
int ndl_close_channel(int chan);

int ndl_create_task() {
	struct task *t = (struct task*) TlsGetValue(tls_task);
	struct core *c = t->core;
	acquire_core_lock(c);
	int ret = create_task(t);
	release_core_lock(c);
	return ret;
}

int ndl_cancel_task() {
	struct task *t = (struct task*) TlsGetValue(tls_task);
	struct core *c = t->core;
	acquire_core_lock(c);
	int ret = start_task(t, NULL, NULL);
	release_core_lock(c);
	return ret;
}

int ndl_start_task(ndl_task_fn fn, void *udata) {
	struct task *t = (struct task*) TlsGetValue(tls_task);
	struct core *c = t->core;
	acquire_core_lock(c);
	int ret = start_task(t, fn, udata);
	release_core_lock(c);
	return ret;
}

int ndl_transfer(void *buf, int flags) {
	struct task *t = (struct task*) TlsGetValue(tls_task);
	struct core *c = t->core;
	acquire_core_lock(c);
	int ret = transfer(t, (uintptr_t) buf | flags);
	release_core_lock(c);
	return ret;
}

