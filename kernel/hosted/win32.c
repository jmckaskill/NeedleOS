#include "../kernel.h"
#include <Windows.h>
#include <stdbool.h>
#include <assert.h>

struct win32_task {
	bool started;
	HANDLE thread;
	ndl_task_fn fn;
	void *udata;
};

struct win32_core {
	CRITICAL_SECTION core_lock;
};

static struct kernel kernel;
static CRITICAL_SECTION global_lock;
static DWORD tls_core;

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

static DWORD WINAPI main_thread(void *param) {
	struct core *c = (struct core*) param;
	struct task *t = c->running;
	TlsSetValue(tls_core, c);
	app_main();
	free_task(c, t);
	schedule_next(c);
	return 0;
}

static DWORD WINAPI task_thread(void *param) {
	struct core *c = (struct core*) param;
	struct task *t = c->running;
	struct win32_task *wt = (struct win32_task*) &t->os;
	TlsSetValue(tls_core, c);
	wt->fn(wt->udata);
	free_task(c, t);
	schedule_next(c);
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

	struct desc *dt = global_alloc(&kernel.kernel_mem);
	struct task *t = (struct task*) desc_to_page(&kernel.kernel_mem, dt);
	memset(t, 0, sizeof(*t));
	t->mem.quota = 1024;

	c->tasks = t;
	c->running = t;
	
	struct win32_task *wt = (struct win32_task*) &t->os;
	wt->started = true;
	wt->thread = CreateThread(NULL, 64*1024, &main_thread, c, 0, NULL);
	Sleep(INFINITE);
}

void acquire_core_lock(struct core *c) {
	struct win32_core *wc = (struct win32_core*) &c->os;
	EnterCriticalSection(&wc->core_lock);
}

void release_core_lock(struct core *c) {
	struct win32_core *wc = (struct win32_core*) &c->os;
	LeaveCriticalSection(&wc->core_lock);
}

void os_pause_task(struct task *t) {
	struct win32_task *wt = (struct win32_task*) &t->os;
	assert(wt->started);
	SuspendThread(wt->thread);
}

void os_resume_task(struct task *t) {
	struct win32_task *wt = (struct win32_task*) &t->os;
	assert(wt->started);
	ResumeThread(wt->thread);
}

void os_start_task(struct core *c, struct task *t, ndl_task_fn fn, void *udata) {
	struct win32_task *wt = (struct win32_task*) &t->os;
	assert(!wt->started);
	wt->fn = fn;
	wt->udata = udata;
	wt->started = true;
	wt->thread = CreateThread(NULL, 64*1024, &task_thread, c, 0, NULL);
	SuspendThread(GetCurrentThread());
}

void os_close_task(struct task *t) {
	struct win32_task *wt = (struct win32_task*) &t->os;
	if (wt->started) {
		CloseHandle(wt->thread);
	}
}


void *ndl_alloc_page() {
	struct core *c = (struct core*) TlsGetValue(tls_core);
	return alloc_page(c);
}

int ndl_release_page(void *pg) {
	struct core *c = (struct core*) TlsGetValue(tls_core);
	return release_page(c, pg);
}

int ndl_send(int chan, int cmd, void *buf, int flags);
int ndl_recv(uint32_t mask, ndl_tick_t wakeup, struct ndl_message *msg);
int ndl_dispatch();

ndl_tick_t ndl_current_tick() {
	uint64_t ticks = GetTickCount64();
	return (ndl_tick_t) NDL_MS_TO_TICK(ticks);
}

ndl_obj_t ndl_create_channel(ndl_dispatch_fn fn, void *udata);
ndl_obj_t ndl_set_dispatch_fn(int chan, ndl_dispatch_fn fn, void *udata);
int ndl_close_channel(int chan);

int ndl_create_task() {
	struct core *c = (struct core*) TlsGetValue(tls_core);
	return create_task(c);
}

int ndl_cancel_task() {
	struct core *c = (struct core*) TlsGetValue(tls_core);
	return start_task(c, NULL, NULL);
}

int ndl_start_task(ndl_task_fn fn, void *udata) {
	struct core *c = (struct core*) TlsGetValue(tls_core);
	return start_task(c, fn, udata);
}

int ndl_transfer(void *buf, int flags);

