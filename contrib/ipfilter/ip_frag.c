/*
 * (C)opyright 1993,1994,1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#if !defined(lint) && defined(LIBC_SCCS)
static	char	sccsid[] = "@(#)ip_frag.c	1.11 3/24/96 (C) 1993-1995 Darren Reed";
static	char	rcsid[] = "$Id: ip_frag.c,v 2.0.2.10 1997/05/24 07:36:23 darrenr Exp $";
#endif

#if !defined(_KERNEL) && !defined(KERNEL)
# include <string.h>
# include <stdlib.h>
#endif
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/file.h>
#if defined(KERNEL) && (__FreeBSD_version >= 220000)
#include <sys/filio.h>
#include <sys/fcntl.h>
#else
#include <sys/ioctl.h>
#endif
#include <sys/uio.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#ifdef _KERNEL
# include <sys/systm.h>
#endif
#if !defined(__SVR4) && !defined(__svr4__)
# include <sys/mbuf.h>
#else
# include <sys/byteorder.h>
# include <sys/dditypes.h>
# include <sys/stream.h>
# include <sys/kmem.h>
#endif

#include <net/if.h>
#ifdef sun
#include <net/af.h>
#endif
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
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

ipfr_t	*ipfr_heads[IPFT_SIZE];
ipfr_t	*ipfr_nattab[IPFT_SIZE];
ipfrstat_t ipfr_stats;
u_long	ipfr_inuse = 0,
	fr_ipfrttl = 120;	/* 60 seconds */
#ifdef _KERNEL
extern	int	ipfr_timer_id;
#endif
#if	SOLARIS && defined(_KERNEL)
extern	kmutex_t	ipf_frag;
extern	kmutex_t	ipf_natfrag;
extern	kmutex_t	ipf_nat;
#endif


static ipfr_t *ipfr_new __P((ip_t *, fr_info_t *, int, ipfr_t **));
static ipfr_t *ipfr_lookup __P((ip_t *, fr_info_t *, ipfr_t **));


ipfrstat_t *ipfr_fragstats()
{
	ipfr_stats.ifs_table = ipfr_heads;
	ipfr_stats.ifs_nattab = ipfr_nattab;
	ipfr_stats.ifs_inuse = ipfr_inuse;
	return &ipfr_stats;
}


/*
 * add a new entry to the fragment cache, registering it as having come
 * through this box, with the result of the filter operation.
 */
static ipfr_t *ipfr_new(ip, fin, pass, table)
ip_t *ip;
fr_info_t *fin;
int pass;
ipfr_t *table[];
{
	ipfr_t	**fp, *fr, frag;
	u_int	idx;

	frag.ipfr_p = ip->ip_p;
	idx = ip->ip_p;
	frag.ipfr_id = ip->ip_id;
	idx += ip->ip_id;
	frag.ipfr_tos = ip->ip_tos;
	frag.ipfr_src.s_addr = ip->ip_src.s_addr;
	idx += ip->ip_src.s_addr;
	frag.ipfr_dst.s_addr = ip->ip_dst.s_addr;
	idx += ip->ip_dst.s_addr;
	idx *= 127;
	idx %= IPFT_SIZE;

	/*
	 * first, make sure it isn't already there...
	 */
	for (fp = &table[idx]; (fr = *fp); fp = &fr->ipfr_next)
		if (!bcmp((char *)&frag.ipfr_src, (char *)&fr->ipfr_src,
			  IPFR_CMPSZ)) {
			ipfr_stats.ifs_exists++;
			MUTEX_EXIT(&ipf_frag);
			return NULL;
		}

	/*
	 * allocate some memory, if possible, if not, just record that we
	 * failed to do so.
	 */
	KMALLOC(fr, ipfr_t *, sizeof(*fr));
	if (fr == NULL) {
		ipfr_stats.ifs_nomem++;
		MUTEX_EXIT(&ipf_frag);
		return NULL;
	}

	/*
	 * Instert the fragment into the fragment table, copy the struct used
	 * in the search using bcopy rather than reassign each field.
	 * Set the ttl to the default and mask out logging from "pass"
	 */
	if ((fr->ipfr_next = table[idx]))
		table[idx]->ipfr_prev = fr;
	fr->ipfr_prev = NULL;
	fr->ipfr_data = NULL;
	table[idx] = fr;
	bcopy((char *)&frag.ipfr_src, (char *)&fr->ipfr_src, IPFR_CMPSZ);
	fr->ipfr_ttl = fr_ipfrttl;
	fr->ipfr_pass = pass & ~(FR_LOGFIRST|FR_LOG);
	/*
	 * Compute the offset of the expected start of the next packet.
	 */
	fr->ipfr_off = (ip->ip_off & 0x1fff) + (fin->fin_dlen >> 3);
	ipfr_stats.ifs_new++;
	ipfr_inuse++;
	return fr;
}


int ipfr_newfrag(ip, fin, pass)
ip_t *ip;
fr_info_t *fin;
int pass;
{
	ipfr_t	*ipf;

	MUTEX_ENTER(&ipf_frag);
	ipf = ipfr_new(ip, fin, pass, ipfr_heads);
	MUTEX_EXIT(&ipf_frag);
	return ipf ? 0 : -1;
}


int ipfr_nat_newfrag(ip, fin, pass, nat)
ip_t *ip;
fr_info_t *fin;
int pass;
nat_t *nat;
{
	ipfr_t	*ipf;

	MUTEX_ENTER(&ipf_natfrag);
	if ((ipf = ipfr_new(ip, fin, pass, ipfr_nattab))) {
		ipf->ipfr_data = nat;
		nat->nat_frag = ipf;
	}
	MUTEX_EXIT(&ipf_natfrag);
	return ipf ? 0 : -1;
}


/*
 * check the fragment cache to see if there is already a record of this packet
 * with its filter result known.
 */
static ipfr_t *ipfr_lookup(ip, fin, table)
ip_t *ip;
fr_info_t *fin;
ipfr_t *table[];
{
	ipfr_t	*f, frag;
	u_int	idx;
	int	ret;

	/*
	 * For fragments, we record protocol, packet id, TOS and both IP#'s
	 * (these should all be the same for all fragments of a packet).
	 *
	 * build up a hash value to index the table with.
	 */
	frag.ipfr_p = ip->ip_p;
	idx = ip->ip_p;
	frag.ipfr_id = ip->ip_id;
	idx += ip->ip_id;
	frag.ipfr_tos = ip->ip_tos;
	frag.ipfr_src.s_addr = ip->ip_src.s_addr;
	idx += ip->ip_src.s_addr;
	frag.ipfr_dst.s_addr = ip->ip_dst.s_addr;
	idx += ip->ip_dst.s_addr;
	idx *= 127;
	idx %= IPFT_SIZE;

	/*
	 * check the table, careful to only compare the right amount of data
	 */
	for (f = table[idx]; f; f = f->ipfr_next)
		if (!bcmp((char *)&frag.ipfr_src, (char *)&f->ipfr_src,
			  IPFR_CMPSZ)) {
			u_short	atoff, off;

			if (f != table[idx]) {
				/*
				 * move fragment info. to the top of the list
				 * to speed up searches.
				 */
				if ((f->ipfr_prev->ipfr_next = f->ipfr_next))
					f->ipfr_next->ipfr_prev = f->ipfr_prev;
				f->ipfr_next = table[idx];
				table[idx]->ipfr_prev = f;
				f->ipfr_prev = NULL;
				table[idx] = f;
			}
			off = ip->ip_off;
			atoff = (off & 0x1fff) - (fin->fin_dlen >> 3);
			/*
			 * If we've follwed the fragments, and this is the
			 * last (in order), shrink expiration time.
			 */
			if (atoff == f->ipfr_off) {
				if (!(off & IP_MF))
					f->ipfr_ttl = 1;
				else
					f->ipfr_off = off;
			}
			ipfr_stats.ifs_hits++;
			return f;
		}
	return NULL;
}


/*
 * functional interface for normal lookups of the fragment cache
 */
nat_t *ipfr_nat_knownfrag(ip, fin)
ip_t *ip;
fr_info_t *fin;
{
	nat_t	*nat;
	ipfr_t	*ipf;

	MUTEX_ENTER(&ipf_natfrag);
	ipf = ipfr_lookup(ip, fin, ipfr_heads);
	nat = ipf ? ipf->ipfr_data : NULL;
	MUTEX_EXIT(&ipf_natfrag);
	return nat;
}


/*
 * functional interface for NAT lookups of the NAT fragment cache
 */
int ipfr_knownfrag(ip, fin)
ip_t *ip;
fr_info_t *fin;
{
	int	ret;
	ipfr_t	*ipf;

	MUTEX_ENTER(&ipf_frag);
	ipf = ipfr_lookup(ip, fin, ipfr_heads);
	ret = ipf ? ipf->ipfr_pass : 0;
	MUTEX_EXIT(&ipf_frag);
	return ret;
}


/*
 * Free memory in use by fragment state info. kept.
 */
void ipfr_unload()
{
	ipfr_t	**fp, *fr;
	nat_t	*nat;
	int	idx;
#if	!SOLARIS && defined(_KERNEL)
	int	s;
#endif

	SPLNET(s);
	MUTEX_ENTER(&ipf_frag);
	for (idx = IPFT_SIZE - 1; idx >= 0; idx--)
		for (fp = &ipfr_heads[idx]; (fr = *fp); ) {
			*fp = fr->ipfr_next;
			KFREE(fr);
		}
	MUTEX_EXIT(&ipf_frag);

	MUTEX_ENTER(&ipf_nat);
	MUTEX_ENTER(&ipf_natfrag);
	for (idx = IPFT_SIZE - 1; idx >= 0; idx--)
		for (fp = &ipfr_nattab[idx]; (fr = *fp); ) {
			*fp = fr->ipfr_next;
			if ((nat = (nat_t *)fr->ipfr_data)) {
				if (nat->nat_frag == fr)
					nat->nat_frag = NULL;
			}
			KFREE(fr);
		}
	MUTEX_EXIT(&ipf_natfrag);
	MUTEX_EXIT(&ipf_nat);
	SPLX(s);
}


#ifdef	_KERNEL
/*
 * Slowly expire held state for fragments.  Timeouts are set * in expectation
 * of this being called twice per second.
 */
# if (BSD >= 199306) || SOLARIS
void ipfr_slowtimer()
# else
int ipfr_slowtimer()
# endif
{
	ipfr_t	**fp, *fr;
	nat_t	*nat;
	int	s, idx;

	MUTEX_ENTER(&ipf_frag);
	SPLNET(s);

	/*
	 * Go through the entire table, looking for entries to expire,
	 * decreasing the ttl by one for each entry.  If it reaches 0,
	 * remove it from the chain and free it.
	 */
	for (idx = IPFT_SIZE - 1; idx >= 0; idx--)
		for (fp = &ipfr_heads[idx]; (fr = *fp); ) {
			--fr->ipfr_ttl;
			if (fr->ipfr_ttl == 0) {
				if (fr->ipfr_prev)
					fr->ipfr_prev->ipfr_next =
					     fr->ipfr_next;
				if (fr->ipfr_next)
					fr->ipfr_next->ipfr_prev =
					     fr->ipfr_prev;
				*fp = fr->ipfr_next;
				ipfr_stats.ifs_expire++;
				ipfr_inuse--;
				KFREE(fr);
			} else
				fp = &fr->ipfr_next;
		}
	MUTEX_EXIT(&ipf_frag);

	/*
	 * Same again for the NAT table, except that if the structure also
	 * still points to a NAT structure, and the NAT structure points back
	 * at the one to be free'd, NULL the reference from the NAT struct.
	 * NOTE: We need to grab both mutex's early, and in this order so as
	 * to prevent a deadlock if both try to expire at the same time.
	 */
	MUTEX_ENTER(&ipf_nat);
	MUTEX_ENTER(&ipf_natfrag);
	for (idx = IPFT_SIZE - 1; idx >= 0; idx--)
		for (fp = &ipfr_nattab[idx]; (fr = *fp); ) {
			--fr->ipfr_ttl;
			if (fr->ipfr_ttl == 0) {
				if (fr->ipfr_prev)
					fr->ipfr_prev->ipfr_next =
					     fr->ipfr_next;
				if (fr->ipfr_next)
					fr->ipfr_next->ipfr_prev =
					     fr->ipfr_prev;
				*fp = fr->ipfr_next;
				ipfr_stats.ifs_expire++;
				ipfr_inuse--;
				if ((nat = (nat_t *)fr->ipfr_data)) {
					if (nat->nat_frag == fr)
						nat->nat_frag = NULL;
				}
				KFREE(fr);
			} else
				fp = &fr->ipfr_next;
		}
	MUTEX_EXIT(&ipf_natfrag);
	MUTEX_EXIT(&ipf_nat);
	SPLX(s);
# if	SOLARIS
	fr_timeoutstate();
	ip_natexpire();
	ipfr_timer_id = timeout(ipfr_slowtimer, NULL, drv_usectohz(500000));
# else
	fr_timeoutstate();
	ip_natexpire();
	ip_slowtimo();
#  if BSD < 199306
	return 0;
#  endif
# endif
}
#endif /* defined(_KERNEL) */
