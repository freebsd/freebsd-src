/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Vincenzo Maffione <vmaffione@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <net/if.h>
#include <net/netmap.h>
#include <net/netmap_virt.h>
#define NETMAP_WITH_LIBS
#include <net/netmap_user.h>

#include <assert.h>

#include "debug.h"
#include "iov.h"
#include "mevent.h"
#include "net_backends.h"
#include "net_backends_priv.h"

/* The virtio-net features supported by netmap. */
#define NETMAP_FEATURES (VIRTIO_NET_F_CSUM | VIRTIO_NET_F_HOST_TSO4 | \
		VIRTIO_NET_F_HOST_TSO6 | VIRTIO_NET_F_HOST_UFO | \
		VIRTIO_NET_F_GUEST_CSUM | VIRTIO_NET_F_GUEST_TSO4 | \
		VIRTIO_NET_F_GUEST_TSO6 | VIRTIO_NET_F_GUEST_UFO)

struct netmap_priv {
	char ifname[IFNAMSIZ];
	struct nm_desc *nmd;
	uint16_t memid;
	struct netmap_ring *rx;
	struct netmap_ring *tx;
	struct mevent *mevp;
	net_be_rxeof_t cb;
	void *cb_param;
};

static void
nmreq_init(struct nmreq *req, char *ifname)
{

	memset(req, 0, sizeof(*req));
	strlcpy(req->nr_name, ifname, sizeof(req->nr_name));
	req->nr_version = NETMAP_API;
}

static int
netmap_set_vnet_hdr_len(struct net_backend *be, int vnet_hdr_len)
{
	int err;
	struct nmreq req;
	struct netmap_priv *priv = NET_BE_PRIV(be);

	nmreq_init(&req, priv->ifname);
	req.nr_cmd = NETMAP_BDG_VNET_HDR;
	req.nr_arg1 = vnet_hdr_len;
	err = ioctl(be->fd, NIOCREGIF, &req);
	if (err) {
		EPRINTLN("Unable to set vnet header length %d", vnet_hdr_len);
		return (err);
	}

	be->be_vnet_hdr_len = vnet_hdr_len;

	return (0);
}

static int
netmap_has_vnet_hdr_len(struct net_backend *be, unsigned vnet_hdr_len)
{
	unsigned prev_hdr_len = be->be_vnet_hdr_len;
	int ret;

	if (vnet_hdr_len == prev_hdr_len) {
		return (1);
	}

	ret = netmap_set_vnet_hdr_len(be, vnet_hdr_len);
	if (ret) {
		return (0);
	}

	netmap_set_vnet_hdr_len(be, prev_hdr_len);

	return (1);
}

static uint64_t
netmap_get_cap(struct net_backend *be)
{

	return (netmap_has_vnet_hdr_len(be, VNET_HDR_LEN) ?
	    NETMAP_FEATURES : 0);
}

static int
netmap_set_cap(struct net_backend *be, uint64_t features __unused,
    unsigned vnet_hdr_len)
{

	return (netmap_set_vnet_hdr_len(be, vnet_hdr_len));
}

static int
netmap_init(struct net_backend *be, const char *devname,
    nvlist_t *nvl __unused, net_be_rxeof_t cb, void *param)
{
	struct netmap_priv *priv = NET_BE_PRIV(be);

	strlcpy(priv->ifname, devname, sizeof(priv->ifname));
	priv->ifname[sizeof(priv->ifname) - 1] = '\0';

	priv->nmd = nm_open(priv->ifname, NULL, NETMAP_NO_TX_POLL, NULL);
	if (priv->nmd == NULL) {
		EPRINTLN("Unable to nm_open(): interface '%s', errno (%s)",
		    devname, strerror(errno));
		return (-1);
	}

	priv->memid = priv->nmd->req.nr_arg2;
	priv->tx = NETMAP_TXRING(priv->nmd->nifp, 0);
	priv->rx = NETMAP_RXRING(priv->nmd->nifp, 0);
	priv->cb = cb;
	priv->cb_param = param;
	be->fd = priv->nmd->fd;

	priv->mevp = mevent_add_disabled(be->fd, EVF_READ, cb, param);
	if (priv->mevp == NULL) {
		EPRINTLN("Could not register event");
		return (-1);
	}

	return (0);
}

static void
netmap_cleanup(struct net_backend *be)
{
	struct netmap_priv *priv = NET_BE_PRIV(be);

	if (priv->mevp) {
		mevent_delete(priv->mevp);
	}
	if (priv->nmd) {
		nm_close(priv->nmd);
	}
	be->fd = -1;
}

static ssize_t
netmap_send(struct net_backend *be, const struct iovec *iov,
	    int iovcnt)
{
	struct netmap_priv *priv = NET_BE_PRIV(be);
	struct netmap_ring *ring;
	ssize_t totlen = 0;
	int nm_buf_size;
	int nm_buf_len;
	uint32_t head;
	uint8_t *nm_buf;
	int j;

	ring = priv->tx;
	head = ring->head;
	if (head == ring->tail) {
		EPRINTLN("No space, drop %zu bytes", count_iov(iov, iovcnt));
		goto txsync;
	}
	nm_buf = NETMAP_BUF(ring, ring->slot[head].buf_idx);
	nm_buf_size = ring->nr_buf_size;
	nm_buf_len = 0;

	for (j = 0; j < iovcnt; j++) {
		uint8_t *iov_frag_buf = iov[j].iov_base;
		int iov_frag_size = iov[j].iov_len;

		totlen += iov_frag_size;

		/*
		 * Split each iovec fragment over more netmap slots, if
		 * necessary.
		 */
		for (;;) {
			int copylen;

			copylen = iov_frag_size < nm_buf_size ? iov_frag_size : nm_buf_size;
			memcpy(nm_buf, iov_frag_buf, copylen);

			iov_frag_buf += copylen;
			iov_frag_size -= copylen;
			nm_buf += copylen;
			nm_buf_size -= copylen;
			nm_buf_len += copylen;

			if (iov_frag_size == 0) {
				break;
			}

			ring->slot[head].len = nm_buf_len;
			ring->slot[head].flags = NS_MOREFRAG;
			head = nm_ring_next(ring, head);
			if (head == ring->tail) {
				/*
				 * We ran out of netmap slots while
				 * splitting the iovec fragments.
				 */
				EPRINTLN("No space, drop %zu bytes",
				    count_iov(iov, iovcnt));
				goto txsync;
			}
			nm_buf = NETMAP_BUF(ring, ring->slot[head].buf_idx);
			nm_buf_size = ring->nr_buf_size;
			nm_buf_len = 0;
		}
	}

	/* Complete the last slot, which must not have NS_MOREFRAG set. */
	ring->slot[head].len = nm_buf_len;
	ring->slot[head].flags = 0;
	head = nm_ring_next(ring, head);

	/* Now update ring->head and ring->cur. */
	ring->head = ring->cur = head;
txsync:
	ioctl(be->fd, NIOCTXSYNC, NULL);

	return (totlen);
}

static ssize_t
netmap_peek_recvlen(struct net_backend *be)
{
	struct netmap_priv *priv = NET_BE_PRIV(be);
	struct netmap_ring *ring = priv->rx;
	uint32_t head = ring->head;
	ssize_t totlen = 0;

	while (head != ring->tail) {
		struct netmap_slot *slot = ring->slot + head;

		totlen += slot->len;
		if ((slot->flags & NS_MOREFRAG) == 0)
			break;
		head = nm_ring_next(ring, head);
	}

	return (totlen);
}

static ssize_t
netmap_recv(struct net_backend *be, const struct iovec *iov, int iovcnt)
{
	struct netmap_priv *priv = NET_BE_PRIV(be);
	struct netmap_slot *slot = NULL;
	struct netmap_ring *ring;
	uint8_t *iov_frag_buf;
	int iov_frag_size;
	ssize_t totlen = 0;
	uint32_t head;

	assert(iovcnt);

	ring = priv->rx;
	head = ring->head;
	iov_frag_buf = iov->iov_base;
	iov_frag_size = iov->iov_len;

	do {
		uint8_t *nm_buf;
		int nm_buf_len;

		if (head == ring->tail) {
			return (0);
		}

		slot = ring->slot + head;
		nm_buf = NETMAP_BUF(ring, slot->buf_idx);
		nm_buf_len = slot->len;

		for (;;) {
			int copylen = nm_buf_len < iov_frag_size ?
			    nm_buf_len : iov_frag_size;

			memcpy(iov_frag_buf, nm_buf, copylen);
			nm_buf += copylen;
			nm_buf_len -= copylen;
			iov_frag_buf += copylen;
			iov_frag_size -= copylen;
			totlen += copylen;

			if (nm_buf_len == 0) {
				break;
			}

			iov++;
			iovcnt--;
			if (iovcnt == 0) {
				/* No space to receive. */
				EPRINTLN("Short iov, drop %zd bytes",
				    totlen);
				return (-ENOSPC);
			}
			iov_frag_buf = iov->iov_base;
			iov_frag_size = iov->iov_len;
		}

		head = nm_ring_next(ring, head);

	} while (slot->flags & NS_MOREFRAG);

	/* Release slots to netmap. */
	ring->head = ring->cur = head;

	return (totlen);
}

static void
netmap_recv_enable(struct net_backend *be)
{
	struct netmap_priv *priv = NET_BE_PRIV(be);

	mevent_enable(priv->mevp);
}

static void
netmap_recv_disable(struct net_backend *be)
{
	struct netmap_priv *priv = NET_BE_PRIV(be);

	mevent_disable(priv->mevp);
}

static struct net_backend netmap_backend = {
	.prefix = "netmap",
	.priv_size = sizeof(struct netmap_priv),
	.init = netmap_init,
	.cleanup = netmap_cleanup,
	.send = netmap_send,
	.peek_recvlen = netmap_peek_recvlen,
	.recv = netmap_recv,
	.recv_enable = netmap_recv_enable,
	.recv_disable = netmap_recv_disable,
	.get_cap = netmap_get_cap,
	.set_cap = netmap_set_cap,
};

/* A clone of the netmap backend, with a different prefix. */
static struct net_backend vale_backend = {
	.prefix = "vale",
	.priv_size = sizeof(struct netmap_priv),
	.init = netmap_init,
	.cleanup = netmap_cleanup,
	.send = netmap_send,
	.peek_recvlen = netmap_peek_recvlen,
	.recv = netmap_recv,
	.recv_enable = netmap_recv_enable,
	.recv_disable = netmap_recv_disable,
	.get_cap = netmap_get_cap,
	.set_cap = netmap_set_cap,
};

DATA_SET(net_backend_set, netmap_backend);
DATA_SET(net_backend_set, vale_backend);
