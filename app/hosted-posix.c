#include "kernel.h"
#include <stdlib.h>
#include <stdio.h>

#define SHARED_SIZE 1*1024*1024
#define KERNEL_SIZE 1*1024*1024
#define USER_SIZE 8*1024*1024

struct kernel ndl_kernel;

static void alloc_pool(struct mempool *pool, size_t sz) {
    if (posix_memalign(&pool->next, PGSZ, sz)) {
        fprinf(stderr, "failed to allocate memory\n");
        exit(3);
    }
    pool->top = pool->next + sz;
}

static pthread_key_t core_key;

struct core *ndl_current_core() {
    return (struct core*) pthread_getspecific(core_key);
}

void ndl_init() {
    alloc_pool(&ndl_kernel.mem_shared, SHARED_SIZE);
    alloc_pool(&ndl_kernel.mem_kernel, KERNEL_SIZE);
    alloc_pool(&ndl_kernel.mem_user, USER_SIZE);
    pthread_key_create(&core_key, NULL);
    pthread_setspecific(core_key, &ndl_kernel.core[0]);
}

