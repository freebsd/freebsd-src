/*-
 * Copyright (c) 2009-2010 The FreeBSD Foundation
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>	/* MAXHOSTNAMELEN */

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "hast.h"
#include "pjdlog.h"
#include "proto_impl.h"
#include "subr.h"

#define	TCP4_CTX_MAGIC	0x7c441c
struct tcp4_ctx {
	int			tc_magic;
	struct sockaddr_in	tc_sin;
	int			tc_fd;
	int			tc_side;
#define	TCP4_SIDE_CLIENT	0
#define	TCP4_SIDE_SERVER_LISTEN	1
#define	TCP4_SIDE_SERVER_WORK	2
};

static void tcp4_close(void *ctx);

static in_addr_t
str2ip(const char *str)
{
	struct hostent *hp;
	in_addr_t ip;

	ip = inet_addr(str);
	if (ip != INADDR_NONE) {
		/* It is a valid IP address. */
		return (ip);
	}
	/* Check if it is a valid host name. */
	hp = gethostbyname(str);
	if (hp == NULL)
		return (INADDR_NONE);
	return (((struct in_addr *)(void *)hp->h_addr)->s_addr);
}

/*
 * Function converts the given string to unsigned number.
 */
static int
numfromstr(const char *str, intmax_t minnum, intmax_t maxnum, intmax_t *nump)
{
	intmax_t digit, num;

	if (str[0] == '\0')
		goto invalid;	/* Empty string. */
	num = 0;
	for (; *str != '\0'; str++) {
		if (*str < '0' || *str > '9')
			goto invalid;	/* Non-digit character. */
		digit = *str - '0';
		if (num > num * 10 + digit)
			goto invalid;	/* Overflow. */
		num = num * 10 + digit;
		if (num > maxnum)
			goto invalid;	/* Too big. */
	}
	if (num < minnum)
		goto invalid;	/* Too small. */
	*nump = num;
	return (0);
invalid:
	errno = EINVAL;
	return (-1);
}

static int
tcp4_addr(const char *addr, struct sockaddr_in *sinp)
{
	char iporhost[MAXHOSTNAMELEN];
	const char *pp;
	size_t size;
	in_addr_t ip;

	if (addr == NULL)
		return (-1);

	if (strncasecmp(addr, "tcp4://", 7) == 0)
		addr += 7;
	else if (strncasecmp(addr, "tcp://", 6) == 0)
		addr += 6;
	else {
		/*
		 * Because TCP4 is the default assume IP or host is given without
		 * prefix.
		 */
	}

	sinp->sin_family = AF_INET;
	sinp->sin_len = sizeof(*sinp);
	/* Extract optional port. */
	pp = strrchr(addr, ':');
	if (pp == NULL) {
		/* Port not given, use the default. */
		sinp->sin_port = htons(HASTD_PORT);
	} else {
		intmax_t port;

		if (numfromstr(pp + 1, 1, 65535, &port) < 0)
			return (errno);
		sinp->sin_port = htons(port);
	}
	/* Extract host name or IP address. */
	if (pp == NULL) {
		size = sizeof(iporhost);
		if (strlcpy(iporhost, addr, size) >= size)
			return (ENAMETOOLONG);
	} else {
		size = (size_t)(pp - addr + 1);
		if (size > sizeof(iporhost))
			return (ENAMETOOLONG);
		(void)strlcpy(iporhost, addr, size);
	}
	/* Convert string (IP address or host name) to in_addr_t. */
	ip = str2ip(iporhost);
	if (ip == INADDR_NONE)
		return (EINVAL);
	sinp->sin_addr.s_addr = ip;

	return (0);
}

static int
tcp4_common_setup(const char *addr, void **ctxp, int side)
{
	struct tcp4_ctx *tctx;
	int ret, val;

	tctx = malloc(sizeof(*tctx));
	if (tctx == NULL)
		return (errno);

	/* Parse given address. */
	if ((ret = tcp4_addr(addr, &tctx->tc_sin)) != 0) {
		free(tctx);
		return (ret);
	}

	tctx->tc_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (tctx->tc_fd == -1) {
		ret = errno;
		free(tctx);
		return (ret);
	}

	/* Socket settings. */
	val = 1;
	if (setsockopt(tctx->tc_fd, IPPROTO_TCP, TCP_NODELAY, &val,
	    sizeof(val)) == -1) {
		pjdlog_warning("Unable to set TCP_NOELAY on %s", addr);
	}
	val = 131072;
	if (setsockopt(tctx->tc_fd, SOL_SOCKET, SO_SNDBUF, &val,
	    sizeof(val)) == -1) {
		pjdlog_warning("Unable to set send buffer size on %s", addr);
	}
	val = 131072;
	if (setsockopt(tctx->tc_fd, SOL_SOCKET, SO_RCVBUF, &val,
	    sizeof(val)) == -1) {
		pjdlog_warning("Unable to set receive buffer size on %s", addr);
	}

	tctx->tc_side = side;
	tctx->tc_magic = TCP4_CTX_MAGIC;
	*ctxp = tctx;

	return (0);
}

static int
tcp4_client(const char *addr, void **ctxp)
{

	return (tcp4_common_setup(addr, ctxp, TCP4_SIDE_CLIENT));
}

static int
tcp4_connect(void *ctx)
{
	struct tcp4_ctx *tctx = ctx;
	struct timeval tv;
	fd_set fdset;
	socklen_t esize;
	int error, flags, ret;

	assert(tctx != NULL);
	assert(tctx->tc_magic == TCP4_CTX_MAGIC);
	assert(tctx->tc_side == TCP4_SIDE_CLIENT);
	assert(tctx->tc_fd >= 0);

	flags = fcntl(tctx->tc_fd, F_GETFL);
	if (flags == -1) {
		KEEP_ERRNO(pjdlog_common(LOG_DEBUG, 1, errno,
		    "fcntl(F_GETFL) failed"));
		return (errno);
	}
	/*
	 * We make socket non-blocking so we can handle connection timeout
	 * manually.
	 */
	flags |= O_NONBLOCK;
	if (fcntl(tctx->tc_fd, F_SETFL, flags) == -1) {
		KEEP_ERRNO(pjdlog_common(LOG_DEBUG, 1, errno,
		    "fcntl(F_SETFL, O_NONBLOCK) failed"));
		return (errno);
	}

	if (connect(tctx->tc_fd, (struct sockaddr *)&tctx->tc_sin,
	    sizeof(tctx->tc_sin)) == 0) {
		error = 0;
		goto done;
	}
	if (errno != EINPROGRESS) {
		error = errno;
		pjdlog_common(LOG_DEBUG, 1, errno, "connect() failed");
		goto done;
	}
	/*
	 * Connection can't be established immediately, let's wait
	 * for HAST_TIMEOUT seconds.
	 */
	tv.tv_sec = HAST_TIMEOUT;
	tv.tv_usec = 0;
again:
	FD_ZERO(&fdset);
	FD_SET(tctx->tc_fd, &fdset); 
	ret = select(tctx->tc_fd + 1, NULL, &fdset, NULL, &tv);
	if (ret == 0) {
		error = ETIMEDOUT;
		goto done;
	} else if (ret == -1) {
		if (errno == EINTR)
			goto again;
		error = errno;
		pjdlog_common(LOG_DEBUG, 1, errno, "select() failed");
		goto done;
	}
	assert(ret > 0);
	assert(FD_ISSET(tctx->tc_fd, &fdset));
	esize = sizeof(error);
	if (getsockopt(tctx->tc_fd, SOL_SOCKET, SO_ERROR, &error,
	    &esize) == -1) {
		error = errno;
		pjdlog_common(LOG_DEBUG, 1, errno,
		    "getsockopt(SO_ERROR) failed");
		goto done;
	}
	if (error != 0) {
		pjdlog_common(LOG_DEBUG, 1, error,
		    "getsockopt(SO_ERROR) returned error");
		goto done;
	}
	error = 0;
done:
	flags &= ~O_NONBLOCK;
	if (fcntl(tctx->tc_fd, F_SETFL, flags) == -1) {
		if (error == 0)
			error = errno;
		pjdlog_common(LOG_DEBUG, 1, errno,
		    "fcntl(F_SETFL, ~O_NONBLOCK) failed");
	}
	return (error);
}

static int
tcp4_server(const char *addr, void **ctxp)
{
	struct tcp4_ctx *tctx;
	int ret, val;

	ret = tcp4_common_setup(addr, ctxp, TCP4_SIDE_SERVER_LISTEN);
	if (ret != 0)
		return (ret);

	tctx = *ctxp;

	val = 1;
	/* Ignore failure. */
	(void)setsockopt(tctx->tc_fd, SOL_SOCKET, SO_REUSEADDR, &val,
	   sizeof(val));

	if (bind(tctx->tc_fd, (struct sockaddr *)&tctx->tc_sin,
	    sizeof(tctx->tc_sin)) < 0) {
		ret = errno;
		tcp4_close(tctx);
		return (ret);
	}
	if (listen(tctx->tc_fd, 8) < 0) {
		ret = errno;
		tcp4_close(tctx);
		return (ret);
	}

	return (0);
}

static int
tcp4_accept(void *ctx, void **newctxp)
{
	struct tcp4_ctx *tctx = ctx;
	struct tcp4_ctx *newtctx;
	socklen_t fromlen;
	int ret;

	assert(tctx != NULL);
	assert(tctx->tc_magic == TCP4_CTX_MAGIC);
	assert(tctx->tc_side == TCP4_SIDE_SERVER_LISTEN);
	assert(tctx->tc_fd >= 0);

	newtctx = malloc(sizeof(*newtctx));
	if (newtctx == NULL)
		return (errno);

	fromlen = sizeof(tctx->tc_sin);
	newtctx->tc_fd = accept(tctx->tc_fd, (struct sockaddr *)&tctx->tc_sin,
	    &fromlen);
	if (newtctx->tc_fd < 0) {
		ret = errno;
		free(newtctx);
		return (ret);
	}

	newtctx->tc_side = TCP4_SIDE_SERVER_WORK;
	newtctx->tc_magic = TCP4_CTX_MAGIC;
	*newctxp = newtctx;

	return (0);
}

static int
tcp4_send(void *ctx, const unsigned char *data, size_t size)
{
	struct tcp4_ctx *tctx = ctx;

	assert(tctx != NULL);
	assert(tctx->tc_magic == TCP4_CTX_MAGIC);
	assert(tctx->tc_fd >= 0);

	return (proto_common_send(tctx->tc_fd, data, size));
}

static int
tcp4_recv(void *ctx, unsigned char *data, size_t size)
{
	struct tcp4_ctx *tctx = ctx;

	assert(tctx != NULL);
	assert(tctx->tc_magic == TCP4_CTX_MAGIC);
	assert(tctx->tc_fd >= 0);

	return (proto_common_recv(tctx->tc_fd, data, size));
}

static int
tcp4_descriptor(const void *ctx)
{
	const struct tcp4_ctx *tctx = ctx;

	assert(tctx != NULL);
	assert(tctx->tc_magic == TCP4_CTX_MAGIC);

	return (tctx->tc_fd);
}

static void
sin2str(struct sockaddr_in *sinp, char *addr, size_t size)
{
	in_addr_t ip;
	unsigned int port;

	assert(addr != NULL);
	assert(sinp->sin_family == AF_INET);

	ip = ntohl(sinp->sin_addr.s_addr);
	port = ntohs(sinp->sin_port);
	PJDLOG_VERIFY(snprintf(addr, size, "tcp4://%u.%u.%u.%u:%u",
	    ((ip >> 24) & 0xff), ((ip >> 16) & 0xff), ((ip >> 8) & 0xff),
	    (ip & 0xff), port) < (ssize_t)size);
}

static bool
tcp4_address_match(const void *ctx, const char *addr)
{
	const struct tcp4_ctx *tctx = ctx;
	struct sockaddr_in sin;
	socklen_t sinlen;
	in_addr_t ip1, ip2;

	assert(tctx != NULL);
	assert(tctx->tc_magic == TCP4_CTX_MAGIC);

	if (tcp4_addr(addr, &sin) != 0)
		return (false);
	ip1 = sin.sin_addr.s_addr;

	sinlen = sizeof(sin);
	if (getpeername(tctx->tc_fd, (struct sockaddr *)&sin, &sinlen) < 0)
		return (false);
	ip2 = sin.sin_addr.s_addr;

	return (ip1 == ip2);
}

static void
tcp4_local_address(const void *ctx, char *addr, size_t size)
{
	const struct tcp4_ctx *tctx = ctx;
	struct sockaddr_in sin;
	socklen_t sinlen;

	assert(tctx != NULL);
	assert(tctx->tc_magic == TCP4_CTX_MAGIC);

	sinlen = sizeof(sin);
	if (getsockname(tctx->tc_fd, (struct sockaddr *)&sin, &sinlen) < 0) {
		PJDLOG_VERIFY(strlcpy(addr, "N/A", size) < size);
		return;
	}
	sin2str(&sin, addr, size);
}

static void
tcp4_remote_address(const void *ctx, char *addr, size_t size)
{
	const struct tcp4_ctx *tctx = ctx;
	struct sockaddr_in sin;
	socklen_t sinlen;

	assert(tctx != NULL);
	assert(tctx->tc_magic == TCP4_CTX_MAGIC);

	sinlen = sizeof(sin);
	if (getpeername(tctx->tc_fd, (struct sockaddr *)&sin, &sinlen) < 0) {
		PJDLOG_VERIFY(strlcpy(addr, "N/A", size) < size);
		return;
	}
	sin2str(&sin, addr, size);
}

static void
tcp4_close(void *ctx)
{
	struct tcp4_ctx *tctx = ctx;

	assert(tctx != NULL);
	assert(tctx->tc_magic == TCP4_CTX_MAGIC);

	if (tctx->tc_fd >= 0)
		close(tctx->tc_fd);
	tctx->tc_magic = 0;
	free(tctx);
}

static struct hast_proto tcp4_proto = {
	.hp_name = "tcp4",
	.hp_client = tcp4_client,
	.hp_connect = tcp4_connect,
	.hp_server = tcp4_server,
	.hp_accept = tcp4_accept,
	.hp_send = tcp4_send,
	.hp_recv = tcp4_recv,
	.hp_descriptor = tcp4_descriptor,
	.hp_address_match = tcp4_address_match,
	.hp_local_address = tcp4_local_address,
	.hp_remote_address = tcp4_remote_address,
	.hp_close = tcp4_close
};

static __constructor void
tcp4_ctor(void)
{

	proto_register(&tcp4_proto, true);
}
