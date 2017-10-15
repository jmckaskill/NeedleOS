#include "needle.h"

void child_task(void* udata) {
	void *page = ndl_alloc_page();
	ndl_release_page(page);
}

void app_main() {
	void *pages[64];
	for (int i = 0; i < 64; i++) {
		pages[i] = ndl_alloc_page();
	}
	for (int i = 0; i < 64; i++) {
		ndl_release_page(pages[i]);
	}

	ndl_create_task();
	ndl_start_task(&child_task, NULL);

	pages[0] = ndl_alloc_page();
}