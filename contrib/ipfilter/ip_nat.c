/*
 * (C)opyright 1995-1996 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * Added redirect stuff and a LOT of bug fixes. (mcn@EnGarde.com)
 */
#if !defined(lint) && defined(LIBC_SCCS)
static	char	sccsid[] = "@(#)ip_nat.c	1.11 6/5/96 (C) 1995 Darren Reed";
static	char	rcsid[] = "$Id: ip_nat.c,v 2.0.2.18 1997/05/24 07:34:44 darrenr Exp $";
#endif

#if defined(__FreeBSD__) && defined(KERNEL) && !defined(_KERNEL)
#define _KERNEL
#endif

#if !defined(_KERNEL) && !defined(KERNEL)
# include <stdio.h>
# include <string.h>
# include <stdlib.h>
#endif
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/file.h>
#if defined(KERNEL) && (__FreeBSD_version >= 220000)
# include <sys/filio.h>
# include <sys/fnctl.h>
#else
# include <sys/ioctl.h>
#endif
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#ifdef _KERNEL
# include <sys/systm.h>
#endif
#if !defined(__SVR4) && !defined(__svr4__)
# include <sys/mbuf.h>
#else
# include <sys/filio.h>
# include <sys/byteorder.h>
# include <sys/dditypes.h>
# include <sys/stream.h>
# include <sys/kmem.h>
#endif
#if __FreeBSD_version >= 300000
# include <sys/queue.h>
#endif
#include <net/if.h>
#if __FreeBSD_version >= 300000
# include <net/if_var.h>
#endif
#ifdef sun
#include <net/af.h>
#endif
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#ifdef RFC1825
#include <vpn/md5.h>
#include <vpn/ipsec.h>
extern struct ifnet vpnif;
#endif

#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/tcpip.h>
#include <netinet/ip_icmp.h>
#include "netinet/ip_compat.h"
#include "netinet/ip_fil.h"
#include "netinet/ip_proxy.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_frag.h"
#include "netinet/ip_state.h"
#ifndef	MIN
#define	MIN(a,b)	(((a)<(b))?(a):(b))
#endif
#undef	SOCKADDR_IN
#define	SOCKADDR_IN	struct sockaddr_in

nat_t	*nat_table[2][NAT_SIZE], *nat_instances = NULL;
ipnat_t	*nat_list = NULL;
u_long	fr_defnatage = 1200;
natstat_t nat_stats;
#if	SOLARIS && defined(_KERNEL)
extern	kmutex_t	ipf_nat;
extern	kmutex_t	ipf_natfrag;
#endif

static	int	flush_nattable __P((void)), clear_natlist __P((void));

void fix_outcksum(sp, n)
u_short *sp;
u_long n;
{
	register u_short sumshort;
	register u_long sum1;

#ifdef sparc
	sum1 = (~(*sp)) & 0xffff;
#else
	sum1 = (~ntohs(*sp)) & 0xffff;
#endif
	sum1 += (n);
	sum1 = (sum1 >> 16) + (sum1 & 0xffff);
	/* Again */
	sum1 = (sum1 >> 16) + (sum1 & 0xffff);
	sumshort = ~(u_short)sum1;
	*(sp) = htons(sumshort);
}


void fix_incksum(sp, n)
u_short *sp;
u_long n;
{
	register u_short sumshort;
	register u_long sum1;

#ifdef sparc
	sum1 = (~(*sp)) & 0xffff;
#else
	sum1 = (~ntohs(*sp)) & 0xffff;
#endif
	sum1 += ~(n) & 0xffff;
	sum1 = (sum1 >> 16) + (sum1 & 0xffff);
	/* Again */
	sum1 = (sum1 >> 16) + (sum1 & 0xffff);
	sumshort = ~(u_short)sum1;
	*(sp) = htons(sumshort);
}


/*
 * How the NAT is organised and works.
 *
 * Inside (interface y) NAT       Outside (interface x)
 * -------------------- -+- -------------------------------------
 * Packet going          |   out, processsed by ip_natout() for x
 * ------------>         |   ------------>
 * src=10.1.1.1          |   src=192.1.1.1
 *                       |
 *                       |   in, processed by ip_natin() for x
 * <------------         |   <------------
 * dst=10.1.1.1          |   dst=192.1.1.1
 * -------------------- -+- -------------------------------------
 * ip_natout() - changes ip_src and if required, sport
 *             - creates a new mapping, if required.
 * ip_natin()  - changes ip_dst and if required, dport
 *
 * In the NAT table, internal source is recorded as "in" and externally
 * seen as "out".
 */

/*
 * Handle ioctls which manipulate the NAT.
 */
int nat_ioctl(data, cmd, mode)
caddr_t data;
int cmd, mode;
{
	register ipnat_t *nat, *n = NULL, **np = NULL;
	ipnat_t natd;
	int error = 0, ret, s;

	/*
	 * For add/delete, look to see if the NAT entry is already present
	 */
	MUTEX_ENTER(&ipf_nat);
	SPLNET(s);
	if ((cmd == SIOCADNAT) || (cmd == SIOCRMNAT)) {
		IRCOPY(data, (char *)&natd, sizeof(natd));
		nat = &natd;
		for (np = &nat_list; (n = *np); np = &n->in_next)
			if (!bcmp((char *)&nat->in_flags, (char *)&n->in_flags,
					IPN_CMPSIZ))
				break;
	}

	switch (cmd)
	{
	case SIOCADNAT :
		if (!(mode & FWRITE)) {
			error = EPERM;
			break;
		}
		if (n) {
			error = EEXIST;
			break;
		}
		KMALLOC(n, ipnat_t *, sizeof(*n));
		if (n == NULL) {
			error = ENOMEM;
			break;
		}
		IRCOPY((char *)data, (char *)n, sizeof(*n));
		n->in_ifp = (void *)GETUNIT(n->in_ifname);
		n->in_apr = ap_match(n->in_p, n->in_plabel);
		n->in_next = *np;
		n->in_use = 0;
		n->in_space = ~(0xffffffff & ntohl(n->in_outmsk));
		if (n->in_space) /* lose 2: broadcast + network address */
			n->in_space -= 2;
		else
			n->in_space = 1;	/* single IP# mapping */
		if (n->in_outmsk != 0xffffffff)
			n->in_nip = ntohl(n->in_outip) + 1;
		else
			n->in_nip = ntohl(n->in_outip);
		if (n->in_redir & NAT_MAP) {
			n->in_pnext = ntohs(n->in_pmin);
			/*
			 * Multiply by the number of ports made available.
			 */
			if (ntohs(n->in_pmax) > ntohs(n->in_pmin))
				n->in_space *= (ntohs(n->in_pmax) -
						ntohs(n->in_pmin));
		}
		/* Otherwise, these fields are preset */
		*np = n;
		nat_stats.ns_rules++;
		break;
	case SIOCRMNAT :
		if (!(mode & FWRITE)) {
			error = EPERM;
			break;
		}
		if (!n) {
			error = ESRCH;
			break;
		}
		*np = n->in_next;
		if (!n->in_use) {
			if (n->in_apr)
				ap_free(n->in_apr);
			KFREE(n);
			nat_stats.ns_rules--;
		} else {
			n->in_flags |= IPN_DELETE;
			n->in_next = NULL;
		}
		break;
	case SIOCGNATS :
		nat_stats.ns_table[0] = nat_table[0];
		nat_stats.ns_table[1] = nat_table[1];
		nat_stats.ns_list = nat_list;
		IWCOPY((char *)&nat_stats, (char *)data, sizeof(nat_stats));
		break;
	case SIOCGNATL :
	    {
		natlookup_t nl;

		IRCOPY((char *)data, (char *)&nl, sizeof(nl));

		if (nat_lookupredir(&nl))
			IWCOPY((char *)&nl, (char *)data, sizeof(nl));
		else
			error = ESRCH;
		break;
	    }
	case SIOCFLNAT :
		if (!(mode & FWRITE)) {
			error = EPERM;
			break;
		}
		ret = flush_nattable();
		IWCOPY((caddr_t)&ret, data, sizeof(ret));
		break;
	case SIOCCNATL :
		if (!(mode & FWRITE)) {
			error = EPERM;
			break;
		}
		ret = clear_natlist();
		IWCOPY((caddr_t)&ret, data, sizeof(ret));
		break;
	case FIONREAD :
#ifdef	IPFILTER_LOG
		*(int *)data = iplused[IPL_LOGNAT];
#endif
		break;
	}
	SPLX(s);
	MUTEX_EXIT(&ipf_nat);
	return error;
}


static void nat_delete(natd)
struct nat *natd;
{
	register struct nat **natp, *nat;
	struct ipnat *ipn;

	for (natp = natd->nat_hstart[0]; (nat = *natp);
	     natp = &nat->nat_hnext[0])
		if (nat == natd) {
			*natp = nat->nat_hnext[0];
			break;
		}

	for (natp = natd->nat_hstart[1]; (nat = *natp);
	     natp = &nat->nat_hnext[1])
		if (nat == natd) {
			*natp = nat->nat_hnext[1];
			break;
		}

	if ((ipn = natd->nat_ptr)) {
		ipn->in_space++;
		ipn->in_use--;
		if (!ipn->in_use && (ipn->in_flags & IPN_DELETE)) {
			if (ipn->in_apr)
				ap_free(ipn->in_apr);
			KFREE(ipn);
			nat_stats.ns_rules--;
		}
	}
	MUTEX_ENTER(&ipf_natfrag);
	if (nat->nat_frag && nat->nat_frag->ipfr_data == nat)
		nat->nat_frag->ipfr_data = NULL;
	MUTEX_EXIT(&ipf_natfrag);
	KFREE(natd);
}


/*
 * flush_nattable - clear the NAT table of all mapping entries.
 */
static int flush_nattable()
{
	register nat_t *nat, **natp;
	register int j = 0;
  
	/*
	 * Everything will be deleted, so lets just make it the deletions
	 * quicker.
	 */
	bzero((char *)nat_table[0], sizeof(nat_table[0]));
	bzero((char *)nat_table[1], sizeof(nat_table[1]));

	for (natp = &nat_instances; (nat = *natp); ) {
		*natp = nat->nat_next;
		nat_delete(nat);
		j++;
	}

	return j;
}


/*
 * clear_natlist - delete all entries in the active NAT mapping list.
 */
static int clear_natlist()
{
	register ipnat_t *n, **np = &nat_list;
	int i = 0;

	while ((n = *np)) {
		*np = n->in_next;
		if (!n->in_use) {
			if (n->in_apr)
				ap_free(n->in_apr);
			KFREE(n);
			nat_stats.ns_rules--;
			i++;
		} else {
			n->in_flags |= IPN_DELETE;
			n->in_next = NULL;
		}
	}
	nat_stats.ns_inuse = 0;
	return i;
}


/*
 * Create a new NAT table entry.
 */
nat_t *nat_new(np, ip, fin, flags, direction)
ipnat_t *np;
ip_t *ip;
fr_info_t *fin;
u_short flags;
int direction;
{
	register u_long sum1, sum2, sumd;
	u_short port = 0, sport = 0, dport = 0, nport = 0;
	struct in_addr in;
	tcphdr_t *tcp = NULL;
	nat_t *nat, **natp;
	u_short nflags;

	nflags = flags & np->in_flags;
	if (flags & IPN_TCPUDP) {
		tcp = (tcphdr_t *)fin->fin_dp;
		sport = tcp->th_sport;
		dport = tcp->th_dport;
	}

	/* Give me a new nat */
	KMALLOC(nat, nat_t *, sizeof(*nat));
	if (nat == NULL)
		return NULL;

	bzero((char *)nat, sizeof(*nat));
	nat->nat_flags = flags;

	/*
	 * Search the current table for a match.
	 */
	if (direction == NAT_OUTBOUND) {
#if SOLARIS
		ill_t *ill = fin->fin_ifp;
#else
		struct ifnet *ifp = fin->fin_ifp;
#endif
		/*
		 * If it's an outbound packet which doesn't match any existing
		 * record, then create a new port
		 */
		do {
			port = 0;
			in.s_addr = np->in_nip;
			if (!in.s_addr && (np->in_outmsk == 0xffffffff)) {
#if SOLARIS
				in.s_addr = ill->ill_ipif->ipif_local_addr;
#else
				struct ifaddr *ifa;
				struct sockaddr_in *sin;

# if	(__FreeBSD_version >= 300000)
				ifa = TAILQ_FIRST(&ifp->if_addrhead);
# else
#  ifdef	__NetBSD__
				ifa = ifp->if_addrlist.tqh_first;
#  else
				ifa = ifp->if_addrlist;
#  endif
# endif
# if	BSD < 199306
				sin = (SOCKADDR_IN *)&ifa->ifa_addr;
# else
				sin = (SOCKADDR_IN *)ifa->ifa_addr;
				while (sin && ifa &&
				       sin->sin_family != AF_INET) {
#  if	(__FreeBSD_version >= 300000)
					ifa = TAILQ_NEXT(ifa, ifa_link);
#  else
#   ifdef	__NetBSD__
					ifa = ifa->ifa_list.tqe_next;
#   else
					ifa = ifa->ifa_next;
#   endif
#  endif
					sin = (SOCKADDR_IN *)ifa->ifa_addr;
				}
				if (!ifa)
					sin = NULL;
				if (!sin) {
					KFREE(nat);
					return NULL;
				}
# endif
				in = sin->sin_addr;
				in.s_addr = ntohl(in.s_addr);
#endif
			}
			if (nflags & IPN_TCPUDP) {
				port = htons(np->in_pnext++);
				if (np->in_pnext >= ntohs(np->in_pmax)) {
					np->in_pnext = ntohs(np->in_pmin);
					np->in_space--;
					if (np->in_outmsk != 0xffffffff)
						np->in_nip++;
				}
			} else if (np->in_outmsk != 0xffffffff) {
				np->in_space--;
				np->in_nip++;
			}

			if (!port && (flags & IPN_TCPUDP))
				port = sport;
			if ((np->in_nip & ntohl(np->in_outmsk)) >
			    ntohl(np->in_outip))
				np->in_nip = ntohl(np->in_outip) + 1;
		} while (nat_inlookup(fin->fin_ifp, flags, ip->ip_dst,
				      dport, in, port));

		/* Setup the NAT table */
		nat->nat_inip = ip->ip_src;
		nat->nat_outip.s_addr = htonl(in.s_addr);
		nat->nat_oip = ip->ip_dst;

		sum1 = (ntohl(ip->ip_src.s_addr) & 0xffff) +
			(ntohl(ip->ip_src.s_addr) >> 16) + ntohs(sport);

		sum2 = (in.s_addr & 0xffff) + (in.s_addr >> 16) + ntohs(port);

		if (flags & IPN_TCPUDP) {
			nat->nat_inport = sport;
			nat->nat_outport = port;
			nat->nat_oport = dport;
		}
	} else {

		/*
		 * Otherwise, it's an inbound packet. Most likely, we don't
		 * want to rewrite source ports and source addresses. Instead,
		 * we want to rewrite to a fixed internal address and fixed
		 * internal port.
		 */
		in.s_addr = ntohl(np->in_inip);
		if (!(nport = np->in_pnext))
			nport = dport;

		nat->nat_inip.s_addr = htonl(in.s_addr);
		nat->nat_outip = ip->ip_dst;
		nat->nat_oip = ip->ip_src;

		sum1 = (ntohl(ip->ip_dst.s_addr) & 0xffff) +
			(ntohl(ip->ip_dst.s_addr) >> 16) + ntohs(dport);

		sum2 = (in.s_addr & 0xffff) + (in.s_addr >> 16) + ntohs(nport);

		if (flags & IPN_TCPUDP) {
			nat->nat_inport = nport;
			nat->nat_outport = dport;
			nat->nat_oport = sport;
		}
	}

	/* Do it twice */
	sum1 = (sum1 & 0xffff) + (sum1 >> 16);
	sum1 = (sum1 & 0xffff) + (sum1 >> 16);

	/* Do it twice */
	sum2 = (sum2 & 0xffff) + (sum2 >> 16);
	sum2 = (sum2 & 0xffff) + (sum2 >> 16);

	if (sum1 > sum2)
		sum2--; /* Because ~1 == -2, We really need ~1 == -1 */
	sumd = sum2 - sum1;
	sumd = (sumd & 0xffff) + (sumd >> 16);
	nat->nat_sumd = (sumd & 0xffff) + (sumd >> 16);

	if ((flags & IPN_TCPUDP) && ((sport != port) || (dport != nport))) {
		if (direction == NAT_OUTBOUND)
			sum1 = (ntohl(ip->ip_src.s_addr) & 0xffff) +
				(ntohl(ip->ip_src.s_addr) >> 16);
		else
			sum1 = (ntohl(ip->ip_dst.s_addr) & 0xffff) +
				(ntohl(ip->ip_dst.s_addr) >> 16);

		sum2 = (in.s_addr & 0xffff) + (in.s_addr >> 16);

		/* Do it twice */
		sum1 = (sum1 & 0xffff) + (sum1 >> 16);
		sum1 = (sum1 & 0xffff) + (sum1 >> 16);

		/* Do it twice */
		sum2 = (sum2 & 0xffff) + (sum2 >> 16);
		sum2 = (sum2 & 0xffff) + (sum2 >> 16);

		if (sum1 > sum2)
			sum2--; /* Because ~1 == -2, We really need ~1 == -1 */
		sumd = sum2 - sum1;
		sumd = (sumd & 0xffff) + (sumd >> 16);
		nat->nat_ipsumd = (sumd & 0xffff) + (sumd >> 16);
	} else
		nat->nat_ipsumd = nat->nat_sumd;

	in.s_addr = htonl(in.s_addr);
	nat->nat_next = nat_instances;
	nat_instances = nat;
	natp = &nat_table[0][nat->nat_inip.s_addr % NAT_SIZE];
	nat->nat_hstart[0] = natp;
	nat->nat_hnext[0] = *natp;
	*natp = nat;
	natp = &nat_table[1][nat->nat_outip.s_addr % NAT_SIZE];
	nat->nat_hstart[1] = natp;
	nat->nat_hnext[1] = *natp;
	*natp = nat;
	nat->nat_ptr = np;
	nat->nat_bytes = 0;
	nat->nat_pkts = 0;
	nat->nat_ifp = fin->fin_ifp;
	nat->nat_dir = direction;
	if (direction == NAT_OUTBOUND) {
		if (flags & IPN_TCPUDP)
			tcp->th_sport = htons(port);
	} else {
		if (flags & IPN_TCPUDP)
			tcp->th_dport = htons(nport);
	}
	nat_stats.ns_added++;
	nat_stats.ns_inuse++;
	np->in_use++;
	return nat;
}


/*
 * NB: these lookups don't lock access to the list, it assume it has already
 * been done!
 */
/*
 * Lookup a nat entry based on the mapped destination ip address/port and
 * real source address/port.  We use this lookup when receiving a packet,
 * we're looking for a table entry, based on the destination address.
 * NOTE: THE PACKET BEING CHECKED (IF FOUND) HAS A MAPPING ALREADY.
 */
nat_t *nat_inlookup(ifp, flags, src, sport, mapdst, mapdport)
void *ifp;
register int flags;
struct in_addr src , mapdst;
u_short sport, mapdport;
{
	register nat_t *nat;

	flags &= IPN_TCPUDP;

	nat = nat_table[1][mapdst.s_addr % NAT_SIZE];
	for (; nat; nat = nat->nat_hnext[1])
		if ((!ifp || ifp == nat->nat_ifp) &&
		    nat->nat_oip.s_addr == src.s_addr &&
		    nat->nat_outip.s_addr == mapdst.s_addr &&
		    flags == nat->nat_flags && (!flags ||
		     (nat->nat_oport == sport &&
		      nat->nat_outport == mapdport)))
			return nat;
	return NULL;
}


/*
 * Lookup a nat entry based on the source 'real' ip address/port and
 * destination address/port.  We use this lookup when sending a packet out,
 * we're looking for a table entry, based on the source address.
 * NOTE: THE PACKET BEING CHECKED (IF FOUND) HAS A MAPPING ALREADY.
 */
nat_t *nat_outlookup(ifp, flags, src, sport, dst, dport)
void *ifp;
register int flags;
struct in_addr src , dst;
u_short sport, dport;
{
	register nat_t *nat;

	flags &= IPN_TCPUDP;

	nat = nat_table[0][src.s_addr % NAT_SIZE];
	for (; nat; nat = nat->nat_hnext[0])
		if ((!ifp || ifp == nat->nat_ifp) &&
		    nat->nat_inip.s_addr == src.s_addr &&
		    nat->nat_oip.s_addr == dst.s_addr &&
		    flags == nat->nat_flags && (!flags ||
		     (nat->nat_inport == sport && nat->nat_oport == dport)))
			return nat;
	return NULL;
}


/*
 * Lookup a nat entry based on the mapped source ip address/port and
 * real destination address/port.  We use this lookup when sending a packet
 * out, we're looking for a table entry, based on the source address.
 */
nat_t *nat_lookupmapip(ifp, flags, mapsrc, mapsport, dst, dport)
void *ifp;
register int flags;
struct in_addr mapsrc , dst;
u_short mapsport, dport;
{
	register nat_t *nat;

	flags &= IPN_TCPUDP;

	nat = nat_table[1][mapsrc.s_addr % NAT_SIZE];
	for (; nat; nat = nat->nat_hnext[0])
		if ((!ifp || ifp == nat->nat_ifp) &&
		    nat->nat_oip.s_addr == dst.s_addr &&
		    nat->nat_outip.s_addr == mapsrc.s_addr &&
		    flags == nat->nat_flags && (!flags ||
		     (nat->nat_outport == mapsport &&
		      nat->nat_oport == dport)))
			return nat;
	return NULL;
}


/*
 * Lookup the NAT tables to search for a matching redirect
 */
nat_t *nat_lookupredir(np)
register natlookup_t *np;
{
	nat_t *nat;

	/*
	 * If nl_inip is non null, this is a lookup based on the real
	 * ip address. Else, we use the fake.
	 */
	if ((nat = nat_outlookup(NULL, IPN_TCPUDP, np->nl_inip, np->nl_inport,
				 np->nl_outip, np->nl_outport))) {
		np->nl_inip = nat->nat_outip;
		np->nl_inport = nat->nat_outport;
	}
	return nat;
}


/*
 * Packets going out on the external interface go through this.
 * Here, the source address requires alteration, if anything.
 */
int ip_natout(ip, hlen, fin)
ip_t *ip;
int hlen;
fr_info_t *fin;
{
	register ipnat_t *np;
	register u_long ipa;
	tcphdr_t *tcp = NULL;
	nat_t *nat;
	u_short nflags = 0, sport = 0, dport = 0, *csump = NULL;
	struct ifnet *ifp;
	frentry_t *fr;

	if ((fr = fin->fin_fr) && !(fr->fr_flags & FR_DUP) &&
	    fr->fr_tif.fd_ifp && fr->fr_tif.fd_ifp != (void *)-1)
		ifp = fr->fr_tif.fd_ifp;
	else
		ifp = fin->fin_ifp;

	if (!(ip->ip_off & 0x1fff) && !(fin->fin_fi.fi_fl & FI_SHORT)) {
		if (ip->ip_p == IPPROTO_TCP)
			nflags = IPN_TCP;
		else if (ip->ip_p == IPPROTO_UDP)
			nflags = IPN_UDP;
		if (nflags) {
			tcp = (tcphdr_t *)fin->fin_dp;
			sport = tcp->th_sport;
			dport = tcp->th_dport;
		}
	}

	ipa = ip->ip_src.s_addr;

	MUTEX_ENTER(&ipf_nat);
	if ((nat = ipfr_nat_knownfrag(ip, fin)))
		;
	else if ((nat = nat_outlookup(fin->fin_ifp, nflags, ip->ip_src, sport,
				      ip->ip_dst, dport)))
		np = nat->nat_ptr;
	else
		/*
		 * If there is no current entry in the nat table for this IP#,
		 * create one for it (if there is a matching rule).
		 */
		for (np = nat_list; np; np = np->in_next)
			if ((np->in_ifp == ifp) && np->in_space &&
			    (!np->in_flags || (np->in_flags & nflags)) &&
			    ((ipa & np->in_inmsk) == np->in_inip) &&
			    ((np->in_redir & NAT_MAP) ||
			     (np->in_pnext == sport))) {
				if (*np->in_plabel && !ap_ok(ip, tcp, np))
					continue;
				/*
				 * If it's a redirection, then we don't want to
				 * create new outgoing port stuff.
				 * Redirections are only for incoming
				 * connections.
				 */
				if (!(np->in_redir & NAT_MAP))
					continue;
				if ((nat = nat_new(np, ip, fin, nflags,
						    NAT_OUTBOUND)))
#ifdef	IPFILTER_LOG
					nat_log(nat, (u_short)np->in_redir);
#else
					;
#endif
				break;
			}

	if (nat) {
			if (!nat->nat_frag && fin->fin_fi.fi_fl & FI_FRAG)
				ipfr_nat_newfrag(ip, fin, 0, nat);
			nat->nat_age = fr_defnatage;
			ip->ip_src = nat->nat_outip;
			nat->nat_bytes += ip->ip_len;
			nat->nat_pkts++;

			/*
			 * Fix up checksums, not by recalculating them, but
			 * simply computing adjustments.
			 */
#if SOLARIS
			if (nat->nat_dir == NAT_OUTBOUND)
				fix_outcksum(&ip->ip_sum, nat->nat_ipsumd);
			else
				fix_incksum(&ip->ip_sum, nat->nat_ipsumd);
#endif

			if (nflags && !(ip->ip_off & 0x1fff) &&
			    !(fin->fin_fi.fi_fl & FI_SHORT)) {

				if (nat->nat_outport)
					tcp->th_sport = nat->nat_outport;

				if (ip->ip_p == IPPROTO_TCP) {
					csump = &tcp->th_sum;
					fr_tcp_age(&nat->nat_age,
						   nat->nat_state, ip, fin,1);
					/*
					 * Increase this because we may have
					 * "keep state" following this too and
					 * packet storms can occur if this is
					 * removed too quickly.
					 */
					if (nat->nat_age == fr_tcpclosed)
						nat->nat_age = fr_tcplastack;
				} else if (ip->ip_p == IPPROTO_UDP) {
					udphdr_t *udp = (udphdr_t *)tcp;

					if (udp->uh_sum)
						csump = &udp->uh_sum;
				} else if (ip->ip_p == IPPROTO_ICMP) {
					icmphdr_t *ic = (icmphdr_t *)tcp;

					csump = &ic->icmp_cksum;
				}
				if (csump) {
					if (nat->nat_dir == NAT_OUTBOUND)
						fix_outcksum(csump,
							     nat->nat_sumd);
					else
						fix_incksum(csump,
							    nat->nat_sumd);
				}
			}
			(void) ap_check(ip, tcp, fin, nat);
			nat_stats.ns_mapped[1]++;
			MUTEX_EXIT(&ipf_nat);
			return 1;
		}
	MUTEX_EXIT(&ipf_nat);
	return 0;
}


/*
 * Packets coming in from the external interface go through this.
 * Here, the destination address requires alteration, if anything.
 */
int ip_natin(ip, hlen, fin)
ip_t *ip;
int hlen;
fr_info_t *fin;
{
	register ipnat_t *np;
	register struct in_addr in;
	struct ifnet *ifp = fin->fin_ifp;
	tcphdr_t *tcp = NULL;
	u_short sport = 0, dport = 0, nflags = 0, *csump = NULL;
	nat_t *nat;

	if (!(ip->ip_off & 0x1fff) && !(fin->fin_fi.fi_fl & FI_SHORT)) {
		if (ip->ip_p == IPPROTO_TCP)
			nflags = IPN_TCP;
		else if (ip->ip_p == IPPROTO_UDP)
			nflags = IPN_UDP;
		if (nflags) {
			tcp = (tcphdr_t *)((char *)ip + hlen);
			dport = tcp->th_dport;
			sport = tcp->th_sport;
		}
	}

	in = ip->ip_dst;

	MUTEX_ENTER(&ipf_nat);

	if ((nat = ipfr_nat_knownfrag(ip, fin)))
		;
	else if ((nat = nat_inlookup(fin->fin_ifp, nflags, ip->ip_src, sport,
				     ip->ip_dst, dport)))
		np = nat->nat_ptr;
	else
		/*
		 * If there is no current entry in the nat table for this IP#,
		 * create one for it (if there is a matching rule).
		 */
		for (np = nat_list; np; np = np->in_next)
			if ((np->in_ifp == ifp) &&
			    (!np->in_flags || (nflags & np->in_flags)) &&
			    ((in.s_addr & np->in_outmsk) == np->in_outip) &&
			    (np->in_redir & NAT_REDIRECT ||
			     np->in_pmin == dport)) {
				/*
				 * If this rule (np) is a redirection, rather
				 * than a mapping, then do a nat_new.
				 * Otherwise, if it's just a mapping, do a
				 * continue;
				 */
				if (!(np->in_redir & NAT_REDIRECT))
					continue;
				if ((nat = nat_new(np, ip, fin, nflags,
						    NAT_INBOUND)))
#ifdef	IPFILTER_LOG
					nat_log(nat, (u_short)np->in_redir);
#else
					;
#endif
				break;
			}
	if (nat) {
			if (!nat->nat_frag && fin->fin_fi.fi_fl & FI_FRAG)
				ipfr_nat_newfrag(ip, fin, 0, nat);
			(void) ap_check(ip, tcp, fin, nat);
			nat->nat_age = fr_defnatage;
			ip->ip_dst = nat->nat_inip;
			nat->nat_bytes += ip->ip_len;
			nat->nat_pkts++;

			/*
			 * Fix up checksums, not by recalculating them, but
			 * simply computing adjustments.
			 */
#if SOLARIS
			if (nat->nat_dir == NAT_OUTBOUND)
				fix_incksum(&ip->ip_sum, nat->nat_ipsumd);
			else
				fix_outcksum(&ip->ip_sum, nat->nat_ipsumd);
#endif
			if (nflags && !(ip->ip_off & 0x1fff) &&
			    !(fin->fin_fi.fi_fl & FI_SHORT)) {

				if (nat->nat_inport)
					tcp->th_dport = nat->nat_inport;

				if (ip->ip_p == IPPROTO_TCP) {
					csump = &tcp->th_sum;
					fr_tcp_age(&nat->nat_age,
						   nat->nat_state, ip, fin,0);
					/*
					 * Increase this because we may have
					 * "keep state" following this too and
					 * packet storms can occur if this is
					 * removed too quickly.
					 */
					if (nat->nat_age == fr_tcpclosed)
						nat->nat_age = fr_tcplastack;
				} else if (ip->ip_p == IPPROTO_UDP) {
					udphdr_t *udp = (udphdr_t *)tcp;

					if (udp->uh_sum)
						csump = &udp->uh_sum;
				} else if (ip->ip_p == IPPROTO_ICMP) {
					icmphdr_t *ic = (icmphdr_t *)tcp;

					csump = &ic->icmp_cksum;
				}
				if (csump) {
					if (nat->nat_dir == NAT_OUTBOUND)
						fix_incksum(csump,
							    nat->nat_sumd);
					else
						fix_outcksum(csump,
							     nat->nat_sumd);
				}
			}
			nat_stats.ns_mapped[0]++;
			MUTEX_EXIT(&ipf_nat);
			return 1;
		}
	MUTEX_EXIT(&ipf_nat);
	return 0;
}


/*
 * Free all memory used by NAT structures allocated at runtime.
 */
void ip_natunload()
{
	int s;

	MUTEX_ENTER(&ipf_nat);
	SPLNET(s);
	(void) clear_natlist();
	(void) flush_nattable();
	(void) ap_unload();
	SPLX(s)
	MUTEX_EXIT(&ipf_nat);
}


/*
 * Slowly expire held state for NAT entries.  Timeouts are set in
 * expectation of this being called twice per second.
 */
void ip_natexpire()
{
	register struct nat *nat, **natp;
	int s;

	MUTEX_ENTER(&ipf_nat);
	SPLNET(s);
	for (natp = &nat_instances; (nat = *natp); ) {
		if (--nat->nat_age) {
			natp = &nat->nat_next;
			continue;
		}
		*natp = nat->nat_next;
#ifdef	IPFILTER_LOG
		nat_log(nat, NL_EXPIRE);
#endif
		nat_delete(nat);
		nat_stats.ns_expire++;
	}
	SPLX(s);
	MUTEX_EXIT(&ipf_nat);
}


#ifdef	IPFILTER_LOG
void nat_log(nat, type)
struct nat *nat;
u_short type;
{
	struct	ipnat	*np;
	struct	natlog	natl;
	int	rulen;

	if (iplused[IPL_LOGNAT] + sizeof(natl) > IPLLOGSIZE) {
		nat_stats.ns_logfail++;
		return;
	}

        if (iplh[IPL_LOGNAT] == iplbuf[IPL_LOGNAT] + IPLLOGSIZE)
                iplh[IPL_LOGNAT] = iplbuf[IPL_LOGNAT];

# ifdef	sun
	uniqtime(&natl);
# endif
# if BSD >= 199306 || defined(__FreeBSD__)
	microtime((struct timeval *)&natl);
# endif
	natl.nl_inip = nat->nat_inip;
	natl.nl_outip = nat->nat_outip;
	natl.nl_origip = nat->nat_oip;
	natl.nl_bytes = nat->nat_bytes;
	natl.nl_pkts = nat->nat_pkts;
	natl.nl_origport = nat->nat_oport;
	natl.nl_inport = nat->nat_inport;
	natl.nl_outport = nat->nat_outport;
	natl.nl_type = type;
	natl.nl_rule = -1;
	if (nat->nat_ptr) {
		for (rulen = 0, np = nat_list; np; np = np->in_next, rulen++)
			if (np == nat->nat_ptr) {
				natl.nl_rule = rulen;
				break;
			}
	}

	if (!fr_copytolog(IPL_LOGNAT, (char *)&natl, sizeof(natl))) {
		iplused[IPL_LOGNAT] += sizeof(natl);
		nat_stats.ns_logged++;
	} else
		nat_stats.ns_logfail++;
	wakeup(iplbuf[IPL_LOGNAT]);
}
#endif
