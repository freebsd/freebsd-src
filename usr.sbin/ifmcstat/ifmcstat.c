/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <kvm.h>
#include <nlist.h>
#include <string.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#define _KERNEL
#include <netinet/if_ether.h>
#undef _KERNEL
#include <netinet/in_var.h>
#include <arpa/inet.h>

kvm_t	*kvmd;

struct	nlist nl[] = {
#define	N_IFNET	0
	{ "_ifnet" },
	{ "" },
};

const char *inet6_n2a __P((struct in6_addr *));
int main __P((void));
char *ifname __P((struct ifnet *));
void kread __P((u_long, void *, int));
void if6_addrlist __P((struct ifaddr *));
void in6_multilist __P((struct in6_multi *));
void in6_multientry __P((struct in6_multi *));

#define	KREAD(addr, buf, type) \
	kread((u_long)addr, (void *)buf, sizeof(type))

const char *inet6_n2a(p)
	struct in6_addr *p;
{
	static char buf[BUFSIZ];

	if (IN6_IS_ADDR_UNSPECIFIED(p))
		return "*";
	return inet_ntop(AF_INET6, (void *)p, buf, sizeof(buf));
}

int main()
{
	char	buf[_POSIX2_LINE_MAX], ifname[IFNAMSIZ];
	struct	ifnet	*ifp, *nifp, ifnet;
	struct	arpcom	arpcom;

	if ((kvmd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, buf)) == NULL) {
		perror("kvm_openfiles");
		exit(1);
	}
	if (kvm_nlist(kvmd, nl) < 0) {
		perror("kvm_nlist");
		exit(1);
	}
	if (nl[N_IFNET].n_value == 0) {
		printf("symbol %s not found\n", nl[N_IFNET].n_name);
		exit(1);
	}
	KREAD(nl[N_IFNET].n_value, &ifp, struct ifnet *);
	while (ifp) {
		KREAD(ifp, &ifnet, struct ifnet);
		printf("%s:\n", if_indextoname(ifnet.if_index, ifname));
		if6_addrlist(TAILQ_FIRST(&ifnet.if_addrhead));
		ifp = TAILQ_NEXT(&ifnet, if_link);
	}

	exit(0);
	/*NOTREACHED*/
}

char *ifname(ifp)
	struct ifnet *ifp;
{
	static char buf[BUFSIZ];

	KREAD(ifp->if_name, buf, IFNAMSIZ);
	return buf;
}

void kread(addr, buf, len)
	u_long addr;
	void *buf;
	int len;
{
	if (kvm_read(kvmd, addr, buf, len) != len) {
		perror("kvm_read");
		exit(1);
	}
}

void
if6_addrlist(ifap)
	struct ifaddr *ifap;
{
	static char in6buf[BUFSIZ];
	struct ifnet ifnet;
	struct ifaddr ifa;
	struct ifaddr *ifap0;
	struct ifmultiaddr ifm, *ifmp = 0;
	struct in6_ifaddr if6a;
	struct in6_multi *mc = 0, in6m;
	int in6_multilist_done = 0;
	struct sockaddr sa;
	struct sockaddr_in6 sin6;
	struct sockaddr_dl sdl;

	if (ifap == NULL)
		return;
	ifap0 = ifap;

	do {
		KREAD(ifap, &ifa, struct ifaddr);
		if (ifa.ifa_addr == NULL)
			continue;
		KREAD(ifa.ifa_addr, &sa, struct sockaddr);
		if (sa.sa_family != PF_INET6)
			continue;
		KREAD(ifap, &if6a, struct in6_ifaddr);
		printf("\tinet6 %s\n",
		       inet_ntop(AF_INET6,
				 (const void *)&if6a.ia_addr.sin6_addr,
				 in6buf, sizeof(in6buf)));
	} while ((ifap = TAILQ_NEXT(&ifa, ifa_link)) != NULL);

	KREAD(ifap0, &ifa, struct ifaddr);
	KREAD(ifa.ifa_ifp, &ifnet, struct ifnet);
	if (ifnet.if_multiaddrs.lh_first)
		ifmp = ifnet.if_multiaddrs.lh_first;
	if (ifmp == NULL)
		return;
	do {
		KREAD(ifmp, &ifm, struct ifmultiaddr);
		if (ifm.ifma_addr == NULL)
			continue;
		KREAD(ifm.ifma_addr, &sa, struct sockaddr);
		if (sa.sa_family != AF_INET6)
			continue;
		in6_multientry((struct in6_multi *)ifm.ifma_protospec);
		if (ifm.ifma_lladdr == 0)
			continue;
		KREAD(ifm.ifma_lladdr, &sdl, struct sockaddr_dl);
		printf("\t\t\tmcast-macaddr %s multicnt %d\n",
		       ether_ntoa((struct ether_addr *)LLADDR(&sdl)),
		       ifm.ifma_refcount);
	} while ((ifmp = LIST_NEXT(&ifm, ifma_link)) != NULL);
}

void
in6_multientry(mc)
	struct in6_multi *mc;
{
	static char mcbuf[BUFSIZ];
	struct in6_multi multi;

	KREAD(mc, &multi, struct in6_multi);
	printf("\t\tgroup %s\n", inet_ntop(AF_INET6,
					   (const void *)&multi.in6m_addr,
					   mcbuf, sizeof(mcbuf)));
}
