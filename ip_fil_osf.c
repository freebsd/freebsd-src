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
#include <net/radix.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
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
#ifdef	IPFILTER_SCAN
#include "netinet/ip_scan.h"
#endif
/*
 * It is important that these #define's only affect this .h file in here
 * because we depend on the routing stuff getting the current names.
 */
#define radix_mask ipf_radix_mask
#define radix_node ipf_radix_node
#define radix_node_head ipf_radix_node_head
#undef radix_mask
#undef radix_node
#undef radix_node_head
#include "md5.h"
#include <sys/kernel.h>
extern	int	ip_optcopy __P((struct ip *, struct ip *));
extern	int	udp_ttl;
extern	int	ipdefttl;
extern	int	ipforwarding;

extern	ipf_main_softc_t	ipfmain;

/* #undef	IPFDEBUG	*/

static	u_short	ipid = 0;
static	int	ipf_send_ip __P((fr_info_t *, mb_t *));

ipfmutex_t	ipf_rw, ipl_mutex, ipf_auth_mx, ipf_timeoutlock;
ipfmutex_t	ipf_nat_new, ipf_natio, ipf_stinsert;
ipfrwlock_t	ipf_mutex, ipf_global, ipf_frag, ipf_tru64, ipf_frcache;
ipfrwlock_t	ipf_state, ipf_nat, ipf_natfrag, ipf_authlk, ipf_ipidfrag;
ipfrwlock_t	ipf_tokens;
int		ipf_locks_done = 0;

#if defined(IPFILTER_LKM)
int
iplidentify(s)
	char *s;
{
	if (strcmp(s, "ipl") == 0)
		return 1;
	return 0;
}
#endif /* IPFILTER_LKM */


int
ipfattach(softc)
	ipf_main_softc_t *softc;
{
	int s, i;

	SPL_NET(s);
	if (softc->ipf_running > 0) {
		printf("IP Filter: already initialized\n");
		SPL_X(s);
		return EBUSY;
	}

	if (ipf_init_all(softc) < 0) {
		SPL_X(s);
#ifdef IPFDEBUG
		printf("ipf_initialise() == %d\n", i);
#endif
		return EIO;
	}

	if (softc->ipf_control_forwarding & 1)
		ipforwarding = 1;

	ipid = 0;

	SPL_X(s);

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
	int s;

	if (softc->ipf_refcnt)
		return EBUSY;

	SPL_NET(s);

	if (softc->ipf_control_forwarding & 2)
		ipforwarding = 0;

	ipf_fini_all(softc);

	SPL_X(s);

	return 0;
}


/*
 * Filter ioctl interface.
 */
int
ipfioctl(dev, cmd, data, mode)
	dev_t dev;
	int cmd;
	caddr_t data;
	int mode;
{
	int error = 0, unit = 0;
	struct proc *p;
	SPL_INT(s);

	unit = minor(dev);
	if ((IPL_LOGMAX < unit) || (unit < 0)) {
		ipfmain.ipf_interror = 130002;
		return ENXIO;
	}

	if (ipfmain.ipf_running <= 0) {
		if (unit != IPL_LOGIPF && cmd != SIOCIPFINTERROR) {
			ipfmain.ipf_interror = 130003;
			return EIO;
		}
		if (cmd != (ioctlcmd_t)SIOCIPFGETNEXT &&
		    cmd != (ioctlcmd_t)SIOCIPFGET &&
		    cmd != (ioctlcmd_t)SIOCIPFSET &&
		    cmd != (ioctlcmd_t)SIOCFRENB &&
		    cmd != (ioctlcmd_t)SIOCGETFS &&
		    cmd != (ioctlcmd_t)SIOCGETFF &&
		    cmd != (ioctlcmd_t)SIOCIPFINTERROR) {
			ipfmain.ipf_interror = 130004;
			return EIO;
		}
	}

	SPL_NET(s);

	p = task_to_proc(current_task());
	error = ipf_ioctlswitch(&ipfmain, unit, data, cmd, mode, p->p_ruid, p);
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
#ifdef	USE_INET6
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

#ifdef	USE_INET6
	hlen = (fin->fin_v == 6) ? sizeof(ip6_t) : sizeof(ip_t);
#else
	hlen = sizeof(ip_t);
#endif
#ifdef MGETHDR
	MGETHDR(m, M_DONTWAIT, MT_HEADER);
#else
	MGET(m, M_DONTWAIT, MT_HEADER);
#endif
	if (m == NULL)
		return -1;
	if (sizeof(*tcp2) + hlen > MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			FREE_MB_T(m);
			return -1;
		}
	}

	m->m_len = sizeof(*tcp2) + hlen;
	m->m_data += max_linkhdr;
	m->m_pkthdr.len = m->m_len;
	m->m_pkthdr.rcvif = (struct ifnet *)0;

	ip = mtod(m, struct ip *);
	bzero((char *)ip, hlen);
#ifdef	USE_INET6
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

#ifdef	USE_INET6
	if (fin->fin_v == 6) {
		ip6->ip6_flow = ((ip6_t *)fin->fin_ip)->ip6_flow;
		ip6->ip6_plen = htons(sizeof(struct tcphdr));
		ip6->ip6_nxt = IPPROTO_TCP;
		ip6->ip6_hlim = 0;
		ip6->ip6_src = fin->fin_dst6.in6;
		ip6->ip6_dst = fin->fin_src6.in6;
/*
		tcp2->th_sum = in6_cksum(m, IPPROTO_TCP,
					 sizeof(*ip6), sizeof(*tcp2));
*/
		return ipf_send_ip(fin, m);
	}
#endif
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
	mb_t *m;
{
	fr_info_t fnew;
	ip_t *ip, *oip;
	int ttl, hlen;

	ip = mtod(m, ip_t *);
	bzero((char *)&fnew, sizeof(fnew));
	fnew.fin_main_soft = fin->fin_main_soft;

	switch (fin->fin_p)
	{
	case IPPROTO_TCP :
		ttl = tcp_ttl;
		break;
	case IPPROTO_UDP :
		ttl = udp_ttl;
		break;
	default :
		ttl = ipdefttl;
		break;
	}

	IP_V_A(ip, fin->fin_v);

	switch (fin->fin_v)
	{
	case 4 :
		oip = fin->fin_ip;
		hlen = sizeof(*oip);
		fnew.fin_v = 4;
		fnew.fin_p = ip->ip_p;
		fnew.fin_plen = ip->ip_len + hlen;
		IP_HL_A(ip, sizeof(*oip) >> 2);
		ip->ip_tos = oip->ip_tos;
		ip->ip_id = fin->fin_ip->ip_id;
		ip->ip_len = htons(ip->ip_len);
		ip->ip_off = 0;
		ip->ip_ttl = ttl;
		ip->ip_sum = 0;
		break;
#ifdef	USE_INET6
	case 6 :
	{
		ip6_t *ip6 = (ip6_t *)ip;

# if TRU64 <= 1885
		ip6->ip6_vcf = 0x60;
# else
		ip6->ip6_vfc = 0x60;
# endif
		ip6->ip6_hlim = ttl;

		hlen = sizeof(*ip6);
		fnew.fin_p = ip6->ip6_nxt;
		fnew.fin_v = 6;
		fnew.fin_plen = ntohs(ip6->ip6_plen) + hlen;
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
	int err, hlen = 0, xtra = 0, iclen, ohlen = 0, avail, code;
	struct in_addr dst4;
	struct icmp *icmp;
	struct mbuf *m;
	i6addr_t dst6;
	void *ifp;
#ifdef USE_INET6
	ip6_t *ip6;
#endif
	ip_t *ip, *ip2;

	if ((type < 0) || (type > ICMP_MAXTYPE))
		return -1;

	code = fin->fin_icode;
#ifdef USE_INET6
	if ((code < 0) || (code > sizeof(icmptoicmp6unreach)/sizeof(int)))
		return -1;
#endif

	if (ipf_checkl4sum(fin) == -1)
		return -1;
#ifdef MGETHDR
	MGETHDR(m, M_DONTWAIT, MT_HEADER);
#else
	MGET(m, M_DONTWAIT, MT_HEADER);
#endif
	if (m == NULL)
		return -1;
	avail = MHLEN;

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
		iclen = hlen + offsetof(struct icmp, icmp_ip) + ohlen;
		if (fin->fin_hlen < fin->fin_plen)
			xtra = MIN(fin->fin_dlen, 8);
		else
			xtra = 0;
	}

#ifdef	USE_INET6
	else if (fin->fin_v == 6) {
		hlen = sizeof(ip6_t);
		ohlen = sizeof(ip6_t);
		iclen = hlen + offsetof(struct icmp, icmp_ip) + ohlen;
		type = icmptoicmp6types[type];
		if (type == ICMP6_DST_UNREACH)
			code = icmptoicmp6unreach[code];

		if (hlen + sizeof(*icmp) + max_linkhdr +
		    fin->fin_plen > avail) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
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
	m->m_data += max_linkhdr;
	m->m_pkthdr.rcvif = (struct ifnet *)0;
	if (xtra > avail)
		xtra = avail;
	iclen += xtra;
	m->m_pkthdr.len = iclen;

	if (avail < 0) {
		FREE_MB_T(m);
		return -1;
	}
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
		ip6->ip6_flow = ((ip6_t *)fin->fin_ip)->ip6_flow;
		ip6->ip6_plen = htons(iclen - hlen);
		ip6->ip6_nxt = IPPROTO_ICMPV6;
		ip6->ip6_hlim = 0;
		ip6->ip6_src = dst6.in6;
		ip6->ip6_dst = fin->fin_src6.in6;
		if (xtra > 0)
			bcopy((char *)fin->fin_ip + ohlen,
			      (char *)&icmp->icmp_ip + ohlen, xtra);
/*
		icmp->icmp_cksum = in6_cksum(m, IPPROTO_ICMPV6,
					     sizeof(*ip6), iclen - hlen);
*/
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
		ip->ip_len = iclen;
		ip->ip_p = IPPROTO_ICMP;
	}
	err = ipf_send_ip(fin, m);
	return err;
}


void iplinit __P((void));

void iplinit()
{
	if (ipfattach(&ipfmain) != 0)
		printf("IP Filter failed to attach\n");
	ip_init();
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
	ipf_main_softc_t *softc;
	u_short ip_off, ip_len;
	struct route iproute;
	frdest_t node;
	frentry_t *fr;

	softc = fin->fin_main_soft;

#ifdef	M_WRITABLE
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
		if ((m0 = m_dup(m, M_DONTWAIT)) != 0) {
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

	hlen = fin->fin_hlen;
	ip = mtod(m0, struct ip *);

#if 0 /* ifdef	USE_INET6 */
	if (fin->fin_v == 6) {
                dst6->sin6_family = AF_INET6;

                fr = fin->fin_fr;
                if (fdp != NULL)
                        ifp = fdp->fd_ifp;
                else {
                        ifp = fin->fin_ifp;
                        dst->sin6_addr = fin->fin_daddr6;
                }

                ip6tx.tx_mbuf = *mpp;
                ip6tx.tx_ip6 = (ip6_t *)ip;
                ip6tx.tx_ro = ro;
                ip6tx.tx_if6 = NULL;
                ip6tx.tx_nexthop = dst6;
                ip6tx.tx_imo6 = NULL;
                ip6tx.tx_pmtudisc = 0;
                ip6tx.tx_dontroute = 0;
                ip6tx.tx_rawoutput = 0;
                ip6tx.tx_mtu = ifp->if_mtu;
                ip6tx.tx_opt = NULL;

                /*
                 * currently "to <if>" and "to <if>:ip#" are not supported
                 * for IPv6
                 */
                return ip6_output(&ip6tx);
	}
# endif
	/*
	 * Route packet.
	 */
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
		dst->sin_addr = fdp->fd_ip;

	dst->sin_len = sizeof(*dst);
	rtalloc(ro);
	if (!ifp) {
		if (!fr || !(fr->fr_flags & FR_FASTROUTE)) {
			error = -2;
			goto bad;
		}
		if (ro->ro_rt == 0 || (ifp = ro->ro_rt->rt_ifp) == 0) {
			i6addr_t i6;

			i6.in4 = ip->ip_dst;

			if (in_localaddr(&i6.in6, ro->ro_rt))
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
	ip_len = ntohs(ip->ip_len);
	if (ip_len <= ifp->if_mtu) {
		if (!ip->ip_sum)
			ip->ip_sum = in_cksum(m, hlen);
#if TRU64 >= 1885
		error = (*ifp->if_output)(ifp, m, (struct sockaddr *)dst,
					  ro->ro_rt, NULL);
#else
		error = (*ifp->if_output)(ifp, m, (struct sockaddr *)dst,
					  ro->ro_rt);
#endif
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
	for (off = hlen + len; off < ip_len; off += len) {
#ifdef	MGETHDR
		MGETHDR(m, M_DONTWAIT, MT_HEADER);
#else
		MGET(m, M_DONTWAIT, MT_HEADER);
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
		if (off + len >= ip_len)
			len = ip_len - off;
		else
			mhip->ip_off |= IP_MF;
		mhip->ip_len = htons((u_short)(len + mhlen));
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
		*mnext = m;
		mnext = &m->m_act;
	}
	/*
	 * Update first fragment by trimming what's been copied out
	 * and updating header, then send each fragment (in order).
	 */
	m_adj(m0, hlen + firstlen - ip_len);
	ip->ip_len = htons((u_short)(hlen + firstlen));
	ip->ip_off = htons((u_short)IP_MF);
	ip->ip_sum = 0;
	ip->ip_sum = in_cksum(m0, hlen);
sendorfree:
	for (m = m0; m; m = m0) {
		m0 = m->m_act;
		m->m_act = 0;
		if (error == 0)
#if TRU64 >= 1885
			error = (*ifp->if_output)(ifp, m,
			    (struct sockaddr *)dst, ro->ro_rt, NULL);
#else
			error = (*ifp->if_output)(ifp, m,
			    (struct sockaddr *)dst, ro->ro_rt);
#endif
		else
			FREE_MB_T(m);
	}
    }
done:
	if (!error)
		softc->ipf_frouteok[0]++;
	else
		softc->ipf_frouteok[1]++;

	if (ro->ro_rt) {
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
	struct sockaddr *sock, *mask;
	struct sockaddr_in *sin;
	struct ifaddr *ifa;
	struct in_addr in;
	struct ifnet *ifp;

	if ((ifptr == NULL) || (ifptr == (void *)-1))
		return -1;

	ifp = ifptr;

	if (v == 4)
		inp->in4.s_addr = 0;
#ifdef USE_INET6
	else if (v == 6)
		bzero((char *)inp, sizeof(*inp));
#endif

	ifa = ifp->if_addrlist;
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
		ifa = ifa->ifa_next;
		if (ifa)
			sock = ifa->ifa_addr;
	}
	if (ifa == NULL || sock == NULL)
		return -1;

	mask = ifa->ifa_netmask;
	if (atype == FRI_BROADCAST)
		sock = ifa->ifa_broadaddr;
	else if (atype == FRI_PEERADDR)
		sock = ifa->ifa_dstaddr;

#ifdef USE_INET6
	if (v == 6)
		return ipf_ifpfillv6addr(atype, (struct sockaddr_in6 *)sock,
					 (struct sockaddr_in6 *)mask,
					 inp, inpmask);
#endif
	return ipf_ifpfillv4addr(atype, (struct sockaddr_in *)sock,
				 (struct sockaddr_in *)mask,
				 &inp->in4, &inpmask->in4);
}


u_32_t
ipf_newisn(fin)
	fr_info_t *fin;
{
	static int iss_seq_off = 0;
	ipf_main_softc_t *softc = fin->fin_main_soft;
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

	MD5Update(&ctx, softc->ipf_iss_secret, sizeof(softc->ipf_iss_secret));

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
/* Function:    ipf_slowtimer                                               */
/* Returns:     Nil                                                         */
/* Parameters:  Nil                                                         */
/*                                                                          */
/* Slowly expire held state for fragments.  Timeouts are set * in           */
/* expectation of this being called twice per second.                       */
/* ------------------------------------------------------------------------ */
void
ipf_timer_func __P((void *ptr))
{
	ipf_main_softc_t *softc = ptr;

        READ_ENTER(&softc->ipf_global);

	if (softc->ipf_running == 1)
		ipf_slowtimer(softc);

	RWLOCK_EXIT(&softc->ipf_global);
}


/* ------------------------------------------------------------------------ */
/* Function:    ipf_nextipid                                                */
/* Returns:     int - 0 == success, -1 == error (packet should be droppped) */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* Returns the next IPv4 ID to use for this packet.                         */
/* ------------------------------------------------------------------------ */
INLINE u_short
ipf_nextipid(fin)
	fr_info_t *fin;
{
	u_short id;

	MUTEX_ENTER(&ipf_rw);
	id = ipid++;
	MUTEX_EXIT(&ipf_rw);

	return id;
}


INLINE int
ipf_checkv4sum(fin)
	fr_info_t *fin;
{
	int manual, pflag, cflags, active;
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

	switch (fin->fin_p)
	{
	case IPPROTO_UDP :
	case IPPROTO_TCP :
		active = M_PROTOCOL_SUM | M_CHECKSUM | M_NOCHECKSUM |
			 M_IPPREPROCESS;
		pflag = M_PROTOCOL_SUM;
		manual = 0;
		break;
	default :
		active = 0;
		pflag = 0;
		manual = 1;
		break;
	}

	cflags = m->m_flags & active;

	if (pflag != 0) {
		if (cflags == pflag) {
			fin->fin_cksum = FI_CK_SUMOK;
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
	int out = fin->fin_out, dpoff, ipoff;
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
#endif
		} else
		{
			m = m_pullup(m, len);
		}
		if (n = NULL)
			n->m_next = n;
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
	int error;

	if (fin->fin_out == 0) {
		struct ifqueue *ifq;

		ifq = &ipintrq;

		if (IF_QFULL(ifq)) {
			IF_DROP(ifq);
			FREE_MB_T(m);
			error = ENOBUFS;
		} else {
			IF_ENQUEUE(ifq, m);
			error = 0;
		}
	} else {
		error = ip_output(m, NULL, NULL, IP_FORWARDING, NULL);
	}

	return error;
}


/*
 * Copy a packet header mbuf chain into a completely new chain, including
 * copying any mbuf clusters.  Use this instead of m_copypacket() when
 * you need a writable copy of an mbuf chain.
 */
struct mbuf *
m_dup(struct mbuf *m, int how)
{
	struct mbuf **p, *top = NULL;
	int remain, moff, nsize;

	if (m == NULL)
		return (NULL);

	/* While there's more data, get a new mbuf, tack it on, and fill it */
	remain = m->m_pkthdr.len;
	moff = 0;
	p = &top;
	while (remain > 0 || top == NULL) {     /* allow m->m_pkthdr.len == 0 */
		struct mbuf *n;

		/* Get the next new mbuf */
		if (remain >= MINCLSIZE) {
			n = m_getcl(how, m->m_type, 0);
			nsize = MCLBYTES;
		} else {
			n = m_get(how, m->m_type);
			nsize = MLEN;
		}
		if (n == NULL)
			goto nospace;

		if (top == NULL) {              /* First one, must be PKTHDR */
			if (!m_dup_pkthdr(n, m, how)) {
				m_free(n);
				goto nospace;
			}
			if ((n->m_flags & M_EXT) == 0)
				nsize = MHLEN;
		}
		n->m_len = 0;

		/* Link it into the new chain */
		*p = n;
		p = &n->m_next;

		/* Copy data from original mbuf(s) into new mbuf */
		while (n->m_len < nsize && m != NULL) {
			int chunk = min(nsize - n->m_len, m->m_len - moff);

			bcopy(m->m_data + moff, n->m_data + n->m_len, chunk);
			moff += chunk;
			n->m_len += chunk;
			remain -= chunk;
			if (moff == m->m_len) {
				m = m->m_next;
				moff = 0;
			}
		}

		/* Check correct total mbuf length */
		KASSERT((remain > 0 && m != NULL) || (remain == 0 && m == NULL),
			("%s: bogus m_pkthdr.len", __func__));
	}
	return (top);

nospace:
	m_freem(top);
	return (NULL);
}


/*
 * Duplicate "from"'s mbuf pkthdr in "to".
 * "from" must have M_PKTHDR set, and "to" must be empty.
 * In particular, this does a deep copy of the packet tags.
 */
int
m_dup_pkthdr(struct mbuf *to, struct mbuf *from, int how)
{
	to->m_flags = (from->m_flags & M_COPYFLAGS) | (to->m_flags & M_EXT);
	if ((to->m_flags & M_EXT) == 0)
		to->m_data = to->m_pktdat;
	to->m_pkthdr = from->m_pkthdr;
	return (0);
}
