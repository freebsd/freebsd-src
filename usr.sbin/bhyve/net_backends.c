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

/*
 * This file implements multiple network backends (tap, netmap, ...),
 * to be used by network frontends such as virtio-net and e1000.
 * The API to access the backend (e.g. send/receive packets, negotiate
 * features) is exported by net_backends.h.
 */

#include <sys/types.h>
#ifndef WITHOUT_CAPSICUM
#include <sys/capsicum.h>
#endif
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/uio.h>

#include <net/if.h>
#include <net/if_tap.h>

#include <assert.h>
#ifndef WITHOUT_CAPSICUM
#include <capsicum_helpers.h>
#endif
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <pthread_np.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "config.h"
#include "debug.h"
#include "iov.h"
#include "mevent.h"
#include "net_backends.h"
#include "net_backends_priv.h"
#include "pci_emul.h"

#define	NET_BE_SIZE(be)		(sizeof(*be) + (be)->priv_size)

void
tap_cleanup(struct net_backend *be)
{
	struct tap_priv *priv = NET_BE_PRIV(be);

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
    nvlist_t *nvl __unused, net_be_rxeof_t cb, void *param)
{
	struct tap_priv *priv = NET_BE_PRIV(be);
	char tbuf[80];
	int opt = 1, up = IFF_UP;

#ifndef WITHOUT_CAPSICUM
	cap_rights_t rights;
#endif

	if (cb == NULL) {
		EPRINTLN("TAP backend requires non-NULL callback");
		return (-1);
	}

	strcpy(tbuf, "/dev/");
	strlcat(tbuf, devname, sizeof(tbuf));

	be->fd = open(tbuf, O_RDWR);
	if (be->fd == -1) {
		EPRINTLN("open of tap device %s failed", tbuf);
		goto error;
	}

	/*
	 * Set non-blocking and register for read
	 * notifications with the event loop
	 */
	if (ioctl(be->fd, FIONBIO, &opt) < 0) {
		EPRINTLN("tap device O_NONBLOCK failed");
		goto error;
	}

	if (ioctl(be->fd, VMIO_SIOCSIFFLAGS, up)) {
		EPRINTLN("tap device link up failed");
		goto error;
	}

#ifndef WITHOUT_CAPSICUM
	cap_rights_init(&rights, CAP_EVENT, CAP_READ, CAP_WRITE);
	if (caph_rights_limit(be->fd, &rights) == -1)
		errx(EX_OSERR, "Unable to apply rights for sandbox");
#endif

	memset(priv->bbuf, 0, sizeof(priv->bbuf));
	priv->bbuflen = 0;

	priv->mevp = mevent_add_disabled(be->fd, EVF_READ, cb, param);
	if (priv->mevp == NULL) {
		EPRINTLN("Could not register event");
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
ssize_t
tap_send(struct net_backend *be, const struct iovec *iov, int iovcnt)
{
	return (writev(be->fd, iov, iovcnt));
}

ssize_t
tap_peek_recvlen(struct net_backend *be)
{
	struct tap_priv *priv = NET_BE_PRIV(be);
	ssize_t ret;

	if (priv->bbuflen > 0) {
		/*
		 * We already have a packet in the bounce buffer.
		 * Just return its length.
		 */
		return priv->bbuflen;
	}

	/*
	 * Read the next packet (if any) into the bounce buffer, so
	 * that we get to know its length and we can return that
	 * to the caller.
	 */
	ret = read(be->fd, priv->bbuf, sizeof(priv->bbuf));
	if (ret < 0 && errno == EWOULDBLOCK) {
		return (0);
	}

	if (ret > 0)
		priv->bbuflen = ret;

	return (ret);
}

ssize_t
tap_recv(struct net_backend *be, const struct iovec *iov, int iovcnt)
{
	struct tap_priv *priv = NET_BE_PRIV(be);
	ssize_t ret;

	if (priv->bbuflen > 0) {
		/*
		 * A packet is available in the bounce buffer, so
		 * we read it from there.
		 */
		ret = buf_to_iov(priv->bbuf, priv->bbuflen,
		    iov, iovcnt, 0);

		/* Mark the bounce buffer as empty. */
		priv->bbuflen = 0;

		return (ret);
	}

	ret = readv(be->fd, iov, iovcnt);
	if (ret < 0 && errno == EWOULDBLOCK) {
		return (0);
	}

	return (ret);
}

void
tap_recv_enable(struct net_backend *be)
{
	struct tap_priv *priv = NET_BE_PRIV(be);

	mevent_enable(priv->mevp);
}

void
tap_recv_disable(struct net_backend *be)
{
	struct tap_priv *priv = NET_BE_PRIV(be);

	mevent_disable(priv->mevp);
}

uint64_t
tap_get_cap(struct net_backend *be __unused)
{

	return (0); /* no capabilities for now */
}

int
tap_set_cap(struct net_backend *be __unused, uint64_t features,
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
	.peek_recvlen = tap_peek_recvlen,
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
	.peek_recvlen = tap_peek_recvlen,
	.recv = tap_recv,
	.recv_enable = tap_recv_enable,
	.recv_disable = tap_recv_disable,
	.get_cap = tap_get_cap,
	.set_cap = tap_set_cap,
};

DATA_SET(net_backend_set, tap_backend);
DATA_SET(net_backend_set, vmnet_backend);

int
netbe_legacy_config(nvlist_t *nvl, const char *opts)
{
	char *backend, *cp;

	if (opts == NULL)
		return (0);

	cp = strchr(opts, ',');
	if (cp == NULL) {
		set_config_value_node(nvl, "backend", opts);
		return (0);
	}
	backend = strndup(opts, cp - opts);
	set_config_value_node(nvl, "backend", backend);
	free(backend);
	return (pci_parse_legacy_config(nvl, cp + 1));
}

/*
 * Initialize a backend and attach to the frontend.
 * This is called during frontend initialization.
 *  @ret is a pointer to the backend to be initialized
 *  @devname is the backend-name as supplied on the command line,
 * 	e.g. -s 2:0,frontend-name,backend-name[,other-args]
 *  @cb is the receive callback supplied by the frontend,
 *	and it is invoked in the event loop when a receive
 *	event is generated in the hypervisor,
 *  @param is a pointer to the frontend, and normally used as
 *	the argument for the callback.
 */
int
netbe_init(struct net_backend **ret, nvlist_t *nvl, net_be_rxeof_t cb,
    void *param)
{
	struct net_backend **pbe, *nbe, *tbe = NULL;
	const char *value, *type;
	char *devname;
	int err;

	value = get_config_value_node(nvl, "backend");
	if (value == NULL) {
		return (-1);
	}
	devname = strdup(value);

	/*
	 * Use the type given by configuration if exists; otherwise
	 * use the prefix of the backend as the type.
	 */
	type = get_config_value_node(nvl, "type");
	if (type == NULL)
		type = devname;

	/*
	 * Find the network backend that matches the user-provided
	 * device name. net_backend_set is built using a linker set.
	 */
	SET_FOREACH(pbe, net_backend_set) {
		if (strncmp(type, (*pbe)->prefix,
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
	if (tbe == NULL) {
		free(devname);
		return (EINVAL);
	}

	nbe = calloc(1, NET_BE_SIZE(tbe));
	*nbe = *tbe;	/* copy the template */
	nbe->fd = -1;
	nbe->sc = param;
	nbe->be_vnet_hdr_len = 0;
	nbe->fe_vnet_hdr_len = 0;

	/* Initialize the backend. */
	err = nbe->init(nbe, devname, nvl, cb, param);
	if (err) {
		free(devname);
		free(nbe);
		return (err);
	}

	*ret = nbe;
	free(devname);

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

ssize_t
netbe_send(struct net_backend *be, const struct iovec *iov, int iovcnt)
{

	return (be->send(be, iov, iovcnt));
}

ssize_t
netbe_peek_recvlen(struct net_backend *be)
{

	return (be->peek_recvlen(be));
}

/*
 * Try to read a packet from the backend, without blocking.
 * If no packets are available, return 0. In case of success, return
 * the length of the packet just read. Return -1 in case of errors.
 */
ssize_t
netbe_recv(struct net_backend *be, const struct iovec *iov, int iovcnt)
{

	return (be->recv(be, iov, iovcnt));
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

size_t
netbe_get_vnet_hdr_len(struct net_backend *be)
{

	return (be->be_vnet_hdr_len);
}
