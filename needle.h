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

typedef void (*ndl_task_fn)(void*);
typedef void (*ndl_dispatch_fn)(void*, ndl_tick_t tick, int cmd, ndl_obj_t obj);


#if defined _WIN32 || defined __linux__ || defined __MACH__
#include "needle-hosted.h"
#elif defined __arm__
#include "needle-arm.h"
#else
#error
#endif

#include "needle-heap.h"

#define NDL_ENOMEM -1
#define NDL_EINVAL -2
#define NDL_EINPROGRESS -3
#define NDL_ENEEDSTART -4
#define NDL_EWOULDBLOCK -5


