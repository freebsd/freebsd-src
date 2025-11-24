/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023, 2025 Mark Johnston <markj@FreeBSD.org>
 *
 * This software was developed by the University of Cambridge Computer
 * Laboratory (Department of Computer Science and Technology) under Innovate
 * UK project 105694, "Digital Security by Design (DSbD) Technology Platform
 * Prototype".
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * The slirp backend enables unprivileged userspace networking via libslirp,
 * which must be installed on the host system via pkg or the ports tree.
 * libslirp.so is dlopen()ed into a helper process with which this backend
 * communicates.
 *
 * Packets received from the guest (i.e., transmitted by the frontend, such as a
 * virtio NIC device model) are injected into the slirp backend via slirp_send(),
 * which sends the packet to the helper process.
 *
 * Packets to be transmitted to the guest (i.e., inserted into the frontend's
 * receive buffers) are buffered in a per-interface socket pair and read by the
 * mevent loop.  Sockets instantiated by libslirp are monitored by a thread
 * which uses poll() and slirp_pollfds_poll() to drive libslirp events; this
 * thread also handles timeout events from the libslirp context.
 */

#include <sys/socket.h>
#include <sys/wait.h>

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "debug.h"
#include "mevent.h"
#include "net_backends.h"
#include "net_backends_priv.h"

#define	SLIRP_MTU	2048

struct slirp_priv {
	int s;
	pid_t helper;
	struct mevent *mevp;
};

extern char **environ;

static int
slirp_init(struct net_backend *be, const char *devname __unused,
    nvlist_t *nvl, net_be_rxeof_t cb, void *param)
{
	struct slirp_priv *priv = NET_BE_PRIV(be);
	nvlist_t *config;
	posix_spawn_file_actions_t fa;
	pid_t child;
	const char **argv;
	char sockname[32];
	int error, s[2];

	if (socketpair(PF_LOCAL, SOCK_SEQPACKET | SOCK_NONBLOCK, 0, s) != 0) {
		EPRINTLN("socketpair");
		return (-1);
	}

	/*
	 * The child will exit once its connection goes away, so make sure only
	 * one end is inherited by the child.
	 */
	if (posix_spawn_file_actions_init(&fa) != 0) {
		EPRINTLN("posix_spawn_file_actions_init");
		goto err;
	}
	if (posix_spawn_file_actions_addclose(&fa, s[0]) != 0) {
		EPRINTLN("posix_spawn_file_actions_addclose");
		posix_spawn_file_actions_destroy(&fa);
		goto err;
	}

	(void)snprintf(sockname, sizeof(sockname), "%d", s[1]);
	argv = (const char *[]){
	    "/usr/libexec/bhyve-slirp-helper", "-S", sockname, NULL
	};
	error = posix_spawn(&child, "/usr/libexec/bhyve-slirp-helper",
	    &fa, NULL, __DECONST(char **, argv), environ);
	posix_spawn_file_actions_destroy(&fa);
	if (error != 0) {
		EPRINTLN("posix_spawn(bhyve-slirp-helper): %s",
		    strerror(error));
		goto err;
	}

	config = nvlist_clone(nvl);
	if (config == NULL) {
		EPRINTLN("nvlist_clone");
		goto err;
	}
	nvlist_add_string(config, "vmname", get_config_value("name"));
	error = nvlist_send(s[0], config);
	nvlist_destroy(config);
	if (error != 0) {
		EPRINTLN("nvlist_send");
		goto err;
	}

	be->fd = s[0];
	priv->mevp = mevent_add_disabled(be->fd, EVF_READ, cb, param);
	if (priv->mevp == NULL) {
		EPRINTLN("Could not register event");
		goto err;
	}

	priv->helper = child;
	priv->s = s[0];
	(void)close(s[1]);

	return (0);

err:
	(void)close(s[0]);
	(void)close(s[1]);
	return (-1);
}

static ssize_t
slirp_send(struct net_backend *be, const struct iovec *iov, int iovcnt)
{
	struct slirp_priv *priv = NET_BE_PRIV(be);
	struct msghdr hdr;

	memset(&hdr, 0, sizeof(hdr));
	hdr.msg_iov = __DECONST(struct iovec *, iov);
	hdr.msg_iovlen = iovcnt;
	return (sendmsg(priv->s, &hdr, MSG_EOR));
}

static void
slirp_cleanup(struct net_backend *be)
{
	struct slirp_priv *priv = NET_BE_PRIV(be);

	if (priv->helper > 0) {
		int status;

		if (kill(priv->helper, SIGKILL) != 0) {
			EPRINTLN("kill(bhyve-slirp-helper): %s",
			    strerror(errno));
			return;
		}
		(void)waitpid(priv->helper, &status, 0);
	}
}

static ssize_t
slirp_peek_recvlen(struct net_backend *be)
{
	struct slirp_priv *priv = NET_BE_PRIV(be);
	uint8_t buf[SLIRP_MTU];
	ssize_t n;

	/*
	 * Copying into the buffer is totally unnecessary, but we don't
	 * implement MSG_TRUNC for SEQPACKET sockets.
	 */
	n = recv(priv->s, buf, sizeof(buf), MSG_PEEK | MSG_DONTWAIT);
	if (n < 0)
		return (errno == EWOULDBLOCK ? 0 : -1);
	assert((size_t)n <= SLIRP_MTU);
	return (n);
}

static ssize_t
slirp_recv(struct net_backend *be, const struct iovec *iov, int iovcnt)
{
	struct slirp_priv *priv = NET_BE_PRIV(be);
	struct msghdr hdr;
	ssize_t n;

	hdr.msg_name = NULL;
	hdr.msg_namelen = 0;
	hdr.msg_iov = __DECONST(struct iovec *, iov);
	hdr.msg_iovlen = iovcnt;
	hdr.msg_control = NULL;
	hdr.msg_controllen = 0;
	hdr.msg_flags = 0;
	n = recvmsg(priv->s, &hdr, MSG_DONTWAIT);
	if (n < 0) {
		if (errno == EWOULDBLOCK)
			return (0);
		return (-1);
	}
	assert(n <= SLIRP_MTU);
	return (n);
}

static void
slirp_recv_enable(struct net_backend *be)
{
	struct slirp_priv *priv = NET_BE_PRIV(be);

	mevent_enable(priv->mevp);
}

static void
slirp_recv_disable(struct net_backend *be)
{
	struct slirp_priv *priv = NET_BE_PRIV(be);

	mevent_disable(priv->mevp);
}

static uint64_t
slirp_get_cap(struct net_backend *be __unused)
{
	return (0);
}

static int
slirp_set_cap(struct net_backend *be __unused, uint64_t features __unused,
    unsigned int vnet_hdr_len __unused)
{
	return ((features || vnet_hdr_len) ? -1 : 0);
}

static struct net_backend slirp_backend = {
	.prefix = "slirp",
	.priv_size = sizeof(struct slirp_priv),
	.init = slirp_init,
	.cleanup = slirp_cleanup,
	.send = slirp_send,
	.peek_recvlen = slirp_peek_recvlen,
	.recv = slirp_recv,
	.recv_enable = slirp_recv_enable,
	.recv_disable = slirp_recv_disable,
	.get_cap = slirp_get_cap,
	.set_cap = slirp_set_cap,
};

DATA_SET(net_backend_set, slirp_backend);
