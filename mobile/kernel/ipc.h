/*
 * IPC Subsystem - Inter-Process Communication
 * uOS(m) - User OS Mobile
 * Microkernel message passing
 */

#ifndef _IPC_H_
#define _IPC_H_

#include <stdint.h>
#include <stddef.h>

/* Message port */
typedef uint32_t port_t;

/* Message types */
#define MSG_TYPE_REQUEST    1
#define MSG_TYPE_REPLY      2
#define MSG_TYPE_SIGNAL     3

/* Message structure */
typedef struct {
    uint32_t msg_type;      /* Message type */
    port_t src_port;        /* Source port */
    port_t dst_port;        /* Destination port */
    uint32_t msg_id;        /* Message ID for reply matching */
    uint32_t data_len;      /* Data length */
    uint8_t *data;          /* Payload */
} message_t;

/* Port queue structure */
typedef struct {
    port_t port_id;
    uint32_t owner_pid;
    message_t *queue;
    uint32_t queue_size;
    uint32_t queue_head;
    uint32_t queue_tail;
} port_queue_t;

/* IPC operations */
int ipc_port_create(uint32_t pid, port_t *port);
int ipc_port_destroy(port_t port);
int ipc_send_message(port_t dst, message_t *msg);
int ipc_recv_message(port_t port, message_t *msg);
int ipc_send_recv(port_t dst, message_t *send_msg, port_t reply_port, 
                  message_t *reply_msg);

/* Port operations */
port_t ipc_allocate_port(void);
void ipc_free_port(port_t port);

#endif /* _IPC_H_ */