/*-
 * Copyright (c) 2012-2013 The FreeBSD Foundation
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

#include <sys/nv.h>

#include <assert.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libcapsicum.h"
#include "libcapsicum_dns.h"

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
	hp->h_aliases = calloc(sizeof(hp->h_aliases[0]), nitems + 1);
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
	hp->h_addr_list = calloc(sizeof(hp->h_addr_list[0]), nitems + 1);
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

struct hostent *
cap_gethostbyname(cap_channel_t *chan, const char *name)
{

	return (cap_gethostbyname2(chan, name, AF_INET));
}

struct hostent *
cap_gethostbyname2(cap_channel_t *chan, const char *name, int type)
{
	struct hostent *hp;
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "cmd", "gethostbyname");
	nvlist_add_number(nvl, "family", (uint64_t)type);
	nvlist_add_string(nvl, "name", name);
	nvl = cap_xfer_nvlist(chan, nvl, 0);
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
    int type)
{
	struct hostent *hp;
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "cmd", "gethostbyaddr");
	nvlist_add_binary(nvl, "addr", addr, (size_t)len);
	nvlist_add_number(nvl, "family", (uint64_t)type);
	nvl = cap_xfer_nvlist(chan, nvl, 0);
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
	canonname = nvlist_get_string(nvl, "ai_canonname");
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
	int error, n;

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "cmd", "getaddrinfo");
	nvlist_add_string(nvl, "hostname", hostname);
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
	nvl = cap_xfer_nvlist(chan, nvl, 0);
	if (nvl == NULL)
		return (EAI_MEMORY);
	if (nvlist_get_number(nvl, "error") != 0) {
		error = (int)nvlist_get_number(nvl, "error");
		nvlist_destroy(nvl);
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
		if (curai == NULL)
			break;
		if (prevai != NULL)
			prevai->ai_next = curai;
		else if (firstai == NULL)
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
	int error;

	nvl = nvlist_create(0);
	nvlist_add_string(nvl, "cmd", "getnameinfo");
	nvlist_add_number(nvl, "hostlen", (uint64_t)hostlen);
	nvlist_add_number(nvl, "servlen", (uint64_t)servlen);
	nvlist_add_binary(nvl, "sa", sa, (size_t)salen);
	nvlist_add_number(nvl, "flags", (uint64_t)flags);
	nvl = cap_xfer_nvlist(chan, nvl, 0);
	if (nvl == NULL)
		return (EAI_MEMORY);
	if (nvlist_get_number(nvl, "error") != 0) {
		error = (int)nvlist_get_number(nvl, "error");
		nvlist_destroy(nvl);
		return (error);
	}

	if (host != NULL)
		strlcpy(host, nvlist_get_string(nvl, "host"), hostlen + 1);
	if (serv != NULL)
		strlcpy(serv, nvlist_get_string(nvl, "serv"), servlen + 1);
	nvlist_destroy(nvl);
	return (0);
}

static void
limit_remove(nvlist_t *limits, const char *prefix)
{
	const char *name;
	size_t prefixlen;
	void *cookie;

	prefixlen = strlen(prefix);
again:
	cookie = NULL;
	while ((name = nvlist_next(limits, NULL, &cookie)) != NULL) {
		if (strncmp(name, prefix, prefixlen) == 0) {
			nvlist_free(limits, name);
			goto again;
		}
	}
}

int
cap_dns_type_limit(cap_channel_t *chan, const char * const *types,
    size_t ntypes)
{
	nvlist_t *limits;
	unsigned int i;
	char nvlname[64];
	int n;

	if (cap_limit_get(chan, &limits) < 0)
		return (-1);
	if (limits == NULL)
		limits = nvlist_create(0);
	else
		limit_remove(limits, "type");
	for (i = 0; i < ntypes; i++) {
		n = snprintf(nvlname, sizeof(nvlname), "type%u", i);
		assert(n > 0 && n < (int)sizeof(nvlname));
		nvlist_add_string(limits, nvlname, types[i]);
	}
	return (cap_limit_set(chan, limits));
}

int
cap_dns_family_limit(cap_channel_t *chan, const int *families,
    size_t nfamilies)
{
	nvlist_t *limits;
	unsigned int i;
	char nvlname[64];
	int n;

	if (cap_limit_get(chan, &limits) < 0)
		return (-1);
	if (limits == NULL)
		limits = nvlist_create(0);
	else
		limit_remove(limits, "family");
	for (i = 0; i < nfamilies; i++) {
		n = snprintf(nvlname, sizeof(nvlname), "family%u", i);
		assert(n > 0 && n < (int)sizeof(nvlname));
		nvlist_add_number(limits, nvlname, (uint64_t)families[i]);
	}
	return (cap_limit_set(chan, limits));
}
