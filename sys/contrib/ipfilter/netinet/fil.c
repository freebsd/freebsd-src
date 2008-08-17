/*	$FreeBSD$	*/

/*
 * Copyright (C) 1993-2003 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#if defined(KERNEL) || defined(_KERNEL)
# undef KERNEL
# undef _KERNEL
# define        KERNEL	1
# define        _KERNEL	1
#endif
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#if defined(__NetBSD__)
# if (NetBSD >= 199905) && !defined(IPFILTER_LKM) && defined(_KERNEL)
#  if (__NetBSD_Version__ < 301000000)
#   include "opt_ipfilter_log.h"
#  else
#   include "opt_ipfilter.h"
#  endif
# endif
#endif
#if defined(_KERNEL) && defined(__FreeBSD_version) && \
    (__FreeBSD_version >= 220000)
# if (__FreeBSD_version >= 400000)
#  if !defined(IPFILTER_LKM)
#   include "opt_inet6.h"
#  endif
#  if (__FreeBSD_version == 400019)
#   define CSUM_DELAY_DATA
#  endif
#  if (__FreeBSD_version >= 800044)
#   include <sys/vimage.h>
#  endif
# endif
# include <sys/filio.h>
#else
# include <sys/ioctl.h>
#endif
#if (defined(__SVR4) || defined(__svr4__)) && defined(sun)
# include <sys/filio.h>
#endif
#if !defined(_AIX51)
# include <sys/fcntl.h>
#endif
#if defined(_KERNEL)
# include <sys/systm.h>
# include <sys/file.h>
#else
# include <stdio.h>
# include <string.h>
# include <stdlib.h>
# include <stddef.h>
# include <sys/file.h>
# define _KERNEL
# ifdef __OpenBSD__
struct file;
# endif
# include <sys/uio.h>
# undef _KERNEL
#endif
#if !defined(__SVR4) && !defined(__svr4__) && !defined(__hpux) && \
    !defined(linux)
# include <sys/mbuf.h>
#else
# if !defined(linux)
#  include <sys/byteorder.h>
# endif
# if (SOLARIS2 < 5) && defined(sun)
#  include <sys/dditypes.h>
# endif
#endif
#ifdef __hpux
# define _NET_ROUTE_INCLUDED
#endif
#if !defined(linux)
# include <sys/protosw.h>
#endif
#include <sys/socket.h>
#include <net/if.h>
#ifdef sun
# include <net/af.h>
#endif
#if !defined(_KERNEL) && (defined(__FreeBSD__) || defined(SOLARIS2))
# if (__FreeBSD_version >= 504000)
#  undef _RADIX_H_
# endif
# include "radix_ipf.h"
#endif
#ifdef __osf__
# include "radix_ipf.h"
#else
# include <net/route.h>
#endif
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#if !defined(linux)
# include <netinet/ip_var.h>
#endif
#if defined(__sgi) && defined(IFF_DRVRLOCK) /* IRIX 6 */
# include <sys/hashing.h>
# include <netinet/in_var.h>
#endif
#include <netinet/tcp.h>
#if (!defined(__sgi) && !defined(AIX)) || defined(_KERNEL)
# include <netinet/udp.h>
# include <netinet/ip_icmp.h>
#endif
#ifdef __hpux
# undef _NET_ROUTE_INCLUDED
#endif
#ifdef __osf__
# undef _RADIX_H_
#endif
#include "netinet/ip_compat.h"
#ifdef	USE_INET6
# include <netinet/icmp6.h>
# if !SOLARIS && defined(_KERNEL) && !defined(__osf__) && !defined(__hpux)
#  include <netinet6/in6_var.h>
# endif
#endif
#include <netinet/tcpip.h>
#include "netinet/ip_fil.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_frag.h"
#include "netinet/ip_state.h"
#include "netinet/ip_proxy.h"
#include "netinet/ip_auth.h"
#ifdef IPFILTER_SCAN
# include "netinet/ip_scan.h"
#endif
#ifdef IPFILTER_SYNC
# include "netinet/ip_sync.h"
#endif
#include "netinet/ip_pool.h"
#include "netinet/ip_htable.h"
#ifdef IPFILTER_COMPILED
# include "netinet/ip_rules.h"
#endif
#if defined(IPFILTER_BPF) && defined(_KERNEL)
# include <net/bpf.h>
#endif
#if defined(__FreeBSD_version) && (__FreeBSD_version >= 300000)
# include <sys/malloc.h>
# if defined(_KERNEL) && !defined(IPFILTER_LKM)
#  include "opt_ipfilter.h"
# endif
#endif
#include "netinet/ipl.h"
/* END OF INCLUDES */

#include <machine/in_cksum.h>

#if !defined(lint)
static const char sccsid[] = "@(#)fil.c	1.36 6/5/96 (C) 1993-2000 Darren Reed";
static const char rcsid[] = "@(#)$FreeBSD$";
/* static const char rcsid[] = "@(#)$Id: fil.c,v 2.243.2.125 2007/10/10 09:27:20 darrenr Exp $"; */
#endif

#ifndef	_KERNEL
# include "ipf.h"
# include "ipt.h"
# include "bpf-ipf.h"
extern	int	opts;
#endif /* _KERNEL */


fr_info_t	frcache[2][8];
struct	filterstats frstats[2];
struct	frentry	*ipfilter[2][2] = { { NULL, NULL }, { NULL, NULL } },
		*ipfilter6[2][2] = { { NULL, NULL }, { NULL, NULL } },
		*ipacct6[2][2] = { { NULL, NULL }, { NULL, NULL } },
		*ipacct[2][2] = { { NULL, NULL }, { NULL, NULL } },
		*ipnatrules[2][2] = { { NULL, NULL }, { NULL, NULL } };
struct	frgroup *ipfgroups[IPL_LOGSIZE][2];
char	ipfilter_version[] = IPL_VERSION;
int	fr_refcnt = 0;
/*
 * For fr_running:
 * 0 == loading, 1 = running, -1 = disabled, -2 = unloading
 */
int	fr_running = 0;
int	fr_flags = IPF_LOGGING;
int	fr_active = 0;
int	fr_control_forwarding = 0;
int	fr_update_ipid = 0;
u_short	fr_ip_id = 0;
int	fr_chksrc = 0;	/* causes a system crash if enabled */
int	fr_minttl = 4;
int	fr_icmpminfragmtu = 68;
u_long	fr_frouteok[2] = {0, 0};
u_long	fr_userifqs = 0;
u_long	fr_badcoalesces[2] = {0, 0};
u_char	ipf_iss_secret[32];
#if defined(IPFILTER_DEFAULT_BLOCK)
int	fr_pass = FR_BLOCK|FR_NOMATCH;
#else
int	fr_pass = (IPF_DEFAULT_PASS)|FR_NOMATCH;
#endif
int	fr_features = 0
#ifdef	IPFILTER_LKM
		| IPF_FEAT_LKM
#endif
#ifdef	IPFILTER_LOG
		| IPF_FEAT_LOG
#endif
#ifdef	IPFILTER_LOOKUP
		| IPF_FEAT_LOOKUP
#endif
#ifdef	IPFILTER_BPF
		| IPF_FEAT_BPF
#endif
#ifdef	IPFILTER_COMPILED
		| IPF_FEAT_COMPILED
#endif
#ifdef	IPFILTER_CKSUM
		| IPF_FEAT_CKSUM
#endif
#ifdef	IPFILTER_SYNC
		| IPF_FEAT_SYNC
#endif
#ifdef	IPFILTER_SCAN
		| IPF_FEAT_SCAN
#endif
#ifdef	USE_INET6
		| IPF_FEAT_IPV6
#endif
	;

static	INLINE int	fr_ipfcheck __P((fr_info_t *, frentry_t *, int));
static	int		fr_portcheck __P((frpcmp_t *, u_short *));
static	int		frflushlist __P((int, minor_t, int *, frentry_t **));
static	ipfunc_t	fr_findfunc __P((ipfunc_t));
static	frentry_t	*fr_firewall __P((fr_info_t *, u_32_t *));
static	int		fr_funcinit __P((frentry_t *fr));
static	INLINE void	frpr_ah __P((fr_info_t *));
static	INLINE void	frpr_esp __P((fr_info_t *));
static	INLINE void	frpr_gre __P((fr_info_t *));
static	INLINE void	frpr_udp __P((fr_info_t *));
static	INLINE void	frpr_tcp __P((fr_info_t *));
static	INLINE void	frpr_icmp __P((fr_info_t *));
static	INLINE void	frpr_ipv4hdr __P((fr_info_t *));
static	INLINE int	frpr_pullup __P((fr_info_t *, int));
static	INLINE void	frpr_short __P((fr_info_t *, int));
static	INLINE int	frpr_tcpcommon __P((fr_info_t *));
static	INLINE int	frpr_udpcommon __P((fr_info_t *));
static	int		fr_updateipid __P((fr_info_t *));
#ifdef	IPFILTER_LOOKUP
static	int		fr_grpmapinit __P((frentry_t *fr));
static	INLINE void	*fr_resolvelookup __P((u_int, u_int, i6addr_t *, lookupfunc_t *));
#endif
static	void		frsynclist __P((frentry_t *, void *));
static	ipftuneable_t	*fr_findtunebyname __P((const char *));
static	ipftuneable_t	*fr_findtunebycookie __P((void *, void **));
static	int		ipf_geniter __P((ipftoken_t *, ipfgeniter_t *));
static	int		ipf_frruleiter __P((void *, int, void *));
static	void		ipf_unlinktoken __P((ipftoken_t *));


/*
 * bit values for identifying presence of individual IP options
 * All of these tables should be ordered by increasing key value on the left
 * hand side to allow for binary searching of the array and include a trailer
 * with a 0 for the bitmask for linear searches to easily find the end with.
 */
const	struct	optlist	ipopts[20] = {
	{ IPOPT_NOP,	0x000001 },
	{ IPOPT_RR,	0x000002 },
	{ IPOPT_ZSU,	0x000004 },
	{ IPOPT_MTUP,	0x000008 },
	{ IPOPT_MTUR,	0x000010 },
	{ IPOPT_ENCODE,	0x000020 },
	{ IPOPT_TS,	0x000040 },
	{ IPOPT_TR,	0x000080 },
	{ IPOPT_SECURITY, 0x000100 },
	{ IPOPT_LSRR,	0x000200 },
	{ IPOPT_E_SEC,	0x000400 },
	{ IPOPT_CIPSO,	0x000800 },
	{ IPOPT_SATID,	0x001000 },
	{ IPOPT_SSRR,	0x002000 },
	{ IPOPT_ADDEXT,	0x004000 },
	{ IPOPT_VISA,	0x008000 },
	{ IPOPT_IMITD,	0x010000 },
	{ IPOPT_EIP,	0x020000 },
	{ IPOPT_FINN,	0x040000 },
	{ 0,		0x000000 }
};

#ifdef USE_INET6
struct optlist ip6exthdr[] = {
	{ IPPROTO_HOPOPTS,		0x000001 },
	{ IPPROTO_IPV6,			0x000002 },
	{ IPPROTO_ROUTING,		0x000004 },
	{ IPPROTO_FRAGMENT,		0x000008 },
	{ IPPROTO_ESP,			0x000010 },
	{ IPPROTO_AH,			0x000020 },
	{ IPPROTO_NONE,			0x000040 },
	{ IPPROTO_DSTOPTS,		0x000080 },
	{ IPPROTO_MOBILITY,		0x000100 },
	{ 0,				0 }
};
#endif

struct optlist tcpopts[] = {
	{ TCPOPT_NOP,			0x000001 },
	{ TCPOPT_MAXSEG,		0x000002 },
	{ TCPOPT_WINDOW,		0x000004 },
	{ TCPOPT_SACK_PERMITTED,	0x000008 },
	{ TCPOPT_SACK,			0x000010 },
	{ TCPOPT_TIMESTAMP,		0x000020 },
	{ 0,				0x000000 }
};

/*
 * bit values for identifying presence of individual IP security options
 */
const	struct	optlist	secopt[8] = {
	{ IPSO_CLASS_RES4,	0x01 },
	{ IPSO_CLASS_TOPS,	0x02 },
	{ IPSO_CLASS_SECR,	0x04 },
	{ IPSO_CLASS_RES3,	0x08 },
	{ IPSO_CLASS_CONF,	0x10 },
	{ IPSO_CLASS_UNCL,	0x20 },
	{ IPSO_CLASS_RES2,	0x40 },
	{ IPSO_CLASS_RES1,	0x80 }
};


/*
 * Table of functions available for use with call rules.
 */
static ipfunc_resolve_t fr_availfuncs[] = {
#ifdef	IPFILTER_LOOKUP
	{ "fr_srcgrpmap", fr_srcgrpmap, fr_grpmapinit },
	{ "fr_dstgrpmap", fr_dstgrpmap, fr_grpmapinit },
#endif
	{ "", NULL, NULL }
};


/*
 * The next section of code is a a collection of small routines that set
 * fields in the fr_info_t structure passed based on properties of the
 * current packet.  There are different routines for the same protocol
 * for each of IPv4 and IPv6.  Adding a new protocol, for which there
 * will "special" inspection for setup, is now more easily done by adding
 * a new routine and expanding the frpr_ipinit*() function rather than by
 * adding more code to a growing switch statement.
 */
#ifdef USE_INET6
static	INLINE int	frpr_ah6 __P((fr_info_t *));
static	INLINE void	frpr_esp6 __P((fr_info_t *));
static	INLINE void	frpr_gre6 __P((fr_info_t *));
static	INLINE void	frpr_udp6 __P((fr_info_t *));
static	INLINE void	frpr_tcp6 __P((fr_info_t *));
static	INLINE void	frpr_icmp6 __P((fr_info_t *));
static	INLINE int	frpr_ipv6hdr __P((fr_info_t *));
static	INLINE void	frpr_short6 __P((fr_info_t *, int));
static	INLINE int	frpr_hopopts6 __P((fr_info_t *));
static	INLINE int	frpr_mobility6 __P((fr_info_t *));
static	INLINE int	frpr_routing6 __P((fr_info_t *));
static	INLINE int	frpr_dstopts6 __P((fr_info_t *));
static	INLINE int	frpr_fragment6 __P((fr_info_t *));
static	INLINE int	frpr_ipv6exthdr __P((fr_info_t *, int, int));


/* ------------------------------------------------------------------------ */
/* Function:    frpr_short6                                                 */
/* Returns:     void                                                        */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* IPv6 Only                                                                */
/* This is function enforces the 'is a packet too short to be legit' rule   */
/* for IPv6 and marks the packet with FI_SHORT if so.  See function comment */
/* for frpr_short() for more details.                                       */
/* ------------------------------------------------------------------------ */
static INLINE void frpr_short6(fin, xmin)
fr_info_t *fin;
int xmin;
{

	if (fin->fin_dlen < xmin)
		fin->fin_flx |= FI_SHORT;
}


/* ------------------------------------------------------------------------ */
/* Function:    frpr_ipv6hdr                                                */
/* Returns:     int    - 0 = IPv6 packet intact, -1 = packet lost           */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* IPv6 Only                                                                */
/* Copy values from the IPv6 header into the fr_info_t struct and call the  */
/* per-protocol analyzer if it exists.  In validating the packet, a protocol*/
/* analyzer may pullup or free the packet itself so we need to be vigiliant */
/* of that possibility arising.                                             */
/* ------------------------------------------------------------------------ */
static INLINE int frpr_ipv6hdr(fin)
fr_info_t *fin;
{
	ip6_t *ip6 = (ip6_t *)fin->fin_ip;
	int p, go = 1, i, hdrcount;
	fr_ip_t *fi = &fin->fin_fi;

	fin->fin_off = 0;

	fi->fi_tos = 0;
	fi->fi_optmsk = 0;
	fi->fi_secmsk = 0;
	fi->fi_auth = 0;

	p = ip6->ip6_nxt;
	fi->fi_ttl = ip6->ip6_hlim;
	fi->fi_src.in6 = ip6->ip6_src;
	fi->fi_dst.in6 = ip6->ip6_dst;
	fin->fin_id = (u_short)(ip6->ip6_flow & 0xffff);

	hdrcount = 0;
	while (go && !(fin->fin_flx & (FI_BAD|FI_SHORT))) {
		switch (p)
		{
		case IPPROTO_UDP :
			frpr_udp6(fin);
			go = 0;
			break;

		case IPPROTO_TCP :
			frpr_tcp6(fin);
			go = 0;
			break;

		case IPPROTO_ICMPV6 :
			frpr_icmp6(fin);
			go = 0;
			break;

		case IPPROTO_GRE :
			frpr_gre6(fin);
			go = 0;
			break;

		case IPPROTO_HOPOPTS :
			p = frpr_hopopts6(fin);
			break;

		case IPPROTO_MOBILITY :
			p = frpr_mobility6(fin);
			break;

		case IPPROTO_DSTOPTS :
			p = frpr_dstopts6(fin);
			break;

		case IPPROTO_ROUTING :
			p = frpr_routing6(fin);
			break;

		case IPPROTO_AH :
			p = frpr_ah6(fin);
			break;

		case IPPROTO_ESP :
			frpr_esp6(fin);
			go = 0;
			break;

		case IPPROTO_IPV6 :
			for (i = 0; ip6exthdr[i].ol_bit != 0; i++)
				if (ip6exthdr[i].ol_val == p) {
					fin->fin_flx |= ip6exthdr[i].ol_bit;
					break;
				}
			go = 0;
			break;

		case IPPROTO_NONE :
			go = 0;
			break;

		case IPPROTO_FRAGMENT :
			p = frpr_fragment6(fin);
			if (fin->fin_off != 0)
				go = 0;
			break;

		default :
			go = 0;
			break;
		}
		hdrcount++;

		/*
		 * It is important to note that at this point, for the
		 * extension headers (go != 0), the entire header may not have
		 * been pulled up when the code gets to this point.  This is
		 * only done for "go != 0" because the other header handlers
		 * will all pullup their complete header.  The other indicator
		 * of an incomplete packet is that this was just an extension
		 * header.
		 */
		if ((go != 0) && (p != IPPROTO_NONE) &&
		    (frpr_pullup(fin, 0) == -1)) {
			p = IPPROTO_NONE;
			go = 0;
		}
	}
	fi->fi_p = p;

	/*
	 * Some of the above functions, like frpr_esp6(), can call fr_pullup
	 * and destroy whatever packet was here.  The caller of this function
	 * expects us to return -1 if there is a problem with fr_pullup.
	 */
	if (fin->fin_m == NULL)
		return -1;

	return 0;
}


/* ------------------------------------------------------------------------ */
/* Function:    frpr_ipv6exthdr                                             */
/* Returns:     int    - value of the next header or IPPROTO_NONE if error  */
/* Parameters:  fin(I)      - pointer to packet information                 */
/*              multiple(I) - flag indicating yes/no if multiple occurances */
/*                            of this extension header are allowed.         */
/*              proto(I)    - protocol number for this extension header     */
/*                                                                          */
/* IPv6 Only                                                                */
/* ------------------------------------------------------------------------ */
static INLINE int frpr_ipv6exthdr(fin, multiple, proto)
fr_info_t *fin;
int multiple, proto;
{
	struct ip6_ext *hdr;
	u_short shift;
	int i;

	fin->fin_flx |= FI_V6EXTHDR;

				/* 8 is default length of extension hdr */
	if ((fin->fin_dlen - 8) < 0) {
		fin->fin_flx |= FI_SHORT;
		return IPPROTO_NONE;
	}

	if (frpr_pullup(fin, 8) == -1)
		return IPPROTO_NONE;

	hdr = fin->fin_dp;
	switch (proto)
	{
	case IPPROTO_FRAGMENT :
		shift = 8;
		break;
	default :
		shift = 8 + (hdr->ip6e_len << 3);
		break;
	}

	if (shift > fin->fin_dlen) {	/* Nasty extension header length? */
		fin->fin_flx |= FI_BAD;
		return IPPROTO_NONE;
	}

	for (i = 0; ip6exthdr[i].ol_bit != 0; i++)
		if (ip6exthdr[i].ol_val == proto) {
			/*
			 * Most IPv6 extension headers are only allowed once.
			 */
			if ((multiple == 0) &&
			    ((fin->fin_optmsk & ip6exthdr[i].ol_bit) != 0))
				fin->fin_flx |= FI_BAD;
			else
				fin->fin_optmsk |= ip6exthdr[i].ol_bit;
			break;
		}

	fin->fin_exthdr = fin->fin_dp;
	fin->fin_dp = (char *)fin->fin_dp + shift;
	fin->fin_dlen -= shift;

	return hdr->ip6e_nxt;
}


/* ------------------------------------------------------------------------ */
/* Function:    frpr_hopopts6                                               */
/* Returns:     int    - value of the next header or IPPROTO_NONE if error  */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* IPv6 Only                                                                */
/* This is function checks pending hop by hop options extension header      */
/* ------------------------------------------------------------------------ */
static INLINE int frpr_hopopts6(fin)
fr_info_t *fin;
{
	return frpr_ipv6exthdr(fin, 0, IPPROTO_HOPOPTS);
}


/* ------------------------------------------------------------------------ */
/* Function:    frpr_mobility6                                              */
/* Returns:     int    - value of the next header or IPPROTO_NONE if error  */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* IPv6 Only                                                                */
/* This is function checks the IPv6 mobility extension header               */
/* ------------------------------------------------------------------------ */
static INLINE int frpr_mobility6(fin)
fr_info_t *fin;
{
	return frpr_ipv6exthdr(fin, 0, IPPROTO_MOBILITY);
}


/* ------------------------------------------------------------------------ */
/* Function:    frpr_routing6                                               */
/* Returns:     int    - value of the next header or IPPROTO_NONE if error  */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* IPv6 Only                                                                */
/* This is function checks pending routing extension header                 */
/* ------------------------------------------------------------------------ */
static INLINE int frpr_routing6(fin)
fr_info_t *fin;
{
	struct ip6_ext *hdr;

	if (frpr_ipv6exthdr(fin, 0, IPPROTO_ROUTING) == IPPROTO_NONE)
		return IPPROTO_NONE;
	hdr = fin->fin_exthdr;

	if ((hdr->ip6e_len & 1) != 0) {
		/*
		 * The routing header data is made up of 128 bit IPv6 addresses
		 * which means it must be a multiple of 2 lots of 8 in length.
		 */
		fin->fin_flx |= FI_BAD;
		/*
		 * Compensate for the changes made in frpr_ipv6exthdr()
		 */
		fin->fin_dlen += 8 + (hdr->ip6e_len << 3);
		fin->fin_dp = hdr;
		return IPPROTO_NONE;
	}

	return hdr->ip6e_nxt;
}


/* ------------------------------------------------------------------------ */
/* Function:    frpr_fragment6                                              */
/* Returns:     int    - value of the next header or IPPROTO_NONE if error  */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* IPv6 Only                                                                */
/* Examine the IPv6 fragment header and extract fragment offset information.*/
/*                                                                          */
/* We don't know where the transport layer header (or whatever is next is), */
/* as it could be behind destination options (amongst others).  Because     */
/* there is no fragment cache, there is no knowledge about whether or not an*/
/* upper layer header has been seen (or where it ends) and thus we are not  */
/* able to continue processing beyond this header with any confidence.      */
/* ------------------------------------------------------------------------ */
static INLINE int frpr_fragment6(fin)
fr_info_t *fin;
{
	struct ip6_frag *frag;
	int extoff;

	fin->fin_flx |= FI_FRAG;

	if (frpr_ipv6exthdr(fin, 0, IPPROTO_FRAGMENT) == IPPROTO_NONE)
		return IPPROTO_NONE;

	extoff = (char *)fin->fin_exthdr - (char *)fin->fin_dp;

	if (frpr_pullup(fin, sizeof(*frag)) == -1)
		return IPPROTO_NONE;

	fin->fin_exthdr = (char *)fin->fin_dp + extoff;
	frag = fin->fin_exthdr;
	/*
	 * Fragment but no fragmentation info set?  Bad packet...
	 */
	if (frag->ip6f_offlg == 0) {
		fin->fin_flx |= FI_BAD;
		return IPPROTO_NONE;
	}

	fin->fin_off = ntohs(frag->ip6f_offlg & IP6F_OFF_MASK);
	fin->fin_off <<= 3;
	if (fin->fin_off != 0)
		fin->fin_flx |= FI_FRAGBODY;

	fin->fin_dp = (char *)fin->fin_dp + sizeof(*frag);
	fin->fin_dlen -= sizeof(*frag);

	return frag->ip6f_nxt;
}


/* ------------------------------------------------------------------------ */
/* Function:    frpr_dstopts6                                               */
/* Returns:     int    - value of the next header or IPPROTO_NONE if error  */
/* Parameters:  fin(I) - pointer to packet information                      */
/*              nextheader(I) - stores next header value                    */
/*                                                                          */
/* IPv6 Only                                                                */
/* This is function checks pending destination options extension header     */
/* ------------------------------------------------------------------------ */
static INLINE int frpr_dstopts6(fin)
fr_info_t *fin;
{
	return frpr_ipv6exthdr(fin, 1, IPPROTO_DSTOPTS);
}


/* ------------------------------------------------------------------------ */
/* Function:    frpr_icmp6                                                  */
/* Returns:     void                                                        */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* IPv6 Only                                                                */
/* This routine is mainly concerned with determining the minimum valid size */
/* for an ICMPv6 packet.                                                    */
/* ------------------------------------------------------------------------ */
static INLINE void frpr_icmp6(fin)
fr_info_t *fin;
{
	int minicmpsz = sizeof(struct icmp6_hdr);
	struct icmp6_hdr *icmp6;

	if (frpr_pullup(fin, ICMP6ERR_MINPKTLEN - sizeof(ip6_t)) == -1)
		return;

	if (fin->fin_dlen > 1) {
		ip6_t *ip6;

		icmp6 = fin->fin_dp;

		fin->fin_data[0] = *(u_short *)icmp6;

		switch (icmp6->icmp6_type)
		{
		case ICMP6_ECHO_REPLY :
		case ICMP6_ECHO_REQUEST :
			minicmpsz = ICMP6ERR_MINPKTLEN - sizeof(ip6_t);
			break;
		case ICMP6_DST_UNREACH :
		case ICMP6_PACKET_TOO_BIG :
		case ICMP6_TIME_EXCEEDED :
		case ICMP6_PARAM_PROB :
			fin->fin_flx |= FI_ICMPERR;
			minicmpsz = ICMP6ERR_IPICMPHLEN - sizeof(ip6_t);
			if (fin->fin_plen < ICMP6ERR_IPICMPHLEN)
				break;

			if (M_LEN(fin->fin_m) < fin->fin_plen) {
				if (fr_coalesce(fin) != 1)
					return;
			}

			/*
			 * If the destination of this packet doesn't match the
			 * source of the original packet then this packet is
			 * not correct.
			 */
			icmp6 = fin->fin_dp;
			ip6 = (ip6_t *)((char *)icmp6 + ICMPERR_ICMPHLEN);
			if (IP6_NEQ(&fin->fin_fi.fi_dst,
				    (i6addr_t *)&ip6->ip6_src))
				fin->fin_flx |= FI_BAD;

			break;
		default :
			break;
		}
	}

	frpr_short6(fin, minicmpsz);
}


/* ------------------------------------------------------------------------ */
/* Function:    frpr_udp6                                                   */
/* Returns:     void                                                        */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* IPv6 Only                                                                */
/* Analyse the packet for IPv6/UDP properties.                              */
/* Is not expected to be called for fragmented packets.                     */
/* ------------------------------------------------------------------------ */
static INLINE void frpr_udp6(fin)
fr_info_t *fin;
{

	frpr_short6(fin, sizeof(struct udphdr));

	if (frpr_udpcommon(fin) == 0) {
		u_char p = fin->fin_p;

		fin->fin_p = IPPROTO_UDP;
		fr_checkv6sum(fin);
		fin->fin_p = p;
	}
}


/* ------------------------------------------------------------------------ */
/* Function:    frpr_tcp6                                                   */
/* Returns:     void                                                        */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* IPv6 Only                                                                */
/* Analyse the packet for IPv6/TCP properties.                              */
/* Is not expected to be called for fragmented packets.                     */
/* ------------------------------------------------------------------------ */
static INLINE void frpr_tcp6(fin)
fr_info_t *fin;
{

	frpr_short6(fin, sizeof(struct tcphdr));

	if (frpr_tcpcommon(fin) == 0) {
		u_char p = fin->fin_p;

		fin->fin_p = IPPROTO_TCP;
		fr_checkv6sum(fin);
		fin->fin_p = p;
	}
}


/* ------------------------------------------------------------------------ */
/* Function:    frpr_esp6                                                   */
/* Returns:     void                                                        */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* IPv6 Only                                                                */
/* Analyse the packet for ESP properties.                                   */
/* The minimum length is taken to be the SPI (32bits) plus a tail (32bits)  */
/* even though the newer ESP packets must also have a sequence number that  */
/* is 32bits as well, it is not possible(?) to determine the version from a */
/* simple packet header.                                                    */
/* ------------------------------------------------------------------------ */
static INLINE void frpr_esp6(fin)
fr_info_t *fin;
{

	frpr_short6(fin, sizeof(grehdr_t));

	(void) frpr_pullup(fin, 8);
}


/* ------------------------------------------------------------------------ */
/* Function:    frpr_ah6                                                    */
/* Returns:     void                                                        */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* IPv6 Only                                                                */
/* Analyse the packet for AH properties.                                    */
/* The minimum length is taken to be the combination of all fields in the   */
/* header being present and no authentication data (null algorithm used.)   */
/* ------------------------------------------------------------------------ */
static INLINE int frpr_ah6(fin)
fr_info_t *fin;
{
	authhdr_t *ah;

	frpr_short6(fin, 12);

	if (frpr_pullup(fin, sizeof(*ah)) == -1)
		return IPPROTO_NONE;

	ah = (authhdr_t *)fin->fin_dp;
	return ah->ah_next;
}


/* ------------------------------------------------------------------------ */
/* Function:    frpr_gre6                                                   */
/* Returns:     void                                                        */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* Analyse the packet for GRE properties.                                   */
/* ------------------------------------------------------------------------ */
static INLINE void frpr_gre6(fin)
fr_info_t *fin;
{
	grehdr_t *gre;

	frpr_short6(fin, sizeof(grehdr_t));

	if (frpr_pullup(fin, sizeof(grehdr_t)) == -1)
		return;

	gre = fin->fin_dp;
	if (GRE_REV(gre->gr_flags) == 1)
		fin->fin_data[0] = gre->gr_call;
}
#endif	/* USE_INET6 */


/* ------------------------------------------------------------------------ */
/* Function:    frpr_pullup                                                 */
/* Returns:     int     - 0 == pullup succeeded, -1 == failure              */
/* Parameters:  fin(I)  - pointer to packet information                     */
/*              plen(I) - length (excluding L3 header) to pullup            */
/*                                                                          */
/* Short inline function to cut down on code duplication to perform a call  */
/* to fr_pullup to ensure there is the required amount of data,             */
/* consecutively in the packet buffer.                                      */
/*                                                                          */
/* This function pulls up 'extra' data at the location of fin_dp.  fin_dp   */
/* points to the first byte after the complete layer 3 header, which will   */
/* include all of the known extension headers for IPv6 or options for IPv4. */
/*                                                                          */
/* Since fr_pullup() expects the total length of bytes to be pulled up, it  */
/* is necessary to add those we can already assume to be pulled up (fin_dp  */
/* - fin_ip) to what is passed through.                                     */
/* ------------------------------------------------------------------------ */
static INLINE int frpr_pullup(fin, plen)
fr_info_t *fin;
int plen;
{
	if (fin->fin_m != NULL) {
		if (fin->fin_dp != NULL)
			plen += (char *)fin->fin_dp -
				((char *)fin->fin_ip + fin->fin_hlen);
		plen += fin->fin_hlen;
		if (M_LEN(fin->fin_m) < plen) {
#if defined(_KERNEL)
			if (fr_pullup(fin->fin_m, fin, plen) == NULL)
				return -1;
#else
			/*
			 * Fake fr_pullup failing
			 */
			*fin->fin_mp = NULL;
			fin->fin_m = NULL;
			fin->fin_ip = NULL;
			return -1;
#endif
		}
	}
	return 0;
}


/* ------------------------------------------------------------------------ */
/* Function:    frpr_short                                                  */
/* Returns:     void                                                        */
/* Parameters:  fin(I)  - pointer to packet information                     */
/*              xmin(I) - minimum header size                               */
/*                                                                          */
/* Check if a packet is "short" as defined by xmin.  The rule we are        */
/* applying here is that the packet must not be fragmented within the layer */
/* 4 header.  That is, it must not be a fragment that has its offset set to */
/* start within the layer 4 header (hdrmin) or if it is at offset 0, the    */
/* entire layer 4 header must be present (min).                             */
/* ------------------------------------------------------------------------ */
static INLINE void frpr_short(fin, xmin)
fr_info_t *fin;
int xmin;
{

	if (fin->fin_off == 0) {
		if (fin->fin_dlen < xmin)
			fin->fin_flx |= FI_SHORT;
	} else if (fin->fin_off < xmin) {
		fin->fin_flx |= FI_SHORT;
	}
}


/* ------------------------------------------------------------------------ */
/* Function:    frpr_icmp                                                   */
/* Returns:     void                                                        */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* IPv4 Only                                                                */
/* Do a sanity check on the packet for ICMP (v4).  In nearly all cases,     */
/* except extrememly bad packets, both type and code will be present.       */
/* The expected minimum size of an ICMP packet is very much dependent on    */
/* the type of it.                                                          */
/*                                                                          */
/* XXX - other ICMP sanity checks?                                          */
/* ------------------------------------------------------------------------ */
static INLINE void frpr_icmp(fin)
fr_info_t *fin;
{
	int minicmpsz = sizeof(struct icmp);
	icmphdr_t *icmp;
	ip_t *oip;

	if (fin->fin_off != 0) {
		frpr_short(fin, ICMPERR_ICMPHLEN);
		return;
	}

	if (frpr_pullup(fin, ICMPERR_ICMPHLEN) == -1)
		return;

	if (fin->fin_dlen > 1) {
		icmp = fin->fin_dp;

		fin->fin_data[0] = *(u_short *)icmp;

		if (fin->fin_dlen >= 6)				/* ID field */
			fin->fin_data[1] = icmp->icmp_id;

		switch (icmp->icmp_type)
		{
		case ICMP_ECHOREPLY :
		case ICMP_ECHO :
		/* Router discovery messaes - RFC 1256 */
		case ICMP_ROUTERADVERT :
		case ICMP_ROUTERSOLICIT :
			minicmpsz = ICMP_MINLEN;
			break;
		/*
		 * type(1) + code(1) + cksum(2) + id(2) seq(2) +
		 * 3 * timestamp(3 * 4)
		 */
		case ICMP_TSTAMP :
		case ICMP_TSTAMPREPLY :
			minicmpsz = 20;
			break;
		/*
		 * type(1) + code(1) + cksum(2) + id(2) seq(2) +
		 * mask(4)
		 */
		case ICMP_MASKREQ :
		case ICMP_MASKREPLY :
			minicmpsz = 12;
			break;
		/*
		 * type(1) + code(1) + cksum(2) + id(2) seq(2) + ip(20+)
		 */
		case ICMP_UNREACH :
#ifdef icmp_nextmtu
			if (icmp->icmp_code == ICMP_UNREACH_NEEDFRAG) {
				if (icmp->icmp_nextmtu < fr_icmpminfragmtu)
					fin->fin_flx |= FI_BAD;
			}
#endif
		case ICMP_SOURCEQUENCH :
		case ICMP_REDIRECT :
		case ICMP_TIMXCEED :
		case ICMP_PARAMPROB :
			fin->fin_flx |= FI_ICMPERR;
			if (fr_coalesce(fin) != 1)
				return;
			/*
			 * ICMP error packets should not be generated for IP
			 * packets that are a fragment that isn't the first
			 * fragment.
			 */
			oip = (ip_t *)((char *)fin->fin_dp + ICMPERR_ICMPHLEN);
			if ((ntohs(oip->ip_off) & IP_OFFMASK) != 0)
				fin->fin_flx |= FI_BAD;

			/*
			 * If the destination of this packet doesn't match the
			 * source of the original packet then this packet is
			 * not correct.
			 */
			if (oip->ip_src.s_addr != fin->fin_daddr)
				fin->fin_flx |= FI_BAD;

			/*
			 * If the destination of this packet doesn't match the
			 * source of the original packet then this packet is
			 * not correct.
			 */
			if (oip->ip_src.s_addr != fin->fin_daddr)
				fin->fin_flx |= FI_BAD;
			break;
		default :
			break;
		}
	}

	frpr_short(fin, minicmpsz);

	if ((fin->fin_flx & FI_FRAG) == 0)
		fr_checkv4sum(fin);
}


/* ------------------------------------------------------------------------ */
/* Function:    frpr_tcpcommon                                              */
/* Returns:     int    - 0 = header ok, 1 = bad packet, -1 = buffer error   */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* TCP header sanity checking.  Look for bad combinations of TCP flags,     */
/* and make some checks with how they interact with other fields.           */
/* If compiled with IPFILTER_CKSUM, check to see if the TCP checksum is     */
/* valid and mark the packet as bad if not.                                 */
/* ------------------------------------------------------------------------ */
static INLINE int frpr_tcpcommon(fin)
fr_info_t *fin;
{
	int flags, tlen;
	tcphdr_t *tcp;

	fin->fin_flx |= FI_TCPUDP;
	if (fin->fin_off != 0)
		return 0;

	if (frpr_pullup(fin, sizeof(*tcp)) == -1)
		return -1;
	tcp = fin->fin_dp;

	if (fin->fin_dlen > 3) {
		fin->fin_sport = ntohs(tcp->th_sport);
		fin->fin_dport = ntohs(tcp->th_dport);
	}

	if ((fin->fin_flx & FI_SHORT) != 0)
		return 1;

	/*
	 * Use of the TCP data offset *must* result in a value that is at
	 * least the same size as the TCP header.
	 */
	tlen = TCP_OFF(tcp) << 2;
	if (tlen < sizeof(tcphdr_t)) {
		fin->fin_flx |= FI_BAD;
		return 1;
	}

	flags = tcp->th_flags;
	fin->fin_tcpf = tcp->th_flags;

	/*
	 * If the urgent flag is set, then the urgent pointer must
	 * also be set and vice versa.  Good TCP packets do not have
	 * just one of these set.
	 */
	if ((flags & TH_URG) != 0 && (tcp->th_urp == 0)) {
		fin->fin_flx |= FI_BAD;
#if 0
	} else if ((flags & TH_URG) == 0 && (tcp->th_urp != 0)) {
		/*
		 * Ignore this case (#if 0) as it shows up in "real"
		 * traffic with bogus values in the urgent pointer field.
		 */
		fin->fin_flx |= FI_BAD;
#endif
	} else if (((flags & (TH_SYN|TH_FIN)) != 0) &&
		   ((flags & (TH_RST|TH_ACK)) == TH_RST)) {
		/* TH_FIN|TH_RST|TH_ACK seems to appear "naturally" */
		fin->fin_flx |= FI_BAD;
#if 1
	} else if (((flags & TH_SYN) != 0) &&
		   ((flags & (TH_URG|TH_PUSH)) != 0)) {
		/*
		 * SYN with URG and PUSH set is not for normal TCP but it is
		 * possible(?) with T/TCP...but who uses T/TCP?
		 */
		fin->fin_flx |= FI_BAD;
#endif
	} else if (!(flags & TH_ACK)) {
		/*
		 * If the ack bit isn't set, then either the SYN or
		 * RST bit must be set.  If the SYN bit is set, then
		 * we expect the ACK field to be 0.  If the ACK is
		 * not set and if URG, PSH or FIN are set, consdier
		 * that to indicate a bad TCP packet.
		 */
		if ((flags == TH_SYN) && (tcp->th_ack != 0)) {
			/*
			 * Cisco PIX sets the ACK field to a random value.
			 * In light of this, do not set FI_BAD until a patch
			 * is available from Cisco to ensure that
			 * interoperability between existing systems is
			 * achieved.
			 */
			/*fin->fin_flx |= FI_BAD*/;
		} else if (!(flags & (TH_RST|TH_SYN))) {
			fin->fin_flx |= FI_BAD;
		} else if ((flags & (TH_URG|TH_PUSH|TH_FIN)) != 0) {
			fin->fin_flx |= FI_BAD;
		}
	}

	/*
	 * At this point, it's not exactly clear what is to be gained by
	 * marking up which TCP options are and are not present.  The one we
	 * are most interested in is the TCP window scale.  This is only in
	 * a SYN packet [RFC1323] so we don't need this here...?
	 * Now if we were to analyse the header for passive fingerprinting,
	 * then that might add some weight to adding this...
	 */
	if (tlen == sizeof(tcphdr_t))
		return 0;

	if (frpr_pullup(fin, tlen) == -1)
		return -1;

#if 0
	tcp = fin->fin_dp;
	ip = fin->fin_ip;
	s = (u_char *)(tcp + 1);
	off = IP_HL(ip) << 2;
# ifdef _KERNEL
	if (fin->fin_mp != NULL) {
		mb_t *m = *fin->fin_mp;

		if (off + tlen > M_LEN(m))
			return;
	}
# endif
	for (tlen -= (int)sizeof(*tcp); tlen > 0; ) {
		opt = *s;
		if (opt == '\0')
			break;
		else if (opt == TCPOPT_NOP)
			ol = 1;
		else {
			if (tlen < 2)
				break;
			ol = (int)*(s + 1);
			if (ol < 2 || ol > tlen)
				break;
		}

		for (i = 9, mv = 4; mv >= 0; ) {
			op = ipopts + i;
			if (opt == (u_char)op->ol_val) {
				optmsk |= op->ol_bit;
				break;
			}
		}
		tlen -= ol;
		s += ol;
	}
#endif /* 0 */

	return 0;
}



/* ------------------------------------------------------------------------ */
/* Function:    frpr_udpcommon                                              */
/* Returns:     int    - 0 = header ok, 1 = bad packet                      */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* Extract the UDP source and destination ports, if present.  If compiled   */
/* with IPFILTER_CKSUM, check to see if the UDP checksum is valid.          */
/* ------------------------------------------------------------------------ */
static INLINE int frpr_udpcommon(fin)
fr_info_t *fin;
{
	udphdr_t *udp;

	fin->fin_flx |= FI_TCPUDP;

	if (!fin->fin_off && (fin->fin_dlen > 3)) {
		if (frpr_pullup(fin, sizeof(*udp)) == -1) {
			fin->fin_flx |= FI_SHORT;
			return 1;
		}

		udp = fin->fin_dp;

		fin->fin_sport = ntohs(udp->uh_sport);
		fin->fin_dport = ntohs(udp->uh_dport);
	}

	return 0;
}


/* ------------------------------------------------------------------------ */
/* Function:    frpr_tcp                                                    */
/* Returns:     void                                                        */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* IPv4 Only                                                                */
/* Analyse the packet for IPv4/TCP properties.                              */
/* ------------------------------------------------------------------------ */
static INLINE void frpr_tcp(fin)
fr_info_t *fin;
{

	frpr_short(fin, sizeof(tcphdr_t));

	if (frpr_tcpcommon(fin) == 0) {
		if ((fin->fin_flx & FI_FRAG) == 0)
			fr_checkv4sum(fin);
	}
}


/* ------------------------------------------------------------------------ */
/* Function:    frpr_udp                                                    */
/* Returns:     void                                                        */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* IPv4 Only                                                                */
/* Analyse the packet for IPv4/UDP properties.                              */
/* ------------------------------------------------------------------------ */
static INLINE void frpr_udp(fin)
fr_info_t *fin;
{

	frpr_short(fin, sizeof(udphdr_t));

	if (frpr_udpcommon(fin) == 0) {
		if ((fin->fin_flx & FI_FRAG) == 0)
			fr_checkv4sum(fin);
	}
}


/* ------------------------------------------------------------------------ */
/* Function:    frpr_esp                                                    */
/* Returns:     void                                                        */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* Analyse the packet for ESP properties.                                   */
/* The minimum length is taken to be the SPI (32bits) plus a tail (32bits)  */
/* even though the newer ESP packets must also have a sequence number that  */
/* is 32bits as well, it is not possible(?) to determine the version from a */
/* simple packet header.                                                    */
/* ------------------------------------------------------------------------ */
static INLINE void frpr_esp(fin)
fr_info_t *fin;
{

	if (fin->fin_off == 0) {
		frpr_short(fin, 8);
		(void) frpr_pullup(fin, 8);
	}

}


/* ------------------------------------------------------------------------ */
/* Function:    frpr_ah                                                     */
/* Returns:     void                                                        */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* Analyse the packet for AH properties.                                    */
/* The minimum length is taken to be the combination of all fields in the   */
/* header being present and no authentication data (null algorithm used.)   */
/* ------------------------------------------------------------------------ */
static INLINE void frpr_ah(fin)
fr_info_t *fin;
{
	authhdr_t *ah;
	int len;

	frpr_short(fin, sizeof(*ah));

	if (((fin->fin_flx & FI_SHORT) != 0) || (fin->fin_off != 0))
		return;

	if (frpr_pullup(fin, sizeof(*ah)) == -1)
		return;

	ah = (authhdr_t *)fin->fin_dp;

	len = (ah->ah_plen + 2) << 2;
	frpr_short(fin, len);
}


/* ------------------------------------------------------------------------ */
/* Function:    frpr_gre                                                    */
/* Returns:     void                                                        */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* Analyse the packet for GRE properties.                                   */
/* ------------------------------------------------------------------------ */
static INLINE void frpr_gre(fin)
fr_info_t *fin;
{
	grehdr_t *gre;

	frpr_short(fin, sizeof(*gre));

	if (fin->fin_off != 0)
		return;

	if (frpr_pullup(fin, sizeof(*gre)) == -1)
		return;

	if (fin->fin_off == 0) {
		gre = fin->fin_dp;
		if (GRE_REV(gre->gr_flags) == 1)
			fin->fin_data[0] = gre->gr_call;
	}
}


/* ------------------------------------------------------------------------ */
/* Function:    frpr_ipv4hdr                                                */
/* Returns:     void                                                        */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* IPv4 Only                                                                */
/* Analyze the IPv4 header and set fields in the fr_info_t structure.       */
/* Check all options present and flag their presence if any exist.          */
/* ------------------------------------------------------------------------ */
static INLINE void frpr_ipv4hdr(fin)
fr_info_t *fin;
{
	u_short optmsk = 0, secmsk = 0, auth = 0;
	int hlen, ol, mv, p, i;
	const struct optlist *op;
	u_char *s, opt;
	u_short off;
	fr_ip_t *fi;
	ip_t *ip;

	fi = &fin->fin_fi;
	hlen = fin->fin_hlen;

	ip = fin->fin_ip;
	p = ip->ip_p;
	fi->fi_p = p;
	fi->fi_tos = ip->ip_tos;
	fin->fin_id = ip->ip_id;
	off = ip->ip_off;

	/* Get both TTL and protocol */
	fi->fi_p = ip->ip_p;
	fi->fi_ttl = ip->ip_ttl;
#if 0
	(*(((u_short *)fi) + 1)) = (*(((u_short *)ip) + 4));
#endif

	/* Zero out bits not used in IPv6 address */
	fi->fi_src.i6[1] = 0;
	fi->fi_src.i6[2] = 0;
	fi->fi_src.i6[3] = 0;
	fi->fi_dst.i6[1] = 0;
	fi->fi_dst.i6[2] = 0;
	fi->fi_dst.i6[3] = 0;

	fi->fi_saddr = ip->ip_src.s_addr;
	fi->fi_daddr = ip->ip_dst.s_addr;

	/*
	 * set packet attribute flags based on the offset and
	 * calculate the byte offset that it represents.
	 */
	off &= IP_MF|IP_OFFMASK;
	if (off != 0) {
		int morefrag = off & IP_MF;

		fi->fi_flx |= FI_FRAG;
		off &= IP_OFFMASK;
		if (off != 0) {
			fin->fin_flx |= FI_FRAGBODY;
			off <<= 3;
			if ((off + fin->fin_dlen > 65535) || 
			    (fin->fin_dlen == 0) ||
			    ((morefrag != 0) && ((fin->fin_dlen & 7) != 0))) {
				/* 
				 * The length of the packet, starting at its
				 * offset cannot exceed 65535 (0xffff) as the 
				 * length of an IP packet is only 16 bits.
				 *
				 * Any fragment that isn't the last fragment
				 * must have a length greater than 0 and it
				 * must be an even multiple of 8.
				 */
				fi->fi_flx |= FI_BAD;
			}
		}
	}
	fin->fin_off = off;

	/*
	 * Call per-protocol setup and checking
	 */
	switch (p)
	{
	case IPPROTO_UDP :
		frpr_udp(fin);
		break;
	case IPPROTO_TCP :
		frpr_tcp(fin);
		break;
	case IPPROTO_ICMP :
		frpr_icmp(fin);
		break;
	case IPPROTO_AH :
		frpr_ah(fin);
		break;
	case IPPROTO_ESP :
		frpr_esp(fin);
		break;
	case IPPROTO_GRE :
		frpr_gre(fin);
		break;
	}

	ip = fin->fin_ip;
	if (ip == NULL)
		return;

	/*
	 * If it is a standard IP header (no options), set the flag fields
	 * which relate to options to 0.
	 */
	if (hlen == sizeof(*ip)) {
		fi->fi_optmsk = 0;
		fi->fi_secmsk = 0;
		fi->fi_auth = 0;
		return;
	}

	/*
	 * So the IP header has some IP options attached.  Walk the entire
	 * list of options present with this packet and set flags to indicate
	 * which ones are here and which ones are not.  For the somewhat out
	 * of date and obscure security classification options, set a flag to
	 * represent which classification is present.
	 */
	fi->fi_flx |= FI_OPTIONS;

	for (s = (u_char *)(ip + 1), hlen -= (int)sizeof(*ip); hlen > 0; ) {
		opt = *s;
		if (opt == '\0')
			break;
		else if (opt == IPOPT_NOP)
			ol = 1;
		else {
			if (hlen < 2)
				break;
			ol = (int)*(s + 1);
			if (ol < 2 || ol > hlen)
				break;
		}
		for (i = 9, mv = 4; mv >= 0; ) {
			op = ipopts + i;
			if ((opt == (u_char)op->ol_val) && (ol > 4)) {
				optmsk |= op->ol_bit;
				if (opt == IPOPT_SECURITY) {
					const struct optlist *sp;
					u_char	sec;
					int j, m;

					sec = *(s + 2);	/* classification */
					for (j = 3, m = 2; m >= 0; ) {
						sp = secopt + j;
						if (sec == sp->ol_val) {
							secmsk |= sp->ol_bit;
							auth = *(s + 3);
							auth *= 256;
							auth += *(s + 4);
							break;
						}
						if (sec < sp->ol_val)
							j -= m;
						else
							j += m;
						m--;
					}
				}
				break;
			}
			if (opt < op->ol_val)
				i -= mv;
			else
				i += mv;
			mv--;
		}
		hlen -= ol;
		s += ol;
	}

	/*
	 *
	 */
	if (auth && !(auth & 0x0100))
		auth &= 0xff00;
	fi->fi_optmsk = optmsk;
	fi->fi_secmsk = secmsk;
	fi->fi_auth = auth;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_makefrip                                                 */
/* Returns:     void                                                        */
/* Parameters:  hlen(I) - length of IP packet header                        */
/*              ip(I)   - pointer to the IP header                          */
/*              fin(IO) - pointer to packet information                     */
/*                                                                          */
/* Compact the IP header into a structure which contains just the info.     */
/* which is useful for comparing IP headers with and store this information */
/* in the fr_info_t structure pointer to by fin.  At present, it is assumed */
/* this function will be called with either an IPv4 or IPv6 packet.         */
/* ------------------------------------------------------------------------ */
int	fr_makefrip(hlen, ip, fin)
int hlen;
ip_t *ip;
fr_info_t *fin;
{
	int v;

	fin->fin_nat = NULL;
	fin->fin_state = NULL;
	fin->fin_depth = 0;
	fin->fin_hlen = (u_short)hlen;
	fin->fin_ip = ip;
	fin->fin_rule = 0xffffffff;
	fin->fin_group[0] = -1;
	fin->fin_group[1] = '\0';
	fin->fin_dp = (char *)ip + hlen;

	v = fin->fin_v;
	if (v == 4) {
		fin->fin_plen = ip->ip_len;
		fin->fin_dlen = fin->fin_plen - hlen;

		frpr_ipv4hdr(fin);
#ifdef	USE_INET6
	} else if (v == 6) {
		fin->fin_plen = ntohs(((ip6_t *)ip)->ip6_plen);
		fin->fin_dlen = fin->fin_plen;
		fin->fin_plen += hlen;

		if (frpr_ipv6hdr(fin) == -1)
			return -1;
#endif
	}
	if (fin->fin_ip == NULL)
		return -1;
	return 0;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_portcheck                                                */
/* Returns:     int - 1 == port matched, 0 == port match failed             */
/* Parameters:  frp(I) - pointer to port check `expression'                 */
/*              pop(I) - pointer to port number to evaluate                 */
/*                                                                          */
/* Perform a comparison of a port number against some other(s), using a     */
/* structure with compare information stored in it.                         */
/* ------------------------------------------------------------------------ */
static INLINE int fr_portcheck(frp, pop)
frpcmp_t *frp;
u_short *pop;
{
	u_short tup, po;
	int err = 1;

	tup = *pop;
	po = frp->frp_port;

	/*
	 * Do opposite test to that required and continue if that succeeds.
	 */
	switch (frp->frp_cmp)
	{
	case FR_EQUAL :
		if (tup != po) /* EQUAL */
			err = 0;
		break;
	case FR_NEQUAL :
		if (tup == po) /* NOTEQUAL */
			err = 0;
		break;
	case FR_LESST :
		if (tup >= po) /* LESSTHAN */
			err = 0;
		break;
	case FR_GREATERT :
		if (tup <= po) /* GREATERTHAN */
			err = 0;
		break;
	case FR_LESSTE :
		if (tup > po) /* LT or EQ */
			err = 0;
		break;
	case FR_GREATERTE :
		if (tup < po) /* GT or EQ */
			err = 0;
		break;
	case FR_OUTRANGE :
		if (tup >= po && tup <= frp->frp_top) /* Out of range */
			err = 0;
		break;
	case FR_INRANGE :
		if (tup <= po || tup >= frp->frp_top) /* In range */
			err = 0;
		break;
	case FR_INCRANGE :
		if (tup < po || tup > frp->frp_top) /* Inclusive range */
			err = 0;
		break;
	default :
		break;
	}
	return err;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_tcpudpchk                                                */
/* Returns:     int - 1 == protocol matched, 0 == check failed              */
/* Parameters:  fin(I) - pointer to packet information                      */
/*              ft(I)  - pointer to structure with comparison data          */
/*                                                                          */
/* Compares the current pcket (assuming it is TCP/UDP) information with a   */
/* structure containing information that we want to match against.          */
/* ------------------------------------------------------------------------ */
int fr_tcpudpchk(fin, ft)
fr_info_t *fin;
frtuc_t *ft;
{
	int err = 1;

	/*
	 * Both ports should *always* be in the first fragment.
	 * So far, I cannot find any cases where they can not be.
	 *
	 * compare destination ports
	 */
	if (ft->ftu_dcmp)
		err = fr_portcheck(&ft->ftu_dst, &fin->fin_dport);

	/*
	 * compare source ports
	 */
	if (err && ft->ftu_scmp)
		err = fr_portcheck(&ft->ftu_src, &fin->fin_sport);

	/*
	 * If we don't have all the TCP/UDP header, then how can we
	 * expect to do any sort of match on it ?  If we were looking for
	 * TCP flags, then NO match.  If not, then match (which should
	 * satisfy the "short" class too).
	 */
	if (err && (fin->fin_p == IPPROTO_TCP)) {
		if (fin->fin_flx & FI_SHORT)
			return !(ft->ftu_tcpf | ft->ftu_tcpfm);
		/*
		 * Match the flags ?  If not, abort this match.
		 */
		if (ft->ftu_tcpfm &&
		    ft->ftu_tcpf != (fin->fin_tcpf & ft->ftu_tcpfm)) {
			FR_DEBUG(("f. %#x & %#x != %#x\n", fin->fin_tcpf,
				 ft->ftu_tcpfm, ft->ftu_tcpf));
			err = 0;
		}
	}
	return err;
}



/* ------------------------------------------------------------------------ */
/* Function:    fr_ipfcheck                                                 */
/* Returns:     int - 0 == match, 1 == no match                             */
/* Parameters:  fin(I)     - pointer to packet information                  */
/*              fr(I)      - pointer to filter rule                         */
/*              portcmp(I) - flag indicating whether to attempt matching on */
/*                           TCP/UDP port data.                             */
/*                                                                          */
/* Check to see if a packet matches an IPFilter rule.  Checks of addresses, */
/* port numbers, etc, for "standard" IPFilter rules are all orchestrated in */
/* this function.                                                           */
/* ------------------------------------------------------------------------ */
static INLINE int fr_ipfcheck(fin, fr, portcmp)
fr_info_t *fin;
frentry_t *fr;
int portcmp;
{
	u_32_t	*ld, *lm, *lip;
	fripf_t *fri;
	fr_ip_t *fi;
	int i;

	fi = &fin->fin_fi;
	fri = fr->fr_ipf;
	lip = (u_32_t *)fi;
	lm = (u_32_t *)&fri->fri_mip;
	ld = (u_32_t *)&fri->fri_ip;

	/*
	 * first 32 bits to check coversion:
	 * IP version, TOS, TTL, protocol
	 */
	i = ((*lip & *lm) != *ld);
	FR_DEBUG(("0. %#08x & %#08x != %#08x\n",
		   ntohl(*lip), ntohl(*lm), ntohl(*ld)));
	if (i)
		return 1;

	/*
	 * Next 32 bits is a constructed bitmask indicating which IP options
	 * are present (if any) in this packet.
	 */
	lip++, lm++, ld++;
	i |= ((*lip & *lm) != *ld);
	FR_DEBUG(("1. %#08x & %#08x != %#08x\n",
		   ntohl(*lip), ntohl(*lm), ntohl(*ld)));
	if (i)
		return 1;

	lip++, lm++, ld++;
	/*
	 * Unrolled loops (4 each, for 32 bits) for address checks.
	 */
	/*
	 * Check the source address.
	 */
#ifdef	IPFILTER_LOOKUP
	if (fr->fr_satype == FRI_LOOKUP) {
		i = (*fr->fr_srcfunc)(fr->fr_srcptr, fi->fi_v, lip);
		if (i == -1)
			return 1;
		lip += 3;
		lm += 3;
		ld += 3;
	} else {
#endif
		i = ((*lip & *lm) != *ld);
		FR_DEBUG(("2a. %#08x & %#08x != %#08x\n",
			   ntohl(*lip), ntohl(*lm), ntohl(*ld)));
		if (fi->fi_v == 6) {
			lip++, lm++, ld++;
			i |= ((*lip & *lm) != *ld);
			FR_DEBUG(("2b. %#08x & %#08x != %#08x\n",
				   ntohl(*lip), ntohl(*lm), ntohl(*ld)));
			lip++, lm++, ld++;
			i |= ((*lip & *lm) != *ld);
			FR_DEBUG(("2c. %#08x & %#08x != %#08x\n",
				   ntohl(*lip), ntohl(*lm), ntohl(*ld)));
			lip++, lm++, ld++;
			i |= ((*lip & *lm) != *ld);
			FR_DEBUG(("2d. %#08x & %#08x != %#08x\n",
				   ntohl(*lip), ntohl(*lm), ntohl(*ld)));
		} else {
			lip += 3;
			lm += 3;
			ld += 3;
		}
#ifdef	IPFILTER_LOOKUP
	}
#endif
	i ^= (fr->fr_flags & FR_NOTSRCIP) >> 6;
	if (i)
		return 1;

	/*
	 * Check the destination address.
	 */
	lip++, lm++, ld++;
#ifdef	IPFILTER_LOOKUP
	if (fr->fr_datype == FRI_LOOKUP) {
		i = (*fr->fr_dstfunc)(fr->fr_dstptr, fi->fi_v, lip);
		if (i == -1)
			return 1;
		lip += 3;
		lm += 3;
		ld += 3;
	} else {
#endif
		i = ((*lip & *lm) != *ld);
		FR_DEBUG(("3a. %#08x & %#08x != %#08x\n",
			   ntohl(*lip), ntohl(*lm), ntohl(*ld)));
		if (fi->fi_v == 6) {
			lip++, lm++, ld++;
			i |= ((*lip & *lm) != *ld);
			FR_DEBUG(("3b. %#08x & %#08x != %#08x\n",
				   ntohl(*lip), ntohl(*lm), ntohl(*ld)));
			lip++, lm++, ld++;
			i |= ((*lip & *lm) != *ld);
			FR_DEBUG(("3c. %#08x & %#08x != %#08x\n",
				   ntohl(*lip), ntohl(*lm), ntohl(*ld)));
			lip++, lm++, ld++;
			i |= ((*lip & *lm) != *ld);
			FR_DEBUG(("3d. %#08x & %#08x != %#08x\n",
				   ntohl(*lip), ntohl(*lm), ntohl(*ld)));
		} else {
			lip += 3;
			lm += 3;
			ld += 3;
		}
#ifdef	IPFILTER_LOOKUP
	}
#endif
	i ^= (fr->fr_flags & FR_NOTDSTIP) >> 7;
	if (i)
		return 1;
	/*
	 * IP addresses matched.  The next 32bits contains:
	 * mast of old IP header security & authentication bits.
	 */
	lip++, lm++, ld++;
	i |= ((*lip & *lm) != *ld);
	FR_DEBUG(("4. %#08x & %#08x != %#08x\n",
		   *lip, *lm, *ld));

	/*
	 * Next we have 32 bits of packet flags.
	 */
	lip++, lm++, ld++;
	i |= ((*lip & *lm) != *ld);
	FR_DEBUG(("5. %#08x & %#08x != %#08x\n",
		   *lip, *lm, *ld));

	if (i == 0) {
		/*
		 * If a fragment, then only the first has what we're
		 * looking for here...
		 */
		if (portcmp) {
			if (!fr_tcpudpchk(fin, &fr->fr_tuc))
				i = 1;
		} else {
			if (fr->fr_dcmp || fr->fr_scmp ||
			    fr->fr_tcpf || fr->fr_tcpfm)
				i = 1;
			if (fr->fr_icmpm || fr->fr_icmp) {
				if (((fi->fi_p != IPPROTO_ICMP) &&
				     (fi->fi_p != IPPROTO_ICMPV6)) ||
				    fin->fin_off || (fin->fin_dlen < 2))
					i = 1;
				else if ((fin->fin_data[0] & fr->fr_icmpm) !=
					 fr->fr_icmp) {
					FR_DEBUG(("i. %#x & %#x != %#x\n",
						 fin->fin_data[0],
						 fr->fr_icmpm, fr->fr_icmp));
					i = 1;
				}
			}
		}
	}
	return i;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_scanlist                                                 */
/* Returns:     int - result flags of scanning filter list                  */
/* Parameters:  fin(I) - pointer to packet information                      */
/*              pass(I) - default result to return for filtering            */
/*                                                                          */
/* Check the input/output list of rules for a match to the current packet.  */
/* If a match is found, the value of fr_flags from the rule becomes the     */
/* return value and fin->fin_fr points to the matched rule.                 */
/*                                                                          */
/* This function may be called recusively upto 16 times (limit inbuilt.)    */
/* When unwinding, it should finish up with fin_depth as 0.                 */
/*                                                                          */
/* Could be per interface, but this gets real nasty when you don't have,    */
/* or can't easily change, the kernel source code to .                      */
/* ------------------------------------------------------------------------ */
int fr_scanlist(fin, pass)
fr_info_t *fin;
u_32_t pass;
{
	int rulen, portcmp, off, skip;
	struct frentry *fr, *fnext;
	u_32_t passt, passo;

	/*
	 * Do not allow nesting deeper than 16 levels.
	 */
	if (fin->fin_depth >= 16)
		return pass;

	fr = fin->fin_fr;

	/*
	 * If there are no rules in this list, return now.
	 */
	if (fr == NULL)
		return pass;

	skip = 0;
	portcmp = 0;
	fin->fin_depth++;
	fin->fin_fr = NULL;
	off = fin->fin_off;

	if ((fin->fin_flx & FI_TCPUDP) && (fin->fin_dlen > 3) && !off)
		portcmp = 1;

	for (rulen = 0; fr; fr = fnext, rulen++) {
		fnext = fr->fr_next;
		if (skip != 0) {
			FR_VERBOSE(("%d (%#x)\n", skip, fr->fr_flags));
			skip--;
			continue;
		}

		/*
		 * In all checks below, a null (zero) value in the
		 * filter struture is taken to mean a wildcard.
		 *
		 * check that we are working for the right interface
		 */
#ifdef	_KERNEL
		if (fr->fr_ifa && fr->fr_ifa != fin->fin_ifp)
			continue;
#else
		if (opts & (OPT_VERBOSE|OPT_DEBUG))
			printf("\n");
		FR_VERBOSE(("%c", FR_ISSKIP(pass) ? 's' :
				  FR_ISPASS(pass) ? 'p' :
				  FR_ISACCOUNT(pass) ? 'A' :
				  FR_ISAUTH(pass) ? 'a' :
				  (pass & FR_NOMATCH) ? 'n' :'b'));
		if (fr->fr_ifa && fr->fr_ifa != fin->fin_ifp)
			continue;
		FR_VERBOSE((":i"));
#endif

		switch (fr->fr_type)
		{
		case FR_T_IPF :
		case FR_T_IPF|FR_T_BUILTIN :
			if (fr_ipfcheck(fin, fr, portcmp))
				continue;
			break;
#if defined(IPFILTER_BPF)
		case FR_T_BPFOPC :
		case FR_T_BPFOPC|FR_T_BUILTIN :
		    {
			u_char *mc;

			if (*fin->fin_mp == NULL)
				continue;
			if (fin->fin_v != fr->fr_v)
				continue;
			mc = (u_char *)fin->fin_m;
			if (!bpf_filter(fr->fr_data, mc, fin->fin_plen, 0))
				continue;
			break;
		    }
#endif
		case FR_T_CALLFUNC|FR_T_BUILTIN :
		    {
			frentry_t *f;

			f = (*fr->fr_func)(fin, &pass);
			if (f != NULL)
				fr = f;
			else
				continue;
			break;
		    }
		default :
			break;
		}

		if ((fin->fin_out == 0) && (fr->fr_nattag.ipt_num[0] != 0)) {
			if (fin->fin_nattag == NULL)
				continue;
			if (fr_matchtag(&fr->fr_nattag, fin->fin_nattag) == 0)
				continue;
		}
		FR_VERBOSE(("=%s.%d *", fr->fr_group, rulen));

		passt = fr->fr_flags;

		/*
		 * Allowing a rule with the "keep state" flag set to match
		 * packets that have been tagged "out of window" by the TCP
		 * state tracking is foolish as the attempt to add a new
		 * state entry to the table will fail.
		 */
		if ((passt & FR_KEEPSTATE) && (fin->fin_flx & FI_OOW))
			continue;

		/*
		 * If the rule is a "call now" rule, then call the function
		 * in the rule, if it exists and use the results from that.
		 * If the function pointer is bad, just make like we ignore
		 * it, except for increasing the hit counter.
		 */
		if ((passt & FR_CALLNOW) != 0) {
			frentry_t *frs;

			ATOMIC_INC64(fr->fr_hits);
			if ((fr->fr_func != NULL) &&
			    (fr->fr_func == (ipfunc_t)-1))
				continue;

			frs = fin->fin_fr;
			fin->fin_fr = fr;
			fr = (*fr->fr_func)(fin, &passt);
			if (fr == NULL) {
				fin->fin_fr = frs;
				continue;
			}
			passt = fr->fr_flags;
		}
		fin->fin_fr = fr;

#ifdef  IPFILTER_LOG
		/*
		 * Just log this packet...
		 */
		if ((passt & FR_LOGMASK) == FR_LOG) {
			if (ipflog(fin, passt) == -1) {
				if (passt & FR_LOGORBLOCK) {
					passt &= ~FR_CMDMASK;
					passt |= FR_BLOCK|FR_QUICK;
				}
				ATOMIC_INCL(frstats[fin->fin_out].fr_skip);
			}
			ATOMIC_INCL(frstats[fin->fin_out].fr_pkl);
			fin->fin_flx |= FI_DONTCACHE;
		}
#endif /* IPFILTER_LOG */
		fr->fr_bytes += (U_QUAD_T)fin->fin_plen;
		passo = pass;
		if (FR_ISSKIP(passt))
			skip = fr->fr_arg;
		else if ((passt & FR_LOGMASK) != FR_LOG)
			pass = passt;
		if (passt & (FR_RETICMP|FR_FAKEICMP))
			fin->fin_icode = fr->fr_icode;
		FR_DEBUG(("pass %#x\n", pass));
		ATOMIC_INC64(fr->fr_hits);
		fin->fin_rule = rulen;
		(void) strncpy(fin->fin_group, fr->fr_group, FR_GROUPLEN);
		if (fr->fr_grp != NULL) {
			fin->fin_fr = *fr->fr_grp;
			passt = fr_scanlist(fin, pass);
			if (fin->fin_fr == NULL) {
				fin->fin_rule = rulen;
				(void) strncpy(fin->fin_group, fr->fr_group,
					       FR_GROUPLEN);
				fin->fin_fr = fr;
				passt = pass;
			}
			pass = passt;
		}

		if (passt & FR_QUICK) {
			/*
			 * Finally, if we've asked to track state for this
			 * packet, set it up.  Add state for "quick" rules
			 * here so that if the action fails we can consider
			 * the rule to "not match" and keep on processing
			 * filter rules.
			 */
			if ((pass & FR_KEEPSTATE) &&
			    !(fin->fin_flx & FI_STATE)) {
				int out = fin->fin_out;

				fin->fin_fr = fr;
				if (fr_addstate(fin, NULL, 0) != NULL) {
					ATOMIC_INCL(frstats[out].fr_ads);
				} else {
					ATOMIC_INCL(frstats[out].fr_bads);
					pass = passo;
					continue;
				}
			}
			break;
		}
	}
	fin->fin_depth--;
	return pass;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_acctpkt                                                  */
/* Returns:     frentry_t* - always returns NULL                            */
/* Parameters:  fin(I) - pointer to packet information                      */
/*              passp(IO) - pointer to current/new filter decision (unused) */
/*                                                                          */
/* Checks a packet against accounting rules, if there are any for the given */
/* IP protocol version.                                                     */
/*                                                                          */
/* N.B.: this function returns NULL to match the prototype used by other    */
/* functions called from the IPFilter "mainline" in fr_check().             */
/* ------------------------------------------------------------------------ */
frentry_t *fr_acctpkt(fin, passp)
fr_info_t *fin;
u_32_t *passp;
{
	char group[FR_GROUPLEN];
	frentry_t *fr, *frsave;
	u_32_t pass, rulen;

	passp = passp;
#ifdef	USE_INET6
	if (fin->fin_v == 6)
		fr = ipacct6[fin->fin_out][fr_active];
	else
#endif
		fr = ipacct[fin->fin_out][fr_active];

	if (fr != NULL) {
		frsave = fin->fin_fr;
		bcopy(fin->fin_group, group, FR_GROUPLEN);
		rulen = fin->fin_rule;
		fin->fin_fr = fr;
		pass = fr_scanlist(fin, FR_NOMATCH);
		if (FR_ISACCOUNT(pass)) {
			ATOMIC_INCL(frstats[0].fr_acct);
		}
		fin->fin_fr = frsave;
		bcopy(group, fin->fin_group, FR_GROUPLEN);
		fin->fin_rule = rulen;
	}
	return NULL;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_firewall                                                 */
/* Returns:     frentry_t* - returns pointer to matched rule, if no matches */
/*                           were found, returns NULL.                      */
/* Parameters:  fin(I) - pointer to packet information                      */
/*              passp(IO) - pointer to current/new filter decision (unused) */
/*                                                                          */
/* Applies an appropriate set of firewall rules to the packet, to see if    */
/* there are any matches.  The first check is to see if a match can be seen */
/* in the cache.  If not, then search an appropriate list of rules.  Once a */
/* matching rule is found, take any appropriate actions as defined by the   */
/* rule - except logging.                                                   */
/* ------------------------------------------------------------------------ */
static frentry_t *fr_firewall(fin, passp)
fr_info_t *fin;
u_32_t *passp;
{
	frentry_t *fr;
	fr_info_t *fc;
	u_32_t pass;
	int out;

	out = fin->fin_out;
	pass = *passp;

	/*
	 * If a packet is found in the auth table, then skip checking
	 * the access lists for permission but we do need to consider
	 * the result as if it were from the ACL's.
	 */
	fc = &frcache[out][CACHE_HASH(fin)];
	READ_ENTER(&ipf_frcache);
	if (!bcmp((char *)fin, (char *)fc, FI_CSIZE)) {
		/*
		 * copy cached data so we can unlock the mutexes earlier.
		 */
		bcopy((char *)fc, (char *)fin, FI_COPYSIZE);
		RWLOCK_EXIT(&ipf_frcache);
		ATOMIC_INCL(frstats[out].fr_chit);

		if ((fr = fin->fin_fr) != NULL) {
			ATOMIC_INC64(fr->fr_hits);
			pass = fr->fr_flags;
		}
	} else {
		RWLOCK_EXIT(&ipf_frcache);

#ifdef	USE_INET6
		if (fin->fin_v == 6)
			fin->fin_fr = ipfilter6[out][fr_active];
		else
#endif
			fin->fin_fr = ipfilter[out][fr_active];
		if (fin->fin_fr != NULL)
			pass = fr_scanlist(fin, fr_pass);

		if (((pass & FR_KEEPSTATE) == 0) &&
		    ((fin->fin_flx & FI_DONTCACHE) == 0)) {
			WRITE_ENTER(&ipf_frcache);
			bcopy((char *)fin, (char *)fc, FI_COPYSIZE);
			RWLOCK_EXIT(&ipf_frcache);
		}
		if ((pass & FR_NOMATCH)) {
			ATOMIC_INCL(frstats[out].fr_nom);
		}
		fr = fin->fin_fr;
	}

	/*
	 * Apply packets per second rate-limiting to a rule as required.
	 */
	if ((fr != NULL) && (fr->fr_pps != 0) &&
	    !ppsratecheck(&fr->fr_lastpkt, &fr->fr_curpps, fr->fr_pps)) {
		pass &= ~(FR_CMDMASK|FR_DUP|FR_RETICMP|FR_RETRST);
		pass |= FR_BLOCK;
		ATOMIC_INCL(frstats[out].fr_ppshit);
	}

	/*
	 * If we fail to add a packet to the authorization queue, then we
	 * drop the packet later.  However, if it was added then pretend
	 * we've dropped it already.
	 */
	if (FR_ISAUTH(pass)) {
		if (fr_newauth(fin->fin_m, fin) != 0) {
#ifdef	_KERNEL
			fin->fin_m = *fin->fin_mp = NULL;
#else
			;
#endif
			fin->fin_error = 0;
		} else
			fin->fin_error = ENOSPC;
	}

	if ((fr != NULL) && (fr->fr_func != NULL) &&
	    (fr->fr_func != (ipfunc_t)-1) && !(pass & FR_CALLNOW))
		(void) (*fr->fr_func)(fin, &pass);

	/*
	 * If a rule is a pre-auth rule, check again in the list of rules
	 * loaded for authenticated use.  It does not particulary matter
	 * if this search fails because a "preauth" result, from a rule,
	 * is treated as "not a pass", hence the packet is blocked.
	 */
	if (FR_ISPREAUTH(pass)) {
		if ((fin->fin_fr = ipauth) != NULL)
			pass = fr_scanlist(fin, fr_pass);
	}

	/*
	 * If the rule has "keep frag" and the packet is actually a fragment,
	 * then create a fragment state entry.
	 */
	if ((pass & (FR_KEEPFRAG|FR_KEEPSTATE)) == FR_KEEPFRAG) {
		if (fin->fin_flx & FI_FRAG) {
			if (fr_newfrag(fin, pass) == -1) {
				ATOMIC_INCL(frstats[out].fr_bnfr);
			} else {
				ATOMIC_INCL(frstats[out].fr_nfr);
			}
		} else {
			ATOMIC_INCL(frstats[out].fr_cfr);
		}
	}

	fr = fin->fin_fr;

	if (passp != NULL)
		*passp = pass;

	return fr;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_check                                                    */
/* Returns:     int -  0 == packet allowed through,                         */
/*              User space:                                                 */
/*                    -1 == packet blocked                                  */
/*                     1 == packet not matched                              */
/*                    -2 == requires authentication                         */
/*              Kernel:                                                     */
/*                   > 0 == filter error # for packet                       */
/* Parameters: ip(I)   - pointer to start of IPv4/6 packet                  */
/*             hlen(I) - length of header                                   */
/*             ifp(I)  - pointer to interface this packet is on             */
/*             out(I)  - 0 == packet going in, 1 == packet going out        */
/*             mp(IO)  - pointer to caller's buffer pointer that holds this */
/*                       IP packet.                                         */
/* Solaris & HP-UX ONLY :                                                   */
/*             qpi(I)  - pointer to STREAMS queue information for this      */
/*                       interface & direction.                             */
/*                                                                          */
/* fr_check() is the master function for all IPFilter packet processing.    */
/* It orchestrates: Network Address Translation (NAT), checking for packet  */
/* authorisation (or pre-authorisation), presence of related state info.,   */
/* generating log entries, IP packet accounting, routing of packets as      */
/* directed by firewall rules and of course whether or not to allow the     */
/* packet to be further processed by the kernel.                            */
/*                                                                          */
/* For packets blocked, the contents of "mp" will be NULL'd and the buffer  */
/* freed.  Packets passed may be returned with the pointer pointed to by    */
/* by "mp" changed to a new buffer.                                         */
/* ------------------------------------------------------------------------ */
int fr_check(ip, hlen, ifp, out
#if defined(_KERNEL) && defined(MENTAT)
, qif, mp)
void *qif;
#else
, mp)
#endif
mb_t **mp;
ip_t *ip;
int hlen;
void *ifp;
int out;
{
	/*
	 * The above really sucks, but short of writing a diff
	 */
	fr_info_t frinfo;
	fr_info_t *fin = &frinfo;
	u_32_t pass = fr_pass;
	frentry_t *fr = NULL;
	int v = IP_V(ip);
	mb_t *mc = NULL;
	mb_t *m;
	/*
	 * The first part of fr_check() deals with making sure that what goes
	 * into the filtering engine makes some sense.  Information about the
	 * the packet is distilled, collected into a fr_info_t structure and
	 * the an attempt to ensure the buffer the packet is in is big enough
	 * to hold all the required packet headers.
	 */
#ifdef	_KERNEL
# ifdef MENTAT
	qpktinfo_t *qpi = qif;

#  if !defined(_INET_IP_STACK_H)
	if ((u_int)ip & 0x3)
		return 2;
#  endif
# else
	SPL_INT(s);
# endif

	READ_ENTER(&ipf_global);

	if (fr_running <= 0) {
		RWLOCK_EXIT(&ipf_global);
		return 0;
	}

	bzero((char *)fin, sizeof(*fin));

# ifdef MENTAT
	if (qpi->qpi_flags & QF_GROUP)
		fin->fin_flx |= FI_MBCAST;
	m = qpi->qpi_m;
	fin->fin_qfm = m;
	fin->fin_qpi = qpi;
# else /* MENTAT */

	m = *mp;

#  if defined(M_MCAST)
	if ((m->m_flags & M_MCAST) != 0)
		fin->fin_flx |= FI_MBCAST|FI_MULTICAST;
#  endif
#  if defined(M_MLOOP)
	if ((m->m_flags & M_MLOOP) != 0)
		fin->fin_flx |= FI_MBCAST|FI_MULTICAST;
#  endif
#  if defined(M_BCAST)
	if ((m->m_flags & M_BCAST) != 0)
		fin->fin_flx |= FI_MBCAST|FI_BROADCAST;
#  endif
#  ifdef M_CANFASTFWD
	/*
	 * XXX For now, IP Filter and fast-forwarding of cached flows
	 * XXX are mutually exclusive.  Eventually, IP Filter should
	 * XXX get a "can-fast-forward" filter rule.
	 */
	m->m_flags &= ~M_CANFASTFWD;
#  endif /* M_CANFASTFWD */
#  ifdef CSUM_DELAY_DATA
	/*
	 * disable delayed checksums.
	 */
	if (m->m_pkthdr.csum_flags & CSUM_DELAY_DATA) {
		in_delayed_cksum(m);
		m->m_pkthdr.csum_flags &= ~CSUM_DELAY_DATA;
	}
#  endif /* CSUM_DELAY_DATA */
# endif /* MENTAT */
#else
	READ_ENTER(&ipf_global);

	bzero((char *)fin, sizeof(*fin));
	m = *mp;
#endif /* _KERNEL */

	fin->fin_v = v;
	fin->fin_m = m;
	fin->fin_ip = ip;
	fin->fin_mp = mp;
	fin->fin_out = out;
	fin->fin_ifp = ifp;
	fin->fin_error = ENETUNREACH;
	fin->fin_hlen = (u_short)hlen;
	fin->fin_dp = (char *)ip + hlen;

	fin->fin_ipoff = (char *)ip - MTOD(m, char *);

	SPL_NET(s);

#ifdef	USE_INET6
	if (v == 6) {
		ATOMIC_INCL(frstats[out].fr_ipv6);
		/*
		 * Jumbo grams are quite likely too big for internal buffer
		 * structures to handle comfortably, for now, so just drop
		 * them.
		 */
		if (((ip6_t *)ip)->ip6_plen == 0) {
			pass = FR_BLOCK|FR_NOMATCH;
			goto finished;
		}
	} else
#endif
	{
#if (defined(OpenBSD) && (OpenBSD >= 200311)) && defined(_KERNEL)
		ip->ip_len = ntohs(ip->ip_len);
		ip->ip_off = ntohs(ip->ip_off);
#endif
	}

	if (fr_makefrip(hlen, ip, fin) == -1) {
		pass = FR_BLOCK|FR_NOMATCH;
		goto finished;
	}

	/*
	 * For at least IPv6 packets, if a m_pullup() fails then this pointer
	 * becomes NULL and so we have no packet to free.
	 */
	if (*fin->fin_mp == NULL)
		goto finished;

	if (!out) {
		if (v == 4) {
#ifdef _KERNEL
			if (fr_chksrc && !fr_verifysrc(fin)) {
				ATOMIC_INCL(frstats[0].fr_badsrc);
				fin->fin_flx |= FI_BADSRC;
			}
#endif
			if (fin->fin_ip->ip_ttl < fr_minttl) {
				ATOMIC_INCL(frstats[0].fr_badttl);
				fin->fin_flx |= FI_LOWTTL;
			}
		}
#ifdef USE_INET6
		else  if (v == 6) {
			if (((ip6_t *)ip)->ip6_hlim < fr_minttl) {
				ATOMIC_INCL(frstats[0].fr_badttl);
				fin->fin_flx |= FI_LOWTTL;
			}
		}
#endif
	}

	if (fin->fin_flx & FI_SHORT) {
		ATOMIC_INCL(frstats[out].fr_short);
	}

	READ_ENTER(&ipf_mutex);

	/*
	 * Check auth now.  This, combined with the check below to see if apass
	 * is 0 is to ensure that we don't count the packet twice, which can
	 * otherwise occur when we reprocess it.  As it is, we only count it
	 * after it has no auth. table matchup.  This also stops NAT from
	 * occuring until after the packet has been auth'd.
	 */
	fr = fr_checkauth(fin, &pass);
	if (!out) {
		if (fr_checknatin(fin, &pass) == -1) {
			goto filterdone;
		}
	}
	if (!out)
		(void) fr_acctpkt(fin, NULL);

	if (fr == NULL) {
		if ((fin->fin_flx & (FI_FRAG|FI_BAD)) == FI_FRAG) {
			fr = fr_knownfrag(fin, &pass);
			/*
			 * Reset the keep state flag here so that we don't
			 * try and add a new state entry because of it, leading
			 * to a blocked packet because the add will fail.
			 */
			if (fr != NULL)
				pass &= ~FR_KEEPSTATE;
		}
		if (fr == NULL)
			fr = fr_checkstate(fin, &pass);
	}

	if ((pass & FR_NOMATCH) || (fr == NULL))
		fr = fr_firewall(fin, &pass);

	/*
	 * If we've asked to track state for this packet, set it up.
	 * Here rather than fr_firewall because fr_checkauth may decide
	 * to return a packet for "keep state"
	 */
	if ((pass & FR_KEEPSTATE) && (fin->fin_m != NULL) &&
	    !(fin->fin_flx & FI_STATE)) {
		if (fr_addstate(fin, NULL, 0) != NULL) {
			ATOMIC_INCL(frstats[out].fr_ads);
		} else {
			ATOMIC_INCL(frstats[out].fr_bads);
			if (FR_ISPASS(pass)) {
				pass &= ~FR_CMDMASK;
				pass |= FR_BLOCK;
			}
		}
	}

	fin->fin_fr = fr;

	/*
	 * Only count/translate packets which will be passed on, out the
	 * interface.
	 */
	if (out && FR_ISPASS(pass)) {
		(void) fr_acctpkt(fin, NULL);

		if (fr_checknatout(fin, &pass) == -1) {
			;
		} else if ((fr_update_ipid != 0) && (v == 4)) {
			if (fr_updateipid(fin) == -1) {
				ATOMIC_INCL(frstats[1].fr_ipud);
				pass &= ~FR_CMDMASK;
				pass |= FR_BLOCK;
			} else {
				ATOMIC_INCL(frstats[0].fr_ipud);
			}
		}
	}

filterdone:
#ifdef	IPFILTER_LOG
	if ((fr_flags & FF_LOGGING) || (pass & FR_LOGMASK)) {
		(void) fr_dolog(fin, &pass);
	}
#endif

	/*
	 * The FI_STATE flag is cleared here so that calling fr_checkstate
	 * will work when called from inside of fr_fastroute.  Although
	 * there is a similar flag, FI_NATED, for NAT, it does have the same
	 * impact on code execution.
	 */
	if (fin->fin_state != NULL) {
		fr_statederef((ipstate_t **)&fin->fin_state);
		fin->fin_flx ^= FI_STATE;
	}

	if (fin->fin_nat != NULL) {
		if (FR_ISBLOCK(pass) && (fin->fin_flx & FI_NEWNAT)) {
			WRITE_ENTER(&ipf_nat);
			nat_delete((nat_t *)fin->fin_nat, NL_DESTROY);
			RWLOCK_EXIT(&ipf_nat);
			fin->fin_nat = NULL;
		} else {
			fr_natderef((nat_t **)&fin->fin_nat);
		}
	}

	/*
	 * Up the reference on fr_lock and exit ipf_mutex.  fr_fastroute
	 * only frees up the lock on ipf_global and the generation of a
	 * packet below could cause a recursive call into IPFilter.
	 * Hang onto the filter rule just in case someone decides to remove
	 * or flush it in the meantime.
	 */
	if (fr != NULL) {
		MUTEX_ENTER(&fr->fr_lock);
		fr->fr_ref++;
		MUTEX_EXIT(&fr->fr_lock);
	}

	RWLOCK_EXIT(&ipf_mutex);

	if ((pass & FR_RETMASK) != 0) {
		/*
		 * Should we return an ICMP packet to indicate error
		 * status passing through the packet filter ?
		 * WARNING: ICMP error packets AND TCP RST packets should
		 * ONLY be sent in repsonse to incoming packets.  Sending them
		 * in response to outbound packets can result in a panic on
		 * some operating systems.
		 */
		if (!out) {
			if (pass & FR_RETICMP) {
				int dst;

				if ((pass & FR_RETMASK) == FR_FAKEICMP)
					dst = 1;
				else
					dst = 0;
				(void) fr_send_icmp_err(ICMP_UNREACH, fin, dst);
				ATOMIC_INCL(frstats[0].fr_ret);
			} else if (((pass & FR_RETMASK) == FR_RETRST) &&
				   !(fin->fin_flx & FI_SHORT)) {
				if (((fin->fin_flx & FI_OOW) != 0) ||
				    (fr_send_reset(fin) == 0)) {
					ATOMIC_INCL(frstats[1].fr_ret);
				}
			}

			/*
			 * When using return-* with auth rules, the auth code
			 * takes over disposing of this packet.
			 */
			if (FR_ISAUTH(pass) && (fin->fin_m != NULL)) {
				fin->fin_m = *fin->fin_mp = NULL;
			}
		} else {
			if (pass & FR_RETRST)
				fin->fin_error = ECONNRESET;
		}
	}

	/*
	 * If we didn't drop off the bottom of the list of rules (and thus
	 * the 'current' rule fr is not NULL), then we may have some extra
	 * instructions about what to do with a packet.
	 * Once we're finished return to our caller, freeing the packet if
	 * we are dropping it (* BSD ONLY *).
	 */
	if (fr != NULL) {
		frdest_t *fdp;

		fdp = &fr->fr_tifs[fin->fin_rev];

		if (!out && (pass & FR_FASTROUTE)) {
			/*
			 * For fastroute rule, no destioation interface defined
			 * so pass NULL as the frdest_t parameter
			 */
			(void) fr_fastroute(fin->fin_m, mp, fin, NULL);
			m = *mp = NULL;
		} else if ((fdp->fd_ifp != NULL) &&
			   (fdp->fd_ifp != (struct ifnet *)-1)) {
			/* this is for to rules: */
			(void) fr_fastroute(fin->fin_m, mp, fin, fdp);
			m = *mp = NULL;
		}

		/*
		 * Generate a duplicated packet.
		 */
		if ((pass & FR_DUP) != 0) {
			mc = M_DUPLICATE(fin->fin_m);
			if (mc != NULL)
				(void) fr_fastroute(mc, &mc, fin, &fr->fr_dif);
		}

		(void) fr_derefrule(&fr);
	}

finished:
	if (!FR_ISPASS(pass)) {
		ATOMIC_INCL(frstats[out].fr_block);
		if (*mp != NULL) {
			FREE_MB_T(*mp);
			m = *mp = NULL;
		}
	} else {
		ATOMIC_INCL(frstats[out].fr_pass);
#if defined(_KERNEL) && defined(__sgi)
		if ((fin->fin_hbuf != NULL) &&
		    (mtod(fin->fin_m, struct ip *) != fin->fin_ip)) {
			COPYBACK(fin->fin_m, 0, fin->fin_plen, fin->fin_hbuf);
		}
#endif
	}

	SPL_X(s);
	RWLOCK_EXIT(&ipf_global);

#ifdef _KERNEL
# if (defined(OpenBSD) && (OpenBSD >= 200311))
	if (FR_ISPASS(pass) && (v == 4)) {
		ip = fin->fin_ip;
		ip->ip_len = ntohs(ip->ip_len);
		ip->ip_off = ntohs(ip->ip_off);
	}
# endif
	return (FR_ISPASS(pass)) ? 0 : fin->fin_error;
#else /* _KERNEL */
	FR_VERBOSE(("fin_flx %#x pass %#x ", fin->fin_flx, pass));
	if ((pass & FR_NOMATCH) != 0)
		return 1;

	if ((pass & FR_RETMASK) != 0)
		switch (pass & FR_RETMASK)
		{
		case FR_RETRST :
			return 3;
		case FR_RETICMP :
			return 4;
		case FR_FAKEICMP :
			return 5;
		}

	switch (pass & FR_CMDMASK)
	{
	case FR_PASS :
		return 0;
	case FR_BLOCK :
		return -1;
	case FR_AUTH :
		return -2;
	case FR_ACCOUNT :
		return -3;
	case FR_PREAUTH :
		return -4;
	}
	return 2;
#endif /* _KERNEL */
}


#ifdef	IPFILTER_LOG
/* ------------------------------------------------------------------------ */
/* Function:    fr_dolog                                                    */
/* Returns:     frentry_t* - returns contents of fin_fr (no change made)    */
/* Parameters:  fin(I) - pointer to packet information                      */
/*              passp(IO) - pointer to current/new filter decision (unused) */
/*                                                                          */
/* Checks flags set to see how a packet should be logged, if it is to be    */
/* logged.  Adjust statistics based on its success or not.                  */
/* ------------------------------------------------------------------------ */
frentry_t *fr_dolog(fin, passp)
fr_info_t *fin;
u_32_t *passp;
{
	u_32_t pass;
	int out;

	out = fin->fin_out;
	pass = *passp;

	if ((fr_flags & FF_LOGNOMATCH) && (pass & FR_NOMATCH)) {
		pass |= FF_LOGNOMATCH;
		ATOMIC_INCL(frstats[out].fr_npkl);
		goto logit;
	} else if (((pass & FR_LOGMASK) == FR_LOGP) ||
	    (FR_ISPASS(pass) && (fr_flags & FF_LOGPASS))) {
		if ((pass & FR_LOGMASK) != FR_LOGP)
			pass |= FF_LOGPASS;
		ATOMIC_INCL(frstats[out].fr_ppkl);
		goto logit;
	} else if (((pass & FR_LOGMASK) == FR_LOGB) ||
		   (FR_ISBLOCK(pass) && (fr_flags & FF_LOGBLOCK))) {
		if ((pass & FR_LOGMASK) != FR_LOGB)
			pass |= FF_LOGBLOCK;
		ATOMIC_INCL(frstats[out].fr_bpkl);
logit:
		if (ipflog(fin, pass) == -1) {
			ATOMIC_INCL(frstats[out].fr_skip);

			/*
			 * If the "or-block" option has been used then
			 * block the packet if we failed to log it.
			 */
			if ((pass & FR_LOGORBLOCK) &&
			    FR_ISPASS(pass)) {
				pass &= ~FR_CMDMASK;
				pass |= FR_BLOCK;
			}
		}
		*passp = pass;
	}

	return fin->fin_fr;
}
#endif /* IPFILTER_LOG */


/* ------------------------------------------------------------------------ */
/* Function:    ipf_cksum                                                   */
/* Returns:     u_short - IP header checksum                                */
/* Parameters:  addr(I) - pointer to start of buffer to checksum            */
/*              len(I)  - length of buffer in bytes                         */
/*                                                                          */
/* Calculate the two's complement 16 bit checksum of the buffer passed.     */
/*                                                                          */
/* N.B.: addr should be 16bit aligned.                                      */
/* ------------------------------------------------------------------------ */
u_short ipf_cksum(addr, len)
u_short *addr;
int len;
{
	u_32_t sum = 0;

	for (sum = 0; len > 1; len -= 2)
		sum += *addr++;

	/* mop up an odd byte, if necessary */
	if (len == 1)
		sum += *(u_char *)addr;

	/*
	 * add back carry outs from top 16 bits to low 16 bits
	 */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	return (u_short)(~sum);
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_cksum                                                    */
/* Returns:     u_short - layer 4 checksum                                  */
/* Parameters:  m(I  )     - pointer to buffer holding packet               */
/*              ip(I)      - pointer to IP header                           */
/*              l4proto(I) - protocol to caclulate checksum for             */
/*              l4hdr(I)   - pointer to layer 4 header                      */
/*              l3len(I)   - length of layer 4 data plus layer 3 header     */
/*                                                                          */
/* Calculates the TCP checksum for the packet held in "m", using the data   */
/* in the IP header "ip" to seed it.                                        */
/*                                                                          */
/* NB: This function assumes we've pullup'd enough for all of the IP header */
/* and the TCP header.  We also assume that data blocks aren't allocated in */
/* odd sizes.                                                               */
/*                                                                          */
/* For IPv6, l3len excludes extension header size.                          */
/*                                                                          */
/* Expects ip_len to be in host byte order when called.                     */
/* ------------------------------------------------------------------------ */
u_short fr_cksum(m, ip, l4proto, l4hdr, l3len)
mb_t *m;
ip_t *ip;
int l4proto, l3len;
void *l4hdr;
{
	u_short *sp, slen, sumsave, l4hlen, *csump;
	u_int sum, sum2;
	int hlen;
#ifdef	USE_INET6
	ip6_t *ip6;
#endif

	csump = NULL;
	sumsave = 0;
	l4hlen = 0;
	sp = NULL;
	slen = 0;
	hlen = 0;
	sum = 0;

	/*
	 * Add up IP Header portion
	 */
#ifdef	USE_INET6
	if (IP_V(ip) == 4) {
#endif
		hlen = IP_HL(ip) << 2;
		slen = l3len - hlen;
		sum = htons((u_short)l4proto);
		sum += htons(slen);
		sp = (u_short *)&ip->ip_src;
		sum += *sp++;	/* ip_src */
		sum += *sp++;
		sum += *sp++;	/* ip_dst */
		sum += *sp++;
#ifdef	USE_INET6
	} else if (IP_V(ip) == 6) {
		ip6 = (ip6_t *)ip;
		hlen = sizeof(*ip6);
		slen = l3len - hlen;
		sum = htons((u_short)l4proto);
		sum += htons(slen);
		sp = (u_short *)&ip6->ip6_src;
		sum += *sp++;	/* ip6_src */
		sum += *sp++;
		sum += *sp++;
		sum += *sp++;
		sum += *sp++;
		sum += *sp++;
		sum += *sp++;
		sum += *sp++;
		sum += *sp++;	/* ip6_dst */
		sum += *sp++;
		sum += *sp++;
		sum += *sp++;
		sum += *sp++;
		sum += *sp++;
		sum += *sp++;
		sum += *sp++;
	}
#endif

	switch (l4proto)
	{
	case IPPROTO_UDP :
		csump = &((udphdr_t *)l4hdr)->uh_sum;
		l4hlen = sizeof(udphdr_t);
		break;

	case IPPROTO_TCP :
		csump = &((tcphdr_t *)l4hdr)->th_sum;
		l4hlen = sizeof(tcphdr_t);
		break;
	case IPPROTO_ICMP :
		csump = &((icmphdr_t *)l4hdr)->icmp_cksum;
		l4hlen = 4;
		sum = 0;
		break;
	default :
		break;
	}

	if (csump != NULL) {
		sumsave = *csump;
		*csump = 0;
	}

	l4hlen = l4hlen;	/* LINT */

#ifdef	_KERNEL
# ifdef MENTAT
	{
	void *rp = m->b_rptr;

	if ((unsigned char *)ip > m->b_rptr && (unsigned char *)ip < m->b_wptr)
		m->b_rptr = (u_char *)ip;
	sum2 = ip_cksum(m, hlen, sum);	/* hlen == offset */
	m->b_rptr = rp;
	sum2 = (u_short)(~sum2 & 0xffff);
	}
# else /* MENTAT */
#  if defined(BSD) || defined(sun)
#   if BSD >= 199103
	m->m_data += hlen;
#   else
	m->m_off += hlen;
#   endif
	m->m_len -= hlen;
	sum2 = in_cksum(m, slen);
	m->m_len += hlen;
#   if BSD >= 199103
	m->m_data -= hlen;
#   else
	m->m_off -= hlen;
#   endif
	/*
	 * Both sum and sum2 are partial sums, so combine them together.
	 */
	sum += ~sum2 & 0xffff;
	while (sum > 0xffff)
		sum = (sum & 0xffff) + (sum >> 16);
	sum2 = ~sum & 0xffff;
#  else /* defined(BSD) || defined(sun) */
{
	union {
		u_char	c[2];
		u_short	s;
	} bytes;
	u_short len = ip->ip_len;
#   if defined(__sgi)
	int add;
#   endif

	/*
	 * Add up IP Header portion
	 */
	if (sp != (u_short *)l4hdr)
		sp = (u_short *)l4hdr;

	switch (l4proto)
	{
	case IPPROTO_UDP :
		sum += *sp++;	/* sport */
		sum += *sp++;	/* dport */
		sum += *sp++;	/* udp length */
		sum += *sp++;	/* checksum */
		break;

	case IPPROTO_TCP :
		sum += *sp++;	/* sport */
		sum += *sp++;	/* dport */
		sum += *sp++;	/* seq */
		sum += *sp++;
		sum += *sp++;	/* ack */
		sum += *sp++;
		sum += *sp++;	/* off */
		sum += *sp++;	/* win */
		sum += *sp++;	/* checksum */
		sum += *sp++;	/* urp */
		break;
	case IPPROTO_ICMP :
		sum = *sp++;	/* type/code */
		sum += *sp++;	/* checksum */
		break;
	}

#   ifdef	__sgi
	/*
	 * In case we had to copy the IP & TCP header out of mbufs,
	 * skip over the mbuf bits which are the header
	 */
	if ((char *)ip != mtod(m, char *)) {
		hlen = (char *)sp - (char *)ip;
		while (hlen) {
			add = MIN(hlen, m->m_len);
			sp = (u_short *)(mtod(m, caddr_t) + add);
			hlen -= add;
			if (add == m->m_len) {
				m = m->m_next;
				if (!hlen) {
					if (!m)
						break;
					sp = mtod(m, u_short *);
				}
				PANIC((!m),("fr_cksum(1): not enough data"));
			}
		}
	}
#   endif

	len -= (l4hlen + hlen);
	if (len <= 0)
		goto nodata;

	while (len > 1) {
		if (((char *)sp - mtod(m, char *)) >= m->m_len) {
			m = m->m_next;
			PANIC((!m),("fr_cksum(2): not enough data"));
			sp = mtod(m, u_short *);
		}
		if (((char *)(sp + 1) - mtod(m, char *)) > m->m_len) {
			bytes.c[0] = *(u_char *)sp;
			m = m->m_next;
			PANIC((!m),("fr_cksum(3): not enough data"));
			sp = mtod(m, u_short *);
			bytes.c[1] = *(u_char *)sp;
			sum += bytes.s;
			sp = (u_short *)((u_char *)sp + 1);
		}
		if ((u_long)sp & 1) {
			bcopy((char *)sp++, (char *)&bytes.s, sizeof(bytes.s));
			sum += bytes.s;
		} else
			sum += *sp++;
		len -= 2;
	}

	if (len != 0)
		sum += ntohs(*(u_char *)sp << 8);
nodata:
	while (sum > 0xffff)
		sum = (sum & 0xffff) + (sum >> 16);
	sum2 = (u_short)(~sum & 0xffff);
}
#  endif /*  defined(BSD) || defined(sun) */
# endif /* MENTAT */
#else /* _KERNEL */
	/*
	 * Add up IP Header portion
	 */
	if (sp != (u_short *)l4hdr)
		sp = (u_short *)l4hdr;

	for (; slen > 1; slen -= 2)
	        sum += *sp++;
	if (slen)
		sum += ntohs(*(u_char *)sp << 8);
	while (sum > 0xffff)
		sum = (sum & 0xffff) + (sum >> 16);
	sum2 = (u_short)(~sum & 0xffff);
#endif /* _KERNEL */
	if (csump != NULL)
		*csump = sumsave;
	return sum2;
}


#if defined(_KERNEL) && ( ((BSD < 199103) && !defined(MENTAT)) || \
    defined(__sgi) ) && !defined(linux) && !defined(_AIX51)
/*
 * Copyright (c) 1982, 1986, 1988, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)uipc_mbuf.c	8.2 (Berkeley) 1/4/94
 * $Id: fil.c,v 2.243.2.125 2007/10/10 09:27:20 darrenr Exp $
 */
/*
 * Copy data from an mbuf chain starting "off" bytes from the beginning,
 * continuing for "len" bytes, into the indicated buffer.
 */
void
m_copydata(m, off, len, cp)
	mb_t *m;
	int off;
	int len;
	caddr_t cp;
{
	unsigned count;

	if (off < 0 || len < 0)
		panic("m_copydata");
	while (off > 0) {
		if (m == 0)
			panic("m_copydata");
		if (off < m->m_len)
			break;
		off -= m->m_len;
		m = m->m_next;
	}
	while (len > 0) {
		if (m == 0)
			panic("m_copydata");
		count = MIN(m->m_len - off, len);
		bcopy(mtod(m, caddr_t) + off, cp, count);
		len -= count;
		cp += count;
		off = 0;
		m = m->m_next;
	}
}


/*
 * Copy data from a buffer back into the indicated mbuf chain,
 * starting "off" bytes from the beginning, extending the mbuf
 * chain if necessary.
 */
void
m_copyback(m0, off, len, cp)
	struct	mbuf *m0;
	int off;
	int len;
	caddr_t cp;
{
	int mlen;
	struct mbuf *m = m0, *n;
	int totlen = 0;

	if (m0 == 0)
		return;
	while (off > (mlen = m->m_len)) {
		off -= mlen;
		totlen += mlen;
		if (m->m_next == 0) {
			n = m_getclr(M_DONTWAIT, m->m_type);
			if (n == 0)
				goto out;
			n->m_len = min(MLEN, len + off);
			m->m_next = n;
		}
		m = m->m_next;
	}
	while (len > 0) {
		mlen = min(m->m_len - off, len);
		bcopy(cp, off + mtod(m, caddr_t), (unsigned)mlen);
		cp += mlen;
		len -= mlen;
		mlen += off;
		off = 0;
		totlen += mlen;
		if (len == 0)
			break;
		if (m->m_next == 0) {
			n = m_get(M_DONTWAIT, m->m_type);
			if (n == 0)
				break;
			n->m_len = min(MLEN, len);
			m->m_next = n;
		}
		m = m->m_next;
	}
out:
#if 0
	if (((m = m0)->m_flags & M_PKTHDR) && (m->m_pkthdr.len < totlen))
		m->m_pkthdr.len = totlen;
#endif
	return;
}
#endif /* (_KERNEL) && ( ((BSD < 199103) && !MENTAT) || __sgi) */


/* ------------------------------------------------------------------------ */
/* Function:    fr_findgroup                                                */
/* Returns:     frgroup_t * - NULL = group not found, else pointer to group */
/* Parameters:  group(I) - group name to search for                         */
/*              unit(I)  - device to which this group belongs               */
/*              set(I)   - which set of rules (inactive/inactive) this is   */
/*              fgpp(O)  - pointer to place to store pointer to the pointer */
/*                         to where to add the next (last) group or where   */
/*                         to delete group from.                            */
/*                                                                          */
/* Search amongst the defined groups for a particular group number.         */
/* ------------------------------------------------------------------------ */
frgroup_t *fr_findgroup(group, unit, set, fgpp)
char *group;
minor_t unit;
int set;
frgroup_t ***fgpp;
{
	frgroup_t *fg, **fgp;

	/*
	 * Which list of groups to search in is dependent on which list of
	 * rules are being operated on.
	 */
	fgp = &ipfgroups[unit][set];

	while ((fg = *fgp) != NULL) {
		if (strncmp(group, fg->fg_name, FR_GROUPLEN) == 0)
			break;
		else
			fgp = &fg->fg_next;
	}
	if (fgpp != NULL)
		*fgpp = fgp;
	return fg;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_addgroup                                                 */
/* Returns:     frgroup_t * - NULL == did not create group,                 */
/*                            != NULL == pointer to the group               */
/* Parameters:  num(I)   - group number to add                              */
/*              head(I)  - rule pointer that is using this as the head      */
/*              flags(I) - rule flags which describe the type of rule it is */
/*              unit(I)  - device to which this group will belong to        */
/*              set(I)   - which set of rules (inactive/inactive) this is   */
/* Write Locks: ipf_mutex                                                   */
/*                                                                          */
/* Add a new group head, or if it already exists, increase the reference    */
/* count to it.                                                             */
/* ------------------------------------------------------------------------ */
frgroup_t *fr_addgroup(group, head, flags, unit, set)
char *group;
void *head;
u_32_t flags;
minor_t unit;
int set;
{
	frgroup_t *fg, **fgp;
	u_32_t gflags;

	if (group == NULL)
		return NULL;

	if (unit == IPL_LOGIPF && *group == '\0')
		return NULL;

	fgp = NULL;
	gflags = flags & FR_INOUT;

	fg = fr_findgroup(group, unit, set, &fgp);
	if (fg != NULL) {
		if (fg->fg_flags == 0)
			fg->fg_flags = gflags;
		else if (gflags != fg->fg_flags)
			return NULL;
		fg->fg_ref++;
		return fg;
	}
	KMALLOC(fg, frgroup_t *);
	if (fg != NULL) {
		fg->fg_head = head;
		fg->fg_start = NULL;
		fg->fg_next = *fgp;
		bcopy(group, fg->fg_name, FR_GROUPLEN);
		fg->fg_flags = gflags;
		fg->fg_ref = 1;
		*fgp = fg;
	}
	return fg;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_delgroup                                                 */
/* Returns:     Nil                                                         */
/* Parameters:  group(I) - group name to delete                             */
/*              unit(I)  - device to which this group belongs               */
/*              set(I)   - which set of rules (inactive/inactive) this is   */
/* Write Locks: ipf_mutex                                                   */
/*                                                                          */
/* Attempt to delete a group head.                                          */
/* Only do this when its reference count reaches 0.                         */
/* ------------------------------------------------------------------------ */
void fr_delgroup(group, unit, set)
char *group;
minor_t unit;
int set;
{
	frgroup_t *fg, **fgp;

	fg = fr_findgroup(group, unit, set, &fgp);
	if (fg == NULL)
		return;

	fg->fg_ref--;
	if (fg->fg_ref == 0) {
		*fgp = fg->fg_next;
		KFREE(fg);
	}
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_getrulen                                                 */
/* Returns:     frentry_t * - NULL == not found, else pointer to rule n     */
/* Parameters:  unit(I)  - device for which to count the rule's number      */
/*              flags(I) - which set of rules to find the rule in           */
/*              group(I) - group name                                       */
/*              n(I)     - rule number to find                              */
/*                                                                          */
/* Find rule # n in group # g and return a pointer to it.  Return NULl if   */
/* group # g doesn't exist or there are less than n rules in the group.     */
/* ------------------------------------------------------------------------ */
frentry_t *fr_getrulen(unit, group, n)
int unit;
char *group;
u_32_t n;
{
	frentry_t *fr;
	frgroup_t *fg;

	fg = fr_findgroup(group, unit, fr_active, NULL);
	if (fg == NULL)
		return NULL;
	for (fr = fg->fg_head; fr && n; fr = fr->fr_next, n--)
		;
	if (n != 0)
		return NULL;
	return fr;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_rulen                                                    */
/* Returns:     int - >= 0 - rule number, -1 == search failed               */
/* Parameters:  unit(I) - device for which to count the rule's number       */
/*              fr(I)   - pointer to rule to match                          */
/*                                                                          */
/* Return the number for a rule on a specific filtering device.             */
/* ------------------------------------------------------------------------ */
int fr_rulen(unit, fr)
int unit;
frentry_t *fr;
{
	frentry_t *fh;
	frgroup_t *fg;
	u_32_t n = 0;

	if (fr == NULL)
		return -1;
	fg = fr_findgroup(fr->fr_group, unit, fr_active, NULL);
	if (fg == NULL)
		return -1;
	for (fh = fg->fg_head; fh; n++, fh = fh->fr_next)
		if (fh == fr)
			break;
	if (fh == NULL)
		return -1;
	return n;
}


/* ------------------------------------------------------------------------ */
/* Function:    frflushlist                                                 */
/* Returns:     int - >= 0 - number of flushed rules                        */
/* Parameters:  set(I)   - which set of rules (inactive/inactive) this is   */
/*              unit(I)  - device for which to flush rules                  */
/*              flags(I) - which set of rules to flush                      */
/*              nfreedp(O) - pointer to int where flush count is stored     */
/*              listp(I)   - pointer to list to flush pointer               */
/* Write Locks: ipf_mutex                                                   */
/*                                                                          */
/* Recursively flush rules from the list, descending groups as they are     */
/* encountered.  if a rule is the head of a group and it has lost all its   */
/* group members, then also delete the group reference.  nfreedp is needed  */
/* to store the accumulating count of rules removed, whereas the returned   */
/* value is just the number removed from the current list.  The latter is   */
/* needed to correctly adjust reference counts on rules that define groups. */
/*                                                                          */
/* NOTE: Rules not loaded from user space cannot be flushed.                */
/* ------------------------------------------------------------------------ */
static int frflushlist(set, unit, nfreedp, listp)
int set;
minor_t unit;
int *nfreedp;
frentry_t **listp;
{
	int freed = 0;
	frentry_t *fp;

	while ((fp = *listp) != NULL) {
		if ((fp->fr_type & FR_T_BUILTIN) ||
		    !(fp->fr_flags & FR_COPIED)) {
			listp = &fp->fr_next;
			continue;
		}
		*listp = fp->fr_next;
		if (fp->fr_grp != NULL) {
			(void) frflushlist(set, unit, nfreedp, fp->fr_grp);
		}

		if (fp->fr_grhead != NULL) {
			fr_delgroup(fp->fr_grhead, unit, set);
			*fp->fr_grhead = '\0';
		}

		ASSERT(fp->fr_ref > 0);
		fp->fr_next = NULL;
		if (fr_derefrule(&fp) == 0)
			freed++;
	}
	*nfreedp += freed;
	return freed;
}


/* ------------------------------------------------------------------------ */
/* Function:    frflush                                                     */
/* Returns:     int - >= 0 - number of flushed rules                        */
/* Parameters:  unit(I)  - device for which to flush rules                  */
/*              flags(I) - which set of rules to flush                      */
/*                                                                          */
/* Calls flushlist() for all filter rules (accounting, firewall - both IPv4 */
/* and IPv6) as defined by the value of flags.                              */
/* ------------------------------------------------------------------------ */
int frflush(unit, proto, flags)
minor_t unit;
int proto, flags;
{
	int flushed = 0, set;

	WRITE_ENTER(&ipf_mutex);
	bzero((char *)frcache, sizeof(frcache));

	set = fr_active;
	if ((flags & FR_INACTIVE) == FR_INACTIVE)
		set = 1 - set;

	if (flags & FR_OUTQUE) {
		if (proto == 0 || proto == 6) {
			(void) frflushlist(set, unit,
			    &flushed, &ipfilter6[1][set]);
			(void) frflushlist(set, unit,
			    &flushed, &ipacct6[1][set]);
		}
		if (proto == 0 || proto == 4) {
			(void) frflushlist(set, unit,
			    &flushed, &ipfilter[1][set]);
			(void) frflushlist(set, unit,
			    &flushed, &ipacct[1][set]);
		}
	}
	if (flags & FR_INQUE) {
		if (proto == 0 || proto == 6) {
			(void) frflushlist(set, unit,
			    &flushed, &ipfilter6[0][set]);
			(void) frflushlist(set, unit,
			    &flushed, &ipacct6[0][set]);
		}
		if (proto == 0 || proto == 4) {
			(void) frflushlist(set, unit,
			    &flushed, &ipfilter[0][set]);
			(void) frflushlist(set, unit,
			    &flushed, &ipacct[0][set]);
		}
	}
	RWLOCK_EXIT(&ipf_mutex);

	if (unit == IPL_LOGIPF) {
		int tmp;

		tmp = frflush(IPL_LOGCOUNT, proto, flags);
		if (tmp >= 0)
			flushed += tmp;
	}
	return flushed;
}


/* ------------------------------------------------------------------------ */
/* Function:    memstr                                                      */
/* Returns:     char *  - NULL if failed, != NULL pointer to matching bytes */
/* Parameters:  src(I)  - pointer to byte sequence to match                 */
/*              dst(I)  - pointer to byte sequence to search                */
/*              slen(I) - match length                                      */
/*              dlen(I) - length available to search in                     */
/*                                                                          */
/* Search dst for a sequence of bytes matching those at src and extend for  */
/* slen bytes.                                                              */
/* ------------------------------------------------------------------------ */
char *memstr(src, dst, slen, dlen)
const char *src;
char *dst;
size_t slen, dlen;
{
	char *s = NULL;

	while (dlen >= slen) {
		if (bcmp(src, dst, slen) == 0) {
			s = dst;
			break;
		}
		dst++;
		dlen--;
	}
	return s;
}
/* ------------------------------------------------------------------------ */
/* Function:    fr_fixskip                                                  */
/* Returns:     Nil                                                         */
/* Parameters:  listp(IO)    - pointer to start of list with skip rule      */
/*              rp(I)        - rule added/removed with skip in it.          */
/*              addremove(I) - adjustment (-1/+1) to make to skip count,    */
/*                             depending on whether a rule was just added   */
/*                             or removed.                                  */
/*                                                                          */
/* Adjust all the rules in a list which would have skip'd past the position */
/* where we are inserting to skip to the right place given the change.      */
/* ------------------------------------------------------------------------ */
void fr_fixskip(listp, rp, addremove)
frentry_t **listp, *rp;
int addremove;
{
	int rules, rn;
	frentry_t *fp;

	rules = 0;
	for (fp = *listp; (fp != NULL) && (fp != rp); fp = fp->fr_next)
		rules++;

	if (!fp)
		return;

	for (rn = 0, fp = *listp; fp && (fp != rp); fp = fp->fr_next, rn++)
		if (FR_ISSKIP(fp->fr_flags) && (rn + fp->fr_arg >= rules))
			fp->fr_arg += addremove;
}


#ifdef	_KERNEL
/* ------------------------------------------------------------------------ */
/* Function:    count4bits                                                  */
/* Returns:     int - >= 0 - number of consecutive bits in input            */
/* Parameters:  ip(I) - 32bit IP address                                    */
/*                                                                          */
/* IPv4 ONLY                                                                */
/* count consecutive 1's in bit mask.  If the mask generated by counting    */
/* consecutive 1's is different to that passed, return -1, else return #    */
/* of bits.                                                                 */
/* ------------------------------------------------------------------------ */
int	count4bits(ip)
u_32_t	ip;
{
	u_32_t	ipn;
	int	cnt = 0, i, j;

	ip = ipn = ntohl(ip);
	for (i = 32; i; i--, ipn *= 2)
		if (ipn & 0x80000000)
			cnt++;
		else
			break;
	ipn = 0;
	for (i = 32, j = cnt; i; i--, j--) {
		ipn *= 2;
		if (j > 0)
			ipn++;
	}
	if (ipn == ip)
		return cnt;
	return -1;
}


# if 0
/* ------------------------------------------------------------------------ */
/* Function:    count6bits                                                  */
/* Returns:     int - >= 0 - number of consecutive bits in input            */
/* Parameters:  msk(I) - pointer to start of IPv6 bitmask                   */
/*                                                                          */
/* IPv6 ONLY                                                                */
/* count consecutive 1's in bit mask.                                       */
/* ------------------------------------------------------------------------ */
int count6bits(msk)
u_32_t *msk;
{
	int i = 0, k;
	u_32_t j;

	for (k = 3; k >= 0; k--)
		if (msk[k] == 0xffffffff)
			i += 32;
		else {
			for (j = msk[k]; j; j <<= 1)
				if (j & 0x80000000)
					i++;
		}
	return i;
}
# endif
#endif /* _KERNEL */


/* ------------------------------------------------------------------------ */
/* Function:    frsynclist                                                  */
/* Returns:     void                                                        */
/* Parameters:  fr(I)  - start of filter list to sync interface names for   */
/*              ifp(I) - interface pointer for limiting sync lookups        */
/* Write Locks: ipf_mutex                                                   */
/*                                                                          */
/* Walk through a list of filter rules and resolve any interface names into */
/* pointers.  Where dynamic addresses are used, also update the IP address  */
/* used in the rule.  The interface pointer is used to limit the lookups to */
/* a specific set of matching names if it is non-NULL.                      */
/* ------------------------------------------------------------------------ */
static void frsynclist(fr, ifp)
frentry_t *fr;
void *ifp;
{
	frdest_t *fdp;
	int v, i;

	for (; fr; fr = fr->fr_next) {
		v = fr->fr_v;

		/*
		 * Lookup all the interface names that are part of the rule.
		 */
		for (i = 0; i < 4; i++) {
			if ((ifp != NULL) && (fr->fr_ifas[i] != ifp))
				continue;
			fr->fr_ifas[i] = fr_resolvenic(fr->fr_ifnames[i], v);
		}

		if (fr->fr_type == FR_T_IPF) {
			if (fr->fr_satype != FRI_NORMAL &&
			    fr->fr_satype != FRI_LOOKUP) {
				(void)fr_ifpaddr(v, fr->fr_satype,
						 fr->fr_ifas[fr->fr_sifpidx],
						 &fr->fr_src, &fr->fr_smsk);
			}
			if (fr->fr_datype != FRI_NORMAL &&
			    fr->fr_datype != FRI_LOOKUP) {
				(void)fr_ifpaddr(v, fr->fr_datype,
						 fr->fr_ifas[fr->fr_difpidx],
						 &fr->fr_dst, &fr->fr_dmsk);
			}
		}

		fdp = &fr->fr_tifs[0];
		if ((ifp == NULL) || (fdp->fd_ifp == ifp))
			fr_resolvedest(fdp, v);

		fdp = &fr->fr_tifs[1];
		if ((ifp == NULL) || (fdp->fd_ifp == ifp))
			fr_resolvedest(fdp, v);

		fdp = &fr->fr_dif;
		if ((ifp == NULL) || (fdp->fd_ifp == ifp)) {
			fr_resolvedest(fdp, v);

			fr->fr_flags &= ~FR_DUP;
			if ((fdp->fd_ifp != (void *)-1) &&
			    (fdp->fd_ifp != NULL))
				fr->fr_flags |= FR_DUP;
		}

#ifdef	IPFILTER_LOOKUP
		if (fr->fr_type == FR_T_IPF && fr->fr_satype == FRI_LOOKUP &&
		    fr->fr_srcptr == NULL) {
			fr->fr_srcptr = fr_resolvelookup(fr->fr_srctype,
							 fr->fr_srcsubtype,
							 &fr->fr_slookup,
							 &fr->fr_srcfunc);
		}
		if (fr->fr_type == FR_T_IPF && fr->fr_datype == FRI_LOOKUP &&
		    fr->fr_dstptr == NULL) {
			fr->fr_dstptr = fr_resolvelookup(fr->fr_dsttype,
							 fr->fr_dstsubtype,
							 &fr->fr_dlookup,
							 &fr->fr_dstfunc);
		}
#endif
	}
}


#ifdef	_KERNEL
/* ------------------------------------------------------------------------ */
/* Function:    frsync                                                      */
/* Returns:     void                                                        */
/* Parameters:  Nil                                                         */
/*                                                                          */
/* frsync() is called when we suspect that the interface list or            */
/* information about interfaces (like IP#) has changed.  Go through all     */
/* filter rules, NAT entries and the state table and check if anything      */
/* needs to be changed/updated.                                             */
/* ------------------------------------------------------------------------ */
void frsync(ifp)
void *ifp;
{
	int i;

# if !SOLARIS
	fr_natsync(ifp);
	fr_statesync(ifp);
# endif

	WRITE_ENTER(&ipf_mutex);
	frsynclist(ipacct[0][fr_active], ifp);
	frsynclist(ipacct[1][fr_active], ifp);
	frsynclist(ipfilter[0][fr_active], ifp);
	frsynclist(ipfilter[1][fr_active], ifp);
	frsynclist(ipacct6[0][fr_active], ifp);
	frsynclist(ipacct6[1][fr_active], ifp);
	frsynclist(ipfilter6[0][fr_active], ifp);
	frsynclist(ipfilter6[1][fr_active], ifp);

	for (i = 0; i < IPL_LOGSIZE; i++) {
		frgroup_t *g;

		for (g = ipfgroups[i][0]; g != NULL; g = g->fg_next)
			frsynclist(g->fg_start, ifp);
		for (g = ipfgroups[i][1]; g != NULL; g = g->fg_next)
			frsynclist(g->fg_start, ifp);
	}
	RWLOCK_EXIT(&ipf_mutex);
}


/*
 * In the functions below, bcopy() is called because the pointer being
 * copied _from_ in this instance is a pointer to a char buf (which could
 * end up being unaligned) and on the kernel's local stack.
 */
/* ------------------------------------------------------------------------ */
/* Function:    copyinptr                                                   */
/* Returns:     int - 0 = success, else failure                             */
/* Parameters:  src(I)  - pointer to the source address                     */
/*              dst(I)  - destination address                               */
/*              size(I) - number of bytes to copy                           */
/*                                                                          */
/* Copy a block of data in from user space, given a pointer to the pointer  */
/* to start copying from (src) and a pointer to where to store it (dst).    */
/* NB: src - pointer to user space pointer, dst - kernel space pointer      */
/* ------------------------------------------------------------------------ */
int copyinptr(src, dst, size)
void *src, *dst;
size_t size;
{
	caddr_t ca;
	int error;

# if SOLARIS
	error = COPYIN(src, &ca, sizeof(ca));
	if (error != 0)
		return error;
# else
	bcopy(src, (caddr_t)&ca, sizeof(ca));
# endif
	error = COPYIN(ca, dst, size);
	if (error != 0)
		error = EFAULT;
	return error;
}


/* ------------------------------------------------------------------------ */
/* Function:    copyoutptr                                                  */
/* Returns:     int - 0 = success, else failure                             */
/* Parameters:  src(I)  - pointer to the source address                     */
/*              dst(I)  - destination address                               */
/*              size(I) - number of bytes to copy                           */
/*                                                                          */
/* Copy a block of data out to user space, given a pointer to the pointer   */
/* to start copying from (src) and a pointer to where to store it (dst).    */
/* NB: src - kernel space pointer, dst - pointer to user space pointer.     */
/* ------------------------------------------------------------------------ */
int copyoutptr(src, dst, size)
void *src, *dst;
size_t size;
{
	caddr_t ca;
	int error;

	bcopy(dst, (caddr_t)&ca, sizeof(ca));
	error = COPYOUT(src, ca, size);
	if (error != 0)
		error = EFAULT;
	return error;
}
#endif


/* ------------------------------------------------------------------------ */
/* Function:    fr_lock                                                     */
/* Returns:     int      - 0 = success, else error                          */
/* Parameters:  data(I)  - pointer to lock value to set                     */
/*              lockp(O) - pointer to location to store old lock value      */
/*                                                                          */
/* Get the new value for the lock integer, set it and return the old value  */
/* in *lockp.                                                               */
/* ------------------------------------------------------------------------ */
int fr_lock(data, lockp)
caddr_t data;
int *lockp;
{
	int arg, err;

	err = BCOPYIN(data, &arg, sizeof(arg));
	if (err != 0)
		return EFAULT;
	err = BCOPYOUT(lockp, data, sizeof(*lockp));
	if (err != 0)
		return EFAULT;
	*lockp = arg;
	return 0;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_getstat                                                  */
/* Returns:     Nil                                                         */
/* Parameters:  fiop(I)  - pointer to ipfilter stats structure              */
/*                                                                          */
/* Stores a copy of current pointers, counters, etc, in the friostat        */
/* structure.                                                               */
/* ------------------------------------------------------------------------ */
void fr_getstat(fiop)
friostat_t *fiop;
{
	int i, j;

	bcopy((char *)frstats, (char *)fiop->f_st, sizeof(filterstats_t) * 2);
	fiop->f_locks[IPL_LOGSTATE] = fr_state_lock;
	fiop->f_locks[IPL_LOGNAT] = fr_nat_lock;
	fiop->f_locks[IPL_LOGIPF] = fr_frag_lock;
	fiop->f_locks[IPL_LOGAUTH] = fr_auth_lock;

	for (i = 0; i < 2; i++)
		for (j = 0; j < 2; j++) {
			fiop->f_ipf[i][j] = ipfilter[i][j];
			fiop->f_acct[i][j] = ipacct[i][j];
			fiop->f_ipf6[i][j] = ipfilter6[i][j];
			fiop->f_acct6[i][j] = ipacct6[i][j];
		}

	fiop->f_ticks = fr_ticks;
	fiop->f_active = fr_active;
	fiop->f_froute[0] = fr_frouteok[0];
	fiop->f_froute[1] = fr_frouteok[1];

	fiop->f_running = fr_running;
	for (i = 0; i < IPL_LOGSIZE; i++) {
		fiop->f_groups[i][0] = ipfgroups[i][0];
		fiop->f_groups[i][1] = ipfgroups[i][1];
	}
#ifdef  IPFILTER_LOG
	fiop->f_logging = 1;
#else
	fiop->f_logging = 0;
#endif
	fiop->f_defpass = fr_pass;
	fiop->f_features = fr_features;
	(void) strncpy(fiop->f_version, ipfilter_version,
		       sizeof(fiop->f_version));
}


#ifdef	USE_INET6
int icmptoicmp6types[ICMP_MAXTYPE+1] = {
	ICMP6_ECHO_REPLY,	/* 0: ICMP_ECHOREPLY */
	-1,			/* 1: UNUSED */
	-1,			/* 2: UNUSED */
	ICMP6_DST_UNREACH,	/* 3: ICMP_UNREACH */
	-1,			/* 4: ICMP_SOURCEQUENCH */
	ND_REDIRECT,		/* 5: ICMP_REDIRECT */
	-1,			/* 6: UNUSED */
	-1,			/* 7: UNUSED */
	ICMP6_ECHO_REQUEST,	/* 8: ICMP_ECHO */
	-1,			/* 9: UNUSED */
	-1,			/* 10: UNUSED */
	ICMP6_TIME_EXCEEDED,	/* 11: ICMP_TIMXCEED */
	ICMP6_PARAM_PROB,	/* 12: ICMP_PARAMPROB */
	-1,			/* 13: ICMP_TSTAMP */
	-1,			/* 14: ICMP_TSTAMPREPLY */
	-1,			/* 15: ICMP_IREQ */
	-1,			/* 16: ICMP_IREQREPLY */
	-1,			/* 17: ICMP_MASKREQ */
	-1,			/* 18: ICMP_MASKREPLY */
};


int	icmptoicmp6unreach[ICMP_MAX_UNREACH] = {
	ICMP6_DST_UNREACH_ADDR,		/* 0: ICMP_UNREACH_NET */
	ICMP6_DST_UNREACH_ADDR,		/* 1: ICMP_UNREACH_HOST */
	-1,				/* 2: ICMP_UNREACH_PROTOCOL */
	ICMP6_DST_UNREACH_NOPORT,	/* 3: ICMP_UNREACH_PORT */
	-1,				/* 4: ICMP_UNREACH_NEEDFRAG */
	ICMP6_DST_UNREACH_NOTNEIGHBOR,	/* 5: ICMP_UNREACH_SRCFAIL */
	ICMP6_DST_UNREACH_ADDR,		/* 6: ICMP_UNREACH_NET_UNKNOWN */
	ICMP6_DST_UNREACH_ADDR,		/* 7: ICMP_UNREACH_HOST_UNKNOWN */
	-1,				/* 8: ICMP_UNREACH_ISOLATED */
	ICMP6_DST_UNREACH_ADMIN,	/* 9: ICMP_UNREACH_NET_PROHIB */
	ICMP6_DST_UNREACH_ADMIN,	/* 10: ICMP_UNREACH_HOST_PROHIB */
	-1,				/* 11: ICMP_UNREACH_TOSNET */
	-1,				/* 12: ICMP_UNREACH_TOSHOST */
	ICMP6_DST_UNREACH_ADMIN,	/* 13: ICMP_UNREACH_ADMIN_PROHIBIT */
};
int	icmpreplytype6[ICMP6_MAXTYPE + 1];
#endif

int	icmpreplytype4[ICMP_MAXTYPE + 1];


/* ------------------------------------------------------------------------ */
/* Function:    fr_matchicmpqueryreply                                      */
/* Returns:     int - 1 if "icmp" is a valid reply to "ic" else 0.          */
/* Parameters:  v(I)    - IP protocol version (4 or 6)                      */
/*              ic(I)   - ICMP information                                  */
/*              icmp(I) - ICMP packet header                                */
/*              rev(I)  - direction (0 = forward/1 = reverse) of packet     */
/*                                                                          */
/* Check if the ICMP packet defined by the header pointed to by icmp is a   */
/* reply to one as described by what's in ic.  If it is a match, return 1,  */
/* else return 0 for no match.                                              */
/* ------------------------------------------------------------------------ */
int fr_matchicmpqueryreply(v, ic, icmp, rev)
int v;
icmpinfo_t *ic;
icmphdr_t *icmp;
int rev;
{
	int ictype;

	ictype = ic->ici_type;

	if (v == 4) {
		/*
		 * If we matched its type on the way in, then when going out
		 * it will still be the same type.
		 */
		if ((!rev && (icmp->icmp_type == ictype)) ||
		    (rev && (icmpreplytype4[ictype] == icmp->icmp_type))) {
			if (icmp->icmp_type != ICMP_ECHOREPLY)
				return 1;
			if (icmp->icmp_id == ic->ici_id)
				return 1;
		}
	}
#ifdef	USE_INET6
	else if (v == 6) {
		if ((!rev && (icmp->icmp_type == ictype)) ||
		    (rev && (icmpreplytype6[ictype] == icmp->icmp_type))) {
			if (icmp->icmp_type != ICMP6_ECHO_REPLY)
				return 1;
			if (icmp->icmp_id == ic->ici_id)
				return 1;
		}
	}
#endif
	return 0;
}


#ifdef	IPFILTER_LOOKUP
/* ------------------------------------------------------------------------ */
/* Function:    fr_resolvelookup                                            */
/* Returns:     void * - NULL = failure, else success.                      */
/* Parameters:  type(I)     - type of lookup these parameters are for.      */
/*              subtype(I)  - whether the info below contains number/name   */
/*              info(I)     - pointer to name/number of the lookup data     */
/*              funcptr(IO) - pointer to pointer for storing IP address     */
/*                           searching function.                            */
/*                                                                          */
/* Search for the "table" number passed in amongst those configured for     */
/* that particular type.  If the type is recognised then the function to    */
/* call to do the IP address search will be change, regardless of whether   */
/* or not the "table" number exists.                                        */
/* ------------------------------------------------------------------------ */
static void *fr_resolvelookup(type, subtype, info, funcptr)
u_int type, subtype;
i6addr_t *info;
lookupfunc_t *funcptr;
{
	char label[FR_GROUPLEN], *name;
	iphtable_t *iph;
	ip_pool_t *ipo;
	void *ptr;

	if (subtype == 0) {
#if defined(SNPRINTF) && defined(_KERNEL)
		SNPRINTF(label, sizeof(label), "%u", info->iplookupnum);
#else
		(void) sprintf(label, "%u", info->iplookupnum);
#endif
		name = label;
	} else if (subtype == 1) {
		/*
		 * Because iplookupname is currently only a 12 character
		 * string and FR_GROUPLEN is 16, copy all of it into the
		 * label buffer and add on a NULL at the end.
		 */
		strncpy(label, info->iplookupname, sizeof(info->iplookupname));
		label[sizeof(info->iplookupname)] = '\0';
		name = label;
	} else {
		return NULL;
	}

	READ_ENTER(&ip_poolrw);

	switch (type)
	{
	case IPLT_POOL :
# if (defined(__osf__) && defined(_KERNEL))
		ptr = NULL;
		*funcptr = NULL;
# else
		ipo = ip_pool_find(IPL_LOGIPF, name);
		ptr = ipo;
		if (ipo != NULL) {
			ATOMIC_INC32(ipo->ipo_ref);
		}
		*funcptr = ip_pool_search;
# endif
		break;
	case IPLT_HASH :
		iph = fr_findhtable(IPL_LOGIPF, name);
		ptr = iph;
		if (iph != NULL) {
			ATOMIC_INC32(iph->iph_ref);
		}
		*funcptr = fr_iphmfindip;
		break;
	default:
		ptr = NULL;
		*funcptr = NULL;
		break;
	}
	RWLOCK_EXIT(&ip_poolrw);

	return ptr;
}
#endif


/* ------------------------------------------------------------------------ */
/* Function:    frrequest                                                   */
/* Returns:     int - 0 == success, > 0 == errno value                      */
/* Parameters:  unit(I)     - device for which this is for                  */
/*              req(I)      - ioctl command (SIOC*)                         */
/*              data(I)     - pointr to ioctl data                          */
/*              set(I)      - 1 or 0 (filter set)                           */
/*              makecopy(I) - flag indicating whether data points to a rule */
/*                            in kernel space & hence doesn't need copying. */
/*                                                                          */
/* This function handles all the requests which operate on the list of      */
/* filter rules.  This includes adding, deleting, insertion.  It is also    */
/* responsible for creating groups when a "head" rule is loaded.  Interface */
/* names are resolved here and other sanity checks are made on the content  */
/* of the rule structure being loaded.  If a rule has user defined timeouts */
/* then make sure they are created and initialised before exiting.          */
/* ------------------------------------------------------------------------ */
int frrequest(unit, req, data, set, makecopy)
int unit;
ioctlcmd_t req;
int set, makecopy;
caddr_t data;
{
	frentry_t frd, *fp, *f, **fprev, **ftail;
	int error = 0, in, v;
	void *ptr, *uptr;
	u_int *p, *pp;
	frgroup_t *fg;
	char *group;

	fg = NULL;
	fp = &frd;
	if (makecopy != 0) {
		error = fr_inobj(data, fp, IPFOBJ_FRENTRY);
		if (error)
			return EFAULT;
		if ((fp->fr_flags & FR_T_BUILTIN) != 0)
			return EINVAL;
		fp->fr_ref = 0;
		fp->fr_flags |= FR_COPIED;
	} else {
		fp = (frentry_t *)data;
		if ((fp->fr_type & FR_T_BUILTIN) == 0)
			return EINVAL;
		fp->fr_flags &= ~FR_COPIED;
	}

	if (((fp->fr_dsize == 0) && (fp->fr_data != NULL)) ||
	    ((fp->fr_dsize != 0) && (fp->fr_data == NULL)))
		return EINVAL;

	v = fp->fr_v;
	uptr = fp->fr_data;

	/*
	 * Only filter rules for IPv4 or IPv6 are accepted.
	 */
	if (v == 4)
		/*EMPTY*/;
#ifdef	USE_INET6
	else if (v == 6)
		/*EMPTY*/;
#endif
	else {
		return EINVAL;
	}

	/*
	 * If the rule is being loaded from user space, i.e. we had to copy it
	 * into kernel space, then do not trust the function pointer in the
	 * rule.
	 */
	if ((makecopy == 1) && (fp->fr_func != NULL)) {
		if (fr_findfunc(fp->fr_func) == NULL)
			return ESRCH;
		error = fr_funcinit(fp);
		if (error != 0)
			return error;
	}

	ptr = NULL;
	/*
	 * Check that the group number does exist and that its use (in/out)
	 * matches what the rule is.
	 */
	if (!strncmp(fp->fr_grhead, "0", FR_GROUPLEN))
		*fp->fr_grhead = '\0';
	group = fp->fr_group;
	if (!strncmp(group, "0", FR_GROUPLEN))
		*group = '\0';

	if (FR_ISACCOUNT(fp->fr_flags))
		unit = IPL_LOGCOUNT;

	if ((req != (int)SIOCZRLST) && (*group != '\0')) {
		fg = fr_findgroup(group, unit, set, NULL);
		if (fg == NULL)
			return ESRCH;
		if (fg->fg_flags == 0)
			fg->fg_flags = fp->fr_flags & FR_INOUT;
		else if (fg->fg_flags != (fp->fr_flags & FR_INOUT))
			return ESRCH;
	}

	in = (fp->fr_flags & FR_INQUE) ? 0 : 1;

	/*
	 * Work out which rule list this change is being applied to.
	 */
	ftail = NULL;
	fprev = NULL;
	if (unit == IPL_LOGAUTH)
		fprev = &ipauth;
	else if (v == 4) {
		if (FR_ISACCOUNT(fp->fr_flags))
			fprev = &ipacct[in][set];
		else if ((fp->fr_flags & (FR_OUTQUE|FR_INQUE)) != 0)
			fprev = &ipfilter[in][set];
	} else if (v == 6) {
		if (FR_ISACCOUNT(fp->fr_flags))
			fprev = &ipacct6[in][set];
		else if ((fp->fr_flags & (FR_OUTQUE|FR_INQUE)) != 0)
			fprev = &ipfilter6[in][set];
	}
	if (fprev == NULL)
		return ESRCH;

	if (*group != '\0') {
		if (!fg && !(fg = fr_findgroup(group, unit, set, NULL)))
			return ESRCH;
		fprev = &fg->fg_start;
	}

	/*
	 * Copy in extra data for the rule.
	 */
	if (fp->fr_dsize != 0) {
		if (makecopy != 0) {
			KMALLOCS(ptr, void *, fp->fr_dsize);
			if (!ptr)
				return ENOMEM;
			error = COPYIN(uptr, ptr, fp->fr_dsize);
			if (error != 0)
				error = EFAULT;
		} else {
			ptr = uptr;
			error = 0;
		}
		if (error != 0) {
			KFREES(ptr, fp->fr_dsize);
			return ENOMEM;
		}
		fp->fr_data = ptr;
	} else
		fp->fr_data = NULL;

	/*
	 * Perform per-rule type sanity checks of their members.
	 */
	switch (fp->fr_type & ~FR_T_BUILTIN)
	{
#if defined(IPFILTER_BPF)
	case FR_T_BPFOPC :
		if (fp->fr_dsize == 0)
			return EINVAL;
		if (!bpf_validate(ptr, fp->fr_dsize/sizeof(struct bpf_insn))) {
			if (makecopy && fp->fr_data != NULL) {
				KFREES(fp->fr_data, fp->fr_dsize);
			}
			return EINVAL;
		}
		break;
#endif
	case FR_T_IPF :
		if (fp->fr_dsize != sizeof(fripf_t))
			return EINVAL;

		/*
		 * Allowing a rule with both "keep state" and "with oow" is
		 * pointless because adding a state entry to the table will
		 * fail with the out of window (oow) flag set.
		 */
		if ((fp->fr_flags & FR_KEEPSTATE) && (fp->fr_flx & FI_OOW))
			return EINVAL;

		switch (fp->fr_satype)
		{
		case FRI_BROADCAST :
		case FRI_DYNAMIC :
		case FRI_NETWORK :
		case FRI_NETMASKED :
		case FRI_PEERADDR :
			if (fp->fr_sifpidx < 0 || fp->fr_sifpidx > 3) {
				if (makecopy && fp->fr_data != NULL) {
					KFREES(fp->fr_data, fp->fr_dsize);
				}
				return EINVAL;
			}
			break;
#ifdef	IPFILTER_LOOKUP
		case FRI_LOOKUP :
			fp->fr_srcptr = fr_resolvelookup(fp->fr_srctype,
							 fp->fr_srcsubtype,
							 &fp->fr_slookup,
							 &fp->fr_srcfunc);
			if (fp->fr_srcptr == NULL)
				return ESRCH;
			break;
#endif
		default :
			break;
		}

		switch (fp->fr_datype)
		{
		case FRI_BROADCAST :
		case FRI_DYNAMIC :
		case FRI_NETWORK :
		case FRI_NETMASKED :
		case FRI_PEERADDR :
			if (fp->fr_difpidx < 0 || fp->fr_difpidx > 3) {
				if (makecopy && fp->fr_data != NULL) {
					KFREES(fp->fr_data, fp->fr_dsize);
				}
				return EINVAL;
			}
			break;
#ifdef	IPFILTER_LOOKUP
		case FRI_LOOKUP :
			fp->fr_dstptr = fr_resolvelookup(fp->fr_dsttype,
							 fp->fr_dstsubtype,
							 &fp->fr_dlookup,
							 &fp->fr_dstfunc);
			if (fp->fr_dstptr == NULL)
				return ESRCH;
			break;
#endif
		default :
			break;
		}
		break;
	case FR_T_NONE :
		break;
	case FR_T_CALLFUNC :
		break;
	case FR_T_COMPIPF :
		break;
	default :
		if (makecopy && fp->fr_data != NULL) {
			KFREES(fp->fr_data, fp->fr_dsize);
		}
		return EINVAL;
	}

	/*
	 * Lookup all the interface names that are part of the rule.
	 */
	frsynclist(fp, NULL);
	fp->fr_statecnt = 0;

	/*
	 * Look for an existing matching filter rule, but don't include the
	 * next or interface pointer in the comparison (fr_next, fr_ifa).
	 * This elminates rules which are indentical being loaded.  Checksum
	 * the constant part of the filter rule to make comparisons quicker
	 * (this meaning no pointers are included).
	 */
	for (fp->fr_cksum = 0, p = (u_int *)&fp->fr_func, pp = &fp->fr_cksum;
	     p < pp; p++)
		fp->fr_cksum += *p;
	pp = (u_int *)(fp->fr_caddr + fp->fr_dsize);
	for (p = (u_int *)fp->fr_data; p < pp; p++)
		fp->fr_cksum += *p;

	WRITE_ENTER(&ipf_mutex);

	/*
	 * Now that the filter rule lists are locked, we can walk the
	 * chain of them without fear.
	 */
	ftail = fprev;
	for (f = *ftail; (f = *ftail) != NULL; ftail = &f->fr_next) {
		if (fp->fr_collect <= f->fr_collect) {
			ftail = fprev;
			f = NULL;
			break;
		}
		fprev = ftail;
	}
	bzero((char *)frcache, sizeof(frcache));

	for (; (f = *ftail) != NULL; ftail = &f->fr_next) {
		if ((fp->fr_cksum != f->fr_cksum) ||
		    (f->fr_dsize != fp->fr_dsize))
			continue;
		if (bcmp((char *)&f->fr_func, (char *)&fp->fr_func, FR_CMPSIZ))
			continue;
		if ((!ptr && !f->fr_data) ||
		    (ptr && f->fr_data &&
		     !bcmp((char *)ptr, (char *)f->fr_data, f->fr_dsize)))
			break;
	}

	/*
	 * If zero'ing statistics, copy current to caller and zero.
	 */
	if (req == (ioctlcmd_t)SIOCZRLST) {
		if (f == NULL)
			error = ESRCH;
		else {
			/*
			 * Copy and reduce lock because of impending copyout.
			 * Well we should, but if we do then the atomicity of
			 * this call and the correctness of fr_hits and
			 * fr_bytes cannot be guaranteed.  As it is, this code
			 * only resets them to 0 if they are successfully
			 * copied out into user space.
			 */
			bcopy((char *)f, (char *)fp, sizeof(*f));
			/* MUTEX_DOWNGRADE(&ipf_mutex); */

			/*
			 * When we copy this rule back out, set the data
			 * pointer to be what it was in user space.
			 */
			fp->fr_data = uptr;
			error = fr_outobj(data, fp, IPFOBJ_FRENTRY);

			if (error == 0) {
				if ((f->fr_dsize != 0) && (uptr != NULL))
					error = COPYOUT(f->fr_data, uptr,
							f->fr_dsize);
					if (error != 0)
						error = EFAULT;
				if (error == 0) {
					f->fr_hits = 0;
					f->fr_bytes = 0;
				}
			}
		}

		if ((ptr != NULL) && (makecopy != 0)) {
			KFREES(ptr, fp->fr_dsize);
		}
		RWLOCK_EXIT(&ipf_mutex);
		return error;
	}

	if (!f) {
		/*
		 * At the end of this, ftail must point to the place where the
		 * new rule is to be saved/inserted/added.
		 * For SIOCAD*FR, this should be the last rule in the group of
		 * rules that have equal fr_collect fields.
		 * For SIOCIN*FR, ...
		 */
		if (req == (ioctlcmd_t)SIOCADAFR ||
		    req == (ioctlcmd_t)SIOCADIFR) {

			for (ftail = fprev; (f = *ftail) != NULL; ) {
				if (f->fr_collect > fp->fr_collect)
					break;
				ftail = &f->fr_next;
			}
			f = NULL;
			ptr = NULL;
			error = 0;
		} else if (req == (ioctlcmd_t)SIOCINAFR ||
			   req == (ioctlcmd_t)SIOCINIFR) {
			while ((f = *fprev) != NULL) {
				if (f->fr_collect >= fp->fr_collect)
					break;
				fprev = &f->fr_next;
			}
			ftail = fprev;
			if (fp->fr_hits != 0) {
				while (fp->fr_hits && (f = *ftail)) {
					if (f->fr_collect != fp->fr_collect)
						break;
					fprev = ftail;
					ftail = &f->fr_next;
					fp->fr_hits--;
				}
			}
			f = NULL;
			ptr = NULL;
			error = 0;
		}
	}

	/*
	 * Request to remove a rule.
	 */
	if (req == (ioctlcmd_t)SIOCRMAFR || req == (ioctlcmd_t)SIOCRMIFR) {
		if (!f)
			error = ESRCH;
		else {
			/*
			 * Do not allow activity from user space to interfere
			 * with rules not loaded that way.
			 */
			if ((makecopy == 1) && !(f->fr_flags & FR_COPIED)) {
				error = EPERM;
				goto done;
			}

			/*
			 * Return EBUSY if the rule is being reference by
			 * something else (eg state information.)
			 */
			if (f->fr_ref > 1) {
				error = EBUSY;
				goto done;
			}
#ifdef	IPFILTER_SCAN
			if (f->fr_isctag[0] != '\0' &&
			    (f->fr_isc != (struct ipscan *)-1))
				ipsc_detachfr(f);
#endif
			if (unit == IPL_LOGAUTH) {
				error = fr_preauthcmd(req, f, ftail);
				goto done;
			}
			if (*f->fr_grhead != '\0')
				fr_delgroup(f->fr_grhead, unit, set);
			fr_fixskip(ftail, f, -1);
			*ftail = f->fr_next;
			f->fr_next = NULL;
			(void) fr_derefrule(&f);
		}
	} else {
		/*
		 * Not removing, so we must be adding/inserting a rule.
		 */
		if (f)
			error = EEXIST;
		else {
			if (unit == IPL_LOGAUTH) {
				error = fr_preauthcmd(req, fp, ftail);
				goto done;
			}
			if (makecopy) {
				KMALLOC(f, frentry_t *);
			} else
				f = fp;
			if (f != NULL) {
				if (fp != f)
					bcopy((char *)fp, (char *)f,
					      sizeof(*f));
				MUTEX_NUKE(&f->fr_lock);
				MUTEX_INIT(&f->fr_lock, "filter rule lock");
#ifdef	IPFILTER_SCAN
				if (f->fr_isctag[0] != '\0' &&
				    ipsc_attachfr(f))
					f->fr_isc = (struct ipscan *)-1;
#endif
				f->fr_hits = 0;
				if (makecopy != 0)
					f->fr_ref = 1;
				f->fr_next = *ftail;
				*ftail = f;
				if (req == (ioctlcmd_t)SIOCINIFR ||
				    req == (ioctlcmd_t)SIOCINAFR)
					fr_fixskip(ftail, f, 1);
				f->fr_grp = NULL;
				group = f->fr_grhead;
				if (*group != '\0') {
					fg = fr_addgroup(group, f, f->fr_flags,
							 unit, set);
					if (fg != NULL)
						f->fr_grp = &fg->fg_start;
				}
			} else
				error = ENOMEM;
		}
	}
done:
	RWLOCK_EXIT(&ipf_mutex);
	if ((ptr != NULL) && (error != 0) && (makecopy != 0)) {
		KFREES(ptr, fp->fr_dsize);
	}
	return (error);
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_funcinit                                                 */
/* Returns:     int - 0 == success, else ESRCH: cannot resolve rule details */
/* Parameters:  fr(I) - pointer to filter rule                              */
/*                                                                          */
/* If a rule is a call rule, then check if the function it points to needs  */
/* an init function to be called now the rule has been loaded.              */
/* ------------------------------------------------------------------------ */
static int fr_funcinit(fr)
frentry_t *fr;
{
	ipfunc_resolve_t *ft;
	int err;

	err = ESRCH;

	for (ft = fr_availfuncs; ft->ipfu_addr != NULL; ft++)
		if (ft->ipfu_addr == fr->fr_func) {
			err = 0;
			if (ft->ipfu_init != NULL)
				err = (*ft->ipfu_init)(fr);
			break;
		}
	return err;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_findfunc                                                 */
/* Returns:     ipfunc_t - pointer to function if found, else NULL          */
/* Parameters:  funcptr(I) - function pointer to lookup                     */
/*                                                                          */
/* Look for a function in the table of known functions.                     */
/* ------------------------------------------------------------------------ */
static ipfunc_t fr_findfunc(funcptr)
ipfunc_t funcptr;
{
	ipfunc_resolve_t *ft;

	for (ft = fr_availfuncs; ft->ipfu_addr != NULL; ft++)
		if (ft->ipfu_addr == funcptr)
			return funcptr;
	return NULL;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_resolvefunc                                              */
/* Returns:     int - 0 == success, else error                              */
/* Parameters:  data(IO) - ioctl data pointer to ipfunc_resolve_t struct    */
/*                                                                          */
/* Copy in a ipfunc_resolve_t structure and then fill in the missing field. */
/* This will either be the function name (if the pointer is set) or the     */
/* function pointer if the name is set.  When found, fill in the other one  */
/* so that the entire, complete, structure can be copied back to user space.*/
/* ------------------------------------------------------------------------ */
int fr_resolvefunc(data)
void *data;
{
	ipfunc_resolve_t res, *ft;
	int err;

	err = BCOPYIN(data, &res, sizeof(res));
	if (err != 0)
		return EFAULT;

	if (res.ipfu_addr == NULL && res.ipfu_name[0] != '\0') {
		for (ft = fr_availfuncs; ft->ipfu_addr != NULL; ft++)
			if (strncmp(res.ipfu_name, ft->ipfu_name,
				    sizeof(res.ipfu_name)) == 0) {
				res.ipfu_addr = ft->ipfu_addr;
				res.ipfu_init = ft->ipfu_init;
				if (COPYOUT(&res, data, sizeof(res)) != 0)
					return EFAULT;
				return 0;
			}
	}
	if (res.ipfu_addr != NULL && res.ipfu_name[0] == '\0') {
		for (ft = fr_availfuncs; ft->ipfu_addr != NULL; ft++)
			if (ft->ipfu_addr == res.ipfu_addr) {
				(void) strncpy(res.ipfu_name, ft->ipfu_name,
					       sizeof(res.ipfu_name));
				res.ipfu_init = ft->ipfu_init;
				if (COPYOUT(&res, data, sizeof(res)) != 0)
					return EFAULT;
				return 0;
			}
	}
	return ESRCH;
}


#if !defined(_KERNEL) || (!defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(__FreeBSD__)) || \
    (defined(__FreeBSD__) && (__FreeBSD_version < 501000)) || \
    (defined(__NetBSD__) && (__NetBSD_Version__ < 105000000)) || \
    (defined(__OpenBSD__) && (OpenBSD < 200006))
/*
 * From: NetBSD
 * ppsratecheck(): packets (or events) per second limitation.
 */
int
ppsratecheck(lasttime, curpps, maxpps)
	struct timeval *lasttime;
	int *curpps;
	int maxpps;	/* maximum pps allowed */
{
	struct timeval tv, delta;
	int rv;

	GETKTIME(&tv);

	delta.tv_sec = tv.tv_sec - lasttime->tv_sec;
	delta.tv_usec = tv.tv_usec - lasttime->tv_usec;
	if (delta.tv_usec < 0) {
		delta.tv_sec--;
		delta.tv_usec += 1000000;
	}

	/*
	 * check for 0,0 is so that the message will be seen at least once.
	 * if more than one second have passed since the last update of
	 * lasttime, reset the counter.
	 *
	 * we do increment *curpps even in *curpps < maxpps case, as some may
	 * try to use *curpps for stat purposes as well.
	 */
	if ((lasttime->tv_sec == 0 && lasttime->tv_usec == 0) ||
	    delta.tv_sec >= 1) {
		*lasttime = tv;
		*curpps = 0;
		rv = 1;
	} else if (maxpps < 0)
		rv = 1;
	else if (*curpps < maxpps)
		rv = 1;
	else
		rv = 0;
	*curpps = *curpps + 1;

	return (rv);
}
#endif


/* ------------------------------------------------------------------------ */
/* Function:    fr_derefrule                                                */
/* Returns:     int   - 0 == rule freed up, else rule not freed             */
/* Parameters:  fr(I) - pointer to filter rule                              */
/*                                                                          */
/* Decrement the reference counter to a rule by one.  If it reaches zero,   */
/* free it and any associated storage space being used by it.               */
/* ------------------------------------------------------------------------ */
int fr_derefrule(frp)
frentry_t **frp;
{
	frentry_t *fr;

	fr = *frp;
	*frp = NULL;

	MUTEX_ENTER(&fr->fr_lock);
	fr->fr_ref--;
	if (fr->fr_ref == 0) {
		MUTEX_EXIT(&fr->fr_lock);
		MUTEX_DESTROY(&fr->fr_lock);

#ifdef IPFILTER_LOOKUP
		if (fr->fr_type == FR_T_IPF && fr->fr_satype == FRI_LOOKUP)
			ip_lookup_deref(fr->fr_srctype, fr->fr_srcptr);
		if (fr->fr_type == FR_T_IPF && fr->fr_datype == FRI_LOOKUP)
			ip_lookup_deref(fr->fr_dsttype, fr->fr_dstptr);
#endif

		if (fr->fr_dsize) {
			KFREES(fr->fr_data, fr->fr_dsize);
		}
		if ((fr->fr_flags & FR_COPIED) != 0) {
			KFREE(fr);
			return 0;
		}
		return 1;
	} else {
		MUTEX_EXIT(&fr->fr_lock);
	}
	return -1;
}


#ifdef	IPFILTER_LOOKUP
/* ------------------------------------------------------------------------ */
/* Function:    fr_grpmapinit                                               */
/* Returns:     int - 0 == success, else ESRCH because table entry not found*/
/* Parameters:  fr(I) - pointer to rule to find hash table for              */
/*                                                                          */
/* Looks for group hash table fr_arg and stores a pointer to it in fr_ptr.  */
/* fr_ptr is later used by fr_srcgrpmap and fr_dstgrpmap.                   */
/* ------------------------------------------------------------------------ */
static int fr_grpmapinit(fr)
frentry_t *fr;
{
	char name[FR_GROUPLEN];
	iphtable_t *iph;

#if defined(SNPRINTF) && defined(_KERNEL)
	SNPRINTF(name, sizeof(name), "%d", fr->fr_arg);
#else
	(void) sprintf(name, "%d", fr->fr_arg);
#endif
	iph = fr_findhtable(IPL_LOGIPF, name);
	if (iph == NULL)
		return ESRCH;
	if ((iph->iph_flags & FR_INOUT) != (fr->fr_flags & FR_INOUT))
		return ESRCH;
	fr->fr_ptr = iph;
	return 0;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_srcgrpmap                                                */
/* Returns:     frentry_t * - pointer to "new last matching" rule or NULL   */
/* Parameters:  fin(I)    - pointer to packet information                   */
/*              passp(IO) - pointer to current/new filter decision (unused) */
/*                                                                          */
/* Look for a rule group head in a hash table, using the source address as  */
/* the key, and descend into that group and continue matching rules against */
/* the packet.                                                              */
/* ------------------------------------------------------------------------ */
frentry_t *fr_srcgrpmap(fin, passp)
fr_info_t *fin;
u_32_t *passp;
{
	frgroup_t *fg;
	void *rval;

	rval = fr_iphmfindgroup(fin->fin_fr->fr_ptr, &fin->fin_src);
	if (rval == NULL)
		return NULL;

	fg = rval;
	fin->fin_fr = fg->fg_start;
	(void) fr_scanlist(fin, *passp);
	return fin->fin_fr;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_dstgrpmap                                                */
/* Returns:     frentry_t * - pointer to "new last matching" rule or NULL   */
/* Parameters:  fin(I)    - pointer to packet information                   */
/*              passp(IO) - pointer to current/new filter decision (unused) */
/*                                                                          */
/* Look for a rule group head in a hash table, using the destination        */
/* address as the key, and descend into that group and continue matching    */
/* rules against  the packet.                                               */
/* ------------------------------------------------------------------------ */
frentry_t *fr_dstgrpmap(fin, passp)
fr_info_t *fin;
u_32_t *passp;
{
	frgroup_t *fg;
	void *rval;

	rval = fr_iphmfindgroup(fin->fin_fr->fr_ptr, &fin->fin_dst);
	if (rval == NULL)
		return NULL;

	fg = rval;
	fin->fin_fr = fg->fg_start;
	(void) fr_scanlist(fin, *passp);
	return fin->fin_fr;
}
#endif /* IPFILTER_LOOKUP */

/*
 * Queue functions
 * ===============
 * These functions manage objects on queues for efficient timeouts.  There are
 * a number of system defined queues as well as user defined timeouts.  It is
 * expected that a lock is held in the domain in which the queue belongs
 * (i.e. either state or NAT) when calling any of these functions that prevents
 * fr_freetimeoutqueue() from being called at the same time as any other.
 */


/* ------------------------------------------------------------------------ */
/* Function:    fr_addtimeoutqueue                                          */
/* Returns:     struct ifqtq * - NULL if malloc fails, else pointer to      */
/*                               timeout queue with given interval.         */
/* Parameters:  parent(I)  - pointer to pointer to parent node of this list */
/*                           of interface queues.                           */
/*              seconds(I) - timeout value in seconds for this queue.       */
/*                                                                          */
/* This routine first looks for a timeout queue that matches the interval   */
/* being requested.  If it finds one, increments the reference counter and  */
/* returns a pointer to it.  If none are found, it allocates a new one and  */
/* inserts it at the top of the list.                                       */
/*                                                                          */
/* Locking.                                                                 */
/* It is assumed that the caller of this function has an appropriate lock   */
/* held (exclusively) in the domain that encompases 'parent'.               */
/* ------------------------------------------------------------------------ */
ipftq_t *fr_addtimeoutqueue(parent, seconds)
ipftq_t **parent;
u_int seconds;
{
	ipftq_t *ifq;
	u_int period;

	period = seconds * IPF_HZ_DIVIDE;

	MUTEX_ENTER(&ipf_timeoutlock);
	for (ifq = *parent; ifq != NULL; ifq = ifq->ifq_next) {
		if (ifq->ifq_ttl == period) {
			/*
			 * Reset the delete flag, if set, so the structure
			 * gets reused rather than freed and reallocated.
			 */
			MUTEX_ENTER(&ifq->ifq_lock);
			ifq->ifq_flags &= ~IFQF_DELETE;
			ifq->ifq_ref++;
			MUTEX_EXIT(&ifq->ifq_lock);
			MUTEX_EXIT(&ipf_timeoutlock);

			return ifq;
		}
	}

	KMALLOC(ifq, ipftq_t *);
	if (ifq != NULL) {
		ifq->ifq_ttl = period;
		ifq->ifq_head = NULL;
		ifq->ifq_tail = &ifq->ifq_head;
		ifq->ifq_next = *parent;
		ifq->ifq_pnext = parent;
		ifq->ifq_ref = 1;
		ifq->ifq_flags = IFQF_USER;
		*parent = ifq;
		fr_userifqs++;
		MUTEX_NUKE(&ifq->ifq_lock);
		MUTEX_INIT(&ifq->ifq_lock, "ipftq mutex");
	}
	MUTEX_EXIT(&ipf_timeoutlock);
	return ifq;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_deletetimeoutqueue                                       */
/* Returns:     int    - new reference count value of the timeout queue     */
/* Parameters:  ifq(I) - timeout queue which is losing a reference.         */
/* Locks:       ifq->ifq_lock                                               */
/*                                                                          */
/* This routine must be called when we're discarding a pointer to a timeout */
/* queue object, taking care of the reference counter.                      */
/*                                                                          */
/* Now that this just sets a DELETE flag, it requires the expire code to    */
/* check the list of user defined timeout queues and call the free function */
/* below (currently commented out) to stop memory leaking.  It is done this */
/* way because the locking may not be sufficient to safely do a free when   */
/* this function is called.                                                 */
/* ------------------------------------------------------------------------ */
int fr_deletetimeoutqueue(ifq)
ipftq_t *ifq;
{

	ifq->ifq_ref--;
	if ((ifq->ifq_ref == 0) && ((ifq->ifq_flags & IFQF_USER) != 0)) {
		ifq->ifq_flags |= IFQF_DELETE;
	}

	return ifq->ifq_ref;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_freetimeoutqueue                                         */
/* Parameters:  ifq(I) - timeout queue which is losing a reference.         */
/* Returns:     Nil                                                         */
/*                                                                          */
/* Locking:                                                                 */
/* It is assumed that the caller of this function has an appropriate lock   */
/* held (exclusively) in the domain that encompases the callers "domain".   */
/* The ifq_lock for this structure should not be held.                      */
/*                                                                          */
/* Remove a user definde timeout queue from the list of queues it is in and */
/* tidy up after this is done.                                              */
/* ------------------------------------------------------------------------ */
void fr_freetimeoutqueue(ifq)
ipftq_t *ifq;
{


	if (((ifq->ifq_flags & IFQF_DELETE) == 0) || (ifq->ifq_ref != 0) ||
	    ((ifq->ifq_flags & IFQF_USER) == 0)) {
		printf("fr_freetimeoutqueue(%lx) flags 0x%x ttl %d ref %d\n",
		       (u_long)ifq, ifq->ifq_flags, ifq->ifq_ttl,
		       ifq->ifq_ref);
		return;
	}

	/*
	 * Remove from its position in the list.
	 */
	*ifq->ifq_pnext = ifq->ifq_next;
	if (ifq->ifq_next != NULL)
		ifq->ifq_next->ifq_pnext = ifq->ifq_pnext;

	MUTEX_DESTROY(&ifq->ifq_lock);
	ATOMIC_DEC(fr_userifqs);
	KFREE(ifq);
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_deletequeueentry                                         */
/* Returns:     Nil                                                         */
/* Parameters:  tqe(I) - timeout queue entry to delete                      */
/*              ifq(I) - timeout queue to remove entry from                 */
/*                                                                          */
/* Remove a tail queue entry from its queue and make it an orphan.          */
/* fr_deletetimeoutqueue is called to make sure the reference count on the  */
/* queue is correct.  We can't, however, call fr_freetimeoutqueue because   */
/* the correct lock(s) may not be held that would make it safe to do so.    */
/* ------------------------------------------------------------------------ */
void fr_deletequeueentry(tqe)
ipftqent_t *tqe;
{
	ipftq_t *ifq;

	ifq = tqe->tqe_ifq;

	MUTEX_ENTER(&ifq->ifq_lock);

	if (tqe->tqe_pnext != NULL) {
		*tqe->tqe_pnext = tqe->tqe_next;
		if (tqe->tqe_next != NULL)
			tqe->tqe_next->tqe_pnext = tqe->tqe_pnext;
		else    /* we must be the tail anyway */
			ifq->ifq_tail = tqe->tqe_pnext;

		tqe->tqe_pnext = NULL;
		tqe->tqe_ifq = NULL;
	}

	(void) fr_deletetimeoutqueue(ifq);

	MUTEX_EXIT(&ifq->ifq_lock);
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_queuefront                                               */
/* Returns:     Nil                                                         */
/* Parameters:  tqe(I) - pointer to timeout queue entry                     */
/*                                                                          */
/* Move a queue entry to the front of the queue, if it isn't already there. */
/* ------------------------------------------------------------------------ */
void fr_queuefront(tqe)
ipftqent_t *tqe;
{
	ipftq_t *ifq;

	ifq = tqe->tqe_ifq;
	if (ifq == NULL)
		return;

	MUTEX_ENTER(&ifq->ifq_lock);
	if (ifq->ifq_head != tqe) {
		*tqe->tqe_pnext = tqe->tqe_next;
		if (tqe->tqe_next)
			tqe->tqe_next->tqe_pnext = tqe->tqe_pnext;
		else
			ifq->ifq_tail = tqe->tqe_pnext;

		tqe->tqe_next = ifq->ifq_head;
		ifq->ifq_head->tqe_pnext = &tqe->tqe_next;
		ifq->ifq_head = tqe;
		tqe->tqe_pnext = &ifq->ifq_head;
	}
	MUTEX_EXIT(&ifq->ifq_lock);
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_queueback                                                */
/* Returns:     Nil                                                         */
/* Parameters:  tqe(I) - pointer to timeout queue entry                     */
/*                                                                          */
/* Move a queue entry to the back of the queue, if it isn't already there.  */
/* ------------------------------------------------------------------------ */
void fr_queueback(tqe)
ipftqent_t *tqe;
{
	ipftq_t *ifq;

	ifq = tqe->tqe_ifq;
	if (ifq == NULL)
		return;
	tqe->tqe_die = fr_ticks + ifq->ifq_ttl;

	MUTEX_ENTER(&ifq->ifq_lock);
	if (tqe->tqe_next != NULL) {		/* at the end already ? */
		/*
		 * Remove from list
		 */
		*tqe->tqe_pnext = tqe->tqe_next;
		tqe->tqe_next->tqe_pnext = tqe->tqe_pnext;

		/*
		 * Make it the last entry.
		 */
		tqe->tqe_next = NULL;
		tqe->tqe_pnext = ifq->ifq_tail;
		*ifq->ifq_tail = tqe;
		ifq->ifq_tail = &tqe->tqe_next;
	}
	MUTEX_EXIT(&ifq->ifq_lock);
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_queueappend                                              */
/* Returns:     Nil                                                         */
/* Parameters:  tqe(I)    - pointer to timeout queue entry                  */
/*              ifq(I)    - pointer to timeout queue                        */
/*              parent(I) - owing object pointer                            */
/*                                                                          */
/* Add a new item to this queue and put it on the very end.                 */
/* ------------------------------------------------------------------------ */
void fr_queueappend(tqe, ifq, parent)
ipftqent_t *tqe;
ipftq_t *ifq;
void *parent;
{

	MUTEX_ENTER(&ifq->ifq_lock);
	tqe->tqe_parent = parent;
	tqe->tqe_pnext = ifq->ifq_tail;
	*ifq->ifq_tail = tqe;
	ifq->ifq_tail = &tqe->tqe_next;
	tqe->tqe_next = NULL;
	tqe->tqe_ifq = ifq;
	tqe->tqe_die = fr_ticks + ifq->ifq_ttl;
	ifq->ifq_ref++;
	MUTEX_EXIT(&ifq->ifq_lock);
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_movequeue                                                */
/* Returns:     Nil                                                         */
/* Parameters:  tq(I)   - pointer to timeout queue information              */
/*              oifp(I) - old timeout queue entry was on                    */
/*              nifp(I) - new timeout queue to put entry on                 */
/*                                                                          */
/* Move a queue entry from one timeout queue to another timeout queue.      */
/* If it notices that the current entry is already last and does not need   */
/* to move queue, the return.                                               */
/* ------------------------------------------------------------------------ */
void fr_movequeue(tqe, oifq, nifq)
ipftqent_t *tqe;
ipftq_t *oifq, *nifq;
{
	/*
	 * Is the operation here going to be a no-op ?
	 */
	MUTEX_ENTER(&oifq->ifq_lock);
	if ((oifq != nifq) || (*oifq->ifq_tail != tqe)) {
		/*
		 * Remove from the old queue
		 */
		*tqe->tqe_pnext = tqe->tqe_next;
		if (tqe->tqe_next)
			tqe->tqe_next->tqe_pnext = tqe->tqe_pnext;
		else
			oifq->ifq_tail = tqe->tqe_pnext;
		tqe->tqe_next = NULL;

		/*
		 * If we're moving from one queue to another, release the
		 * lock on the old queue and get a lock on the new queue.
		 * For user defined queues, if we're moving off it, call
		 * delete in case it can now be freed.
		 */
		if (oifq != nifq) {
			tqe->tqe_ifq = NULL;

			(void) fr_deletetimeoutqueue(oifq);

			MUTEX_EXIT(&oifq->ifq_lock);

			MUTEX_ENTER(&nifq->ifq_lock);

			tqe->tqe_ifq = nifq;
			nifq->ifq_ref++;
		}

		/*
		 * Add to the bottom of the new queue
		 */
		tqe->tqe_die = fr_ticks + nifq->ifq_ttl;
		tqe->tqe_pnext = nifq->ifq_tail;
		*nifq->ifq_tail = tqe;
		nifq->ifq_tail = &tqe->tqe_next;
	}
	MUTEX_EXIT(&nifq->ifq_lock);
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_updateipid                                               */
/* Returns:     int - 0 == success, -1 == error (packet should be droppped) */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* When we are doing NAT, change the IP of every packet to represent a      */
/* single sequence of packets coming from the host, hiding any host         */
/* specific sequencing that might otherwise be revealed.  If the packet is  */
/* a fragment, then store the 'new' IPid in the fragment cache and look up  */
/* the fragment cache for non-leading fragments.  If a non-leading fragment */
/* has no match in the cache, return an error.                              */
/* ------------------------------------------------------------------------ */
static int fr_updateipid(fin)
fr_info_t *fin;
{
	u_short id, ido, sums;
	u_32_t sumd, sum;
	ip_t *ip;

	if (fin->fin_off != 0) {
		sum = fr_ipid_knownfrag(fin);
		if (sum == 0xffffffff)
			return -1;
		sum &= 0xffff;
		id = (u_short)sum;
	} else {
		id = fr_nextipid(fin);
		if (fin->fin_off == 0 && (fin->fin_flx & FI_FRAG) != 0)
			(void) fr_ipid_newfrag(fin, (u_32_t)id);
	}

	ip = fin->fin_ip;
	ido = ntohs(ip->ip_id);
	if (id == ido)
		return 0;
	ip->ip_id = htons(id);
	CALC_SUMD(ido, id, sumd);	/* DESTRUCTIVE MACRO! id,ido change */
	sum = (~ntohs(ip->ip_sum)) & 0xffff;
	sum += sumd;
	sum = (sum >> 16) + (sum & 0xffff);
	sum = (sum >> 16) + (sum & 0xffff);
	sums = ~(u_short)sum;
	ip->ip_sum = htons(sums);
	return 0;
}


#ifdef	NEED_FRGETIFNAME
/* ------------------------------------------------------------------------ */
/* Function:    fr_getifname                                                */
/* Returns:     char *    - pointer to interface name                       */
/* Parameters:  ifp(I)    - pointer to network interface                    */
/*              buffer(O) - pointer to where to store interface name        */
/*                                                                          */
/* Constructs an interface name in the buffer passed.  The buffer passed is */
/* expected to be at least LIFNAMSIZ in bytes big.  If buffer is passed in  */
/* as a NULL pointer then return a pointer to a static array.               */
/* ------------------------------------------------------------------------ */
char *fr_getifname(ifp, buffer)
struct ifnet *ifp;
char *buffer;
{
	static char namebuf[LIFNAMSIZ];
# if defined(MENTAT) || defined(__FreeBSD__) || defined(__osf__) || \
     defined(__sgi) || defined(linux) || defined(_AIX51) || \
     (defined(sun) && !defined(__SVR4) && !defined(__svr4__))
	int unit, space;
	char temp[20];
	char *s;
# endif

	if (buffer == NULL)
		buffer = namebuf;
	(void) strncpy(buffer, ifp->if_name, LIFNAMSIZ);
	buffer[LIFNAMSIZ - 1] = '\0';
# if defined(MENTAT) || defined(__FreeBSD__) || defined(__osf__) || \
     defined(__sgi) || defined(_AIX51) || \
     (defined(sun) && !defined(__SVR4) && !defined(__svr4__))
	for (s = buffer; *s; s++)
		;
	unit = ifp->if_unit;
	space = LIFNAMSIZ - (s - buffer);
	if (space > 0) {
#  if defined(SNPRINTF) && defined(_KERNEL)
		SNPRINTF(temp, sizeof(temp), "%d", unit);
#  else
		(void) sprintf(temp, "%d", unit);
#  endif
		(void) strncpy(s, temp, space);
	}
# endif
	return buffer;
}
#endif


/* ------------------------------------------------------------------------ */
/* Function:    fr_ioctlswitch                                              */
/* Returns:     int     - -1 continue processing, else ioctl return value   */
/* Parameters:  unit(I) - device unit opened                                */
/*              data(I) - pointer to ioctl data                             */
/*              cmd(I)  - ioctl command                                     */
/*              mode(I) - mode value                                        */
/*              uid(I)  - uid making the ioctl call                         */
/*              ctx(I)  - pointer to context data                           */
/*                                                                          */
/* Based on the value of unit, call the appropriate ioctl handler or return */
/* EIO if ipfilter is not running.   Also checks if write perms are req'd   */
/* for the device in order to execute the ioctl.                            */
/* ------------------------------------------------------------------------ */
int fr_ioctlswitch(unit, data, cmd, mode, uid, ctx)
int unit, mode, uid;
ioctlcmd_t cmd;
void *data, *ctx;
{
	int error = 0;

	switch (unit)
	{
	case IPL_LOGIPF :
		error = fr_ipf_ioctl(data, cmd, mode, uid, ctx);
		break;
	case IPL_LOGNAT :
		if (fr_running > 0)
			error = fr_nat_ioctl(data, cmd, mode, uid, ctx);
		else
			error = EIO;
		break;
	case IPL_LOGSTATE :
		if (fr_running > 0)
			error = fr_state_ioctl(data, cmd, mode, uid, ctx);
		else
			error = EIO;
		break;
	case IPL_LOGAUTH :
		if (fr_running > 0)
			error = fr_auth_ioctl(data, cmd, mode, uid, ctx);
		else
			error = EIO;
		break;
	case IPL_LOGSYNC :
#ifdef IPFILTER_SYNC
		if (fr_running > 0)
			error = fr_sync_ioctl(data, cmd, mode, uid, ctx);
		else
#endif
			error = EIO;
		break;
	case IPL_LOGSCAN :
#ifdef IPFILTER_SCAN
		if (fr_running > 0)
			error = fr_scan_ioctl(data, cmd, mode, uid, ctx);
		else
#endif
			error = EIO;
		break;
	case IPL_LOGLOOKUP :
#ifdef IPFILTER_LOOKUP
		if (fr_running > 0)
			error = ip_lookup_ioctl(data, cmd, mode, uid, ctx);
		else
#endif
			error = EIO;
		break;
	default :
		error = EIO;
		break;
	}

	return error;
}


/*
 * This array defines the expected size of objects coming into the kernel
 * for the various recognised object types.
 */
static	int	fr_objbytes[IPFOBJ_COUNT][2] = {
	{ 1,	sizeof(struct frentry) },		/* frentry */
	{ 0,	sizeof(struct friostat) },
	{ 0,	sizeof(struct fr_info) },
	{ 0,	sizeof(struct fr_authstat) },
	{ 0,	sizeof(struct ipfrstat) },
	{ 0,	sizeof(struct ipnat) },
	{ 0,	sizeof(struct natstat) },
	{ 0,	sizeof(struct ipstate_save) },
	{ 1,	sizeof(struct nat_save) },		/* nat_save */
	{ 0,	sizeof(struct natlookup) },
	{ 1,	sizeof(struct ipstate) },		/* ipstate */
	{ 0,	sizeof(struct ips_stat) },
	{ 0,	sizeof(struct frauth) },
	{ 0,	sizeof(struct ipftune) },
	{ 0,	sizeof(struct nat) },			/* nat_t */
	{ 0,	sizeof(struct ipfruleiter) },
	{ 0,	sizeof(struct ipfgeniter) },
	{ 0,	sizeof(struct ipftable) },
	{ 0,	sizeof(struct ipflookupiter) },
	{ 0,	sizeof(struct ipftq) * IPF_TCP_NSTATES },
};


/* ------------------------------------------------------------------------ */
/* Function:    fr_inobj                                                    */
/* Returns:     int     - 0 = success, else failure                         */
/* Parameters:  data(I) - pointer to ioctl data                             */
/*              ptr(I)  - pointer to store real data in                     */
/*              type(I) - type of structure being moved                     */
/*                                                                          */
/* Copy in the contents of what the ipfobj_t points to.  In future, we      */
/* add things to check for version numbers, sizes, etc, to make it backward */
/* compatible at the ABI for user land.                                     */
/* ------------------------------------------------------------------------ */
int fr_inobj(data, ptr, type)
void *data;
void *ptr;
int type;
{
	ipfobj_t obj;
	int error = 0;

	if ((type < 0) || (type >= IPFOBJ_COUNT))
		return EINVAL;

	error = BCOPYIN(data, &obj, sizeof(obj));
	if (error != 0)
		return EFAULT;

	if (obj.ipfo_type != type)
		return EINVAL;

#ifndef	IPFILTER_COMPAT
	if ((fr_objbytes[type][0] & 1) != 0) {
		if (obj.ipfo_size < fr_objbytes[type][1])
			return EINVAL;
	} else if (obj.ipfo_size != fr_objbytes[type][1]) {
		return EINVAL;
	}
#else
	if (obj.ipfo_rev != IPFILTER_VERSION)
		/* XXX compatibility hook here */
		;
	if ((fr_objbytes[type][0] & 1) != 0) {
		if (obj.ipfo_size < fr_objbytes[type][1])
			/* XXX compatibility hook here */
			return EINVAL;
	} else if (obj.ipfo_size != fr_objbytes[type][1])
		/* XXX compatibility hook here */
		return EINVAL;
#endif

	if ((fr_objbytes[type][0] & 1) != 0) {
		error = COPYIN(obj.ipfo_ptr, ptr, fr_objbytes[type][1]);
	} else {
		error = COPYIN(obj.ipfo_ptr, ptr, obj.ipfo_size);
	}
	if (error != 0)
		error = EFAULT;
	return error;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_inobjsz                                                  */
/* Returns:     int     - 0 = success, else failure                         */
/* Parameters:  data(I) - pointer to ioctl data                             */
/*              ptr(I)  - pointer to store real data in                     */
/*              type(I) - type of structure being moved                     */
/*              sz(I)   - size of data to copy                              */
/*                                                                          */
/* As per fr_inobj, except the size of the object to copy in is passed in   */
/* but it must not be smaller than the size defined for the type and the    */
/* type must allow for varied sized objects.  The extra requirement here is */
/* that sz must match the size of the object being passed in - this is not  */
/* not possible nor required in fr_inobj().                                 */
/* ------------------------------------------------------------------------ */
int fr_inobjsz(data, ptr, type, sz)
void *data;
void *ptr;
int type, sz;
{
	ipfobj_t obj;
	int error;

	if ((type < 0) || (type >= IPFOBJ_COUNT))
		return EINVAL;
	if (((fr_objbytes[type][0] & 1) == 0) || (sz < fr_objbytes[type][1]))
		return EINVAL;

	error = BCOPYIN(data, &obj, sizeof(obj));
	if (error != 0)
		return EFAULT;

	if (obj.ipfo_type != type)
		return EINVAL;

#ifndef	IPFILTER_COMPAT
	if (obj.ipfo_size != sz)
		return EINVAL;
#else
	if (obj.ipfo_rev != IPFILTER_VERSION)
		/* XXX compatibility hook here */
		;
	if (obj.ipfo_size != sz)
		/* XXX compatibility hook here */
		return EINVAL;
#endif

	error = COPYIN(obj.ipfo_ptr, ptr, sz);
	if (error != 0)
		error = EFAULT;
	return error;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_outobjsz                                                 */
/* Returns:     int     - 0 = success, else failure                         */
/* Parameters:  data(I) - pointer to ioctl data                             */
/*              ptr(I)  - pointer to store real data in                     */
/*              type(I) - type of structure being moved                     */
/*              sz(I)   - size of data to copy                              */
/*                                                                          */
/* As per fr_outobj, except the size of the object to copy out is passed in */
/* but it must not be smaller than the size defined for the type and the    */
/* type must allow for varied sized objects.  The extra requirement here is */
/* that sz must match the size of the object being passed in - this is not  */
/* not possible nor required in fr_outobj().                                */
/* ------------------------------------------------------------------------ */
int fr_outobjsz(data, ptr, type, sz)
void *data;
void *ptr;
int type, sz;
{
	ipfobj_t obj;
	int error;

	if ((type < 0) || (type >= IPFOBJ_COUNT) ||
	    ((fr_objbytes[type][0] & 1) == 0) ||
	    (sz < fr_objbytes[type][1]))
		return EINVAL;

	error = BCOPYIN(data, &obj, sizeof(obj));
	if (error != 0)
		return EFAULT;

	if (obj.ipfo_type != type)
		return EINVAL;

#ifndef	IPFILTER_COMPAT
	if (obj.ipfo_size != sz)
		return EINVAL;
#else
	if (obj.ipfo_rev != IPFILTER_VERSION)
		/* XXX compatibility hook here */
		;
	if (obj.ipfo_size != sz)
		/* XXX compatibility hook here */
		return EINVAL;
#endif

	error = COPYOUT(ptr, obj.ipfo_ptr, sz);
	if (error != 0)
		error = EFAULT;
	return error;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_outobj                                                   */
/* Returns:     int     - 0 = success, else failure                         */
/* Parameters:  data(I) - pointer to ioctl data                             */
/*              ptr(I)  - pointer to store real data in                     */
/*              type(I) - type of structure being moved                     */
/*                                                                          */
/* Copy out the contents of what ptr is to where ipfobj points to.  In      */
/* future, we add things to check for version numbers, sizes, etc, to make  */
/* it backward  compatible at the ABI for user land.                        */
/* ------------------------------------------------------------------------ */
int fr_outobj(data, ptr, type)
void *data;
void *ptr;
int type;
{
	ipfobj_t obj;
	int error;

	if ((type < 0) || (type >= IPFOBJ_COUNT))
		return EINVAL;

	error = BCOPYIN(data, &obj, sizeof(obj));
	if (error != 0)
		return EFAULT;

	if (obj.ipfo_type != type)
		return EINVAL;

#ifndef	IPFILTER_COMPAT
	if ((fr_objbytes[type][0] & 1) != 0) {
		if (obj.ipfo_size < fr_objbytes[type][1])
			return EINVAL;
	} else if (obj.ipfo_size != fr_objbytes[type][1])
		return EINVAL;
#else
	if (obj.ipfo_rev != IPFILTER_VERSION)
		/* XXX compatibility hook here */
		;
	if ((fr_objbytes[type][0] & 1) != 0) {
		if (obj.ipfo_size < fr_objbytes[type][1])
			/* XXX compatibility hook here */
			return EINVAL;
	} else if (obj.ipfo_size != fr_objbytes[type][1])
		/* XXX compatibility hook here */
		return EINVAL;
#endif

	error = COPYOUT(ptr, obj.ipfo_ptr, obj.ipfo_size);
	if (error != 0)
		error = EFAULT;
	return error;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_checkl4sum                                               */
/* Returns:     int     - 0 = good, -1 = bad, 1 = cannot check              */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* If possible, calculate the layer 4 checksum for the packet.  If this is  */
/* not possible, return without indicating a failure or success but in a    */
/* way that is ditinguishable.                                              */
/* ------------------------------------------------------------------------ */
int fr_checkl4sum(fin)
fr_info_t *fin;
{
	u_short sum, hdrsum, *csump;
	udphdr_t *udp;
	int dosum;

	if ((fin->fin_flx & FI_NOCKSUM) != 0)
		return 0;

	if (fin->fin_cksum == 1)
		return 0;

	if (fin->fin_cksum == -1)
		return -1;

	/*
	 * If the TCP packet isn't a fragment, isn't too short and otherwise
	 * isn't already considered "bad", then validate the checksum.  If
	 * this check fails then considered the packet to be "bad".
	 */
	if ((fin->fin_flx & (FI_FRAG|FI_SHORT|FI_BAD)) != 0)
		return 1;

	csump = NULL;
	hdrsum = 0;
	dosum = 0;
	sum = 0;

#if SOLARIS && defined(_KERNEL) && (SOLARIS2 >= 6) && defined(ICK_VALID)
	if (dohwcksum && ((*fin->fin_mp)->b_ick_flag == ICK_VALID)) {
		hdrsum = 0;
		sum = 0;
	} else {
#endif
		switch (fin->fin_p)
		{
		case IPPROTO_TCP :
			csump = &((tcphdr_t *)fin->fin_dp)->th_sum;
			dosum = 1;
			break;

		case IPPROTO_UDP :
			udp = fin->fin_dp;
			if (udp->uh_sum != 0) {
				csump = &udp->uh_sum;
				dosum = 1;
			}
			break;

		case IPPROTO_ICMP :
			csump = &((struct icmp *)fin->fin_dp)->icmp_cksum;
			dosum = 1;
			break;

		default :
			return 1;
			/*NOTREACHED*/
		}

		if (csump != NULL)
			hdrsum = *csump;

		if (dosum) {
			sum = fr_cksum(fin->fin_m, fin->fin_ip,
				       fin->fin_p, fin->fin_dp,
				       fin->fin_dlen + fin->fin_hlen);
		}
#if SOLARIS && defined(_KERNEL) && (SOLARIS2 >= 6) && defined(ICK_VALID)
	}
#endif
#if !defined(_KERNEL)
	if (sum == hdrsum) {
		FR_DEBUG(("checkl4sum: %hx == %hx\n", sum, hdrsum));
	} else {
		FR_DEBUG(("checkl4sum: %hx != %hx\n", sum, hdrsum));
	}
#endif
	if (hdrsum == sum) {
		fin->fin_cksum = 1;
		return 0;
	}
	fin->fin_cksum = -1;
	return -1;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_ifpfillv4addr                                            */
/* Returns:     int     - 0 = address update, -1 = address not updated      */
/* Parameters:  atype(I)   - type of network address update to perform      */
/*              sin(I)     - pointer to source of address information       */
/*              mask(I)    - pointer to source of netmask information       */
/*              inp(I)     - pointer to destination address store           */
/*              inpmask(I) - pointer to destination netmask store           */
/*                                                                          */
/* Given a type of network address update (atype) to perform, copy          */
/* information from sin/mask into inp/inpmask.  If ipnmask is NULL then no  */
/* netmask update is performed unless FRI_NETMASKED is passed as atype, in  */
/* which case the operation fails.  For all values of atype other than      */
/* FRI_NETMASKED, if inpmask is non-NULL then the mask is set to an all 1s  */
/* value.                                                                   */
/* ------------------------------------------------------------------------ */
int fr_ifpfillv4addr(atype, sin, mask, inp, inpmask)
int atype;
struct sockaddr_in *sin, *mask;
struct in_addr *inp, *inpmask;
{
	if (inpmask != NULL && atype != FRI_NETMASKED)
		inpmask->s_addr = 0xffffffff;

	if (atype == FRI_NETWORK || atype == FRI_NETMASKED) {
		if (atype == FRI_NETMASKED) {
			if (inpmask == NULL)
				return -1;
			inpmask->s_addr = mask->sin_addr.s_addr;
		}
		inp->s_addr = sin->sin_addr.s_addr & mask->sin_addr.s_addr;
	} else {
		inp->s_addr = sin->sin_addr.s_addr;
	}
	return 0;
}


#ifdef	USE_INET6
/* ------------------------------------------------------------------------ */
/* Function:    fr_ifpfillv6addr                                            */
/* Returns:     int     - 0 = address update, -1 = address not updated      */
/* Parameters:  atype(I)   - type of network address update to perform      */
/*              sin(I)     - pointer to source of address information       */
/*              mask(I)    - pointer to source of netmask information       */
/*              inp(I)     - pointer to destination address store           */
/*              inpmask(I) - pointer to destination netmask store           */
/*                                                                          */
/* Given a type of network address update (atype) to perform, copy          */
/* information from sin/mask into inp/inpmask.  If ipnmask is NULL then no  */
/* netmask update is performed unless FRI_NETMASKED is passed as atype, in  */
/* which case the operation fails.  For all values of atype other than      */
/* FRI_NETMASKED, if inpmask is non-NULL then the mask is set to an all 1s  */
/* value.                                                                   */
/* ------------------------------------------------------------------------ */
int fr_ifpfillv6addr(atype, sin, mask, inp, inpmask)
int atype;
struct sockaddr_in6 *sin, *mask;
struct in_addr *inp, *inpmask;
{
	i6addr_t *src, *dst, *and, *dmask;

	src = (i6addr_t *)&sin->sin6_addr;
	and = (i6addr_t *)&mask->sin6_addr;
	dst = (i6addr_t *)inp;
	dmask = (i6addr_t *)inpmask;

	if (inpmask != NULL && atype != FRI_NETMASKED) {
		dmask->i6[0] = 0xffffffff;
		dmask->i6[1] = 0xffffffff;
		dmask->i6[2] = 0xffffffff;
		dmask->i6[3] = 0xffffffff;
	}

	if (atype == FRI_NETWORK || atype == FRI_NETMASKED) {
		if (atype == FRI_NETMASKED) {
			if (inpmask == NULL)
				return -1;
			dmask->i6[0] = and->i6[0];
			dmask->i6[1] = and->i6[1];
			dmask->i6[2] = and->i6[2];
			dmask->i6[3] = and->i6[3];
		}

		dst->i6[0] = src->i6[0] & and->i6[0];
		dst->i6[1] = src->i6[1] & and->i6[1];
		dst->i6[2] = src->i6[2] & and->i6[2];
		dst->i6[3] = src->i6[3] & and->i6[3];
	} else {
		dst->i6[0] = src->i6[0];
		dst->i6[1] = src->i6[1];
		dst->i6[2] = src->i6[2];
		dst->i6[3] = src->i6[3];
	}
	return 0;
}
#endif


/* ------------------------------------------------------------------------ */
/* Function:    fr_matchtag                                                 */
/* Returns:     0 == mismatch, 1 == match.                                  */
/* Parameters:  tag1(I) - pointer to first tag to compare                   */
/*              tag2(I) - pointer to second tag to compare                  */
/*                                                                          */
/* Returns true (non-zero) or false(0) if the two tag structures can be     */
/* considered to be a match or not match, respectively.  The tag is 16      */
/* bytes long (16 characters) but that is overlayed with 4 32bit ints so    */
/* compare the ints instead, for speed. tag1 is the master of the           */
/* comparison.  This function should only be called with both tag1 and tag2 */
/* as non-NULL pointers.                                                    */
/* ------------------------------------------------------------------------ */
int fr_matchtag(tag1, tag2)
ipftag_t *tag1, *tag2;
{
	if (tag1 == tag2)
		return 1;

	if ((tag1->ipt_num[0] == 0) && (tag2->ipt_num[0] == 0))
		return 1;

	if ((tag1->ipt_num[0] == tag2->ipt_num[0]) &&
	    (tag1->ipt_num[1] == tag2->ipt_num[1]) &&
	    (tag1->ipt_num[2] == tag2->ipt_num[2]) &&
	    (tag1->ipt_num[3] == tag2->ipt_num[3]))
		return 1;
	return 0;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_coalesce                                                 */
/* Returns:     1 == success, -1 == failure, 0 == no change                 */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* Attempt to get all of the packet data into a single, contiguous buffer.  */
/* If this call returns a failure then the buffers have also been freed.    */
/* ------------------------------------------------------------------------ */
int fr_coalesce(fin)
fr_info_t *fin;
{
	if ((fin->fin_flx & FI_COALESCE) != 0)
		return 1;

	/*
	 * If the mbuf pointers indicate that there is no mbuf to work with,
	 * return but do not indicate success or failure.
	 */
	if (fin->fin_m == NULL || fin->fin_mp == NULL)
		return 0;

#if defined(_KERNEL)
	if (fr_pullup(fin->fin_m, fin, fin->fin_plen) == NULL) {
		ATOMIC_INCL(fr_badcoalesces[fin->fin_out]);
# ifdef MENTAT
		FREE_MB_T(*fin->fin_mp);
# endif
		*fin->fin_mp = NULL;
		fin->fin_m = NULL;
		return -1;
	}
#else
	fin = fin;	/* LINT */
#endif
	return 1;
}


/*
 * The following table lists all of the tunable variables that can be
 * accessed via SIOCIPFGET/SIOCIPFSET/SIOCIPFGETNEXt.  The format of each row
 * in the table below is as follows:
 *
 * pointer to value, name of value, minimum, maximum, size of the value's
 *     container, value attribute flags
 *
 * For convienience, IPFT_RDONLY means the value is read-only, IPFT_WRDISABLED
 * means the value can only be written to when IPFilter is loaded but disabled.
 * The obvious implication is if neither of these are set then the value can be
 * changed at any time without harm.
 */
ipftuneable_t ipf_tuneables[] = {
	/* filtering */
	{ { &fr_flags },	"fr_flags",		0,	0xffffffff,
		sizeof(fr_flags),		0,	NULL },
	{ { &fr_active },	"fr_active",		0,	0,
		sizeof(fr_active),		IPFT_RDONLY,	NULL },
	{ { &fr_control_forwarding },	"fr_control_forwarding",	0, 1,
		sizeof(fr_control_forwarding),	0,	NULL },
	{ { &fr_update_ipid },	"fr_update_ipid",	0,	1,
		sizeof(fr_update_ipid),		0,	NULL },
	{ { &fr_chksrc },	"fr_chksrc",		0,	1,
		sizeof(fr_chksrc),		0,	NULL },
	{ { &fr_minttl },	"fr_minttl",		0,	1,
		sizeof(fr_minttl),		0,	NULL },
	{ { &fr_icmpminfragmtu }, "fr_icmpminfragmtu",	0,	1,
		sizeof(fr_icmpminfragmtu),	0,	NULL },
	{ { &fr_pass },		"fr_pass",		0,	0xffffffff,
		sizeof(fr_pass),		0,	NULL },
	/* state */
	{ { &fr_tcpidletimeout }, "fr_tcpidletimeout",	1,	0x7fffffff,
		sizeof(fr_tcpidletimeout),	IPFT_WRDISABLED,	NULL },
	{ { &fr_tcpclosewait },	"fr_tcpclosewait",	1,	0x7fffffff,
		sizeof(fr_tcpclosewait),	IPFT_WRDISABLED,	NULL },
	{ { &fr_tcplastack },	"fr_tcplastack",	1,	0x7fffffff,
		sizeof(fr_tcplastack),		IPFT_WRDISABLED,	NULL },
	{ { &fr_tcptimeout },	"fr_tcptimeout",	1,	0x7fffffff,
		sizeof(fr_tcptimeout),		IPFT_WRDISABLED,	NULL },
	{ { &fr_tcpclosed },	"fr_tcpclosed",		1,	0x7fffffff,
		sizeof(fr_tcpclosed),		IPFT_WRDISABLED,	NULL },
	{ { &fr_tcphalfclosed }, "fr_tcphalfclosed",	1,	0x7fffffff,
		sizeof(fr_tcphalfclosed),	IPFT_WRDISABLED,	NULL },
	{ { &fr_udptimeout },	"fr_udptimeout",	1,	0x7fffffff,
		sizeof(fr_udptimeout),		IPFT_WRDISABLED,	NULL },
	{ { &fr_udpacktimeout }, "fr_udpacktimeout",	1,	0x7fffffff,
		sizeof(fr_udpacktimeout),	IPFT_WRDISABLED,	NULL },
	{ { &fr_icmptimeout },	"fr_icmptimeout",	1,	0x7fffffff,
		sizeof(fr_icmptimeout),		IPFT_WRDISABLED,	NULL },
	{ { &fr_icmpacktimeout }, "fr_icmpacktimeout",	1,	0x7fffffff,
		sizeof(fr_icmpacktimeout),	IPFT_WRDISABLED,	NULL },
	{ { &fr_iptimeout }, "fr_iptimeout",		1,	0x7fffffff,
		sizeof(fr_iptimeout),		IPFT_WRDISABLED,	NULL },
	{ { &fr_statemax },	"fr_statemax",		1,	0x7fffffff,
		sizeof(fr_statemax),		0,	NULL },
	{ { &fr_statesize },	"fr_statesize",		1,	0x7fffffff,
		sizeof(fr_statesize),		IPFT_WRDISABLED,	NULL },
	{ { &fr_state_lock },	"fr_state_lock",	0,	1,
		sizeof(fr_state_lock),		IPFT_RDONLY,	NULL },
	{ { &fr_state_maxbucket }, "fr_state_maxbucket", 1,	0x7fffffff,
		sizeof(fr_state_maxbucket),	IPFT_WRDISABLED,	NULL },
	{ { &fr_state_maxbucket_reset }, "fr_state_maxbucket_reset",	0, 1,
		sizeof(fr_state_maxbucket_reset), IPFT_WRDISABLED,	NULL },
	{ { &ipstate_logging },	"ipstate_logging",	0,	1,
		sizeof(ipstate_logging),	0,	NULL },
	/* nat */
	{ { &fr_nat_lock },		"fr_nat_lock",		0,	1,
		sizeof(fr_nat_lock),		IPFT_RDONLY,	NULL },
	{ { &ipf_nattable_sz },	"ipf_nattable_sz",	1,	0x7fffffff,
		sizeof(ipf_nattable_sz),	IPFT_WRDISABLED,	NULL },
	{ { &ipf_nattable_max }, "ipf_nattable_max",	1,	0x7fffffff,
		sizeof(ipf_nattable_max),	0,	NULL },
	{ { &ipf_natrules_sz },	"ipf_natrules_sz",	1,	0x7fffffff,
		sizeof(ipf_natrules_sz),	IPFT_WRDISABLED,	NULL },
	{ { &ipf_rdrrules_sz },	"ipf_rdrrules_sz",	1,	0x7fffffff,
		sizeof(ipf_rdrrules_sz),	IPFT_WRDISABLED,	NULL },
	{ { &ipf_hostmap_sz },	"ipf_hostmap_sz",	1,	0x7fffffff,
		sizeof(ipf_hostmap_sz),		IPFT_WRDISABLED,	NULL },
	{ { &fr_nat_maxbucket }, "fr_nat_maxbucket",	1,	0x7fffffff,
		sizeof(fr_nat_maxbucket),	0,			NULL },
	{ { &fr_nat_maxbucket_reset },	"fr_nat_maxbucket_reset",	0, 1,
		sizeof(fr_nat_maxbucket_reset),	IPFT_WRDISABLED,	NULL },
	{ { &nat_logging },		"nat_logging",		0,	1,
		sizeof(nat_logging),		0,	NULL },
	{ { &fr_defnatage },	"fr_defnatage",		1,	0x7fffffff,
		sizeof(fr_defnatage),		IPFT_WRDISABLED,	NULL },
	{ { &fr_defnatipage },	"fr_defnatipage",	1,	0x7fffffff,
		sizeof(fr_defnatipage),		IPFT_WRDISABLED,	NULL },
	{ { &fr_defnaticmpage }, "fr_defnaticmpage",	1,	0x7fffffff,
		sizeof(fr_defnaticmpage),	IPFT_WRDISABLED,	NULL },
	{ { &fr_nat_doflush }, "fr_nat_doflush",	0,	1,
		sizeof(fr_nat_doflush),		0,	NULL },
	/* proxy */
	{ { &ipf_proxy_debug }, "ipf_proxy_debug",	0,	10,
		sizeof(ipf_proxy_debug),	0,	0 },
	/* frag */
	{ { &ipfr_size },	"ipfr_size",		1,	0x7fffffff,
		sizeof(ipfr_size),		IPFT_WRDISABLED,	NULL },
	{ { &fr_ipfrttl },	"fr_ipfrttl",		1,	0x7fffffff,
		sizeof(fr_ipfrttl),		IPFT_WRDISABLED,	NULL },
#ifdef IPFILTER_LOG
	/* log */
	{ { &ipl_suppress },	"ipl_suppress",		0,	1,
		sizeof(ipl_suppress),		0,	NULL },
	{ { &ipl_logmax },	"ipl_logmax",		0,	0x7fffffff,
		sizeof(ipl_logmax),		IPFT_WRDISABLED,	NULL },
	{ { &ipl_logall },	"ipl_logall",		0,	1,
		sizeof(ipl_logall),		0,	NULL },
	{ { &ipl_logsize },	"ipl_logsize",		0,	0x80000,
		sizeof(ipl_logsize),		0,	NULL },
#endif
	{ { NULL },		NULL,			0,	0,
		0,				0,	NULL }
};

static ipftuneable_t *ipf_tunelist = NULL;


/* ------------------------------------------------------------------------ */
/* Function:    fr_findtunebycookie                                         */
/* Returns:     NULL = search failed, else pointer to tune struct           */
/* Parameters:  cookie(I) - cookie value to search for amongst tuneables    */
/*              next(O)   - pointer to place to store the cookie for the    */
/*                          "next" tuneable, if it is desired.              */
/*                                                                          */
/* This function is used to walk through all of the existing tunables with  */
/* successive calls.  It searches the known tunables for the one which has  */
/* a matching value for "cookie" - ie its address.  When returning a match, */
/* the next one to be found may be returned inside next.                    */
/* ------------------------------------------------------------------------ */
static ipftuneable_t *fr_findtunebycookie(cookie, next)
void *cookie, **next;
{
	ipftuneable_t *ta, **tap;

	for (ta = ipf_tuneables; ta->ipft_name != NULL; ta++)
		if (ta == cookie) {
			if (next != NULL) {
				/*
				 * If the next entry in the array has a name
				 * present, then return a pointer to it for
				 * where to go next, else return a pointer to
				 * the dynaminc list as a key to search there
				 * next.  This facilitates a weak linking of
				 * the two "lists" together.
				 */
				if ((ta + 1)->ipft_name != NULL)
					*next = ta + 1;
				else
					*next = &ipf_tunelist;
			}
			return ta;
		}

	for (tap = &ipf_tunelist; (ta = *tap) != NULL; tap = &ta->ipft_next)
		if (tap == cookie) {
			if (next != NULL)
				*next = &ta->ipft_next;
			return ta;
		}

	if (next != NULL)
		*next = NULL;
	return NULL;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_findtunebyname                                           */
/* Returns:     NULL = search failed, else pointer to tune struct           */
/* Parameters:  name(I) - name of the tuneable entry to find.               */
/*                                                                          */
/* Search the static array of tuneables and the list of dynamic tuneables   */
/* for an entry with a matching name.  If we can find one, return a pointer */
/* to the matching structure.                                               */
/* ------------------------------------------------------------------------ */
static ipftuneable_t *fr_findtunebyname(name)
const char *name;
{
	ipftuneable_t *ta;

	for (ta = ipf_tuneables; ta->ipft_name != NULL; ta++)
		if (!strcmp(ta->ipft_name, name)) {
			return ta;
		}

	for (ta = ipf_tunelist; ta != NULL; ta = ta->ipft_next)
		if (!strcmp(ta->ipft_name, name)) {
			return ta;
		}

	return NULL;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_addipftune                                               */
/* Returns:     int - 0 == success, else failure                            */
/* Parameters:  newtune - pointer to new tune struct to add to tuneables    */
/*                                                                          */
/* Appends the tune structure pointer to by "newtune" to the end of the     */
/* current list of "dynamic" tuneable parameters.  Once added, the owner    */
/* of the object is not expected to ever change "ipft_next".                */
/* ------------------------------------------------------------------------ */
int fr_addipftune(newtune)
ipftuneable_t *newtune;
{
	ipftuneable_t *ta, **tap;

	ta = fr_findtunebyname(newtune->ipft_name);
	if (ta != NULL)
		return EEXIST;

	for (tap = &ipf_tunelist; *tap != NULL; tap = &(*tap)->ipft_next)
		;

	newtune->ipft_next = NULL;
	*tap = newtune;
	return 0;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_delipftune                                               */
/* Returns:     int - 0 == success, else failure                            */
/* Parameters:  oldtune - pointer to tune struct to remove from the list of */
/*                        current dynamic tuneables                         */
/*                                                                          */
/* Search for the tune structure, by pointer, in the list of those that are */
/* dynamically added at run time.  If found, adjust the list so that this   */
/* structure is no longer part of it.                                       */
/* ------------------------------------------------------------------------ */
int fr_delipftune(oldtune)
ipftuneable_t *oldtune;
{
	ipftuneable_t *ta, **tap;

	for (tap = &ipf_tunelist; (ta = *tap) != NULL; tap = &ta->ipft_next)
		if (ta == oldtune) {
			*tap = oldtune->ipft_next;
			oldtune->ipft_next = NULL;
			return 0;
		}

	return ESRCH;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_ipftune                                                  */
/* Returns:     int - 0 == success, else failure                            */
/* Parameters:  cmd(I)  - ioctl command number                              */
/*              data(I) - pointer to ioctl data structure                   */
/*                                                                          */
/* Implement handling of SIOCIPFGETNEXT, SIOCIPFGET and SIOCIPFSET.  These  */
/* three ioctls provide the means to access and control global variables    */
/* within IPFilter, allowing (for example) timeouts and table sizes to be   */
/* changed without rebooting, reloading or recompiling.  The initialisation */
/* and 'destruction' routines of the various components of ipfilter are all */
/* each responsible for handling their own values being too big.            */
/* ------------------------------------------------------------------------ */
int fr_ipftune(cmd, data)
ioctlcmd_t cmd;
void *data;
{
	ipftuneable_t *ta;
	ipftune_t tu;
	void *cookie;
	int error;

	error = fr_inobj(data, &tu, IPFOBJ_TUNEABLE);
	if (error != 0)
		return error;

	tu.ipft_name[sizeof(tu.ipft_name) - 1] = '\0';
	cookie = tu.ipft_cookie;
	ta = NULL;

	switch (cmd)
	{
	case SIOCIPFGETNEXT :
		/*
		 * If cookie is non-NULL, assume it to be a pointer to the last
		 * entry we looked at, so find it (if possible) and return a
		 * pointer to the next one after it.  The last entry in the
		 * the table is a NULL entry, so when we get to it, set cookie
		 * to NULL and return that, indicating end of list, erstwhile
		 * if we come in with cookie set to NULL, we are starting anew
		 * at the front of the list.
		 */
		if (cookie != NULL) {
			ta = fr_findtunebycookie(cookie, &tu.ipft_cookie);
		} else {
			ta = ipf_tuneables;
			tu.ipft_cookie = ta + 1;
		}
		if (ta != NULL) {
			/*
			 * Entry found, but does the data pointed to by that
			 * row fit in what we can return?
			 */
			if (ta->ipft_sz > sizeof(tu.ipft_un))
				return EINVAL;

			tu.ipft_vlong = 0;
			if (ta->ipft_sz == sizeof(u_long))
				tu.ipft_vlong = *ta->ipft_plong;
			else if (ta->ipft_sz == sizeof(u_int))
				tu.ipft_vint = *ta->ipft_pint;
			else if (ta->ipft_sz == sizeof(u_short))
				tu.ipft_vshort = *ta->ipft_pshort;
			else if (ta->ipft_sz == sizeof(u_char))
				tu.ipft_vchar = *ta->ipft_pchar;

			tu.ipft_sz = ta->ipft_sz;
			tu.ipft_min = ta->ipft_min;
			tu.ipft_max = ta->ipft_max;
			tu.ipft_flags = ta->ipft_flags;
			bcopy(ta->ipft_name, tu.ipft_name,
			      MIN(sizeof(tu.ipft_name),
				  strlen(ta->ipft_name) + 1));
		}
		error = fr_outobj(data, &tu, IPFOBJ_TUNEABLE);
		break;

	case SIOCIPFGET :
	case SIOCIPFSET :
		/*
		 * Search by name or by cookie value for a particular entry
		 * in the tuning paramter table.
		 */
		error = ESRCH;
		if (cookie != NULL) {
			ta = fr_findtunebycookie(cookie, NULL);
			if (ta != NULL)
				error = 0;
		} else if (tu.ipft_name[0] != '\0') {
			ta = fr_findtunebyname(tu.ipft_name);
			if (ta != NULL)
				error = 0;
		}
		if (error != 0)
			break;

		if (cmd == (ioctlcmd_t)SIOCIPFGET) {
			/*
			 * Fetch the tuning parameters for a particular value
			 */
			tu.ipft_vlong = 0;
			if (ta->ipft_sz == sizeof(u_long))
				tu.ipft_vlong = *ta->ipft_plong;
			else if (ta->ipft_sz == sizeof(u_int))
				tu.ipft_vint = *ta->ipft_pint;
			else if (ta->ipft_sz == sizeof(u_short))
				tu.ipft_vshort = *ta->ipft_pshort;
			else if (ta->ipft_sz == sizeof(u_char))
				tu.ipft_vchar = *ta->ipft_pchar;
			tu.ipft_cookie = ta;
			tu.ipft_sz = ta->ipft_sz;
			tu.ipft_min = ta->ipft_min;
			tu.ipft_max = ta->ipft_max;
			tu.ipft_flags = ta->ipft_flags;
			error = fr_outobj(data, &tu, IPFOBJ_TUNEABLE);

		} else if (cmd == (ioctlcmd_t)SIOCIPFSET) {
			/*
			 * Set an internal parameter.  The hard part here is
			 * getting the new value safely and correctly out of
			 * the kernel (given we only know its size, not type.)
			 */
			u_long in;

			if (((ta->ipft_flags & IPFT_WRDISABLED) != 0) &&
			    (fr_running > 0)) {
				error = EBUSY;
				break;
			}

			in = tu.ipft_vlong;
			if (in < ta->ipft_min || in > ta->ipft_max) {
				error = EINVAL;
				break;
			}

			if (ta->ipft_sz == sizeof(u_long)) {
				tu.ipft_vlong = *ta->ipft_plong;
				*ta->ipft_plong = in;
			} else if (ta->ipft_sz == sizeof(u_int)) {
				tu.ipft_vint = *ta->ipft_pint;
				*ta->ipft_pint = (u_int)(in & 0xffffffff);
			} else if (ta->ipft_sz == sizeof(u_short)) {
				tu.ipft_vshort = *ta->ipft_pshort;
				*ta->ipft_pshort = (u_short)(in & 0xffff);
			} else if (ta->ipft_sz == sizeof(u_char)) {
				tu.ipft_vchar = *ta->ipft_pchar;
				*ta->ipft_pchar = (u_char)(in & 0xff);
			}
			error = fr_outobj(data, &tu, IPFOBJ_TUNEABLE);
		}
		break;

	default :
		error = EINVAL;
		break;
	}

	return error;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_initialise                                               */
/* Returns:     int - 0 == success,  < 0 == failure                         */
/* Parameters:  None.                                                       */
/*                                                                          */
/* Call of the initialise functions for all the various subsystems inside   */
/* of IPFilter.  If any of them should fail, return immeadiately a failure  */
/* BUT do not try to recover from the error here.                           */
/* ------------------------------------------------------------------------ */
int fr_initialise()
{
	int i;

	bzero(&frstats, sizeof(frstats));

#ifdef IPFILTER_LOG
	i = fr_loginit();
	if (i < 0)
		return -10 + i;
#endif
	i = fr_natinit();
	if (i < 0)
		return -20 + i;

	i = fr_stateinit();
	if (i < 0)
		return -30 + i;

	i = fr_authinit();
	if (i < 0)
		return -40 + i;

	i = fr_fraginit();
	if (i < 0)
		return -50 + i;

	i = appr_init();
	if (i < 0)
		return -60 + i;

#ifdef IPFILTER_SYNC
	i = ipfsync_init();
	if (i < 0)
		return -70 + i;
#endif
#ifdef IPFILTER_SCAN
	i = ipsc_init();
	if (i < 0)
		return -80 + i;
#endif
#ifdef IPFILTER_LOOKUP
	i = ip_lookup_init();
	if (i < 0)
		return -90 + i;
#endif
#ifdef IPFILTER_COMPILED
	ipfrule_add();
#endif
	return 0;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_deinitialise                                             */
/* Returns:     None.                                                       */
/* Parameters:  None.                                                       */
/*                                                                          */
/* Call all the various subsystem cleanup routines to deallocate memory or  */
/* destroy locks or whatever they've done that they need to now undo.       */
/* The order here IS important as there are some cross references of        */
/* internal data structures.                                                */
/* ------------------------------------------------------------------------ */
void fr_deinitialise()
{
	fr_fragunload();
	fr_authunload();
	fr_natunload();
	fr_stateunload();
#ifdef IPFILTER_SCAN
	fr_scanunload();
#endif
	appr_unload();

#ifdef IPFILTER_COMPILED
	ipfrule_remove();
#endif

	(void) frflush(IPL_LOGIPF, 0, FR_INQUE|FR_OUTQUE|FR_INACTIVE);
	(void) frflush(IPL_LOGIPF, 0, FR_INQUE|FR_OUTQUE);
	(void) frflush(IPL_LOGCOUNT, 0, FR_INQUE|FR_OUTQUE|FR_INACTIVE);
	(void) frflush(IPL_LOGCOUNT, 0, FR_INQUE|FR_OUTQUE);

#ifdef IPFILTER_LOOKUP
	ip_lookup_unload();
#endif

#ifdef IPFILTER_LOG
	fr_logunload();
#endif
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_zerostats                                                */
/* Returns:     int - 0 = success, else failure                             */
/* Parameters:  data(O) - pointer to pointer for copying data back to       */
/*                                                                          */
/* Copies the current statistics out to userspace and then zero's the       */
/* current ones in the kernel. The lock is only held across the bzero() as  */
/* the copyout may result in paging (ie network activity.)                  */
/* ------------------------------------------------------------------------ */
int	fr_zerostats(data)
void	*data;
{
	friostat_t fio;
	int error;

	fr_getstat(&fio);
	error = fr_outobj(data, &fio, IPFOBJ_IPFSTAT);
	if (error)
		return EFAULT;

	WRITE_ENTER(&ipf_mutex);
	bzero(&frstats, sizeof(frstats));
	RWLOCK_EXIT(&ipf_mutex);

	return 0;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_resolvedest                                              */
/* Returns:     Nil                                                         */
/* Parameters:  fdp(IO) - pointer to destination information to resolve     */
/*              v(I)    - IP protocol version to match                      */
/*                                                                          */
/* Looks up an interface name in the frdest structure pointed to by fdp and */
/* if a matching name can be found for the particular IP protocol version   */
/* then store the interface pointer in the frdest struct.  If no match is   */
/* found, then set the interface pointer to be -1 as NULL is considered to  */
/* indicate there is no information at all in the structure.                */
/* ------------------------------------------------------------------------ */
void fr_resolvedest(fdp, v)
frdest_t *fdp;
int v;
{
	void *ifp;

	ifp = NULL;
	v = v;		/* LINT */

	if (*fdp->fd_ifname != '\0') {
		ifp = GETIFP(fdp->fd_ifname, v);
		if (ifp == NULL)
			ifp = (void *)-1;
	}
	fdp->fd_ifp = ifp;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_resolvenic                                               */
/* Returns:     void* - NULL = wildcard name, -1 = failed to find NIC, else */
/*                      pointer to interface structure for NIC              */
/* Parameters:  name(I) - complete interface name                           */
/*              v(I)    - IP protocol version                               */
/*                                                                          */
/* Look for a network interface structure that firstly has a matching name  */
/* to that passed in and that is also being used for that IP protocol       */
/* version (necessary on some platforms where there are separate listings   */
/* for both IPv4 and IPv6 on the same physical NIC.                         */
/*                                                                          */
/* One might wonder why name gets terminated with a \0 byte in here.  The   */
/* reason is an interface name could get into the kernel structures of ipf  */
/* in any number of ways and so long as they all use the same sized array   */
/* to put the name in, it makes sense to ensure it gets null terminated     */
/* before it is used for its intended purpose - finding its match in the    */
/* kernel's list of configured interfaces.                                  */
/*                                                                          */
/* NOTE: This SHOULD ONLY be used with IPFilter structures that have an     */
/*       array for the name that is LIFNAMSIZ bytes (at least) in length.   */
/* ------------------------------------------------------------------------ */
void *fr_resolvenic(name, v)
char *name;
int v;
{
	void *nic;

	if (name[0] == '\0')
		return NULL;

	if ((name[1] == '\0') && ((name[0] == '-') || (name[0] == '*'))) {
		return NULL;
	}

	name[LIFNAMSIZ - 1] = '\0';

	nic = GETIFP(name, v);
	if (nic == NULL)
		nic = (void *)-1;
	return nic;
}


ipftoken_t *ipftokenhead = NULL, **ipftokentail = &ipftokenhead;


/* ------------------------------------------------------------------------ */
/* Function:    ipf_expiretokens                                            */
/* Returns:     None.                                                       */
/* Parameters:  None.                                                       */
/*                                                                          */
/* This function is run every ipf tick to see if there are any tokens that  */
/* have been held for too long and need to be freed up.                     */
/* ------------------------------------------------------------------------ */
void ipf_expiretokens()
{
	ipftoken_t *it;

	WRITE_ENTER(&ipf_tokens);
	while ((it = ipftokenhead) != NULL) {
		if (it->ipt_die > fr_ticks)
			break;

		ipf_freetoken(it);
	}
	RWLOCK_EXIT(&ipf_tokens);
}


/* ------------------------------------------------------------------------ */
/* Function:    ipf_deltoken                                                */
/* Returns:     int     - 0 = success, else error                           */
/* Parameters:  type(I) - the token type to match                           */
/*              uid(I)  - uid owning the token                              */
/*              ptr(I)  - context pointer for the token                     */
/*                                                                          */
/* This function looks for a a token in the current list that matches up    */
/* the fields (type, uid, ptr).  If none is found, ESRCH is returned, else  */
/* call ipf_freetoken() to remove it from the list.                         */
/* ------------------------------------------------------------------------ */
int ipf_deltoken(type, uid, ptr)
int type, uid;
void *ptr;
{
	ipftoken_t *it;
	int error = ESRCH;

	WRITE_ENTER(&ipf_tokens);
	for (it = ipftokenhead; it != NULL; it = it->ipt_next)
		if (ptr == it->ipt_ctx && type == it->ipt_type &&
		    uid == it->ipt_uid) {
			ipf_freetoken(it);
			error = 0;
			break;
	}
	RWLOCK_EXIT(&ipf_tokens);

	return error;
}


/* ------------------------------------------------------------------------ */
/* Function:    ipf_findtoken                                               */
/* Returns:     ipftoken_t * - NULL if no memory, else pointer to token     */
/* Parameters:  type(I) - the token type to match                           */
/*              uid(I)  - uid owning the token                              */
/*              ptr(I)  - context pointer for the token                     */
/*                                                                          */
/* This function looks for a live token in the list of current tokens that  */
/* matches the tuple (type, uid, ptr).  If one cannot be found then one is  */
/* allocated.  If one is found then it is moved to the top of the list of   */
/* currently active tokens.                                                 */
/*                                                                          */
/* NOTE: It is by design that this function returns holding a read lock on  */
/*       ipf_tokens.  Callers must make sure they release it!               */
/* ------------------------------------------------------------------------ */
ipftoken_t *ipf_findtoken(type, uid, ptr)
int type, uid;
void *ptr;
{
	ipftoken_t *it, *new;

	KMALLOC(new, ipftoken_t *);

	WRITE_ENTER(&ipf_tokens);
	for (it = ipftokenhead; it != NULL; it = it->ipt_next) {
		if (it->ipt_alive == 0)
			continue;
		if (ptr == it->ipt_ctx && type == it->ipt_type &&
		    uid == it->ipt_uid)
			break;
	}

	if (it == NULL) {
		it = new;
		new = NULL;
		if (it == NULL)
			return NULL;
		it->ipt_data = NULL;
		it->ipt_ctx = ptr;
		it->ipt_uid = uid;
		it->ipt_type = type;
		it->ipt_next = NULL;
		it->ipt_alive = 1;
	} else {
		if (new != NULL) {
			KFREE(new);
			new = NULL;
		}

		ipf_unlinktoken(it);
	}
	it->ipt_pnext = ipftokentail;
	*ipftokentail = it;
	ipftokentail = &it->ipt_next;
	it->ipt_next = NULL;

	it->ipt_die = fr_ticks + 2;

	MUTEX_DOWNGRADE(&ipf_tokens);

	return it;
}


/* ------------------------------------------------------------------------ */
/* Function:    ipf_unlinktoken                                             */
/* Returns:     None.                                                       */
/* Parameters:  token(I) - pointer to token structure                       */
/*                                                                          */
/* This function unlinks a token structure from the linked list of tokens   */
/* that "own" it.  The head pointer never needs to be explicitly adjusted   */
/* but the tail does due to the linked list implementation.                 */
/* ------------------------------------------------------------------------ */
static void ipf_unlinktoken(token)
ipftoken_t *token;
{

	if (ipftokentail == &token->ipt_next)
		ipftokentail = token->ipt_pnext;

	*token->ipt_pnext = token->ipt_next;
	if (token->ipt_next != NULL)
		token->ipt_next->ipt_pnext = token->ipt_pnext;
}


/* ------------------------------------------------------------------------ */
/* Function:    ipf_freetoken                                               */
/* Returns:     None.                                                       */
/* Parameters:  token(I) - pointer to token structure                       */
/*                                                                          */
/* This function unlinks a token from the linked list and on the path to    */
/* free'ing the data, it calls the dereference function that is associated  */
/* with the type of data pointed to by the token as it is considered to     */
/* hold a reference to it.                                                  */
/* ------------------------------------------------------------------------ */
void ipf_freetoken(token)
ipftoken_t *token;
{
	void *data, **datap;

	ipf_unlinktoken(token);

	data = token->ipt_data;
	datap = &data;

	if ((data != NULL) && (data != (void *)-1)) {
		switch (token->ipt_type)
		{
		case IPFGENITER_IPF :
			(void) fr_derefrule((frentry_t **)datap);
			break;
		case IPFGENITER_IPNAT :
			WRITE_ENTER(&ipf_nat);
			fr_ipnatderef((ipnat_t **)datap);
			RWLOCK_EXIT(&ipf_nat);
			break;
		case IPFGENITER_NAT :
			fr_natderef((nat_t **)datap);
			break;
		case IPFGENITER_STATE :
			fr_statederef((ipstate_t **)datap);
			break;
		case IPFGENITER_FRAG :
#ifdef USE_MUTEXES
			fr_fragderef((ipfr_t **)datap, &ipf_frag);
#else
			fr_fragderef((ipfr_t **)datap);
#endif
			break;
		case IPFGENITER_NATFRAG :
#ifdef USE_MUTEXES
			fr_fragderef((ipfr_t **)datap, &ipf_natfrag);
#else
			fr_fragderef((ipfr_t **)datap);
#endif
			break;
		case IPFGENITER_HOSTMAP :
			WRITE_ENTER(&ipf_nat);
			fr_hostmapdel((hostmap_t **)datap);
			RWLOCK_EXIT(&ipf_nat);
			break;
		default :
#ifdef IPFILTER_LOOKUP
			ip_lookup_iterderef(token->ipt_type, data);
#endif
			break;
		}
	}

	KFREE(token);
}


/* ------------------------------------------------------------------------ */
/* Function:    ipf_getnextrule                                             */
/* Returns:     int - 0 = success, else error                               */
/* Parameters:  t(I)   - pointer to destination information to resolve      */
/*              ptr(I) - pointer to ipfobj_t to copyin from user space      */
/*                                                                          */
/* This function's first job is to bring in the ipfruleiter_t structure via */
/* the ipfobj_t structure to determine what should be the next rule to      */
/* return. Once the ipfruleiter_t has been brought in, it then tries to     */
/* find the 'next rule'.  This may include searching rule group lists or    */
/* just be as simple as looking at the 'next' field in the rule structure.  */
/* When we have found the rule to return, increase its reference count and  */
/* if we used an existing rule to get here, decrease its reference count.   */
/* ------------------------------------------------------------------------ */
int ipf_getnextrule(ipftoken_t *t, void *ptr)
{
	frentry_t *fr, *next, zero;
	int error, count, out;
	ipfruleiter_t it;
	frgroup_t *fg;
	char *dst;

	if (t == NULL || ptr == NULL)
		return EFAULT;
	error = fr_inobj(ptr, &it, IPFOBJ_IPFITER);
	if (error != 0)
		return error;
	if ((it.iri_inout < 0) || (it.iri_inout > 3))
		return EINVAL;
	if ((it.iri_active != 0) && (it.iri_active != 1))
		return EINVAL;
	if (it.iri_nrules == 0)
		return ENOSPC;
	if (it.iri_rule == NULL)
		return EFAULT;

	out = it.iri_inout & F_OUT;
	fr = t->ipt_data;
	READ_ENTER(&ipf_mutex);
	if (fr == NULL) {
		if (*it.iri_group == '\0') {
			if ((it.iri_inout & F_ACIN) != 0) {
				if (it.iri_v == 4)
					next = ipacct[out][it.iri_active];
				else
					next = ipacct6[out][it.iri_active];
			} else {
				if (it.iri_v == 4)
					next = ipfilter[out][it.iri_active];
				else
					next = ipfilter6[out][it.iri_active];
			}
		} else {
			fg = fr_findgroup(it.iri_group, IPL_LOGIPF,
					  it.iri_active, NULL);
			if (fg != NULL)
				next = fg->fg_start;
			else
				next = NULL;
		}
	} else {
		next = fr->fr_next;
	}

	dst = (char *)it.iri_rule;
	count = it.iri_nrules;
	/*
	 * The ipfruleiter may ask for more than 1 rule at a time to be
	 * copied out, so long as that many exist in the list to start with!
	 */
	for (;;) {
		if (next != NULL) {
			if (count == 1) {
				MUTEX_ENTER(&next->fr_lock);
				next->fr_ref++;
				MUTEX_EXIT(&next->fr_lock);
				t->ipt_data = next;
			}
		} else {
			bzero(&zero, sizeof(zero));
			next = &zero;
			count = 1;
			t->ipt_data = NULL;
		}
		RWLOCK_EXIT(&ipf_mutex);

		error = COPYOUT(next, dst, sizeof(*next));
		if (error != 0)
			return EFAULT;

		if (next->fr_data != NULL) {
			dst += sizeof(*next);
			error = COPYOUT(next->fr_data, dst, next->fr_dsize);
			if (error != 0)
				error = EFAULT;
			else
				dst += next->fr_dsize;
		}

		if ((count == 1) || (error != 0))
			break;

		count--;

		READ_ENTER(&ipf_mutex);
		next = next->fr_next;
	}

	if (fr != NULL) {
		(void) fr_derefrule(&fr);
	}

	return error;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_frruleiter                                               */
/* Returns:     int - 0 = success, else error                               */
/* Parameters:  data(I) - the token type to match                           */
/*              uid(I)  - uid owning the token                              */
/*              ptr(I)  - context pointer for the token                     */
/*                                                                          */
/* This function serves as a stepping stone between fr_ipf_ioctl and        */
/* ipf_getnextrule.  It's role is to find the right token in the kernel for */
/* the process doing the ioctl and use that to ask for the next rule.       */
/* ------------------------------------------------------------------------ */
static int ipf_frruleiter(data, uid, ctx)
void *data, *ctx;
int uid;
{
	ipftoken_t *token;
	int error;

	token = ipf_findtoken(IPFGENITER_IPF, uid, ctx);
	if (token != NULL)
		error = ipf_getnextrule(token, data);
	else
		error = EFAULT;
	RWLOCK_EXIT(&ipf_tokens);

	return error;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_geniter                                                  */
/* Returns:     int - 0 = success, else error                               */
/* Parameters:  token(I) - pointer to ipftoken_t structure                  */
/*              itp(I)   -                                                  */
/*                                                                          */
/* ------------------------------------------------------------------------ */
static int ipf_geniter(token, itp)
ipftoken_t *token;
ipfgeniter_t *itp;
{
	int error;

	switch (itp->igi_type)
	{
	case IPFGENITER_FRAG :
#ifdef USE_MUTEXES
		error = fr_nextfrag(token, itp,
				    &ipfr_list, &ipfr_tail, &ipf_frag);
#else
		error = fr_nextfrag(token, itp, &ipfr_list, &ipfr_tail);
#endif
		break;
	default :
		error = EINVAL;
		break;
	}

	return error;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_genericiter                                              */
/* Returns:     int - 0 = success, else error                               */
/* Parameters:  data(I) - the token type to match                           */
/*              uid(I)  - uid owning the token                              */
/*              ptr(I)  - context pointer for the token                     */
/*                                                                          */
/* ------------------------------------------------------------------------ */
int ipf_genericiter(data, uid, ctx)
void *data, *ctx;
int uid;
{
	ipftoken_t *token;
	ipfgeniter_t iter;
	int error;

	error = fr_inobj(data, &iter, IPFOBJ_GENITER);
	if (error != 0)
		return error;

	token = ipf_findtoken(iter.igi_type, uid, ctx);
	if (token != NULL) {
		token->ipt_subtype = iter.igi_type;
		error = ipf_geniter(token, &iter);
	} else
		error = EFAULT;
	RWLOCK_EXIT(&ipf_tokens);

	return error;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_ipf_ioctl                                                */
/* Returns:     int - 0 = success, else error                               */
/* Parameters:  data(I) - the token type to match                           */
/*              cmd(I)  - the ioctl command number                          */
/*              mode(I) - mode flags for the ioctl                          */
/*              uid(I)  - uid owning the token                              */
/*              ptr(I)  - context pointer for the token                     */
/*                                                                          */
/* This function handles all of the ioctl command that are actually isssued */
/* to the /dev/ipl device.                                                  */
/* ------------------------------------------------------------------------ */
int fr_ipf_ioctl(data, cmd, mode, uid, ctx)
caddr_t data;
ioctlcmd_t cmd;
int mode, uid;
void *ctx;
{
	friostat_t fio;
	int error, tmp;
	SPL_INT(s);

	switch (cmd)
	{
	case SIOCFRENB :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			error = BCOPYIN(data, &tmp, sizeof(tmp));
			if (error != 0) {
				error = EFAULT;
				break;
			}

			WRITE_ENTER(&ipf_global);
			if (tmp) {
				if (fr_running > 0)
					error = 0;
				else
					error = ipfattach();
				if (error == 0)
					fr_running = 1;
				else
					(void) ipfdetach();
			} else {
				error = ipfdetach();
				if (error == 0)
					fr_running = -1;
			}
			RWLOCK_EXIT(&ipf_global);
		}
		break;

	case SIOCIPFSET :
		if (!(mode & FWRITE)) {
			error = EPERM;
			break;
		}
		/* FALLTHRU */
	case SIOCIPFGETNEXT :
	case SIOCIPFGET :
		error = fr_ipftune(cmd, (void *)data);
		break;

	case SIOCSETFF :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			error = BCOPYIN(data, &fr_flags, sizeof(fr_flags));
			if (error != 0)
				error = EFAULT;
		}
		break;

	case SIOCGETFF :
		error = BCOPYOUT(&fr_flags, data, sizeof(fr_flags));
		if (error != 0)
			error = EFAULT;
		break;

	case SIOCFUNCL :
		error = fr_resolvefunc((void *)data);
		break;

	case SIOCINAFR :
	case SIOCRMAFR :
	case SIOCADAFR :
	case SIOCZRLST :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = frrequest(IPL_LOGIPF, cmd, data, fr_active, 1);
		break;

	case SIOCINIFR :
	case SIOCRMIFR :
	case SIOCADIFR :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = frrequest(IPL_LOGIPF, cmd, data,
					  1 - fr_active, 1);
		break;

	case SIOCSWAPA :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			WRITE_ENTER(&ipf_mutex);
			bzero((char *)frcache, sizeof(frcache[0]) * 2);
			error = BCOPYOUT(&fr_active, data, sizeof(fr_active));
			if (error != 0)
				error = EFAULT;
			else
				fr_active = 1 - fr_active;
			RWLOCK_EXIT(&ipf_mutex);
		}
		break;

	case SIOCGETFS :
		fr_getstat(&fio);
		error = fr_outobj((void *)data, &fio, IPFOBJ_IPFSTAT);
		break;

	case SIOCFRZST :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = fr_zerostats(data);
		break;

	case SIOCIPFFL :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			error = BCOPYIN(data, &tmp, sizeof(tmp));
			if (!error) {
				tmp = frflush(IPL_LOGIPF, 4, tmp);
				error = BCOPYOUT(&tmp, data, sizeof(tmp));
				if (error != 0)
					error = EFAULT;
			} else
				error = EFAULT;
		}
		break;

#ifdef USE_INET6
	case SIOCIPFL6 :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			error = BCOPYIN(data, &tmp, sizeof(tmp));
			if (!error) {
				tmp = frflush(IPL_LOGIPF, 6, tmp);
				error = BCOPYOUT(&tmp, data, sizeof(tmp));
				if (error != 0)
					error = EFAULT;
			} else
				error = EFAULT;
		}
		break;
#endif

	case SIOCSTLCK :
		error = BCOPYIN(data, &tmp, sizeof(tmp));
		if (error == 0) {
			fr_state_lock = tmp;
			fr_nat_lock = tmp;
			fr_frag_lock = tmp;
			fr_auth_lock = tmp;
		} else
			error = EFAULT;
		break;

#ifdef	IPFILTER_LOG
	case SIOCIPFFB :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			tmp = ipflog_clear(IPL_LOGIPF);
			error = BCOPYOUT(&tmp, data, sizeof(tmp));
			if (error)
				error = EFAULT;
		}
		break;
#endif /* IPFILTER_LOG */

	case SIOCFRSYN :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			WRITE_ENTER(&ipf_global);
#ifdef MENTAT
			error = ipfsync();
#else
			frsync(NULL);
			error = 0;
#endif
			RWLOCK_EXIT(&ipf_global);

		}
		break;

	case SIOCGFRST :
		error = fr_outobj((void *)data, fr_fragstats(),
				  IPFOBJ_FRAGSTAT);
		break;

#ifdef	IPFILTER_LOG
	case FIONREAD :
		tmp = (int)iplused[IPL_LOGIPF];

		error = BCOPYOUT(&tmp, data, sizeof(tmp));
		break;
#endif

	case SIOCIPFITER :
		SPL_SCHED(s);
		error = ipf_frruleiter(data, uid, ctx);
		SPL_X(s);
		break;

	case SIOCGENITER :
		SPL_SCHED(s);
		error = ipf_genericiter(data, uid, ctx);
		SPL_X(s);
		break;

	case SIOCIPFDELTOK :
		SPL_SCHED(s);
		error = BCOPYIN(data, &tmp, sizeof(tmp));
		if (error == 0)
			error = ipf_deltoken(tmp, uid, ctx);
		SPL_X(s);
		break;

	default :
		error = EINVAL;
		break;
	}

	return error;
}


/* ------------------------------------------------------------------------ */
/* Function:    ipf_queueflush                                              */
/* Returns:     int - number of entries flushed (0 = none)                  */
/* Parameters:  deletefn(I) - function to call to delete entry              */
/*              ipfqs(I)    - top of the list of ipf internal queues        */
/*              userqs(I)   - top of the list of user defined timeouts      */
/*                                                                          */
/* This fucntion gets called when the state/NAT hash tables fill up and we  */
/* need to try a bit harder to free up some space.  The algorithm used is   */
/* to look for the oldest entries on each timeout queue and free them if    */
/* they are within the given window we are considering.  Where the window   */
/* starts and the steps taken to increase its size depend upon how long ipf */
/* has been running (fr_ticks.)  Anything modified in the last 30 seconds   */
/* is not touched.                                                          */
/*                                              touched                     */
/*         die     fr_ticks   30*1.5    1800*1.5   |  43200*1.5             */
/*           |          |        |           |     |     |                  */
/* future <--+----------+--------+-----------+-----+-----+-----------> past */
/*                     now        \_int=30s_/ \_int=1hr_/ \_int=12hr        */
/*                                                                          */
/* Points to note:                                                          */
/* - tqe_die is the time, in the future, when entries die.                  */
/* - tqe_die - fr_ticks is how long left the connection has to live in ipf  */
/*   ticks.                                                                 */
/* - tqe_touched is when the entry was last used by NAT/state               */
/* - the closer tqe_touched is to fr_ticks, the further tqe_die will be for */
/*   any given timeout queue and vice versa.                                */
/* - both tqe_die and tqe_touched increase over time                        */
/* - timeout queues are sorted with the highest value of tqe_die at the     */
/*   bottom and therefore the smallest values of each are at the top        */
/*                                                                          */
/* We start by setting up a maximum range to scan for things to move of     */
/* iend (newest) to istart (oldest) in chunks of "interval".  If nothing is */
/* found in that range, "interval" is adjusted (so long as it isn't 30) and */
/* we start again with a new value for "iend" and "istart".  The downside   */
/* of the current implementation is that it may return removing just 1 entry*/
/* every time (pathological case) where it could remove more.               */
/* ------------------------------------------------------------------------ */
int ipf_queueflush(deletefn, ipfqs, userqs)
ipftq_delete_fn_t deletefn;
ipftq_t *ipfqs, *userqs;
{
	u_long interval, istart, iend;
	ipftq_t *ifq, *ifqnext;
	ipftqent_t *tqe, *tqn;
	int removed;

	/*
	 * NOTE: Use of "* 15 / 10" is required here because if "* 1.5" is
	 *       used then the operations are upgraded to floating point
	 *       and kernels don't like floating point...
	 */
	if (fr_ticks > IPF_TTLVAL(43200 * 15 / 10)) {
		istart = IPF_TTLVAL(86400 * 4);
		interval = IPF_TTLVAL(43200);
	} else if (fr_ticks > IPF_TTLVAL(1800 * 15 / 10)) {
		istart = IPF_TTLVAL(43200);
		interval = IPF_TTLVAL(1800);
	} else if (fr_ticks > IPF_TTLVAL(30 * 15 / 10)) {
		istart = IPF_TTLVAL(1800);
		interval = IPF_TTLVAL(30);
	} else {
		return 0;
	}
	if (istart > fr_ticks) {
		if (fr_ticks - interval < interval)
			istart = interval;
		else
			istart = (fr_ticks / interval) * interval;
	}

	iend = fr_ticks - interval;
	removed = 0;

	for (;;) {
		u_long try;

		try = fr_ticks - istart; 

		for (ifq = ipfqs; ifq != NULL; ifq = ifq->ifq_next) {
			for (tqn = ifq->ifq_head; ((tqe = tqn) != NULL); ) {
				if (try < tqe->tqe_touched)
					break;
				tqn = tqe->tqe_next;
				if ((*deletefn)(tqe->tqe_parent) == 0)
					removed++;
			}
		}

		for (ifq = userqs; ifq != NULL; ifq = ifqnext) {
			ifqnext = ifq->ifq_next;

			for (tqn = ifq->ifq_head; ((tqe = tqn) != NULL); ) {
				if (try < tqe->tqe_touched)
					break;
				tqn = tqe->tqe_next;
				if ((*deletefn)(tqe->tqe_parent) == 0)
					removed++;
			}
		}

		if (try >= iend) {
			if (removed > 0)
				break;
			if (interval == IPF_TTLVAL(43200)) {
				interval = IPF_TTLVAL(1800);
			} else if (interval == IPF_TTLVAL(1800)) {
				interval = IPF_TTLVAL(30);
			} else {
				break;
			}
			if (interval >= fr_ticks)
				break;

			iend = fr_ticks - interval;
		}
		istart -= interval;
	}

	return removed;
}
