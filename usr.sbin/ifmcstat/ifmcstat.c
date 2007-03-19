/*	$KAME: ifmcstat.c,v 1.48 2006/11/15 05:13:59 itojun Exp $	*/

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
 */

#ifdef __FreeBSD__
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <kvm.h>
#include <nlist.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <net/if.h>
#ifdef __FreeBSD__
# include <net/if_var.h>
#endif
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/igmp.h>
#ifdef HAVE_IGMPV3
#include <netinet/in_msf.h>
#endif
#ifdef HAVE_MLDV2
#include <net/route.h>
#include <netinet6/in6_msf.h>
#endif
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
#include <netinet/icmp6.h>
#define _KERNEL
/* defined _KERNEL only to define IGMP_v?_ROUTER and MLD_V?_ROUTER */
#ifdef __FreeBSD__
#include <sys/sysctl.h>
#endif
#include <netinet/igmp_var.h>
#include <netinet6/mld6_var.h>
#ifdef __FreeBSD__
#define IGMP_v1_ROUTER IGMP_V1_ROUTER
#define IGMP_v2_ROUTER IGMP_V2_ROUTER
#define IGMP_v3_ROUTER IGMP_V3_ROUTER
#endif
#undef _KERNEL
#include <arpa/inet.h>

#include <netdb.h>

kvm_t	*kvmd;
int ifindex = 0;
int af = AF_UNSPEC;

struct	nlist nl[] = {
#define	N_IFNET	0
	{ "_ifnet" },
#ifndef __FreeBSD__
#define N_IN6_MK 1
	{ "_in6_mk" },
#endif
	{ "" },
};

const char *inet6_n2a __P((struct in6_addr *));
int main __P((int, char **));
char *ifname __P((struct ifnet *));
void kread __P((u_long, void *, int));
#ifndef __FreeBSD__
void acmc __P((struct ether_multi *));
#endif
void if6_addrlist __P((struct ifaddr *));
void in6_multilist __P((struct in6_multi *));
struct in6_multi * in6_multientry __P((struct in6_multi *));
void if_addrlist(struct ifaddr *);
void in_multilist(struct in_multi *);
struct in_multi * in_multientry(struct in_multi *);
#ifdef HAVE_IGMPV3
void in_addr_slistentry(struct in_addr_slist *ias, char *heading);
#endif
#ifdef HAVE_MLDV2
void in6_addr_slistentry(struct in6_addr_slist *ias, char *heading);
#endif

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
	const int niflags = NI_NUMERICHOST;

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_addr = *p;
	if (IN6_IS_ADDR_LINKLOCAL(p) || IN6_IS_ADDR_MC_LINKLOCAL(p) ||
	    IN6_IS_ADDR_MC_NODELOCAL(p)) {
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

int main(argc, argv)
	int argc;
	char **argv;
{
	char	buf[_POSIX2_LINE_MAX], ifname[IFNAMSIZ];
	int c;
	struct	ifnet	*ifp, *nifp, ifnet;
#ifndef __FreeBSD__
#ifndef __NetBSD__
	struct	arpcom	arpcom;
#else
	struct ethercom ec;
	struct sockaddr_dl sdl;
#endif
#endif
	const char *kernel = NULL;

	/* "ifmcstat [kernel]" format is supported for backward compatiblity */
	if (argc == 2)
		kernel = argv[1];

	while ((c = getopt(argc, argv, "i:f:k:")) != -1) {
		switch (c) {
		case 'i':
			if ((ifindex = if_nametoindex(optarg)) == 0) {
				fprintf(stderr, "%s: unknown interface\n", optarg);
				exit(1);
			}
			break;
		case 'f':
			if (strcmp(optarg, "inet") == 0) {
				af = AF_INET;
				break;
			}
			if (strcmp(optarg, "inet6") == 0) {
				af = AF_INET6;
				break;
			}
			fprintf(stderr, "%s: unknown address family\n", optarg);
			exit(1);
			/*NOTREACHED*/
		case 'k':
			kernel = strdup(optarg);
			break;
		default:
			fprintf(stderr, "usage: ifmcstat [-i interface] [-f address family] [-k kernel]\n");
			exit(1);
			/*NOTREACHED*/
		}
	}

	if ((kvmd = kvm_openfiles(kernel, NULL, NULL, O_RDONLY, buf)) == NULL) {
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
#if defined(__NetBSD__) || defined(__OpenBSD__)
		nifp = ifnet.if_list.tqe_next;
#else
		nifp = ifnet.if_link.tqe_next;
#endif
		if (ifindex && ifindex != ifnet.if_index)
			goto next;
	
		printf("%s:\n", if_indextoname(ifnet.if_index, ifname));
#if defined(__NetBSD__) || defined(__OpenBSD__)
		if_addrlist(ifnet.if_addrlist.tqh_first);
		if6_addrlist(ifnet.if_addrlist.tqh_first);
#else
		if_addrlist(TAILQ_FIRST(&ifnet.if_addrhead));
		if6_addrlist(TAILQ_FIRST(&ifnet.if_addrhead));
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
#elif defined(__FreeBSD__)
		/* not supported */
#else /* __OpenBSD__ */
		if (ifnet.if_type == IFT_ETHER) {
			KREAD(ifp, &arpcom, struct arpcom);
			printf("\tenaddr %s",
			    ether_ntoa((struct ether_addr *)arpcom.ac_enaddr));
			KREAD(ifp, &arpcom, struct arpcom);
			printf(" multicnt %d", arpcom.ac_multicnt);
			acmc(arpcom.ac_multiaddrs.lh_first);
			printf("\n");
		}
#endif

next:
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

	KREAD(ifp, &ifnet, struct ifnet);
	strlcpy(buf, ifnet.if_xname, sizeof(buf));
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

#ifndef __FreeBSD__
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
	struct ifaddr ifa;
	struct sockaddr sa;
	struct in6_ifaddr if6a;
#ifndef __FreeBSD__
	struct in6_multi *mc = 0;
#endif
	struct ifaddr *ifap0;

	if (af && af != AF_INET6)
		return;
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
#ifndef __FreeBSD__
		mc = mc ? mc : if6a.ia6_multiaddrs.lh_first;
#endif
	nextifap:
#if defined(__NetBSD__) || defined(__OpenBSD__)
		ifap = ifa.ifa_list.tqe_next;
#else
		ifap = ifa.ifa_link.tqe_next;
#endif
	}
#ifdef __FreeBSD__
	if (ifap0) {
		struct ifnet ifnet;
		struct ifmultiaddr ifm, *ifmp = 0;
		struct sockaddr_dl sdl;

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
#else
	if (mc)
		in6_multilist(mc);
#endif
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

		for (mkp = in6_mk.lh_first; mkp; mkp = mk.mk_entry.le_next) {
			KREAD(mkp, &mk, struct multi6_kludge);
			if (strcmp(nam, ifname(mk.mk_ifp)) == 0 &&
			    mk.mk_head.lh_first) {
				printf("\t(on kludge entry for %s)\n", nam);
				in6_multilist(mk.mk_head.lh_first);
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
#ifdef HAVE_MLDV2
	struct in6_multi_source src;
	struct router6_info rt6i;
#endif

	KREAD(mc, &multi, struct in6_multi);
	printf("\t\tgroup %s", inet6_n2a(&multi.in6m_addr));
	printf(" refcnt %u\n", multi.in6m_refcount);

#ifdef HAVE_MLDV2
	if (multi.in6m_rti != NULL) {
		KREAD(multi.in6m_rti, &rt6i, struct router_info);
		printf("\t\t\t");
		switch (rt6i.rt6i_type) {
		case MLD_V1_ROUTER:
			printf("mldv1");
			break;
		case MLD_V2_ROUTER:
			printf("mldv2");
			break;
		default:
			printf("mldv?(%d)", rt6i.rt6i_type);
			break;
		}

		if (multi.in6m_source == NULL) {
			printf("\n");
			return(multi.in6m_entry.le_next);
		}

		KREAD(multi.in6m_source, &src, struct in6_multi_source);
		printf(" mode=%s grpjoin=%d\n",
		    src.i6ms_mode == MCAST_INCLUDE ? "include" :
		    src.i6ms_mode == MCAST_EXCLUDE ? "exclude" :
		    "???",
		    src.i6ms_grpjoin);
		in6_addr_slistentry(src.i6ms_cur, "current");
		in6_addr_slistentry(src.i6ms_rec, "recorded");
		in6_addr_slistentry(src.i6ms_in, "included");
		in6_addr_slistentry(src.i6ms_ex, "excluded");
		in6_addr_slistentry(src.i6ms_alw, "allowed");
		in6_addr_slistentry(src.i6ms_blk, "blocked");
		in6_addr_slistentry(src.i6ms_toin, "to-include");
		in6_addr_slistentry(src.i6ms_ex, "to-exclude");
	}
#endif
	return(multi.in6m_entry.le_next);
}

#ifdef HAVE_MLDV2
void
in6_addr_slistentry(struct in6_addr_slist *ias, char *heading)
{
	struct in6_addr_slist slist;
	struct i6as_head head;
	struct in6_addr_source src;

	if (ias == NULL) {
		printf("\t\t\t\t%s (none)\n", heading);
		return;
	}
	memset(&slist, 0, sizeof(slist));
	KREAD(ias, &slist, struct in6_addr_source);
	printf("\t\t\t\t%s (entry num=%d)\n", heading, slist.numsrc);
	if (slist.numsrc == 0) {
		return;
	}
	KREAD(slist.head, &head, struct i6as_head);

	KREAD(head.lh_first, &src, struct in6_addr_source);
	while (1) {
		printf("\t\t\t\t\tsource %s (ref=%d)\n",
			inet6_n2a(&src.i6as_addr.sin6_addr),
			src.i6as_refcount);
		if (src.i6as_list.le_next == NULL)
			break;
		KREAD(src.i6as_list.le_next, &src, struct in6_addr_source);
	}
	return;
}
#endif

void
in6_multilist(mc)
	struct in6_multi *mc;
{
	while (mc)
		mc = in6_multientry(mc);
}

void
if_addrlist(ifap)
	struct ifaddr *ifap;
{
	struct ifaddr ifa;
	struct sockaddr sa;
	struct in_ifaddr ia;
#ifndef __FreeBSD__
	struct in_multi *mc = 0;
#endif
	struct ifaddr *ifap0;

	if (af && af != AF_INET)
		return;
	ifap0 = ifap;
	while (ifap) {
		KREAD(ifap, &ifa, struct ifaddr);
		if (ifa.ifa_addr == NULL)
			goto nextifap;
		KREAD(ifa.ifa_addr, &sa, struct sockaddr);
		if (sa.sa_family != PF_INET)
			goto nextifap;
		KREAD(ifap, &ia, struct in_ifaddr);
		printf("\tinet %s\n", inet_ntoa(ia.ia_addr.sin_addr));
#ifndef __FreeBSD__
		mc = mc ? mc : ia.ia_multiaddrs.lh_first;
#endif
	nextifap:
#if defined(__NetBSD__) || defined(__OpenBSD__)
		ifap = ifa.ifa_list.tqe_next;
#else
		ifap = ifa.ifa_link.tqe_next;
#endif
	}
#ifdef __FreeBSD__
	if (ifap0) {
		struct ifnet ifnet;
		struct ifmultiaddr ifm, *ifmp = 0;
		struct sockaddr_dl sdl;

		KREAD(ifap0, &ifa, struct ifaddr);
		KREAD(ifa.ifa_ifp, &ifnet, struct ifnet);
		if (TAILQ_FIRST(&ifnet.if_multiaddrs))
			ifmp = TAILQ_FIRST(&ifnet.if_multiaddrs);
		while (ifmp) {
			KREAD(ifmp, &ifm, struct ifmultiaddr);
			if (ifm.ifma_addr == NULL)
				goto nextmulti;
			KREAD(ifm.ifma_addr, &sa, struct sockaddr);
			if (sa.sa_family != AF_INET)
				goto nextmulti;
			(void)in_multientry((struct in_multi *)
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
#else /* !FreeBSD */
	if (mc)
		in_multilist(mc);
#endif
}

void
in_multilist(mc)
	struct in_multi *mc;
{
	while (mc)
		mc = in_multientry(mc);
}

struct in_multi *
in_multientry(mc)
	struct in_multi *mc;
{
	struct in_multi multi;
	struct router_info rti;
#ifdef HAVE_IGMPV3
	struct in_multi_source src;
#endif

	KREAD(mc, &multi, struct in_multi);
	printf("\t\tgroup %s\n", inet_ntoa(multi.inm_addr));

	if (multi.inm_rti != NULL) {
		KREAD(multi.inm_rti, &rti, struct router_info);
		printf("\t\t\t");
		switch (rti.rti_type) {
		case IGMP_v1_ROUTER:
			printf("igmpv1");
			break;
		case IGMP_v2_ROUTER:
			printf("igmpv2");
			break;
#ifdef HAVE_IGMPV3
		case IGMP_v3_ROUTER:
			printf("igmpv3");
			break;
#endif
		default:
			printf("igmpv?(%d)", rti.rti_type);
			break;
		}

#ifdef HAVE_IGMPV3
		if (multi.inm_source == NULL) {
			printf("\n");
			return (multi.inm_list.le_next);
		}

		KREAD(multi.inm_source, &src, struct in_multi_source);
		printf(" mode=%s grpjoin=%d\n",
		    src.ims_mode == MCAST_INCLUDE ? "include" :
		    src.ims_mode == MCAST_EXCLUDE ? "exclude" :
		    "???",
		    src.ims_grpjoin);
		in_addr_slistentry(src.ims_cur, "current");
		in_addr_slistentry(src.ims_rec, "recorded");
		in_addr_slistentry(src.ims_in, "included");
		in_addr_slistentry(src.ims_ex, "excluded");
		in_addr_slistentry(src.ims_alw, "allowed");
		in_addr_slistentry(src.ims_blk, "blocked");
		in_addr_slistentry(src.ims_toin, "to-include");
		in_addr_slistentry(src.ims_ex, "to-exclude");
#else
		printf("\n");
#endif
	}

#ifdef __FreeBSD__
	return (NULL);
#else
	return (multi.inm_list.le_next);
#endif
}

#ifdef HAVE_IGMPV3
void
in_addr_slistentry(struct in_addr_slist *ias, char *heading)
{
	struct in_addr_slist slist;
	struct ias_head head;
	struct in_addr_source src;

	if (ias == NULL) {
		printf("\t\t\t\t%s (none)\n", heading);
		return;
	}
	memset(&slist, 0, sizeof(slist));
	KREAD(ias, &slist, struct in_addr_source);
	printf("\t\t\t\t%s (entry num=%d)\n", heading, slist.numsrc);
	if (slist.numsrc == 0) {
		return;
	}
	KREAD(slist.head, &head, struct ias_head);

	KREAD(head.lh_first, &src, struct in_addr_source);
	while (1) {
		printf("\t\t\t\t\tsource %s (ref=%d)\n",
			inet_ntoa(src.ias_addr.sin_addr), src.ias_refcount);
		if (src.ias_list.le_next == NULL)
			break;
		KREAD(src.ias_list.le_next, &src, struct in_addr_source);
	}
	return;
}
#endif
