/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
 *
 * $FreeBSD$
 */

/*
 * This file implements multiple network backends (tap, netmap, ...),
 * to be used by network frontends such as virtio-net and e1000.
 * The API to access the backend (e.g. send/receive packets, negotiate
 * features) is exported by net_backends.h.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>		/* u_short etc */
#ifndef WITHOUT_CAPSICUM
#include <sys/capsicum.h>
#endif
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/uio.h>

#include <net/if.h>
#include <net/netmap.h>
#include <net/netmap_virt.h>
#define NETMAP_WITH_LIBS
#include <net/netmap_user.h>

#ifndef WITHOUT_CAPSICUM
#include <capsicum_helpers.h>
#endif
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sysexits.h>
#include <assert.h>
#include <pthread.h>
#include <pthread_np.h>
#include <poll.h>
#include <assert.h>


#include "iov.h"
#include "mevent.h"
#include "net_backends.h"

#include <sys/linker_set.h>

/*
 * Each network backend registers a set of function pointers that are
 * used to implement the net backends API.
 * This might need to be exposed if we implement backends in separate files.
 */
struct net_backend {
	const char *prefix;	/* prefix matching this backend */

	/*
	 * Routines used to initialize and cleanup the resources needed
	 * by a backend. The cleanup function is used internally,
	 * and should not be called by the frontend.
	 */
	int (*init)(struct net_backend *be, const char *devname,
	    net_be_rxeof_t cb, void *param);
	void (*cleanup)(struct net_backend *be);

	/*
	 * Called to serve a guest transmit request. The scatter-gather
	 * vector provided by the caller has 'iovcnt' elements and contains
	 * the packet to send.
	 */
	ssize_t (*send)(struct net_backend *be, struct iovec *iov, int iovcnt);

	/*
	 * Called to receive a packet from the backend. When the function
	 * returns a positive value 'len', the scatter-gather vector
	 * provided by the caller contains a packet with such length.
	 * The function returns 0 if the backend doesn't have a new packet to
	 * receive.
	 */
	ssize_t (*recv)(struct net_backend *be, struct iovec *iov, int iovcnt);

	/*
	 * Ask the backend to enable or disable receive operation in the
	 * backend. On return from a disable operation, it is guaranteed
	 * that the receive callback won't be called until receive is
	 * enabled again. Note however that it is up to the caller to make
	 * sure that netbe_recv() is not currently being executed by another
	 * thread.
	 */
	void (*recv_enable)(struct net_backend *be);
	void (*recv_disable)(struct net_backend *be);

	/*
	 * Ask the backend for the virtio-net features it is able to
	 * support. Possible features are TSO, UFO and checksum offloading
	 * in both rx and tx direction and for both IPv4 and IPv6.
	 */
	uint64_t (*get_cap)(struct net_backend *be);

	/*
	 * Tell the backend to enable/disable the specified virtio-net
	 * features (capabilities).
	 */
	int (*set_cap)(struct net_backend *be, uint64_t features,
	    unsigned int vnet_hdr_len);

	struct pci_vtnet_softc *sc;
	int fd;

	/*
	 * Length of the virtio-net header used by the backend and the
	 * frontend, respectively. A zero value means that the header
	 * is not used.
	 */
	unsigned int be_vnet_hdr_len;
	unsigned int fe_vnet_hdr_len;

	/* Size of backend-specific private data. */
	size_t priv_size;

	/* Room for backend-specific data. */
	char opaque[0];
};

SET_DECLARE(net_backend_set, struct net_backend);

#define VNET_HDR_LEN	sizeof(struct virtio_net_rxhdr)

#define WPRINTF(params) printf params

/*
 * The tap backend
 */

struct tap_priv {
	struct mevent *mevp;
};

static void
tap_cleanup(struct net_backend *be)
{
	struct tap_priv *priv = (struct tap_priv *)be->opaque;

	if (priv->mevp) {
		mevent_delete(priv->mevp);
	}
	if (be->fd != -1) {
		close(be->fd);
		be->fd = -1;
	}
}

static int
tap_init(struct net_backend *be, const char *devname,
	 net_be_rxeof_t cb, void *param)
{
	struct tap_priv *priv = (struct tap_priv *)be->opaque;
	char tbuf[80];
	int opt = 1;
#ifndef WITHOUT_CAPSICUM
	cap_rights_t rights;
#endif

	if (cb == NULL) {
		WPRINTF(("TAP backend requires non-NULL callback\n\r"));
		return (-1);
	}

	strcpy(tbuf, "/dev/");
	strlcat(tbuf, devname, sizeof(tbuf));

	be->fd = open(tbuf, O_RDWR);
	if (be->fd == -1) {
		WPRINTF(("open of tap device %s failed\n\r", tbuf));
		goto error;
	}

	/*
	 * Set non-blocking and register for read
	 * notifications with the event loop
	 */
	if (ioctl(be->fd, FIONBIO, &opt) < 0) {
		WPRINTF(("tap device O_NONBLOCK failed\n\r"));
		goto error;
	}

#ifndef WITHOUT_CAPSICUM
	cap_rights_init(&rights, CAP_EVENT, CAP_READ, CAP_WRITE);
	if (caph_rights_limit(be->fd, &rights) == -1)
		errx(EX_OSERR, "Unable to apply rights for sandbox");
#endif

	priv->mevp = mevent_add_disabled(be->fd, EVF_READ, cb, param);
	if (priv->mevp == NULL) {
		WPRINTF(("Could not register event\n\r"));
		goto error;
	}

	return (0);

error:
	tap_cleanup(be);
	return (-1);
}

/*
 * Called to send a buffer chain out to the tap device
 */
static ssize_t
tap_send(struct net_backend *be, struct iovec *iov, int iovcnt)
{
	return (writev(be->fd, iov, iovcnt));
}

static ssize_t
tap_recv(struct net_backend *be, struct iovec *iov, int iovcnt)
{
	ssize_t ret;

	/* Should never be called without a valid tap fd */
	assert(be->fd != -1);

	ret = readv(be->fd, iov, iovcnt);

	if (ret < 0 && errno == EWOULDBLOCK) {
		return (0);
	}

	return (ret);
}

static void
tap_recv_enable(struct net_backend *be)
{
	struct tap_priv *priv = (struct tap_priv *)be->opaque;

	mevent_enable(priv->mevp);
}

static void
tap_recv_disable(struct net_backend *be)
{
	struct tap_priv *priv = (struct tap_priv *)be->opaque;

	mevent_disable(priv->mevp);
}

static uint64_t
tap_get_cap(struct net_backend *be)
{

	return (0); /* no capabilities for now */
}

static int
tap_set_cap(struct net_backend *be, uint64_t features,
		unsigned vnet_hdr_len)
{

	return ((features || vnet_hdr_len) ? -1 : 0);
}

static struct net_backend tap_backend = {
	.prefix = "tap",
	.priv_size = sizeof(struct tap_priv),
	.init = tap_init,
	.cleanup = tap_cleanup,
	.send = tap_send,
	.recv = tap_recv,
	.recv_enable = tap_recv_enable,
	.recv_disable = tap_recv_disable,
	.get_cap = tap_get_cap,
	.set_cap = tap_set_cap,
};

/* A clone of the tap backend, with a different prefix. */
static struct net_backend vmnet_backend = {
	.prefix = "vmnet",
	.priv_size = sizeof(struct tap_priv),
	.init = tap_init,
	.cleanup = tap_cleanup,
	.send = tap_send,
	.recv = tap_recv,
	.recv_enable = tap_recv_enable,
	.recv_disable = tap_recv_disable,
	.get_cap = tap_get_cap,
	.set_cap = tap_set_cap,
};

DATA_SET(net_backend_set, tap_backend);
DATA_SET(net_backend_set, vmnet_backend);

/*
 * The netmap backend
 */

/* The virtio-net features supported by netmap. */
#define NETMAP_FEATURES (VIRTIO_NET_F_CSUM | VIRTIO_NET_F_HOST_TSO4 | \
		VIRTIO_NET_F_HOST_TSO6 | VIRTIO_NET_F_HOST_UFO | \
		VIRTIO_NET_F_GUEST_CSUM | VIRTIO_NET_F_GUEST_TSO4 | \
		VIRTIO_NET_F_GUEST_TSO6 | VIRTIO_NET_F_GUEST_UFO | \
		VIRTIO_NET_F_MRG_RXBUF)

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
	struct netmap_priv *priv = (struct netmap_priv *)be->opaque;

	nmreq_init(&req, priv->ifname);
	req.nr_cmd = NETMAP_BDG_VNET_HDR;
	req.nr_arg1 = vnet_hdr_len;
	err = ioctl(be->fd, NIOCREGIF, &req);
	if (err) {
		WPRINTF(("Unable to set vnet header length %d\n\r",
				vnet_hdr_len));
		return (err);
	}

	be->be_vnet_hdr_len = vnet_hdr_len;

	return (0);
}

static int
netmap_has_vnet_hdr_len(struct net_backend *be, unsigned vnet_hdr_len)
{
	int prev_hdr_len = be->be_vnet_hdr_len;
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
netmap_set_cap(struct net_backend *be, uint64_t features,
	       unsigned vnet_hdr_len)
{

	return (netmap_set_vnet_hdr_len(be, vnet_hdr_len));
}

static int
netmap_init(struct net_backend *be, const char *devname,
	    net_be_rxeof_t cb, void *param)
{
	struct netmap_priv *priv = (struct netmap_priv *)be->opaque;

	strlcpy(priv->ifname, devname, sizeof(priv->ifname));
	priv->ifname[sizeof(priv->ifname) - 1] = '\0';

	priv->nmd = nm_open(priv->ifname, NULL, NETMAP_NO_TX_POLL, NULL);
	if (priv->nmd == NULL) {
		WPRINTF(("Unable to nm_open(): interface '%s', errno (%s)\n\r",
			devname, strerror(errno)));
		free(priv);
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
		WPRINTF(("Could not register event\n\r"));
		return (-1);
	}

	return (0);
}

static void
netmap_cleanup(struct net_backend *be)
{
	struct netmap_priv *priv = (struct netmap_priv *)be->opaque;

	if (priv->mevp) {
		mevent_delete(priv->mevp);
	}
	if (priv->nmd) {
		nm_close(priv->nmd);
	}
	be->fd = -1;
}

static ssize_t
netmap_send(struct net_backend *be, struct iovec *iov,
	    int iovcnt)
{
	struct netmap_priv *priv = (struct netmap_priv *)be->opaque;
	struct netmap_ring *ring;
	ssize_t totlen = 0;
	int nm_buf_size;
	int nm_buf_len;
	uint32_t head;
	void *nm_buf;
	int j;

	ring = priv->tx;
	head = ring->head;
	if (head == ring->tail) {
		WPRINTF(("No space, drop %zu bytes\n\r", count_iov(iov, iovcnt)));
		goto txsync;
	}
	nm_buf = NETMAP_BUF(ring, ring->slot[head].buf_idx);
	nm_buf_size = ring->nr_buf_size;
	nm_buf_len = 0;

	for (j = 0; j < iovcnt; j++) {
		int iov_frag_size = iov[j].iov_len;
		void *iov_frag_buf = iov[j].iov_base;

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
				WPRINTF(("No space, drop %zu bytes\n\r",
				   count_iov(iov, iovcnt)));
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
netmap_recv(struct net_backend *be, struct iovec *iov, int iovcnt)
{
	struct netmap_priv *priv = (struct netmap_priv *)be->opaque;
	struct netmap_slot *slot = NULL;
	struct netmap_ring *ring;
	void *iov_frag_buf;
	int iov_frag_size;
	ssize_t totlen = 0;
	uint32_t head;

	assert(iovcnt);

	ring = priv->rx;
	head = ring->head;
	iov_frag_buf = iov->iov_base;
	iov_frag_size = iov->iov_len;

	do {
		int nm_buf_len;
		void *nm_buf;

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
				WPRINTF(("Short iov, drop %zd bytes\n\r",
				    totlen));
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
	struct netmap_priv *priv = (struct netmap_priv *)be->opaque;

	mevent_enable(priv->mevp);
}

static void
netmap_recv_disable(struct net_backend *be)
{
	struct netmap_priv *priv = (struct netmap_priv *)be->opaque;

	mevent_disable(priv->mevp);
}

static struct net_backend netmap_backend = {
	.prefix = "netmap",
	.priv_size = sizeof(struct netmap_priv),
	.init = netmap_init,
	.cleanup = netmap_cleanup,
	.send = netmap_send,
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
	.recv = netmap_recv,
	.recv_enable = netmap_recv_enable,
	.recv_disable = netmap_recv_disable,
	.get_cap = netmap_get_cap,
	.set_cap = netmap_set_cap,
};

DATA_SET(net_backend_set, netmap_backend);
DATA_SET(net_backend_set, vale_backend);

/*
 * Initialize a backend and attach to the frontend.
 * This is called during frontend initialization.
 *  @pbe is a pointer to the backend to be initialized
 *  @devname is the backend-name as supplied on the command line,
 * 	e.g. -s 2:0,frontend-name,backend-name[,other-args]
 *  @cb is the receive callback supplied by the frontend,
 *	and it is invoked in the event loop when a receive
 *	event is generated in the hypervisor,
 *  @param is a pointer to the frontend, and normally used as
 *	the argument for the callback.
 */
int
netbe_init(struct net_backend **ret, const char *devname, net_be_rxeof_t cb,
    void *param)
{
	struct net_backend **pbe, *nbe, *tbe = NULL;
	int err;

	/*
	 * Find the network backend that matches the user-provided
	 * device name. net_backend_set is built using a linker set.
	 */
	SET_FOREACH(pbe, net_backend_set) {
		if (strncmp(devname, (*pbe)->prefix,
		    strlen((*pbe)->prefix)) == 0) {
			tbe = *pbe;
			assert(tbe->init != NULL);
			assert(tbe->cleanup != NULL);
			assert(tbe->send != NULL);
			assert(tbe->recv != NULL);
			assert(tbe->get_cap != NULL);
			assert(tbe->set_cap != NULL);
			break;
		}
	}

	*ret = NULL;
	if (tbe == NULL)
		return (EINVAL);
	nbe = calloc(1, sizeof(*nbe) + tbe->priv_size);
	*nbe = *tbe;	/* copy the template */
	nbe->fd = -1;
	nbe->sc = param;
	nbe->be_vnet_hdr_len = 0;
	nbe->fe_vnet_hdr_len = 0;

	/* Initialize the backend. */
	err = nbe->init(nbe, devname, cb, param);
	if (err) {
		free(nbe);
		return (err);
	}

	*ret = nbe;

	return (0);
}

void
netbe_cleanup(struct net_backend *be)
{

	if (be != NULL) {
		be->cleanup(be);
		free(be);
	}
}

uint64_t
netbe_get_cap(struct net_backend *be)
{

	assert(be != NULL);
	return (be->get_cap(be));
}

int
netbe_set_cap(struct net_backend *be, uint64_t features,
	      unsigned vnet_hdr_len)
{
	int ret;

	assert(be != NULL);

	/* There are only three valid lengths, i.e., 0, 10 and 12. */
	if (vnet_hdr_len && vnet_hdr_len != VNET_HDR_LEN
		&& vnet_hdr_len != (VNET_HDR_LEN - sizeof(uint16_t)))
		return (-1);

	be->fe_vnet_hdr_len = vnet_hdr_len;

	ret = be->set_cap(be, features, vnet_hdr_len);
	assert(be->be_vnet_hdr_len == 0 ||
	       be->be_vnet_hdr_len == be->fe_vnet_hdr_len);

	return (ret);
}

static __inline struct iovec *
iov_trim(struct iovec *iov, int *iovcnt, unsigned int tlen)
{
	struct iovec *riov;

	/* XXX short-cut: assume first segment is >= tlen */
	assert(iov[0].iov_len >= tlen);

	iov[0].iov_len -= tlen;
	if (iov[0].iov_len == 0) {
		assert(*iovcnt > 1);
		*iovcnt -= 1;
		riov = &iov[1];
	} else {
		iov[0].iov_base = (void *)((uintptr_t)iov[0].iov_base + tlen);
		riov = &iov[0];
	}

	return (riov);
}

ssize_t
netbe_send(struct net_backend *be, struct iovec *iov, int iovcnt)
{

	assert(be != NULL);
	if (be->be_vnet_hdr_len != be->fe_vnet_hdr_len) {
		/*
		 * The frontend uses a virtio-net header, but the backend
		 * does not. We ignore it (as it must be all zeroes) and
		 * strip it.
		 */
		assert(be->be_vnet_hdr_len == 0);
		iov = iov_trim(iov, &iovcnt, be->fe_vnet_hdr_len);
	}

	return (be->send(be, iov, iovcnt));
}

/*
 * Try to read a packet from the backend, without blocking.
 * If no packets are available, return 0. In case of success, return
 * the length of the packet just read. Return -1 in case of errors.
 */
ssize_t
netbe_recv(struct net_backend *be, struct iovec *iov, int iovcnt)
{
	/* Length of prepended virtio-net header. */
	unsigned int hlen = be->fe_vnet_hdr_len;
	int ret;

	assert(be != NULL);

	if (hlen && hlen != be->be_vnet_hdr_len) {
		/*
		 * The frontend uses a virtio-net header, but the backend
		 * does not. We need to prepend a zeroed header.
		 */
		struct virtio_net_rxhdr *vh;

		assert(be->be_vnet_hdr_len == 0);

		/*
		 * Get a pointer to the rx header, and use the
		 * data immediately following it for the packet buffer.
		 */
		vh = iov[0].iov_base;
		iov = iov_trim(iov, &iovcnt, hlen);

		/*
		 * The only valid field in the rx packet header is the
		 * number of buffers if merged rx bufs were negotiated.
		 */
		memset(vh, 0, hlen);
		if (hlen == VNET_HDR_LEN) {
			vh->vrh_bufs = 1;
		}
	}

	ret = be->recv(be, iov, iovcnt);
	if (ret > 0) {
		ret += hlen;
	}

	return (ret);
}

/*
 * Read a packet from the backend and discard it.
 * Returns the size of the discarded packet or zero if no packet was available.
 * A negative error code is returned in case of read error.
 */
ssize_t
netbe_rx_discard(struct net_backend *be)
{
	/*
	 * MP note: the dummybuf is only used to discard frames,
	 * so there is no need for it to be per-vtnet or locked.
	 * We only make it large enough for TSO-sized segment.
	 */
	static uint8_t dummybuf[65536 + 64];
	struct iovec iov;

	iov.iov_base = dummybuf;
	iov.iov_len = sizeof(dummybuf);

	return netbe_recv(be, &iov, 1);
}

void
netbe_rx_disable(struct net_backend *be)
{

	return be->recv_disable(be);
}

void
netbe_rx_enable(struct net_backend *be)
{

	return be->recv_enable(be);
}
