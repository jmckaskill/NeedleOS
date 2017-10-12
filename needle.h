#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef _MSC_VER
#define NDL_ALIGNED(x) __declspec(align(x))
#else
#define NDL_ALIGNED(x) __attribute__((aligned(x)))
#endif

typedef uint32_t ndl_tick_t;
#define NDL_MS_TO_TICK(ms) ((ms) * 1000 / 32768)

struct ndl_message {
    ndl_tick_t tick;
    int cmd;
    void *buf;
    int new_channel;
};

typedef void (*ndl_task_fn)(void*);
typedef void (*ndl_dispatch_fn)(void*, struct ndl_message *m);


#define NDL_TX_ONLY(chan) ((chan) & 0xFF00)
#define NDL_RX_ONLY(chan) ((chan) & 0xFF)

#if defined _WIN32 || defined __linux__ || defined __MACH__
#include "needle-hosted.h"
#elif defined __arm__
#include "needle-arm.h"
#else
#error
#endif

#include "needle-heap.h"

void ndl_init();
int ndl_errno();

#define NDL_EINVAL -2


