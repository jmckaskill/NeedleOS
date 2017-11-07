#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef _MSC_VER
#define NDL_ALIGNED(x) __declspec(align(x))
#else
#define NDL_ALIGNED(x) __attribute__((aligned(x)))
#endif

typedef uint32_t ndl_tick_t;
typedef ptrdiff_t ndl_obj_t;
#define NDL_MS_TO_TICK(ms) ((ms) * 1000 / 32768)
#define NDL_TICK_PER_SEC 32768

#define NDL_OBJ_BUF(TYPE, P) ((TYPE *) (void*) ((uintptr_t) (P) & ~0xFFFU))
#define NDL_OBJ_TX(P) ((P) & (0x1F << 5))
#define NDL_OBJ_RX(P) ((P) & (0x1F))

#define NDL_CMD_HIGH_PRI (1U << 31)
#define NDL_COPY_BUF (1 << 11)
#define NDL_MOVE_TX(ch) ((ch) & (0x1F << 5))
#define NDL_COPY_TX(ch) (NDL_MOVE_TX(ch) | (1 << 10))
#define NDL_MOVE_RX(ch) ((ch) & 0x1F)

#define NDL_MASK(ch) (1U << (unsigned) ((ch) & 0x1F))

struct ndl_message {
	int chan;
    ndl_tick_t tick;
    int cmd;
    ndl_obj_t obj;
};

struct ndl_channel {
	ndl_obj_t rx;
	ndl_obj_t tx;
};

typedef void (*ndl_task_fn)(void*);
typedef void (*ndl_dispatch_fn)(void*, ndl_tick_t tick, int cmd, ndl_obj_t obj);


#if defined _WIN32 || defined __linux__ || defined __MACH__
uintptr_t ndl_syscall3(int num, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2);

static inline uintptr_t ndl_syscall0(int num) {
	return ndl_syscall3(num, 0, 0, 0);
}

static inline uintptr_t ndl_syscall1(int num, uintptr_t arg0) {
	return ndl_syscall3(num, arg0, 0, 0);
}

static inline uintptr_t ndl_syscall2(int num, uintptr_t arg0, uintptr_t arg1) {
	return ndl_syscall3(num, arg0, arg1, 0);
}
#elif defined __arm__
static inline uintptr_t ndl_syscall0(int num) {
	register int svcno __asm__("r0") = num;
	register uintptr_t ret __asm__("r0");
	__asm__ volatile("svc #0" : "=r"(ret) : "r"(svcno) : "r0", "r1", "r2", "r3", "r12");
	return ret;
}

static inline uintptr_t ndl_syscall1(int num, uintptr_t arg0) {
	register int svcno __asm__("r0") = num;
	register uintptr_t a0 __asm__("r1") = arg0;
	register uintptr_t ret __asm__("r0");
	__asm__ volatile("svc #0" : "=r"(ret) : "r"(svcno), "r"(a0) : "r0", "r1", "r2", "r3", "r12");
	return ret;
}

static inline uintptr_t ndl_syscall2(int num, uintptr_t arg0, uintptr_t arg1) {
	register int svcno __asm__("r0") = num;
	register uintptr_t a0 __asm__("r1") = arg0;
	register uintptr_t a1 __asm__("r2") = arg1;
	register uintptr_t ret __asm__("r0");
	__asm__ volatile("svc #0" : "=r"(ret) : "r"(svcno), "r"(a0), "r"(a1) : "r0", "r1", "r2", "r3", "r12");
	return ret;
}

static inline uintptr_t ndl_syscall3(int num, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2) {
	register int svcno __asm__("r0") = num;
	register uintptr_t a0 __asm__("r1") = arg0;
	register uintptr_t a1 __asm__("r2") = arg1;
	register uintptr_t a2 __asm__("r3") = arg2;
	register uintptr_t ret __asm__("r0");
	__asm__ volatile("svc #0" : "=r"(ret) : "r"(svcno), "r"(a0), "r"(a1), "r"(a2) : "r0", "r1", "r2", "r3", "r12");
	return ret;
}
#else
#error
#endif

#define NDL_ENOMEM -1
#define NDL_EINVAL -2
#define NDL_EINPROGRESS -3
#define NDL_ENEEDSTART -4
#define NDL_EWOULDBLOCK -5

#define NDL_SVC_ALLOC_PAGE 0
#define NDL_SVC_FREE_PAGE 1
#define NDL_SVC_SEND 2
#define NDL_SVC_RECV 3
#define NDL_SVC_CURRENT_TICK 4
#define NDL_SVC_PIPE 5
#define NDL_SVC_CLOSE 6
#define NDL_SVC_FORK 7
#define NDL_SVC_START 8
#define NDL_SVC_MOVE 9
#define NDL_SVC_DISPATCH 10

static inline void *ndl_alloc_page() {
	return (void*)ndl_syscall0(NDL_SVC_ALLOC_PAGE);
}

static inline int ndl_release_page(void *page) {
	return ndl_syscall1(NDL_SVC_FREE_PAGE, (uintptr_t)page);
}

static inline int ndl_send(int chan, int cmd, void *buf, int flags) {
	return ndl_syscall3(NDL_SVC_SEND, chan, cmd, (uintptr_t)buf | flags);
}

static inline int ndl_recv(uint32_t mask, ndl_tick_t wakeup, struct ndl_message *msg) {
	return ndl_syscall3(NDL_SVC_RECV, mask, wakeup, (uintptr_t)msg);
}

static inline int ndl_dispatch() {
	return ndl_syscall0(NDL_SVC_DISPATCH);
}

static inline ndl_tick_t ndl_current_tick() {
	return ndl_syscall0(NDL_SVC_CURRENT_TICK);
}

static inline int ndl_create_channel(struct ndl_channel *pch, ndl_dispatch_fn fn, void *udata) {
	return ndl_syscall3(NDL_SVC_PIPE, (uintptr_t)pch, (uintptr_t)(void*)fn, (uintptr_t)udata);
}

static inline int ndl_set_dispatch_fn(int chan, ndl_dispatch_fn fn, void *udata) {
	return ndl_syscall3(NDL_SVC_PIPE, chan, (uintptr_t)(void*)fn, (uintptr_t)udata);
}

static inline int ndl_close_channel(int chan) {
	return ndl_syscall1(NDL_SVC_CLOSE, chan);
}

static inline int ndl_create_task() {
	return ndl_syscall0(NDL_SVC_FORK);
}

static inline int ndl_cancel_task() {
	return ndl_syscall1(NDL_SVC_START, 0);
}

static inline int ndl_start_task(ndl_task_fn fn, void *udata) {
	return ndl_syscall2(NDL_SVC_START, (uintptr_t)(void*)fn, (uintptr_t)udata);
}

static inline int ndl_transfer(void *buf, int flags) {
	return ndl_syscall1(NDL_SVC_MOVE, (uintptr_t)buf | flags);
}




