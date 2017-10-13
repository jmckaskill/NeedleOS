#include "needle.h"

int main() {
    void *p = ndl_alloc_page();
    ndl_release_page(p);
}

void init_board() {

}
