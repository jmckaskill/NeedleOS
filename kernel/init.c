#include "kernel.h"

#define L1_SIZE (16*1024U)
#define SUPER_SIZE (16*1024*1024U)
#define ALIGN_UP(P,SZ) (((uintptr_t) (P) + (SZ) - 1) & ~((SZ)-1))
#define ALIGN_DOWN(P,SZ) (((uintptr_t) (P)) & ~((SZ)-1))

#define L1_PRIV_EXECUTE_NEVER (1U << 0)
#define L1_SECTION (1U << 1)
#define L1_BUFFERED (1U << 2)
#define L1_CACHED (1U << 3)
#define L1_EXECUTE_NEVER (1U << 4)
#define L1_SIMPLE_UNPRIV_ACCESS (1U << 11) // AP[1]
#define L1_TYPE_EXTENSION(x) ((x) << 12)
// write back with write allocate on both inner and outer domains
#define L1_ALL_WRITE_BACK (L1_TYPE_EXTENSION(5) | L1_BUFFERED)
#define L1_SIMPLE_DISABLE_WRITE (1U << 15) // AP[2]
#define L1_SHAREABLE (1U << 16)
#define L1_NOT_GLOBAL (1U << 17)
#define L1_SUPER_SECTION (1U << 18)
#define L1_NON_SECURE (1U << 19)

struct stacks init_kernel() {
    // top page is the IRQ stack
    // second to top page is the globals
    char *globals = ((char*) INIT_MEM_START) + INIT_MEM_SIZE - PGSZ*2;
    struct kern_mem *kernel_mem = (struct kern_mem*) globals;
    struct kern_mem *user_mem = kernel_mem + 1;

    // init memory layout is
    //  instructions
    //  globals (page aligned)
    //  L1 table (16K aligned)
    //  IRQ stack (at least a page) - already in use by this function

    uintptr_t *L1_table = (uintptr_t*) ALIGN_DOWN(INIT_MEM_START + INIT_MEM_SIZE - PGSZ, L1_SIZE);
    char *globals = (char*) L1_table - PGSZ

    // init the super sections
    for (uintptr_t i = 0; i < 256; i++) {
        L1_table[i*16] = (i * SUPER_SIZE) 
            | L1_PRIV_EXECUTE_NEVER
            | L1_SECTION
            | L1_EXECUTE_NEVER
            | L1_NON_SECURE;
            // leave TEX,B,C as 0 - strongly ordered, shareable
            // leave writeable AP[2] = 0
            // leave with no unpriv access AP[1] = 0
            // leave global, NG = 0
    }

    // setup init memory layout
    uintptr_t init_super = INIT_MEM_START / SUPER_SIZE;
    for (int i = 0; i < ALIGN_UP(INIT_MEM_SIZE, SUPER_SIZE) / SUPER_SIZE; i++) {
        uintptr_t mmu = L1_table[init_super + i*16];
        mmu &= ~L1_PRIV_EXECUTE_NEVER;
        mmu &= ~L1_EXECUTE_NEVER;
    }

    // main memory layout is
    // user memory (super aligned)
    //  pages (page aligned)
    // kernel memory (super aligned)
    //  pages (page aligned)
    //  descriptors
    //  global tables
    size_t user_pages = ALIGN_UP(USER_MEM_SIZE, SUPER_SIZE) / PGSZ;
    size_t kern_pages = ALIGN_UP(KERN_MEM_SIZE, PGSZ) / PGSZ;

    struct page *user_page = (struct page*) ALIGN_UP(MEM_START, SUPER_SIZE);
    struct page *kern_page = user_page + user_pages;

    struct kern_desc *user_begin = (struct kern_desc*) (kern_page + kern_pages);
    struct kern_desc *user_end = user_begin + user_pages;
    struct kern_desc *kern_begin = user_end;
    struct kern_desc *kern_end = kern_begin + kern_pages;

    kernel_mem->free = NULL;
    kernel_mem->top = kern_begin;
    kernel_mem->begin = kern_begin;
    kernel_mem->end = kern_end;
    kernel_mem->pages = kern_page;

    user_mem->free = NULL;
    user_mem->top = user_begin;
    user_mem->begin = user_begin;
    user_mem->end = user_end;
    user_mem->pages = user_page;

    // setup kernel memory as 

    // copy the super section translation entry to the whole super section
    for (int i = 0; i < 256; i++) {
        uintptr_t super_val = L1_table[i*16];
        for (int j = 1; j < 16; j++) {
            L1_table[i*16 + j] = super_val;
        }
    }
}