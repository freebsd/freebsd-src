/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Mariusz Zaborski <oshogbo@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#ifndef _CAP_NETWORK_H_
#define	_CAP_NETWORK_H_

#ifdef HAVE_CASPER
#define WITH_CASPER
#endif

#include <sys/dnv.h>
#include <sys/nv.h>

#include <sys/socket.h>

struct addrinfo;
struct hostent;

struct cap_net_limit;
typedef struct cap_net_limit cap_net_limit_t;

#define CAPNET_ADDR2NAME		(0x01)
#define CAPNET_NAME2ADDR		(0x02)
#define CAPNET_DEPRECATED_ADDR2NAME	(0x04)
#define CAPNET_DEPRECATED_NAME2ADDR	(0x08)
#define CAPNET_CONNECT			(0x10)
#define CAPNET_BIND			(0x20)
#define CAPNET_CONNECTDNS		(0x40)

#ifdef WITH_CASPER
/* Capability functions. */
int cap_bind(cap_channel_t *chan, int s, const struct sockaddr *addr,
    socklen_t addrlen);
int cap_connect(cap_channel_t *chan, int s, const struct sockaddr *name,
    socklen_t namelen);

int cap_getaddrinfo(cap_channel_t *chan, const char *hostname,
    const char *servname, const struct addrinfo *hints, struct addrinfo **res);
int cap_getnameinfo(cap_channel_t *chan, const struct sockaddr *sa,
    socklen_t salen, char *host, size_t hostlen, char *serv, size_t servlen,
    int flags);

/* Limit functions. */
cap_net_limit_t *cap_net_limit_init(cap_channel_t *chan, uint64_t mode);
int cap_net_limit(cap_net_limit_t *limit);
void cap_net_free(cap_net_limit_t *limit);

cap_net_limit_t *cap_net_limit_addr2name_family(cap_net_limit_t *limit,
    int *family, size_t size);
cap_net_limit_t *cap_net_limit_addr2name(cap_net_limit_t *limit,
    const struct sockaddr *sa, socklen_t salen);

cap_net_limit_t *cap_net_limit_name2addr_family(cap_net_limit_t *limit,
    int *family, size_t size);
cap_net_limit_t *cap_net_limit_name2addr(cap_net_limit_t *limit,
    const char *name, const char *serv);

cap_net_limit_t *cap_net_limit_connect(cap_net_limit_t *limit,
    const struct sockaddr *sa, socklen_t salen);

cap_net_limit_t *cap_net_limit_bind(cap_net_limit_t *limit,
    const struct sockaddr *sa, socklen_t salen);

/* Deprecated functions. */
struct hostent *cap_gethostbyname(cap_channel_t *chan, const char *name);
struct hostent *cap_gethostbyname2(cap_channel_t *chan, const char *name,
    int af);
struct hostent *cap_gethostbyaddr(cap_channel_t *chan, const void *addr,
    socklen_t len, int af);
#else
/* Capability functions. */
#define cap_bind(chan, s, addr, addrlen)					\
	bind(s, addr, addrlen)
#define cap_connect(chan, s, name, namelen)					\
	connect(s, name, namelen)
#define	cap_getaddrinfo(chan, hostname, servname, hints, res)			\
	getaddrinfo(hostname, servname, hints, res)
#define	cap_getnameinfo(chan, sa, salen, host, hostlen, serv, servlen, flags)	\
	getnameinfo(sa, salen, host, hostlen, serv, servlen, flags)

/* Limit functions. */
#define cap_net_limit_init(chan, mode)	((cap_net_limit_t *)malloc(8))
#define cap_net_free(limit)		free(limit)
static inline int
cap_net_limit(cap_net_limit_t *limit)
{
	free(limit);
	return (0);
}

static inline cap_net_limit_t *
cap_net_limit_addr2name_family(cap_net_limit_t *limit,
    int *family __unused, size_t size __unused)
{
	return (limit);
}

static inline cap_net_limit_t *
cap_net_limit_addr2name(cap_net_limit_t *limit,
    const struct sockaddr *sa __unused, socklen_t salen __unused)
{
	return (limit);
}

static inline cap_net_limit_t *
cap_net_limit_name2addr_family(cap_net_limit_t *limit,
    int *family __unused, size_t size __unused)
{
	return (limit);
}

static inline cap_net_limit_t *
cap_net_limit_name2addr(cap_net_limit_t *limit,
    const char *name __unused, const char *serv __unused)
{
	return (limit);
}

static inline cap_net_limit_t *
cap_net_limit_connect(cap_net_limit_t *limit,
    const struct sockaddr *sa __unused, socklen_t salen __unused)
{
	return (limit);
}

static inline cap_net_limit_t *
cap_net_limit_bind(cap_net_limit_t *limit,
    const struct sockaddr *sa __unused, socklen_t salen __unused)
{
	return (limit);
}

/* Deprecated functions. */
#define	cap_gethostbyname(chan, name)		 gethostbyname(name)
#define	cap_gethostbyname2(chan, name, type)	 gethostbyname2(name, type)
#define	cap_gethostbyaddr(chan, addr, len, type) gethostbyaddr(addr, len, type)
#endif

#endif	/* !_CAP_NETWORK_H_ */
