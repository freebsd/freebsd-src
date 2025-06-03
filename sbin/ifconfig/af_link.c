/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ifaddrs.h>
#include <unistd.h>

#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/ethernet.h>

#include "ifconfig.h"
#include "ifconfig_netlink.h"

static struct ifreq link_ridreq;

extern char *f_ether;

static void
print_ether(const struct ether_addr *addr, const char *prefix)
{
	char *ether_format = ether_ntoa(addr);

	if (f_ether != NULL) {
		if (strcmp(f_ether, "dash") == 0) {
			char *format_char;

			while ((format_char = strchr(ether_format, ':')) != NULL) {
				*format_char = '-';
			}
		} else if (strcmp(f_ether, "dotted") == 0) {
			/* Indices 0 and 1 is kept as is. */
			ether_format[ 2] = ether_format[ 3];
			ether_format[ 3] = ether_format[ 4];
			ether_format[ 4] = '.';
			ether_format[ 5] = ether_format[ 6];
			ether_format[ 6] = ether_format[ 7];
			ether_format[ 7] = ether_format[ 9];
			ether_format[ 8] = ether_format[10];
			ether_format[ 9] = '.';
			ether_format[10] = ether_format[12];
			ether_format[11] = ether_format[13];
			ether_format[12] = ether_format[15];
			ether_format[13] = ether_format[16];
			ether_format[14] = '\0';
		}
	}
	printf("\t%s %s\n", prefix, ether_format);
}

static void
print_lladdr(struct sockaddr_dl *sdl)
{
	if (match_ether(sdl)) {
		print_ether((struct ether_addr *)LLADDR(sdl), "ether");
	} else {
		int n = sdl->sdl_nlen > 0 ? sdl->sdl_nlen + 1 : 0;
		printf("\tlladdr %s\n", link_ntoa(sdl) + n);
	}
}

static void
print_pcp(if_ctx *ctx)
{
	struct ifreq ifr = {};

	if (ioctl_ctx_ifr(ctx, SIOCGLANPCP, &ifr) == 0 &&
	    ifr.ifr_lan_pcp != IFNET_PCP_NONE)
		printf("\tpcp %d\n", ifr.ifr_lan_pcp);
}

#ifdef WITHOUT_NETLINK
static void
link_status(if_ctx *ctx, const struct ifaddrs *ifa)
{
	/* XXX no const 'cuz LLADDR is defined wrong */
	struct sockaddr_dl *sdl;
	struct ifreq ifr;
	int rc, sock_hw;
	static const u_char laggaddr[6] = {0};

	sdl = satosdl(ifa->ifa_addr);
	if (sdl == NULL || sdl->sdl_alen == 0)
		return;

	print_lladdr(sdl);

	/*
	 * Best-effort (i.e. failures are silent) to get original
	 * hardware address, as read by NIC driver at attach time. Only
	 * applies to Ethernet NICs (IFT_ETHER). However, laggX
	 * interfaces claim to be IFT_ETHER, and re-type their component
	 * Ethernet NICs as IFT_IEEE8023ADLAG. So, check for both. If
	 * the MAC is zeroed, then it's actually a lagg.
	 */
	if ((sdl->sdl_type != IFT_ETHER &&
	    sdl->sdl_type != IFT_IEEE8023ADLAG) ||
	    sdl->sdl_alen != ETHER_ADDR_LEN)
		return;

	strncpy(ifr.ifr_name, ifa->ifa_name, sizeof(ifr.ifr_name));
	memcpy(&ifr.ifr_addr, ifa->ifa_addr, sizeof(ifa->ifa_addr->sa_len));
	ifr.ifr_addr.sa_family = AF_LOCAL;
	if ((sock_hw = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
		warn("socket(AF_LOCAL,SOCK_DGRAM)");
		return;
	}
	rc = ioctl(sock_hw, SIOCGHWADDR, &ifr);
	close(sock_hw);
	if (rc != 0)
		return;

	/*
	 * If this is definitely a lagg device or the hwaddr
	 * matches the link addr, don't bother.
	 */
	if (memcmp(ifr.ifr_addr.sa_data, laggaddr, sdl->sdl_alen) == 0 ||
	    memcmp(ifr.ifr_addr.sa_data, LLADDR(sdl), sdl->sdl_alen) == 0)
		goto pcp;

	print_ether((const struct ether_addr *)&ifr.ifr_addr.sa_data, "hwaddr");
pcp:
	print_pcp(ctx);
}

#else

static void
link_status_nl(if_ctx *ctx, if_link_t *link, if_addr_t *ifa __unused)
{
	if (link->ifla_address != NULL) {
		struct sockaddr_dl sdl = {
			.sdl_len = sizeof(struct sockaddr_dl),
			.sdl_family = AF_LINK,
			.sdl_type = convert_iftype(link->ifi_type),
			.sdl_alen = NLA_DATA_LEN(link->ifla_address),
		};
		memcpy(LLADDR(&sdl), NLA_DATA(link->ifla_address), sdl.sdl_alen);
		print_lladdr(&sdl);

		if (link->iflaf_orig_hwaddr != NULL) {
			struct nlattr *hwaddr = link->iflaf_orig_hwaddr;

			if (memcmp(NLA_DATA(hwaddr), NLA_DATA(link->ifla_address), sdl.sdl_alen))
				print_ether((struct ether_addr *)NLA_DATA(hwaddr), "hwaddr");
		}
	}
	if (convert_iftype(link->ifi_type) == IFT_ETHER)
		print_pcp(ctx);
}
#endif

static void
link_getaddr(const char *addr, int which)
{
	char *temp;
	struct sockaddr_dl sdl;
	struct sockaddr *sa = &link_ridreq.ifr_addr;

	if (which != ADDR)
		errx(1, "can't set link-level netmask or broadcast");
	if (!strcmp(addr, "random")) {
		sdl.sdl_len = sizeof(sdl);
		sdl.sdl_alen = ETHER_ADDR_LEN;
		sdl.sdl_nlen = 0;
		sdl.sdl_family = AF_LINK;
		arc4random_buf(&sdl.sdl_data, ETHER_ADDR_LEN);
		/* Non-multicast and claim it is locally administered. */
		sdl.sdl_data[0] &= 0xfc;
		sdl.sdl_data[0] |= 0x02;
	} else {
		if ((temp = malloc(strlen(addr) + 2)) == NULL)
			errx(1, "malloc failed");
		temp[0] = ':';
		strcpy(temp + 1, addr);
		sdl.sdl_len = sizeof(sdl);
		if (link_addr(temp, &sdl) == -1)
			errx(1, "malformed link-level address");
		free(temp);
	}
	if (sdl.sdl_alen > sizeof(sa->sa_data))
		errx(1, "malformed link-level address");
	sa->sa_family = AF_LINK;
	sa->sa_len = sdl.sdl_alen;
	bcopy(LLADDR(&sdl), sa->sa_data, sdl.sdl_alen);
}

static struct afswtch af_link = {
	.af_name	= "link",
	.af_af		= AF_LINK,
#ifdef WITHOUT_NETLINK
	.af_status	= link_status,
#else
	.af_status	= link_status_nl,
#endif
	.af_getaddr	= link_getaddr,
	.af_aifaddr	= SIOCSIFLLADDR,
	.af_addreq	= &link_ridreq,
	.af_exec	= af_exec_ioctl,
};
static struct afswtch af_ether = {
	.af_name	= "ether",
	.af_af		= AF_LINK,
#ifdef WITHOUT_NETLINK
	.af_status	= link_status,
#else
	.af_status	= link_status_nl,
#endif
	.af_getaddr	= link_getaddr,
	.af_aifaddr	= SIOCSIFLLADDR,
	.af_addreq	= &link_ridreq,
	.af_exec	= af_exec_ioctl,
};
static struct afswtch af_lladdr = {
	.af_name	= "lladdr",
	.af_af		= AF_LINK,
#ifdef WITHOUT_NETLINK
	.af_status	= link_status,
#else
	.af_status	= link_status_nl,
#endif
	.af_getaddr	= link_getaddr,
	.af_aifaddr	= SIOCSIFLLADDR,
	.af_addreq	= &link_ridreq,
	.af_exec	= af_exec_ioctl,
};

static __constructor void
link_ctor(void)
{
	af_register(&af_link);
	af_register(&af_ether);
	af_register(&af_lladdr);
}
