/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Mark Johnston <markj@FreeBSD.org>
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
 * The slirp backend enables unprivileged networking via libslirp, which must be
 * installed on the host system via pkg or the ports tree.  bhyve dlopen()s
 * libslirp.so upon instantiating the slirp backend.  Various network parameters
 * are hard-coded in _slirp_init().
 *
 * Packets received from the guest (i.e., transmitted by the frontend, such as a
 * virtio NIC device model) are injected into the slirp backend via slirp_send().
 * Packets to be transmitted to the guest (i.e., inserted into the frontend's
 * receive buffers) are buffered in a per-interface socket pair and read by the
 * mevent loop.  Sockets instantiated by libslirp are monitored by a thread
 * which uses poll() and slirp_pollfds_poll() to drive libslirp events; this
 * thread also handles timeout events from the libslirp context.
 */

#include <sys/socket.h>

#include <assert.h>
#include <capsicum_helpers.h>
#include <dlfcn.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <pthread_np.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "debug.h"
#include "libslirp.h"
#include "mevent.h"
#include "net_backends.h"
#include "net_backends_priv.h"

typedef int (*slirp_add_hostxfwd_p_t)(Slirp *,
    const struct sockaddr *, socklen_t, const struct sockaddr *, socklen_t,
    int);
typedef void (*slirp_cleanup_p_t)(Slirp *);
typedef void (*slirp_input_p_t)(Slirp *, const uint8_t *, int);
typedef Slirp *(*slirp_new_p_t)(const SlirpConfig *, const SlirpCb *, void *);
typedef void (*slirp_pollfds_fill_p_t)(Slirp *, uint32_t *timeout,
    SlirpAddPollCb, void *);
typedef void (*slirp_pollfds_poll_p_t)(Slirp *, int, SlirpGetREventsCb, void *);

/* Function pointer table, initialized by slirp_init_once(). */
static slirp_add_hostxfwd_p_t slirp_add_hostxfwd_p;
static slirp_cleanup_p_t slirp_cleanup_p;
static slirp_input_p_t slirp_input_p;
static slirp_new_p_t slirp_new_p;
static slirp_pollfds_fill_p_t slirp_pollfds_fill_p;
static slirp_pollfds_poll_p_t slirp_pollfds_poll_p;

static void
checked_close(int *fdp)
{
	int error;

	if (*fdp != -1) {
		error = close(*fdp);
		assert(error == 0);
		*fdp = -1;
	}
}

static int
slirp_init_once(void)
{
	static void *handle = NULL;

	if (handle != NULL)
		return (0);
	handle = dlopen("libslirp.so.0", RTLD_LAZY);
	if (handle == NULL) {
		EPRINTLN("Unable to open libslirp.so.0: %s", dlerror());
		return (-1);
	}

#define IMPORT_SYM(sym) do {					\
	sym##_p = (sym##_p_t)dlsym(handle, #sym);		\
	if (sym##_p == NULL) {					\
		EPRINTLN("failed to resolve %s", #sym);		\
		goto err;					\
	}							\
} while (0)
	IMPORT_SYM(slirp_add_hostxfwd);
	IMPORT_SYM(slirp_cleanup);
	IMPORT_SYM(slirp_input);
	IMPORT_SYM(slirp_new);
	IMPORT_SYM(slirp_pollfds_fill);
	IMPORT_SYM(slirp_pollfds_poll);
#undef IMPORT_SYM

	/*
	 * libslirp uses glib, which uses tzdata to format log messages.  Help
	 * it out.
	 *
	 * XXX-MJ glib will also look for charset files, not sure what we can do
	 * about that...
	 */
	caph_cache_tzdata();

	return (0);

err:
	dlclose(handle);
	handle = NULL;
	return (-1);
}

struct slirp_priv {
	Slirp *slirp;

#define	SLIRP_MTU	2048
	struct mevent *mevp;
	int pipe[2];		/* used to buffer data sent to the guest */
	int wakeup[2];		/* used to wake up the pollfd thread */

	pthread_t pollfd_td;
	struct pollfd *pollfds;
	size_t npollfds;

	/* Serializes libslirp calls. */
	pthread_mutex_t mtx;
};

static void
slirp_priv_init(struct slirp_priv *priv)
{
	int error;

	memset(priv, 0, sizeof(*priv));
	priv->pipe[0] = priv->pipe[1] = -1;
	priv->wakeup[0] = priv->wakeup[1] = -1;
	error = pthread_mutex_init(&priv->mtx, NULL);
	assert(error == 0);
}

static void
slirp_priv_cleanup(struct slirp_priv *priv)
{
	int error;

	checked_close(&priv->pipe[0]);
	checked_close(&priv->pipe[1]);
	checked_close(&priv->wakeup[0]);
	checked_close(&priv->wakeup[1]);
	if (priv->mevp)
		mevent_delete(priv->mevp);
	if (priv->slirp != NULL)
		slirp_cleanup_p(priv->slirp);
	error = pthread_mutex_destroy(&priv->mtx);
	assert(error == 0);
}

static int64_t
slirp_cb_clock_get_ns(void *param __unused)
{
	struct timespec ts;
	int error;

	error = clock_gettime(CLOCK_MONOTONIC, &ts);
	assert(error == 0);
	return ((int64_t)(ts.tv_sec * 1000000000L + ts.tv_nsec));
}

static void
slirp_cb_notify(void *param)
{
	struct slirp_priv *priv;

	/* Wake up the poll thread.  We assume that priv->mtx is held here. */
	priv = param;
	(void)write(priv->wakeup[1], "M", 1);
}

static void
slirp_cb_register_poll_fd(int fd, void *param __unused)
{
	const int one = 1;

	(void)setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(int));
}

static ssize_t
slirp_cb_send_packet(const void *buf, size_t len, void *param)
{
	struct slirp_priv *priv;
	ssize_t n;

	priv = param;

	assert(len <= SLIRP_MTU);
	n = send(priv->pipe[1], buf, len, 0);
	if (n < 0) {
		EPRINTLN("slirp_cb_send_packet: send: %s", strerror(errno));
		return (n);
	}
	assert((size_t)n == len);

	return (n);
}

static void
slirp_cb_unregister_poll_fd(int fd __unused, void *opaque __unused)
{
}

/* Callbacks invoked from within libslirp. */
static const struct SlirpCb slirp_cbs = {
	.clock_get_ns = slirp_cb_clock_get_ns,
	.notify = slirp_cb_notify,
	.register_poll_fd = slirp_cb_register_poll_fd,
	.send_packet = slirp_cb_send_packet,
	.unregister_poll_fd = slirp_cb_unregister_poll_fd,
};

static int
slirpev2pollev(int events)
{
	int ret;

	ret = 0;
	if (events & SLIRP_POLL_IN)
		ret |= POLLIN;
	if (events & SLIRP_POLL_OUT)
		ret |= POLLOUT;
	if (events & SLIRP_POLL_PRI)
		ret |= POLLPRI;
	if (events & SLIRP_POLL_ERR)
		ret |= POLLERR;
	if (events & SLIRP_POLL_HUP)
		ret |= POLLHUP;
	return (ret);
}

static int
pollev2slirpev(int events)
{
	int ret;

	ret = 0;
	if (events & POLLIN)
		ret |= SLIRP_POLL_IN;
	if (events & POLLOUT)
		ret |= SLIRP_POLL_OUT;
	if (events & POLLPRI)
		ret |= SLIRP_POLL_PRI;
	if (events & POLLERR)
		ret |= SLIRP_POLL_ERR;
	if (events & POLLHUP)
		ret |= SLIRP_POLL_HUP;
	return (ret);
}

static int
slirp_addpoll_cb(int fd, int events, void *param)
{
	struct slirp_priv *priv;
	struct pollfd *pollfd, *pollfds;
	size_t i;

	priv = param;

	for (i = 0; i < priv->npollfds; i++)
		if (priv->pollfds[i].fd == -1)
			break;
	if (i == priv->npollfds) {
		const size_t POLLFD_GROW = 4;

		priv->npollfds += POLLFD_GROW;
		pollfds = realloc(priv->pollfds,
		    sizeof(*pollfds) * priv->npollfds);
		if (pollfds == NULL)
			return (-1);
		for (i = priv->npollfds - POLLFD_GROW; i < priv->npollfds; i++)
			pollfds[i].fd = -1;
		priv->pollfds = pollfds;

		i = priv->npollfds - POLLFD_GROW;
	}
	pollfd = &priv->pollfds[i];
	pollfd->fd = fd;
	pollfd->events = slirpev2pollev(events);
	pollfd->revents = 0;

	return ((int)i);
}

static int
slirp_poll_revents(int idx, void *param)
{
	struct slirp_priv *priv;
	struct pollfd *pollfd;
	short revents;

	priv = param;
	assert(idx >= 0);
	assert((unsigned int)idx < priv->npollfds);
	pollfd = &priv->pollfds[idx];
	assert(pollfd->fd != -1);

	/* The kernel may report POLLHUP even if we didn't ask for it. */
	revents = pollfd->revents;
	if ((pollfd->events & POLLHUP) == 0)
		revents &= ~POLLHUP;
	return (pollev2slirpev(revents));
}

static void *
slirp_pollfd_td_loop(void *param)
{
	struct slirp_priv *priv;
	struct pollfd *pollfds;
	size_t npollfds;
	uint32_t timeout;
	int error;

	pthread_set_name_np(pthread_self(), "slirp pollfd");
	priv = param;

	pthread_mutex_lock(&priv->mtx);
	for (;;) {
		int wakeup;

		for (size_t i = 0; i < priv->npollfds; i++)
			priv->pollfds[i].fd = -1;

		/* Register for notifications from slirp_cb_notify(). */
		wakeup = slirp_addpoll_cb(priv->wakeup[0], POLLIN, priv);

		timeout = UINT32_MAX;
		slirp_pollfds_fill_p(priv->slirp, &timeout, slirp_addpoll_cb,
		    priv);

		pollfds = priv->pollfds;
		npollfds = priv->npollfds;
		pthread_mutex_unlock(&priv->mtx);
		error = poll(pollfds, npollfds, timeout);
		if (error == -1 && errno != EINTR) {
			EPRINTLN("poll: %s", strerror(errno));
			exit(1);
		}
		pthread_mutex_lock(&priv->mtx);
		slirp_pollfds_poll_p(priv->slirp, error == -1,
		    slirp_poll_revents, priv);

		/*
		 * If we were woken up by the notify callback, mask the
		 * interrupt.
		 */
		if ((pollfds[wakeup].revents & POLLIN) != 0) {
			ssize_t n;

			do {
				uint8_t b;

				n = read(priv->wakeup[0], &b, 1);
			} while (n == 1);
			if (n != -1 || errno != EAGAIN) {
				EPRINTLN("read(wakeup): %s", strerror(errno));
				exit(1);
			}
		}
	}
}

static int
parse_addr(char *addr, struct sockaddr_in *sinp)
{
	char *port;
	int error, porti;

	memset(sinp, 0, sizeof(*sinp));
	sinp->sin_family = AF_INET;
	sinp->sin_len = sizeof(struct sockaddr_in);

	port = strchr(addr, ':');
	if (port == NULL)
		return (EINVAL);
	*port++ = '\0';

	if (strlen(addr) > 0) {
		error = inet_pton(AF_INET, addr, &sinp->sin_addr);
		if (error != 1)
			return (error == 0 ? EPFNOSUPPORT : errno);
	} else {
		sinp->sin_addr.s_addr = htonl(INADDR_ANY);
	}

	porti = strlen(port) > 0 ? atoi(port) : 0;
	if (porti < 0 || porti > UINT16_MAX)
		return (EINVAL);
	sinp->sin_port = htons(porti);

	return (0);
}

static int
parse_hostfwd_rule(const char *descr, int *is_udp, struct sockaddr *hostaddr,
    struct sockaddr *guestaddr)
{
	struct sockaddr_in *hostaddrp, *guestaddrp;
	const char *proto;
	char *p, *host, *guest;
	int error;

	error = 0;
	*is_udp = 0;

	p = strdup(descr);
	if (p == NULL)
		return (ENOMEM);

	host = strchr(p, ':');
	if (host == NULL) {
		error = EINVAL;
		goto out;
	}
	*host++ = '\0';

	proto = p;
	*is_udp = strcmp(proto, "udp") == 0;

	guest = strchr(host, '-');
	if (guest == NULL) {
		error = EINVAL;
		goto out;
	}
	*guest++ = '\0';

	hostaddrp = (struct sockaddr_in *)hostaddr;
	error = parse_addr(host, hostaddrp);
	if (error != 0)
		goto out;

	guestaddrp = (struct sockaddr_in *)guestaddr;
	error = parse_addr(guest, guestaddrp);
	if (error != 0)
		goto out;

out:
	free(p);
	return (error);
}

static int
config_one_hostfwd(struct slirp_priv *priv, const char *rule)
{
	struct sockaddr hostaddr, guestaddr;
	int error, is_udp;

	error = parse_hostfwd_rule(rule, &is_udp, &hostaddr, &guestaddr);
	if (error != 0) {
		EPRINTLN("Unable to parse hostfwd rule '%s': %s",
		    rule, strerror(error));
		return (error);
	}

	error = slirp_add_hostxfwd_p(priv->slirp, &hostaddr, hostaddr.sa_len,
	    &guestaddr, guestaddr.sa_len, is_udp ? SLIRP_HOSTFWD_UDP : 0);
	if (error != 0) {
		EPRINTLN("Unable to add hostfwd rule '%s': %s",
		    rule, strerror(errno));
		return (error);
	}

	return (0);
}

static int
_slirp_init(struct net_backend *be, const char *devname __unused,
    nvlist_t *nvl, net_be_rxeof_t cb, void *param)
{
	struct slirp_priv *priv = NET_BE_PRIV(be);
	SlirpConfig config = {
		.version = 4,
		.if_mtu = SLIRP_MTU,
		.restricted = true,
		.in_enabled = true,
		.vnetwork.s_addr = htonl(0x0a000200),	/* 10.0.2.0/24 */
		.vnetmask.s_addr = htonl(0xffffff00),
		.vdhcp_start.s_addr = htonl(0x0a00020f),/* 10.0.2.15 */
		.vhost.s_addr = htonl(0x0a000202),	/* 10.0.2.2 */
		.enable_emu = false,
	};
	const char *hostfwd;
	int error, sndbuf;

	error = slirp_init_once();
	if (error != 0)
		return (error);

	slirp_priv_init(priv);

	priv->slirp = slirp_new_p(&config, &slirp_cbs, priv);
	if (priv->slirp == NULL) {
		EPRINTLN("Unable to create slirp instance");
		goto err;
	}

	hostfwd = get_config_value_node(nvl, "hostfwd");
	if (hostfwd != NULL) {
		char *rules, *tofree;
		const char *rule;

		tofree = rules = strdup(hostfwd);
		if (rules == NULL)
			goto err;
		while ((rule = strsep(&rules, ";")) != NULL) {
			error = config_one_hostfwd(priv, rule);
			if (error != 0) {
				free(tofree);
				goto err;
			}
		}
		free(tofree);
	}

	error = socketpair(PF_LOCAL, SOCK_DGRAM | SOCK_CLOEXEC, 0, priv->pipe);
	if (error != 0) {
		EPRINTLN("Unable to create pipe: %s", strerror(errno));
		goto err;
	}

	error = pipe2(priv->wakeup, O_CLOEXEC | O_NONBLOCK);
	if (error != 0) {
		EPRINTLN("Unable to create wakeup pipe: %s", strerror(errno));
		goto err;
	}

	/*
	 * Try to avoid dropping buffered packets in slirp_cb_send_packet().
	 */
	sndbuf = 1024 * 1024;
	error = setsockopt(priv->pipe[1], SOL_SOCKET, SO_SNDBUF, &sndbuf,
	    sizeof(sndbuf));
	if (error != 0) {
		EPRINTLN("Could not set socket buffer size: %s",
		    strerror(errno));
		goto err;
	}

	be->fd = priv->pipe[0];
	priv->mevp = mevent_add_disabled(be->fd, EVF_READ, cb, param);
	if (priv->mevp == NULL) {
		EPRINTLN("Could not register event");
		goto err;
	}

	error = pthread_create(&priv->pollfd_td, NULL, slirp_pollfd_td_loop,
	    priv);
	if (error != 0) {
		EPRINTLN("Unable to create pollfd thread: %s", strerror(error));
		goto err;
	}

	return (0);

err:
	slirp_priv_cleanup(priv);
	return (-1);
}

static ssize_t
slirp_send(struct net_backend *be, const struct iovec *iov, int iovcnt)
{
	struct slirp_priv *priv = NET_BE_PRIV(be);

	if (iovcnt == 1) {
		/* We can avoid copying if there's a single segment. */
		pthread_mutex_lock(&priv->mtx);
		slirp_input_p(priv->slirp, iov->iov_base,
		    (int)iov->iov_len);
		pthread_mutex_unlock(&priv->mtx);
		return (iov[0].iov_len);
	} else {
		uint8_t *pkt;
		size_t pktlen;

		pktlen = 0;
		for (int i = 0; i < iovcnt; i++)
			pktlen += iov[i].iov_len;
		pkt = malloc(pktlen);
		if (pkt == NULL)
			return (-1);
		pktlen = 0;
		for (int i = 0; i < iovcnt; i++) {
			memcpy(pkt + pktlen, iov[i].iov_base, iov[i].iov_len);
			pktlen += iov[i].iov_len;
		}
		pthread_mutex_lock(&priv->mtx);
		slirp_input_p(priv->slirp, pkt, (int)pktlen);
		pthread_mutex_unlock(&priv->mtx);
		free(pkt);
		return (pktlen);
	}
}

static void
_slirp_cleanup(struct net_backend *be)
{
	struct slirp_priv *priv = NET_BE_PRIV(be);

	slirp_priv_cleanup(priv);
}

static ssize_t
slirp_peek_recvlen(struct net_backend *be)
{
	struct slirp_priv *priv = NET_BE_PRIV(be);
	ssize_t n;

	n = recv(priv->pipe[0], NULL, 0, MSG_PEEK | MSG_DONTWAIT | MSG_TRUNC);
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
	n = recvmsg(priv->pipe[0], &hdr, MSG_DONTWAIT);
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
	.init = _slirp_init,
	.cleanup = _slirp_cleanup,
	.send = slirp_send,
	.peek_recvlen = slirp_peek_recvlen,
	.recv = slirp_recv,
	.recv_enable = slirp_recv_enable,
	.recv_disable = slirp_recv_disable,
	.get_cap = slirp_get_cap,
	.set_cap = slirp_set_cap,
};

DATA_SET(net_backend_set, slirp_backend);
