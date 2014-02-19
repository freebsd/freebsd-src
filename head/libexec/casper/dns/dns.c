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

#include <netinet/in.h>

#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>

#include <libcapsicum.h>
#include <libcapsicum_dns.h>
#include <libcasper.h>
#include <nv.h>
#include <pjdlog.h>

static bool
dns_allowed_type(const nvlist_t *limits, const char *type)
{
	const char *name;
	bool notypes;
	void *cookie;

	if (limits == NULL)
		return (true);

	notypes = true;
	cookie = NULL;
	while ((name = nvlist_next(limits, NULL, &cookie)) != NULL) {
		if (strncmp(name, "type", sizeof("type") - 1) != 0)
			continue;
		notypes = false;
		if (strcmp(nvlist_get_string(limits, name), type) == 0)
			return (true);
	}

	/* If there are no types at all, allow any type. */
	if (notypes)
		return (true);

	return (false);
}

static bool
dns_allowed_family(const nvlist_t *limits, int family)
{
	const char *name;
	bool nofamilies;
	void *cookie;

	if (limits == NULL)
		return (true);

	nofamilies = true;
	cookie = NULL;
	while ((name = nvlist_next(limits, NULL, &cookie)) != NULL) {
		if (strncmp(name, "family", sizeof("family") - 1) != 0)
			continue;
		nofamilies = false;
		if (family == AF_UNSPEC)
			continue;
		if (nvlist_get_number(limits, name) == (uint64_t)family)
			return (true);
	}

	/* If there are no families at all, allow any family. */
	if (nofamilies)
		return (true);

	return (false);
}

static void
hostent_pack(const struct hostent *hp, nvlist_t *nvl)
{
	unsigned int ii;

	nvlist_add_string(nvl, "name", hp->h_name);
	nvlist_add_number(nvl, "addrtype", (uint64_t)hp->h_addrtype);
	nvlist_add_number(nvl, "length", (uint64_t)hp->h_length);

	if (hp->h_aliases == NULL) {
		nvlist_add_number(nvl, "naliases", 0);
	} else {
		for (ii = 0; hp->h_aliases[ii] != NULL; ii++) {
			nvlist_addf_string(nvl, hp->h_aliases[ii], "alias%u",
			    ii);
		}
		nvlist_add_number(nvl, "naliases", (uint64_t)ii);
	}

	if (hp->h_addr_list == NULL) {
		nvlist_add_number(nvl, "naddrs", 0);
	} else {
		for (ii = 0; hp->h_addr_list[ii] != NULL; ii++) {
			nvlist_addf_binary(nvl, hp->h_addr_list[ii],
			    (size_t)hp->h_length, "addr%u", ii);
		}
		nvlist_add_number(nvl, "naddrs", (uint64_t)ii);
	}
}

static int
dns_gethostbyname(const nvlist_t *limits, const nvlist_t *nvlin,
    nvlist_t *nvlout)
{
	struct hostent *hp;
	int family;

	if (!dns_allowed_type(limits, "NAME"))
		return (NO_RECOVERY);

	family = (int)nvlist_get_number(nvlin, "family");

	if (!dns_allowed_family(limits, family))
		return (NO_RECOVERY);

	hp = gethostbyname2(nvlist_get_string(nvlin, "name"), family);
	if (hp == NULL)
		return (h_errno);
	hostent_pack(hp, nvlout);
	return (0);
}

static int
dns_gethostbyaddr(const nvlist_t *limits, const nvlist_t *nvlin,
    nvlist_t *nvlout)
{
	struct hostent *hp;
	const void *addr;
	size_t addrsize;
	int family;

	if (!dns_allowed_type(limits, "ADDR"))
		return (NO_RECOVERY);

	family = (int)nvlist_get_number(nvlin, "family");

	if (!dns_allowed_family(limits, family))
		return (NO_RECOVERY);

	addr = nvlist_get_binary(nvlin, "addr", &addrsize);
	hp = gethostbyaddr(addr, (socklen_t)addrsize, family);
	if (hp == NULL)
		return (h_errno);
	hostent_pack(hp, nvlout);
	return (0);
}

static int
dns_getnameinfo(const nvlist_t *limits, const nvlist_t *nvlin, nvlist_t *nvlout)
{
	struct sockaddr_storage sast;
	const void *sabin;
	char *host, *serv;
	size_t sabinsize, hostlen, servlen;
	socklen_t salen;
	int error, flags;

	if (!dns_allowed_type(limits, "NAME"))
		return (NO_RECOVERY);

	error = 0;
	host = serv = NULL;
	memset(&sast, 0, sizeof(sast));

	hostlen = (size_t)nvlist_get_number(nvlin, "hostlen");
	servlen = (size_t)nvlist_get_number(nvlin, "servlen");

	if (hostlen > 0) {
		host = calloc(1, hostlen + 1);
		if (host == NULL) {
			error = EAI_MEMORY;
			goto out;
		}
	}
	if (servlen > 0) {
		serv = calloc(1, servlen + 1);
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

	memcpy(&sast, sabin, sabinsize);
	salen = (socklen_t)sabinsize;

	if ((sast.ss_family != AF_INET ||
	     salen != sizeof(struct sockaddr_in)) &&
	    (sast.ss_family != AF_INET6 ||
	     salen != sizeof(struct sockaddr_in6))) {
		error = EAI_FAIL;
		goto out;
	}

	if (!dns_allowed_family(limits, (int)sast.ss_family))
		return (NO_RECOVERY);

	flags = (int)nvlist_get_number(nvlin, "flags");

	error = getnameinfo((struct sockaddr *)&sast, salen, host, hostlen,
	    serv, servlen, flags);
	if (error != 0)
		goto out;

	nvlist_move_string(nvlout, "host", host);
	nvlist_move_string(nvlout, "serv", serv);
out:
	if (error != 0) {
		free(host);
		free(serv);
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
	nvlist_add_string(nvl, "ai_canonname", ai->ai_canonname);

	return (nvl);
}

static int
dns_getaddrinfo(const nvlist_t *limits, const nvlist_t *nvlin, nvlist_t *nvlout)
{
	struct addrinfo hints, *hintsp, *res, *cur;
	const char *hostname, *servname;
	nvlist_t *elem;
	unsigned int ii;
	int error, family;

	if (!dns_allowed_type(limits, "ADDR"))
		return (NO_RECOVERY);

	hostname = nvlist_get_string(nvlin, "hostname");
	servname = nvlist_get_string(nvlin, "servname");
	if (nvlist_exists_number(nvlin, "hints.ai_flags")) {
		size_t addrlen;

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
		hintsp = &hints;
		family = hints.ai_family;
	} else {
		hintsp = NULL;
		family = AF_UNSPEC;
	}

	if (!dns_allowed_family(limits, family))
		return (NO_RECOVERY);

	error = getaddrinfo(hostname, servname, hintsp, &res);
	if (error != 0)
		goto out;

	for (cur = res, ii = 0; cur != NULL; cur = cur->ai_next, ii++) {
		elem = addrinfo_pack(cur);
		nvlist_movef_nvlist(nvlout, elem, "res%u", ii);
	}

	freeaddrinfo(res);
	error = 0;
out:
	return (error);
}

static bool
limit_has_entry(const nvlist_t *limits, const char *prefix)
{
	const char *name;
	size_t prefixlen;
	void *cookie;

	if (limits == NULL)
		return (false);

	prefixlen = strlen(prefix);

	cookie = NULL;
	while ((name = nvlist_next(limits, NULL, &cookie)) != NULL) {
		if (strncmp(name, prefix, prefixlen) == 0)
			return (true);
	}

	return (false);
}

static int
dns_limit(const nvlist_t *oldlimits, const nvlist_t *newlimits)
{
	const char *name;
	void *cookie;
	int nvtype;
	bool hastype, hasfamily;

	hastype = false;
	hasfamily = false;

	cookie = NULL;
	while ((name = nvlist_next(newlimits, &nvtype, &cookie)) != NULL) {
		if (nvtype == NV_TYPE_STRING) {
			const char *type;

			if (strncmp(name, "type", sizeof("type") - 1) != 0)
				return (EINVAL);
			type = nvlist_get_string(newlimits, name);
			if (strcmp(type, "ADDR") != 0 &&
			    strcmp(type, "NAME") != 0) {
				return (EINVAL);
			}
			if (!dns_allowed_type(oldlimits, type))
				return (ENOTCAPABLE);
			hastype = true;
		} else if (nvtype == NV_TYPE_NUMBER) {
			int family;

			if (strncmp(name, "family", sizeof("family") - 1) != 0)
				return (EINVAL);
			family = (int)nvlist_get_number(newlimits, name);
			if (!dns_allowed_family(oldlimits, family))
				return (ENOTCAPABLE);
			hasfamily = true;
		} else {
			return (EINVAL);
		}
	}

	/*
	 * If the new limit doesn't mention type or family we have to
	 * check if the current limit does have those. Missing type or
	 * family in the limit means that all types or families are
	 * allowed.
	 */
	if (!hastype) {
		if (limit_has_entry(oldlimits, "type"))
			return (ENOTCAPABLE);
	}
	if (!hasfamily) {
		if (limit_has_entry(oldlimits, "family"))
			return (ENOTCAPABLE);
	}

	return (0);
}

static int
dns_command(const char *cmd, const nvlist_t *limits, nvlist_t *nvlin,
    nvlist_t *nvlout)
{
	int error;

	if (strcmp(cmd, "gethostbyname") == 0)
		error = dns_gethostbyname(limits, nvlin, nvlout);
	else if (strcmp(cmd, "gethostbyaddr") == 0)
		error = dns_gethostbyaddr(limits, nvlin, nvlout);
	else if (strcmp(cmd, "getnameinfo") == 0)
		error = dns_getnameinfo(limits, nvlin, nvlout);
	else if (strcmp(cmd, "getaddrinfo") == 0)
		error = dns_getaddrinfo(limits, nvlin, nvlout);
	else
		error = NO_RECOVERY;

	return (error);
}

int
main(int argc, char *argv[])
{

	return (service_start("system.dns", PARENT_FILENO, dns_limit,
	    dns_command, argc, argv));
}
