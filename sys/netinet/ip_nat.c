/*
 * Copyright (C) 1995-2000 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * Added redirect stuff and a LOT of bug fixes. (mcn@EnGarde.com)
 */
#if !defined(lint)
static const char sccsid[] = "@(#)ip_nat.c	1.11 6/5/96 (C) 1995 Darren Reed";
/*static const char rcsid[] = "@(#)$Id: ip_nat.c,v 2.37.2.16 2000/07/18 13:57:40 darrenr Exp $";*/
static const char rcsid[] = "@(#)$FreeBSD$";
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
#if (defined(KERNEL) || defined(_KERNEL)) && (__FreeBSD_version >= 220000)
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
u_int	ipf_hostmap_sz = HOSTMAP_SIZE;
u_32_t	nat_masks = 0;
u_32_t	rdr_masks = 0;
ipnat_t	**nat_rules = NULL;
ipnat_t	**rdr_rules = NULL;
hostmap_t	**maptable  = NULL;

u_long	fr_defnatage = DEF_NAT_AGE,
	fr_defnaticmpage = 6;		/* 3 seconds */
natstat_t nat_stats;
int	fr_nat_lock = 0;
#if	(SOLARIS || defined(__sgi)) && defined(_KERNEL)
extern	kmutex_t	ipf_rw;
extern	KRWLOCK_T	ipf_nat;
#endif

static	int	nat_flushtable __P((void));
static	int	nat_clearlist __P((void));
static	void	nat_addnat __P((struct ipnat *));
static	void	nat_addrdr __P((struct ipnat *));
static	void	nat_delete __P((struct nat *));
static	void	nat_delrdr __P((struct ipnat *));
static	void	nat_delnat __P((struct ipnat *));
static	int	fr_natgetent __P((caddr_t));
static	int	fr_natgetsz __P((caddr_t));
static	int	fr_natputent __P((caddr_t));
static	void	nat_tabmove __P((nat_t *, u_32_t));
static	int	nat_match __P((fr_info_t *, ipnat_t *, ip_t *));
static	hostmap_t *nat_hostmap __P((ipnat_t *, struct in_addr,
				    struct in_addr));
static	void	nat_hostmapdel __P((struct hostmap *));


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

	KMALLOCS(maptable, hostmap_t **, sizeof(hostmap_t *) * ipf_hostmap_sz);
	if (maptable != NULL)
		bzero((char *)maptable, sizeof(hostmap_t *) * ipf_hostmap_sz);
	else
		return -1;
	return 0;
}


static void nat_addrdr(n)
ipnat_t *n;
{
	ipnat_t **np;
	u_32_t j;
	u_int hv;
	int k;

	k = countbits(n->in_outmsk);
	if ((k >= 0) && (k != 32))
		rdr_masks |= 1 << k;
	j = (n->in_outip & n->in_outmsk);
	hv = NAT_HASH_FN(j, 0, ipf_rdrrules_sz);
	np = rdr_rules + hv;
	while (*np != NULL)
		np = &(*np)->in_rnext;
	n->in_rnext = NULL;
	n->in_prnext = np;
	*np = n;
}


static void nat_addnat(n)
ipnat_t *n;
{
	ipnat_t **np;
	u_32_t j;
	u_int hv;
	int k;

	k = countbits(n->in_inmsk);
	if ((k >= 0) && (k != 32))
		nat_masks |= 1 << k;
	j = (n->in_inip & n->in_inmsk);
	hv = NAT_HASH_FN(j, 0, ipf_natrules_sz);
	np = nat_rules + hv;
	while (*np != NULL)
		np = &(*np)->in_mnext;
	n->in_mnext = NULL;
	n->in_pmnext = np;
	*np = n;
}


static void nat_delrdr(n)
ipnat_t *n;
{
	if (n->in_rnext)
		n->in_rnext->in_prnext = n->in_prnext;
	*n->in_prnext = n->in_rnext;
}


static void nat_delnat(n)
ipnat_t *n;
{
	if (n->in_mnext)
		n->in_mnext->in_pmnext = n->in_pmnext;
	*n->in_pmnext = n->in_mnext;
}


/*
 * check if an ip address has already been allocated for a given mapping that
 * is not doing port based translation.
 *
 * Must be called with ipf_nat held as a write lock.
 */
static struct hostmap *nat_hostmap(np, real, map)
ipnat_t *np;
struct in_addr real;
struct in_addr map;
{
	hostmap_t *hm;
	u_int hv;

	hv = real.s_addr % HOSTMAP_SIZE;
	for (hm = maptable[hv]; hm; hm = hm->hm_next)
		if ((hm->hm_realip.s_addr == real.s_addr) &&
		    (np == hm->hm_ipnat)) {
			hm->hm_ref++;
			return hm;
		}

	KMALLOC(hm, hostmap_t *);
	if (hm) {
		hm->hm_next = maptable[hv];
		hm->hm_pnext = maptable + hv;
		if (maptable[hv])
			maptable[hv]->hm_pnext = &hm->hm_next;
		maptable[hv] = hm;
		hm->hm_ipnat = np;
		hm->hm_realip = real;
		hm->hm_mapip = map;
		hm->hm_ref = 1;
	}
	return hm;
}


/*
 * Must be called with ipf_nat held as a write lock.
 */
static void nat_hostmapdel(hm)
struct hostmap *hm;
{
	ATOMIC_DEC32(hm->hm_ref);
	if (hm->hm_ref == 0) {
		if (hm->hm_next)
			hm->hm_next->hm_pnext = hm->hm_pnext;
		*hm->hm_pnext = hm->hm_next;
		KFREE(hm);
	}
}


void fix_outcksum(sp, n)
u_short *sp;
u_32_t n;
{
	register u_short sumshort;
	register u_32_t sum1;

	if (!n)
		return;
#if SOLARIS2 >= 6
	else if (n & NAT_HW_CKSUM) {
		*sp = n & 0xffff;
		return;
	}
#endif
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
#if SOLARIS2 >= 6
	else if (n & NAT_HW_CKSUM) {
		*sp = n & 0xffff;
		return;
	}
#endif
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
 * fix_datacksum is used *only* for the adjustments of checksums in the data
 * section of an IP packet.
 *
 * The only situation in which you need to do this is when NAT'ing an 
 * ICMP error message. Such a message, contains in its body the IP header
 * of the original IP packet, that causes the error.
 *
 * You can't use fix_incksum or fix_outcksum in that case, because for the
 * kernel the data section of the ICMP error is just data, and no special 
 * processing like hardware cksum or ntohs processing have been done by the 
 * kernel on the data section.
 */
void fix_datacksum(sp, n)
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
#if defined(__NetBSD__) || defined(__OpenBSD__) || (__FreeBSD_version >= 300003)
u_long cmd;
#else
int cmd;
#endif
caddr_t data;
int mode;
{
	register ipnat_t *nat, *nt, *n = NULL, **np = NULL;
	int error = 0, ret, arg;
	ipnat_t natd;
	u_32_t i, j;

#if (BSD >= 199306) && defined(_KERNEL)
	if ((securelevel >= 2) && (mode & FWRITE))
		return EPERM;
#endif

	nat = NULL;     /* XXX gcc -Wuninitialized */
	KMALLOC(nt, ipnat_t *);
	if ((cmd == SIOCADNAT) || (cmd == SIOCRMNAT))
		error = IRCOPYPTR(data, (char *)&natd, sizeof(natd));
	else if (cmd == SIOCIPFFL) {	/* SIOCFLNAT & SIOCCNATL */
		error = IRCOPY(data, (char *)&arg, sizeof(arg));
		if (error)
			error = EFAULT;
	}

	if (error)
		goto done;

	/*
	 * For add/delete, look to see if the NAT entry is already present
	 */
	WRITE_ENTER(&ipf_nat);
	if ((cmd == SIOCADNAT) || (cmd == SIOCRMNAT)) {
		nat = &natd;
		nat->in_flags &= IPN_USERFLAGS;
		if ((nat->in_redir & NAT_MAPBLK) == 0) {
			if ((nat->in_flags & IPN_SPLIT) == 0)
				nat->in_inip &= nat->in_inmsk;
			if ((nat->in_flags & IPN_IPRANGE) == 0)
				nat->in_outip &= nat->in_outmsk;
		}
		for (np = &nat_list; (n = *np); np = &n->in_next)
			if (!bcmp((char *)&nat->in_flags, (char *)&n->in_flags,
					IPN_CMPSIZ))
				break;
	}

	switch (cmd)
	{
#ifdef  IPFILTER_LOG
	case SIOCIPFFB :
	{
		int tmp;

		if (!(mode & FWRITE))
			error = EPERM;
		else {
			tmp = ipflog_clear(IPL_LOGNAT);
			IWCOPY((char *)&tmp, (char *)data, sizeof(tmp));
		}
		break;
	}
#endif
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
		n->in_ifp = (void *)GETUNIT(n->in_ifname, 4);
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
			n->in_flags &= ~IPN_NOTDST;
			nat_addrdr(n);
		}
		if (n->in_redir & (NAT_MAP|NAT_MAPBLK)) {
			n->in_flags &= ~IPN_NOTSRC;
			nat_addnat(n);
		}

		n->in_use = 0;
		if (n->in_redir & NAT_MAPBLK)
			n->in_space = USABLE_PORTS * ~ntohl(n->in_outmsk);
		else if (n->in_flags & IPN_AUTOPORTMAP)
			n->in_space = USABLE_PORTS * ~ntohl(n->in_inmsk);
		else if (n->in_flags & IPN_IPRANGE)
			n->in_space = ntohl(n->in_outmsk) - ntohl(n->in_outip);
		else if (n->in_flags & IPN_SPLIT)
			n->in_space = 2;
		else
			n->in_space = ~ntohl(n->in_outmsk);
		/*
		 * Calculate the number of valid IP addresses in the output
		 * mapping range.  In all cases, the range is inclusive of
		 * the start and ending IP addresses.
		 * If to a CIDR address, lose 2: broadcast + network address
		 *			         (so subtract 1)
		 * If to a range, add one.
		 * If to a single IP address, set to 1.
		 */
		if (n->in_space) {
			if ((n->in_flags & IPN_IPRANGE) != 0)
				n->in_space += 1;
			else
				n->in_space -= 1;
		} else
			n->in_space = 1;
		if ((n->in_outmsk != 0xffffffff) && (n->in_outmsk != 0) &&
		    ((n->in_flags & (IPN_IPRANGE|IPN_SPLIT)) == 0))
			n->in_nip = ntohl(n->in_outip) + 1;
		else if ((n->in_flags & IPN_SPLIT) &&
			 (n->in_redir & NAT_REDIRECT))
			n->in_nip = ntohl(n->in_inip);
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
		error = IWCOPYPTR((char *)&nat_stats, (char *)data,
				  sizeof(nat_stats));
		break;
	case SIOCGNATL :
	    {
		natlookup_t nl;

		MUTEX_DOWNGRADE(&ipf_nat);
		error = IRCOPYPTR((char *)data, (char *)&nl, sizeof(nl));
		if (error)
			break;

		if (nat_lookupredir(&nl)) {
			error = IWCOPYPTR((char *)&nl, (char *)data,
					  sizeof(nl));
		} else
			error = ESRCH;
		break;
	    }
	case SIOCIPFFL :	/* old SIOCFLNAT & SIOCCNATL */
		if (!(mode & FWRITE)) {
			error = EPERM;
			break;
		}
		error = 0;
		if (arg == 0)
			ret = nat_flushtable();
		else if (arg == 1)
			ret = nat_clearlist();
		else
			error = EINVAL;
		MUTEX_DOWNGRADE(&ipf_nat);
		if (!error) {
			error = IWCOPY((caddr_t)&ret, data, sizeof(ret));
			if (error)
				error = EFAULT;
		}
		break;
	case SIOCSTLCK :
		error = IRCOPY(data, (caddr_t)&arg, sizeof(arg));
		if (!error) {
			error = IWCOPY((caddr_t)&fr_nat_lock, data,
					sizeof(fr_nat_lock));
			if (!error)
				fr_nat_lock = arg;
		} else
			error = EFAULT;
		break;
	case SIOCSTPUT :
		if (fr_nat_lock)
			error = fr_natputent(data);
		else
			error = EACCES;
		break;
	case SIOCSTGSZ :
		if (fr_nat_lock)
			error = fr_natgetsz(data);
		else
			error = EACCES;
		break;
	case SIOCSTGET :
		if (fr_nat_lock)
			error = fr_natgetent(data);
		else
			error = EACCES;
		break;
	case FIONREAD :
#ifdef	IPFILTER_LOG
		arg = (int)iplused[IPL_LOGNAT];
		MUTEX_DOWNGRADE(&ipf_nat);
		error = IWCOPY((caddr_t)&arg, (caddr_t)data, sizeof(arg));
		if (error)
			error = EFAULT;
#endif
		break;
	default :
		error = EINVAL;
		break;
	}
	RWLOCK_EXIT(&ipf_nat);			/* READ/WRITE */
done:
	if (nt)
		KFREE(nt);
	return error;
}


static int fr_natgetsz(data)
caddr_t data;
{
	ap_session_t *aps;
	nat_t *nat, *n;
	int error = 0;
	natget_t ng;

	error = IRCOPY(data, (caddr_t)&ng, sizeof(ng));
	if (error)
		return EFAULT;

	nat = ng.ng_ptr;
	if (!nat) {
		nat = nat_instances;
		ng.ng_sz = 0;
		if (nat == NULL) {
			error = IWCOPY((caddr_t)&ng, data, sizeof(ng));
			if (error)
				error = EFAULT;
			return error;
		}
	} else {
		/*
		 * Make sure the pointer we're copying from exists in the
		 * current list of entries.  Security precaution to prevent
		 * copying of random kernel data.
		 */
		for (n = nat_instances; n; n = n->nat_next)
			if (n == nat)
				break;
		if (!n)
			return ESRCH;
	}

	ng.ng_sz = sizeof(nat_save_t);
	aps = nat->nat_aps;
	if ((aps != NULL) && (aps->aps_data != 0)) {
		ng.ng_sz += sizeof(ap_session_t);
		ng.ng_sz += aps->aps_psiz;
	}

	error = IWCOPY((caddr_t)&ng, data, sizeof(ng));
	if (error)
		error = EFAULT;
	return error;
}


static int fr_natgetent(data)
caddr_t data;
{
	nat_save_t ipn, *ipnp, *ipnn = NULL;
	register nat_t *n, *nat;
	ap_session_t *aps;
	int error;

	error = IRCOPY(data, (caddr_t)&ipnp, sizeof(ipnp));
	if (error)
		return EFAULT;
	error = IRCOPY((caddr_t)ipnp, (caddr_t)&ipn, sizeof(ipn));
	if (error)
		return EFAULT;

	nat = ipn.ipn_next;
	if (!nat) {
		nat = nat_instances;
		if (nat == NULL) {
			if (nat_instances == NULL)
				return ENOENT;
			return 0;
		}
	} else {
		/*
		 * Make sure the pointer we're copying from exists in the
		 * current list of entries.  Security precaution to prevent
		 * copying of random kernel data.
		 */
		for (n = nat_instances; n; n = n->nat_next)
			if (n == nat)
				break;
		if (!n)
			return ESRCH;
	}

	ipn.ipn_next = nat->nat_next;
	ipn.ipn_dsize = 0;
	bcopy((char *)nat, (char *)&ipn.ipn_nat, sizeof(ipn.ipn_nat));
	ipn.ipn_nat.nat_data = NULL;

	if (nat->nat_ptr) {
		bcopy((char *)nat->nat_ptr, (char *)&ipn.ipn_ipnat,
		      sizeof(ipn.ipn_ipnat));
	}

	if (nat->nat_fr)
		bcopy((char *)nat->nat_fr, (char *)&ipn.ipn_rule,
		      sizeof(ipn.ipn_rule));

	if ((aps = nat->nat_aps)) {
		ipn.ipn_dsize = sizeof(*aps);
		if (aps->aps_data)
			ipn.ipn_dsize += aps->aps_psiz;
		KMALLOCS(ipnn, nat_save_t *, sizeof(*ipnn) + ipn.ipn_dsize);
		if (ipnn == NULL)
			return ENOMEM;
		bcopy((char *)&ipn, (char *)ipnn, sizeof(ipn));

		bcopy((char *)aps, ipnn->ipn_data, sizeof(*aps));
		if (aps->aps_data) {
			bcopy(aps->aps_data, ipnn->ipn_data + sizeof(*aps),
			      aps->aps_psiz);
			ipnn->ipn_dsize += aps->aps_psiz;
		}
		error = IWCOPY((caddr_t)ipnn, ipnp,
			       sizeof(ipn) + ipn.ipn_dsize);
		if (error)
			error = EFAULT;
		KFREES(ipnn, sizeof(*ipnn) + ipn.ipn_dsize);
	} else {
		error = IWCOPY((caddr_t)&ipn, ipnp, sizeof(ipn));
		if (error)
			error = EFAULT;
	}
	return error;
}


static int fr_natputent(data)
caddr_t data;
{
	nat_save_t ipn, *ipnp, *ipnn = NULL;
	register nat_t *n, *nat;
	ap_session_t *aps;
	frentry_t *fr;
	ipnat_t *in;

	int error;

	error = IRCOPY(data, (caddr_t)&ipnp, sizeof(ipnp));
	if (error)
		return EFAULT;
	error = IRCOPY((caddr_t)ipnp, (caddr_t)&ipn, sizeof(ipn));
	if (error)
		return EFAULT;
	nat = NULL;
	if (ipn.ipn_dsize) {
		KMALLOCS(ipnn, nat_save_t *, sizeof(ipn) + ipn.ipn_dsize);
		if (ipnn == NULL)
			return ENOMEM;
		bcopy((char *)&ipn, (char *)ipnn, sizeof(ipn));
		error = IRCOPY((caddr_t)ipnp, (caddr_t)ipn.ipn_data,
			       ipn.ipn_dsize);
		if (error) {
			error = EFAULT;
			goto junkput;
		}
	} else
		ipnn = NULL;

	KMALLOC(nat, nat_t *);
	if (nat == NULL) {
		error = EFAULT;
		goto junkput;
	}

	bcopy((char *)&ipn.ipn_nat, (char *)nat, sizeof(*nat));
	/*
	 * Initialize all these so that nat_delete() doesn't cause a crash.
	 */
	nat->nat_phnext[0] = NULL;
	nat->nat_phnext[1] = NULL;
	fr = nat->nat_fr;
	nat->nat_fr = NULL;
	aps = nat->nat_aps;
	nat->nat_aps = NULL;
	in = nat->nat_ptr;
	nat->nat_ptr = NULL;
	nat->nat_data = NULL;

	/*
	 * Restore the rule associated with this nat session
	 */
	if (in) {
		KMALLOC(in, ipnat_t *);
		if (in == NULL) {
			error = ENOMEM;
			goto junkput;
		}
		nat->nat_ptr = in;
		bcopy((char *)&ipn.ipn_ipnat, (char *)in, sizeof(*in));
		in->in_use = 1;
		in->in_flags |= IPN_DELETE;
		in->in_next = NULL;
		in->in_rnext = NULL;
		in->in_prnext = NULL;
		in->in_mnext = NULL;
		in->in_pmnext = NULL;
		in->in_ifp = GETUNIT(in->in_ifname, 4);
		if (in->in_plabel[0] != '\0') {
			in->in_apr = appr_match(in->in_p, in->in_plabel);
		}
	}

	/*
	 * Restore ap_session_t structure.  Include the private data allocated
	 * if it was there.
	 */
	if (aps) {
		KMALLOC(aps, ap_session_t *);
		if (aps == NULL) {
			error = ENOMEM;
			goto junkput;
		}
		nat->nat_aps = aps;
		aps->aps_next = ap_sess_list;
		ap_sess_list = aps;
		bcopy(ipnn->ipn_data, (char *)aps, sizeof(*aps));
		if (in)
			aps->aps_apr = in->in_apr;
		if (aps->aps_psiz) {
			KMALLOCS(aps->aps_data, void *, aps->aps_psiz);
			if (aps->aps_data == NULL) {
				error = ENOMEM;
				goto junkput;
			}
			bcopy(ipnn->ipn_data + sizeof(*aps), aps->aps_data,
			      aps->aps_psiz);
		} else {
			aps->aps_psiz = 0;
			aps->aps_data = NULL;
		}
	}

	/*
	 * If there was a filtering rule associated with this entry then
	 * build up a new one.
	 */
	if (fr != NULL) {
		if (nat->nat_flags & FI_NEWFR) {
			KMALLOC(fr, frentry_t *);
			nat->nat_fr = fr;
			if (fr == NULL) {
				error = ENOMEM;
				goto junkput;
			}
			bcopy((char *)&ipn.ipn_fr, (char *)fr, sizeof(*fr));
			ipn.ipn_nat.nat_fr = fr;
			error = IWCOPY((caddr_t)&ipn, ipnp, sizeof(ipn));
			if (error) {
				error = EFAULT;
				goto junkput;
			}
		} else {
			for (n = nat_instances; n; n = n->nat_next)
				if (n->nat_fr == fr)
					break;
			if (!n) {
				error = ESRCH;
				goto junkput;
			}
		}
	}

	if (ipnn)
		KFREES(ipnn, sizeof(ipn) + ipn.ipn_dsize);
	nat_insert(nat);
	return 0;
junkput:
	if (ipnn)
		KFREES(ipnn, sizeof(ipn) + ipn.ipn_dsize);
	if (nat)
		nat_delete(nat);
	return error;
}


/*
 * Delete a nat entry from the various lists and table.
 */
static void nat_delete(natd)
struct nat *natd;
{
	struct ipnat *ipn;

	if (natd->nat_flags & FI_WILDP)
		nat_stats.ns_wilds--;
	if (natd->nat_hnext[0])
		natd->nat_hnext[0]->nat_phnext[0] = natd->nat_phnext[0];
	*natd->nat_phnext[0] = natd->nat_hnext[0];
	if (natd->nat_hnext[1])
		natd->nat_hnext[1]->nat_phnext[1] = natd->nat_phnext[1];
	*natd->nat_phnext[1] = natd->nat_hnext[1];

	if (natd->nat_fr != NULL) {
		ATOMIC_DEC32(natd->nat_fr->fr_ref);
	}

	if (natd->nat_hm != NULL)
		nat_hostmapdel(natd->nat_hm);

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

	MUTEX_DESTROY(&natd->nat_lock);
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
#ifdef	IPFILTER_LOG
		nat_log(nat, NL_FLUSH);
#endif
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
	struct in_addr in, inb;
	tcphdr_t *tcp = NULL;
	hostmap_t *hm = NULL;
	nat_t *nat, *natl;
	u_short nflags;
#if SOLARIS && defined(_KERNEL) && (SOLARIS2 >= 6)
	qif_t *qf = fin->fin_qif;
#endif

	nflags = flags & np->in_flags;
	if (flags & IPN_TCPUDP) {
		tcp = (tcphdr_t *)fin->fin_dp;
		sport = tcp->th_sport;
		dport = tcp->th_dport;
	}

	/* Give me a new nat */
	KMALLOC(nat, nat_t *);
	if (nat == NULL) {
		nat_stats.ns_memfail++;
		return NULL;
	}

	bzero((char *)nat, sizeof(*nat));
	nat->nat_flags = flags;
	if (flags & FI_WILDP)
		nat_stats.ns_wilds++;
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
			in.s_addr = htonl(np->in_nip);
			if (l == 0) {
				/*
				 * Check to see if there is an existing NAT
				 * setup for this IP address pair.
				 */
				hm = nat_hostmap(np, ip->ip_src, in);
				if (hm != NULL)
					in.s_addr = hm->hm_mapip.s_addr;
			} else if ((l == 1) && (hm != NULL)) {
				nat_hostmapdel(hm);
				hm = NULL;
			}
			in.s_addr = ntohl(in.s_addr);

			nat->nat_hm = hm;

			if ((np->in_outmsk == 0xffffffff) &&
			    (np->in_pnext == 0)) {
				if (l > 0)
					goto badnat;
			}

			if (np->in_redir & NAT_MAPBLK) {
				if ((l >= np->in_ppip) || ((l > 0) &&
				     !(flags & IPN_TCPUDP)))
					goto badnat;
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
			} else if (!np->in_outip &&
				   (np->in_outmsk == 0xffffffff)) {
				/*
				 * 0/32 - use the interface's IP address.
				 */
				if ((l > 0) ||
				    fr_ifpaddr(4, fin->fin_ifp, &in) == -1)
					goto badnat;
				in.s_addr = ntohl(in.s_addr);
			} else if (!np->in_outip && !np->in_outmsk) {
				/*
				 * 0/0 - use the original source address/port.
				 */
				if (l > 0)
					goto badnat;
				in.s_addr = ntohl(ip->ip_src.s_addr);
			} else if ((np->in_outmsk != 0xffffffff) &&
				   (np->in_pnext == 0) &&
				   ((l > 0) || (hm == NULL)))
				np->in_nip++;
			natl = NULL;

			if ((nflags & IPN_TCPUDP) &&
			    ((np->in_redir & NAT_MAPBLK) == 0) &&
			    (np->in_flags & IPN_AUTOPORTMAP)) {
				if ((l > 0) && (l % np->in_ppip == 0)) {
					if (l > np->in_space) {
						goto badnat;
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

			if (np->in_flags & IPN_IPRANGE) {
				if (np->in_nip > ntohl(np->in_outmsk))
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
			natl = nat_inlookup(fin->fin_ifp, flags & ~FI_WILDP,
					    (u_int)ip->ip_p, ip->ip_dst, inb,
					    (port << 16) | dport, 1);

			/*
			 * Has the search wrapped around and come back to the
			 * start ?
			 */
			if ((natl != NULL) &&
			    (np->in_pnext != 0) && (st_port == np->in_pnext) &&
			    (np->in_nip != 0) && (st_ip == np->in_nip))
				goto badnat;
			l++;
		} while (natl != NULL);

		if (np->in_space > 0)
			np->in_space--;

		/* Setup the NAT table */
		nat->nat_inip = ip->ip_src;
		nat->nat_outip.s_addr = htonl(in.s_addr);
		nat->nat_oip = ip->ip_dst;
		if (nat->nat_hm == NULL)
			nat->nat_hm = nat_hostmap(np, ip->ip_src,
						  nat->nat_outip);

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
		if (np->in_flags & IPN_SPLIT) {
			in.s_addr = np->in_nip;
			if (np->in_inip == htonl(in.s_addr))
				np->in_nip = ntohl(np->in_inmsk);
			else {
				np->in_nip = ntohl(np->in_inip);
				if (np->in_flags & IPN_ROUNDR) {
					nat_delrdr(np);
					nat_addrdr(np);
				}
			}
		} else {
			in.s_addr = ntohl(np->in_inip);
			if (np->in_flags & IPN_ROUNDR) {
				nat_delrdr(np);
				nat_addrdr(np);
			}
		}
		if (!np->in_pnext)
			nport = dport;
		else {
			/*
			 * Whilst not optimized for the case where
			 * pmin == pmax, the gain is not significant.
			 */
			nport = ntohs(dport) - ntohs(np->in_pmin) +
				ntohs(np->in_pnext);
			nport = htons(nport);
		}

		/*
		 * When the redirect-to address is set to 0.0.0.0, just
		 * assume a blank `forwarding' of the packet.  We don't
		 * setup any translation for this either.
		 */
		if (in.s_addr == 0) {
			if (nport == dport)
				goto badnat;
			in.s_addr = ntohl(ip->ip_dst.s_addr);
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
	nat->nat_sumd[0] = (sumd & 0xffff) + (sumd >> 16);
#if SOLARIS && defined(_KERNEL) && (SOLARIS2 >= 6)
	if ((flags == IPN_TCP) && dohwcksum &&
	    (qf->qf_ill->ill_ick.ick_magic == ICK_M_CTL_MAGIC)) {
		if (direction == NAT_OUTBOUND)
			sum1 = LONG_SUM(ntohl(in.s_addr));
		else
			sum1 = LONG_SUM(ntohl(ip->ip_src.s_addr));
		sum1 += LONG_SUM(ntohl(ip->ip_dst.s_addr));
		sum1 += 30;
		sum1 = (sum1 & 0xffff) + (sum1 >> 16);
		nat->nat_sumd[1] = NAT_HW_CKSUM|(sum1 & 0xffff);
	} else
#endif
		nat->nat_sumd[1] = nat->nat_sumd[0];

	if ((flags & IPN_TCPUDP) && ((sport != port) || (dport != nport))) {
		if (direction == NAT_OUTBOUND)
			sum1 = LONG_SUM(ntohl(ip->ip_src.s_addr));
		else
			sum1 = LONG_SUM(ntohl(ip->ip_dst.s_addr));

		sum2 = LONG_SUM(in.s_addr);

		CALC_SUMD(sum1, sum2, sumd);
		nat->nat_ipsumd = (sumd & 0xffff) + (sumd >> 16);
	} else
		nat->nat_ipsumd = nat->nat_sumd[0];

	in.s_addr = htonl(in.s_addr);

#ifdef  _KERNEL
	strncpy(nat->nat_ifname, IFNAME(fin->fin_ifp), IFNAMSIZ);
#endif
	nat_insert(nat);

	nat->nat_dir = direction;
	nat->nat_ifp = fin->fin_ifp;
	nat->nat_ptr = np;
	nat->nat_p = ip->ip_p;
	nat->nat_bytes = 0;
	nat->nat_pkts = 0;
	nat->nat_fr = fin->fin_fr;
	if (nat->nat_fr != NULL) {
		ATOMIC_INC32(nat->nat_fr->fr_ref);
	}
	if (direction == NAT_OUTBOUND) {
		if (flags & IPN_TCPUDP)
			tcp->th_sport = port;
	} else {
		if (flags & IPN_TCPUDP)
			tcp->th_dport = nport;
	}
	np->in_use++;
#ifdef	IPFILTER_LOG
	nat_log(nat, (u_int)np->in_redir);
#endif
	return nat;
badnat:
	nat_stats.ns_badnat++;
	if ((hm = nat->nat_hm) != NULL)
		nat_hostmapdel(hm);
	KFREE(nat);
	return NULL;
}


void	nat_insert(nat)
nat_t	*nat;
{
	nat_t **natp;
	u_int hv;

	MUTEX_INIT(&nat->nat_lock, "nat entry lock", NULL);

	nat->nat_age = fr_defnatage;
	nat->nat_ifname[sizeof(nat->nat_ifname) - 1] = '\0';
	if (nat->nat_ifname[0] !='\0') {
		nat->nat_ifp = GETUNIT(nat->nat_ifname, 4);
	}

	nat->nat_next = nat_instances;
	nat_instances = nat;

	hv = NAT_HASH_FN(nat->nat_inip.s_addr, nat->nat_inport,
			 ipf_nattable_sz);
	natp = &nat_table[0][hv];
	if (*natp)
		(*natp)->nat_phnext[0] = &nat->nat_hnext[0];
	nat->nat_phnext[0] = natp;
	nat->nat_hnext[0] = *natp;
	*natp = nat;

	hv = NAT_HASH_FN(nat->nat_outip.s_addr, nat->nat_outport,
			 ipf_nattable_sz);
	natp = &nat_table[1][hv];
	if (*natp)
		(*natp)->nat_phnext[1] = &nat->nat_hnext[1];
	nat->nat_phnext[1] = natp;
	nat->nat_hnext[1] = *natp;
	*natp = nat;

	nat_stats.ns_added++;
	nat_stats.ns_inuse++;
}


nat_t *nat_icmplookup(ip, fin, dir)
ip_t *ip;
fr_info_t *fin;
int dir;
{
	icmphdr_t *icmp;
	tcphdr_t *tcp = NULL;
	ip_t *oip;
	int flags = 0, type, minlen;

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
	minlen = (oip->ip_hl << 2);
	if (minlen < sizeof(ip_t))
		return NULL;
	if (ip->ip_len < ICMPERR_IPICMPHLEN + minlen)
		return NULL;
	/*
	 * Is the buffer big enough for all of it ?  It's the size of the IP
	 * header claimed in the encapsulated part which is of concern.  It
	 * may be too big to be in this buffer but not so big that it's
	 * outside the ICMP packet, leading to TCP deref's causing problems.
	 * This is possible because we don't know how big oip_hl is when we
	 * do the pullup early in fr_check() and thus can't gaurantee it is
	 * all here now.
	 */
#ifdef  _KERNEL
	{
	mb_t *m;

# if SOLARIS
	m = fin->fin_qfm;
	if ((char *)oip + fin->fin_dlen - ICMPERR_ICMPHLEN > (char *)m->b_wptr)
		return NULL;
# else
	m = *(mb_t **)fin->fin_mp;
	if ((char *)oip + fin->fin_dlen - ICMPERR_ICMPHLEN >
	    (char *)ip + m->m_len)
		return NULL;
# endif
	}
#endif

	if (oip->ip_p == IPPROTO_TCP)
		flags = IPN_TCP;
	else if (oip->ip_p == IPPROTO_UDP)
		flags = IPN_UDP;
	if (flags & IPN_TCPUDP) {
		minlen += 8;		/* + 64bits of data to get ports */
		if (ip->ip_len < ICMPERR_IPICMPHLEN + minlen)
			return NULL;
		tcp = (tcphdr_t *)((char *)oip + (oip->ip_hl << 2));
		if (dir == NAT_INBOUND)
			return nat_inlookup(fin->fin_ifp, flags,
				(u_int)oip->ip_p, oip->ip_dst, oip->ip_src,
				(tcp->th_sport << 16) | tcp->th_dport, 0);
		else
			return nat_outlookup(fin->fin_ifp, flags,
				(u_int)oip->ip_p, oip->ip_dst, oip->ip_src,
				(tcp->th_sport << 16) | tcp->th_dport, 0);
	}
	if (dir == NAT_INBOUND)
		return nat_inlookup(fin->fin_ifp, 0, (u_int)oip->ip_p,
			oip->ip_dst, oip->ip_src, 0, 0);
	else
		return nat_outlookup(fin->fin_ifp, 0, (u_int)oip->ip_p,
			oip->ip_dst, oip->ip_src, 0, 0);
}


/*
 * This should *ONLY* be used for incoming packets to make sure a NAT'd ICMP
 * packet gets correctly recognised.
 */
nat_t *nat_icmp(ip, fin, nflags, dir)
ip_t *ip;
fr_info_t *fin;
u_int *nflags;
int dir;
{
	u_32_t sum1, sum2, sumd, sumd2 = 0;
	struct in_addr in;
	icmphdr_t *icmp;
	udphdr_t *udp;
	nat_t *nat;
	ip_t *oip;
	int flags = 0;

	if ((fin->fin_fi.fi_fl & FI_SHORT) || (ip->ip_off & IP_OFFMASK))
		return NULL;
	/*
	 * nat_icmplookup() will return NULL for `defective' packets.
	 */
	if ((ip->ip_v != 4) || !(nat = nat_icmplookup(ip, fin, dir)))
		return NULL;
	*nflags = IPN_ICMPERR;
	icmp = (icmphdr_t *)fin->fin_dp;
	oip = (ip_t *)&icmp->icmp_ip;
	if (oip->ip_p == IPPROTO_TCP)
		flags = IPN_TCP;
	else if (oip->ip_p == IPPROTO_UDP)
		flags = IPN_UDP;
	udp = (udphdr_t *)((((char *)oip) + (oip->ip_hl << 2)));
	/*
	 * Need to adjust ICMP header to include the real IP#'s and
	 * port #'s.  Only apply a checksum change relative to the
	 * IP address change as it will be modified again in ip_natout
	 * for both address and port.  Two checksum changes are
	 * necessary for the two header address changes.  Be careful
	 * to only modify the checksum once for the port # and twice
	 * for the IP#.
	 */

	/*
	 * Step 1
	 * Fix the IP addresses in the offending IP packet. You also need
	 * to adjust the IP header checksum of that offending IP packet
	 * and the ICMP checksum of the ICMP error message itself.
	 *
	 * Unfortunately, for UDP and TCP, the IP addresses are also contained
	 * in the pseudo header that is used to compute the UDP resp. TCP
	 * checksum. So, we must compensate that as well. Even worse, the
	 * change in the UDP and TCP checksums require yet another
	 * adjustment of the ICMP checksum of the ICMP error message.
	 *
	 * For the moment we forget about TCP, because that checksum is not
	 * in the first 8 bytes, so it will not be available in most cases.
	 */

	if (oip->ip_dst.s_addr == nat->nat_oip.s_addr) {
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
		/*
		 * Fix IP checksum of the offending IP packet to adjust for
		 * the change in the IP address.
		 *
		 * Normally, you would expect that the ICMP checksum of the 
		 * ICMP error message needs to be adjusted as well for the
		 * IP address change in oip.
		 * However, this is a NOP, because the ICMP checksum is 
		 * calculated over the complete ICMP packet, which includes the
		 * changed oip IP addresses and oip->ip_sum. However, these 
		 * two changes cancel each other out (if the delta for
		 * the IP address is x, then the delta for ip_sum is minus x), 
		 * so no change in the icmp_cksum is necessary.
		 *
		 * Be careful that nat_dir refers to the direction of the
		 * offending IP packet (oip), not to its ICMP response (icmp)
		 */
		fix_datacksum(&oip->ip_sum, sumd);

		/*
		 * Fix UDP pseudo header checksum to compensate for the
		 * IP address change.
		 */
		if (oip->ip_p == IPPROTO_UDP && udp->uh_sum) {
			/*
			 * The UDP checksum is optional, only adjust it 
			 * if it has been set.
			 */
			sum1 = ntohs(udp->uh_sum);
			fix_datacksum(&udp->uh_sum, sumd);
			sum2 = ntohs(udp->uh_sum);

			/*
			 * Fix ICMP checksum to compensate the UDP 
			 * checksum adjustment.
			 */
			CALC_SUMD(sum1, sum2, sumd);
			sumd2 = sumd;
		}

#if 0
		/*
		 * Fix TCP pseudo header checksum to compensate for the 
		 * IP address change. Before we can do the change, we
		 * must make sure that oip is sufficient large to hold
		 * the TCP checksum (normally it does not!).
		 */
		if (oip->ip_p == IPPROTO_TCP) {
		
		}
#endif
	} else {

		/*
		 * Fix IP checksum of the offending IP packet to adjust for
		 * the change in the IP address.
		 *
		 * Normally, you would expect that the ICMP checksum of the 
		 * ICMP error message needs to be adjusted as well for the
		 * IP address change in oip.
		 * However, this is a NOP, because the ICMP checksum is 
		 * calculated over the complete ICMP packet, which includes the
		 * changed oip IP addresses and oip->ip_sum. However, these 
		 * two changes cancel each other out (if the delta for
		 * the IP address is x, then the delta for ip_sum is minus x), 
		 * so no change in the icmp_cksum is necessary.
		 *
		 * Be careful that nat_dir refers to the direction of the
		 * offending IP packet (oip), not to its ICMP response (icmp)
		 */
		fix_datacksum(&oip->ip_sum, sumd);

/* XXX FV : without having looked at Solaris source code, it seems unlikely
 * that SOLARIS would compensate this in the kernel (a body of an IP packet 
 * in the data section of an ICMP packet). I have the feeling that this should
 * be unconditional, but I'm not in a position to check.
 */
#if !SOLARIS && !defined(__sgi)
		/*
		 * Fix UDP pseudo header checksum to compensate for the
		 * IP address change.
		 */
		if (oip->ip_p == IPPROTO_UDP && udp->uh_sum) {
			/*
			 * The UDP checksum is optional, only adjust it 
			 * if it has been set 
			 */
			sum1 = ntohs(udp->uh_sum);
			fix_datacksum(&udp->uh_sum, sumd);
			sum2 = ntohs(udp->uh_sum);

			/*
			 * Fix ICMP checksum to compensate the UDP 
			 * checksum adjustment.
			 */
			CALC_SUMD(sum1, sum2, sumd);
			sumd2 = sumd;
		}
		
#if 0
		/* 
		 * Fix TCP pseudo header checksum to compensate for the 
		 * IP address change. Before we can do the change, we
		 * must make sure that oip is sufficient large to hold
		 * the TCP checksum (normally it does not!).
		 */
		if (oip->ip_p == IPPROTO_TCP) {
		
		};
#endif
		
#endif
	}

	if ((flags & IPN_TCPUDP) != 0) {
		tcphdr_t *tcp;

		/*
		 * XXX - what if this is bogus hl and we go off the end ?
		 * In this case, nat_icmpinlookup() will have returned NULL.
		 */
		tcp = (tcphdr_t *)udp;

		/*
		 * Step 2 :
		 * For offending TCP/UDP IP packets, translate the ports as
		 * well, based on the NAT specification. Of course such
		 * a change must be reflected in the ICMP checksum as well.
		 *
		 * Advance notice : Now it becomes complicated :-)
		 *
		 * Since the port fields are part of the TCP/UDP checksum
		 * of the offending IP packet, you need to adjust that checksum
		 * as well... but, if you change, you must change the icmp
		 * checksum *again*, to reflect that change.
		 *
		 * To further complicate: the TCP checksum is not in the first
		 * 8 bytes of the offending ip packet, so it most likely is not
		 * available (we might have to fix that if the encounter a
		 * device that returns more than 8 data bytes on icmp error)
		 */

		if (nat->nat_oport == tcp->th_dport) {
			if (tcp->th_sport != nat->nat_inport) {
				/*
				 * Fix ICMP checksum to compensate port
				 * adjustment.
				 */
				sum1 = ntohs(tcp->th_sport);
				sum2 = ntohs(nat->nat_inport);
				CALC_SUMD(sum1, sum2, sumd);
				sumd2 += sumd;
				tcp->th_sport = nat->nat_inport;

				/*
				 * Fix udp checksum to compensate port
				 * adjustment.  NOTE : the offending IP packet
				 * flows the other direction compared to the
				 * ICMP message.
				 *
				 * The UDP checksum is optional, only adjust
				 * it if it has been set.
				 */
				if (oip->ip_p == IPPROTO_UDP && udp->uh_sum) {

					sum1 = ntohs(udp->uh_sum);
					fix_datacksum(&udp->uh_sum, sumd);
					sum2 = ntohs(udp->uh_sum);

					/*
					 * Fix ICMP checksum to 
					 * compensate UDP checksum 
					 * adjustment.
					 */
					CALC_SUMD(sum1, sum2, sumd);
					sumd2 += sumd;
				}
			}
		} else {
			if (tcp->th_dport != nat->nat_outport) {
				/*
				 * Fix ICMP checksum to compensate port
				 * adjustment.
				 */
				sum1 = ntohs(tcp->th_dport);
				sum2 = ntohs(nat->nat_outport);
				CALC_SUMD(sum1, sum2, sumd);
				sumd2 += sumd;
				tcp->th_dport = nat->nat_outport;

				/*
				 * Fix udp checksum to compensate port
				 * adjustment.   NOTE : the offending IP
				 * packet flows the other direction compared
				 * to the ICMP message.
				 *
				 * The UDP checksum is optional, only adjust
				 * it if it has been set.
				 */
				if (oip->ip_p == IPPROTO_UDP && udp->uh_sum) {

					sum1 = ntohs(udp->uh_sum);
					fix_datacksum(&udp->uh_sum, sumd);
					sum2 = ntohs(udp->uh_sum);

					/*
					 * Fix ICMP checksum to compensate
					 * UDP checksum adjustment.
					 */
					CALC_SUMD(sum1, sum2, sumd);
					sumd2 += sumd;
				}
			}
		}
		if (sumd2) {
			sumd2 = (sumd2 & 0xffff) + (sumd2 >> 16);
			sumd2 = (sumd2 & 0xffff) + (sumd2 >> 16);
			if (nat->nat_dir == NAT_OUTBOUND) {
				fix_outcksum(&icmp->icmp_cksum, sumd2);
			} else {
				fix_incksum(&icmp->icmp_cksum, sumd2);
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
nat_t *nat_inlookup(ifp, flags, p, src, mapdst, ports, rw)
void *ifp;
register u_int flags, p;
struct in_addr src , mapdst;
u_32_t ports;
int rw;
{
	register u_short sport, dport;
	register nat_t *nat;
	register int nflags;
	register u_32_t dst;
	u_int hv;

	dst = mapdst.s_addr;
	dport = ports >> 16;
	sport = ports & 0xffff;
	flags &= IPN_TCPUDP;

	hv = NAT_HASH_FN(dst, dport, ipf_nattable_sz);
	nat = nat_table[1][hv];
	for (; nat; nat = nat->nat_hnext[1]) {
		nflags = nat->nat_flags;
		if ((!ifp || ifp == nat->nat_ifp) &&
		    nat->nat_oip.s_addr == src.s_addr &&
		    nat->nat_outip.s_addr == dst &&
		    (((p == 0) && (flags == (nat->nat_flags & IPN_TCPUDP)))
		     || (p == nat->nat_p)) && (!flags ||
		     (((nat->nat_oport == sport) || (nflags & FI_W_DPORT)) &&
		      ((nat->nat_outport == dport) || (nflags & FI_W_SPORT)))))
			return nat;
	}
	if (!nat_stats.ns_wilds || !(flags & IPN_TCPUDP))
		return NULL;
	if (!rw) {
		RWLOCK_EXIT(&ipf_nat);
	}
	hv = NAT_HASH_FN(dst, 0, ipf_nattable_sz);
	if (!rw) {
		WRITE_ENTER(&ipf_nat);
	}
	nat = nat_table[1][hv];
	for (; nat; nat = nat->nat_hnext[1]) {
		nflags = nat->nat_flags;
		if (ifp && ifp != nat->nat_ifp)
			continue;
		if (!(nflags & IPN_TCPUDP))
			continue;
		if (!(nflags & FI_WILDP))
			continue;
		if (nat->nat_oip.s_addr != src.s_addr ||
		    nat->nat_outip.s_addr != dst)
			continue;
		if (((nat->nat_oport == sport) || (nflags & FI_W_DPORT)) &&
		    ((nat->nat_outport == dport) || (nflags & FI_W_SPORT))) {
			nat_tabmove(nat, ports);
			break;
		}
	}
	if (!rw) {
		MUTEX_DOWNGRADE(&ipf_nat);
	}
	return nat;
}


/*
 * This function is only called for TCP/UDP NAT table entries where the
 * original was placed in the table without hashing on the ports and we now
 * want to include hashing on port numbers.
 */
static void nat_tabmove(nat, ports)
nat_t *nat;
u_32_t ports;
{
	register u_short sport, dport;
	nat_t **natp;
	u_int hv;

	dport = ports >> 16;
	sport = ports & 0xffff;

	if (nat->nat_oport == dport) {
		nat->nat_inport = sport;
		nat->nat_outport = sport;
	}

	/*
	 * Remove the NAT entry from the old location
	 */
	if (nat->nat_hnext[0])
		nat->nat_hnext[0]->nat_phnext[0] = nat->nat_phnext[0];
	*nat->nat_phnext[0] = nat->nat_hnext[0];

	if (nat->nat_hnext[1])
		nat->nat_hnext[1]->nat_phnext[1] = nat->nat_phnext[1];
	*nat->nat_phnext[1] = nat->nat_hnext[1];

	/*
	 * Add into the NAT table in the new position
	 */
	hv = NAT_HASH_FN(nat->nat_inip.s_addr, sport, ipf_nattable_sz);
	natp = &nat_table[0][hv];
	if (*natp)
		(*natp)->nat_phnext[0] = &nat->nat_hnext[0];
	nat->nat_phnext[0] = natp;
	nat->nat_hnext[0] = *natp;
	*natp = nat;

	hv = NAT_HASH_FN(nat->nat_outip.s_addr, sport, ipf_nattable_sz);
	natp = &nat_table[1][hv];
	if (*natp)
		(*natp)->nat_phnext[1] = &nat->nat_hnext[1];
	nat->nat_phnext[1] = natp;
	nat->nat_hnext[1] = *natp;
	*natp = nat;
}


/*
 * Lookup a nat entry based on the source 'real' ip address/port and
 * destination address/port.  We use this lookup when sending a packet out,
 * we're looking for a table entry, based on the source address.
 * NOTE: THE PACKET BEING CHECKED (IF FOUND) HAS A MAPPING ALREADY.
 */
nat_t *nat_outlookup(ifp, flags, p, src, dst, ports, rw)
void *ifp;
register u_int flags, p;
struct in_addr src , dst;
u_32_t ports;
int rw;
{
	register u_short sport, dport;
	register nat_t *nat;
	register int nflags;
	u_32_t srcip;
	u_int hv;

	sport = ports & 0xffff;
	dport = ports >> 16;
	flags &= IPN_TCPUDP;
	srcip = src.s_addr;

	hv = NAT_HASH_FN(srcip, sport, ipf_nattable_sz);
	nat = nat_table[0][hv];
	for (; nat; nat = nat->nat_hnext[0]) {
		nflags = nat->nat_flags;

		if ((!ifp || ifp == nat->nat_ifp) &&
		    nat->nat_inip.s_addr == srcip &&
		    nat->nat_oip.s_addr == dst.s_addr &&
		    (((p == 0) && (flags == (nflags & IPN_TCPUDP)))
		     || (p == nat->nat_p)) && (!flags ||
		     ((nat->nat_inport == sport || nflags & FI_W_SPORT) &&
		      (nat->nat_oport == dport || nflags & FI_W_DPORT))))
			return nat;
	}
	if (!nat_stats.ns_wilds || !(flags & IPN_TCPUDP))
		return NULL;
	if (!rw) {
		RWLOCK_EXIT(&ipf_nat);
	}
	hv = NAT_HASH_FN(srcip, 0, ipf_nattable_sz);
	if (!rw) {
		WRITE_ENTER(&ipf_nat);
	}
	nat = nat_table[0][hv];
	for (; nat; nat = nat->nat_hnext[0]) {
		nflags = nat->nat_flags;
		if (ifp && ifp != nat->nat_ifp)
			continue;
		if (!(nflags & IPN_TCPUDP))
			continue;
		if (!(nflags & FI_WILDP))
			continue;
		if ((nat->nat_inip.s_addr != srcip) ||
		    (nat->nat_oip.s_addr != dst.s_addr))
			continue;
		if (((nat->nat_inport == sport) || (nflags & FI_W_SPORT)) &&
		    ((nat->nat_oport == dport) || (nflags & FI_W_DPORT))) {
			nat_tabmove(nat, ports);
			break;
		}
	}
	if (!rw) {
		MUTEX_DOWNGRADE(&ipf_nat);
	}
	return nat;
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
				 np->nl_outip, ports, 0))) {
		np->nl_realip = nat->nat_outip;
		np->nl_realport = nat->nat_outport;
	}
	return nat;
}


static int nat_match(fin, np, ip)
fr_info_t *fin;
ipnat_t *np;
ip_t *ip;
{
	frtuc_t *ft;

	if (ip->ip_v != 4)
		return 0;

	if (np->in_p && ip->ip_p != np->in_p)
		return 0;
	if (fin->fin_out) {
		if (!(np->in_redir & (NAT_MAP|NAT_MAPBLK)))
			return 0;
		if (((fin->fin_fi.fi_saddr & np->in_inmsk) != np->in_inip)
		    ^ ((np->in_flags & IPN_NOTSRC) != 0))
			return 0;
		if (((fin->fin_fi.fi_daddr & np->in_srcmsk) != np->in_srcip)
		    ^ ((np->in_flags & IPN_NOTDST) != 0))
			return 0;
	} else {
		if (!(np->in_redir & NAT_REDIRECT))
			return 0;
		if (((fin->fin_fi.fi_saddr & np->in_srcmsk) != np->in_srcip)
		    ^ ((np->in_flags & IPN_NOTSRC) != 0))
			return 0;
		if (((fin->fin_fi.fi_daddr & np->in_outmsk) != np->in_outip)
		    ^ ((np->in_flags & IPN_NOTDST) != 0))
			return 0;
	}

	ft = &np->in_tuc;
	if (!(fin->fin_fi.fi_fl & FI_TCPUDP) ||
	    (fin->fin_fi.fi_fl & FI_SHORT) || (ip->ip_off & IP_OFFMASK)) {
		if (ft->ftu_scmp || ft->ftu_dcmp)
			return 0;
		return 1;
	}

	return fr_tcpudpchk(ft, fin);
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
	u_short sport = 0, dport = 0, *csump = NULL;
	struct ifnet *ifp;
	int natadd = 1;
	frentry_t *fr;
	u_int nflags = 0, hv, msk;
	u_32_t iph;
	nat_t *nat;
	int i;

	if (nat_list == NULL || (fr_nat_lock))
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

	if ((ip->ip_p == IPPROTO_ICMP) &&
	    (nat = nat_icmp(ip, fin, &nflags, NAT_OUTBOUND)))
		;
	else if ((ip->ip_off & (IP_OFFMASK|IP_MF)) &&
	    (nat = ipfr_nat_knownfrag(ip, fin)))
		natadd = 0;
	else if ((nat = nat_outlookup(ifp, nflags, (u_int)ip->ip_p,
				      ip->ip_src, ip->ip_dst,
				      (dport << 16) | sport, 0))) {
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
			nat_stats.ns_wilds--;
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
		hv = NAT_HASH_FN(iph, 0, ipf_natrules_sz);
		for (np = nat_rules[hv]; np; np = np->in_mnext)
		{
			if ((np->in_ifp && (np->in_ifp != ifp)) ||
			    !np->in_space)
				continue;
			if ((np->in_flags & IPN_RF) &&
			    !(np->in_flags & nflags))
				continue;
			if (np->in_flags & IPN_FILTER) {
				if (!nat_match(fin, np, ip))
					continue;
			} else if ((ipa & np->in_inmsk) != np->in_inip)
				continue;
			if (np->in_redir & (NAT_MAP|NAT_MAPBLK)) {
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

	/*
	 * NOTE: ipf_nat must now only be held as a read lock
	 */
	if (nat) {
		np = nat->nat_ptr;
		if (natadd && (fin->fin_fi.fi_fl & FI_FRAG) &&
		    np && (np->in_flags & IPN_FRAG))
			ipfr_nat_newfrag(ip, fin, 0, nat);
		MUTEX_ENTER(&nat->nat_lock);
		nat->nat_age = fr_defnatage;
		nat->nat_bytes += ip->ip_len;
		nat->nat_pkts++;
		MUTEX_EXIT(&nat->nat_lock);

		/*
		 * Fix up checksums, not by recalculating them, but
		 * simply computing adjustments.
		 */
		if (nflags == IPN_ICMPERR) {
			u_32_t s1, s2, sumd;

			s1 = LONG_SUM(ntohl(ip->ip_src.s_addr));
			s2 = LONG_SUM(ntohl(nat->nat_outip.s_addr));
			CALC_SUMD(s1, s2, sumd);

			if (nat->nat_dir == NAT_OUTBOUND)
				fix_incksum(&ip->ip_sum, sumd);
			else
				fix_outcksum(&ip->ip_sum, sumd);
		}
#if SOLARIS || defined(__sgi)
		else {
			if (nat->nat_dir == NAT_OUTBOUND)
				fix_outcksum(&ip->ip_sum, nat->nat_ipsumd);
			else
				fix_incksum(&ip->ip_sum, nat->nat_ipsumd);
		}
#endif
		ip->ip_src = nat->nat_outip;

		if (!(ip->ip_off & IP_OFFMASK) &&
		    !(fin->fin_fi.fi_fl & FI_SHORT)) {

			if ((nat->nat_outport != 0) && (nflags & IPN_TCPUDP)) {
				tcp->th_sport = nat->nat_outport;
				fin->fin_data[0] = ntohs(tcp->th_sport);
			}

			if (ip->ip_p == IPPROTO_TCP) {
				csump = &tcp->th_sum;
				MUTEX_ENTER(&nat->nat_lock);
				fr_tcp_age(&nat->nat_age,
					   nat->nat_tcpstate, fin, 1);
				if (nat->nat_age < fr_defnaticmpage)
					nat->nat_age = fr_defnaticmpage;
#ifdef LARGE_NAT
				else if (nat->nat_age > fr_defnatage)
					nat->nat_age = fr_defnatage;
#endif
				/*
				 * Increase this because we may have
				 * "keep state" following this too and
				 * packet storms can occur if this is
				 * removed too quickly.
				 */
				if (nat->nat_age == fr_tcpclosed)
					nat->nat_age = fr_tcplastack;
				MUTEX_EXIT(&nat->nat_lock);
			} else if (ip->ip_p == IPPROTO_UDP) {
				udphdr_t *udp = (udphdr_t *)tcp;

				if (udp->uh_sum)
					csump = &udp->uh_sum;
			} else if (ip->ip_p == IPPROTO_ICMP) {
				nat->nat_age = fr_defnaticmpage;
			}

			if (csump) {
				if (nat->nat_dir == NAT_OUTBOUND)
					fix_outcksum(csump, nat->nat_sumd[1]);
				else
					fix_incksum(csump, nat->nat_sumd[1]);
			}
		}

		if ((np->in_apr != NULL) && (np->in_dport == 0 ||
		     (tcp != NULL && dport == np->in_dport))) {
			i = appr_check(ip, fin, nat);
			if (i == 0)
				i = 1;
		} else
			i = 1;
		ATOMIC_INCL(nat_stats.ns_mapped[1]);
		RWLOCK_EXIT(&ipf_nat);	/* READ */
		return i;
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

	if ((nat_list == NULL) || (ip->ip_v != 4) || (fr_nat_lock))
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

	if ((ip->ip_p == IPPROTO_ICMP) &&
	    (nat = nat_icmp(ip, fin, &nflags, NAT_INBOUND)))
		;
	else if ((ip->ip_off & (IP_OFFMASK|IP_MF)) &&
		 (nat = ipfr_nat_knownfrag(ip, fin)))
		natadd = 0;
	else if ((nat = nat_inlookup(fin->fin_ifp, nflags, (u_int)ip->ip_p,
				     ip->ip_src, in, (dport << 16) | sport,
				     0))) {
		nflags = nat->nat_flags;
		if ((nflags & (FI_W_SPORT|FI_W_DPORT)) != 0) {
			if ((nat->nat_oport != sport) && (nflags & FI_W_DPORT))
				nat->nat_oport = sport;
			else if ((nat->nat_outport != dport) &&
				 (nflags & FI_W_SPORT))
				nat->nat_outport = dport;
			nat->nat_flags &= ~(FI_W_SPORT|FI_W_DPORT);
			nflags = nat->nat_flags;
			nat_stats.ns_wilds--;
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
		hv = NAT_HASH_FN(iph, 0, ipf_rdrrules_sz);
		for (np = rdr_rules[hv]; np; np = np->in_rnext) {
			if ((np->in_ifp && (np->in_ifp != ifp)) ||
			    (np->in_p && (np->in_p != ip->ip_p)) ||
			    (np->in_flags && !(nflags & np->in_flags)))
				continue;
			if (np->in_flags & IPN_FILTER) {
				if (!nat_match(fin, np, ip))
					continue;
			} else if ((in.s_addr & np->in_outmsk) != np->in_outip)
				continue;
			if ((np->in_redir & NAT_REDIRECT) &&
			    (!np->in_pmin || (np->in_flags & IPN_FILTER) ||
			     ((ntohs(np->in_pmax) >= ntohs(dport)) &&
			      (ntohs(dport) >= ntohs(np->in_pmin)))))
				if ((nat = nat_new(np, ip, fin, nflags,
						    NAT_INBOUND))) {
					np->in_hits++;
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

	/*
	 * NOTE: ipf_nat must now only be held as a read lock
	 */
	if (nat) {
		np = nat->nat_ptr;
		fin->fin_fr = nat->nat_fr;
		if (natadd && (fin->fin_fi.fi_fl & FI_FRAG) &&
		    np && (np->in_flags & IPN_FRAG))
			ipfr_nat_newfrag(ip, fin, 0, nat);
		if ((np->in_apr != NULL) && (np->in_dport == 0 ||
		    (tcp != NULL && sport == np->in_dport))) {
			i = appr_check(ip, fin, nat);
			if (i == -1) {
				RWLOCK_EXIT(&ipf_nat);
				return i;
			}
		}

		MUTEX_ENTER(&nat->nat_lock);
		if (nflags != IPN_ICMPERR)
			nat->nat_age = fr_defnatage;

		nat->nat_bytes += ip->ip_len;
		nat->nat_pkts++;
		MUTEX_EXIT(&nat->nat_lock);
		ip->ip_dst = nat->nat_inip;
		fin->fin_fi.fi_daddr = nat->nat_inip.s_addr;

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
				MUTEX_ENTER(&nat->nat_lock);
				fr_tcp_age(&nat->nat_age,
					   nat->nat_tcpstate, fin, 0);
				if (nat->nat_age < fr_defnaticmpage)
					nat->nat_age = fr_defnaticmpage;
#ifdef LARGE_NAT
				else if (nat->nat_age > fr_defnatage)
					nat->nat_age = fr_defnatage;
#endif
				/*
				 * Increase this because we may have
				 * "keep state" following this too and
				 * packet storms can occur if this is
				 * removed too quickly.
				 */
				if (nat->nat_age == fr_tcpclosed)
					nat->nat_age = fr_tcplastack;
				MUTEX_EXIT(&nat->nat_lock);
			} else if (ip->ip_p == IPPROTO_UDP) {
				udphdr_t *udp = (udphdr_t *)tcp;

				if (udp->uh_sum)
					csump = &udp->uh_sum;
			} else if (ip->ip_p == IPPROTO_ICMP) {
				nat->nat_age = fr_defnaticmpage;
			}

			if (csump) {
				if (nat->nat_dir == NAT_OUTBOUND)
					fix_incksum(csump, nat->nat_sumd[0]);
				else
					fix_outcksum(csump, nat->nat_sumd[0]);
			}
		}
		ATOMIC_INCL(nat_stats.ns_mapped[0]);
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
	if (maptable != NULL) {
		KFREES(maptable, sizeof(hostmap_t *) * ipf_hostmap_sz);
		maptable = NULL;
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
			if (fr_ifpaddr(4, ifp2, &in) != -1)
				nat->nat_outip = in;
			sum2 = nat->nat_outip.s_addr;

			if (sum1 == sum2)
				continue;
			/*
			 * Readjust the checksum adjustment to take into
			 * account the new IP#.
			 */
			CALC_SUMD(sum1, sum2, sumd);
			/* XXX - dont change for TCP when solaris does
			 * hardware checksumming.
			 */
			sumd += nat->nat_sumd[0];
			nat->nat_sumd[0] = (sumd & 0xffff) + (sumd >> 16);
			nat->nat_sumd[1] = nat->nat_sumd[0];
		}

	for (n = nat_list; (n != NULL); n = n->in_next)
		if (n->in_ifp == ifp) {
			n->in_ifp = (void *)GETUNIT(n->in_ifname, 4);
			if (!n->in_ifp)
				n->in_ifp = (void *)-1;
		}
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
	natl.nl_p = nat->nat_p;
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
