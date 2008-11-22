/*
 * Copyright (c) 2007 Bruce M. Simpson <bms@FreeBSD.org>
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
 * Print the running system's current multicast group memberships.
 * As this relies on getifmaddrs(), it may not be used with a core file.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

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
#include <stdint.h>
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

void ifmalist_dump_af(const struct ifmaddrs * const ifmap, int const af);

void
ifmalist_dump_af(const struct ifmaddrs * const ifmap, int const af)
{
	const struct ifmaddrs *ifma;
	sockunion_t *psa;
	char myifname[IFNAMSIZ];
#ifdef INET6
	char addrbuf[INET6_ADDRSTRLEN];
#endif
	char *pcolon;
	char *pafname, *pifname, *plladdr, *pgroup;
#ifdef INET6
	void *in6addr;
#endif

	switch (af) {
	case AF_INET:
		pafname = "IPv4";
		break;
#ifdef INET6
	case AF_INET6:
		pafname = "IPv6";
		break;
#endif
	case AF_LINK:
		pafname = "Link-layer";
		break;
	default:
		return;		/* XXX */
	}

	fprintf(stdout, "%s Multicast Group Memberships\n", pafname);
	fprintf(stdout, "%-20s\t%-16s\t%s\n", "Group", "Link-layer Address",
	    "Netif");

	for (ifma = ifmap; ifma; ifma = ifma->ifma_next) {

		if (ifma->ifma_name == NULL || ifma->ifma_addr == NULL)
			continue;

		/* Group address */
		psa = (sockunion_t *)ifma->ifma_addr;
		if (psa->sa.sa_family != af)
			continue;

		switch (psa->sa.sa_family) {
		case AF_INET:
			pgroup = inet_ntoa(psa->sin.sin_addr);
			break;
#ifdef INET6
		case AF_INET6:
			in6addr = &psa->sin6.sin6_addr;
			inet_ntop(psa->sa.sa_family, in6addr, addrbuf,
			    sizeof(addrbuf));
			pgroup = addrbuf;
			break;
#endif
		case AF_LINK:
			if ((psa->sdl.sdl_alen == ETHER_ADDR_LEN) ||
			    (psa->sdl.sdl_type == IFT_ETHER)) {
				pgroup =
ether_ntoa((struct ether_addr *)&psa->sdl.sdl_data);
#ifdef notyet
			} else {
				pgroup = addr2ascii(AF_LINK,
				    &psa->sdl,
				    sizeof(struct sockaddr_dl),
				    addrbuf);
#endif
			}
			break;
		default:
			continue;	/* XXX */
		}

		/* Link-layer mapping, if any */
		psa = (sockunion_t *)ifma->ifma_lladdr;
		if (psa != NULL) {
			if (psa->sa.sa_family == AF_LINK) {
				if ((psa->sdl.sdl_alen == ETHER_ADDR_LEN) ||
				    (psa->sdl.sdl_type == IFT_ETHER)) {
					/* IEEE 802 */
					plladdr =
ether_ntoa((struct ether_addr *)&psa->sdl.sdl_data);
#ifdef notyet
				} else {
					/* something more exotic */
					plladdr = addr2ascii(AF_LINK,
					    &psa->sdl,
					    sizeof(struct sockaddr_dl),
					    addrbuf);
#endif
				}
			} else {
				/* not a link-layer address */
				plladdr = "<invalid>";
			}
		} else {
			plladdr = "<none>";
		}

		/* Interface upon which the membership exists */
		psa = (sockunion_t *)ifma->ifma_name;
		if (psa != NULL && psa->sa.sa_family == AF_LINK) {
			strlcpy(myifname, link_ntoa(&psa->sdl), IFNAMSIZ);
			pcolon = strchr(myifname, ':');
			if (pcolon)
				*pcolon = '\0';
			pifname = myifname;
		} else {
			pifname = "";
		}

		fprintf(stdout, "%-20s\t%-16s\t%s\n", pgroup, plladdr, pifname);
	}
}

void
ifmalist_dump(void)
{
	struct ifmaddrs *ifmap;

	if (getifmaddrs(&ifmap))
		err(EX_OSERR, "getifmaddrs");

	ifmalist_dump_af(ifmap, AF_LINK);
	fputs("\n", stdout);
	ifmalist_dump_af(ifmap, AF_INET);
#ifdef INET6
	fputs("\n", stdout);
	ifmalist_dump_af(ifmap, AF_INET6);
#endif

	freeifmaddrs(ifmap);
}
