#define NDL_TINY_CLASSES 2
#define NDL_SMALL_CLASSES 2
#define NDL_MEDIUM_CLASSES 7

#if defined _MSC_VER
#define ALIGN_BEFORE(x) __declspec(align(x))
#define ALIGN_AFTER(x)
#else
#define ALIGN_BEFORE(x)
#define ALIGN_AFTER(x) __attribute__((aligned(x)))
#endif

ALIGN_BEFORE(32) struct ndl_tiny_page
{
    struct ndl_tiny_page *next, *prev;
    uint32_t free[4];
    uint32_t avail;
} ALIGN_AFTER(32);

ALIGN_BEFORE(16) struct ndl_small_page {
    struct ndl_small_page *next, *prev;
    uint64_t free;
} ALIGN_AFTER(16);

ALIGN_BEFORE(16) struct ndl_medium_page {
    struct ndl_medium_page *next, *prev;
    uint32_t free;
} ALIGN_AFTER(16);

struct ndl_heap {
    struct ndl_tiny_page *tiny[NDL_TINY_CLASSES];
    struct ndl_small_page *small[NDL_SMALL_CLASSES];
    struct ndl_medium_page *medium[NDL_MEDIUM_CLASSES];
};