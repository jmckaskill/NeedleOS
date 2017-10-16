#include "needle.h"

void child_task(void* udata) {
	int ch = (int) (uintptr_t) udata;
	void *page = ndl_alloc_page();
	ndl_release_page(page);

	struct ndl_message msg;
	ndl_recv(NDL_MASK(ch), 0, &msg);
}

void app_main() {
	void *pages[64];
	for (int i = 0; i < 64; i++) {
		pages[i] = ndl_alloc_page();
	}
	for (int i = 0; i < 64; i++) {
		ndl_release_page(pages[i]);
	}

	int ch = ndl_create_channel(NULL, NULL);

	ndl_create_task();
	int newch = ndl_transfer(NULL, NDL_MOVE_RX(ch));
	ndl_start_task(&child_task, (void*) (uintptr_t) newch);

	ndl_send(ch, 1, NULL, 0);
}

