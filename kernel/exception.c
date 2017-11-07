#include "kernel.h"

uintptr_t ndl_syscall3(int num, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2) {
	struct kern_task *t = kern_current_task();
	struct kern_cpu *c = t->cpu;
	kern_lock(c);
	uintptr_t ret = NDL_EINVAL;

	switch (num) {
	case NDL_SVC_ALLOC_PAGE:
		ret = (uintptr_t)kern_alloc_page(t);
		break;
	case NDL_SVC_FREE_PAGE:
		ret = kern_free_page(t, (void*)arg0);
		break;
	case NDL_SVC_SEND:
		ret = kern_send(t, arg0, arg1, arg2);
		break;
	case NDL_SVC_RECV:
		ret = kern_recv(t, arg0, arg1, (struct ndl_message*) arg2);
		break;
	case NDL_SVC_CURRENT_TICK:
		ret = kern_tick();
		break;
	case NDL_SVC_PIPE:
		ret = kern_pipe(t, arg0, arg1, arg2);
		break;
	case NDL_SVC_CLOSE:
		ret = kern_close(t, arg0);
		break;
	case NDL_SVC_FORK:
		ret = kern_fork(t);
		break;
	case NDL_SVC_START:
		ret = kern_start(t, arg0, arg1);
		break;
	case NDL_SVC_MOVE:
		ret = kern_move(t, arg0);
		break;
	case NDL_SVC_DISPATCH:
		ret = kern_dispatch(t);
		break;
	}

	kern_unlock(c);
	return ret;
}
