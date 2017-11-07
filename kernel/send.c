#include "kernel.h"

int send(struct core *c, int chan, int cmd, ndl_obj_t obj) {
    struct kern_task *t = c->running;
    struct chan *ch = t->tx[NDL_TX_CHAN(chan) >> 8];
    if (!ch) {
        return NDL_EINVAL;
    }
    struct kern_task *rx = ch->receiver;
    if (rx->rx_mask & ch->rx_mask) {
        // fast path, the receiver is waiting for this message
        rx->rx_mask = 0;
        
        return 0;
    } else {
        // slow path, add the message to the queue
    }
}