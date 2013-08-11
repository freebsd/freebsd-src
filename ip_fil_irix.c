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
# define        KERNEL	1
# define        _KERNEL	1
#endif
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/dir.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if.h>
#include <sys/debug.h>
#ifdef IFF_DRVRLOCK /* IRIX6 */
# include <sys/hashing.h>
#endif
#include <net/route.h>
#include <netinet/in.h>
#if !defined(IFF_DRVRLOCK) /* IRIX < 6 */
# include <netinet/in_var.h>
#endif
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
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

#include "md5.h"

extern	int	tcp_mtudisc;
extern	int	ipforwarding;
extern	struct	protosw	inetsw[];
extern	int	tcp_ttl;
#if IRIX >= 60500
extern	toid_t	ipf_timer_id;
#endif

static	u_short	ipid = 0;
static	int	(*ipf_savep) __P((ip_t *, int, void *, int, struct mbuf **));
static	int	ipf_send_ip __P((fr_info_t *, struct mbuf *));
static	int	ipfopen __P((dev_t *, int, int, cred_t *));
static	int	ipfclose __P((dev_t *, int, int, cred_t *));
static	int	ipfread __P((dev_t, struct uio *, cred_t *));


int
ipf_attach()
{
	int error = 0, s;

	SPL_NET(s);
	if (ipf_running > 0) {
		SPL_X(s);
		return EBUSY;
	}

	if (ipf_initialise() < 0)
		return -1;

	error = ipf_ipfilter_attach();
	if (error) {
		ipf_deinitialise();
		SPL_X(s);
		return error;
	}

	if (ipf_checkp != ipf_check) {
		ipf_savep = ipf_checkp;
		ipf_checkp = ipf_check;
	}

	if (ipf_control_forwarding & 1)
		ipforwarding = 1;

	ipid = 0;

	SPL_X(s);

#if IRIX >= 60500
	ipf_timer_id = timeout(ipf_slowtimer, &ipfmain, hz/2);
#else
	timeout(ipf_slowtimer, &ipfmain, hz/2);
#endif
	return 0;
}


/*
 * Disable the filter by removing the hooks from the IP input/output
 * stream.
 */
int
ipf_detach()
{
	int s, error = 0;

	SPL_NET(s);

#if IRIX >= 60500
	if (ipf_timer_id != 0) {
		/* error = untimeout(ipf_timer_id); XXX - does not work with */
		/* timeout() return value, only itimeout() and dtimeout().  */
		ipf_timer_id = 0;
	}
#else
	untimeout(ipf_slowtimer);
#endif

	if (ipf_control_forwarding & 2)
		ipforwarding = 0;

	ipf_deinitialise();

	if (ipf_savep != NULL)
		ipf_checkp = ipf_savep;
	ipf_savep = NULL;
	(void) frflush(&ipfmain, IPL_LOGIPF, FR_INQUE|FR_OUTQUE|FR_INACTIVE);
	(void) frflush(&ipfmain, IPL_LOGIPF, FR_INQUE|FR_OUTQUE);

	ipf_ipfilter_detach();

	SPL_X(s);
	return 0;
}


/*
 * Filter ioctl interface.
 */
int
ipfioctl(dev, cmd, data, mode, cp, rp)
	dev_t dev;
	int cmd;
	caddr_t data;
	int mode;
	cred_t *cp;
	int *rp;
{
	int error = 0, unit = 0;
	SPL_INT(s);

	unit = GET_MINOR(dev);
	if ((IPL_LOGMAX < unit) || (unit < 0)) {
		ipfmain.ipf_interror = 130002;
		return ENXIO;
	}

	if (ipf_running <= 0) {
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

	error = ipf_ioctlswitch(&ipfmain, unit, data, cmd, mode,
				cp->cr_uid, curproc);
	if (error != -1) {
		SPL_X(s);
		return error;
	}
	error = 0;

	SPL_X(s);
	return error;
}


/*
 * routines below for saving IP headers to buffer
 */
static int
ipfopen(pdev, flags, devtype, cp)
	dev_t *pdev;
	int flags, devtype;
	cred_t *cp;
{
	u_int unit = geteminor(*pdev);
	int error;

	if (IPL_LOGMAX < unit) {
		error = ENXIO;
	} else {
		switch (unit)
		{
		case IPL_LOGIPF :
		case IPL_LOGNAT :
		case IPL_LOGSTATE :
		case IPL_LOGAUTH :
		case IPL_LOGLOOKUP :
		case IPL_LOGSYNC :
#ifdef IPFILTER_SCAN
		case IPL_LOGSCAN :
#endif
			error = 0;
			break;
		default :
			error = ENXIO;
			break;
		}
	}
	return error;
}


static int
ipfclose(dev, flags, devtype, cp)
	dev_t dev;
	int flags, devtype;
	cred_t *cp;
{
	u_int	unit = GET_MINOR(dev);

	if (IPL_LOGMAX < unit)
		unit = ENXIO;
	else
		unit = 0;
	return unit;
}

/*
 * ipfread/ipflog
 * both of these must operate with at least splnet() lest they be
 * called during packet processing and cause an inconsistancy to appear in
 * the filter lists.
 */
static int
ipfread(dev, uio, crp)
	dev_t dev;
	register struct uio *uio;
	cred_t *crp;
{
	if (ipf_running < 1) {
		ipfmain.ipf_interror = 130006;
		return EIO;
	}

#ifdef IPFILTER_LOG
	return ipflog_read(GET_MINOR(dev), uio);
#else
	ipfmain.ipf_interror = 130007;
	return ENXIO;
#endif
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
#ifdef	USE_INET6
	ip6_t *ip6;
#endif
	ip_t *ip;

	tcp = fin->fin_dp;
	if (tcp->th_flags & TH_RST)
		return -1;		/* feedback loop */

	if (ipf_checkl4sum(fin) == -1)
		return -1;

	m = m_get(M_DONTWAIT, MT_HEADER);
	if (m == NULL)
		return -1;

	tlen = fin->fin_dlen - (TCP_OFF(tcp) << 2) +
			((tcp->th_flags & TH_SYN) ? 1 : 0) +
			((tcp->th_flags & TH_FIN) ? 1 : 0);

#ifdef	USE_INET6
	hlen = (fin->fin_v == 6) ? sizeof(ip6_t) : sizeof(ip_t);
#else
	hlen = sizeof(ip_t);
#endif
	m->m_len = sizeof(*tcp2) + hlen;
# if (BSD >= 199103)
	m->m_data += max_linkhdr;
	m->m_pkthdr.len = m->m_len;
	m->m_pkthdr.rcvif = (struct ifnet *)0;
# endif
	ip = mtod(m, struct ip *);
	bzero((char *)ip, hlen);
# ifdef	USE_INET6
	ip6 = (ip6_t *)ip;
# endif
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
	tcp->th_sum = 0;
	tcp->th_urp = 0;

# ifdef	USE_INET6
	if (fin->fin_v == 6) {
		ip6->ip6_flow = ((ip6_t *)fin->fin_ip)->ip6_flow;
		ip6->ip6_plen = htons(sizeof(struct tcphdr));
		ip6->ip6_nxt = IPPROTO_TCP;
		ip6->ip6_hlim = 0;
		ip6->ip6_src = fin->fin_dst6;
		ip6->ip6_dst = fin->fin_src6;
		tcp2->th_sum = in6_cksum(m, IPPROTO_TCP,
					 sizeof(*ip6), sizeof(*tcp2));
		return ipf_send_ip(fin, m);
	}
# endif
	ip->ip_p = IPPROTO_TCP;
	ip->ip_len = htons(sizeof(struct tcphdr));
	ip->ip_src.s_addr = fin->fin_daddr;
	ip->ip_dst.s_addr = fin->fin_saddr;
	tcp2->th_sum = in_cksum(m, hlen + sizeof(*tcp2));
	ip->ip_len = hlen + sizeof(*tcp2);
	return ipf_send_ip(fin, m);
}


static int
ipf_send_ip(fin, m)
	fr_info_t *fin;
	struct mbuf *m;
{
	fr_info_t fnew;
	ip_t *oip;
	ip_t *ip;
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
		fnew.fin_plen = ip->ip_len;
		IP_HL_A(ip, sizeof(*oip) >> 2);
		ip->ip_tos = oip->ip_tos;
		ip->ip_id = fin->fin_ip->ip_id;
		if (ip->ip_p == IPPROTO_TCP && tcp_mtudisc != 0)
			ip->ip_off = htons(IP_DF);
		ip->ip_ttl = tcp_ttl;
		ip->ip_sum = 0;
		ip->ip_len = htons(ip->ip_len);
		break;
#ifdef	USE_INET6
	case 6 :
	{
		ip6_t *ip6 = (ip6_t *)ip;

		ip6->ip6_hlim = 127;
		m = *mpp;

		return ip6_output(m, NULL, NULL, 0, NULL, NULL);
	}
#endif
	default :
		return EINVAL;
	}
#ifdef	IPSEC
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
	int err, hlen = 0, xtra = 0, iclen, ohlen = 0, avail;
	struct in_addr dst4;
	struct icmp *icmp;
	struct mbuf *m;
	i6addr_t dst6;
	void *ifp;
#ifdef USE_INET6
	ip6_t *ip6, *ip62;
	int code;
#endif
	ip_t *ip, *ip2;

	if ((type < 0) || (type > ICMP_MAXTYPE))
		return -1;

#ifdef USE_INET6
	code = fin->fin_icode;
	if ((code < 0) || (code > sizeof(icmptoicmp6unreach)/sizeof(int)))
		return -1;
#endif

	if (ipf_checkl4sum(fin) == -1)
		return -1;

	avail = 0;
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
				return 0;
			}

		avail = MLEN;
		m = m_get(M_DONTWAIT, MT_HEADER);
		if (m == NULL)
			return -1;

		if (dst == 0) {
			if (ipf_ifpaddr(softc, 4, FRI_NORMAL, ifp,
					&dst6, NULL) == -1) {
				FREE_MB_T(m);
				return -1;
			}
			dst4 = dst6.in4;
		} else
			dst4.s_addr = fin->fin_daddr;

		hlen = sizeof(ip_t);
		if (fin->fin_hlen < fin->fin_plen)
			xtra = MIN(fin->fin_dlen, 8);
		else
			xtra = 0;
	}

#ifdef	USE_INET6
	else if (fin->fin_v == 6) {
		hlen = sizeof(ip6_t);
		ohlen = sizeof(ip6_t);
		type = icmptoicmp6types[type];
		if (type == ICMP6_DST_UNREACH)
			code = icmptoicmp6unreach[code];

		MGETHDR(m, M_DONTWAIT, MT_HEADER);
		if (m == NULL)
			return -1;

		MCLGET(m, M_DONTWAIT);
		if (m == NULL)
			return -1;
		avail = (m->m_flags & M_EXT) ? MCLBYTES : MHLEN;
		xtra = MIN(fin->fin_plen,
			   avail - hlen - sizeof(*icmp) - max_linkhdr);
		if (dst == 0) {
			if (ipf_ifpaddr(softc, 6, FRI_NORMAL, ifp,
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

	iclen = hlen + sizeof(*icmp);
# if (BSD >= 199103)
	avail -= (max_linkhdr + iclen);
	if (avail < 0) {
		FREE_MB_T(m);
		return -1;
	}
	m->m_data += max_linkhdr;
	m->m_pkthdr.rcvif = (struct ifnet *)0;
	iclen += xtra;
	m->m_pkthdr.len = iclen;
#else
	avail -= (m->m_off + iclen);
	if (avail < 0) {
		FREE_MB_T(m);
		return -1;
	}
	iclen += xtra;
#endif
	m->m_len = iclen;
	ip = mtod(m, ip_t *);
	icmp = (struct icmp *)((char *)ip + hlen);
	ip2 = (ip_t *)&icmp->icmp_ip;

	icmp->icmp_type = type;
	icmp->icmp_code = fin->fin_icode;
	icmp->icmp_cksum = 0;
#ifdef	icmp_nextmtu
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

#ifdef	USE_INET6
	ip6 = (ip6_t *)ip;
	if (fin->fin_v == 6) {
		ip62 = (ip6_t *)ip2;

		ip62->ip6_plen = htons(ip62->ip6_plen);
		ip6->ip6_flow = ((ip6_t *)fin->fin_ip)->ip6_flow;
		ip6->ip6_plen = htons(iclen - hlen);
		ip6->ip6_nxt = IPPROTO_ICMPV6;
		ip6->ip6_hlim = 0;
		ip6->ip6_src = dst6;
		ip6->ip6_dst = fin->fin_src6;
		if (xtra > 0)
			bcopy((char *)fin->fin_ip + fin->fin_hlen,
			      (char *)&icmp->icmp_ip + fin->fin_hlen, xtra);
		icmp->icmp_cksum = in6_cksum(m, IPPROTO_ICMPV6,
					     sizeof(*ip6), iclen - hlen);
	} else
#endif
	{
		ip->ip_p = IPPROTO_ICMP;
		ip->ip_src.s_addr = dst4.s_addr;
		ip->ip_dst.s_addr = fin->fin_saddr;

		if (xtra > 0)
			bcopy((char *)fin->fin_ip + fin->fin_hlen,
			      (char *)&icmp->icmp_ip + fin->fin_hlen, xtra);
		icmp->icmp_cksum = ipf_cksum((u_short *)icmp,
					     sizeof(*icmp) + 8);
		ip->ip_len = iclen;
		ip->ip_p = IPPROTO_ICMP;
	}
	err = ipf_send_ip(fin, m);
	return err;
}


void
ipfinit(void)
{
	int i;

	for (i = 0; i < 256; i++)
		if (cdevsw[i].d_open == ipfopen) {
			printf("ipfinit:ipfilter @%d\n", i); break;
		}
	if (i == 256)
		printf("ipfinit:ipfilter not found\n");

	if (ipf_attach() != 0)
		printf("IP Filter failed to attach\n");
	else
		ip_init();
}


int
ipfattach(void)
{
	int i;

	for (i = 0; i < 256; i++)
		if (cdevsw[i].d_open == ipfopen) {
			printf("ipfattach:ipfilter @%d\n", i); break;
		}
	if (i == 256)
		printf("ipfattach:ipfilter not found\n");
	return 0;
}

void
ipfstart(void)
{
	int i;

	for (i = 0; i < 256; i++)
		if (cdevsw[i].d_open == ipfopen) {
			printf("ipfstart:ipfilter @%d\n", i);
			break;
		}
	if (i==256)
		printf("ipfstart:ipfilter not found\n");
}


size_t
mbufchainlen(m0)
	register struct mbuf *m0;
{
	register size_t len = 0;

	for (; m0; m0 = m0->m_next)
		len += m0->m_len;
	return len;
}


/*
 * m0 - pointer to mbuf where the IP packet starts
 * mpp - pointer to the mbuf pointer that is the start of the mbuf chain
 */
int
ipf_fastroute(m0, mpp, fin, fdp)
	struct mbuf *m0, **mpp;
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
	frdest_t node;
	frentry_t *fr;

	hlen = fin->fin_hlen;
	ip = mtod(m0, struct ip *);

#ifdef	USE_INET6
	if (fin->fin_v == 6) {
		/*
		 * currently "to <if>" and "to <if>:ip#" are not supported
		 * for IPv6
		 */
		return ip6_output(m0, NULL, NULL, 0, NULL, NULL);
	}
#endif
	/*
	 * Route packet.
	 */
	ROUTE_RDLOCK();

	ro = &iproute;
	bzero((caddr_t)ro, sizeof (*ro));
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

	if ((fdp != NULL) && (fdp->fd_ip.s_addr != 0))
		dst->sin_addr = fdp->fd_ip.s_addr;

	rtalloc(ro);

	ROUTE_UNLOCK();

	if (!ifp) {
		if (!fr || !(fr->fr_flags & FR_FASTROUTE)) {
			error = -2;
			goto bad;
		}
		if (ro->ro_rt == 0 || (ifp = ro->ro_rt->rt_ifp) == 0) {
			if (in_localaddr(ip->ip_dst))
				error = EHOSTUNREACH;
			else
				error = ENETUNREACH;
			goto bad;
		}
		if (ro->ro_rt->rt_flags & RTF_GATEWAY)
			dst = (struct sockaddr_in *)&ro->ro_rt->rt_gateway;
	}
	if (ro->ro_rt)
		ro->ro_rt->rt_use++;

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
#if IRIX >= 60500
		error = (*ifp->if_output)(ifp, m, (struct sockaddr *)dst,
					  NULL);
#else
		error = (*ifp->if_output)(ifp, m, (struct sockaddr *)dst);
#endif
		goto done;
	}
	/*
	 * Too large for interface; fragment if possible.
	 * Must be able to put at least 8 bytes per fragment.
	 */
	if (ip->ip_off & htons(IP_DF)) {
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
		u_short ipoff, iplen;

#ifdef	MGETHDR
		MGETHDR(m, M_DONTWAIT, MT_HEADER);
#else
		MGET(m, M_DONTWAIT, MT_HEADER);
#endif
		if (m == 0) {
			error = ENOBUFS;
			goto bad;
		}
#if (BSD >= 199103)
		m->m_data += max_linkhdr;
#else
		m->m_off = MMAXOFF - hlen;
#endif
		mhip = mtod(m, struct ip *);
		bcopy((char *)ip, (char *)mhip, sizeof(*ip));
		if (hlen > sizeof (struct ip)) {
			mhlen = ip_optcopy(ip, mhip) + sizeof (struct ip);
			IP_HL_A(mhip, mhlen >> 2);
		}

		m->m_len = mhlen;
		ipoff = ntohs(ip->ip_off);
		iplen = ntohs(ip->ip_len);

		mhip->ip_off = ((off - hlen) >> 3) + (ipoff & ~IP_MF);
		if (ipoff & IP_MF)
			mhip->ip_off |= IP_MF;
		if (off + len >= iplen)
			len = iplen - off;
		else
			mhip->ip_off |= IP_MF;

		mhip->ip_len = htons((u_short)(len + mhlen));
		m->m_next = m_copy(m0, off, len);
		if (m->m_next == 0) {
			error = ENOBUFS;	/* ??? */
			goto sendorfree;
		}
#if (BSD >= 199103)
		m->m_pkthdr.len = mhlen + len;
		m->m_pkthdr.rcvif = NULL;
#endif
		mhip->ip_off = htons((u_short)mhip->ip_off);
		mhip->ip_sum = 0;
		mhip->ip_sum = in_cksum(m, mhlen);
		*mnext = m;
		mnext = &m->m_act;
	}
	/*
	 * Update first fragment by trimming what's been copied out
	 * and updating header, then send each fragment (in order).
	 */
	m_adj(m0, hlen + firstlen - ntohs(ip->ip_len));
	ip->ip_len = htons((u_short)(hlen + firstlen));
	ip->ip_off = htons(IP_MF);
	ip->ip_sum = 0;
	ip->ip_sum = in_cksum(m0, hlen);
sendorfree:
	for (m = m0; m; m = m0) {
		m0 = m->m_act;
		m->m_act = 0;
		if (error == 0)
#if IRIX >= 60500
			error = (*ifp->if_output)(ifp, m,
			    (struct sockaddr *)dst, ro->ro_rt);
#else
			error = (*ifp->if_output)(ifp, m,
			    (struct sockaddr *)dst);
#endif
		else
			m_freem(m);
	}
    }
done:
	if (!error)
		ipf_frouteok[0]++;
	else
		ipf_frouteok[1]++;

	if (ro->ro_rt)
		RTFREE(ro->ro_rt);
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
	m_freem(m);
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
	dst->sin_family = AF_INET;
	dst->sin_addr.s_addr = fin->fin_saddr;
	rtalloc(&iproute);
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
	struct sockaddr_in *sin, *mask;
	struct ifaddr *ifa;
	struct in_addr in;
	struct ifnet *ifp;

	if ((ifptr == NULL) || (ifptr == (void *)-1))
		return -1;

	mask = NULL;
	ifp = ifptr;

	if (v == 4)
		inp->s_addr = 0;
#ifdef      USE_INET6
	else if (v == 6)
		bzero((char *)inp, sizeof(*inp));
#endif

#if defined(IFF_DRVRLOCK) /* IRIX 6 */
	ifa = &((struct in_ifaddr *)ifp->in_ifaddr)->ia_ifa;
#else
	ifa = ifp->if_addrlist;
#endif

	sin = (struct sockaddr_in *)ifa->ifa_addr;
	while (sin && ifa) {
		if ((v == 4) && (sin->sin_family == AF_INET))
			break;
# ifdef USE_INET6
		if ((v == 6) && (sin->sin_family == AF_INET6)) {
			inp6 = &((struct sockaddr_in6 *)sin)->sin6_addr;
			if (!IN6_IS_ADDR_LINKLOCAL(inp6) &&
			    !IN6_IS_ADDR_LOOPBACK(inp6))
				break;
		}
# endif
		ifa = ifa->ifa_next;
		if (ifa != NULL)
			sin = (struct sockaddr_in *)ifa->ifa_addr;
	}
	if (ifa == NULL || sin == NULL)
		return -1;

	mask = (struct sockaddr_in *)ifa->ifa_netmask;
	if (atype == FRI_BROADCAST)
		sin = (struct sockaddr_in *)ifa->ifa_broadaddr;
	else if (atype == FRI_PEERADDR)
		sin = (struct sockaddr_in *)ifa->ifa_dstaddr;

#ifdef USE_INET6
	if (v == 6)
		return ipf_ifpfillv6addr(atype, (struct sockaddr_in6 *)sin,
					 (struct sockaddr_in6 *)mask,
					 inp, inpmask);
#endif
	return ipf_ifpfillv4addr(atype, sin, mask, &inp->in4, &inpmask->in4);
}


void
#if IRIX >= 60500
ipf_slowtimer(void *arg)
#else
ipf_slowtimer()
#endif
{
	if (ipf_running <= 0) {
#if IRIX >= 60500
		ipf_timer_id = 0;
#endif
		return;
	}

	READ_ENTER(&ipf_global);
	if (ipf_running <= 0) {
#if IRIX >= 60500
		ipf_timer_id = 0;
#endif
		RWLOCK_EXIT(&ipf_global);
		return;
	}

	ipf_ipfilter_intfsync();
	ipf_expiretokens(&ipfmain);
	ipf_frag_expire(&ipfmain);
	ipf_state_expire(&ipfmain);
	ipf_nat_expire(&ipfmain);
	ipf_auth_expire(&ipfmain);
	ipf_lookup_expire(&ipfmain);
	ipf_rule_expire(&ipfmain);
	ipf_ticks++;

#if IRIX >= 60500
	ipf_timer_id = timeout(ipf_slowtimer, NULL, hz/2);
#else
	timeout(ipf_slowtimer, NULL, hz/2);
#endif
	RWLOCK_EXIT(&ipf_global);
}


u_32_t
ipf_newisn(fin)
	fr_info_t *fin;
{
	static iss_seq_off = 0;
	u_char hash[16];
	u_32_t newiss;
	MD5_CTX ctx;

	/*
	 * Compute the base value of the ISS.  It is a hash
	 * of (saddr, sport, daddr, dport, secret).
	 */
	MD5Init(&ctx);

	MD5Update(&ctx, (u_char *) &fin->fin_fi.fi_src,
		  sizeof(fin->fin_fi.fi_src));
	MD5Update(&ctx, (u_char *) &fin->fin_fi.fi_dst,
		  sizeof(fin->fin_fi.fi_dst));
	MD5Update(&ctx, (u_char *) &fin->fin_dat, sizeof(fin->fin_dat));

	MD5Update(&ctx, ipf_iss_secret, sizeof(ipf_iss_secret));

	MD5Final(hash, &ctx);

	bcopy(hash, &newiss, sizeof(newiss));

	/*
	 * Now increment our "timer", and add it in to
	 * the computed value.
	 *
	 * XXX Use `addin'?
	 * XXX TCP_ISSINCR too large to use?
	 */
	iss_seq_off += 0x00010000;
	newiss += iss_seq_off;
	return newiss;
}


/* ------------------------------------------------------------------------ */
/* Function:    ipf_nextipid                                                */
/* Returns:     int - 0 == success, -1 == error (packet should be droppped) */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* Returns the next IPv4 ID to use for this packet.                         */
/* ------------------------------------------------------------------------ */
u_short
ipf_nextipid(fin)
	fr_info_t *fin;
{
	u_short id;

	MUTEX_ENTER(&ipf_rw);
	id = ipid++;
	MUTEX_EXIT(&ipf_rw);

	return id;
}


int
ipf_checkv4sum(fin)
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
/* We assume that 'min' is a pointer to a buffer that is part of the chain  */
/* of buffers that starts at *fin->fin_mp.                                  */
/* ------------------------------------------------------------------------ */
void *
ipf_pullup(xmin, fin, len)
	mb_t *xmin;
	fr_info_t *fin;
	int len;
{
	int out = fin->fin_out, dpoff;
	mb_t *m = xmin, *n;
	char *ip;

	if (m == NULL)
		return NULL;

	ip = (char *)fin->fin_ip;
	if ((fin->fin_flx & FI_COALESCE) != 0)
		return ip;

	if (fin->fin_dp != NULL)
		dpoff = (char *)fin->fin_dp - (char *)ip;
	else
		dpoff = 0;

	if (M_LEN(m) < len) {
		KMALLOCS(fin->fin_hbuf, void *, fin->fin_plen);
		if (fin->fin_hbuf == NULL) {
			return NULL;
		}
		m_copydata(m, 0, fin->fin_plen, fin->fin_hbuf);
		ip = fin->fin_hbuf;
		fin->fin_flx |= FI_COALESCE;
	} else if (len == fin->fin_plen) {
		fin->fin_flx |= FI_COALESCE;
	}

	fin->fin_ip = (ip_t *)ip;
	if (fin->fin_dp != NULL)
		fin->fin_dp = (char *)fin->fin_ip + dpoff;
	if (fin->fin_fraghdr != NULL)
		fin->fin_fraghdr = (char *)ip +
				   ((char *)fin->fin_fraghdr -
				    (char *)fin->fin_ip);
	return ip;
}


int
ipf_inject(fin, m)
	fr_info_t *fin;
	mb_t *m;
{
	int error;

	if (fin->fin_out == 0) {
		struct ifqueue *ifq;

#if (IRIX >= 60516)
		ifq = &((struct ifnet *)fin->fin_ifp)->if_snd;
#else
		ifq = &ipintrq;
#endif

		if (IF_QFULL(ifq)) {
			IF_DROP(ifq);
			FREE_MB_T(m);
			error = ENOBUFS;
		} else {
			IF_ENQUEUE(ifq, m);
#if IRIX < 60500
			schednetisr(NETISR_IP);
#endif
			error = 0;
		}
	} else {
#if IRIX >= 60500
		error = ip_output(m, NULL, NULL, IP_FORWARDING, NULL, NULL);
#else
		error = ip_output(m, NULL, NULL, IP_FORWARDING, NULL);
#endif
	}

	return error;
}


/*
 * In the face of no kernel random function, this is implemented...it is
 * not meant to be random, just a fill in.
 */
int
ipf_random()
{
	static int last = 0;
	static int calls = 0;
	struct timeval tv;
	int number;

	GETKTIME(&tv);
	last *= tv.tv_usec + calls++;
	last += (int)&range * ipf_ticks;
	number = last + tv.tv_sec;
	return number;
}

u_int
ipf_pcksum(fin, hlen, sum)
	fr_info_t *fin;
	int hlen;
	u_int sum;
{
        union {
                u_char  c[2];
                u_short s;
        } bytes;
        int add;

        /*
         * In case we had to copy the IP & TCP header out of mbufs,
         * skip over the mbuf bits which are the header
         */
        if ((char *)ip != mtod(m, char *)) {
                hlen = (char *)sp - (char *)ip;
                while (hlen) {
                        add = MIN(hlen, m->m_len);
                        sp = (u_short *)(mtod(m, char *) + add);
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

	l4hlen =;
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
	return sum2;
}
