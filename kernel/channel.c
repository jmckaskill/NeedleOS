#include "kernel.h"
#include <assert.h>
#include <string.h>

#ifdef _MSC_VER
#include <intrin.h>
static inline int ffs(unsigned long x) {
	unsigned long idx;
	if (_BitScanForward(&idx, x)) {
		return idx + 1;
	} else {
		return 0;
	}
}
#else
#include <strings.h>
#endif

#define MAKE_CHAN_PAIR(tx, rx) (((tx) << 5) | (rx))

static int find_chan_slot(struct chan **channels) {
	for (int i = 1; i < 32; i++) {
		if (!channels[i]) {
			return i;
		}
	}
	return 0;
}

int kern_pipe(struct kern_task *t, int chan, ndl_dispatch_fn fn, void *udata) {
	struct core *c = t->core;
	struct kernel *k = c->kernel;

	struct chan *ch;
	if (!chan) {
		// need to create the channel pair
		int rx = find_chan_slot(t->rx);
		int tx = find_chan_slot(t->tx);

		if (!tx || !rx) {
			// too many open channels
			return NDL_ENOMEM;
		}

		struct kern_desc *dc = kern_alloc(&k->kernel_mem);
		if (!dc) {
			return NDL_ENOMEM;
		}

		ch = (struct chan*) desc_to_page(&k->kernel_mem, dc);
		memset(ch, 0, sizeof(*ch));
		ch->receiver = t;
		ch->rx_mask = 1U << (unsigned) rx;
		ch->used = ch->msgs;
		ch->ref = 2;

		t->rx[rx] = ch;
		t->tx[tx] = ch;

		chan = MAKE_CHAN_PAIR(tx, rx);

	} else {
		// channel already exists, we are just updating the dispatch function
		ch = t->rx[NDL_OBJ_RX(chan)];
		if (!ch) {
			return NDL_EINVAL;
		}
	}

	ch->fn = fn;
	ch->udata = udata;

	if (fn) {
		t->dispatch_mask |= ch->rx_mask;
	} else {
		t->dispatch_mask &= ~ch->rx_mask;
	}

	return chan;
}

int kern_send(struct kern_task *t, int chan, int cmd, ndl_obj_t obj) {
	struct core *c = t->core;

	struct chan *ch = t->tx[NDL_OBJ_TX(chan) >> 5];
	void *pg = NDL_OBJ_BUF(void, obj);

	if (!ch) {
		// invalid channel, caller will need to free send data
		return NDL_EINVAL;
	}
	
	struct kern_task *rxer = ch->receiver;


	struct msg *m = ch->free;
	if (!m && ch->used >= &ch->msgs[NUM_MSGS]) {
		// overfull channel, caller will need to free send data (or try again later)
		return NDL_EWOULDBLOCK;
	}

	struct kern_desc *d = NULL;
	if (pg && t != ch->receiver && (d = verify_page(t, pg)) == NULL) {
		// invalid page
		return NDL_EINVAL;
	}

	// grab the slot in the channel
	if (m) {
		ch->free = m->next;
	} else {
		m = ch->used;
		ch->used = m + 1;
	}

	// transfer message data over
	if (d) {
		transfer_page(t, rxer, d);
	}

	m->cmd = cmd;
	m->obj = (uintptr_t) pg;

	// add the msg to the end of the queue
	struct msg *end = ch->normal_priority.last;
	if (end) {
		end->next = m;
	} else {
		ch->normal_priority.first = m;
	}
	m->next = NULL;
	ch->normal_priority.last = m;

	rxer->ready_mask |= ch->rx_mask;

	return 0;
}

int kern_recv(struct kern_task *t, uint32_t mask, ndl_tick_t wakeup, struct ndl_message *r) {
	struct core *c = t->core;

	unsigned idx;
	while ((idx = ffs(t->ready_mask & mask)) == 0) {

		// no message, need to go to sleep
		t->rx_mask = mask;

		// remove ourselves from the ready list
		if (t->next) {
			t->next->prev = t->prev;
		}
		if (t->prev) {
			t->prev->next = t->next;
		}
		if (c->ready == t) {
			c->ready = t->next;
		}

		// add ourselves to the asleep list
		t->next = c->asleep;
		t->prev = NULL;
		if (t->next) {
			t->next->prev = t;
		}
		c->asleep = t;

		os_yield(c);
	}

	int chan = idx - 1;
	struct chan *ch = t->rx[chan];
	assert(ch);

	struct msg *msg = ch->normal_priority.first;
	assert(msg);

	struct msg *next = msg->next;
	ch->normal_priority.first = next;
	if (!next) {
		ch->normal_priority.last = NULL;
		t->ready_mask &= ~ch->rx_mask;
	}

	r->chan = chan;
	r->tick = kern_tick();
	r->cmd = msg->cmd;
	r->obj = msg->obj;

	// put the msg back into the pool
	msg->next = ch->free;
	ch->free = msg;

	return 0;
}

int kern_move(struct kern_task *t, ndl_obj_t obj) {
	struct core *c = t->core;
	int tx = NDL_OBJ_TX(obj);
	int rx = NDL_OBJ_RX(obj);
	void *pg = NDL_OBJ_BUF(void, obj);

	struct kern_task *to = t->creating_task;
	if (!to) {
		return NDL_ENEEDSTART;
	}

	// verify the inputs

	int txnew = 0;
	if (tx) {
		if (!t->tx[tx]) {
			return NDL_EINVAL;
		}
		txnew = find_chan_slot(to->tx);
		if (!txnew) {
			return NDL_ENOMEM;
		}
	}

	int rxnew = 0;
	if (rx) {
		if (!t->rx[rx]) {
			return NDL_EINVAL;
		}
		rxnew = find_chan_slot(to->rx);
		if (!rxnew) {
			return NDL_ENOMEM;
		}
	}

	struct kern_desc *d = NULL;
	if (pg && (d = verify_page(t, pg)) != NULL) {
		return NDL_EINVAL;
	}

	if (txnew) {
		to->tx[txnew] = t->tx[tx];
		t->tx[tx] = NULL;
	}

	if (rxnew) {
		struct chan *ch = t->rx[rx];
		t->ready_mask &= ~ch->rx_mask;
		t->dispatch_mask &= ~ch->rx_mask;
		t->rx[rx] = NULL;

		ch->receiver = to;
		ch->rx_mask = 1U << (unsigned) rxnew;
		ch->fn = NULL;
		ch->udata = NULL;
		to->rx[rxnew] = ch;

		if (ch->normal_priority.first) {
			to->ready_mask |= ch->rx_mask;
		}
	}

	if (d) {
		transfer_page(t, to, d);
	}

	return (uintptr_t) pg | MAKE_CHAN_PAIR(txnew, rxnew);
}

