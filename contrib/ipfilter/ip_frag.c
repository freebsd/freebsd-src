/*
 * (C)opyright 1993,1994,1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#if !defined(lint) && defined(LIBC_SCCS)
static	char	sccsid[] = "@(#)ip_frag.c	1.11 3/24/96 (C) 1993-1995 Darren Reed";
static	char	rcsid[] = "$Id: ip_frag.c,v 2.0.2.5 1997/04/02 12:23:21 darrenr Exp $";
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
#if defined(__FreeBSD__) && (__FreeBSD__ >= 3)
#include <sys/ioccom.h>
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
#include "ip_compat.h"
#include "ip_fil.h"
#include "ip_frag.h"
#include "ip_nat.h"
#include "ip_state.h"

ipfr_t	*ipfr_heads[IPFT_SIZE];
ipfrstat_t ipfr_stats;
u_long	ipfr_inuse = 0,
	fr_ipfrttl = 120;	/* 60 seconds */
#ifdef _KERNEL
extern	int	ipfr_timer_id;
#endif
#if	SOLARIS
# ifdef	_KERNEL
extern	kmutex_t	ipf_frag;
# else
#define	bcmp(a,b,c)	memcmp(a,b,c)
#define	bcopy(a,b,c)	memmove(b,a,c)
# endif
#endif

#ifdef __FreeBSD__
# if BSD < 199306
int ipfr_slowtimer __P((void));
# else
void ipfr_slowtimer __P((void));
# endif
#endif /* __FreeBSD__ */

ipfrstat_t *ipfr_fragstats()
{
	ipfr_stats.ifs_table = ipfr_heads;
	ipfr_stats.ifs_inuse = ipfr_inuse;
	return &ipfr_stats;
}


/*
 * add a new entry to the fragment cache, registering it as having come
 * through this box, with the result of the filter operation.
 */
int ipfr_newfrag(ip, fin, pass)
ip_t *ip;
fr_info_t *fin;
int pass;
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
	MUTEX_ENTER(&ipf_frag);
	for (fp = &ipfr_heads[idx]; (fr = *fp); fp = &fr->ipfr_next)
		if (!bcmp((char *)&frag.ipfr_src, (char *)&fr->ipfr_src,
			  IPFR_CMPSZ)) {
			ipfr_stats.ifs_exists++;
			MUTEX_EXIT(&ipf_frag);
			return -1;
		}

	KMALLOC(fr, ipfr_t *, sizeof(*fr));
	if (fr == NULL) {
		ipfr_stats.ifs_nomem++;
		MUTEX_EXIT(&ipf_frag);
		return -1;
	}
	if ((fr->ipfr_next = ipfr_heads[idx]))
		ipfr_heads[idx]->ipfr_prev = fr;
	fr->ipfr_prev = NULL;
	ipfr_heads[idx] = fr;
	bcopy((char *)&frag.ipfr_src, (char *)&fr->ipfr_src, IPFR_CMPSZ);
	fr->ipfr_ttl = fr_ipfrttl;
	fr->ipfr_pass = pass & ~(FR_LOGFIRST|FR_LOG);
	fr->ipfr_off = (ip->ip_off & 0x1fff) + (fin->fin_dlen >> 3);
	ipfr_stats.ifs_new++;
	ipfr_inuse++;
	MUTEX_EXIT(&ipf_frag);
	return 0;
}


/*
 * check the fragment cache to see if there is already a record of this packet
 * with its filter result known.
 */
int ipfr_knownfrag(ip, fin)
ip_t *ip;
fr_info_t *fin;
{
	ipfr_t	*f, frag;
	u_int	idx;
	int	ret;

	/*
	 * For fragments, we record protocol, packet id, TOS and both IP#'s
	 * (these should all be the same for all fragments of a packet).
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

	MUTEX_ENTER(&ipf_frag);
	for (f = ipfr_heads[idx]; f; f = f->ipfr_next)
		if (!bcmp((char *)&frag.ipfr_src, (char *)&f->ipfr_src,
			  IPFR_CMPSZ)) {
			u_short	atoff, off;

			if (f != ipfr_heads[idx]) {
				/*
				 * move fragment info. to the top of the list
				 * to speed up searches.
				 */
				if ((f->ipfr_prev->ipfr_next = f->ipfr_next))
					f->ipfr_next->ipfr_prev = f->ipfr_prev;
				f->ipfr_next = ipfr_heads[idx];
				ipfr_heads[idx]->ipfr_prev = f;
				f->ipfr_prev = NULL;
				ipfr_heads[idx] = f;
			}
			ret = f->ipfr_pass;
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
			MUTEX_EXIT(&ipf_frag);
			return ret;
		}
	MUTEX_EXIT(&ipf_frag);
	return 0;
}


/*
 * Free memory in use by fragment state info. kept.
 */
void ipfr_unload()
{
	ipfr_t	**fp, *fr;
	int	idx;
#if	!SOLARIS && defined(_KERNEL)
	int	s;
#endif

	MUTEX_ENTER(&ipf_frag);
	SPLNET(s);
	for (idx = IPFT_SIZE - 1; idx >= 0; idx--)
		for (fp = &ipfr_heads[idx]; (fr = *fp); ) {
			*fp = fr->ipfr_next;
			KFREE(fr);
		}
	SPLX(s);
	MUTEX_EXIT(&ipf_frag);
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
	int	s, idx;

	MUTEX_ENTER(&ipf_frag);
	SPLNET(s);

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
	SPLX(s);
# if	SOLARIS
	MUTEX_EXIT(&ipf_frag);
	fr_timeoutstate();
	ip_natexpire();
	ipfr_timer_id = timeout(ipfr_slowtimer, NULL, HZ/2);
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
