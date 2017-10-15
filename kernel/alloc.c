#include "kernel.h"

struct page *alloc_page(struct core *c) {
    struct kernel *k = c->kernel;
    struct task *t = c->running;
    if (t->user.count >= t->user.quota) {
        return NULL;
    }

    acquire_core_lock();

    struct desc *d = c->user.first;
    if (d) {
        struct desc *next = d->next;
        c->user.first = d->next;
        if (!next) {
            c->user.last = NULL;
        }
        c->user.count--;
    } else {
        d = global_alloc(&k->user);
        if (!d) {
            return NULL;
        }
    }

    struct desc *next = t->user.first;
    d->next = next;
    d->prev = NULL;
    t->user.first = d;
    t->user.count++;

    release_core_lock();

    size_t idx = d - k->user.begin;
    return k->user.pages + idx;
}
