#include "kernel.h"
#include "tinycthread.h"

struct hosted_task {
    bool run;
    bool yielded;
    cnd_t wakeup;
    ndl_task_fn fn;
    void *udata;
};

struct hosted_cpu {
    mtx_t cpu_lock;
};

static struct kernel kernel;
static mtx_t global_lock;
static tss_t thread_task;

#ifdef _WIN32
static size_t min_stack;
#else
static pthread_attr_t min_stack;
#endif

#define PAGE_ALIGN(P) (((uintptr_t) (P) + (PGSZ) - 1) & ~((PGSZ)-1))

static void init_mem_pool(struct kernel_pool *p, size_t sz) {
    size_t num_pages = (sz / (PGSZ + sizeof(struct kern_desc))) - 1;
#ifdef _WIN32
    char *data = (char*) VirtualAlloc(NULL, sz, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    char *data = (char*) mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
#endif
	p->begin = (struct kern_desc*) data;
	p->end = p->begin + num_pages;
	p->pages = (struct page*) PAGE_ALIGN(p->end);
	p->free = NULL;
	p->top = p->begin;
}

#ifdef _WIN32
typedef DWORD (WINAPI *thread_fn)(void*);
#define THREAD_RET DWORD WINAPI
#else
typedef void* (*thread_fn)(void*);
#define THREAD_RET void*
#endif

static THREAD_RET interrupt_thread(void *udata);

int main() {
    mtx_init(&global_lock, mtx_plain);
	init_mem_pool(&kernel.kernel_mem, KERN_MEM_SIZE);
    init_mem_pool(&kernel.user_mem, USER_MEM_SIZE);

#ifdef _WIN32
    min_stack = ...
#else
    pthread_attr_init(&min_stack);
    pthread_attr_setstacksize(&min_stack, PTHREAD_MIN_STACK);
#endif

	struct kern_desc *dc = kern_alloc(&kernel.kernel_mem);
	struct core *c = (struct core*) desc_to_page(&kernel.kernel_mem, dc);
	memset(c, 0, sizeof(*c));
	c->kernel = &kernel;

	struct hosted_cpu *hc = (struct hosted_cpu*) &c->platform;
    mtx_init(&hc->cpu_lock);

    mtx_lock(&hc->cpu_lock);
    start_initial_task(c);
    mtx_unlock(&hc->cpu_lock);

    interrupt_thread(c);
    return 0;
}

static THREAD_RET interrupt_thread(void *udata) {
    struct kern_cpu *c = (struct kern_cpu*) udata;
}

static THREAD_RET task_thread(void *udata) {
    struct kern_task *t = (struct kern_task*) udata;
    struct kern_cpu *c = t->cpu;
    struct hosted_task *ht = (struct hosted_task*) &t->platform;
    struct hosted_cpu *hc = (struct hosted_cpu*) &c->platform;
    tss_set(thread_task, t);
    ht->fn(ht->udata);
    tss_set(thread_task, NULL);

    mtx_lock(&hc->cpu_lock);
    free_task(c, t);
    schedule_next(c);
    mtx_unlock(&hc->cpu_lock);

    return 0;
}

struct kern_desc *kern_alloc(struct kernel_pool *p) {
    mtx_lock(&global_lock);
	struct kern_desc *ret = p->free;
	if (ret) {
		p->free = ret->next;
	} else if (p->top < p->end) {
		ret = p->top;
		p->top = ret + 1;
	} else {
		ret = NULL;
    }
    mtx_unlock(&global_lock);
	return ret;
}

void kern_free(struct kernel_pool *p, struct kern_desc *first, struct kern_desc *last) {
	mtx_lock(&global_lock);
	last->next = p->free;
	p->free = first;
	mtx_unlock(&global_lock);
}

static void yield(struct kern_cpu *c) {
    struct hosted_cpu *hc = (struct hosted_cpu*) &c->platform;
    struct kern_task *from = (struct kern_task*) tss_get(thread_task);
    // the tss variable gets unset when the thread wants to exit
    if (from) {
        struct hosted_task *hf = (struct hosted_task*) &from->platform;
        assert(hf->run);
        hf->run = false;
        do {
            cnd_wait(&hf->wakeup, &hc->cpu_lock);
        } while (!hf->run);
    }
}

void os_switch(struct kern_cpu *c, struct kern_task *t) {
    struct hosted_task *ht = (struct hosted_task*) &t->platform;
    struct hosted_cpu *hc = (struct hosted_cpu*) &c->platform;
    
    // wakeup the new thread
    ht->run = true;
    cnd_signal(&ht->wakeup);
    
    // send this thread to sleep
    yield(c);
}

void os_start_task(struct kern_cpu *c, struct kern_task *t, ndl_task_fn fn, void *udata) {
    struct hosted_task *ht = (struct hosted_task*) &t->platform;
    ht->run = true;
    cnd_init(&ht->wakeup);

    // use platform specific thread create functions so we can set the stack size
#ifdef _WIN32
    HANDLE thrd = CreateThread(NULL, min_stack, &task_thread, t, 0, NULL);
    CloseHandle(thrd);
#else
    pthread_t thrd;
    pthread_create(&thrd, &min_stack, &task_thread, t);
    pthread_detach(thrd);
#endif

    yield(c);
}

void os_close_task(struct kern_task *t) {
    struct hosted_task *ht = (struct hosted_task*) &t->platform;
    cnd_destroy(&ht->wakeup);
}

ndl_tick_t kern_tick() {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (NDL_TICK_PER_SEC * ts.ts_sec) + (ts.ts_nsec * NDL_TICK_PER_SEC) / 1000000000;
}


// user functions


void *ndl_alloc_page() {
	struct kern_task *t = (struct kern_task*) tss_get(thread_task);
	struct hosted_cpu *hc = (struct hosted_cpu*) &t->cpu->platform;
    mtx_lock(&hc->cpu_lock);
    void *ret = kern_alloc_page(t);
    mtx_unlock(&hc->cpu_lock);
	return ret;
}

int ndl_release_page(void *pg) {
	struct kern_task *t = (struct kern_task*) tss_get(thread_task);
	struct hosted_cpu *hc = (struct hosted_cpu*) &t->cpu->platform;
    mtx_lock(&hc->cpu_lock);
	int ret = kern_free_page(t, pg);
	mtx_unlock(&hc->cpu_lock);
	return ret;
}

int ndl_send(int chan, int cmd, void *buf, int flags) {
	struct kern_task *t = (struct kern_task*) tss_get(thread_task);
	struct hosted_cpu *hc = (struct hosted_cpu*) &t->cpu->platform;
    mtx_lock(&hc->cpu_lock);
	int ret = kern_send(t, chan, cmd, (uintptr_t) buf | flags);
	mtx_unlock(&hc->cpu_lock);
	return ret;
}

int ndl_recv(uint32_t mask, ndl_tick_t wakeup, struct ndl_message *r) {
	struct kern_task *t = (struct kern_task*) tss_get(thread_task);
	struct hosted_cpu *hc = (struct hosted_cpu*) &t->cpu->platform;
    mtx_lock(&hc->cpu_lock);
	int ret = kern_recv(t, mask, wakeup, r);
	mtx_unlock(&hc->cpu_lock);
	return ret;
}

int ndl_dispatch();

ndl_tick_t ndl_current_tick() {
	return kern_tick();
}

int ndl_create_channel(ndl_dispatch_fn fn, void *udata) {
	struct kern_task *t = (struct kern_task*) tss_get(thread_task);
	struct hosted_cpu *hc = (struct hosted_cpu*) &t->cpu->platform;
    mtx_lock(&hc->cpu_lock);
	int ret = kern_pipe(t, 0, fn, udata);
	mtx_unlock(&hc->cpu_lock);
	return ret;
}

int ndl_set_dispatch_fn(int chan, ndl_dispatch_fn fn, void *udata);
int ndl_close_channel(int chan);

int ndl_create_task() {
	struct kern_task *t = (struct kern_task*) tss_get(thread_task);
	struct hosted_cpu *hc = (struct hosted_cpu*) &t->cpu->platform;
    mtx_lock(&hc->cpu_lock);
	int ret = kern_fork(t);
	mtx_unlock(&hc->cpu_lock);
	return ret;
}

int ndl_cancel_task() {
	struct kern_task *t = (struct kern_task*) tss_get(thread_task);
	struct hosted_cpu *hc = (struct hosted_cpu*) &t->cpu->platform;
    mtx_lock(&hc->cpu_lock);
	int ret = kern_start(t, NULL, NULL);
	mtx_unlock(&hc->cpu_lock);
	return ret;
}

int ndl_start_task(ndl_task_fn fn, void *udata) {
	struct kern_task *t = (struct kern_task*) tss_get(thread_task);
	struct hosted_cpu *hc = (struct hosted_cpu*) &t->cpu->platform;
    mtx_lock(&hc->cpu_lock);
	int ret = kern_start(t, fn, udata);
	mtx_unlock(&hc->cpu_lock);
	return ret;
}

int ndl_transfer(void *buf, int flags) {
	struct kern_task *t = (struct kern_task*) tss_get(thread_task);
    struct hosted_cpu *hc = (struct hosted_cpu*) &t->cpu->platform;
    mtx_lock(&hc->cpu_lock);
	int ret = kern_move(t, (uintptr_t) buf | flags);
	mtx_unlock(&hc->cpu_lock);
	return ret;
}

