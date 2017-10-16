#include "needle-svc.h"

void *ndl_alloc_page();
int ndl_release_page(void *pg);

int ndl_send(int chan, int cmd, void *buf, int flags);
int ndl_recv(uint32_t mask, ndl_tick_t wakeup, struct ndl_message *r);
int ndl_dispatch();
ndl_tick_t ndl_current_tick();
int ndl_create_channel(ndl_dispatch_fn fn, void *udata);
int ndl_set_dispatch_fn(int chan, ndl_dispatch_fn fn, void *udata);
int ndl_close_channel(int chan);
int ndl_create_task();
int ndl_cancel_task();
int ndl_start_task(ndl_task_fn fn, void *udata);
int ndl_transfer(void *buf, int flags);


