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

#ifndef WITHOUT_CAPSICUM
#include <sys/capsicum.h>
#endif
#include <sys/socket.h>
#include <sys/sysctl.h>

#ifndef WITHOUT_CAPSICUM
#include <capsicum_helpers.h>
#endif
#include <err.h>
#include <netgraph.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "config.h"
#include "debug.h"
#include "net_backends.h"
#include "net_backends_priv.h"

#define NG_SBUF_MAX_SIZE (4 * 1024 * 1024)

static int
ng_init(struct net_backend *be, const char *devname __unused,
	 nvlist_t *nvl, net_be_rxeof_t cb, void *param)
{
	struct tap_priv *p = NET_BE_PRIV(be);
	struct ngm_connect ngc;
	const char *value, *nodename;
	int sbsz;
	int ctrl_sock;
	int flags;
	unsigned long maxsbsz;
	size_t msbsz;
#ifndef WITHOUT_CAPSICUM
	cap_rights_t rights;
#endif

	if (cb == NULL) {
		EPRINTLN("Netgraph backend requires non-NULL callback");
		return (-1);
	}

	be->fd = -1;

	memset(&ngc, 0, sizeof(ngc));

	value = get_config_value_node(nvl, "path");
	if (value == NULL) {
		EPRINTLN("path must be provided");
		return (-1);
	}
	strncpy(ngc.path, value, NG_PATHSIZ - 1);

	value = get_config_value_node(nvl, "hook");
	if (value == NULL)
		value = "vmlink";
	strncpy(ngc.ourhook, value, NG_HOOKSIZ - 1);

	value = get_config_value_node(nvl, "peerhook");
	if (value == NULL) {
		EPRINTLN("peer hook must be provided");
		return (-1);
	}
	strncpy(ngc.peerhook, value, NG_HOOKSIZ - 1);

	nodename = get_config_value_node(nvl, "socket");
	if (NgMkSockNode(nodename,
		&ctrl_sock, &be->fd) < 0) {
		EPRINTLN("can't get Netgraph sockets");
		return (-1);
	}

	if (NgSendMsg(ctrl_sock, ".",
		NGM_GENERIC_COOKIE,
		NGM_CONNECT, &ngc, sizeof(ngc)) < 0) {
		EPRINTLN("can't connect to node");
		close(ctrl_sock);
		goto error;
	}

	close(ctrl_sock);

	flags = fcntl(be->fd, F_GETFL);

	if (flags < 0) {
		EPRINTLN("can't get socket flags");
		goto error;
	}

	if (fcntl(be->fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		EPRINTLN("can't set O_NONBLOCK flag");
		goto error;
	}

	/*
	 * The default ng_socket(4) buffer's size is too low.
	 * Calculate the minimum value between NG_SBUF_MAX_SIZE
	 * and kern.ipc.maxsockbuf.
	 */
	msbsz = sizeof(maxsbsz);
	if (sysctlbyname("kern.ipc.maxsockbuf", &maxsbsz, &msbsz,
		NULL, 0) < 0) {
		EPRINTLN("can't get 'kern.ipc.maxsockbuf' value");
		goto error;
	}

	/*
	 * We can't set the socket buffer size to kern.ipc.maxsockbuf value,
	 * as it takes into account the mbuf(9) overhead.
	 */
	maxsbsz = maxsbsz * MCLBYTES / (MSIZE + MCLBYTES);

	sbsz = MIN(NG_SBUF_MAX_SIZE, maxsbsz);

	if (setsockopt(be->fd, SOL_SOCKET, SO_SNDBUF, &sbsz,
		sizeof(sbsz)) < 0) {
		EPRINTLN("can't set TX buffer size");
		goto error;
	}

	if (setsockopt(be->fd, SOL_SOCKET, SO_RCVBUF, &sbsz,
		sizeof(sbsz)) < 0) {
		EPRINTLN("can't set RX buffer size");
		goto error;
	}

#ifndef WITHOUT_CAPSICUM
	cap_rights_init(&rights, CAP_EVENT, CAP_READ, CAP_WRITE);
	if (caph_rights_limit(be->fd, &rights) == -1)
		errx(EX_OSERR, "Unable to apply rights for sandbox");
#endif

	memset(p->bbuf, 0, sizeof(p->bbuf));
	p->bbuflen = 0;

	p->mevp = mevent_add_disabled(be->fd, EVF_READ, cb, param);
	if (p->mevp == NULL) {
		EPRINTLN("Could not register event");
		goto error;
	}

	return (0);

error:
	tap_cleanup(be);
	return (-1);
}

static struct net_backend ng_backend = {
	.prefix = "netgraph",
	.priv_size = sizeof(struct tap_priv),
	.init = ng_init,
	.cleanup = tap_cleanup,
	.send = tap_send,
	.peek_recvlen = tap_peek_recvlen,
	.recv = tap_recv,
	.recv_enable = tap_recv_enable,
	.recv_disable = tap_recv_disable,
	.get_cap = tap_get_cap,
	.set_cap = tap_set_cap,
};

DATA_SET(net_backend_set, ng_backend);
