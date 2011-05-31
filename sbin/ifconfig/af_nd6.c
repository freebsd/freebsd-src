/*
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

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

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
#include <net/if_var.h>
#include <netinet/in_var.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <netinet6/nd6.h>

#include "ifconfig.h"

#define	MAX_SYSCTL_TRY	5
#define	ND6BITS	"\020\001PERFORMNUD\002ACCEPT_RTADV\003PREFER_SOURCE" \
		"\004IFDISABLED\005DONT_SET_IFROUTE\006AUTO_LINKLOCAL" \
		"\020DEFAULTIF"

static int isnd6defif(int);
void setnd6flags(const char *, int, int, const struct afswtch *);
void setnd6defif(const char *, int, int, const struct afswtch *);

void
setnd6flags(const char *dummyaddr __unused,
	int d, int s,
	const struct afswtch *afp)
{
	struct in6_ndireq nd;
	int error;

	memset(&nd, 0, sizeof(nd));
	strncpy(nd.ifname, ifr.ifr_name, sizeof(nd.ifname));
	error = ioctl(s, SIOCGIFINFO_IN6, &nd);
	if (error) {
		warn("ioctl(SIOCGIFINFO_IN6)");
		return;
	}
	if (d < 0)
		nd.ndi.flags &= ~(-d);
	else
		nd.ndi.flags |= d;
	error = ioctl(s, SIOCSIFINFO_IN6, (caddr_t)&nd);
	if (error)
		warn("ioctl(SIOCSIFINFO_IN6)");
}

void
setnd6defif(const char *dummyaddr __unused,
	int d, int s,
	const struct afswtch *afp)
{
	struct in6_ndifreq ndifreq;
	int ifindex;
	int error;

	memset(&ndifreq, 0, sizeof(ndifreq));
	strncpy(ndifreq.ifname, ifr.ifr_name, sizeof(ndifreq.ifname));

	if (d < 0) {
		if (isnd6defif(s)) {
			/* ifindex = 0 means to remove default if */
			ifindex = 0;
		} else
			return;
	} else if ((ifindex = if_nametoindex(ndifreq.ifname)) == 0) {
		warn("if_nametoindex(%s)", ndifreq.ifname);
		return;
	}

	ndifreq.ifindex = ifindex;
	error = ioctl(s, SIOCSDEFIFACE_IN6, (caddr_t)&ndifreq);
	if (error)
		warn("ioctl(SIOCSDEFIFACE_IN6)");
}

static int
isnd6defif(int s)
{
	struct in6_ndifreq ndifreq;
	unsigned int ifindex;
	int error;

	memset(&ndifreq, 0, sizeof(ndifreq));
	strncpy(ndifreq.ifname, ifr.ifr_name, sizeof(ndifreq.ifname));

	ifindex = if_nametoindex(ndifreq.ifname);
	error = ioctl(s, SIOCGDEFIFACE_IN6, (caddr_t)&ndifreq);
	if (error) {
		warn("ioctl(SIOCGDEFIFACE_IN6)");
		return (error);
	}
	return (ndifreq.ifindex == ifindex);
}

static void
nd6_status(int s)
{
	struct in6_ndireq nd;
	struct rt_msghdr *rtm;
	size_t needed;
	char *buf, *next;
	int mib[6], ntry;
	int s6;
	int error;
	int isinet6, isdefif;

	/* Check if the interface has at least one IPv6 address. */
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET6;
	mib[4] = NET_RT_IFLIST;
	mib[5] = if_nametoindex(ifr.ifr_name);

	/* Try to prevent a race between two sysctls. */
	ntry = 0;
	do {
		error = sysctl(mib, 6, NULL, &needed, NULL, 0);
		if (error) {
			warn("sysctl(NET_RT_IFLIST)/estimate");
			return;
		}
		buf = malloc(needed);
		if (buf == NULL) {
			warn("malloc for sysctl(NET_RT_IFLIST) failed");
			return;
		}
		if ((error = sysctl(mib, 6, buf, &needed, NULL, 0)) < 0) {
			if (errno != ENOMEM || ++ntry >= MAX_SYSCTL_TRY) {
				warn("sysctl(NET_RT_IFLIST)/get");
				free(buf);
				return;
			}
			free(buf);
			buf = NULL;
		}
	} while (buf == NULL);
	
	isinet6 = 0;
	for (next = buf; next < buf + needed; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;

		if (rtm->rtm_version != RTM_VERSION)
			continue;
		if (rtm->rtm_type == RTM_NEWADDR) {
			isinet6 = 1;
			break;
		}
	}
	free(buf);
	if (!isinet6)
		return;

	memset(&nd, 0, sizeof(nd));
	strncpy(nd.ifname, ifr.ifr_name, sizeof(nd.ifname));
	if ((s6 = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		warn("socket(AF_INET6, SOCK_DGRAM)");
		return;
	}
	error = ioctl(s6, SIOCGIFINFO_IN6, &nd);
	if (error) {
		warn("ioctl(SIOCGIFINFO_IN6)");
		close(s6);
		return;
	}
	isdefif = isnd6defif(s6);
	close(s6);
	if (nd.ndi.flags == 0 && !isdefif)
		return;
	printb("\tnd6 options",
	    (unsigned int)(nd.ndi.flags | (isdefif << 15)), ND6BITS);
	putchar('\n');
}

static struct afswtch af_nd6 = {
	.af_name	= "nd6",
	.af_af		= AF_LOCAL,
	.af_other_status= nd6_status,
};

static __constructor void
nd6_ctor(void)
{

	if (!feature_present("inet6"))
		return;

	af_register(&af_nd6);
}
