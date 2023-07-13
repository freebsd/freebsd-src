/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003 Ryan McBride. All rights reserved.
 * Copyright (c) 2004 Max Laier. All rights reserved.
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/nv.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>
#include <net/pfvar.h>
#include <net/if_pfsync.h>
#include <net/route.h>
#include <arpa/inet.h>

#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ifconfig.h"

static int
pfsync_do_ioctl(if_ctx *ctx, uint cmd, nvlist_t **nvl)
{
	void *data;
	size_t nvlen;
	struct ifreq ifr = {};

	data = nvlist_pack(*nvl, &nvlen);

	ifr.ifr_cap_nv.buffer = malloc(IFR_CAP_NV_MAXBUFSIZE);
	memcpy(ifr.ifr_cap_nv.buffer, data, nvlen);
	ifr.ifr_cap_nv.buf_length = IFR_CAP_NV_MAXBUFSIZE;
	ifr.ifr_cap_nv.length = nvlen;
	free(data);

	if (ioctl_ctx_ifr(ctx, cmd, &ifr) == -1) {
		free(ifr.ifr_cap_nv.buffer);
		return -1;
	}

	nvlist_destroy(*nvl);
	*nvl = NULL;

	*nvl = nvlist_unpack(ifr.ifr_cap_nv.buffer, ifr.ifr_cap_nv.length, 0);
	if (*nvl == NULL) {
		free(ifr.ifr_cap_nv.buffer);
		return (EIO);
	}

	free(ifr.ifr_cap_nv.buffer);
	return (errno);
}

static nvlist_t *
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
		nvlist_add_number(nvl, "af", in6->sin6_family);
		nvlist_add_binary(nvl, "address", in6, sizeof(*in6));
		break;
	}
#endif
	default:
		nvlist_add_number(nvl, "af", AF_UNSPEC);
		nvlist_add_binary(nvl, "address", sa, sizeof(*sa));
		break;
	}

	return (nvl);
}

static int
pfsync_syncpeer_nvlist_to_sockaddr(const nvlist_t *nvl,
    struct sockaddr_storage *sa)
{
	int af;

#if (!defined INET && !defined INET6)
	(void)sa;
#endif

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
		if (len != sizeof(*in6))
			return (EINVAL);

		memcpy(in6, addr, sizeof(*in6));
		break;
	}
#endif
	default:
		return (EINVAL);
	}

	return (0);
}

static void
setpfsync_syncdev(if_ctx *ctx, const char *val, int dummy __unused)
{
	nvlist_t *nvl = nvlist_create(0);

	if (strlen(val) > IFNAMSIZ)
		errx(1, "interface name %s is too long", val);

	if (pfsync_do_ioctl(ctx, SIOCGETPFSYNCNV, &nvl) == -1)
		err(1, "SIOCGETPFSYNCNV");

	if (nvlist_exists_string(nvl, "syncdev"))
		nvlist_free_string(nvl, "syncdev");

	nvlist_add_string(nvl, "syncdev", val);

	if (pfsync_do_ioctl(ctx, SIOCSETPFSYNCNV, &nvl) == -1)
		err(1, "SIOCSETPFSYNCNV");
}

static void
unsetpfsync_syncdev(if_ctx *ctx, const char *val __unused, int dummy __unused)
{
	nvlist_t *nvl = nvlist_create(0);

	if (pfsync_do_ioctl(ctx, SIOCGETPFSYNCNV, &nvl) == -1)
		err(1, "SIOCGETPFSYNCNV");

	if (nvlist_exists_string(nvl, "syncdev"))
		nvlist_free_string(nvl, "syncdev");

	nvlist_add_string(nvl, "syncdev", "");

	if (pfsync_do_ioctl(ctx, SIOCSETPFSYNCNV, &nvl) == -1)
		err(1, "SIOCSETPFSYNCNV");
}

static void
setpfsync_syncpeer(if_ctx *ctx, const char *val, int dummy __unused)
{
	struct addrinfo *peerres;
	struct sockaddr_storage addr;
	int ecode;

	nvlist_t *nvl = nvlist_create(0);

	if (pfsync_do_ioctl(ctx, SIOCGETPFSYNCNV, &nvl) == -1)
		err(1, "SIOCGETPFSYNCNV");

	if ((ecode = getaddrinfo(val, NULL, NULL, &peerres)) != 0)
		errx(1, "error in parsing address string: %s",
		    gai_strerror(ecode));

	switch (peerres->ai_family) {
#ifdef INET
	case AF_INET: {
		struct sockaddr_in *sin = satosin(peerres->ai_addr);

		memcpy(&addr, sin, sizeof(*sin));
		break;
	}
#endif
#ifdef INET6
	case AF_INET6: {
		struct sockaddr_in6 *sin6 = satosin6(peerres->ai_addr);

		memcpy(&addr, sin6, sizeof(*sin6));
		break;
	}
#endif
	default:
		errx(1, "syncpeer address %s not supported", val);
	}

	if (nvlist_exists_nvlist(nvl, "syncpeer"))
		nvlist_free_nvlist(nvl, "syncpeer");

	nvlist_add_nvlist(nvl, "syncpeer",
	    pfsync_sockaddr_to_syncpeer_nvlist(&addr));

	if (pfsync_do_ioctl(ctx, SIOCSETPFSYNCNV, &nvl) == -1)
		err(1, "SIOCSETPFSYNCNV");

	nvlist_destroy(nvl);
	freeaddrinfo(peerres);
}

static void
unsetpfsync_syncpeer(if_ctx *ctx, const char *val __unused, int dummy __unused)
{
	struct sockaddr_storage addr;
	memset(&addr, 0, sizeof(addr));

	nvlist_t *nvl = nvlist_create(0);

	if (pfsync_do_ioctl(ctx, SIOCGETPFSYNCNV, &nvl) == -1)
		err(1, "SIOCGETPFSYNCNV");

	if (nvlist_exists_nvlist(nvl, "syncpeer"))
		nvlist_free_nvlist(nvl, "syncpeer");

	nvlist_add_nvlist(nvl, "syncpeer",
	    pfsync_sockaddr_to_syncpeer_nvlist(&addr));

	if (pfsync_do_ioctl(ctx, SIOCSETPFSYNCNV, &nvl) == -1)
		err(1, "SIOCSETPFSYNCNV");

	nvlist_destroy(nvl);
}

static void
setpfsync_maxupd(if_ctx *ctx, const char *val, int dummy __unused)
{
	int maxupdates;
	nvlist_t *nvl = nvlist_create(0);

	maxupdates = atoi(val);
	if ((maxupdates < 0) || (maxupdates > 255))
		errx(1, "maxupd %s: out of range", val);

	if (pfsync_do_ioctl(ctx, SIOCGETPFSYNCNV, &nvl) == -1)
		err(1, "SIOCGETPFSYNCNV");

	nvlist_free_number(nvl, "maxupdates");
	nvlist_add_number(nvl, "maxupdates", maxupdates);

	if (pfsync_do_ioctl(ctx, SIOCSETPFSYNCNV, &nvl) == -1)
		err(1, "SIOCSETPFSYNCNV");

	nvlist_destroy(nvl);
}

static void
setpfsync_defer(if_ctx *ctx, const char *val __unused, int d)
{
	nvlist_t *nvl = nvlist_create(0);

	if (pfsync_do_ioctl(ctx, SIOCGETPFSYNCNV, &nvl) == -1)
		err(1, "SIOCGETPFSYNCNV");

	nvlist_free_number(nvl, "flags");
	nvlist_add_number(nvl, "flags", d ? PFSYNCF_DEFER : 0);

	if (pfsync_do_ioctl(ctx, SIOCSETPFSYNCNV, &nvl) == -1)
		err(1, "SIOCSETPFSYNCNV");

	nvlist_destroy(nvl);
}

static void
setpfsync_version(if_ctx *ctx, const char *val, int dummy __unused)
{
	int version;
	nvlist_t *nvl = nvlist_create(0);

	/* Don't verify, kernel knows which versions are supported.*/
	version = atoi(val);

	if (pfsync_do_ioctl(ctx, SIOCGETPFSYNCNV, &nvl) == -1)
		err(1, "SIOCGETPFSYNCNV");

	nvlist_free_number(nvl, "version");
	nvlist_add_number(nvl, "version", version);

	if (pfsync_do_ioctl(ctx, SIOCSETPFSYNCNV, &nvl) == -1)
		err(1, "SIOCSETPFSYNCNV");

	nvlist_destroy(nvl);
}

static void
pfsync_status(if_ctx *ctx)
{
	nvlist_t *nvl;
	char syncdev[IFNAMSIZ];
	char syncpeer_str[NI_MAXHOST];
	struct sockaddr_storage syncpeer;
	int maxupdates = 0;
	int flags = 0;
	int version;
	int error;

	nvl = nvlist_create(0);

	if (pfsync_do_ioctl(ctx, SIOCGETPFSYNCNV, &nvl) == -1) {
		nvlist_destroy(nvl);
		return;
	}

	memset((char *)&syncdev, 0, IFNAMSIZ);
	if (nvlist_exists_string(nvl, "syncdev"))
		strlcpy(syncdev, nvlist_get_string(nvl, "syncdev"),
		    IFNAMSIZ);
	if (nvlist_exists_number(nvl, "maxupdates"))
		maxupdates = nvlist_get_number(nvl, "maxupdates");
	if (nvlist_exists_number(nvl, "version"))
		version = nvlist_get_number(nvl, "version");
	if (nvlist_exists_number(nvl, "flags"))
		flags = nvlist_get_number(nvl, "flags");
	if (nvlist_exists_nvlist(nvl, "syncpeer")) {
		pfsync_syncpeer_nvlist_to_sockaddr(nvlist_get_nvlist(nvl,
							     "syncpeer"),
		    &syncpeer);
	}

	nvlist_destroy(nvl);

	if (syncdev[0] != '\0' || syncpeer.ss_family != AF_UNSPEC)
		printf("\t");

	if (syncdev[0] != '\0')
		printf("syncdev: %s ", syncdev);

	if ((syncpeer.ss_family == AF_INET &&
	    ((struct sockaddr_in *)&syncpeer)->sin_addr.s_addr !=
	    htonl(INADDR_PFSYNC_GROUP)) || syncpeer.ss_family == AF_INET6) {

		struct sockaddr *syncpeer_sa =
		    (struct sockaddr *)&syncpeer;
		if ((error = getnameinfo(syncpeer_sa, syncpeer_sa->sa_len,
			 syncpeer_str, sizeof(syncpeer_str), NULL, 0,
			 NI_NUMERICHOST)) != 0)
			errx(1, "getnameinfo: %s", gai_strerror(error));
		printf("syncpeer: %s ", syncpeer_str);
	}

	printf("maxupd: %d ", maxupdates);
	printf("defer: %s ", (flags & PFSYNCF_DEFER) ? "on" : "off");
	printf("version: %d\n", version);
	printf("\tsyncok: %d\n", (flags & PFSYNCF_OK) ? 1 : 0);
}

static struct cmd pfsync_cmds[] = {
	DEF_CMD_ARG("syncdev",		setpfsync_syncdev),
	DEF_CMD("-syncdev",	1,	unsetpfsync_syncdev),
	DEF_CMD_ARG("syncif",		setpfsync_syncdev),
	DEF_CMD("-syncif",	1,	unsetpfsync_syncdev),
	DEF_CMD_ARG("syncpeer",		setpfsync_syncpeer),
	DEF_CMD("-syncpeer",	1,	unsetpfsync_syncpeer),
	DEF_CMD_ARG("maxupd",		setpfsync_maxupd),
	DEF_CMD("defer",	1,	setpfsync_defer),
	DEF_CMD("-defer",	0,	setpfsync_defer),
	DEF_CMD_ARG("version",		setpfsync_version),
};
static struct afswtch af_pfsync = {
	.af_name	= "af_pfsync",
	.af_af		= AF_UNSPEC,
	.af_other_status = pfsync_status,
};

static __constructor void
pfsync_ctor(void)
{
	for (size_t i = 0; i < nitems(pfsync_cmds);  i++)
		cmd_register(&pfsync_cmds[i]);
	af_register(&af_pfsync);
}
