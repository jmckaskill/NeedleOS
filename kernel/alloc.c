#include "kernel.h"

#define acquire_core_lock() __asm__ volatile ("cpsid aif")
#define release_core_lock() __asm__ volatile ("cpsie aif")

struct desc *global_alloc(struct kernel_pool *p);

void *alloc_page() {
    struct task *t = CORE.running;
    if (t->user.count >= t->user.quota) {
        return NULL;
    }

    acquire_core_lock();

    struct desc *d = CORE.user.first;
    if (d) {
        struct desc *next = d->next;
        CORE.user.first = d->next;
        if (!next) {
            CORE.user.last = NULL;
        }
        CORE.user.count--;
    } else {
        d = global_alloc(&KERNEL.user);
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

    size_t idx = d - &DESCRIPTORS;
    return &PAGES + idx;
}
