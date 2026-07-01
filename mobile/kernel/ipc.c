/*
 * IPC Subsystem Implementation
 * uOS(m) - User OS Mobile
 */

#include "ipc.h"
#include "memory.h"
#include "memory_utils.h"

/* Maximum ports */
#define MAX_PORTS 256
#define MAX_MESSAGES_PER_PORT 64

/* Port management */
static port_queue_t port_table[MAX_PORTS];
static int port_next_id = 1;

extern void uart_puts(const char *s);
extern void uart_putc(char c);

/* Initialize IPC subsystem */
int ipc_init(void) {
    uart_puts("IPC subsystem initializing...\n");

    for (int i = 0; i < MAX_PORTS; i++) {
        port_table[i].port_id = 0;
        port_table[i].owner_pid = 0;
        port_table[i].queue = NULL;
        port_table[i].queue_size = 0;
    }

    uart_puts("IPC ready\n");
    return 0;
}

/* Create a new port */
int ipc_port_create(uint32_t pid, port_t *port) {
    if (!port) return -1;

    for (int i = 0; i < MAX_PORTS; i++) {
        if (port_table[i].port_id == 0) {
            port_table[i].port_id = port_next_id++;
            port_table[i].owner_pid = pid;
            port_table[i].queue_size = MAX_MESSAGES_PER_PORT;
            port_table[i].queue_head = 0;
            port_table[i].queue_tail = 0;

            *port = port_table[i].port_id;
            return 0;
        }
    }

    return -1;
}

/* Destroy a port */
int ipc_port_destroy(port_t port) {
    for (int i = 0; i < MAX_PORTS; i++) {
        if (port_table[i].port_id == port) {
            port_table[i].port_id = 0;
            port_table[i].owner_pid = 0;
            return 0;
        }
    }
    return -1;
}

/* Find port in table */
static int ipc_find_port(port_t port) {
    for (int i = 0; i < MAX_PORTS; i++) {
        if (port_table[i].port_id == port) {
            return i;
        }
    }
    return -1;
}

/* Send message to port */
int ipc_send_message(port_t dst, message_t *msg) {
    if (!msg) return -1;

    int port_idx = ipc_find_port(dst);
    if (port_idx < 0) return -1;

    port_queue_t *pq = &port_table[port_idx];

    uint32_t next_tail = (pq->queue_tail + 1) % pq->queue_size;
    if (next_tail == pq->queue_head) {
        return -1;
    }

    if (pq->queue == NULL) {
        return -1;
    }

    pq->queue[pq->queue_tail] = *msg;
    pq->queue_tail = next_tail;

    return 0;
}

/* Receive message from port */
int ipc_recv_message(port_t port, message_t *msg) {
    if (!msg) return -1;

    int port_idx = ipc_find_port(port);
    if (port_idx < 0) return -1;

    port_queue_t *pq = &port_table[port_idx];

    if (pq->queue_head == pq->queue_tail) {
        return -1;
    }

    *msg = pq->queue[pq->queue_head];
    pq->queue_head = (pq->queue_head + 1) % pq->queue_size;

    return 0;
}

/* Send and receive (synchronous RPC) */
int ipc_send_recv(port_t dst, message_t *send_msg, port_t reply_port,
                  message_t *reply_msg) {
    if (ipc_send_message(dst, send_msg) < 0) {
        return -1;
    }
    if (ipc_recv_message(reply_port, reply_msg) < 0) {
        return -1;
    }
    return 0;
}

/* Allocate a new port */
port_t ipc_allocate_port(void) {
    port_t port;
    if (ipc_port_create(0, &port) == 0) {
        return port;
    }
    return -1;
}

/* Free a port */
void ipc_free_port(port_t port) {
    ipc_port_destroy(port);
}

void ipc_ring_init(ipc_ring_t *ring) {
    ring->head = 0;
    ring->tail = 0;
    ring->count = 0;
}

int ipc_ring_push(ipc_ring_t *ring, const void *buf, uint32_t len) {
    if (ring->count >= IPC_RING_CAPACITY || len == 0) return -1;
    const uint8_t *src = (const uint8_t *)buf;
    for (uint32_t i = 0; i < len; i++) {
        ring->data[ring->tail] = src[i];
        ring->tail = (ring->tail + 1) % IPC_RING_CAPACITY;
        ring->count++;
    }
    return 0;
}

int ipc_ring_pop(ipc_ring_t *ring, void *buf, uint32_t len) {
    if (ring->count == 0 || len == 0) return -1;
    uint8_t *dst = (uint8_t *)buf;
    uint32_t to_read = (len < ring->count) ? len : ring->count;
    for (uint32_t i = 0; i < to_read; i++) {
        dst[i] = ring->data[ring->head];
        ring->head = (ring->head + 1) % IPC_RING_CAPACITY;
        ring->count--;
    }
    return (int)to_read;
}

int ipc_ring_empty(const ipc_ring_t *ring) {
    return ring->count == 0;
}

int sys_ipc_send(port_t dst_port, const void *buf, uint32_t len) {
    message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_type = MSG_TYPE_REQUEST;
    msg.dst_port = dst_port;
    msg.src_port = 0;
    msg.data_len = len;
    msg.data = (uint8_t *)buf;
    return ipc_send_message(dst_port, &msg);
}

int sys_ipc_recv(port_t src_port, void *buf, uint32_t len, int block) {
    (void)block;
    message_t msg;
    if (ipc_recv_message(src_port, &msg) < 0) return -1;
    uint32_t to_copy = msg.data_len < len ? msg.data_len : len;
    memcpy(buf, msg.data, to_copy);
    return (int)to_copy;
}