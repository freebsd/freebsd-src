/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 InnoGames GmbH
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/errno.h>

#include <netinet/in.h>

#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>

#include <netpfil/pf/pfsync_nv.h>

int
pfsync_syncpeer_nvlist_to_sockaddr(const nvlist_t *nvl,
    struct sockaddr_storage *sa)
{
	int af;
	int error;

	if (!nvlist_exists_number(nvl, "af"))
		return (EINVAL);
	if (!nvlist_exists_binary(nvl, "address"))
		return (EINVAL);

	af = nvlist_get_number(nvl, "af");

	switch (af) {
#ifdef INET
	case AF_INET: {
		struct sockaddr_in *in = (struct sockaddr_in *)sa;
		size_t len;
		const void *addr = nvlist_get_binary(nvl, "address", &len);
		in->sin_family = af;
		if (len != sizeof(*in))
			return (EINVAL);

		memcpy(in, addr, sizeof(*in));
		break;
	}
#endif
#ifdef INET6
	case AF_INET6: {
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)sa;
		size_t len;
		const void *addr = nvlist_get_binary(nvl, "address", &len);
		in6->sin6_family = af;
		if (len != sizeof(*in6))
			return (EINVAL);

		memcpy(in6, addr, sizeof(*in6));

		error = sa6_embedscope(in6, V_ip6_use_defzone);
		if (error)
			return (error);

		break;
	}
#endif
	default:
		return (EINVAL);
	}

	return (0);
}

nvlist_t *
pfsync_sockaddr_to_syncpeer_nvlist(struct sockaddr_storage *sa)
{
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	if (nvl == NULL) {
		return (nvl);
	}

	switch (sa->ss_family) {
#ifdef INET
	case AF_INET: {
		struct sockaddr_in *in = (struct sockaddr_in *)sa;
		nvlist_add_number(nvl, "af", in->sin_family);
		nvlist_add_binary(nvl, "address", in, sizeof(*in));
		break;
	}
#endif
#ifdef INET6
	case AF_INET6: {
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)sa;
		sa6_recoverscope(in6);
		nvlist_add_number(nvl, "af", in6->sin6_family);
		nvlist_add_binary(nvl, "address", in6, sizeof(*in6));
		break;
	}
#endif
	default:
		return NULL;
	}

	return (nvl);
}

int
pfsync_nvstatus_to_kstatus(const nvlist_t *nvl, struct pfsync_kstatus *status)
{
	struct sockaddr_storage addr;
	int error;

	if (!nvlist_exists_number(nvl, "maxupdates"))
		return (EINVAL);
	if (!nvlist_exists_number(nvl, "flags"))
		return (EINVAL);

	status->maxupdates = nvlist_get_number(nvl, "maxupdates");
	status->version = nvlist_get_number(nvl, "version");
	status->flags = nvlist_get_number(nvl, "flags");

	if (nvlist_exists_string(nvl, "syncdev"))
		strlcpy(status->syncdev, nvlist_get_string(nvl, "syncdev"),
		    IFNAMSIZ);

	if (nvlist_exists_nvlist(nvl, "syncpeer")) {
		memset(&addr, 0, sizeof(addr));
		if ((error = pfsync_syncpeer_nvlist_to_sockaddr(nvlist_get_nvlist(nvl, "syncpeer"), &addr)) != 0)
			return (error);

		status->syncpeer = addr;
	} else {
		memset(&status->syncpeer, 0, sizeof(status->syncpeer));
	}

	return (0);
}
