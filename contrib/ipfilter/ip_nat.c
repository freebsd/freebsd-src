/*
 * Copyright (C) 1995-1998 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * Added redirect stuff and a LOT of bug fixes. (mcn@EnGarde.com)
 */
#if !defined(lint)
static const char sccsid[] = "@(#)ip_nat.c	1.11 6/5/96 (C) 1995 Darren Reed";
static const char rcsid[] = "@(#)$Id: ip_nat.c,v 2.2.2.5 1999/10/05 12:58:33 darrenr Exp $";
#endif

#if defined(__FreeBSD__) && defined(KERNEL) && !defined(_KERNEL)
#define _KERNEL
#endif

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/file.h>
#if defined(__NetBSD__) && (NetBSD >= 199905) && !defined(IPFILTER_LKM) && \
    defined(_KERNEL)
# include "opt_ipfilter_log.h"
#endif
#if !defined(_KERNEL) && !defined(KERNEL)
# include <stdio.h>
# include <string.h>
# include <stdlib.h>
#endif
#if defined(KERNEL) && (__FreeBSD_version >= 220000)
# include <sys/filio.h>
# include <sys/fcntl.h>
#else
# include <sys/ioctl.h>
#endif
#include <sys/fcntl.h>
#include <sys/uio.h>
#ifndef linux
# include <sys/protosw.h>
#endif
#include <sys/socket.h>
#if defined(_KERNEL) && !defined(linux)
# include <sys/systm.h>
#endif
#if !defined(__SVR4) && !defined(__svr4__)
# ifndef linux
#  include <sys/mbuf.h>
# endif
#else
# include <sys/filio.h>
# include <sys/byteorder.h>
# ifdef _KERNEL
#  include <sys/dditypes.h>
# endif
# include <sys/stream.h>
# include <sys/kmem.h>
#endif
#if __FreeBSD_version >= 300000
# include <sys/queue.h>
#endif
#include <net/if.h>
#if __FreeBSD_version >= 300000
# include <net/if_var.h>
# if defined(_KERNEL) && !defined(IPFILTER_LKM)
#  include "opt_ipfilter.h"
# endif
#endif
#ifdef sun
# include <net/af.h>
#endif
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#ifdef __sgi
# ifdef IFF_DRVRLOCK /* IRIX6 */
#include <sys/hashing.h>
#include <netinet/in_var.h>
# endif
#endif

#ifdef RFC1825
# include <vpn/md5.h>
# include <vpn/ipsec.h>
extern struct ifnet vpnif;
#endif

#ifndef linux
# include <netinet/ip_var.h>
#endif
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include "netinet/ip_compat.h"
#include <netinet/tcpip.h>
#include "netinet/ip_fil.h"
#include "netinet/ip_proxy.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_frag.h"
#include "netinet/ip_state.h"
#if (__FreeBSD_version >= 300000)
# include <sys/malloc.h>
#endif
#ifndef	MIN
# define	MIN(a,b)	(((a)<(b))?(a):(b))
#endif
#undef	SOCKADDR_IN
#define	SOCKADDR_IN	struct sockaddr_in

nat_t	**nat_table[2] = { NULL, NULL },
	*nat_instances = NULL;
ipnat_t	*nat_list = NULL;
u_int	ipf_nattable_sz = NAT_TABLE_SZ;
u_int	ipf_natrules_sz = NAT_SIZE;
u_int	ipf_rdrrules_sz = RDR_SIZE;
u_32_t	nat_masks = 0;
u_32_t	rdr_masks = 0;
ipnat_t	**nat_rules = NULL;
ipnat_t	**rdr_rules = NULL;

u_long	fr_defnatage = DEF_NAT_AGE,
	fr_defnaticmpage = 6;		/* 3 seconds */
natstat_t nat_stats;
#if	(SOLARIS || defined(__sgi)) && defined(_KERNEL)
extern	kmutex_t	ipf_rw;
extern	KRWLOCK_T	ipf_nat;
#endif

static	int	nat_flushtable __P((void));
static	int	nat_clearlist __P((void));
static	void	nat_delete __P((struct nat *));
static	void	nat_delrdr __P((struct ipnat *));
static	void	nat_delnat __P((struct ipnat *));


int nat_init()
{
	KMALLOCS(nat_table[0], nat_t **, sizeof(nat_t *) * ipf_nattable_sz);
	if (nat_table[0] != NULL)
		bzero((char *)nat_table[0], ipf_nattable_sz * sizeof(nat_t *));
	else
		return -1;

	KMALLOCS(nat_table[1], nat_t **, sizeof(nat_t *) * ipf_nattable_sz);
	if (nat_table[1] != NULL)
		bzero((char *)nat_table[1], ipf_nattable_sz * sizeof(nat_t *));
	else
		return -1;

	KMALLOCS(nat_rules, ipnat_t **, sizeof(ipnat_t *) * ipf_natrules_sz);
	if (nat_rules != NULL)
		bzero((char *)nat_rules, ipf_natrules_sz * sizeof(ipnat_t *));
	else
		return -1;

	KMALLOCS(rdr_rules, ipnat_t **, sizeof(ipnat_t *) * ipf_rdrrules_sz);
	if (rdr_rules != NULL)
		bzero((char *)rdr_rules, ipf_rdrrules_sz * sizeof(ipnat_t *));
	else
		return -1;
	return 0;
}


void nat_delrdr(n)
ipnat_t *n;
{
	ipnat_t **n1;
	u_32_t iph;
	u_int hv;

	iph = n->in_outip & n->in_outmsk;
	hv = NAT_HASH_FN(iph, ipf_rdrrules_sz);
	for (n1 = &rdr_rules[hv]; *n1 && (*n1 != n); n1 = &(*n1)->in_rnext)
		;
	if (*n1)
		*n1 = n->in_rnext;
}


static void nat_delnat(n)
ipnat_t *n;
{
	ipnat_t **n1;
	u_32_t iph;
	u_int hv;

	iph = n->in_inip & n->in_inmsk;
	hv = NAT_HASH_FN(iph, ipf_natrules_sz);
	for (n1 = &nat_rules[hv]; *n1 && (*n1 != n); n1 = &(*n1)->in_mnext)
		;
	if (*n1)
		*n1 = n->in_mnext;
}


void fix_outcksum(sp, n)
u_short *sp;
u_32_t n;
{
	register u_short sumshort;
	register u_32_t sum1;

	if (!n)
		return;
	sum1 = (~ntohs(*sp)) & 0xffff;
	sum1 += (n);
	sum1 = (sum1 >> 16) + (sum1 & 0xffff);
	/* Again */
	sum1 = (sum1 >> 16) + (sum1 & 0xffff);
	sumshort = ~(u_short)sum1;
	*(sp) = htons(sumshort);
}


void fix_incksum(sp, n)
u_short *sp;
u_32_t n;
{
	register u_short sumshort;
	register u_32_t sum1;

	if (!n)
		return;
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
#if defined(__NetBSD__) || defined(__OpenBSD__)
u_long cmd;
#else
int cmd;
#endif
caddr_t data;
int mode;
{
	register ipnat_t *nat, *nt, *n = NULL, **np = NULL;
	int error = 0, ret, k;
	ipnat_t natd;
	u_32_t i, j;
#if defined(_KERNEL) && !SOLARIS
	int s;
#endif

#if (BSD >= 199306) && defined(_KERNEL)
	if ((securelevel >= 2) && (mode & FWRITE))
		return EPERM;
#endif

	nat = NULL;     /* XXX gcc -Wuninitialized */
	KMALLOC(nt, ipnat_t *);
	if ((cmd == SIOCADNAT) || (cmd == SIOCRMNAT))
		IRCOPY(data, (char *)&natd, sizeof(natd));

	/*
	 * For add/delete, look to see if the NAT entry is already present
	 */
	SPL_NET(s);
	WRITE_ENTER(&ipf_nat);
	if ((cmd == SIOCADNAT) || (cmd == SIOCRMNAT)) {
		nat = &natd;
		nat->in_flags &= IPN_USERFLAGS;
		if ((nat->in_redir & NAT_MAPBLK) == 0) {
			nat->in_inip &= nat->in_inmsk;
			if ((nat->in_flags & IPN_RANGE) == 0)
				nat->in_outip &= nat->in_outmsk;
		}
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
		if (nt == NULL) {
			error = ENOMEM;
			break;
		}
		n = nt;
		nt = NULL;
		bcopy((char *)nat, (char *)n, sizeof(*n));
		n->in_ifp = (void *)GETUNIT(n->in_ifname);
		if (!n->in_ifp)
			n->in_ifp = (void *)-1;
		if (n->in_plabel[0] != '\0') {
			n->in_apr = appr_match(n->in_p, n->in_plabel);
			if (!n->in_apr) {
				error = ENOENT;
				break;
			}
		}
		n->in_next = NULL;
		*np = n;

		if (n->in_redir & NAT_REDIRECT) {
			u_int hv;

			k = countbits(n->in_outmsk);
			if ((k >= 0) && (k != 32))
				rdr_masks |= 1 << k;
			j = (n->in_outip & n->in_outmsk);
			hv = NAT_HASH_FN(j, ipf_rdrrules_sz);
			np = rdr_rules + hv;
			while (*np != NULL)
				np = &(*np)->in_rnext;
			n->in_rnext = NULL;
			*np = n;
		}
		if (n->in_redir & (NAT_MAP|NAT_MAPBLK)) {
			u_int hv;

			k = countbits(n->in_inmsk);
			if ((k >= 0) && (k != 32))
				nat_masks |= 1 << k;
			j = (n->in_inip & n->in_inmsk);
			hv = NAT_HASH_FN(j, ipf_natrules_sz);
			np = nat_rules + hv;
			while (*np != NULL)
				np = &(*np)->in_mnext;
			n->in_mnext = NULL;
			*np = n;
		}

		n->in_use = 0;
		if (n->in_redir & NAT_MAPBLK)
			n->in_space = USABLE_PORTS * ~ntohl(n->in_outmsk);
		else if (n->in_flags & IPN_AUTOPORTMAP)
			n->in_space = USABLE_PORTS * ~ntohl(n->in_inmsk);
		else if (n->in_flags & IPN_RANGE)
			n->in_space = ntohl(n->in_outmsk) - ntohl(n->in_outip);
		else
			n->in_space = ~ntohl(n->in_outmsk);
		/*
		 * Calculate the number of valid IP addresses in the output
		 * mapping range.  In all cases, the range is inclusive of
		 * the start and ending IP addresses.
		 * If to a CIDR address, lose 2: broadcast + network address
		 *                               (so subtract 1)
		 * If to a range, add one.
		 * If to a single IP address, set to 1.
		 */
		if (n->in_space) {
			if ((n->in_flags & IPN_RANGE) != 0)
				n->in_space += 1;
			else
				n->in_space -= 1;
		} else
			n->in_space = 1;
		if ((n->in_outmsk != 0xffffffff) && (n->in_outmsk != 0) &&
		    ((n->in_flags & IPN_RANGE) == 0))
			n->in_nip = ntohl(n->in_outip) + 1;
		else
			n->in_nip = ntohl(n->in_outip);
		if (n->in_redir & NAT_MAP) {
			n->in_pnext = ntohs(n->in_pmin);
			/*
			 * Multiply by the number of ports made available.
			 */
			if (ntohs(n->in_pmax) >= ntohs(n->in_pmin)) {
				n->in_space *= (ntohs(n->in_pmax) -
						ntohs(n->in_pmin) + 1);
				/*
				 * Because two different sources can map to
				 * different destinations but use the same
				 * local IP#/port #.
				 * If the result is smaller than in_space, then
				 * we may have wrapped around 32bits.
				 */
				i = n->in_inmsk;
				if ((i != 0) && (i != 0xffffffff)) {
					j = n->in_space * (~ntohl(i) + 1);
					if (j >= n->in_space)
						n->in_space = j;
					else
						n->in_space = 0xffffffff;
				}
			}
			/*
			 * If no protocol is specified, multiple by 256.
			 */
			if ((n->in_flags & IPN_TCPUDP) == 0) {
					j = n->in_space * 256;
					if (j >= n->in_space)
						n->in_space = j;
					else
						n->in_space = 0xffffffff;
			}
		}
		/* Otherwise, these fields are preset */
		n = NULL;
		nat_stats.ns_rules++;
		break;
	case SIOCRMNAT :
		if (!(mode & FWRITE)) {
			error = EPERM;
			n = NULL;
			break;
		}
		if (!n) {
			error = ESRCH;
			break;
		}
		if (n->in_redir & NAT_REDIRECT)
			nat_delrdr(n);
		if (n->in_redir & (NAT_MAPBLK|NAT_MAP))
			nat_delnat(n);
		if (nat_list == NULL) {
			nat_masks = 0;
			rdr_masks = 0;
		}
		*np = n->in_next;
		if (!n->in_use) {
			if (n->in_apr)
				appr_free(n->in_apr);
			KFREE(n);
			nat_stats.ns_rules--;
		} else {
			n->in_flags |= IPN_DELETE;
			n->in_next = NULL;
		}
		n = NULL;
		break;
	case SIOCGNATS :
		MUTEX_DOWNGRADE(&ipf_nat);
		nat_stats.ns_table[0] = nat_table[0];
		nat_stats.ns_table[1] = nat_table[1];
		nat_stats.ns_list = nat_list;
		nat_stats.ns_nattab_sz = ipf_nattable_sz;
		nat_stats.ns_rultab_sz = ipf_natrules_sz;
		nat_stats.ns_rdrtab_sz = ipf_rdrrules_sz;
		nat_stats.ns_instances = nat_instances;
		nat_stats.ns_apslist = ap_sess_list;
		IWCOPY((char *)&nat_stats, (char *)data, sizeof(nat_stats));
		break;
	case SIOCGNATL :
	    {
		natlookup_t nl;

		MUTEX_DOWNGRADE(&ipf_nat);
		IRCOPY((char *)data, (char *)&nl, sizeof(nl));

		if (nat_lookupredir(&nl)) {
			IWCOPY((char *)&nl, (char *)data, sizeof(nl));
		} else
			error = ESRCH;
		break;
	    }
	case SIOCFLNAT :
		if (!(mode & FWRITE)) {
			error = EPERM;
			break;
		}
		ret = nat_flushtable();
		MUTEX_DOWNGRADE(&ipf_nat);
		IWCOPY((caddr_t)&ret, data, sizeof(ret));
		break;
	case SIOCCNATL :
		if (!(mode & FWRITE)) {
			error = EPERM;
			break;
		}
		ret = nat_clearlist();
		MUTEX_DOWNGRADE(&ipf_nat);
		IWCOPY((caddr_t)&ret, data, sizeof(ret));
		break;
	case FIONREAD :
#ifdef	IPFILTER_LOG
		MUTEX_DOWNGRADE(&ipf_nat);
		IWCOPY((caddr_t)&iplused[IPL_LOGNAT], (caddr_t)data,
		       sizeof(iplused[IPL_LOGNAT]));
#endif
		break;
	default :
		error = EINVAL;
		break;
	}
	RWLOCK_EXIT(&ipf_nat);			/* READ/WRITE */
	SPL_X(s);
	if (nt)
		KFREE(nt);
	return error;
}


/*
 * Delete a nat entry from the various lists and table.
 */
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

	if (natd->nat_fr != NULL) {
		ATOMIC_DEC(natd->nat_fr->fr_ref);
	}
	/*
	 * If there is an active reference from the nat entry to its parent
	 * rule, decrement the rule's reference count and free it too if no
	 * longer being used.
	 */
	ipn = natd->nat_ptr;
	if (ipn != NULL) {
		ipn->in_space++;
		ipn->in_use--;
		if (!ipn->in_use && (ipn->in_flags & IPN_DELETE)) {
			if (ipn->in_apr)
				appr_free(ipn->in_apr);
			KFREE(ipn);
			nat_stats.ns_rules--;
		}
	}

	/*
	 * If there's a fragment table entry too for this nat entry, then
	 * dereference that as well.
	 */
	ipfr_forget((void *)natd);
	aps_free(natd->nat_aps);
	nat_stats.ns_inuse--;
	KFREE(natd);
}


/*
 * nat_flushtable - clear the NAT table of all mapping entries.
 */
static int nat_flushtable()
{
	register nat_t *nat, **natp;
	register int j = 0;
  
	/*
	 * ALL NAT mappings deleted, so lets just make the deletions
	 * quicker.
	 */
	if (nat_table[0] != NULL)
		bzero((char *)nat_table[0],
		      sizeof(nat_table[0]) * ipf_nattable_sz);
	if (nat_table[1] != NULL)
		bzero((char *)nat_table[1],
		      sizeof(nat_table[1]) * ipf_nattable_sz);

	for (natp = &nat_instances; (nat = *natp); ) {
		*natp = nat->nat_next;
		nat_delete(nat);
		j++;
	}
	nat_stats.ns_inuse = 0;
	return j;
}


/*
 * nat_clearlist - delete all rules in the active NAT mapping list.
 */
static int nat_clearlist()
{
	register ipnat_t *n, **np = &nat_list;
	int i = 0;

	if (nat_rules != NULL)
		bzero((char *)nat_rules, sizeof(*nat_rules) * ipf_natrules_sz);
	if (rdr_rules != NULL)
		bzero((char *)rdr_rules, sizeof(*rdr_rules) * ipf_rdrrules_sz);

	while ((n = *np)) {
		*np = n->in_next;
		if (!n->in_use) {
			if (n->in_apr)
				appr_free(n->in_apr);
			KFREE(n);
			nat_stats.ns_rules--;
		} else {
			n->in_flags |= IPN_DELETE;
			n->in_next = NULL;
		}
		i++;
	}
	nat_masks = 0;
	rdr_masks = 0;
	return i;
}


/*
 * Create a new NAT table entry.
 * NOTE: assumes write lock on ipf_nat has been obtained already.
 */
nat_t *nat_new(np, ip, fin, flags, direction)
ipnat_t *np;
ip_t *ip;
fr_info_t *fin;
u_int flags;
int direction;
{
	register u_32_t sum1, sum2, sumd, l;
	u_short port = 0, sport = 0, dport = 0, nport = 0;
	nat_t *nat, **natp, *natl = NULL;
	struct in_addr in, inb;
	tcphdr_t *tcp = NULL;
	u_short nflags;
	u_int hv;

	nflags = flags & np->in_flags;
	if (flags & IPN_TCPUDP) {
		tcp = (tcphdr_t *)fin->fin_dp;
		sport = tcp->th_sport;
		dport = tcp->th_dport;
	}

	/* Give me a new nat */
	KMALLOC(nat, nat_t *);
	if (nat == NULL)
		return NULL;

	bzero((char *)nat, sizeof(*nat));
	nat->nat_flags = flags;
	/*
	 * Search the current table for a match.
	 */
	if (direction == NAT_OUTBOUND) {
		/*
		 * Values at which the search for a free resouce starts.
		 */
		u_32_t st_ip;
		u_short st_port;

		/*
		 * If it's an outbound packet which doesn't match any existing
		 * record, then create a new port
		 */
		l = 0;
		st_ip = np->in_nip;
		st_port = np->in_pnext;

		do {
			port = 0;
			in.s_addr = np->in_nip;
			if (l == 0) {
				natl = nat_maplookup(fin->fin_ifp, flags,
						     ip->ip_src, ip->ip_dst);
				if (natl != NULL) {
					in = natl->nat_outip;
#ifndef sparc
					in.s_addr = ntohl(in.s_addr);
#endif
				}
			}

			if ((np->in_outmsk == 0xffffffff) &&
			    (np->in_pnext == 0)) {
				if (l > 0) {
					KFREE(nat);
					return NULL;
				}
			}

			if (np->in_redir & NAT_MAPBLK) {
				if ((l >= np->in_ppip) || ((l > 0) &&
				     !(flags & IPN_TCPUDP))) {
					KFREE(nat);
					return NULL;
				}
				/*
				 * map-block - Calculate destination address.
				 */
				in.s_addr = ntohl(ip->ip_src.s_addr);
				in.s_addr &= ntohl(~np->in_inmsk);
				inb.s_addr = in.s_addr;
				in.s_addr /= np->in_ippip;
				in.s_addr &= ntohl(~np->in_outmsk);
				in.s_addr += ntohl(np->in_outip);
				/*
				 * Calculate destination port.
				 */
				if ((flags & IPN_TCPUDP) &&
				    (np->in_ppip != 0)) {
					port = ntohs(sport) + l;
					port %= np->in_ppip;
					port += np->in_ppip *
						(inb.s_addr % np->in_ippip);
					port += MAPBLK_MINPORT;
					port = htons(port);
				}
			} else if (!in.s_addr &&
				   (np->in_outmsk == 0xffffffff)) {
				/*
				 * 0/32 - use the interface's IP address.
				 */
				if ((l > 0) ||
				    fr_ifpaddr(fin->fin_ifp, &in) == -1) {
					KFREE(nat);
					return NULL;
				}
			} else if (!in.s_addr && !np->in_outmsk) {
				/*
				 * 0/0 - use the original source address/port.
				 */
				if (l > 0) {
					KFREE(nat);
					return NULL;
				}
				in.s_addr = ntohl(ip->ip_src.s_addr);
			} else if ((np->in_outmsk != 0xffffffff) &&
				   (np->in_pnext == 0) &&
				   ((l > 0) || (natl == NULL)))
				np->in_nip++;
			natl = NULL;

			if ((nflags & IPN_TCPUDP) &&
			    ((np->in_redir & NAT_MAPBLK) == 0) &&
			    (np->in_flags & IPN_AUTOPORTMAP)) {
				if ((l > 0) && (l % np->in_ppip == 0)) {
					if (l > np->in_space) {
						KFREE(nat);
						return NULL;
					} else if ((l > np->in_ppip) &&
						   np->in_outmsk != 0xffffffff)
						np->in_nip++;
				}
				if (np->in_ppip != 0) {
					port = ntohs(sport);
					port += (l % np->in_ppip);
					port %= np->in_ppip;
					port += np->in_ppip *
						(ntohl(ip->ip_src.s_addr) %
						 np->in_ippip);
					port += MAPBLK_MINPORT;
					port = htons(port);
				}
			} else if (((np->in_redir & NAT_MAPBLK) == 0) &&
				   (nflags & IPN_TCPUDP) &&
				   (np->in_pnext != 0)) {
				port = htons(np->in_pnext++);
				if (np->in_pnext > ntohs(np->in_pmax)) {
					np->in_pnext = ntohs(np->in_pmin);
					if (np->in_outmsk != 0xffffffff)
						np->in_nip++;
				}
			}

			if (np->in_flags & IPN_RANGE) {
				if (np->in_nip >= ntohl(np->in_outmsk))
					np->in_nip = ntohl(np->in_outip);
			} else {
				if ((np->in_outmsk != 0xffffffff) &&
				    ((np->in_nip + 1) & ntohl(np->in_outmsk)) >
				    ntohl(np->in_outip))
					np->in_nip = ntohl(np->in_outip) + 1;
			}

			if (!port && (flags & IPN_TCPUDP))
				port = sport;

			/*
			 * Here we do a lookup of the connection as seen from
			 * the outside.  If an IP# pair already exists, try
			 * again.  So if you have A->B becomes C->B, you can
			 * also have D->E become C->E but not D->B causing
			 * another C->B.  Also take protocol and ports into
			 * account when determining whether a pre-existing
			 * NAT setup will cause an external conflict where
			 * this is appropriate.
			 */
			inb.s_addr = htonl(in.s_addr);
			natl = nat_inlookup(fin->fin_ifp, flags,
					    (u_int)ip->ip_p, ip->ip_dst, inb,
					    (port << 16) | dport);

			/*
			 * Has the search wrapped around and come back to the
			 * start ?
			 */
			if ((natl != NULL) &&
			    (np->in_pnext != 0) && (st_port == np->in_pnext) &&
			    (np->in_nip != 0) && (st_ip == np->in_nip)) {
				KFREE(nat);
				return NULL;
			}
			l++;
		} while (natl != NULL);

		if (np->in_space > 0)
			np->in_space--;

		/* Setup the NAT table */
		nat->nat_inip = ip->ip_src;
		nat->nat_outip.s_addr = htonl(in.s_addr);
		nat->nat_oip = ip->ip_dst;

		sum1 = LONG_SUM(ntohl(ip->ip_src.s_addr)) + ntohs(sport);
		sum2 = LONG_SUM(in.s_addr) + ntohs(port);

		if (flags & IPN_TCPUDP) {
			nat->nat_inport = sport;
			nat->nat_outport = port;	/* sport */
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

		/*
		 * When the redirect-to address is set to 0.0.0.0, just
		 * assume a blank `forwarding' of the packet.  We don't
		 * setup any translation for this either.
		 */
		if ((in.s_addr == 0) && (nport == dport)) {
			KFREE(nat);
			return NULL;
		}

		nat->nat_inip.s_addr = htonl(in.s_addr);
		nat->nat_outip = ip->ip_dst;
		nat->nat_oip = ip->ip_src;

		sum1 = LONG_SUM(ntohl(ip->ip_dst.s_addr)) + ntohs(dport);
		sum2 = LONG_SUM(in.s_addr) + ntohs(nport);

		if (flags & IPN_TCPUDP) {
			nat->nat_inport = nport;
			nat->nat_outport = dport;
			nat->nat_oport = sport;
		}
	}

	CALC_SUMD(sum1, sum2, sumd);
	nat->nat_sumd = (sumd & 0xffff) + (sumd >> 16);

	if ((flags & IPN_TCPUDP) && ((sport != port) || (dport != nport))) {
		if (direction == NAT_OUTBOUND)
			sum1 = LONG_SUM(ntohl(ip->ip_src.s_addr));
		else
			sum1 = LONG_SUM(ntohl(ip->ip_dst.s_addr));

		sum2 = LONG_SUM(in.s_addr);

		CALC_SUMD(sum1, sum2, sumd);
		nat->nat_ipsumd = (sumd & 0xffff) + (sumd >> 16);
	} else
		nat->nat_ipsumd = nat->nat_sumd;

	in.s_addr = htonl(in.s_addr);
	nat->nat_next = nat_instances;
	nat_instances = nat;
	hv = NAT_HASH_FN(nat->nat_inip.s_addr, ipf_nattable_sz);
	natp = &nat_table[0][hv];
	nat->nat_hstart[0] = natp;
	nat->nat_hnext[0] = *natp;
	*natp = nat;
	hv = NAT_HASH_FN(nat->nat_outip.s_addr, ipf_nattable_sz);
	natp = &nat_table[1][hv];
	nat->nat_hstart[1] = natp;
	nat->nat_hnext[1] = *natp;
	*natp = nat;
	nat->nat_dir = direction;
	nat->nat_ifp = fin->fin_ifp;
	nat->nat_ptr = np;
	nat->nat_p = ip->ip_p;
	nat->nat_bytes = 0;
	nat->nat_pkts = 0;
	nat->nat_age = fr_defnatage;
	nat->nat_fr = fin->fin_fr;
	if (nat->nat_fr != NULL) {
		ATOMIC_INC(nat->nat_fr->fr_ref);
	}
	if (direction == NAT_OUTBOUND) {
		if (flags & IPN_TCPUDP)
			tcp->th_sport = port;
	} else {
		if (flags & IPN_TCPUDP)
			tcp->th_dport = nport;
	}
	nat_stats.ns_added++;
	nat_stats.ns_inuse++;
	np->in_use++;
	return nat;
}


nat_t *nat_icmpinlookup(ip, fin)
ip_t *ip;
fr_info_t *fin;
{
	icmphdr_t *icmp;
	tcphdr_t *tcp = NULL;
	ip_t *oip;
	int flags = 0, type;

	icmp = (icmphdr_t *)fin->fin_dp;
	/*
	 * Does it at least have the return (basic) IP header ?
	 * Only a basic IP header (no options) should be with an ICMP error
	 * header.
	 */
	if ((ip->ip_hl != 5) || (ip->ip_len < ICMPERR_MINPKTLEN))
		return NULL;
	type = icmp->icmp_type;
	/*
	 * If it's not an error type, then return.
	 */
	if ((type != ICMP_UNREACH) && (type != ICMP_SOURCEQUENCH) &&
	    (type != ICMP_REDIRECT) && (type != ICMP_TIMXCEED) &&
	    (type != ICMP_PARAMPROB))
		return NULL;

	oip = (ip_t *)((char *)fin->fin_dp + 8);
	if (ip->ip_len < ICMPERR_MAXPKTLEN + ((oip->ip_hl - 5) << 2))
		return NULL;
	if (oip->ip_p == IPPROTO_TCP)
		flags = IPN_TCP;
	else if (oip->ip_p == IPPROTO_UDP)
		flags = IPN_UDP;
	if (flags & IPN_TCPUDP) {
		tcp = (tcphdr_t *)((char *)oip + (oip->ip_hl << 2));
		return nat_inlookup(fin->fin_ifp, flags, (u_int)oip->ip_p,
				    oip->ip_dst, oip->ip_src,
				    (tcp->th_sport << 16) | tcp->th_dport);
	}
	return nat_inlookup(fin->fin_ifp, 0, (u_int)oip->ip_p, oip->ip_dst,
			    oip->ip_src, 0);
}


/*
 * This should *ONLY* be used for incoming packets to make sure a NAT'd ICMP
 * packet gets correctly recognised.
 */
nat_t *nat_icmpin(ip, fin, nflags)
ip_t *ip;
fr_info_t *fin;
u_int *nflags;
{
	u_32_t sum1, sum2, sumd;
	struct in_addr in;
	icmphdr_t *icmp;
	nat_t *nat;
	ip_t *oip;
	int flags = 0;

	if (!(nat = nat_icmpinlookup(ip, fin)))
		return NULL;
	*nflags = IPN_ICMPERR;
	icmp = (icmphdr_t *)fin->fin_dp;
	oip = (ip_t *)&icmp->icmp_ip;
	if (oip->ip_p == IPPROTO_TCP)
		flags = IPN_TCP;
	else if (oip->ip_p == IPPROTO_UDP)
		flags = IPN_UDP;
	/*
	 * Need to adjust ICMP header to include the real IP#'s and
	 * port #'s.  Only apply a checksum change relative to the
	 * IP address change is it will be modified again in ip_natout
	 * for both address and port.  Two checksum changes are
	 * necessary for the two header address changes.  Be careful
	 * to only modify the checksum once for the port # and twice
	 * for the IP#.
	 */
	if (nat->nat_dir == NAT_OUTBOUND) {
		sum1 = LONG_SUM(ntohl(oip->ip_src.s_addr));
		in = nat->nat_inip;
		oip->ip_src = in;
	} else {
		sum1 = LONG_SUM(ntohl(oip->ip_dst.s_addr));
		in = nat->nat_outip;
		oip->ip_dst = in;
	}

	sum2 = LONG_SUM(ntohl(in.s_addr));

	CALC_SUMD(sum1, sum2, sumd);

	if (nat->nat_dir == NAT_OUTBOUND) {
		fix_incksum(&oip->ip_sum, sumd);

		sumd += (sumd & 0xffff);
		while (sumd > 0xffff)
			sumd = (sumd & 0xffff) + (sumd >> 16);
		fix_outcksum(&icmp->icmp_cksum, sumd);
	} else {
		fix_outcksum(&oip->ip_sum, sumd);

		sumd += (sumd & 0xffff);
		while (sumd > 0xffff)
			sumd = (sumd & 0xffff) + (sumd >> 16);
		fix_incksum(&icmp->icmp_cksum, sumd);
	}


	if ((flags & IPN_TCPUDP) != 0) {
		tcphdr_t *tcp;

		/* XXX - what if this is bogus hl and we go off the end ? */
		tcp = (tcphdr_t *)((((char *)oip) + (oip->ip_hl << 2)));

		if (nat->nat_dir == NAT_OUTBOUND) {
			if (tcp->th_sport != nat->nat_inport) {
				sum1 = ntohs(tcp->th_sport);
				sum2 = ntohs(nat->nat_inport);
				CALC_SUMD(sum1, sum2, sumd);
				tcp->th_sport = nat->nat_inport;
				fix_outcksum(&icmp->icmp_cksum, sumd);
			}
		} else {
			if (tcp->th_dport != nat->nat_outport) {
				sum1 = ntohs(tcp->th_dport);
				sum2 = ntohs(nat->nat_outport);
				CALC_SUMD(sum1, sum2, sumd);
				tcp->th_dport = nat->nat_outport;
				fix_incksum(&icmp->icmp_cksum, sumd);
			}
		}
	}
	nat->nat_age = fr_defnaticmpage;
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
nat_t *nat_inlookup(ifp, flags, p, src, mapdst, ports)
void *ifp;
register u_int flags, p;
struct in_addr src , mapdst;
u_32_t ports;
{
	register u_short sport, mapdport;
	register nat_t *nat;
	register int nflags;
	u_int hv;

	mapdport = ports >> 16;
	sport = ports & 0xffff;
	flags &= IPN_TCPUDP;

	hv = NAT_HASH_FN(mapdst.s_addr, ipf_nattable_sz);
	nat = nat_table[1][hv];
	for (; nat; nat = nat->nat_hnext[1]) {
		nflags = nat->nat_flags;
		if ((!ifp || ifp == nat->nat_ifp) &&
		    nat->nat_oip.s_addr == src.s_addr &&
		    nat->nat_outip.s_addr == mapdst.s_addr &&
		    (((p == 0) && (flags == (nat->nat_flags & IPN_TCPUDP)))
		     || (p == nat->nat_p)) && (!flags ||
		     (((nat->nat_oport == sport) || (nflags & FI_W_DPORT)) &&
		      ((nat->nat_outport == mapdport) ||
		       (nflags & FI_W_SPORT)))))
			return nat;
	}
	return NULL;
}


/*
 * Lookup a nat entry based on the source 'real' ip address/port and
 * destination address/port.  We use this lookup when sending a packet out,
 * we're looking for a table entry, based on the source address.
 * NOTE: THE PACKET BEING CHECKED (IF FOUND) HAS A MAPPING ALREADY.
 */
nat_t *nat_outlookup(ifp, flags, p, src, dst, ports)
void *ifp;
register u_int flags, p;
struct in_addr src , dst;
u_32_t ports;
{
	register u_short sport, dport;
	register nat_t *nat;
	register int nflags;
	u_int hv;

	sport = ports & 0xffff;
	dport = ports >> 16;
	flags &= IPN_TCPUDP;

	hv = NAT_HASH_FN(src.s_addr, ipf_nattable_sz);
	nat = nat_table[0][hv];
	for (; nat; nat = nat->nat_hnext[0]) {
		nflags = nat->nat_flags;

		if ((!ifp || ifp == nat->nat_ifp) &&
		    nat->nat_inip.s_addr == src.s_addr &&
		    nat->nat_oip.s_addr == dst.s_addr &&
		    (((p == 0) && (flags == (nat->nat_flags & IPN_TCPUDP)))
		     || (p == nat->nat_p)) && (!flags ||
		     ((nat->nat_inport == sport || nflags & FI_W_SPORT) &&
		      (nat->nat_oport == dport || nflags & FI_W_DPORT))))
			return nat;
	}
	return NULL;
}


/*
 * check if an ip address has already been allocated for a given mapping that
 * is not doing port based translation.
 */
nat_t *nat_maplookup(ifp, flags, src, dst)
void *ifp;
register u_int flags;
struct in_addr src , dst;
{
	register nat_t *nat;
	register int oflags;
	u_int hv;

	hv = NAT_HASH_FN(src.s_addr, ipf_nattable_sz);
	nat = nat_table[0][hv];
	for (; nat; nat = nat->nat_hnext[0]) {
		oflags = (flags & IPN_TCPUDP) & nat->nat_ptr->in_flags;
		if (oflags != 0)
			continue;

		if ((!ifp || ifp == nat->nat_ifp) &&
		    nat->nat_inip.s_addr == src.s_addr &&
		    nat->nat_oip.s_addr == dst.s_addr)
			return nat;
	}
	return NULL;
}


/*
 * Lookup the NAT tables to search for a matching redirect
 */
nat_t *nat_lookupredir(np)
register natlookup_t *np;
{
	u_32_t ports;
	nat_t *nat;

	ports = (np->nl_outport << 16) | np->nl_inport;
	/*
	 * If nl_inip is non null, this is a lookup based on the real
	 * ip address. Else, we use the fake.
	 */
	if ((nat = nat_outlookup(NULL, np->nl_flags, 0, np->nl_inip,
				 np->nl_outip, ports))) {
		np->nl_realip = nat->nat_outip;
		np->nl_realport = nat->nat_outport;
	}
	return nat;
}


/*
 * Packets going out on the external interface go through this.
 * Here, the source address requires alteration, if anything.
 */
int ip_natout(ip, fin)
ip_t *ip;
fr_info_t *fin;
{
	register ipnat_t *np = NULL;
	register u_32_t ipa;
	tcphdr_t *tcp = NULL;
	u_short nflags = 0, sport = 0, dport = 0, *csump = NULL;
	struct ifnet *ifp;
	int natadd = 1;
	frentry_t *fr;
	u_int hv, msk;
	u_32_t iph;
	nat_t *nat;
	int i;

	if (nat_list == NULL)
		return 0;

	if ((fr = fin->fin_fr) && !(fr->fr_flags & FR_DUP) &&
	    fr->fr_tif.fd_ifp && fr->fr_tif.fd_ifp != (void *)-1)
		ifp = fr->fr_tif.fd_ifp;
	else
		ifp = fin->fin_ifp;

	if (!(ip->ip_off & IP_OFFMASK) && !(fin->fin_fi.fi_fl & FI_SHORT)) {
		if (ip->ip_p == IPPROTO_TCP)
			nflags = IPN_TCP;
		else if (ip->ip_p == IPPROTO_UDP)
			nflags = IPN_UDP;
		if ((nflags & IPN_TCPUDP)) {
			tcp = (tcphdr_t *)fin->fin_dp;
			sport = tcp->th_sport;
			dport = tcp->th_dport;
		}
	}

	ipa = ip->ip_src.s_addr;

	READ_ENTER(&ipf_nat);
	if ((ip->ip_off & (IP_OFFMASK|IP_MF)) &&
	    (nat = ipfr_nat_knownfrag(ip, fin)))
		natadd = 0;
	else if ((nat = nat_outlookup(ifp, nflags, (u_int)ip->ip_p, ip->ip_src,
				      ip->ip_dst, (dport << 16) | sport))) {
		nflags = nat->nat_flags;
		if ((nflags & (FI_W_SPORT|FI_W_DPORT)) != 0) {
			if ((nflags & FI_W_SPORT) &&
			    (nat->nat_inport != sport))
				nat->nat_inport = sport;
			else if ((nflags & FI_W_DPORT) &&
				 (nat->nat_oport != dport))
				nat->nat_oport = dport;
			if (nat->nat_outport == 0)
				nat->nat_outport = sport;
			nat->nat_flags &= ~(FI_W_DPORT|FI_W_SPORT);
			nflags = nat->nat_flags;
		}
	} else {
		RWLOCK_EXIT(&ipf_nat);
		WRITE_ENTER(&ipf_nat);
		/*
		 * If there is no current entry in the nat table for this IP#,
		 * create one for it (if there is a matching rule).
		 */
		msk = 0xffffffff;
		i = 32;
maskloop:
		iph = ipa & htonl(msk);
		hv = NAT_HASH_FN(iph, ipf_natrules_sz);
		for (np = nat_rules[hv]; np; np = np->in_mnext)
		{
			if ((np->in_ifp == ifp) && np->in_space &&
			    (!(np->in_flags & IPN_RF) ||
			     (np->in_flags & nflags)) &&
			    ((ipa & np->in_inmsk) == np->in_inip) &&
			    ((np->in_redir & (NAT_MAP|NAT_MAPBLK)) ||
			     (np->in_pnext == sport))) {
				if (*np->in_plabel && !appr_ok(ip, tcp, np))
					continue;
				/*
				 * If it's a redirection, then we don't want to
				 * create new outgoing port stuff.
				 * Redirections are only for incoming
				 * connections.
				 */
				if (!(np->in_redir & (NAT_MAP|NAT_MAPBLK)))
					continue;
				if ((nat = nat_new(np, ip, fin, (u_int)nflags,
						    NAT_OUTBOUND))) {
					np->in_hits++;
#ifdef	IPFILTER_LOG
					nat_log(nat, (u_int)np->in_redir);
#endif
					break;
				}
			}
		}
		if ((np == NULL) && (i > 0)) {
			do {
				i--;
				msk <<= 1;
			} while ((i >= 0) && ((nat_masks & (1 << i)) == 0));
			if (i >= 0)
				goto maskloop;
		}
		MUTEX_DOWNGRADE(&ipf_nat);
	}

	if (nat) {
		np = nat->nat_ptr;
		if (natadd && fin->fin_fi.fi_fl & FI_FRAG)
			ipfr_nat_newfrag(ip, fin, 0, nat);
		ip->ip_src = nat->nat_outip;
		MUTEX_ENTER(&ipf_rw);
		nat->nat_age = fr_defnatage;
		nat->nat_bytes += ip->ip_len;
		nat->nat_pkts++;
		MUTEX_EXIT(&ipf_rw);

		/*
		 * Fix up checksums, not by recalculating them, but
		 * simply computing adjustments.
		 */
#if SOLARIS || defined(__sgi)
		if (nat->nat_dir == NAT_OUTBOUND)
			fix_outcksum(&ip->ip_sum, nat->nat_ipsumd);
		else
			fix_incksum(&ip->ip_sum, nat->nat_ipsumd);
#endif

		if (!(ip->ip_off & IP_OFFMASK) &&
		    !(fin->fin_fi.fi_fl & FI_SHORT)) {

			if ((nat->nat_outport != 0) && (nflags & IPN_TCPUDP)) {
				tcp->th_sport = nat->nat_outport;
				fin->fin_data[0] = ntohs(tcp->th_sport);
			}

			if (ip->ip_p == IPPROTO_TCP) {
				csump = &tcp->th_sum;
				MUTEX_ENTER(&ipf_rw);
				fr_tcp_age(&nat->nat_age,
					   nat->nat_tcpstate, ip, fin, 1);
				if (nat->nat_age < fr_defnaticmpage)
					nat->nat_age = fr_defnaticmpage;
#ifdef LARGE_NAT
				else if (nat->nat_age > DEF_NAT_AGE)
					nat->nat_age = DEF_NAT_AGE;
#endif
				/*
				 * Increase this because we may have
				 * "keep state" following this too and
				 * packet storms can occur if this is
				 * removed too quickly.
				 */
				if (nat->nat_age == fr_tcpclosed)
					nat->nat_age = fr_tcplastack;
				MUTEX_EXIT(&ipf_rw);
			} else if (ip->ip_p == IPPROTO_UDP) {
				udphdr_t *udp = (udphdr_t *)tcp;

				if (udp->uh_sum)
					csump = &udp->uh_sum;
			}
			if (csump) {
				if (nat->nat_dir == NAT_OUTBOUND)
					fix_outcksum(csump, nat->nat_sumd);
				else
					fix_incksum(csump, nat->nat_sumd);
			}
		}
		if ((np->in_apr != NULL) && (np->in_dport == 0 ||
		     (tcp != NULL && dport == np->in_dport)))
			(void) appr_check(ip, fin, nat);
		ATOMIC_INC(nat_stats.ns_mapped[1]);
		RWLOCK_EXIT(&ipf_nat);	/* READ */
		return 1;
	}
	RWLOCK_EXIT(&ipf_nat);			/* READ/WRITE */
	return 0;
}


/*
 * Packets coming in from the external interface go through this.
 * Here, the destination address requires alteration, if anything.
 */
int ip_natin(ip, fin)
ip_t *ip;
fr_info_t *fin;
{
	register struct in_addr src;
	register struct in_addr in;
	register ipnat_t *np;
	u_int nflags = 0, natadd = 1, hv, msk;
	struct ifnet *ifp = fin->fin_ifp;
	tcphdr_t *tcp = NULL;
	u_short sport = 0, dport = 0, *csump = NULL;
	nat_t *nat;
	u_32_t iph;
	int i;

	if (nat_list == NULL)
		return 0;

	if (!(ip->ip_off & IP_OFFMASK) && !(fin->fin_fi.fi_fl & FI_SHORT)) {
		if (ip->ip_p == IPPROTO_TCP)
			nflags = IPN_TCP;
		else if (ip->ip_p == IPPROTO_UDP)
			nflags = IPN_UDP;
		if ((nflags & IPN_TCPUDP)) {
			tcp = (tcphdr_t *)fin->fin_dp;
			dport = tcp->th_dport;
			sport = tcp->th_sport;
		}
	}

	in = ip->ip_dst;
	/* make sure the source address is to be redirected */
	src = ip->ip_src;

	READ_ENTER(&ipf_nat);

	if ((ip->ip_p == IPPROTO_ICMP) && (nat = nat_icmpin(ip, fin, &nflags)))
		;
	else if ((ip->ip_off & IP_OFFMASK) &&
		 (nat = ipfr_nat_knownfrag(ip, fin)))
		natadd = 0;
	else if ((nat = nat_inlookup(fin->fin_ifp, nflags, (u_int)ip->ip_p,
				     ip->ip_src, in, (dport << 16) | sport))) {
		nflags = nat->nat_flags;
		if ((nflags & (FI_W_SPORT|FI_W_DPORT)) != 0) {
			if ((nat->nat_oport != sport) && (nflags & FI_W_DPORT))
				nat->nat_oport = sport;
			else if ((nat->nat_outport != dport) &&
				 (nflags & FI_W_SPORT))
				nat->nat_outport = dport;
			nat->nat_flags &= ~(FI_W_SPORT|FI_W_DPORT);
			nflags = nat->nat_flags;
		}
	} else {
		RWLOCK_EXIT(&ipf_nat);
		WRITE_ENTER(&ipf_nat);
		/*
		 * If there is no current entry in the nat table for this IP#,
		 * create one for it (if there is a matching rule).
		 */
		msk = 0xffffffff;
		i = 32;
maskloop:
		iph = in.s_addr & htonl(msk);
		hv = NAT_HASH_FN(iph, ipf_rdrrules_sz);
		for (np = rdr_rules[hv]; np; np = np->in_rnext)
			if ((np->in_ifp == ifp) &&
			    (!np->in_flags || (nflags & np->in_flags)) &&
			    ((in.s_addr & np->in_outmsk) == np->in_outip) &&
			    ((src.s_addr & np->in_srcmsk) == np->in_srcip) &&
			    (np->in_redir & NAT_REDIRECT) &&
			     (!np->in_pmin || np->in_pmin == dport)) {
				if ((nat = nat_new(np, ip, fin, nflags,
						    NAT_INBOUND))) {
					np->in_hits++;
#ifdef	IPFILTER_LOG
					nat_log(nat, (u_int)np->in_redir);
#endif
					break;
				}
			}
		if ((np == NULL) && (i > 0)) {
			do {
				i--;
				msk <<= 1;
			} while ((i >= 0) && ((rdr_masks & (1 << i)) == 0));
			if (i >= 0)
				goto maskloop;
		}
		MUTEX_DOWNGRADE(&ipf_nat);
	}
	if (nat) {
		np = nat->nat_ptr;
		fin->fin_fr = nat->nat_fr;
		if (natadd && fin->fin_fi.fi_fl & FI_FRAG)
			ipfr_nat_newfrag(ip, fin, 0, nat);
		if ((np->in_apr != NULL) && (np->in_dport == 0 ||
		    (tcp != NULL && sport == np->in_dport)))
			(void) appr_check(ip, fin, nat);

		MUTEX_ENTER(&ipf_rw);
		if (nflags != IPN_ICMPERR)
			nat->nat_age = fr_defnatage;

		nat->nat_bytes += ip->ip_len;
		nat->nat_pkts++;
		MUTEX_EXIT(&ipf_rw);
		ip->ip_dst = nat->nat_inip;
		fin->fin_fi.fi_dst = nat->nat_inip;

		/*
		 * Fix up checksums, not by recalculating them, but
		 * simply computing adjustments.
		 */
#if SOLARIS || defined(__sgi)
		if (nat->nat_dir == NAT_OUTBOUND)
			fix_incksum(&ip->ip_sum, nat->nat_ipsumd);
		else
			fix_outcksum(&ip->ip_sum, nat->nat_ipsumd);
#endif
		if (!(ip->ip_off & IP_OFFMASK) &&
		    !(fin->fin_fi.fi_fl & FI_SHORT)) {

			if ((nat->nat_inport != 0) && (nflags & IPN_TCPUDP)) {
				tcp->th_dport = nat->nat_inport;
				fin->fin_data[1] = ntohs(tcp->th_dport);
			}

			if (ip->ip_p == IPPROTO_TCP) {
				csump = &tcp->th_sum;
				MUTEX_ENTER(&ipf_rw);
				fr_tcp_age(&nat->nat_age,
					   nat->nat_tcpstate, ip, fin, 0);
				if (nat->nat_age < fr_defnaticmpage)
					nat->nat_age = fr_defnaticmpage;
#ifdef LARGE_NAT
				else if (nat->nat_age > DEF_NAT_AGE)
					nat->nat_age = DEF_NAT_AGE;
#endif
				/*
				 * Increase this because we may have
				 * "keep state" following this too and
				 * packet storms can occur if this is
				 * removed too quickly.
				 */
				if (nat->nat_age == fr_tcpclosed)
					nat->nat_age = fr_tcplastack;
				MUTEX_EXIT(&ipf_rw);
			} else if (ip->ip_p == IPPROTO_UDP) {
				udphdr_t *udp = (udphdr_t *)tcp;

				if (udp->uh_sum)
					csump = &udp->uh_sum;
			}
			if (csump) {
				if (nat->nat_dir == NAT_OUTBOUND)
					fix_incksum(csump, nat->nat_sumd);
				else
					fix_outcksum(csump, nat->nat_sumd);
			}
		}
		ATOMIC_INC(nat_stats.ns_mapped[0]);
		RWLOCK_EXIT(&ipf_nat);			/* READ */
		return 1;
	}
	RWLOCK_EXIT(&ipf_nat);			/* READ/WRITE */
	return 0;
}


/*
 * Free all memory used by NAT structures allocated at runtime.
 */
void ip_natunload()
{
	WRITE_ENTER(&ipf_nat);
	(void) nat_clearlist();
	(void) nat_flushtable();
	RWLOCK_EXIT(&ipf_nat);

	if (nat_table[0] != NULL) {
		KFREES(nat_table[0], sizeof(nat_t *) * ipf_nattable_sz);
		nat_table[0] = NULL;
	}
	if (nat_table[1] != NULL) {
		KFREES(nat_table[1], sizeof(nat_t *) * ipf_nattable_sz);
		nat_table[1] = NULL;
	}
	if (nat_rules != NULL) {
		KFREES(nat_rules, sizeof(ipnat_t *) * ipf_natrules_sz);
		nat_rules = NULL;
	}
	if (rdr_rules != NULL) {
		KFREES(rdr_rules, sizeof(ipnat_t *) * ipf_rdrrules_sz);
		rdr_rules = NULL;
	}
}


/*
 * Slowly expire held state for NAT entries.  Timeouts are set in
 * expectation of this being called twice per second.
 */
void ip_natexpire()
{
	register struct nat *nat, **natp;
#if defined(_KERNEL) && !SOLARIS
	int s;
#endif

	SPL_NET(s);
	WRITE_ENTER(&ipf_nat);
	for (natp = &nat_instances; (nat = *natp); ) {
		nat->nat_age--;
		if (nat->nat_age) {
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
	RWLOCK_EXIT(&ipf_nat);
	SPL_X(s);
}


/*
 */
void ip_natsync(ifp)
void *ifp;
{
	register ipnat_t *n;
	register nat_t *nat;
	register u_32_t sum1, sum2, sumd;
	struct in_addr in;
	ipnat_t *np;
	void *ifp2;
#if defined(_KERNEL) && !SOLARIS
	int s;
#endif

	/*
	 * Change IP addresses for NAT sessions for any protocol except TCP
	 * since it will break the TCP connection anyway.
	 */
	SPL_NET(s);
	WRITE_ENTER(&ipf_nat);
	for (nat = nat_instances; nat; nat = nat->nat_next)
		if (((ifp == NULL) || (ifp == nat->nat_ifp)) &&
		    !(nat->nat_flags & IPN_TCP) && (np = nat->nat_ptr) &&
		    (np->in_outmsk == 0xffffffff) && !np->in_nip) {
			ifp2 = nat->nat_ifp;
			/*
			 * Change the map-to address to be the same as the
			 * new one.
			 */
			sum1 = nat->nat_outip.s_addr;
			if (fr_ifpaddr(ifp2, &in) != -1)
				nat->nat_outip.s_addr = htonl(in.s_addr);
			sum2 = nat->nat_outip.s_addr;

			if (sum1 == sum2)
				continue;
			/*
			 * Readjust the checksum adjustment to take into
			 * account the new IP#.
			 */
			CALC_SUMD(sum1, sum2, sumd);
			sumd += nat->nat_sumd;
			nat->nat_sumd = (sumd & 0xffff) + (sumd >> 16);
		}

	for (n = nat_list; (n != NULL); n = n->in_next)
		if (n->in_ifp == ifp)
			n->in_ifp = (void *)GETUNIT(n->in_ifname);
	RWLOCK_EXIT(&ipf_nat);
	SPL_X(s);
}


#ifdef	IPFILTER_LOG
void nat_log(nat, type)
struct nat *nat;
u_int type;
{
	struct ipnat *np;
	struct natlog natl;
	void *items[1];
	size_t sizes[1];
	int rulen, types[1];

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
#ifndef LARGE_NAT
	if (nat->nat_ptr != NULL) {
		for (rulen = 0, np = nat_list; np; np = np->in_next, rulen++)
			if (np == nat->nat_ptr) {
				natl.nl_rule = rulen;
				break;
			}
	}
#endif
	items[0] = &natl;
	sizes[0] = sizeof(natl);
	types[0] = 0;

	(void) ipllog(IPL_LOGNAT, NULL, items, sizes, types, 1);
}
#endif
