void *ndl_svc_alloc_page(int flags);
int ndl_svc_release_page(void *page, int flags);
int ndl_svc_send(int chan, int cmd, void *buf, int flags);
int ndl_svc_recv(uint32_t mask, ndl_tick_t wakeup, struct ndl_message *msg);
int ndl_svc_dispatch();
ndl_tick_t ndl_svc_current_tick();
int ndl_svc_create_channel();
int ndl_svc_close_channel(int chan);
int ndl_svc_set_dispatch_fn(int chan, ndl_dispatch_fn fn, void *udata);
int ndl_svc_create_task();
int ndl_svc_cancel_task();
int ndl_svc_start_task(ndl_task_fn fn, void *udata);
int ndl_svc_set_task_priority(int pri);
int ndl_svc_transfer_channel(int chan);
int ndl_svc_transfer_page(void *buf, int flags);
