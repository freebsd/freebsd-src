/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2009 Hiroki Sato.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/route.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ifaddrs.h>

#include <arpa/inet.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <netinet6/nd6.h>

#include "ifconfig.h"

#define	MAX_SYSCTL_TRY	5
#ifdef DRAFT_IETF_6MAN_IPV6ONLY_FLAG
#define	ND6BITS	"\020\001PERFORMNUD\002ACCEPT_RTADV\003PREFER_SOURCE" \
		"\004IFDISABLED\005DONT_SET_IFROUTE\006AUTO_LINKLOCAL" \
		"\007NO_RADR\010NO_PREFER_IFACE\011NO_DAD" \
		"\012IPV6_ONLY\013IPV6_ONLY_MANUAL" \
		"\020DEFAULTIF"
#else
#define	ND6BITS	"\020\001PERFORMNUD\002ACCEPT_RTADV\003PREFER_SOURCE" \
		"\004IFDISABLED\005DONT_SET_IFROUTE\006AUTO_LINKLOCAL" \
		"\007NO_RADR\010NO_PREFER_IFACE\011NO_DAD\020DEFAULTIF"
#endif

static int isnd6defif(if_ctx *ctx, int s);
void setnd6flags(if_ctx *, const char *, int);
void setnd6defif(if_ctx *,const char *, int);
void nd6_status(if_ctx *);

void
setnd6flags(if_ctx *ctx, const char *dummyaddr __unused, int d)
{
	struct in6_ndireq nd = {};
	int error;

	strlcpy(nd.ifname, ctx->ifname, sizeof(nd.ifname));
	error = ioctl_ctx(ctx, SIOCGIFINFO_IN6, &nd);
	if (error) {
		warn("ioctl(SIOCGIFINFO_IN6)");
		return;
	}
	if (d < 0)
		nd.ndi.flags &= ~(-d);
	else
		nd.ndi.flags |= d;
	error = ioctl_ctx(ctx, SIOCSIFINFO_IN6, (caddr_t)&nd);
	if (error)
		warn("ioctl(SIOCSIFINFO_IN6)");
}

void
setnd6defif(if_ctx *ctx, const char *dummyaddr __unused, int d)
{
	struct in6_ndifreq ndifreq = {};
	int ifindex;
	int error;

	strlcpy(ndifreq.ifname, ctx->ifname, sizeof(ndifreq.ifname));

	if (d < 0) {
		if (isnd6defif(ctx, ctx->io_s)) {
			/* ifindex = 0 means to remove default if */
			ifindex = 0;
		} else
			return;
	} else if ((ifindex = if_nametoindex(ndifreq.ifname)) == 0) {
		warn("if_nametoindex(%s)", ndifreq.ifname);
		return;
	}

	ndifreq.ifindex = ifindex;
	error = ioctl_ctx(ctx, SIOCSDEFIFACE_IN6, (caddr_t)&ndifreq);
	if (error)
		warn("ioctl(SIOCSDEFIFACE_IN6)");
}

static int
isnd6defif(if_ctx *ctx, int s)
{
	struct in6_ndifreq ndifreq = {};
	unsigned int ifindex;
	int error;

	strlcpy(ndifreq.ifname, ctx->ifname, sizeof(ndifreq.ifname));

	ifindex = if_nametoindex(ndifreq.ifname);
	error = ioctl(s, SIOCGDEFIFACE_IN6, (caddr_t)&ndifreq);
	if (error) {
		warn("ioctl(SIOCGDEFIFACE_IN6)");
		return (error);
	}
	return (ndifreq.ifindex == ifindex);
}

void
nd6_status(if_ctx *ctx)
{
	struct in6_ndireq nd = {};
	int s6;
	int error;
	int isdefif;

	strlcpy(nd.ifname, ctx->ifname, sizeof(nd.ifname));
	if ((s6 = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		if (errno != EAFNOSUPPORT && errno != EPROTONOSUPPORT)
			warn("socket(AF_INET6, SOCK_DGRAM)");
		return;
	}
	error = ioctl(s6, SIOCGIFINFO_IN6, &nd);
	if (error) {
		if (errno != EPFNOSUPPORT)
			warn("ioctl(SIOCGIFINFO_IN6)");
		close(s6);
		return;
	}
	isdefif = isnd6defif(ctx, s6);
	close(s6);
	if (nd.ndi.flags == 0 && !isdefif)
		return;
	printb("\tnd6 options",
	    (unsigned int)(nd.ndi.flags | (isdefif << 15)), ND6BITS);
	putchar('\n');
}
