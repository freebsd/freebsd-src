/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013 The FreeBSD Foundation
 * Copyright (c) 2015 Mariusz Zaborski <oshogbo@FreeBSD.org>
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/nv.h>

#include <assert.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "libcasper.h"
#include "libcasper_impl.h"

/*
 * Currently there is only one service_connection per service.
 * In the future we may want multiple connections from multiple clients
 * per one service instance, but it has to be carefully designed.
 * The problem is that we may restrict/sandbox service instance according
 * to the limits provided. When new connection comes in with different
 * limits we won't be able to access requested resources.
 * Not to mention one process will serve to multiple mutually untrusted
 * clients and compromise of this service instance by one of its clients
 * can lead to compromise of the other clients.
 */

/*
 * Client connections to the given service.
 */
#define	SERVICE_CONNECTION_MAGIC	0x5e91c0ec
struct service_connection {
	int		 sc_magic;
	cap_channel_t	*sc_chan;
	nvlist_t	*sc_limits;
	struct service	*sc_service;
	size_t		 sc_pollidx;
};

#define	SERVICE_MAGIC	0x5e91ce
struct service {
	int			 s_magic;
	char			*s_name;
	uint64_t		 s_flags;
	service_limit_func_t	*s_limit;
	service_command_func_t	*s_command;
};

#define	POLLSET_CHUNK	8
static struct pollfd		*pollset_pfds;
static struct service_connection **pollset_conns;
static size_t			 pollset_cap;
static size_t			 pollset_size;

static int
pollset_add(struct service_connection *sconn, int sock)
{
	size_t i, newcap;
	void *p;

	for (i = 0; i < pollset_size; i++) {
		if (pollset_pfds[i].fd < 0)
			break;
	}
	if (i == pollset_size) {
		newcap = roundup2(pollset_size + 1, POLLSET_CHUNK);
		if (newcap > pollset_cap) {
			p = reallocarray(pollset_pfds, newcap,
			    sizeof(*pollset_pfds));
			if (p == NULL)
				return (-1);
			pollset_pfds = p;
			p = reallocarray(pollset_conns, newcap,
			    sizeof(*pollset_conns));
			if (p == NULL)
				return (-1);
			pollset_conns = p;
			pollset_cap = newcap;
		}
		pollset_size++;
	}
	pollset_pfds[i].fd = sock;
	pollset_pfds[i].events = POLLIN;
	pollset_pfds[i].revents = 0;
	pollset_conns[i] = sconn;
	sconn->sc_pollidx = i;
	return (0);
}

static void
pollset_remove(struct service_connection *sconn)
{

	pollset_pfds[sconn->sc_pollidx].fd = -1;
	pollset_conns[sconn->sc_pollidx] = NULL;
}

bool
service_have_connections(void)
{
	size_t i;

	for (i = 0; i < pollset_size; i++) {
		if (pollset_pfds[i].fd >= 0)
			return (true);
	}
	return (false);
}

bool
service_poll_dispatch(void)
{
	size_t i;
	int ret;

	do {
		ret = poll(pollset_pfds, pollset_size, -1);
	} while (ret == -1 && errno == EINTR);
	if (ret == -1)
		return (false);

	for (i = 0; i < pollset_size; i++) {
		if (pollset_pfds[i].revents == 0)
			continue;
		service_message(pollset_conns[i]->sc_service,
		    pollset_conns[i]);
	}
	return (true);
}

struct service *
service_alloc(const char *name, service_limit_func_t *limitfunc,
    service_command_func_t *commandfunc, uint64_t flags)
{
	struct service *service;

	service = malloc(sizeof(*service));
	if (service == NULL)
		return (NULL);
	service->s_name = strdup(name);
	if (service->s_name == NULL) {
		free(service);
		return (NULL);
	}
	service->s_limit = limitfunc;
	service->s_command = commandfunc;
	service->s_flags = flags;
	service->s_magic = SERVICE_MAGIC;

	return (service);
}

void
service_free(struct service *service)
{
	size_t i;

	assert(service->s_magic == SERVICE_MAGIC);

	service->s_magic = 0;
	for (i = 0; i < pollset_size; i++) {
		if (pollset_conns[i] != NULL &&
		    pollset_conns[i]->sc_service == service)
			service_connection_remove(service, pollset_conns[i]);
	}
	free(service->s_name);
	free(service);
}

struct service_connection *
service_connection_add(struct service *service, int sock,
    const nvlist_t *limits)
{
	struct service_connection *sconn;
	int serrno;

	assert(service->s_magic == SERVICE_MAGIC);

	sconn = malloc(sizeof(*sconn));
	if (sconn == NULL)
		return (NULL);
	sconn->sc_chan = cap_wrap(sock,
	    service_get_channel_flags(service));
	if (sconn->sc_chan == NULL) {
		serrno = errno;
		free(sconn);
		errno = serrno;
		return (NULL);
	}
	if (limits == NULL) {
		sconn->sc_limits = NULL;
	} else {
		sconn->sc_limits = nvlist_clone(limits);
		if (sconn->sc_limits == NULL) {
			serrno = errno;
			(void)cap_unwrap(sconn->sc_chan, NULL);
			free(sconn);
			errno = serrno;
			return (NULL);
		}
	}
	sconn->sc_service = service;
	if (pollset_add(sconn, sock) == -1) {
		serrno = errno;
		nvlist_destroy(sconn->sc_limits);
		(void)cap_unwrap(sconn->sc_chan, NULL);
		free(sconn);
		errno = serrno;
		return (NULL);
	}
	sconn->sc_magic = SERVICE_CONNECTION_MAGIC;
	return (sconn);
}

void
service_connection_remove(struct service *service,
    struct service_connection *sconn)
{

	assert(service->s_magic == SERVICE_MAGIC);
	assert(sconn->sc_magic == SERVICE_CONNECTION_MAGIC);

	pollset_remove(sconn);
	sconn->sc_magic = 0;
	nvlist_destroy(sconn->sc_limits);
	cap_close(sconn->sc_chan);
	free(sconn);
}

int
service_connection_clone(struct service *service,
    struct service_connection *sconn)
{
	struct service_connection *newsconn;
	int serrno, sock[2];

	if (socketpair(PF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, sock) < 0)
		return (-1);

	newsconn = service_connection_add(service, sock[0],
	    service_connection_get_limits(sconn));
	if (newsconn == NULL) {
		serrno = errno;
		close(sock[0]);
		close(sock[1]);
		errno = serrno;
		return (-1);
	}

	return (sock[1]);
}

cap_channel_t *
service_connection_get_chan(const struct service_connection *sconn)
{

	assert(sconn->sc_magic == SERVICE_CONNECTION_MAGIC);

	return (sconn->sc_chan);
}

int
service_connection_get_sock(const struct service_connection *sconn)
{

	assert(sconn->sc_magic == SERVICE_CONNECTION_MAGIC);

	return (cap_sock(sconn->sc_chan));
}

const nvlist_t *
service_connection_get_limits(const struct service_connection *sconn)
{

	assert(sconn->sc_magic == SERVICE_CONNECTION_MAGIC);

	return (sconn->sc_limits);
}

void
service_connection_set_limits(struct service_connection *sconn,
    nvlist_t *limits)
{

	assert(sconn->sc_magic == SERVICE_CONNECTION_MAGIC);

	nvlist_destroy(sconn->sc_limits);
	sconn->sc_limits = limits;
}

void
service_message(struct service *service, struct service_connection *sconn)
{
	nvlist_t *nvlin, *nvlout;
	const char *cmd;
	int error, flags;

	flags = 0;
	if ((service->s_flags & CASPER_SERVICE_NO_UNIQ_LIMITS) != 0)
		flags = NV_FLAG_NO_UNIQUE;

	nvlin = cap_recv_nvlist(service_connection_get_chan(sconn));
	if (nvlin == NULL) {
		service_connection_remove(service, sconn);
		return;
	}

	error = EDOOFUS;
	nvlout = nvlist_create(flags);

	cmd = nvlist_get_string(nvlin, "cmd");
	if (strcmp(cmd, "limit_set") == 0) {
		nvlist_t *nvllim;

		nvllim = nvlist_take_nvlist(nvlin, "limits");
		if (service->s_limit == NULL) {
			error = EOPNOTSUPP;
		} else {
			error = service->s_limit(
			    service_connection_get_limits(sconn), nvllim);
		}
		if (error == 0) {
			service_connection_set_limits(sconn, nvllim);
			/* Function consumes nvllim. */
		} else {
			nvlist_destroy(nvllim);
		}
	} else if (strcmp(cmd, "limit_get") == 0) {
		const nvlist_t *nvllim;

		nvllim = service_connection_get_limits(sconn);
		if (nvllim != NULL)
			nvlist_add_nvlist(nvlout, "limits", nvllim);
		else
			nvlist_add_null(nvlout, "limits");
		error = 0;
	} else if (strcmp(cmd, "clone") == 0) {
		int sock;

		sock = service_connection_clone(service, sconn);
		if (sock == -1) {
			error = errno;
		} else {
			nvlist_move_descriptor(nvlout, "sock", sock);
			error = 0;
		}
	} else {
		error = service->s_command(cmd,
		    service_connection_get_limits(sconn), nvlin, nvlout);
	}

	nvlist_destroy(nvlin);
	nvlist_add_number(nvlout, "error", (uint64_t)error);

	if (cap_send_nvlist(service_connection_get_chan(sconn), nvlout) == -1)
		service_connection_remove(service, sconn);

	nvlist_destroy(nvlout);
}

const char *
service_name(struct service *service)
{

	assert(service->s_magic == SERVICE_MAGIC);
	return (service->s_name);
}

int
service_get_channel_flags(struct service *service)
{
	int flags;

	assert(service->s_magic == SERVICE_MAGIC);
	flags = 0;

	if ((service->s_flags & CASPER_SERVICE_NO_UNIQ_LIMITS) != 0)
		flags |= CASPER_NO_UNIQ;

	return (flags);
}

static void
stdnull(void)
{
	int fd;

	fd = open(_PATH_DEVNULL, O_RDWR);
	if (fd == -1)
		errx(1, "Unable to open %s", _PATH_DEVNULL);

	if (setsid() == -1)
		errx(1, "Unable to detach from session");

	if (dup2(fd, STDIN_FILENO) == -1)
		errx(1, "Unable to cover stdin");
	if (dup2(fd, STDOUT_FILENO) == -1)
		errx(1, "Unable to cover stdout");
	if (dup2(fd, STDERR_FILENO) == -1)
		errx(1, "Unable to cover stderr");

	if (fd > STDERR_FILENO)
		close(fd);
}

static void
service_clean(int *sockp, int *procfdp, uint64_t flags)
{
	int fd, maxfd, minfd;

	fd_fix_environment(sockp);
	fd_fix_environment(procfdp);

	assert(*sockp > STDERR_FILENO);
	assert(*procfdp > STDERR_FILENO);
	assert(*sockp != *procfdp);

	if ((flags & CASPER_SERVICE_STDIO) == 0)
		stdnull();

	if ((flags & CASPER_SERVICE_FD) == 0) {
		if (*procfdp > *sockp) {
			maxfd = *procfdp;
			minfd = *sockp;
		} else {
			maxfd = *sockp;
			minfd = *procfdp;
		}

		for (fd = STDERR_FILENO + 1; fd < maxfd; fd++) {
			if (fd != minfd)
				close(fd);
		}
		closefrom(maxfd + 1);
	}
}

void
service_start(struct service *service, int sock, int procfd)
{

	assert(service != NULL);
	assert(service->s_magic == SERVICE_MAGIC);
	setproctitle("%s", service->s_name);
	service_clean(&sock, &procfd, service->s_flags);

	if (service_connection_add(service, sock, NULL) == NULL)
		_exit(1);

	while (service_have_connections()) {
		if (!service_poll_dispatch())
			_exit(1);
	}

	_exit(0);
}
