#define NDL_TINY_CLASSES 2
#define NDL_SMALL_CLASSES 2
#define NDL_MEDIUM_CLASSES 7

struct ndl_tiny_page {
    struct ndl_tiny_page *next, *prev;
    uint32_t free[4];
    uint32_t avail;
} NDL_ALIGNED(32);

struct ndl_small_page {
    struct ndl_small_page *next, *prev;
    uint64_t free;
} NDL_ALIGNED(16);

struct ndl_medium_page {
    struct ndl_medium_page *next, *prev;
    uint32_t free;
} NDL_ALIGNED(16);

struct ndl_heap {
    struct ndl_tiny_page *tiny[NDL_TINY_CLASSES];
    struct ndl_small_page *small[NDL_SMALL_CLASSES];
    struct ndl_medium_page *medium[NDL_MEDIUM_CLASSES];
};

struct ndl_task {
    int errno;
    struct ndl_heap heap;
};