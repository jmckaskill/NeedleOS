#include "needle.h"

int main() {
    ndl_init();

    void *p = ndl_alloc_page();
    ndl_release_page(p);
}