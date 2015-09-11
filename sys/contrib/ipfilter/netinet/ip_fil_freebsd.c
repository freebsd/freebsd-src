/*	$FreeBSD$	*/

/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#if !defined(lint)
static const char sccsid[] = "@(#)ip_fil.c	2.41 6/5/96 (C) 1993-2000 Darren Reed";
static const char rcsid[] = "@(#)$Id$";
#endif

#if defined(KERNEL) || defined(_KERNEL)
# undef KERNEL
# undef _KERNEL
# define	KERNEL	1
# define	_KERNEL	1
#endif
#if defined(__FreeBSD_version) && (__FreeBSD_version >= 400000) && \
    !defined(KLD_MODULE) && !defined(IPFILTER_LKM)
# include "opt_inet6.h"
#endif
#if defined(__FreeBSD_version) && (__FreeBSD_version >= 440000) && \
    !defined(KLD_MODULE) && !defined(IPFILTER_LKM)
# include "opt_random_ip_id.h"
#endif
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/file.h>
# include <sys/fcntl.h>
# include <sys/filio.h>
#include <sys/time.h>
#include <sys/systm.h>
# include <sys/dirent.h>
#if defined(__FreeBSD_version) && (__FreeBSD_version >= 800000)
#include <sys/jail.h>
#endif
# include <sys/mbuf.h>
# include <sys/sockopt.h>
#if !defined(__hpux)
# include <sys/mbuf.h>
#endif
#include <sys/socket.h>
# include <sys/selinfo.h>
# include <netinet/tcp_var.h>

#include <net/if.h>
# include <net/if_var.h>
#  include <net/netisr.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#if defined(__FreeBSD_version) && (__FreeBSD_version >= 800000)
#include <net/vnet.h>
#else
#define CURVNET_SET(arg)
#define CURVNET_RESTORE()
#endif
#if defined(__osf__)
# include <netinet/tcp_timer.h>
#endif
#include <netinet/udp.h>
#include <netinet/tcpip.h>
#include <netinet/ip_icmp.h>
#include "netinet/ip_compat.h"
#ifdef USE_INET6
# include <netinet/icmp6.h>
#endif
#include "netinet/ip_fil.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_frag.h"
#include "netinet/ip_state.h"
#include "netinet/ip_proxy.h"
#include "netinet/ip_auth.h"
#include "netinet/ip_sync.h"
#include "netinet/ip_lookup.h"
#include "netinet/ip_dstlist.h"
#ifdef	IPFILTER_SCAN
#include "netinet/ip_scan.h"
#endif
#include "netinet/ip_pool.h"
# include <sys/malloc.h>
#include <sys/kernel.h>
#ifdef CSUM_DATA_VALID
#include <machine/in_cksum.h>
#endif
extern	int	ip_optcopy __P((struct ip *, struct ip *));


# ifdef IPFILTER_M_IPFILTER
MALLOC_DEFINE(M_IPFILTER, "ipfilter", "IP Filter packet filter data structures");
# endif


static	int	(*ipf_savep) __P((void *, ip_t *, int, void *, int, struct mbuf **));
static	int	ipf_send_ip __P((fr_info_t *, mb_t *));
static void	ipf_timer_func __P((void *arg));
int		ipf_locks_done = 0;

ipf_main_softc_t ipfmain;

# include <sys/conf.h>
# if defined(NETBSD_PF)
#  include <net/pfil.h>
# endif /* NETBSD_PF */
/*
 * We provide the ipf_checkp name just to minimize changes later.
 */
int (*ipf_checkp) __P((void *, ip_t *ip, int hlen, void *ifp, int out, mb_t **mp));


static eventhandler_tag ipf_arrivetag, ipf_departtag, ipf_clonetag;

static void ipf_ifevent(void *arg);

static void ipf_ifevent(arg)
	void *arg;
{
        ipf_sync(arg, NULL);
}



static int
ipf_check_wrapper(void *arg, struct mbuf **mp, struct ifnet *ifp, int dir)
{
	struct ip *ip = mtod(*mp, struct ip *);
	int rv;

	/*
	 * IPFilter expects evreything in network byte order
	 */
#if (__FreeBSD_version < 1000019)
	ip->ip_len = htons(ip->ip_len);
	ip->ip_off = htons(ip->ip_off);
#endif
	rv = ipf_check(&ipfmain, ip, ip->ip_hl << 2, ifp, (dir == PFIL_OUT),
		       mp);
#if (__FreeBSD_version < 1000019)
	if ((rv == 0) && (*mp != NULL)) {
		ip = mtod(*mp, struct ip *);
		ip->ip_len = ntohs(ip->ip_len);
		ip->ip_off = ntohs(ip->ip_off);
	}
#endif
	return rv;
}

# ifdef USE_INET6
#  include <netinet/ip6.h>

static int
ipf_check_wrapper6(void *arg, struct mbuf **mp, struct ifnet *ifp, int dir)
{
	return (ipf_check(&ipfmain, mtod(*mp, struct ip *),
			  sizeof(struct ip6_hdr), ifp, (dir == PFIL_OUT), mp));
}
# endif
#if	defined(IPFILTER_LKM)
int ipf_identify(s)
	char *s;
{
	if (strcmp(s, "ipl") == 0)
		return 1;
	return 0;
}
#endif /* IPFILTER_LKM */


static void
ipf_timer_func(arg)
	void *arg;
{
	ipf_main_softc_t *softc = arg;
	SPL_INT(s);

	SPL_NET(s);
	READ_ENTER(&softc->ipf_global);

        if (softc->ipf_running > 0)
		ipf_slowtimer(softc);

	if (softc->ipf_running == -1 || softc->ipf_running == 1) {
#if 0
		softc->ipf_slow_ch = timeout(ipf_timer_func, softc, hz/2);
#endif
		callout_init(&softc->ipf_slow_ch, 1);
		callout_reset(&softc->ipf_slow_ch,
			(hz / IPF_HZ_DIVIDE) * IPF_HZ_MULT,
			ipf_timer_func, softc);
	}
	RWLOCK_EXIT(&softc->ipf_global);
	SPL_X(s);
}


int
ipfattach(softc)
	ipf_main_softc_t *softc;
{
#ifdef USE_SPL
	int s;
#endif

	SPL_NET(s);
	if (softc->ipf_running > 0) {
		SPL_X(s);
		return EBUSY;
	}

	if (ipf_init_all(softc) < 0) {
		SPL_X(s);
		return EIO;
	}


	if (ipf_checkp != ipf_check) {
		ipf_savep = ipf_checkp;
		ipf_checkp = ipf_check;
	}

	bzero((char *)ipfmain.ipf_selwait, sizeof(ipfmain.ipf_selwait));
	softc->ipf_running = 1;

	if (softc->ipf_control_forwarding & 1)
		V_ipforwarding = 1;

	SPL_X(s);
#if 0
	softc->ipf_slow_ch = timeout(ipf_timer_func, softc,
				     (hz / IPF_HZ_DIVIDE) * IPF_HZ_MULT);
#endif
	callout_init(&softc->ipf_slow_ch, 1);
	callout_reset(&softc->ipf_slow_ch, (hz / IPF_HZ_DIVIDE) * IPF_HZ_MULT,
		ipf_timer_func, softc);
	return 0;
}


/*
 * Disable the filter by removing the hooks from the IP input/output
 * stream.
 */
int
ipfdetach(softc)
	ipf_main_softc_t *softc;
{
#ifdef USE_SPL
	int s;
#endif

	if (softc->ipf_control_forwarding & 2)
		V_ipforwarding = 0;

	SPL_NET(s);

#if 0
	if (softc->ipf_slow_ch.callout != NULL)
		untimeout(ipf_timer_func, softc, softc->ipf_slow_ch);
	bzero(&softc->ipf_slow, sizeof(softc->ipf_slow));
#endif
	callout_drain(&softc->ipf_slow_ch);

#ifndef NETBSD_PF
	if (ipf_checkp != NULL)
		ipf_checkp = ipf_savep;
	ipf_savep = NULL;
#endif

	ipf_fini_all(softc);

	softc->ipf_running = -2;

	SPL_X(s);

	return 0;
}


/*
 * Filter ioctl interface.
 */
int
ipfioctl(dev, cmd, data, mode
, p)
	struct thread *p;
#    define	p_cred	td_ucred
#    define	p_uid	td_ucred->cr_ruid
	struct cdev *dev;
	ioctlcmd_t cmd;
	caddr_t data;
	int mode;
{
	int error = 0, unit = 0;
	SPL_INT(s);

#if (BSD >= 199306)
        if (securelevel_ge(p->p_cred, 3) && (mode & FWRITE))
	{
		ipfmain.ipf_interror = 130001;
		return EPERM;
	}
#endif

	unit = GET_MINOR(dev);
	if ((IPL_LOGMAX < unit) || (unit < 0)) {
		ipfmain.ipf_interror = 130002;
		return ENXIO;
	}

	if (ipfmain.ipf_running <= 0) {
		if (unit != IPL_LOGIPF && cmd != SIOCIPFINTERROR) {
			ipfmain.ipf_interror = 130003;
			return EIO;
		}
		if (cmd != SIOCIPFGETNEXT && cmd != SIOCIPFGET &&
		    cmd != SIOCIPFSET && cmd != SIOCFRENB &&
		    cmd != SIOCGETFS && cmd != SIOCGETFF &&
		    cmd != SIOCIPFINTERROR) {
			ipfmain.ipf_interror = 130004;
			return EIO;
		}
	}

	SPL_NET(s);

	CURVNET_SET(TD_TO_VNET(p));
	error = ipf_ioctlswitch(&ipfmain, unit, data, cmd, mode, p->p_uid, p);
	CURVNET_RESTORE();
	if (error != -1) {
		SPL_X(s);
		return error;
	}

	SPL_X(s);

	return error;
}


/*
 * ipf_send_reset - this could conceivably be a call to tcp_respond(), but that
 * requires a large amount of setting up and isn't any more efficient.
 */
int
ipf_send_reset(fin)
	fr_info_t *fin;
{
	struct tcphdr *tcp, *tcp2;
	int tlen = 0, hlen;
	struct mbuf *m;
#ifdef USE_INET6
	ip6_t *ip6;
#endif
	ip_t *ip;

	tcp = fin->fin_dp;
	if (tcp->th_flags & TH_RST)
		return -1;		/* feedback loop */

	if (ipf_checkl4sum(fin) == -1)
		return -1;

	tlen = fin->fin_dlen - (TCP_OFF(tcp) << 2) +
			((tcp->th_flags & TH_SYN) ? 1 : 0) +
			((tcp->th_flags & TH_FIN) ? 1 : 0);

#ifdef USE_INET6
	hlen = (fin->fin_v == 6) ? sizeof(ip6_t) : sizeof(ip_t);
#else
	hlen = sizeof(ip_t);
#endif
#ifdef MGETHDR
	MGETHDR(m, M_NOWAIT, MT_HEADER);
#else
	MGET(m, M_NOWAIT, MT_HEADER);
#endif
	if (m == NULL)
		return -1;
	if (sizeof(*tcp2) + hlen > MLEN) {
		if (!(MCLGET(m, M_NOWAIT))) {
			FREE_MB_T(m);
			return -1;
		}
	}

	m->m_len = sizeof(*tcp2) + hlen;
#if (BSD >= 199103)
	m->m_data += max_linkhdr;
	m->m_pkthdr.len = m->m_len;
	m->m_pkthdr.rcvif = (struct ifnet *)0;
#endif
	ip = mtod(m, struct ip *);
	bzero((char *)ip, hlen);
#ifdef USE_INET6
	ip6 = (ip6_t *)ip;
#endif
	tcp2 = (struct tcphdr *)((char *)ip + hlen);
	tcp2->th_sport = tcp->th_dport;
	tcp2->th_dport = tcp->th_sport;

	if (tcp->th_flags & TH_ACK) {
		tcp2->th_seq = tcp->th_ack;
		tcp2->th_flags = TH_RST;
		tcp2->th_ack = 0;
	} else {
		tcp2->th_seq = 0;
		tcp2->th_ack = ntohl(tcp->th_seq);
		tcp2->th_ack += tlen;
		tcp2->th_ack = htonl(tcp2->th_ack);
		tcp2->th_flags = TH_RST|TH_ACK;
	}
	TCP_X2_A(tcp2, 0);
	TCP_OFF_A(tcp2, sizeof(*tcp2) >> 2);
	tcp2->th_win = tcp->th_win;
	tcp2->th_sum = 0;
	tcp2->th_urp = 0;

#ifdef USE_INET6
	if (fin->fin_v == 6) {
		ip6->ip6_flow = ((ip6_t *)fin->fin_ip)->ip6_flow;
		ip6->ip6_plen = htons(sizeof(struct tcphdr));
		ip6->ip6_nxt = IPPROTO_TCP;
		ip6->ip6_hlim = 0;
		ip6->ip6_src = fin->fin_dst6.in6;
		ip6->ip6_dst = fin->fin_src6.in6;
		tcp2->th_sum = in6_cksum(m, IPPROTO_TCP,
					 sizeof(*ip6), sizeof(*tcp2));
		return ipf_send_ip(fin, m);
	}
#endif
	ip->ip_p = IPPROTO_TCP;
	ip->ip_len = htons(sizeof(struct tcphdr));
	ip->ip_src.s_addr = fin->fin_daddr;
	ip->ip_dst.s_addr = fin->fin_saddr;
	tcp2->th_sum = in_cksum(m, hlen + sizeof(*tcp2));
	ip->ip_len = htons(hlen + sizeof(*tcp2));
	return ipf_send_ip(fin, m);
}


/*
 * ip_len must be in network byte order when called.
 */
static int
ipf_send_ip(fin, m)
	fr_info_t *fin;
	mb_t *m;
{
	fr_info_t fnew;
	ip_t *ip, *oip;
	int hlen;

	ip = mtod(m, ip_t *);
	bzero((char *)&fnew, sizeof(fnew));
	fnew.fin_main_soft = fin->fin_main_soft;

	IP_V_A(ip, fin->fin_v);
	switch (fin->fin_v)
	{
	case 4 :
		oip = fin->fin_ip;
		hlen = sizeof(*oip);
		fnew.fin_v = 4;
		fnew.fin_p = ip->ip_p;
		fnew.fin_plen = ntohs(ip->ip_len);
		IP_HL_A(ip, sizeof(*oip) >> 2);
		ip->ip_tos = oip->ip_tos;
		ip->ip_id = fin->fin_ip->ip_id;
#if defined(FreeBSD) && (__FreeBSD_version > 460000)
		ip->ip_off = htons(path_mtu_discovery ? IP_DF : 0);
#else
		ip->ip_off = 0;
#endif
		ip->ip_ttl = V_ip_defttl;
		ip->ip_sum = 0;
		break;
#ifdef USE_INET6
	case 6 :
	{
		ip6_t *ip6 = (ip6_t *)ip;

		ip6->ip6_vfc = 0x60;
		ip6->ip6_hlim = IPDEFTTL;

		hlen = sizeof(*ip6);
		fnew.fin_p = ip6->ip6_nxt;
		fnew.fin_v = 6;
		fnew.fin_plen = ntohs(ip6->ip6_plen) + hlen;
		break;
	}
#endif
	default :
		return EINVAL;
	}
#ifdef IPSEC
	m->m_pkthdr.rcvif = NULL;
#endif

	fnew.fin_ifp = fin->fin_ifp;
	fnew.fin_flx = FI_NOCKSUM;
	fnew.fin_m = m;
	fnew.fin_ip = ip;
	fnew.fin_mp = &m;
	fnew.fin_hlen = hlen;
	fnew.fin_dp = (char *)ip + hlen;
	(void) ipf_makefrip(hlen, ip, &fnew);

	return ipf_fastroute(m, &m, &fnew, NULL);
}


int
ipf_send_icmp_err(type, fin, dst)
	int type;
	fr_info_t *fin;
	int dst;
{
	int err, hlen, xtra, iclen, ohlen, avail, code;
	struct in_addr dst4;
	struct icmp *icmp;
	struct mbuf *m;
	i6addr_t dst6;
	void *ifp;
#ifdef USE_INET6
	ip6_t *ip6;
#endif
	ip_t *ip, *ip2;

	if ((type < 0) || (type >= ICMP_MAXTYPE))
		return -1;

	code = fin->fin_icode;
#ifdef USE_INET6
#if 0
	/* XXX Fix an off by one error: s/>/>=/
	 was:
	 if ((code < 0) || (code > sizeof(icmptoicmp6unreach)/sizeof(int)))
	 Fix obtained from NetBSD ip_fil_netbsd.c r1.4: */
#endif
	if ((code < 0) || (code >= sizeof(icmptoicmp6unreach)/sizeof(int)))
		return -1;
#endif

	if (ipf_checkl4sum(fin) == -1)
		return -1;
#ifdef MGETHDR
	MGETHDR(m, M_NOWAIT, MT_HEADER);
#else
	MGET(m, M_NOWAIT, MT_HEADER);
#endif
	if (m == NULL)
		return -1;
	avail = MHLEN;

	xtra = 0;
	hlen = 0;
	ohlen = 0;
	dst4.s_addr = 0;
	ifp = fin->fin_ifp;
	if (fin->fin_v == 4) {
		if ((fin->fin_p == IPPROTO_ICMP) && !(fin->fin_flx & FI_SHORT))
			switch (ntohs(fin->fin_data[0]) >> 8)
			{
			case ICMP_ECHO :
			case ICMP_TSTAMP :
			case ICMP_IREQ :
			case ICMP_MASKREQ :
				break;
			default :
				FREE_MB_T(m);
				return 0;
			}

		if (dst == 0) {
			if (ipf_ifpaddr(&ipfmain, 4, FRI_NORMAL, ifp,
					&dst6, NULL) == -1) {
				FREE_MB_T(m);
				return -1;
			}
			dst4 = dst6.in4;
		} else
			dst4.s_addr = fin->fin_daddr;

		hlen = sizeof(ip_t);
		ohlen = fin->fin_hlen;
		iclen = hlen + offsetof(struct icmp, icmp_ip) + ohlen;
		if (fin->fin_hlen < fin->fin_plen)
			xtra = MIN(fin->fin_dlen, 8);
		else
			xtra = 0;
	}

#ifdef USE_INET6
	else if (fin->fin_v == 6) {
		hlen = sizeof(ip6_t);
		ohlen = sizeof(ip6_t);
		iclen = hlen + offsetof(struct icmp, icmp_ip) + ohlen;
		type = icmptoicmp6types[type];
		if (type == ICMP6_DST_UNREACH)
			code = icmptoicmp6unreach[code];

		if (iclen + max_linkhdr + fin->fin_plen > avail) {
			if (!(MCLGET(m, M_NOWAIT))) {
				FREE_MB_T(m);
				return -1;
			}
			avail = MCLBYTES;
		}
		xtra = MIN(fin->fin_plen, avail - iclen - max_linkhdr);
		xtra = MIN(xtra, IPV6_MMTU - iclen);
		if (dst == 0) {
			if (ipf_ifpaddr(&ipfmain, 6, FRI_NORMAL, ifp,
					&dst6, NULL) == -1) {
				FREE_MB_T(m);
				return -1;
			}
		} else
			dst6 = fin->fin_dst6;
	}
#endif
	else {
		FREE_MB_T(m);
		return -1;
	}

	avail -= (max_linkhdr + iclen);
	if (avail < 0) {
		FREE_MB_T(m);
		return -1;
	}
	if (xtra > avail)
		xtra = avail;
	iclen += xtra;
	m->m_data += max_linkhdr;
	m->m_pkthdr.rcvif = (struct ifnet *)0;
	m->m_pkthdr.len = iclen;
	m->m_len = iclen;
	ip = mtod(m, ip_t *);
	icmp = (struct icmp *)((char *)ip + hlen);
	ip2 = (ip_t *)&icmp->icmp_ip;

	icmp->icmp_type = type;
	icmp->icmp_code = fin->fin_icode;
	icmp->icmp_cksum = 0;
#ifdef icmp_nextmtu
	if (type == ICMP_UNREACH && fin->fin_icode == ICMP_UNREACH_NEEDFRAG) {
		if (fin->fin_mtu != 0) {
			icmp->icmp_nextmtu = htons(fin->fin_mtu);

		} else if (ifp != NULL) {
			icmp->icmp_nextmtu = htons(GETIFMTU_4(ifp));

		} else {	/* make up a number... */
			icmp->icmp_nextmtu = htons(fin->fin_plen - 20);
		}
	}
#endif

	bcopy((char *)fin->fin_ip, (char *)ip2, ohlen);

#ifdef USE_INET6
	ip6 = (ip6_t *)ip;
	if (fin->fin_v == 6) {
		ip6->ip6_flow = ((ip6_t *)fin->fin_ip)->ip6_flow;
		ip6->ip6_plen = htons(iclen - hlen);
		ip6->ip6_nxt = IPPROTO_ICMPV6;
		ip6->ip6_hlim = 0;
		ip6->ip6_src = dst6.in6;
		ip6->ip6_dst = fin->fin_src6.in6;
		if (xtra > 0)
			bcopy((char *)fin->fin_ip + ohlen,
			      (char *)&icmp->icmp_ip + ohlen, xtra);
		icmp->icmp_cksum = in6_cksum(m, IPPROTO_ICMPV6,
					     sizeof(*ip6), iclen - hlen);
	} else
#endif
	{
		ip->ip_p = IPPROTO_ICMP;
		ip->ip_src.s_addr = dst4.s_addr;
		ip->ip_dst.s_addr = fin->fin_saddr;

		if (xtra > 0)
			bcopy((char *)fin->fin_ip + ohlen,
			      (char *)&icmp->icmp_ip + ohlen, xtra);
		icmp->icmp_cksum = ipf_cksum((u_short *)icmp,
					     sizeof(*icmp) + 8);
		ip->ip_len = htons(iclen);
		ip->ip_p = IPPROTO_ICMP;
	}
	err = ipf_send_ip(fin, m);
	return err;
}




/*
 * m0 - pointer to mbuf where the IP packet starts
 * mpp - pointer to the mbuf pointer that is the start of the mbuf chain
 */
int
ipf_fastroute(m0, mpp, fin, fdp)
	mb_t *m0, **mpp;
	fr_info_t *fin;
	frdest_t *fdp;
{
	register struct ip *ip, *mhip;
	register struct mbuf *m = *mpp;
	register struct route *ro;
	int len, off, error = 0, hlen, code;
	struct ifnet *ifp, *sifp;
	struct sockaddr_in *dst;
	struct route iproute;
	u_short ip_off;
	frdest_t node;
	frentry_t *fr;

	ro = NULL;

#ifdef M_WRITABLE
	/*
	* HOT FIX/KLUDGE:
	*
	* If the mbuf we're about to send is not writable (because of
	* a cluster reference, for example) we'll need to make a copy
	* of it since this routine modifies the contents.
	*
	* If you have non-crappy network hardware that can transmit data
	* from the mbuf, rather than making a copy, this is gonna be a
	* problem.
	*/
	if (M_WRITABLE(m) == 0) {
		m0 = m_dup(m, M_NOWAIT);
		if (m0 != 0) {
			FREE_MB_T(m);
			m = m0;
			*mpp = m;
		} else {
			error = ENOBUFS;
			FREE_MB_T(m);
			goto done;
		}
	}
#endif

#ifdef USE_INET6
	if (fin->fin_v == 6) {
		/*
		 * currently "to <if>" and "to <if>:ip#" are not supported
		 * for IPv6
		 */
		return ip6_output(m, NULL, NULL, 0, NULL, NULL, NULL);
	}
#endif

	hlen = fin->fin_hlen;
	ip = mtod(m0, struct ip *);
	ifp = NULL;

	/*
	 * Route packet.
	 */
	ro = &iproute;
	bzero(ro, sizeof (*ro));
	dst = (struct sockaddr_in *)&ro->ro_dst;
	dst->sin_family = AF_INET;
	dst->sin_addr = ip->ip_dst;

	fr = fin->fin_fr;
	if ((fr != NULL) && !(fr->fr_flags & FR_KEEPSTATE) && (fdp != NULL) &&
	    (fdp->fd_type == FRD_DSTLIST)) {
		if (ipf_dstlist_select_node(fin, fdp->fd_ptr, NULL, &node) == 0)
			fdp = &node;
	}

	if (fdp != NULL)
		ifp = fdp->fd_ptr;
	else
		ifp = fin->fin_ifp;

	if ((ifp == NULL) && ((fr == NULL) || !(fr->fr_flags & FR_FASTROUTE))) {
		error = -2;
		goto bad;
	}

	if ((fdp != NULL) && (fdp->fd_ip.s_addr != 0))
		dst->sin_addr = fdp->fd_ip;

	dst->sin_len = sizeof(*dst);
	in_rtalloc(ro, M_GETFIB(m0));

	if ((ifp == NULL) && (ro->ro_rt != NULL))
		ifp = ro->ro_rt->rt_ifp;

	if ((ro->ro_rt == NULL) || (ifp == NULL)) {
		if (in_localaddr(ip->ip_dst))
			error = EHOSTUNREACH;
		else
			error = ENETUNREACH;
		goto bad;
	}
	if (ro->ro_rt->rt_flags & RTF_GATEWAY)
		dst = (struct sockaddr_in *)ro->ro_rt->rt_gateway;
	if (ro->ro_rt)
		counter_u64_add(ro->ro_rt->rt_pksent, 1);

	/*
	 * For input packets which are being "fastrouted", they won't
	 * go back through output filtering and miss their chance to get
	 * NAT'd and counted.  Duplicated packets aren't considered to be
	 * part of the normal packet stream, so do not NAT them or pass
	 * them through stateful checking, etc.
	 */
	if ((fdp != &fr->fr_dif) && (fin->fin_out == 0)) {
		sifp = fin->fin_ifp;
		fin->fin_ifp = ifp;
		fin->fin_out = 1;
		(void) ipf_acctpkt(fin, NULL);
		fin->fin_fr = NULL;
		if (!fr || !(fr->fr_flags & FR_RETMASK)) {
			u_32_t pass;

			(void) ipf_state_check(fin, &pass);
		}

		switch (ipf_nat_checkout(fin, NULL))
		{
		case 0 :
			break;
		case 1 :
			ip->ip_sum = 0;
			break;
		case -1 :
			error = -1;
			goto bad;
			break;
		}

		fin->fin_ifp = sifp;
		fin->fin_out = 0;
	} else
		ip->ip_sum = 0;
	/*
	 * If small enough for interface, can just send directly.
	 */
	if (ntohs(ip->ip_len) <= ifp->if_mtu) {
		if (!ip->ip_sum)
			ip->ip_sum = in_cksum(m, hlen);
		error = (*ifp->if_output)(ifp, m, (struct sockaddr *)dst,
			    ro
			);
		goto done;
	}
	/*
	 * Too large for interface; fragment if possible.
	 * Must be able to put at least 8 bytes per fragment.
	 */
	ip_off = ntohs(ip->ip_off);
	if (ip_off & IP_DF) {
		error = EMSGSIZE;
		goto bad;
	}
	len = (ifp->if_mtu - hlen) &~ 7;
	if (len < 8) {
		error = EMSGSIZE;
		goto bad;
	}

    {
	int mhlen, firstlen = len;
	struct mbuf **mnext = &m->m_act;

	/*
	 * Loop through length of segment after first fragment,
	 * make new header and copy data of each part and link onto chain.
	 */
	m0 = m;
	mhlen = sizeof (struct ip);
	for (off = hlen + len; off < ntohs(ip->ip_len); off += len) {
#ifdef MGETHDR
		MGETHDR(m, M_NOWAIT, MT_HEADER);
#else
		MGET(m, M_NOWAIT, MT_HEADER);
#endif
		if (m == 0) {
			m = m0;
			error = ENOBUFS;
			goto bad;
		}
		m->m_data += max_linkhdr;
		mhip = mtod(m, struct ip *);
		bcopy((char *)ip, (char *)mhip, sizeof(*ip));
		if (hlen > sizeof (struct ip)) {
			mhlen = ip_optcopy(ip, mhip) + sizeof (struct ip);
			IP_HL_A(mhip, mhlen >> 2);
		}
		m->m_len = mhlen;
		mhip->ip_off = ((off - hlen) >> 3) + ip_off;
		if (off + len >= ntohs(ip->ip_len))
			len = ntohs(ip->ip_len) - off;
		else
			mhip->ip_off |= IP_MF;
		mhip->ip_len = htons((u_short)(len + mhlen));
		*mnext = m;
		m->m_next = m_copy(m0, off, len);
		if (m->m_next == 0) {
			error = ENOBUFS;	/* ??? */
			goto sendorfree;
		}
		m->m_pkthdr.len = mhlen + len;
		m->m_pkthdr.rcvif = NULL;
		mhip->ip_off = htons((u_short)mhip->ip_off);
		mhip->ip_sum = 0;
		mhip->ip_sum = in_cksum(m, mhlen);
		mnext = &m->m_act;
	}
	/*
	 * Update first fragment by trimming what's been copied out
	 * and updating header, then send each fragment (in order).
	 */
	m_adj(m0, hlen + firstlen - ip->ip_len);
	ip->ip_len = htons((u_short)(hlen + firstlen));
	ip->ip_off = htons((u_short)IP_MF);
	ip->ip_sum = 0;
	ip->ip_sum = in_cksum(m0, hlen);
sendorfree:
	for (m = m0; m; m = m0) {
		m0 = m->m_act;
		m->m_act = 0;
		if (error == 0)
			error = (*ifp->if_output)(ifp, m,
			    (struct sockaddr *)dst,
			    ro
			    );
		else
			FREE_MB_T(m);
	}
    }
done:
	if (!error)
		ipfmain.ipf_frouteok[0]++;
	else
		ipfmain.ipf_frouteok[1]++;

	if ((ro != NULL) && (ro->ro_rt != NULL)) {
		RTFREE(ro->ro_rt);
	}
	return 0;
bad:
	if (error == EMSGSIZE) {
		sifp = fin->fin_ifp;
		code = fin->fin_icode;
		fin->fin_icode = ICMP_UNREACH_NEEDFRAG;
		fin->fin_ifp = ifp;
		(void) ipf_send_icmp_err(ICMP_UNREACH, fin, 1);
		fin->fin_ifp = sifp;
		fin->fin_icode = code;
	}
	FREE_MB_T(m);
	goto done;
}


int
ipf_verifysrc(fin)
	fr_info_t *fin;
{
	struct sockaddr_in *dst;
	struct route iproute;

	bzero((char *)&iproute, sizeof(iproute));
	dst = (struct sockaddr_in *)&iproute.ro_dst;
	dst->sin_len = sizeof(*dst);
	dst->sin_family = AF_INET;
	dst->sin_addr = fin->fin_src;
	in_rtalloc(&iproute, 0);
	if (iproute.ro_rt == NULL)
		return 0;
	return (fin->fin_ifp == iproute.ro_rt->rt_ifp);
}


/*
 * return the first IP Address associated with an interface
 */
int
ipf_ifpaddr(softc, v, atype, ifptr, inp, inpmask)
	ipf_main_softc_t *softc;
	int v, atype;
	void *ifptr;
	i6addr_t *inp, *inpmask;
{
#ifdef USE_INET6
	struct in6_addr *inp6 = NULL;
#endif
	struct sockaddr *sock, *mask;
	struct sockaddr_in *sin;
	struct ifaddr *ifa;
	struct ifnet *ifp;

	if ((ifptr == NULL) || (ifptr == (void *)-1))
		return -1;

	sin = NULL;
	ifp = ifptr;

	if (v == 4)
		inp->in4.s_addr = 0;
#ifdef USE_INET6
	else if (v == 6)
		bzero((char *)inp, sizeof(*inp));
#endif
	ifa = TAILQ_FIRST(&ifp->if_addrhead);

	sock = ifa->ifa_addr;
	while (sock != NULL && ifa != NULL) {
		sin = (struct sockaddr_in *)sock;
		if ((v == 4) && (sin->sin_family == AF_INET))
			break;
#ifdef USE_INET6
		if ((v == 6) && (sin->sin_family == AF_INET6)) {
			inp6 = &((struct sockaddr_in6 *)sin)->sin6_addr;
			if (!IN6_IS_ADDR_LINKLOCAL(inp6) &&
			    !IN6_IS_ADDR_LOOPBACK(inp6))
				break;
		}
#endif
		ifa = TAILQ_NEXT(ifa, ifa_link);
		if (ifa != NULL)
			sock = ifa->ifa_addr;
	}

	if (ifa == NULL || sin == NULL)
		return -1;

	mask = ifa->ifa_netmask;
	if (atype == FRI_BROADCAST)
		sock = ifa->ifa_broadaddr;
	else if (atype == FRI_PEERADDR)
		sock = ifa->ifa_dstaddr;

	if (sock == NULL)
		return -1;

#ifdef USE_INET6
	if (v == 6) {
		return ipf_ifpfillv6addr(atype, (struct sockaddr_in6 *)sock,
					 (struct sockaddr_in6 *)mask,
					 inp, inpmask);
	}
#endif
	return ipf_ifpfillv4addr(atype, (struct sockaddr_in *)sock,
				 (struct sockaddr_in *)mask,
				 &inp->in4, &inpmask->in4);
}


u_32_t
ipf_newisn(fin)
	fr_info_t *fin;
{
	u_32_t newiss;
	newiss = arc4random();
	return newiss;
}


INLINE int
ipf_checkv4sum(fin)
	fr_info_t *fin;
{
#ifdef CSUM_DATA_VALID
	int manual = 0;
	u_short sum;
	ip_t *ip;
	mb_t *m;

	if ((fin->fin_flx & FI_NOCKSUM) != 0)
		return 0;

	if ((fin->fin_flx & FI_SHORT) != 0)
		return 1;

	if (fin->fin_cksum != FI_CK_NEEDED)
		return (fin->fin_cksum > FI_CK_NEEDED) ? 0 : -1;

	m = fin->fin_m;
	if (m == NULL) {
		manual = 1;
		goto skipauto;
	}
	ip = fin->fin_ip;

	if ((m->m_pkthdr.csum_flags & (CSUM_IP_CHECKED|CSUM_IP_VALID)) ==
	    CSUM_IP_CHECKED) {
		fin->fin_cksum = FI_CK_BAD;
		fin->fin_flx |= FI_BAD;
		return -1;
	}
	if (m->m_pkthdr.csum_flags & CSUM_DATA_VALID) {
		/* UDP may have zero checksum */
		if (fin->fin_p == IPPROTO_UDP && (fin->fin_flx & (FI_FRAG|FI_SHORT|FI_BAD)) == 0) {
			udphdr_t *udp = fin->fin_dp;
			if (udp->uh_sum == 0) {
				/* we're good no matter what the hardware checksum flags
				   and csum_data say (handling of csum_data for zero UDP
				   checksum is not consistent across all drivers) */
				fin->fin_cksum = 1;
				return 0;
			}
		}

		if (m->m_pkthdr.csum_flags & CSUM_PSEUDO_HDR)
			sum = m->m_pkthdr.csum_data;
		else
			sum = in_pseudo(ip->ip_src.s_addr, ip->ip_dst.s_addr,
					htonl(m->m_pkthdr.csum_data +
					fin->fin_dlen + fin->fin_p));
		sum ^= 0xffff;
		if (sum != 0) {
			fin->fin_cksum = FI_CK_BAD;
			fin->fin_flx |= FI_BAD;
		} else {
			fin->fin_cksum = FI_CK_SUMOK;
			return 0;
		}
	} else {
		if (m->m_pkthdr.csum_flags == CSUM_DELAY_DATA) {
			fin->fin_cksum = FI_CK_L4FULL;
			return 0;
		} else if (m->m_pkthdr.csum_flags == CSUM_TCP ||
			   m->m_pkthdr.csum_flags == CSUM_UDP) {
			fin->fin_cksum = FI_CK_L4PART;
			return 0;
		} else if (m->m_pkthdr.csum_flags == CSUM_IP) {
			fin->fin_cksum = FI_CK_L4PART;
			return 0;
		} else {
			manual = 1;
		}
	}
skipauto:
	if (manual != 0) {
		if (ipf_checkl4sum(fin) == -1) {
			fin->fin_flx |= FI_BAD;
			return -1;
		}
	}
#else
	if (ipf_checkl4sum(fin) == -1) {
		fin->fin_flx |= FI_BAD;
		return -1;
	}
#endif
	return 0;
}


#ifdef USE_INET6
INLINE int
ipf_checkv6sum(fin)
	fr_info_t *fin;
{
	if ((fin->fin_flx & FI_NOCKSUM) != 0)
		return 0;

	if ((fin->fin_flx & FI_SHORT) != 0)
		return 1;

	if (fin->fin_cksum != FI_CK_NEEDED)
		return (fin->fin_cksum > FI_CK_NEEDED) ? 0 : -1;

	if (ipf_checkl4sum(fin) == -1) {
		fin->fin_flx |= FI_BAD;
		return -1;
	}
	return 0;
}
#endif /* USE_INET6 */


size_t
mbufchainlen(m0)
	struct mbuf *m0;
	{
	size_t len;

	if ((m0->m_flags & M_PKTHDR) != 0) {
		len = m0->m_pkthdr.len;
	} else {
		struct mbuf *m;

		for (m = m0, len = 0; m != NULL; m = m->m_next)
			len += m->m_len;
	}
	return len;
}


/* ------------------------------------------------------------------------ */
/* Function:    ipf_pullup                                                  */
/* Returns:     NULL == pullup failed, else pointer to protocol header      */
/* Parameters:  xmin(I)- pointer to buffer where data packet starts         */
/*              fin(I) - pointer to packet information                      */
/*              len(I) - number of bytes to pullup                          */
/*                                                                          */
/* Attempt to move at least len bytes (from the start of the buffer) into a */
/* single buffer for ease of access.  Operating system native functions are */
/* used to manage buffers - if necessary.  If the entire packet ends up in  */
/* a single buffer, set the FI_COALESCE flag even though ipf_coalesce() has */
/* not been called.  Both fin_ip and fin_dp are updated before exiting _IF_ */
/* and ONLY if the pullup succeeds.                                         */
/*                                                                          */
/* We assume that 'xmin' is a pointer to a buffer that is part of the chain */
/* of buffers that starts at *fin->fin_mp.                                  */
/* ------------------------------------------------------------------------ */
void *
ipf_pullup(xmin, fin, len)
	mb_t *xmin;
	fr_info_t *fin;
	int len;
{
	int dpoff, ipoff;
	mb_t *m = xmin;
	char *ip;

	if (m == NULL)
		return NULL;

	ip = (char *)fin->fin_ip;
	if ((fin->fin_flx & FI_COALESCE) != 0)
		return ip;

	ipoff = fin->fin_ipoff;
	if (fin->fin_dp != NULL)
		dpoff = (char *)fin->fin_dp - (char *)ip;
	else
		dpoff = 0;

	if (M_LEN(m) < len) {
		mb_t *n = *fin->fin_mp;
		/*
		 * Assume that M_PKTHDR is set and just work with what is left
		 * rather than check..
		 * Should not make any real difference, anyway.
		 */
		if (m != n) {
			/*
			 * Record the mbuf that points to the mbuf that we're
			 * about to go to work on so that we can update the
			 * m_next appropriately later.
			 */
			for (; n->m_next != m; n = n->m_next)
				;
		} else {
			n = NULL;
		}

#ifdef MHLEN
		if (len > MHLEN)
#else
		if (len > MLEN)
#endif
		{
#ifdef HAVE_M_PULLDOWN
			if (m_pulldown(m, 0, len, NULL) == NULL)
				m = NULL;
#else
			FREE_MB_T(*fin->fin_mp);
			m = NULL;
			n = NULL;
#endif
		} else
		{
			m = m_pullup(m, len);
		}
		if (n != NULL)
			n->m_next = m;
		if (m == NULL) {
			/*
			 * When n is non-NULL, it indicates that m pointed to
			 * a sub-chain (tail) of the mbuf and that the head
			 * of this chain has not yet been free'd.
			 */
			if (n != NULL) {
				FREE_MB_T(*fin->fin_mp);
			}

			*fin->fin_mp = NULL;
			fin->fin_m = NULL;
			return NULL;
		}

		if (n == NULL)
			*fin->fin_mp = m;

		while (M_LEN(m) == 0) {
			m = m->m_next;
		}
		fin->fin_m = m;
		ip = MTOD(m, char *) + ipoff;

		fin->fin_ip = (ip_t *)ip;
		if (fin->fin_dp != NULL)
			fin->fin_dp = (char *)fin->fin_ip + dpoff;
		if (fin->fin_fraghdr != NULL)
			fin->fin_fraghdr = (char *)ip +
					   ((char *)fin->fin_fraghdr -
					    (char *)fin->fin_ip);
	}

	if (len == fin->fin_plen)
		fin->fin_flx |= FI_COALESCE;
	return ip;
}


int
ipf_inject(fin, m)
	fr_info_t *fin;
	mb_t *m;
{
	int error = 0;

	if (fin->fin_out == 0) {
		netisr_dispatch(NETISR_IP, m);
	} else {
		fin->fin_ip->ip_len = ntohs(fin->fin_ip->ip_len);
		fin->fin_ip->ip_off = ntohs(fin->fin_ip->ip_off);
		error = ip_output(m, NULL, NULL, IP_FORWARDING, NULL, NULL);
	}

	return error;
}

int ipf_pfil_unhook(void) {
#if defined(NETBSD_PF) && (__FreeBSD_version >= 500011)
	struct pfil_head *ph_inet;
#  ifdef USE_INET6
	struct pfil_head *ph_inet6;
#  endif
#endif

#ifdef NETBSD_PF
	ph_inet = pfil_head_get(PFIL_TYPE_AF, AF_INET);
	if (ph_inet != NULL)
		pfil_remove_hook((void *)ipf_check_wrapper, NULL,
		    PFIL_IN|PFIL_OUT|PFIL_WAITOK, ph_inet);
# ifdef USE_INET6
	ph_inet6 = pfil_head_get(PFIL_TYPE_AF, AF_INET6);
	if (ph_inet6 != NULL)
		pfil_remove_hook((void *)ipf_check_wrapper6, NULL,
		    PFIL_IN|PFIL_OUT|PFIL_WAITOK, ph_inet6);
# endif
#endif

	return (0);
}

int ipf_pfil_hook(void) {
#if defined(NETBSD_PF) && (__FreeBSD_version >= 500011)
	struct pfil_head *ph_inet;
#  ifdef USE_INET6
	struct pfil_head *ph_inet6;
#  endif
#endif

# ifdef NETBSD_PF
	ph_inet = pfil_head_get(PFIL_TYPE_AF, AF_INET);
#    ifdef USE_INET6
	ph_inet6 = pfil_head_get(PFIL_TYPE_AF, AF_INET6);
#    endif
	if (ph_inet == NULL
#    ifdef USE_INET6
	    && ph_inet6 == NULL
#    endif
	   ) {
		return ENODEV;
	}

	if (ph_inet != NULL)
		pfil_add_hook((void *)ipf_check_wrapper, NULL,
		    PFIL_IN|PFIL_OUT|PFIL_WAITOK, ph_inet);
#  ifdef USE_INET6
	if (ph_inet6 != NULL)
		pfil_add_hook((void *)ipf_check_wrapper6, NULL,
				      PFIL_IN|PFIL_OUT|PFIL_WAITOK, ph_inet6);
#  endif
# endif
	return (0);
}

void
ipf_event_reg(void)
{
	ipf_arrivetag = EVENTHANDLER_REGISTER(ifnet_arrival_event, \
					       ipf_ifevent, &ipfmain, \
					       EVENTHANDLER_PRI_ANY);
	ipf_departtag = EVENTHANDLER_REGISTER(ifnet_departure_event, \
					       ipf_ifevent, &ipfmain, \
					       EVENTHANDLER_PRI_ANY);
	ipf_clonetag  = EVENTHANDLER_REGISTER(if_clone_event, ipf_ifevent, \
					       &ipfmain, EVENTHANDLER_PRI_ANY);
}

void
ipf_event_dereg(void)
{
	if (ipf_arrivetag != NULL) {
		EVENTHANDLER_DEREGISTER(ifnet_arrival_event, ipf_arrivetag);
	}
	if (ipf_departtag != NULL) {
		EVENTHANDLER_DEREGISTER(ifnet_departure_event, ipf_departtag);
	}
	if (ipf_clonetag != NULL) {
		EVENTHANDLER_DEREGISTER(if_clone_event, ipf_clonetag);
	}
}


u_32_t
ipf_random()
{
	return arc4random();
}


u_int
ipf_pcksum(fin, hlen, sum)
	fr_info_t *fin;
	int hlen;
	u_int sum;
{
	struct mbuf *m;
	u_int sum2;
	int off;

	m = fin->fin_m;
	off = (char *)fin->fin_dp - (char *)fin->fin_ip;
	m->m_data += hlen;
	m->m_len -= hlen;
	sum2 = in_cksum(fin->fin_m, fin->fin_plen - off);
	m->m_len += hlen;
	m->m_data -= hlen;

	/*
	 * Both sum and sum2 are partial sums, so combine them together.
	 */
	sum += ~sum2 & 0xffff;
	while (sum > 0xffff)
		sum = (sum & 0xffff) + (sum >> 16);
	sum2 = ~sum & 0xffff;
	return sum2;
}
