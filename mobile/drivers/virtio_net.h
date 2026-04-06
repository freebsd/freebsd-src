/*
 * VirtIO Network Device Header
 * uOS(m) - User OS Mobile
 */

#ifndef _VIRTIO_NET_H_
#define _VIRTIO_NET_H_

#include <stdint.h>
#include <stddef.h>

/* Initialize VirtIO network device */
int virtio_net_init(void);

/* Send a packet */
int virtio_net_send_packet(const uint8_t *data, size_t len);

/* Receive a packet */
int virtio_net_receive_packet(uint8_t *buffer, size_t *len);

#endif /* _VIRTIO_NET_H_ */