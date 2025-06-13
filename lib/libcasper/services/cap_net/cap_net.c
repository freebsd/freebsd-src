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
 */

#include <sys/cdefs.h>
#include <sys/cnv.h>
#include <sys/dnv.h>
#include <sys/nv.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <libcasper.h>
#include <libcasper_service.h>

#include "cap_net.h"

#define	CAPNET_MASK	(CAPNET_ADDR2NAME | CAPNET_NAME2ADDR	\
    CAPNET_DEPRECATED_ADDR2NAME | CAPNET_DEPRECATED_NAME2ADDR | \
    CAPNET_CONNECT | CAPNET_BIND | CAPNET_CONNECTDNS)

/*
 * Defines for the names of the limits.
 * XXX: we should convert all string constats to this to avoid typos.
 */
#define	LIMIT_NV_BIND			"bind"
#define	LIMIT_NV_CONNECT		"connect"
#define	LIMIT_NV_ADDR2NAME		"addr2name"
#define	LIMIT_NV_NAME2ADDR		"name2addr"

struct cap_net_limit {
	cap_channel_t	*cnl_chan;
	uint64_t	 cnl_mode;
	nvlist_t	*cnl_addr2name;
	nvlist_t	*cnl_name2addr;
	nvlist_t	*cnl_connect;
	nvlist_t	*cnl_bind;
};

static struct hostent hent;

static void
hostent_free(struct hostent *hp)
{
	unsigned int ii;

	free(hp->h_name);
	hp->h_name = NULL;
	if (hp->h_aliases != NULL) {
		for (ii = 0; hp->h_aliases[ii] != NULL; ii++)
			free(hp->h_aliases[ii]);
		free(hp->h_aliases);
		hp->h_aliases = NULL;
	}
	if (hp->h_addr_list != NULL) {
		for (ii = 0; hp->h_addr_list[ii] != NULL; ii++)
			free(hp->h_addr_list[ii]);
		free(hp->h_addr_list);
		hp->h_addr_list = NULL;
	}
}

static struct hostent *
hostent_unpack(const nvlist_t *nvl, struct hostent *hp)
{
	unsigned int ii, nitems;
	char nvlname[64];
	int n;

	hostent_free(hp);

	hp->h_name = strdup(nvlist_get_string(nvl, "name"));
	if (hp->h_name == NULL)
		goto fail;
	hp->h_addrtype = (int)nvlist_get_number(nvl, "addrtype");
	hp->h_length = (int)nvlist_get_number(nvl, "length");

	nitems = (unsigned int)nvlist_get_number(nvl, "naliases");
	hp->h_aliases = calloc(nitems + 1, sizeof(hp->h_aliases[0]));
	if (hp->h_aliases == NULL)
		goto fail;
	for (ii = 0; ii < nitems; ii++) {
		n = snprintf(nvlname, sizeof(nvlname), "alias%u", ii);
		assert(n > 0 && n < (int)sizeof(nvlname));
		hp->h_aliases[ii] =
		    strdup(nvlist_get_string(nvl, nvlname));
		if (hp->h_aliases[ii] == NULL)
			goto fail;
	}
	hp->h_aliases[ii] = NULL;

	nitems = (unsigned int)nvlist_get_number(nvl, "naddrs");
	hp->h_addr_list = calloc(nitems + 1, sizeof(hp->h_addr_list[0]));
	if (hp->h_addr_list == NULL)
		goto fail;
	for (ii = 0; ii < nitems; ii++) {
		hp->h_addr_list[ii] = malloc(hp->h_length);
		if (hp->h_addr_list[ii] == NULL)
			goto fail;
		n = snprintf(nvlname, sizeof(nvlname), "addr%u", ii);
		assert(n > 0 && n < (int)sizeof(nvlname));
		bcopy(nvlist_get_binary(nvl, nvlname, NULL),
		    hp->h_addr_list[ii], hp->h_length);
	}
	hp->h_addr_list[ii] = NULL;

	return (hp);
fail:
	hostent_free(hp);
	h_errno = NO_RECOVERY;
	return (NULL);
}

static int
request_cb(cap_channel_t *chan, const char *name, int s,
    const struct sockaddr *saddr, socklen_t len)
{
	nvlist_t *nvl;
	int serrno;

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "cmd", name);
	nvlist_add_descriptor(nvl, "s", s);
	nvlist_add_binary(nvl, "saddr", saddr, len);

	nvl = cap_xfer_nvlist(chan, nvl);
	if (nvl == NULL)
		return (-1);

	if (nvlist_get_number(nvl, "error") != 0) {
		serrno = (int)nvlist_get_number(nvl, "error");
		nvlist_destroy(nvl);
		errno = serrno;
		return (-1);
	}

	s = dup2(s, nvlist_get_descriptor(nvl, "s"));
	nvlist_destroy(nvl);

	return (s == -1 ? -1 : 0);
}

int
cap_bind(cap_channel_t *chan, int s, const struct sockaddr *addr,
    socklen_t addrlen)
{

	return (request_cb(chan, LIMIT_NV_BIND, s, addr, addrlen));
}

int
cap_connect(cap_channel_t *chan, int s, const struct sockaddr *name,
    socklen_t namelen)
{

	return (request_cb(chan, LIMIT_NV_CONNECT, s, name, namelen));
}


struct hostent *
cap_gethostbyname(cap_channel_t *chan, const char *name)
{

	return (cap_gethostbyname2(chan, name, AF_INET));
}

struct hostent *
cap_gethostbyname2(cap_channel_t *chan, const char *name, int af)
{
	struct hostent *hp;
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "cmd", "gethostbyname");
	nvlist_add_number(nvl, "family", (uint64_t)af);
	nvlist_add_string(nvl, "name", name);
	nvl = cap_xfer_nvlist(chan, nvl);
	if (nvl == NULL) {
		h_errno = NO_RECOVERY;
		return (NULL);
	}
	if (nvlist_get_number(nvl, "error") != 0) {
		h_errno = (int)nvlist_get_number(nvl, "error");
		nvlist_destroy(nvl);
		return (NULL);
	}

	hp = hostent_unpack(nvl, &hent);
	nvlist_destroy(nvl);
	return (hp);
}

struct hostent *
cap_gethostbyaddr(cap_channel_t *chan, const void *addr, socklen_t len,
    int af)
{
	struct hostent *hp;
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "cmd", "gethostbyaddr");
	nvlist_add_binary(nvl, "addr", addr, (size_t)len);
	nvlist_add_number(nvl, "family", (uint64_t)af);
	nvl = cap_xfer_nvlist(chan, nvl);
	if (nvl == NULL) {
		h_errno = NO_RECOVERY;
		return (NULL);
	}
	if (nvlist_get_number(nvl, "error") != 0) {
		h_errno = (int)nvlist_get_number(nvl, "error");
		nvlist_destroy(nvl);
		return (NULL);
	}
	hp = hostent_unpack(nvl, &hent);
	nvlist_destroy(nvl);
	return (hp);
}

static struct addrinfo *
addrinfo_unpack(const nvlist_t *nvl)
{
	struct addrinfo *ai;
	const void *addr;
	size_t addrlen;
	const char *canonname;

	addr = nvlist_get_binary(nvl, "ai_addr", &addrlen);
	ai = malloc(sizeof(*ai) + addrlen);
	if (ai == NULL)
		return (NULL);
	ai->ai_flags = (int)nvlist_get_number(nvl, "ai_flags");
	ai->ai_family = (int)nvlist_get_number(nvl, "ai_family");
	ai->ai_socktype = (int)nvlist_get_number(nvl, "ai_socktype");
	ai->ai_protocol = (int)nvlist_get_number(nvl, "ai_protocol");
	ai->ai_addrlen = (socklen_t)addrlen;
	canonname = dnvlist_get_string(nvl, "ai_canonname", NULL);
	if (canonname != NULL) {
		ai->ai_canonname = strdup(canonname);
		if (ai->ai_canonname == NULL) {
			free(ai);
			return (NULL);
		}
	} else {
		ai->ai_canonname = NULL;
	}
	ai->ai_addr = (void *)(ai + 1);
	bcopy(addr, ai->ai_addr, addrlen);
	ai->ai_next = NULL;

	return (ai);
}

int
cap_getaddrinfo(cap_channel_t *chan, const char *hostname, const char *servname,
    const struct addrinfo *hints, struct addrinfo **res)
{
	struct addrinfo *firstai, *prevai, *curai;
	unsigned int ii;
	const nvlist_t *nvlai;
	char nvlname[64];
	nvlist_t *nvl;
	int error, serrno, n;

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "cmd", "getaddrinfo");
	if (hostname != NULL)
		nvlist_add_string(nvl, "hostname", hostname);
	if (servname != NULL)
		nvlist_add_string(nvl, "servname", servname);
	if (hints != NULL) {
		nvlist_add_number(nvl, "hints.ai_flags",
		    (uint64_t)hints->ai_flags);
		nvlist_add_number(nvl, "hints.ai_family",
		    (uint64_t)hints->ai_family);
		nvlist_add_number(nvl, "hints.ai_socktype",
		    (uint64_t)hints->ai_socktype);
		nvlist_add_number(nvl, "hints.ai_protocol",
		    (uint64_t)hints->ai_protocol);
	}
	nvl = cap_xfer_nvlist(chan, nvl);
	if (nvl == NULL)
		return (EAI_MEMORY);
	if (nvlist_get_number(nvl, "error") != 0) {
		error = (int)nvlist_get_number(nvl, "error");
		serrno = dnvlist_get_number(nvl, "errno", 0);
		nvlist_destroy(nvl);
		errno = (error == EAI_SYSTEM) ? serrno : 0;
		return (error);
	}

	nvlai = NULL;
	firstai = prevai = curai = NULL;
	for (ii = 0; ; ii++) {
		n = snprintf(nvlname, sizeof(nvlname), "res%u", ii);
		assert(n > 0 && n < (int)sizeof(nvlname));
		if (!nvlist_exists_nvlist(nvl, nvlname))
			break;
		nvlai = nvlist_get_nvlist(nvl, nvlname);
		curai = addrinfo_unpack(nvlai);
		if (curai == NULL) {
			nvlist_destroy(nvl);
			return (EAI_MEMORY);
		}
		if (prevai != NULL)
			prevai->ai_next = curai;
		else
			firstai = curai;
		prevai = curai;
	}
	nvlist_destroy(nvl);
	if (curai == NULL && nvlai != NULL) {
		if (firstai == NULL)
			freeaddrinfo(firstai);
		return (EAI_MEMORY);
	}

	*res = firstai;
	return (0);
}

int
cap_getnameinfo(cap_channel_t *chan, const struct sockaddr *sa, socklen_t salen,
    char *host, size_t hostlen, char *serv, size_t servlen, int flags)
{
	nvlist_t *nvl;
	int error, serrno;

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "cmd", "getnameinfo");
	nvlist_add_number(nvl, "hostlen", (uint64_t)hostlen);
	nvlist_add_number(nvl, "servlen", (uint64_t)servlen);
	nvlist_add_binary(nvl, "sa", sa, (size_t)salen);
	nvlist_add_number(nvl, "flags", (uint64_t)flags);
	nvl = cap_xfer_nvlist(chan, nvl);
	if (nvl == NULL)
		return (EAI_MEMORY);
	if (nvlist_get_number(nvl, "error") != 0) {
		error = (int)nvlist_get_number(nvl, "error");
		serrno = dnvlist_get_number(nvl, "errno", 0);
		nvlist_destroy(nvl);
		errno = (error == EAI_SYSTEM) ? serrno : 0;
		return (error);
	}

	if (host != NULL && nvlist_exists_string(nvl, "host"))
		strlcpy(host, nvlist_get_string(nvl, "host"), hostlen);
	if (serv != NULL && nvlist_exists_string(nvl, "serv"))
		strlcpy(serv, nvlist_get_string(nvl, "serv"), servlen);
	nvlist_destroy(nvl);
	return (0);
}

cap_net_limit_t *
cap_net_limit_init(cap_channel_t *chan, uint64_t mode)
{
	cap_net_limit_t *limit;

	limit = calloc(1, sizeof(*limit));
	if (limit != NULL) {
		limit->cnl_mode = mode;
		limit->cnl_chan = chan;
		limit->cnl_addr2name = nvlist_create(0);
		limit->cnl_name2addr = nvlist_create(0);
		limit->cnl_connect = nvlist_create(0);
		limit->cnl_bind = nvlist_create(0);
	}

	return (limit);
}

static void
pack_limit(nvlist_t *lnvl, const char *name, nvlist_t *limit)
{

	if (!nvlist_empty(limit)) {
		nvlist_move_nvlist(lnvl, name, limit);
	} else {
		nvlist_destroy(limit);
	}
}

int
cap_net_limit(cap_net_limit_t *limit)
{
	nvlist_t *lnvl;
	cap_channel_t *chan;

	lnvl = nvlist_create(0);
	nvlist_add_number(lnvl, "mode", limit->cnl_mode);

	pack_limit(lnvl, LIMIT_NV_ADDR2NAME, limit->cnl_addr2name);
	pack_limit(lnvl, LIMIT_NV_NAME2ADDR, limit->cnl_name2addr);
	pack_limit(lnvl, LIMIT_NV_CONNECT, limit->cnl_connect);
	pack_limit(lnvl, LIMIT_NV_BIND, limit->cnl_bind);

	chan = limit->cnl_chan;
	free(limit);

	return (cap_limit_set(chan, lnvl));
}

void
cap_net_free(cap_net_limit_t *limit)
{

	if (limit == NULL)
		return;

	nvlist_destroy(limit->cnl_addr2name);
	nvlist_destroy(limit->cnl_name2addr);
	nvlist_destroy(limit->cnl_connect);
	nvlist_destroy(limit->cnl_bind);

	free(limit);
}

static void
pack_family(nvlist_t *nvl, int *family, size_t size)
{
	size_t i;

	i = 0;
	if (!nvlist_exists_number_array(nvl, "family")) {
		uint64_t val;

		val = family[0];
		nvlist_add_number_array(nvl, "family", &val, 1);
		i += 1;
	}

	for (; i < size; i++) {
		nvlist_append_number_array(nvl, "family", family[i]);
	}
}

static void
pack_sockaddr(nvlist_t *res, const struct sockaddr *sa, socklen_t salen)
{
	nvlist_t *nvl;

	if (!nvlist_exists_nvlist(res, "sockaddr")) {
		nvl = nvlist_create(NV_FLAG_NO_UNIQUE);
	} else {
		nvl = nvlist_take_nvlist(res, "sockaddr");
	}

	nvlist_add_binary(nvl, "", sa, salen);
	nvlist_move_nvlist(res, "sockaddr", nvl);
}

cap_net_limit_t *
cap_net_limit_addr2name_family(cap_net_limit_t *limit, int *family, size_t size)
{

	pack_family(limit->cnl_addr2name, family, size);
	return (limit);
}

cap_net_limit_t *
cap_net_limit_name2addr_family(cap_net_limit_t *limit, int *family, size_t size)
{

	pack_family(limit->cnl_name2addr, family, size);
	return (limit);
}

cap_net_limit_t *
cap_net_limit_name2addr(cap_net_limit_t *limit, const char *host,
    const char *serv)
{
	nvlist_t *nvl;

	if (!nvlist_exists_nvlist(limit->cnl_name2addr, "hosts")) {
		nvl = nvlist_create(NV_FLAG_NO_UNIQUE);
	} else {
		nvl = nvlist_take_nvlist(limit->cnl_name2addr, "hosts");
	}

	nvlist_add_string(nvl,
	    host != NULL ? host : "",
	    serv != NULL ? serv : "");

	nvlist_move_nvlist(limit->cnl_name2addr, "hosts", nvl);
	return (limit);
}

cap_net_limit_t *
cap_net_limit_addr2name(cap_net_limit_t *limit, const struct sockaddr *sa,
    socklen_t salen)
{

	pack_sockaddr(limit->cnl_addr2name, sa, salen);
	return (limit);
}


cap_net_limit_t *
cap_net_limit_connect(cap_net_limit_t *limit, const struct sockaddr *sa,
    socklen_t salen)
{

	pack_sockaddr(limit->cnl_connect, sa, salen);
	return (limit);
}

cap_net_limit_t *
cap_net_limit_bind(cap_net_limit_t *limit, const struct sockaddr *sa,
    socklen_t salen)
{

	pack_sockaddr(limit->cnl_bind, sa, salen);
	return (limit);
}

/*
 * Service functions.
 */

static nvlist_t *capdnscache;

static void
net_add_sockaddr_to_cache(struct sockaddr *sa, socklen_t salen, bool deprecated)
{
	void *cookie;

	if (capdnscache == NULL) {
		capdnscache = nvlist_create(NV_FLAG_NO_UNIQUE);
	} else {
		/* Lets keep it clean. Look for dups. */
		cookie = NULL;
		while (nvlist_next(capdnscache, NULL, &cookie) != NULL) {
			const void *data;
			size_t size;

			assert(cnvlist_type(cookie) == NV_TYPE_BINARY);

			data = cnvlist_get_binary(cookie, &size);
			if (salen != size)
				continue;
			if (memcmp(data, sa, size) == 0)
				return;
		}
	}

	nvlist_add_binary(capdnscache, deprecated ? "d" : "", sa, salen);
}

static void
net_add_hostent_to_cache(const char *address, size_t asize, int family)
{

	if (family != AF_INET && family != AF_INET6)
		return;

	if (family == AF_INET6) {
		struct sockaddr_in6 connaddr;

		memset(&connaddr, 0, sizeof(connaddr));
		connaddr.sin6_family = AF_INET6;
		memcpy((char *)&connaddr.sin6_addr, address, asize);
		connaddr.sin6_port = 0;

		net_add_sockaddr_to_cache((struct sockaddr *)&connaddr,
		    sizeof(connaddr), true);
	} else {
		struct sockaddr_in connaddr;

		memset(&connaddr, 0, sizeof(connaddr));
		connaddr.sin_family = AF_INET;
		memcpy((char *)&connaddr.sin_addr.s_addr, address, asize);
		connaddr.sin_port = 0;

		net_add_sockaddr_to_cache((struct sockaddr *)&connaddr,
		    sizeof(connaddr), true);
	}
}

static bool
net_allowed_mode(const nvlist_t *limits, uint64_t mode)
{

	if (limits == NULL)
		return (true);

	return ((nvlist_get_number(limits, "mode") & mode) == mode);
}

static bool
net_allowed_family(const nvlist_t *limits, int family)
{
	const uint64_t *allowedfamily;
	size_t i, allsize;

	if (limits == NULL)
		return (true);

	/* If there are no familes at all, allow any mode. */
	if (!nvlist_exists_number_array(limits, "family"))
		return (true);

	allowedfamily = nvlist_get_number_array(limits, "family", &allsize);
	for (i = 0; i < allsize; i++) {
		/* XXX: what with AF_UNSPEC? */
		if (allowedfamily[i] == (uint64_t)family) {
			return (true);
		}
	}

	return (false);
}

static bool
net_allowed_bsaddr_impl(const nvlist_t *salimits, const void *saddr,
    size_t saddrsize)
{
	void *cookie;
	const void *limit;
	size_t limitsize;

	cookie = NULL;
	while (nvlist_next(salimits, NULL, &cookie) != NULL) {
		limit = cnvlist_get_binary(cookie, &limitsize);

		if (limitsize != saddrsize) {
			continue;
		}
		if (memcmp(limit, saddr, limitsize) == 0) {
			return (true);
		}

		/*
		 * In case of deprecated version (gethostbyname) we have to
		 * ignore port, because there is no such info in the hostent.
		 * Suporting only AF_INET and AF_INET6.
		 */
		if (strcmp(cnvlist_name(cookie), "d") != 0 ||
		    (saddrsize != sizeof(struct sockaddr_in) &&
		    saddrsize != sizeof(struct sockaddr_in6))) {
			continue;
		}
		if (saddrsize == sizeof(struct sockaddr_in)) {
			const struct sockaddr_in *saddrptr;
			struct sockaddr_in sockaddr;

			saddrptr = (const struct sockaddr_in *)saddr;
			memcpy(&sockaddr, limit, sizeof(sockaddr));
			sockaddr.sin_port = saddrptr->sin_port;

			if (memcmp(&sockaddr, saddr, saddrsize) == 0) {
				return (true);
			}
		} else if (saddrsize == sizeof(struct sockaddr_in6)) {
			const struct sockaddr_in6 *saddrptr;
			struct sockaddr_in6 sockaddr;

			saddrptr = (const struct sockaddr_in6 *)saddr;
			memcpy(&sockaddr, limit, sizeof(sockaddr));
			sockaddr.sin6_port = saddrptr->sin6_port;

			if (memcmp(&sockaddr, saddr, saddrsize) == 0) {
				return (true);
			}
		}
	}

	return (false);
}

static bool
net_allowed_bsaddr(const nvlist_t *limits, const void *saddr, size_t saddrsize)
{

	if (limits == NULL)
		return (true);

	if (!nvlist_exists_nvlist(limits, "sockaddr"))
		return (true);

	return (net_allowed_bsaddr_impl(nvlist_get_nvlist(limits, "sockaddr"),
	    saddr, saddrsize));
}

static bool
net_allowed_hosts(const nvlist_t *limits, const char *name, const char *srvname)
{
	void *cookie;
	const nvlist_t *hlimits;
	const char *testname, *testsrvname;

	if (limits == NULL) {
		return (true);
	}

	/* If there are no hosts at all, allow any. */
	if (!nvlist_exists_nvlist(limits, "hosts")) {
		return (true);
	}

	cookie = NULL;
	testname = (name == NULL ? "" : name);
	testsrvname = (srvname == NULL ? "" : srvname);
	hlimits = nvlist_get_nvlist(limits, "hosts");
	while (nvlist_next(hlimits, NULL, &cookie) != NULL) {
		if (strcmp(cnvlist_name(cookie), "") != 0 &&
		    strcmp(cnvlist_name(cookie), testname) != 0) {
			continue;
		}

		if (strcmp(cnvlist_get_string(cookie), "") != 0 &&
		    strcmp(cnvlist_get_string(cookie), testsrvname) != 0) {
			continue;
		}

		return (true);
	}

	return (false);
}

static void
hostent_pack(const struct hostent *hp, nvlist_t *nvl, bool addtocache)
{
	unsigned int ii;
	char nvlname[64];
	int n;

	nvlist_add_string(nvl, "name", hp->h_name);
	nvlist_add_number(nvl, "addrtype", (uint64_t)hp->h_addrtype);
	nvlist_add_number(nvl, "length", (uint64_t)hp->h_length);

	if (hp->h_aliases == NULL) {
		nvlist_add_number(nvl, "naliases", 0);
	} else {
		for (ii = 0; hp->h_aliases[ii] != NULL; ii++) {
			n = snprintf(nvlname, sizeof(nvlname), "alias%u", ii);
			assert(n > 0 && n < (int)sizeof(nvlname));
			nvlist_add_string(nvl, nvlname, hp->h_aliases[ii]);
		}
		nvlist_add_number(nvl, "naliases", (uint64_t)ii);
	}

	if (hp->h_addr_list == NULL) {
		nvlist_add_number(nvl, "naddrs", 0);
	} else {
		for (ii = 0; hp->h_addr_list[ii] != NULL; ii++) {
			n = snprintf(nvlname, sizeof(nvlname), "addr%u", ii);
			assert(n > 0 && n < (int)sizeof(nvlname));
			nvlist_add_binary(nvl, nvlname, hp->h_addr_list[ii],
			    (size_t)hp->h_length);
			if (addtocache) {
				net_add_hostent_to_cache(hp->h_addr_list[ii],
				    hp->h_length, hp->h_addrtype);
			}
		}
		nvlist_add_number(nvl, "naddrs", (uint64_t)ii);
	}
}

static int
net_gethostbyname(const nvlist_t *limits, const nvlist_t *nvlin,
    nvlist_t *nvlout)
{
	struct hostent *hp;
	int family;
	const nvlist_t *funclimit;
	const char *name;
	bool dnscache;

	if (!net_allowed_mode(limits, CAPNET_DEPRECATED_NAME2ADDR))
		return (ENOTCAPABLE);

	dnscache = net_allowed_mode(limits, CAPNET_CONNECTDNS);
	funclimit = NULL;
	if (limits != NULL) {
		funclimit = dnvlist_get_nvlist(limits, LIMIT_NV_NAME2ADDR,
		    NULL);
	}

	family = (int)nvlist_get_number(nvlin, "family");
	if (!net_allowed_family(funclimit, family))
		return (ENOTCAPABLE);

	name = nvlist_get_string(nvlin, "name");
	if (!net_allowed_hosts(funclimit, name, ""))
		return (ENOTCAPABLE);

	hp = gethostbyname2(name, family);
	if (hp == NULL)
		return (h_errno);
	hostent_pack(hp, nvlout, dnscache);
	return (0);
}

static int
net_gethostbyaddr(const nvlist_t *limits, const nvlist_t *nvlin,
    nvlist_t *nvlout)
{
	struct hostent *hp;
	const void *addr;
	size_t addrsize;
	int family;
	const nvlist_t *funclimit;

	if (!net_allowed_mode(limits, CAPNET_DEPRECATED_ADDR2NAME))
		return (ENOTCAPABLE);

	funclimit = NULL;
	if (limits != NULL) {
		funclimit = dnvlist_get_nvlist(limits, LIMIT_NV_ADDR2NAME,
		    NULL);
	}

	family = (int)nvlist_get_number(nvlin, "family");
	if (!net_allowed_family(funclimit, family))
		return (ENOTCAPABLE);

	addr = nvlist_get_binary(nvlin, "addr", &addrsize);
	if (!net_allowed_bsaddr(funclimit, addr, addrsize))
		return (ENOTCAPABLE);

	hp = gethostbyaddr(addr, (socklen_t)addrsize, family);
	if (hp == NULL)
		return (h_errno);
	hostent_pack(hp, nvlout, false);
	return (0);
}

static int
net_getnameinfo(const nvlist_t *limits, const nvlist_t *nvlin, nvlist_t *nvlout)
{
	struct sockaddr_storage sast;
	const void *sabin;
	char *host, *serv;
	size_t sabinsize, hostlen, servlen;
	socklen_t salen;
	int error, serrno, flags;
	const nvlist_t *funclimit;

	host = serv = NULL;
	if (!net_allowed_mode(limits, CAPNET_ADDR2NAME)) {
		serrno = ENOTCAPABLE;
		error = EAI_SYSTEM;
		goto out;
	}
	funclimit = NULL;
	if (limits != NULL) {
		funclimit = dnvlist_get_nvlist(limits, LIMIT_NV_ADDR2NAME,
		    NULL);
	}
	error = 0;
	memset(&sast, 0, sizeof(sast));

	hostlen = (size_t)nvlist_get_number(nvlin, "hostlen");
	servlen = (size_t)nvlist_get_number(nvlin, "servlen");

	if (hostlen > 0) {
		host = calloc(1, hostlen);
		if (host == NULL) {
			error = EAI_MEMORY;
			goto out;
		}
	}
	if (servlen > 0) {
		serv = calloc(1, servlen);
		if (serv == NULL) {
			error = EAI_MEMORY;
			goto out;
		}
	}

	sabin = nvlist_get_binary(nvlin, "sa", &sabinsize);
	if (sabinsize > sizeof(sast)) {
		error = EAI_FAIL;
		goto out;
	}
	if (!net_allowed_bsaddr(funclimit, sabin, sabinsize)) {
		serrno = ENOTCAPABLE;
		error = EAI_SYSTEM;
		goto out;
	}

	memcpy(&sast, sabin, sabinsize);
	salen = (socklen_t)sabinsize;

	if ((sast.ss_family != AF_INET ||
	     salen != sizeof(struct sockaddr_in)) &&
	    (sast.ss_family != AF_INET6 ||
	     salen != sizeof(struct sockaddr_in6))) {
		error = EAI_FAIL;
		goto out;
	}

	if (!net_allowed_family(funclimit, (int)sast.ss_family)) {
		serrno = ENOTCAPABLE;
		error = EAI_SYSTEM;
		goto out;
	}

	flags = (int)nvlist_get_number(nvlin, "flags");

	error = getnameinfo((struct sockaddr *)&sast, salen, host, hostlen,
	    serv, servlen, flags);
	serrno = errno;
	if (error != 0)
		goto out;

	if (host != NULL)
		nvlist_move_string(nvlout, "host", host);
	if (serv != NULL)
		nvlist_move_string(nvlout, "serv", serv);
out:
	if (error != 0) {
		free(host);
		free(serv);
		if (error == EAI_SYSTEM)
			nvlist_add_number(nvlout, "errno", serrno);
	}
	return (error);
}

static nvlist_t *
addrinfo_pack(const struct addrinfo *ai)
{
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	nvlist_add_number(nvl, "ai_flags", (uint64_t)ai->ai_flags);
	nvlist_add_number(nvl, "ai_family", (uint64_t)ai->ai_family);
	nvlist_add_number(nvl, "ai_socktype", (uint64_t)ai->ai_socktype);
	nvlist_add_number(nvl, "ai_protocol", (uint64_t)ai->ai_protocol);
	nvlist_add_binary(nvl, "ai_addr", ai->ai_addr, (size_t)ai->ai_addrlen);
	if (ai->ai_canonname != NULL)
		nvlist_add_string(nvl, "ai_canonname", ai->ai_canonname);

	return (nvl);
}

static int
net_getaddrinfo(const nvlist_t *limits, const nvlist_t *nvlin, nvlist_t *nvlout)
{
	struct addrinfo hints, *hintsp, *res, *cur;
	const char *hostname, *servname;
	char nvlname[64];
	nvlist_t *elem;
	unsigned int ii;
	int error, serrno, family, n;
	const nvlist_t *funclimit;
	bool dnscache;

	if (!net_allowed_mode(limits, CAPNET_NAME2ADDR)) {
		serrno = ENOTCAPABLE;
		error = EAI_SYSTEM;
		goto out;
	}
	dnscache = net_allowed_mode(limits, CAPNET_CONNECTDNS);
	funclimit = NULL;
	if (limits != NULL) {
		funclimit = dnvlist_get_nvlist(limits, LIMIT_NV_NAME2ADDR,
		    NULL);
	}

	hostname = dnvlist_get_string(nvlin, "hostname", NULL);
	servname = dnvlist_get_string(nvlin, "servname", NULL);
	if (nvlist_exists_number(nvlin, "hints.ai_flags")) {
		hints.ai_flags = (int)nvlist_get_number(nvlin,
		    "hints.ai_flags");
		hints.ai_family = (int)nvlist_get_number(nvlin,
		    "hints.ai_family");
		hints.ai_socktype = (int)nvlist_get_number(nvlin,
		    "hints.ai_socktype");
		hints.ai_protocol = (int)nvlist_get_number(nvlin,
		    "hints.ai_protocol");
		hints.ai_addrlen = 0;
		hints.ai_addr = NULL;
		hints.ai_canonname = NULL;
		hints.ai_next = NULL;
		hintsp = &hints;
		family = hints.ai_family;
	} else {
		hintsp = NULL;
		family = AF_UNSPEC;
	}

	if (!net_allowed_family(funclimit, family)) {
		errno = ENOTCAPABLE;
		error = EAI_SYSTEM;
		goto out;
	}
	if (!net_allowed_hosts(funclimit, hostname, servname)) {
		errno = ENOTCAPABLE;
		error = EAI_SYSTEM;
		goto out;
	}
	error = getaddrinfo(hostname, servname, hintsp, &res);
	serrno = errno;
	if (error != 0) {
		goto out;
	}

	for (cur = res, ii = 0; cur != NULL; cur = cur->ai_next, ii++) {
		elem = addrinfo_pack(cur);
		n = snprintf(nvlname, sizeof(nvlname), "res%u", ii);
		assert(n > 0 && n < (int)sizeof(nvlname));
		nvlist_move_nvlist(nvlout, nvlname, elem);
		if (dnscache) {
			net_add_sockaddr_to_cache(cur->ai_addr,
			    cur->ai_addrlen, false);
		}
	}

	freeaddrinfo(res);
	error = 0;
out:
	if (error == EAI_SYSTEM)
		nvlist_add_number(nvlout, "errno", serrno);
	return (error);
}

static int
net_bind(const nvlist_t *limits, nvlist_t *nvlin, nvlist_t *nvlout)
{
	int socket, serrno;
	const void *saddr;
	size_t len;
	const nvlist_t *funclimit;

	if (!net_allowed_mode(limits, CAPNET_BIND))
		return (ENOTCAPABLE);
	funclimit = NULL;
	if (limits != NULL)
		funclimit = dnvlist_get_nvlist(limits, LIMIT_NV_BIND, NULL);

	saddr = nvlist_get_binary(nvlin, "saddr", &len);

	if (!net_allowed_bsaddr(funclimit, saddr, len))
		return (ENOTCAPABLE);

	socket = nvlist_take_descriptor(nvlin, "s");
	if (bind(socket, saddr, len) < 0) {
		serrno = errno;
		close(socket);
		return (serrno);
	}

	nvlist_move_descriptor(nvlout, "s", socket);

	return (0);
}

static int
net_connect(const nvlist_t *limits, nvlist_t *nvlin, nvlist_t *nvlout)
{
	int socket, serrno;
	const void *saddr;
	const nvlist_t *funclimit;
	size_t len;
	bool conn, conndns, allowed;

	conn = net_allowed_mode(limits, CAPNET_CONNECT);
	conndns = net_allowed_mode(limits, CAPNET_CONNECTDNS);

	if (!conn && !conndns)
		return (ENOTCAPABLE);

	funclimit = NULL;
	if (limits != NULL)
		funclimit = dnvlist_get_nvlist(limits, LIMIT_NV_CONNECT, NULL);

	saddr = nvlist_get_binary(nvlin, "saddr", &len);
	allowed = false;

	if (conn && net_allowed_bsaddr(funclimit, saddr, len)) {
		allowed = true;
	}
	if (conndns && capdnscache != NULL &&
	   net_allowed_bsaddr_impl(capdnscache, saddr, len)) {
		allowed = true;
	}

	if (allowed == false) {
		return (ENOTCAPABLE);
	}

	socket = dup(nvlist_get_descriptor(nvlin, "s"));
	if (connect(socket, saddr, len) < 0) {
		serrno = errno;
		close(socket);
		return (serrno);
	}

	nvlist_move_descriptor(nvlout, "s", socket);

	return (0);
}

static bool
verify_only_sa_newlimts(const nvlist_t *oldfunclimits,
    const nvlist_t *newfunclimit)
{
	void *cookie;

	cookie = NULL;
	while (nvlist_next(newfunclimit, NULL, &cookie) != NULL) {
		void *sacookie;

		if (strcmp(cnvlist_name(cookie), "sockaddr") != 0)
			return (false);

		if (cnvlist_type(cookie) != NV_TYPE_NVLIST)
			return (false);

		sacookie = NULL;
		while (nvlist_next(cnvlist_get_nvlist(cookie), NULL,
		    &sacookie) != NULL) {
			const void *sa;
			size_t sasize;

			if (cnvlist_type(sacookie) != NV_TYPE_BINARY)
				return (false);

			sa = cnvlist_get_binary(sacookie, &sasize);
			if (!net_allowed_bsaddr(oldfunclimits, sa, sasize))
				return (false);
		}
	}

	return (true);
}

static bool
verify_bind_newlimts(const nvlist_t *oldlimits,
    const nvlist_t *newfunclimit)
{
	const nvlist_t *oldfunclimits;

	oldfunclimits = NULL;
	if (oldlimits != NULL) {
		oldfunclimits = dnvlist_get_nvlist(oldlimits, LIMIT_NV_BIND,
		    NULL);
	}

	return (verify_only_sa_newlimts(oldfunclimits, newfunclimit));
}


static bool
verify_connect_newlimits(const nvlist_t *oldlimits,
    const nvlist_t *newfunclimit)
{
	const nvlist_t *oldfunclimits;

	oldfunclimits = NULL;
	if (oldlimits != NULL) {
		oldfunclimits = dnvlist_get_nvlist(oldlimits, LIMIT_NV_CONNECT,
		    NULL);
	}

	return (verify_only_sa_newlimts(oldfunclimits, newfunclimit));
}

static bool
verify_addr2name_newlimits(const nvlist_t *oldlimits,
    const nvlist_t *newfunclimit)
{
	void *cookie;
	const nvlist_t *oldfunclimits;

	oldfunclimits = NULL;
	if (oldlimits != NULL) {
		oldfunclimits = dnvlist_get_nvlist(oldlimits,
		    LIMIT_NV_ADDR2NAME, NULL);
	}

	cookie = NULL;
	while (nvlist_next(newfunclimit, NULL, &cookie) != NULL) {
		if (strcmp(cnvlist_name(cookie), "sockaddr") == 0) {
			void *sacookie;

			if (cnvlist_type(cookie) != NV_TYPE_NVLIST)
				return (false);

			sacookie = NULL;
			while (nvlist_next(cnvlist_get_nvlist(cookie), NULL,
			    &sacookie) != NULL) {
				const void *sa;
				size_t sasize;

				if (cnvlist_type(sacookie) != NV_TYPE_BINARY)
					return (false);

				sa = cnvlist_get_binary(sacookie, &sasize);
				if (!net_allowed_bsaddr(oldfunclimits, sa,
				    sasize)) {
					return (false);
				}
			}
		} else if (strcmp(cnvlist_name(cookie), "family") == 0) {
			size_t i, sfamilies;
			const uint64_t *families;

			if (cnvlist_type(cookie) != NV_TYPE_NUMBER_ARRAY)
				return (false);

			families = cnvlist_get_number_array(cookie, &sfamilies);
			for (i = 0; i < sfamilies; i++) {
				if (!net_allowed_family(oldfunclimits,
				    families[i])) {
					return (false);
				}
			}
		} else {
			return (false);
		}
	}

	return (true);
}

static bool
verify_name2addr_newlimits(const nvlist_t *oldlimits,
    const nvlist_t *newfunclimit)
{
	void *cookie;
	const nvlist_t *oldfunclimits;

	oldfunclimits = NULL;
	if (oldlimits != NULL) {
		oldfunclimits = dnvlist_get_nvlist(oldlimits,
		    LIMIT_NV_NAME2ADDR, NULL);
	}

	cookie = NULL;
	while (nvlist_next(newfunclimit, NULL, &cookie) != NULL) {
		if (strcmp(cnvlist_name(cookie), "hosts") == 0) {
			void *hostcookie;

			if (cnvlist_type(cookie) != NV_TYPE_NVLIST)
				return (false);

			hostcookie = NULL;
			while (nvlist_next(cnvlist_get_nvlist(cookie), NULL,
			    &hostcookie) != NULL) {
				if (cnvlist_type(hostcookie) != NV_TYPE_STRING)
					return (false);

				if (!net_allowed_hosts(oldfunclimits,
				    cnvlist_name(hostcookie),
				    cnvlist_get_string(hostcookie))) {
					return (false);
				}
			}
		} else if (strcmp(cnvlist_name(cookie), "family") == 0) {
			size_t i, sfamilies;
			const uint64_t *families;

			if (cnvlist_type(cookie) != NV_TYPE_NUMBER_ARRAY)
				return (false);

			families = cnvlist_get_number_array(cookie, &sfamilies);
			for (i = 0; i < sfamilies; i++) {
				if (!net_allowed_family(oldfunclimits,
				    families[i])) {
					return (false);
				}
			}
		} else {
			return (false);
		}
	}

	return (true);
}

static int
net_limit(const nvlist_t *oldlimits, const nvlist_t *newlimits)
{
	const char *name;
	void *cookie;
	bool hasmode, hasconnect, hasbind, hasaddr2name, hasname2addr;

	/*
	 * Modes:
	 *	ADDR2NAME:
	 *		getnameinfo
	 *	DEPRECATED_ADDR2NAME:
	 *		gethostbyaddr
	 *
	 *	NAME2ADDR:
	 *		getaddrinfo
	 *	DEPRECATED_NAME2ADDR:
	 *		gethostbyname
	 *
	 * Limit scheme:
	 *	mode	: NV_TYPE_NUMBER
	 *	connect : NV_TYPE_NVLIST
	 *		sockaddr : NV_TYPE_NVLIST
	 *			""	: NV_TYPE_BINARY
	 *			...	: NV_TYPE_BINARY
	 *	bind	: NV_TYPE_NVLIST
	 *		sockaddr : NV_TYPE_NVLIST
	 *			""	: NV_TYPE_BINARY
	 *			...	: NV_TYPE_BINARY
	 *	addr2name : NV_TYPE_NVLIST
	 *		family  : NV_TYPE_NUMBER_ARRAY
	 *		sockaddr : NV_TYPE_NVLIST
	 *			""	: NV_TYPE_BINARY
	 *			...	: NV_TYPE_BINARY
	 *	name2addr : NV_TYPE_NVLIST
	 *		family : NV_TYPE_NUMBER
	 *		hosts	: NV_TYPE_NVLIST
	 *			host	: servname : NV_TYPE_STRING
	 */

	hasmode = false;
	hasconnect = false;
	hasbind = false;
	hasaddr2name = false;
	hasname2addr = false;

	cookie = NULL;
	while ((name = nvlist_next(newlimits, NULL, &cookie)) != NULL) {
		if (strcmp(name, "mode") == 0) {
			if (cnvlist_type(cookie) != NV_TYPE_NUMBER) {
				return (NO_RECOVERY);
			}
			if (!net_allowed_mode(oldlimits,
			    cnvlist_get_number(cookie))) {
				return (ENOTCAPABLE);
			}
			hasmode = true;
			continue;
		}

		if (cnvlist_type(cookie) != NV_TYPE_NVLIST) {
			return (NO_RECOVERY);
		}

		if (strcmp(name, LIMIT_NV_BIND) == 0) {
			hasbind = true;
			if (!verify_bind_newlimts(oldlimits,
			    cnvlist_get_nvlist(cookie))) {
				return (ENOTCAPABLE);
			}
		} else if (strcmp(name, LIMIT_NV_CONNECT) == 0) {
			hasconnect = true;
			if (!verify_connect_newlimits(oldlimits,
			    cnvlist_get_nvlist(cookie))) {
				return (ENOTCAPABLE);
			}
		} else if (strcmp(name, LIMIT_NV_ADDR2NAME) == 0) {
			hasaddr2name = true;
			if (!verify_addr2name_newlimits(oldlimits,
			    cnvlist_get_nvlist(cookie))) {
				return (ENOTCAPABLE);
			}
		} else if (strcmp(name, LIMIT_NV_NAME2ADDR) == 0) {
			hasname2addr = true;
			if (!verify_name2addr_newlimits(oldlimits,
			    cnvlist_get_nvlist(cookie))) {
				return (ENOTCAPABLE);
			}
		}
	}

	/* Mode is required. */
	if (!hasmode)
		return (ENOTCAPABLE);

	/*
	 * If the new limit doesn't mention mode or family we have to
	 * check if the current limit does have those. Missing mode or
	 * family in the limit means that all modes or families are
	 * allowed.
	 */
	if (oldlimits == NULL)
		return (0);
	if (!hasbind && nvlist_exists(oldlimits, LIMIT_NV_BIND))
		return (ENOTCAPABLE);
	if (!hasconnect && nvlist_exists(oldlimits, LIMIT_NV_CONNECT))
		return (ENOTCAPABLE);
	if (!hasaddr2name && nvlist_exists(oldlimits, LIMIT_NV_ADDR2NAME))
		return (ENOTCAPABLE);
	if (!hasname2addr && nvlist_exists(oldlimits, LIMIT_NV_NAME2ADDR))
		return (ENOTCAPABLE);
	return (0);
}

static int
net_command(const char *cmd, const nvlist_t *limits, nvlist_t *nvlin,
    nvlist_t *nvlout)
{

	if (strcmp(cmd, "bind") == 0)
		return (net_bind(limits, nvlin, nvlout));
	else if (strcmp(cmd, "connect") == 0)
		return (net_connect(limits, nvlin, nvlout));
	else if (strcmp(cmd, "gethostbyname") == 0)
		return (net_gethostbyname(limits, nvlin, nvlout));
	else if (strcmp(cmd, "gethostbyaddr") == 0)
		return (net_gethostbyaddr(limits, nvlin, nvlout));
	else if (strcmp(cmd, "getnameinfo") == 0)
		return (net_getnameinfo(limits, nvlin, nvlout));
	else if (strcmp(cmd, "getaddrinfo") == 0)
		return (net_getaddrinfo(limits, nvlin, nvlout));

	return (EINVAL);
}

CREATE_SERVICE("system.net", net_limit, net_command, 0);
