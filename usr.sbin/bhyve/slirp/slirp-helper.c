/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023, 2025 Mark Johnston <markj@FreeBSD.org>
 *
 * This software was developed by the University of Cambridge Computer
 * Laboratory (Department of Computer Science and Technology) under Innovate
 * UK project 105694, "Digital Security by Design (DSbD) Technology Platform
 * Prototype".
 */

/*
 * A helper process which lets bhyve's libslirp-based network backend work
 * outside bhyve's Capsicum sandbox.  We are started with a SOCK_SEQPACKET
 * socket through which we pass and receive packets from the guest's frontend.
 *
 * At initialization time, we receive an nvlist over the socket which describes
 * the desired slirp configuration.
 */

#include <sys/nv.h>
#include <sys/socket.h>

#include <assert.h>
#include <capsicum_helpers.h>
#include <dlfcn.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "libslirp.h"

#define	SLIRP_MTU	2048

struct slirp_priv {
	Slirp *slirp;		/* libslirp handle */
	int sock;		/* data and control socket */
	int wakeup[2];		/* used to wake up the pollfd thread */
	struct pollfd *pollfds;
	size_t npollfds;
	size_t lastpollfd;
};

typedef int (*slirp_add_hostxfwd_p_t)(Slirp *,
    const struct sockaddr *, socklen_t, const struct sockaddr *, socklen_t,
    int);
typedef void (*slirp_cleanup_p_t)(Slirp *);
typedef void (*slirp_input_p_t)(Slirp *, const uint8_t *, int);
typedef Slirp *(*slirp_new_p_t)(const SlirpConfig *, const SlirpCb *, void *);
typedef void (*slirp_pollfds_fill_p_t)(Slirp *, uint32_t *timeout,
    SlirpAddPollCb, void *);
typedef void (*slirp_pollfds_poll_p_t)(Slirp *, int, SlirpGetREventsCb, void *);

/* Function pointer table, initialized by libslirp_init(). */
static slirp_add_hostxfwd_p_t slirp_add_hostxfwd_p;
static slirp_cleanup_p_t slirp_cleanup_p;
static slirp_input_p_t slirp_input_p;
static slirp_new_p_t slirp_new_p;
static slirp_pollfds_fill_p_t slirp_pollfds_fill_p;
static slirp_pollfds_poll_p_t slirp_pollfds_poll_p;

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
	n = send(priv->sock, buf, len, MSG_EOR);
	if (n < 0) {
		warn("slirp_cb_send_packet: send");
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
slirp_addpoll(struct slirp_priv *priv, int fd, int events)
{
	struct pollfd *pollfd, *pollfds;
	size_t i;

	for (i = priv->lastpollfd + 1; i < priv->npollfds; i++)
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
	priv->lastpollfd = i;

	return ((int)i);
}

static int
slirp_addpoll_cb(int fd, int events, void *param)
{
	struct slirp_priv *priv;

	priv = param;
	return (slirp_addpoll(priv, fd, events));
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

/*
 * Main loop.  Poll libslirp's descriptors plus a couple of our own.
 */
static void
slirp_pollfd_loop(struct slirp_priv *priv)
{
	struct pollfd *pollfds;
	size_t npollfds;
	uint32_t timeout;
	int error;

	for (;;) {
		int input, wakeup;

		for (size_t i = 0; i < priv->npollfds; i++)
			priv->pollfds[i].fd = -1;
		priv->lastpollfd = -1;

		/* Register for notifications from slirp_cb_notify(). */
		wakeup = slirp_addpoll(priv, priv->wakeup[0], POLLIN);
		/* Register for input from our parent process. */
		input = slirp_addpoll(priv, priv->sock, POLLIN | POLLRDHUP);

		timeout = UINT32_MAX;
		slirp_pollfds_fill_p(priv->slirp, &timeout, slirp_addpoll_cb,
		    priv);

		pollfds = priv->pollfds;
		npollfds = priv->npollfds;
		error = poll(pollfds, npollfds, timeout);
		if (error == -1 && errno != EINTR)
			err(1, "poll");
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
			if (n != -1 || errno != EAGAIN)
				err(1, "read");
		}

		/*
		 * If new packets arrived from our parent, feed them to
		 * libslirp.
		 */
		if ((pollfds[input].revents & (POLLHUP | POLLRDHUP)) != 0)
			errx(1, "parent process closed connection");
		if ((pollfds[input].revents & POLLIN) != 0) {
			ssize_t n;

			do {
				uint8_t buf[SLIRP_MTU];

				n = recv(priv->sock, buf, sizeof(buf),
				    MSG_DONTWAIT);
				if (n < 0) {
					if (errno == EWOULDBLOCK)
						break;
					err(1, "recv");
				}
				slirp_input_p(priv->slirp, buf, (int)n);
			} while (n >= 0);
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

	hostaddrp = (struct sockaddr_in *)(void *)hostaddr;
	error = parse_addr(host, hostaddrp);
	if (error != 0)
		goto out;

	guestaddrp = (struct sockaddr_in *)(void *)guestaddr;
	error = parse_addr(guest, guestaddrp);
	if (error != 0)
		goto out;

out:
	free(p);
	return (error);
}

static void
config_one_hostfwd(Slirp *slirp, const char *rule)
{
	struct sockaddr hostaddr, guestaddr;
	int error, is_udp;

	error = parse_hostfwd_rule(rule, &is_udp, &hostaddr, &guestaddr);
	if (error != 0)
		errx(1, "unable to parse hostfwd rule '%s': %s", rule,
		    strerror(error));

	error = slirp_add_hostxfwd_p(slirp, &hostaddr, hostaddr.sa_len,
	    &guestaddr, guestaddr.sa_len, is_udp ? SLIRP_HOSTFWD_UDP : 0);
	if (error != 0)
		errx(1, "Unable to add hostfwd rule '%s': %s", rule,
		    strerror(errno));
}

/*
 * Drop privileges to the "nobody" user.  Ideally we'd chroot to somewhere like
 * /var/empty but libslirp might need to access /etc/resolv.conf.
 */
static void
drop_privs(void)
{
	struct passwd *pw;

	if (geteuid() != 0)
		return;

	pw = getpwnam("nobody");
	if (pw == NULL)
		err(1, "getpwnam(nobody) failed");
	if (initgroups(pw->pw_name, pw->pw_gid) != 0)
		err(1, "initgroups");
	if (setgid(pw->pw_gid) != 0)
		err(1, "setgid");
	if (setuid(pw->pw_uid) != 0)
		err(1, "setuid");
}

static void
libslirp_init(void)
{
	void *handle;

	handle = dlopen("libslirp.so.0", RTLD_LAZY);
	if (handle == NULL)
		errx(1, "unable to open libslirp.so.0: %s", dlerror());

#define IMPORT_SYM(sym) do {					\
	sym##_p = (sym##_p_t)dlsym(handle, #sym);		\
	if (sym##_p == NULL)					\
		errx(1, "failed to resolve %s", #sym);		\
} while (0)
	IMPORT_SYM(slirp_add_hostxfwd);
	IMPORT_SYM(slirp_cleanup);
	IMPORT_SYM(slirp_input);
	IMPORT_SYM(slirp_new);
	IMPORT_SYM(slirp_pollfds_fill);
	IMPORT_SYM(slirp_pollfds_poll);
#undef IMPORT_SYM
}

static void
usage(void)
{
	fprintf(stderr, "Usage: slirp-helper -S <socket>\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	struct slirp_priv priv;
	SlirpConfig slirpconfig;
	Slirp *slirp;
	nvlist_t *config;
	const char *hostfwd, *vmname;
	int ch, fd, sd;
	bool restricted;

	sd = -1;
	while ((ch = getopt(argc, argv, "S:")) != -1) {
		switch (ch) {
		case 'S':
			sd = atoi(optarg);
			if (fcntl(sd, F_GETFD) == -1)
				err(1, "invalid socket %s", optarg);
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (sd == -1)
		usage();

	/*
	 * Clean the fd space: point stdio to /dev/null and keep our socket.
	 */
	fd = open("/dev/null", O_RDWR);
	if (fd == -1)
		err(1, "open(/dev/null)");
	if (dup2(fd, STDIN_FILENO) == -1)
		err(1, "dup2(stdin)");
	if (dup2(fd, STDOUT_FILENO) == -1)
		err(1, "dup2(stdout)");
	if (dup2(fd, STDERR_FILENO) == -1)
		err(1, "dup2(stderr)");
	if (dup2(sd, 3) == -1)
		err(1, "dup2(slirp socket)");
	sd = 3;
	closefrom(sd + 1);

	memset(&priv, 0, sizeof(priv));
	priv.sock = sd;
	if (ioctl(priv.sock, FIONBIO, &(int){0}) == -1)
		err(1, "ioctl(FIONBIO)");
	if (pipe2(priv.wakeup, O_CLOEXEC | O_NONBLOCK) != 0)
		err(1, "pipe2");

	/*
	 * Apply the configuration we received from bhyve.
	 */
	config = nvlist_recv(sd, 0);
	if (config == NULL)
		err(1, "nvlist_recv");
	vmname = get_config_value_node(config, "vmname");
	if (vmname != NULL)
		setproctitle("%s", vmname);
	restricted = !get_config_bool_node_default(config, "open", false);

	slirpconfig = (SlirpConfig){
		.version = 4,
		.if_mtu = SLIRP_MTU,
		.restricted = restricted,
		.in_enabled = true,
		.vnetwork.s_addr = htonl(0x0a000200),	/* 10.0.2.0/24 */
		.vnetmask.s_addr = htonl(0xffffff00),	/* 255.255.255.0 */
		.vdhcp_start.s_addr = htonl(0x0a00020f),/* 10.0.2.15 */
		.vhost.s_addr = htonl(0x0a000202),	/* 10.0.2.2 */
		.vnameserver.s_addr = htonl(0x0a000203),/* 10.0.2.3 */
		.enable_emu = false,
	};
	libslirp_init();
	slirp = slirp_new_p(&slirpconfig, &slirp_cbs, &priv);

	hostfwd = get_config_value_node(config, "hostfwd");
	if (hostfwd != NULL) {
		char *rules, *tofree;
		const char *rule;

		tofree = rules = strdup(hostfwd);
		if (rules == NULL)
			err(1, "strdup");
		while ((rule = strsep(&rules, ";")) != NULL)
			config_one_hostfwd(slirp, rule);
		free(tofree);
	}

	priv.slirp = slirp;

	/*
	 * Drop root privileges if we have them.
	 */
	drop_privs();

	/*
	 * In restricted mode, we can enter a Capsicum sandbox without losing
	 * functionality.
	 */
	if (restricted && caph_enter() != 0)
		err(1, "caph_enter");

	/*
	 * Enter our main loop.  If bhyve goes away, we should observe a hangup
	 * on the socket and exit.
	 */
	slirp_pollfd_loop(&priv);
	/* NOTREACHED */

	return (1);
}
