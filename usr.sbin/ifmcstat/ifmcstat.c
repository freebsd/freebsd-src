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
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
# include <net/if_var.h>
#endif
#include <net/if_types.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#ifndef __NetBSD__
# ifdef	__FreeBSD__
#  define	KERNEL
# endif
# include <netinet/if_ether.h>
# ifdef	__FreeBSD__
#  undef	KERNEL
# endif
#else
# include <net/if_ether.h>
#endif
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
#if !(defined(__FreeBSD__) && __FreeBSD__ >= 3)
void acmc __P((struct ether_multi *));
#endif
void if6_addrlist __P((struct ifaddr *));
void in6_multilist __P((struct in6_multi *));
struct in6_multi * in6_multientry __P((struct in6_multi *));

#if !defined(__NetBSD__) && !(defined(__FreeBSD__) && __FreeBSD__ >= 3) && !defined(__OpenBSD__)
#ifdef __bsdi__
struct ether_addr {
	u_int8_t ether_addr_octet[6];
};
#endif
static char *ether_ntoa __P((struct ether_addr *));
#endif

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
#ifndef __NetBSD__
	struct	arpcom	arpcom;
#else
	struct ethercom ec;
	struct sockaddr_dl sdl;
#endif

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

#if defined(__NetBSD__) || defined(__OpenBSD__)
		if6_addrlist(ifnet.if_addrlist.tqh_first);
		nifp = ifnet.if_list.tqe_next;
#elif defined(__FreeBSD__) && __FreeBSD__ >= 3
		if6_addrlist(TAILQ_FIRST(&ifnet.if_addrhead));
		nifp = ifnet.if_link.tqe_next;
#else
		if6_addrlist(ifnet.if_addrlist);
		nifp = ifnet.if_next;
#endif

#ifdef __NetBSD__
		KREAD(ifnet.if_sadl, &sdl, struct sockaddr_dl);
		if (sdl.sdl_type == IFT_ETHER) {
			printf("\tenaddr %s",
			       ether_ntoa((struct ether_addr *)LLADDR(&sdl)));
			KREAD(ifp, &ec, struct ethercom);
			printf(" multicnt %d", ec.ec_multicnt);
			acmc(ec.ec_multiaddrs.lh_first);
			printf("\n");
		}
#elif defined(__FreeBSD__) && __FreeBSD__ >= 3
		/* not supported */
#else
		if (ifnet.if_type == IFT_ETHER) {
			KREAD(ifp, &arpcom, struct arpcom);
			printf("\tenaddr %s",
			    ether_ntoa((struct ether_addr *)arpcom.ac_enaddr));
			KREAD(ifp, &arpcom, struct arpcom);
			printf(" multicnt %d", arpcom.ac_multicnt);
#ifdef __OpenBSD__
			acmc(arpcom.ac_multiaddrs.lh_first);
#else
			acmc(arpcom.ac_multiaddrs);
#endif
			printf("\n");
		}
#endif

		ifp = nifp;
	}

	exit(0);
	/*NOTREACHED*/
}

char *ifname(ifp)
	struct ifnet *ifp;
{
	static char buf[BUFSIZ];

#if defined(__NetBSD__) || defined(__OpenBSD__)
	KREAD(ifp->if_xname, buf, IFNAMSIZ);
#else
	KREAD(ifp->if_name, buf, IFNAMSIZ);
#endif
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

#if !(defined(__FreeBSD__) && __FreeBSD__ >= 3)
void acmc(am)
	struct ether_multi *am;
{
	struct ether_multi em;

	while (am) {
		KREAD(am, &em, struct ether_multi);

		printf("\n\t\t");
		printf("%s -- ", ether_ntoa((struct ether_addr *)em.enm_addrlo));
		printf("%s ", ether_ntoa((struct ether_addr *)&em.enm_addrhi));
		printf("%d", em.enm_refcount);
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
		am = em.enm_next;
#else
		am = em.enm_list.le_next;
#endif
	}
}
#endif

void
if6_addrlist(ifap)
	struct ifaddr *ifap;
{
	static char in6buf[BUFSIZ];
	struct ifaddr ifa;
	struct sockaddr sa;
	struct in6_ifaddr if6a;
	struct in6_multi *mc = 0;
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	struct ifaddr *ifap0;
#endif /* __FreeBSD__ >= 3 */

#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	ifap0 = ifap;
#endif /* __FreeBSD__ >= 3 */
	while (ifap) {
		KREAD(ifap, &ifa, struct ifaddr);
		if (ifa.ifa_addr == NULL)
			goto nextifap;
		KREAD(ifa.ifa_addr, &sa, struct sockaddr);
		if (sa.sa_family != PF_INET6)
			goto nextifap;
		KREAD(ifap, &if6a, struct in6_ifaddr);
		printf("\tinet6 %s\n",
		       inet_ntop(AF_INET6,
				 (const void *)&if6a.ia_addr.sin6_addr,
				 in6buf, sizeof(in6buf)));
#if !(defined(__FreeBSD__) && __FreeBSD__ >= 3)
		mc = mc ? mc : if6a.ia6_multiaddrs.lh_first;
#endif
	nextifap:
#if defined(__NetBSD__) || defined(__OpenBSD__)
		ifap = ifa.ifa_list.tqe_next;
#elif defined(__FreeBSD__) && __FreeBSD__ >= 3
		ifap = ifa.ifa_link.tqe_next;
#else
		ifap = ifa.ifa_next;
#endif /* __FreeBSD__ >= 3 */
	}
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	if (ifap0) {
		struct ifnet ifnet;
		struct ifmultiaddr ifm, *ifmp = 0;
		struct sockaddr_in6 sin6;
		struct in6_multi in6m;
		struct sockaddr_dl sdl;
		int in6_multilist_done = 0;

		KREAD(ifap0, &ifa, struct ifaddr);
		KREAD(ifa.ifa_ifp, &ifnet, struct ifnet);
		if (ifnet.if_multiaddrs.lh_first)
			ifmp = ifnet.if_multiaddrs.lh_first;
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
			ifmp = ifm.ifma_link.le_next;
		}
	}
#else
	if (mc)
		in6_multilist(mc);
#endif
}

struct in6_multi *
in6_multientry(mc)
	struct in6_multi *mc;
{
	static char mcbuf[BUFSIZ];
	struct in6_multi multi;

	KREAD(mc, &multi, struct in6_multi);
	printf("\t\tgroup %s\n", inet_ntop(AF_INET6,
					   (const void *)&multi.in6m_addr,
					   mcbuf, sizeof(mcbuf)));
	return(multi.in6m_entry.le_next);
}

void
in6_multilist(mc)
	struct in6_multi *mc;
{
	while (mc)
		mc = in6_multientry(mc);
}

#if !defined(__NetBSD__) && !(defined(__FreeBSD__) && __FreeBSD__ >= 3) && !defined(__OpenBSD__)
static char *
ether_ntoa(e)
	struct ether_addr *e;
{
	static char buf[20];
	u_char *p;

	p = (u_char *)e;

	snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
		p[0], p[1], p[2], p[3], p[4], p[5]);
	return buf;
}
#endif
