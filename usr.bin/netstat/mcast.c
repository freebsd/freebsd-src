/*
 * Copyright (c) 2004 Bruce M Simpson <bms@spc.org>
 * All rights reserved.
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

/*
 * Print the system's current multicast group memberships.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_mib.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <ctype.h>
#include <err.h>
#include <ifaddrs.h>
#include <sysexits.h>

#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "netstat.h"

union sockunion {
	struct sockaddr_storage	ss;
	struct sockaddr		sa;
	struct sockaddr_dl	sdl;
	struct sockaddr_in	sin;
	struct sockaddr_in6	sin6;
};
typedef union sockunion sockunion_t;

#define MYIFNAME_SIZE 128

void
ifmalist_dump(void)
{
	struct ifmaddrs *ifmap, *ifma;
	sockunion_t *psa;
	char myifname[MYIFNAME_SIZE];
	char addrbuf[INET6_ADDRSTRLEN];
	char *pcolon;
	void *addr;
	char *pifname, *plladdr, *pgroup;

	if (getifmaddrs(&ifmap))
		err(EX_OSERR, "getifmaddrs");

	fputs("IPv4/IPv6 Multicast Group Memberships\n", stdout);
	fprintf(stdout, "%-20s\t%-16s\t%s\n", "Group", "Gateway", "Netif");

	for (ifma = ifmap; ifma; ifma = ifma->ifma_next) {

		if (ifma->ifma_name == NULL || ifma->ifma_addr == NULL)
			continue;

		/* Group address */
		psa = (sockunion_t *)ifma->ifma_addr;
		switch (psa->sa.sa_family) {
		case AF_INET:
			pgroup = inet_ntoa(psa->sin.sin_addr);
			break;
		case AF_INET6:
			addr = &psa->sin6.sin6_addr;
			inet_ntop(psa->sa.sa_family, addr, addrbuf,
			    sizeof(addrbuf));
			pgroup = addrbuf;
			break;
		default:
			continue;	/* XXX */
		}

		/* Link-layer mapping, if any */
		psa = (sockunion_t *)ifma->ifma_lladdr;
		if (psa != NULL) {
			switch (psa->sa.sa_family) {
			case AF_INET:
				plladdr = inet_ntoa(psa->sin.sin_addr);
				break;
			case AF_LINK:
				if (psa->sdl.sdl_type == IFT_ETHER)
					plladdr = ether_ntoa((struct ether_addr *)&psa->sdl.sdl_data);
				else
					plladdr = link_ntoa(&psa->sdl);
				break;
			}
		} else
			plladdr = "<none>";

		/* Interface upon which the membership exists */
		psa = (sockunion_t *)ifma->ifma_name;
		switch (psa->sa.sa_family) {
		case AF_LINK:
			strlcpy(myifname, link_ntoa(&psa->sdl),
			    MYIFNAME_SIZE);
			pcolon = strchr(myifname, ':');
			if (pcolon)
				*pcolon = '\0';
			pifname = myifname;
			break;
		default:
			pifname = "";
			break;
		}

		fprintf(stdout, "%-20s\t%-16s\t%s\n", pgroup, plladdr, pifname);
	}

	freeifmaddrs(ifmap);
}
