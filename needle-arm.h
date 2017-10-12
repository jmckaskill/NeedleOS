static inline struct ndl_task *ndl_current_task() {
    register struct ndl_task *r9 asm("r9");
    return r9;
}

static inline uintptr_t ndl_syscall0(int num) {
    register int svcno asm("r7") = num;
    register uintptr_t ret asm("r0");
    __asm__ volatile("svc #0" : "=r"(ret) : "r"(svcno));
    return ret;
}

static inline uintptr_t ndl_syscall1(int num, uintptr_t arg0) {
    register int svcno asm("r7") = num;
    register uintptr_t a0 asm("r0") = arg0;
    register uintptr_t ret asm("r0");
    __asm__ volatile("svc #0" : "=r"(ret) : "r"(svcno), "r"(a0));
    return ret;
}

static inline uintptr_t ndl_syscall2(int num, uintptr_t arg0, uintptr_t arg1) {
    register int svcno asm("r7") = num;
    register uintptr_t a0 asm("r0") = arg0;
    register uintptr_t a1 asm("r1") = arg1;
    register uintptr_t ret asm("r0");
    __asm__ volatile("svc #0" : "=r"(ret) : "r"(svcno), "r"(a0), "r"(a1));
    return ret;
}

static inline uintptr_t ndl_syscall3(int num, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2) {
    register int svcno asm("r7") = num;
    register uintptr_t a0 asm("r0") = arg0;
    register uintptr_t a1 asm("r1") = arg1;
    register uintptr_t a2 asm("r2") = arg2;
    register uintptr_t ret asm("r0");
    __asm__ volatile("svc #0" : "=r"(ret) : "r"(svcno), "r"(a0), "r"(a1), "r"(a2));
    return ret;
}

static inline uintptr_t ndl_syscall4(int num, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3) {
    register int svcno asm("r7") = num;
    register uintptr_t a0 asm("r0") = arg0;
    register uintptr_t a1 asm("r1") = arg1;
    register uintptr_t a2 asm("r2") = arg2;
    register uintptr_t a3 asm("r3") = arg3;
    register uintptr_t ret asm("r0");
    __asm__ volatile("svc #0" : "=r"(ret) : "r"(svcno), "r"(a0), "r"(a1), "r"(a2), "r"(a3));
    return ret;
}

#define NDL_SVC_ALLOC_PAGE 0
#define NDL_SVC_RELEASE_PAGE 1
#define NDL_SVC_SEND 2
#define NDL_SVC_RECV 3
#define NDL_SVC_CURRENT_TICK 4
#define NDL_SVC_CREATE_CHANNEL 5
#define NDL_SVC_CLOSE_CHANNEL 6
#define NDL_SVC_SET_DISPATCH_FN 7
#define NDL_SVC_CREATE_TASK 8
#define NDL_SVC_CANCEL_TASK 9
#define NDL_SVC_START_TASK 10
#define NDL_SVC_SET_TASK_PRIORITY 11
#define NDL_SVC_TRANFER_CHANNEL 12
#define NDL_SVC_TRANSFER_SHARED 13
#define NDL_SVC_DISPATCH 14
#define NDL_SVC_ALLOC_SHARED 15
#define NDL_SVC_RELEASE_SHARED 16


static inline void *ndl_alloc_page() {
    return (void*) ndl_syscall0(NDL_SVC_ALLOC_PAGE);
}

static inline int ndl_release_page(void *page) {
    return ndl_syscall1(NDL_SVC_RELEASE_PAGE, (uintptr_t) page);
}

static inline int ndl_send(int chan, int cmd, void *buf, int flags) {
    return ndl_syscall4(NDL_SVC_SEND, chan, cmd, (uintptr_t) buf, flags);
}

static inline int ndl_recv(uint32_t mask, ndl_tick_t wakeup, struct ndl_message *msg) {
    return ndl_syscall3(NDL_SVC_RECV, mask, wakeup, (uintptr_t) msg);
}

static inline int ndl_dispatch() {
    return ndl_syscall0(NDL_SVC_DISPATCH);
}

static inline ndl_tick_t ndl_current_tick() {
    return ndl_syscall0(NDL_SVC_CURRENT_TICK);
}

static inline int ndl_create_channel() {
    return ndl_syscall0(NDL_SVC_CREATE_CHANNEL);
}

static inline int ndl_close_channel(int chan) {
    return ndl_syscall1(NDL_SVC_CLOSE_CHANNEL, chan);
}

static inline int ndl_set_dispatch_fn(int chan, ndl_dispatch_fn fn, void *udata) {
    return ndl_syscall3(NDL_SVC_SET_DISPATCH_FN, chan, (uintptr_t) (void*) fn, (uintptr_t) udata);
}

static inline int ndl_create_task() {
    return ndl_syscall0(NDL_SVC_CREATE_TASK);
}

static inline int ndl_cancel_task() {
    return ndl_syscall0(NDL_SVC_CANCEL_TASK);
}

static inline int ndl_start_task(ndl_task_fn fn, void *udata) {
    return ndl_syscall2(NDL_SVC_START_TASK, (uintptr_t) (void*) fn, (uintptr_t) udata);
}

static inline int ndl_set_task_priority(int pri) {
    return ndl_syscall1(NDL_SVC_SET_TASK_PRIORITY, pri);
}

static inline int ndl_transfer_channel(int chan) {
    return ndl_syscall1(NDL_SVC_TRANFER_CHANNEL, chan);
}

static inline int ndl_transfer_shared(void *buf, int flags) {
    return ndl_syscall2(NDL_SVC_TRANSFER_SHARED, (uintptr_t) buf, flags);
}



