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

#ifndef __NET_BACKENDS_PRIV_H__
#define __NET_BACKENDS_PRIV_H__

#include <sys/linker_set.h>

/*
 * Each network backend registers a set of function pointers that are
 * used to implement the net backends API.  Frontends should not invoke
 * these functions directly, but should instead use the interface provided by
 * net_backends.h.
 */
struct net_backend {
	const char *prefix;	/* prefix matching this backend */

	/*
	 * Routines used to initialize and cleanup the resources needed
	 * by a backend. The cleanup function is used internally,
	 * and should not be called by the frontend.
	 */
	int (*init)(struct net_backend *be, const char *devname,
	    nvlist_t *nvl, net_be_rxeof_t cb, void *param);
	void (*cleanup)(struct net_backend *be);

	/*
	 * Called to serve a guest transmit request. The scatter-gather
	 * vector provided by the caller has 'iovcnt' elements and contains
	 * the packet to send.
	 */
	ssize_t (*send)(struct net_backend *be, const struct iovec *iov,
	    int iovcnt);

	/*
	 * Get the length of the next packet that can be received from
	 * the backend. If no packets are currently available, this
	 * function returns 0.
	 */
	ssize_t (*peek_recvlen)(struct net_backend *be);

	/*
	 * Called to receive a packet from the backend. When the function
	 * returns a positive value 'len', the scatter-gather vector
	 * provided by the caller contains a packet with such length.
	 * The function returns 0 if the backend doesn't have a new packet to
	 * receive.
	 */
	ssize_t (*recv)(struct net_backend *be, const struct iovec *iov,
	    int iovcnt);

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

	/* Backend-specific private data follows. */
};

#define	NET_BE_PRIV(be)		((void *)((be) + 1))

SET_DECLARE(net_backend_set, struct net_backend);

#define VNET_HDR_LEN	sizeof(struct virtio_net_rxhdr)

/*
 * Export the tap backend routines for the benefit of other backends which have
 * a similar interface to the kernel, i.e., they send and receive data using
 * standard I/O system calls with a single file descriptor.
 */

struct tap_priv {
	struct mevent *mevp;
	/*
	 * A bounce buffer that allows us to implement the peek_recvlen
	 * callback. In the future we may get the same information from
	 * the kevent data.
	 */
	char bbuf[1 << 16];
	ssize_t bbuflen;
};

void	tap_cleanup(struct net_backend *be);
ssize_t	tap_send(struct net_backend *be, const struct iovec *iov, int iovcnt);
ssize_t	tap_recv(struct net_backend *be, const struct iovec *iov, int iovcnt);
ssize_t	tap_peek_recvlen(struct net_backend *be);
void	tap_recv_enable(struct net_backend *be);
ssize_t	tap_recv(struct net_backend *be, const struct iovec *iov, int iovcnt);
void	tap_recv_disable(struct net_backend *be);
uint64_t tap_get_cap(struct net_backend *be);
int	tap_set_cap(struct net_backend *be, uint64_t features,
	    unsigned vnet_hdr_len);

#endif /* !__NET_BACKENDS_PRIV_H__ */
