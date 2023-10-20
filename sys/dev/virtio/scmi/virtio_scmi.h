/*
 * Virtio SCMI Device
 *
 * Copyright (c) 2023 Arm Ltd
 *
 * This header is BSD licensed so anyone can use the definitions
 * to implement compatible drivers/servers:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of IBM nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL IBM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef VIRTIO_SCMI_H
#define VIRTIO_SCMI_H

#include <dev/virtio/virtqueue.h>

/* Features bits */
/* Device implements some SCMI notifications, or delayed responses */
#define VIRTIO_SCMI_F_P2A_CHANNELS	(1 << 0)
/* Device implements any SCMI statistics region */
#define VIRTIO_SCMI_F_SHARED_MEMORY	(1 << 1)

#define VIRTIO_SCMI_FEATURES	\
	(VIRTIO_SCMI_F_P2A_CHANNELS | VIRTIO_SCMI_F_SHARED_MEMORY)

/* Virtqueues */
enum vtscmi_chan {
	VIRTIO_SCMI_CHAN_A2P,
	VIRTIO_SCMI_CHAN_P2A,
	VIRTIO_SCMI_CHAN_MAX
};

typedef void virtio_scmi_rx_callback_t(void *msg, unsigned int len, void *priv);

device_t virtio_scmi_transport_get(void);
int virtio_scmi_channel_size_get(device_t dev, enum vtscmi_chan chan);
int virtio_scmi_channel_callback_set(device_t dev, enum vtscmi_chan chan,
				     virtio_scmi_rx_callback_t *cb, void *priv);
int virtio_scmi_message_enqueue(device_t dev, enum vtscmi_chan chan,
				void *buf, unsigned int tx_len,
				unsigned int rx_len);
void *virtio_scmi_message_poll(device_t dev, uint32_t *rx_len);

#endif
