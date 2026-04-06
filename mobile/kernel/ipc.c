/*
 * IPC Subsystem Implementation
 * uOS(m) - User OS Mobile
 */

#include "ipc.h"

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
    
    return -1;  /* No free ports */
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
    
    /* Check if queue is full */
    uint32_t next_tail = (pq->queue_tail + 1) % pq->queue_size;
    if (next_tail == pq->queue_head) {
        return -1;  /* Queue full */
    }
    
    /* Enqueue message */
    if (pq->queue == NULL) {
        return -1;  /* Queue not allocated */
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
    
    /* Check if queue is empty */
    if (pq->queue_head == pq->queue_tail) {
        return -1;  /* No messages */
    }
    
    /* Dequeue message */
    *msg = pq->queue[pq->queue_head];
    pq->queue_head = (pq->queue_head + 1) % pq->queue_size;
    
    return 0;
}

/* Send and receive (synchronous RPC) */
int ipc_send_recv(port_t dst, message_t *send_msg, port_t reply_port, 
                  message_t *reply_msg) {
    /* Send request */
    if (ipc_send_message(dst, send_msg) < 0) {
        return -1;
    }
    
    /* Wait for reply */
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