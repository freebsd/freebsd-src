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
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#define	_KERNEL
#include <netinet/if_ether.h>
#undef	_KERNEL
#include <netinet/in_var.h>
#include <arpa/inet.h>

#include <netdb.h>

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
struct in6_multi * in6_multientry __P((struct in6_multi *));

#define	KREAD(addr, buf, type) \
	kread((u_long)addr, (void *)buf, sizeof(type))

#ifdef N_IN6_MK
struct multi6_kludge {
	LIST_ENTRY(multi6_kludge) mk_entry;
	struct ifnet *mk_ifp;
	struct in6_multihead mk_head;
};
#endif

const char *inet6_n2a(p)
	struct in6_addr *p;
{
	static char buf[NI_MAXHOST];
	struct sockaddr_in6 sin6;
	u_int32_t scopeid;
#ifdef NI_WITHSCOPEID
	const int niflags = NI_NUMERICHOST | NI_WITHSCOPEID;
#else
	const int niflags = NI_NUMERICHOST;
#endif

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_addr = *p;
	if (IN6_IS_ADDR_LINKLOCAL(p) || IN6_IS_ADDR_MC_LINKLOCAL(p)) {
		scopeid = ntohs(*(u_int16_t *)&sin6.sin6_addr.s6_addr[2]);
		if (scopeid) {
			sin6.sin6_scope_id = scopeid;
			sin6.sin6_addr.s6_addr[2] = 0;
			sin6.sin6_addr.s6_addr[3] = 0;
		}
	}
	if (getnameinfo((struct sockaddr *)&sin6, sin6.sin6_len,
			buf, sizeof(buf), NULL, 0, niflags) == 0)
		return buf;
	else
		return "(invalid)";
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
		nifp = TAILQ_NEXT(&ifnet, if_link);

		/* not supported */

		ifp = nifp;
	}

	exit(0);
	/*NOTREACHED*/
}

char *ifname(ifp)
	struct ifnet *ifp;
{
	static char buf[BUFSIZ];
	struct ifnet ifnet;
	char ifnamebuf[IFNAMSIZ];

	KREAD(ifp, &ifnet, struct ifnet);
	KREAD(ifnet.if_name, ifnamebuf, sizeof(ifnamebuf));
	snprintf(buf, sizeof(buf), "%s%d", ifnamebuf,
		 ifnet.if_unit); /* does snprintf allow overlap copy?? */
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
	struct ifaddr ifa;
	struct sockaddr sa;
	struct in6_ifaddr if6a;
	struct in6_multi *mc = 0;
	struct ifaddr *ifap0;

	ifap0 = ifap;
	while (ifap) {
		KREAD(ifap, &ifa, struct ifaddr);
		if (ifa.ifa_addr == NULL)
			goto nextifap;
		KREAD(ifa.ifa_addr, &sa, struct sockaddr);
		if (sa.sa_family != PF_INET6)
			goto nextifap;
		KREAD(ifap, &if6a, struct in6_ifaddr);
		printf("\tinet6 %s\n", inet6_n2a(&if6a.ia_addr.sin6_addr));
	nextifap:
		ifap = TAILQ_NEXT(&ifa, ifa_link);
	}
	if (ifap0) {
		struct ifnet ifnet;
		struct ifmultiaddr ifm, *ifmp = 0;
		struct sockaddr_in6 sin6;
		struct in6_multi in6m;
		struct sockaddr_dl sdl;
		int in6_multilist_done = 0;

		KREAD(ifap0, &ifa, struct ifaddr);
		KREAD(ifa.ifa_ifp, &ifnet, struct ifnet);
		if (TAILQ_FIRST(&ifnet.if_multiaddrs))
			ifmp = TAILQ_FIRST(&ifnet.if_multiaddrs);
		while (ifmp) {
			KREAD(ifmp, &ifm, struct ifmultiaddr);
			if (ifm.ifma_addr == NULL)
				goto nextmulti;
			KREAD(ifm.ifma_addr, &sa, struct sockaddr);
			if (sa.sa_family != AF_INET6)
				goto nextmulti;
			(void)in6_multientry((struct in6_multi *)
					     ifm.ifma_protospec);
			if (ifm.ifma_lladdr == 0)
				goto nextmulti;
			KREAD(ifm.ifma_lladdr, &sdl, struct sockaddr_dl);
			printf("\t\t\tmcast-macaddr %s multicnt %d\n",
			       ether_ntoa((struct ether_addr *)LLADDR(&sdl)),
			       ifm.ifma_refcount);
		    nextmulti:
			ifmp = TAILQ_NEXT(&ifm, ifma_link);
		}
	}
#ifdef N_IN6_MK
	if (nl[N_IN6_MK].n_value != 0) {
		LIST_HEAD(in6_mktype, multi6_kludge) in6_mk;
		struct multi6_kludge *mkp, mk;
		char *nam;

		KREAD(nl[N_IN6_MK].n_value, &in6_mk, struct in6_mktype);
		KREAD(ifap0, &ifa, struct ifaddr);

		nam = strdup(ifname(ifa.ifa_ifp));
		if (!nam) {
			fprintf(stderr, "ifmcstat: not enough core\n");
			exit(1);
		}

		LIST_FOREACH(mkp, &in6_mk, mk_entry) {
			KREAD(mkp, &mk, struct multi6_kludge);
			if (strcmp(nam, ifname(mk.mk_ifp)) == 0 &&
			    LIST_FIRST(&mk.mk_head)) {
				printf("\t(on kludge entry for %s)\n", nam);
				in6_multilist(LIST_FIRST(&mk.mk_head));
			}
		}

		free(nam);
	}
#endif
}

struct in6_multi *
in6_multientry(mc)
	struct in6_multi *mc;
{
	struct in6_multi multi;

	KREAD(mc, &multi, struct in6_multi);
	printf("\t\tgroup %s", inet6_n2a(&multi.in6m_addr));
	printf(" refcnt %u\n", multi.in6m_refcount);
	return(LIST_NEXT(&multi, in6m_entry));
}

void
in6_multilist(mc)
	struct in6_multi *mc;
{
	while (mc)
		mc = in6_multientry(mc);
}
