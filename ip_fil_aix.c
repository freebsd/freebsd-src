/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#define	__FULL_PROTO
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
#include <sys/device.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_var.h>
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
#ifdef INET
extern	int	ip_optcopy __P((struct ip *, struct ip *));
#endif

/*
ipstat
inbound_fw
nethsque
rw_read_locked
ip_output
netisr_dispatch
ipforwarding
ip_output_post_fw
ipintr_noqueue_post_fw
max_linkhdr
in_localaddr
ip_optcopy
outbound_fw
*/


static	u_short	ipid = 0;
static	int	(*fr_savep) __P((ip_t *, int, void *, int, struct mbuf **));
static	int	fr_send_ip __P((fr_info_t *, mb_t *));
#ifdef KMUTEX_T
extern  ipfmutex_t	ipf_rw;
extern	ipfrwlock_t	ipf_mutex;
#endif
#ifdef USE_INET6
static int ipfr_fastroute6 __P((struct mbuf *, struct mbuf **,
				fr_info_t *, frdest_t *));
#endif

#include <sys/conf.h>
#include <sys/device.h>

void ipf_check_inbound __P((struct ifnet *, struct mbuf *, inbound_fw_args_t *));
int ipf_check_outbound __P((struct ifnet *, struct mbuf *, outbound_fw_args_t *));

int ipfopen __P((dev_t, u_long, chan_t, int));
int ipfclose __P((dev_t, chan_t));
int ipfread __P((dev_t, struct uio *, chan_t, int));
int ipfwrite __P((dev_t, struct uio *, chan_t, int));
int ipfioctl __P((dev_t, int, caddr_t, int));
int ipfconfig __P((dev_t, int, struct uio *));

ipfmutex_t	ipl_mutex, ipf_auth_mx, ipf_rw, ipf_stinsert;
ipfmutex_t	ipf_nat_new, ipf_natio, ipf_timeoutlock;
ipfrwlock_t	ipf_mutex, ipf_global, ipf_ipidfrag, ipf_tokens;
ipfrwlock_t	ipf_frag, ipf_state, ipf_nat, ipf_natfrag, ipf_authlk;
int		ipf_locks_done = 0;

const struct devsw ipfdevsw  = {
        ipfopen,	/* d_open	entry point for open routine */
        ipfclose,	/* d_close	entry point for close routine */
        ipfread,	/* d_read	entry point for read routine */
        ipfwrite,	/* d_write	entry point for write routine */
        ipfioctl,	/* d_ioctl	entry point for ioctl routine */
        nodev,		/* d_strategy	entry point for strategy routine */
        NULL, 		/* d_ttys	pointer to tty device structure */
        nodev,		/* d_select	entry point for select routine */
        ipfconfig,	/* d_config	entry point for config routine */
        nodev,		/* d_print	entry point for print routine */
        nodev,		/* d_dump	entry point for dump routine */
        nodev,		/* d_mpx	entry point for mpx routine */
        nodev,		/* d_revoke	entry point for revoke routine */
        NULL,		/* d_dsdptr     pointer to device specific data */
        0,		/* d_selptr	ptr to outstanding select cntl blks */
#ifdef _IA64
	DEV_MPSAFE | DEV_64BIT
#else
        0		/* d_opts	internal device switch control field */
#endif

};


int
ipfconfig(devno, cmd, uiop)
	dev_t devno;
	int cmd;
	struct uio *uiop;
{
	int error = EINVAL;

	printf("ipfconfig(%u,%x,%p)\n", devno,cmd,uiop);

	switch (cmd)
	{
	case CFG_INIT :
		error = devswadd(devno, &ipfdevsw);
		if (error == 0) {
			ipf_load_all();
			ipf_create_all(&ipfmain);
			error = ipfattach(&ipfmain);
		}
		break;
	case CFG_TERM :
		error = devswdel(devno);
		break;
	case CFG_QVPD :
		error = 0;
		break;
	default :
		return EINVAL;
	}

	return 0;
}


int
ipfattach(softc)
	ipf_main_softc_t *softc;
{
	int s;

	SPL_NET(s);
	if ((ipf_running > 0) || (inbound_fw == ipf_check_inbound)) {
		printf("IP Filter: already initialized\n");
		SPL_X(s);
		return EBUSY;
	}

	inbound_fw = ipf_check_inbound;
	outbound_fw = ipf_check_outbound;

	if (fr_control_forwarding & 1)
		ipforwarding = 1;

	ipid = 0;

	ipf_init_all(softc);

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

	SPL_NET(s);

	inbound_fw = NULL;
	outbound_fw = NULL;
	(void) frflush(IPL_LOGIPF, 0, FR_INQUE|FR_OUTQUE|FR_INACTIVE);
	(void) frflush(IPL_LOGIPF, 0, FR_INQUE|FR_OUTQUE);

	if (fr_control_forwarding & 2)
		ipforwarding = 0;

	ipf_fini_all(softc);

	SPL_X(s);
	return 0;
}


/*
 * explicit inbound hook to call ipf_check from
 */
void
ipf_check_inbound(ifp, m, args)
	struct ifnet *ifp;
	struct mbuf *m;
	inbound_fw_args_t *args;
{
	ip_t *ip;

	if (ipf_check_mbuf(&m) == -1) {
		if (m != NULL) {
			FREE_MB_T(m);
		}
		return;
	}

	ip = mtod(m, ip_t *);

	switch (ipf_check(ip, ip->ip_hl << 2, ifp, 0, &m))
	{
	case 0 :
		ipintr_noqueue_post_fw(ifp, m, args);
		break;
	default :
		if (m != NULL) {
			FREE_MB_T(m);
		}
		break;
	}

	return;
}


/*
 * explicit outbound hook to call ipf_check from
 */
int
ipf_check_outbound(ifp, m, args)
	struct ifnet *ifp;
	struct mbuf *m;
	outbound_fw_args_t *args;
{
	ip_t *ip;

	if (ipf_check_mbuf(&m) == -1) {
		if (m != NULL) {
			FREE_MB_T(m);
		}
		return 1;	/* FIREWALL_NOTOK */
	}

	ip = mtod(m, ip_t *);

	switch (ipf_check(ip, ip->ip_hl << 2, ifp, 1, &m))
	{
	case 0 :
		ip_output_post_fw(ifp, m, args);
		return 0;	/* FIREWALL_OK */
	default :
		break;
	}

	if (m != NULL) {
		FREE_MB_T(m);
	}
	return 1;		/* FIREWALL_NOTOK */
}

int
ipf_check_mbuf(mp)
	struct mbuf **mp;
{
	struct mbuf *m, *m0;
	int i, hlen;
	ip_t *ip;

	m = *mp;

	if ((m->m_len < sizeof (struct ip)) &&
	    (m = m_pullup(m, sizeof (struct ip))) == 0) {
		*mp = m;
		ipstat.ips_toosmall++;
		return -1;
	}
	ip = mtod(m, struct ip *);

	switch (ip->ip_v)
	{
	case 4 :
		hlen = ip->ip_hl << 2;
		if (hlen < sizeof(struct ip)) {	/* minimum header length */
			ipstat.ips_badhlen++;
			return -1;
		}
		if ((hlen > sizeof(struct ip)) && (hlen > m->m_len)) {
			if ((m = m_pullup(m, hlen)) == 0) {
				*mp = m;
				ipstat.ips_badhlen++;
				return -2;
			}
			ip = mtod(m, struct ip *);
		}
		if (ip->ip_sum = in_cksum(m, hlen)) {
			ipstat.ips_badsum++;
			return -1;
		}

		/*
		 * Convert fields to host representation.
		 * XXX - no need for NTOHS on big endian (sparc)
		 */
		NTOHS(ip->ip_len);
		if (ip->ip_len < hlen) {
			ipstat.ips_badlen++;
			return -1;
		}
		NTOHS(ip->ip_id);
		NTOHS(ip->ip_off);

		/*
		 * Check that the amount of data in the buffers
		 * is as at least much as the IP header would have us expect.
		 * Trim mbufs if longer than we expect.
		 * Drop packet if shorter than we expect.
		 */
		i = -(u_short)ip->ip_len;
		m0 = m;
		for (;;) {
			i += m->m_len;
			if (m->m_next == 0)
				break;
			m = m->m_next;
		}
		if (i != 0) {
			if (i < 0) {
				ipstat.ips_tooshort++;
				m = m0;
				return -1;
			}
			if (i <= m->m_len)
				m->m_len -= i;
			else
				m_adj(m0, -i);
		}
		break;
#ifdef USE_INET6
	case 6 :
		break;
#endif
	default :
		return -1;
	}
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

	error = ipf_ioctlswitch(unit, data, cmd, mode, curproc->p_uid, curproc);
	if (error != -1) {
		SPL_X(s);
		return error;
	}

	SPL_X(s);
	return error;
}


/*
 * routines below for saving IP headers to buffer
 */
int
ipfopen(dev_t dev, u_long flags, chan_t chan, int ext)
{
	u_int unit = GET_MINOR(dev);
	int error;

	if (IPL_LOGMAX < unit)
		error = ENXIO;
	else {
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


int
ipfclose(dev_t dev, chan_t chan)
{
	u_int	unit = GET_MINOR(dev);

	if (IPL_LOGMAX < unit)
		unit = ENXIO;
	else
		unit = 0;
	return unit;
}

/*
 * ipfread/ipllog
 * both of these must operate with at least splnet() lest they be
 * called during packet processing and cause an inconsistancy to appear in
 * the filter lists.
 */
int
ipfread(dev_t dev, struct uio *uio, chan_t chan, int ext)
{

	if (ipf_running < 1) {
		ipfmain.ipf_interror = 130006;
		return EIO;
	}

	if (GET_MINOR(dev) == IPL_LOGSYNC)
		return ipfsync_read(uio);

#ifdef IPFILTER_LOG
	return ipflog_read(GET_MINOR(dev), uio);
#else
	ipfmain.ipf_interror = 130007;
	return ENXIO;
#endif
}


/*
 * ipfwrite
 * both of these must operate with at least splnet() lest they be
 * called during packet processing and cause an inconsistancy to appear in
 * the filter lists.
 */
int
ipfwrite(dev_t dev, struct uio *uio, chan_t chan, int ext)
{

	if (GET_MINOR(dev) == IPL_LOGSYNC)
		return ipfsync_write(uio);
	ipfmain.ipf_interror = 130009;
	return ENXIO;
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
	MGETHDR(m, M_DONTWAIT, MT_HEADER);
#else
	MGET(m, M_DONTWAIT, MT_HEADER);
#endif
	if (m == NULL)
		return -1;
	if (sizeof(*tcp2) + hlen > MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if (m == NULL)
			return -1;
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
#ifdef USE_INET6
	ip6 = (ip6_t *)ip;
#endif
	bzero((char *)ip, sizeof(*tcp2) + hlen);
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
	tcp2->th_x2 = 0;
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
		ip6->ip6_src = fin->fin_dst6;
		ip6->ip6_dst = fin->fin_src6;
		tcp2->th_sum = in6_cksum(m, IPPROTO_TCP,
					 sizeof(*ip6), sizeof(*tcp2));
		return ipf_send_ip(fin, m);
	}
#endif
#ifdef INET
	ip->ip_p = IPPROTO_TCP;
	ip->ip_len = htons(sizeof(struct tcphdr));
	ip->ip_src.s_addr = fin->fin_daddr;
	ip->ip_dst.s_addr = fin->fin_saddr;
	tcp2->th_sum = in_cksum(m, hlen + sizeof(*tcp2));
	ip->ip_len = hlen + sizeof(*tcp2);
	return ipf_send_ip(fin, m);
#else
	return 0;
#endif
}


static int
ipf_send_ip(fin, m)
	fr_info_t *fin;
	mb_t *m;
{
	fr_info_t fnew;
#ifdef INET
	ip_t *oip;
#endif
	ip_t *ip;
	int hlen;

	ip = mtod(m, ip_t *);
	bzero((char *)&fnew, sizeof(fnew));
	fnew.fin_main_soft = fin->fin_main_soft;

	IP_V_A(ip, fin->fin_v);
	switch (fin->fin_v)
	{
#ifdef INET
	case 4 :
		oip = fin->fin_ip;
		hlen = sizeof(*oip);
		fnew.fin_v = 4;
		fnew.fin_p = ip->ip_p;
		fnew.fin_plen = ntohs(ip->ip_len);
		IP_HL_A(ip, sizeof(*oip) >> 2);
		ip->ip_tos = oip->ip_tos;
		ip->ip_id = ipf_nextipid(fin);
		ip->ip_off = 0;
		ip->ip_ttl = IPDEFTTL;
		ip->ip_sum = 0;
		break;
#endif
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
	void *ifp;
#ifdef USE_INET6
	ip6_t *ip6;
	struct in6_addr dst6;
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

	xtra = 0;
	hlen = 0;
	ohlen = 0;
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
			if (fr_ifpaddr(softc, 4, FRI_NORMAL, ifp,
				       &dst4, NULL) == -1) {
				FREE_MB_T(m);
				return -1;
			}
		} else
			dst4.s_addr = fin->fin_daddr;

		hlen = sizeof(ip_t);
		ohlen = fin->fin_hlen;
		if (fin->fin_hlen < fin->fin_plen)
			xtra = MIN(fin->fin_dlen, 8);
		else
			xtra = 0;
	}

#ifdef USE_INET6
	else if (fin->fin_v == 6) {
		hlen = sizeof(ip6_t);
		ohlen = sizeof(ip6_t);
		type = icmptoicmp6types[type];
		if (type == ICMP6_DST_UNREACH)
			code = icmptoicmp6unreach[code];

		if (hlen + sizeof(*icmp) + max_linkhdr +
		    fin->fin_plen > avail) {
			MCLGET(m, M_DONTWAIT);
			if (m == NULL)
				return -1;
			if ((m->m_flags & M_EXT) == 0) {
				FREE_MB_T(m);
				return -1;
			}
			avail = MCLBYTES;
		}
		xtra = MIN(fin->fin_plen,
			   avail - hlen - sizeof(*icmp) - max_linkhdr);
		if (dst == 0) {
			if (fr_ifpaddr(softc, 6, FRI_NORMAL, ifp,
				       (struct in_addr *)&dst6, NULL) == -1) {
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

#if defined(M_CSUM_IPv4)
	/*
	 * Clear any in-bound checksum flags for this packet.
	 */
	m->m_pkthdr.csuminfo = 0;
#endif /* __NetBSD__ && M_CSUM_IPv4 */

#ifdef USE_INET6
	ip6 = (ip6_t *)ip;
	if (fin->fin_v == 6) {
		ip6->ip6_flow = ((ip6_t *)fin->fin_ip)->ip6_flow;
		ip6->ip6_plen = htons(iclen - hlen);
		ip6->ip6_nxt = IPPROTO_ICMPV6;
		ip6->ip6_hlim = 0;
		ip6->ip6_src = dst6;
		ip6->ip6_dst = fin->fin_src6;
		if (xtra > 0)
			bcopy((char *)fin->fin_ip + ohlen,
			      (char *)&icmp->icmp_ip + ohlen, xtra);
		icmp->icmp_cksum = in6_cksum(m, IPPROTO_ICMPV6,
					     sizeof(*ip6), iclen - hlen);
	} else
#endif
	{
		ip2->ip_len = htons(ip2->ip_len);
		ip2->ip_off = htons(ip2->ip_off);
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

	if (fin->fin_v == 6) {
#ifdef USE_INET6
		error = ipfr_fastroute6(m0, mpp, fin, fdp);
#else
		error = EPROTONOSUPPORT;
#endif
		if ((error != 0) && (*mpp != NULL)) {
			FREE_MB_T(*mpp);
		}
		return error;
	}
#ifndef INET
	FREE_MB_T(*mpp);
	return EPROTONOSUPPORT;
#else

	hlen = fin->fin_hlen;
	ip = mtod(m0, struct ip *);

# if defined(M_CSUM_IPv4)
	/*
	 * Clear any in-bound checksum flags for this packet.
	 */
	m0->m_pkthdr.csuminfo = 0;
# endif /* __NetBSD__ && M_CSUM_IPv4 */

	/*
	 * Route packet.
	 */
	ro = &iproute;
	bzero((caddr_t)ro, sizeof (*ro));
	dst = (struct sockaddr_in *)&ro->ro_dst;
	dst->sin_family = AF_INET;
	dst->sin_addr = ip->ip_dst;
	ifp = NULL;

	fr = fin->fin_fr;
	if ((fr == NULL) || !(fr->fr_flags & FR_FASTROUTE))) {
		error = -2;
		goto bad;
	}
	if ((fr != NULL) && !(fr->fr_flags & FR_KEEPSTATE) && (fdp != NULL) &&
	    (fdp->fd_type == FRD_DSTLIST)) {
		if (ipf_dstlist_select_node(fin, fdp->fd_ptr, NULL, &node) == 0)
			fdp = &node;
	}
	if (fdp != NULL)
		ifp = fdp->fd_ptr;
	else
		ifp = fin->fin_ifp;

	if (ifp == NULL) {
		error = -2;
		goto bad;
	}

	if ((fdp != NULL) && (fdp->fd_ip.s_addr != 0))
		dst->sin_addr = fdp->fd_ip;

	dst->sin_len = sizeof(*dst);
	rtalloc(ro);

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
	if (ip->ip_len <= ifp->if_mtu) {
		int i = 0;

		if (m->m_flags & M_EXT)
			i = 1;

		ip->ip_len = htons(ip->ip_len);
		ip->ip_off = htons(ip->ip_off);
# if defined(M_CSUM_IPv4)
#  if (__NetBSD_Version__ >= 105009999)
		if (ifp->if_csum_flags_tx & M_CSUM_IPv4)
			m->m_pkthdr.csuminfo |= M_CSUM_IPv4;
#  else
		if (ifp->if_capabilities & IFCAP_CSUM_IPv4)
			m->m_pkthdr.csuminfo |= M_CSUM_IPv4;
#  endif /* (__NetBSD_Version__ >= 105009999) */
		else if (ip->ip_sum == 0)
			ip->ip_sum = in_cksum(m, hlen);
# else
		if (!ip->ip_sum)
			ip->ip_sum = in_cksum(m, hlen);
# endif /* M_CSUM_IPv4 */
		error = (*ifp->if_output)(ifp, m, (struct sockaddr *)dst,
					  ro->ro_rt);
		if (i) {
			ip->ip_len = ntohs(ip->ip_len);
			ip->ip_off = ntohs(ip->ip_off);
		}
		goto done;
	}

	/*
	 * Too large for interface; fragment if possible.
	 * Must be able to put at least 8 bytes per fragment.
	 */
	ip_off = ip->ip_off;
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
	for (off = hlen + len; off < ip->ip_len; off += len) {
# ifdef MGETHDR
		MGETHDR(m, M_DONTWAIT, MT_HEADER);
# else
		MGET(m, M_DONTWAIT, MT_HEADER);
# endif
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
		if (off + len >= ip->ip_len)
			len = ip->ip_len - off;
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
			    (struct sockaddr *)dst, ro->ro_rt);
		else
			FREE_MB_T(m);
	}
    }
done:
	if (!error)
		ipf_frouteok[0]++;
	else
		ipf_frouteok[1]++;

	if (ro->ro_rt) {
		RTFREE(ro->ro_rt);
	}
	return error;
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
#endif /* INET */
}


#if defined(USE_INET6)
/*
 * This is the IPv6 specific fastroute code.  It doesn't clean up the mbuf's
 * or ensure that it is an IPv6 packet that is being forwarded, those are
 * expected to be done by the called (ipf_fastroute).
 */
static int
ipf_fastroute6(m0, mpp, fin, fdp)
	struct mbuf *m0, **mpp;
	fr_info_t *fin;
	frdest_t *fdp;
{
	struct route_in6 ip6route;
	struct sockaddr_in6 *dst6;
	struct route_in6 *ro;
	struct rtentry *rt;
	struct ifnet *ifp;
	frentry_t *fr;
	u_long mtu;
	int error;

	ro = &ip6route;
	fr = fin->fin_fr;
	bzero((caddr_t)ro, sizeof(*ro));
	dst6 = (struct sockaddr_in6 *)&ro->ro_dst;
	dst6->sin6_family = AF_INET6;
	dst6->sin6_len = sizeof(struct sockaddr_in6);
	dst6->sin6_addr = fin->fin_fi.fi_dst.in6;

	if (fdp != NULL)
		ifp = fdp->fd_ifp;
	else
		ifp = fin->fin_ifp;

	if (fdp != NULL) {
		if (IP6_NOTZERO(&fdp->fd_ip6))
			dst6->sin6_addr = fdp->fd_ip6.in6;
	}

	rtalloc((struct route *)ro);

	if ((ifp == NULL) && (ro->ro_rt != NULL))
		ifp = ro->ro_rt->rt_ifp;

	if ((ro->ro_rt == NULL) || (ifp == NULL)) {
		error = EHOSTUNREACH;
		goto bad;
	}

	rt = fdp ? NULL : ro->ro_rt;

	/* KAME */
	if (IN6_IS_ADDR_LINKLOCAL(&dst6->sin6_addr))
		dst6->sin6_addr.s6_addr16[1] = htons(ifp->if_index);

	{
		if (ro->ro_rt->rt_flags & RTF_GATEWAY)
			dst6 = (struct sockaddr_in6 *)ro->ro_rt->rt_gateway;
		ro->ro_rt->rt_use++;

		error = ip6_getpmtu(ro, ro, ifp, &finaldst, &mtu, &frag);
		if ((error == 0) && (m0->m_pkthdr.len <= mtu)) {
			error = nd6_output(ifp, ifp, *mpp, dst6, rt);
		} else {
			error = EMSGSIZE;
		}
	}
bad:
	if (ro->ro_rt != NULL) {
		RTFREE(ro->ro_rt);
	}
	return error;
}
#endif


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
	struct in_addr *inp, *inpmask;
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

	ifp = ifptr;
	mask = NULL;

	if (v == 4)
		inp->s_addr = 0;
#ifdef USE_INET6
	else if (v == 6)
		bzero((char *)inp, sizeof(struct in6_addr));
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
		if (ifa != NULL)
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
				 (struct sockaddr_in *)mask, inp, inpmask);
}


u_32_t
ipf_newisn(fin)
	fr_info_t *fin;
{
	u_32_t newiss;
#if 0
	static int iss_seq_off = 0;
	u_char hash[16];
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

	memcpy(&newiss, hash, sizeof(newiss));

	/*
	 * Now increment our "timer", and add it in to
	 * the computed value.
	 *
	 * XXX Use `addin'?
	 * XXX TCP_ISSINCR too large to use?
	 */
	iss_seq_off += 0x00010000;
	newiss += iss_seq_off;
#endif
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


INLINE int
ipf_checkv4sum(fin)
	fr_info_t *fin;
{
#ifdef M_CSUM_TCP_UDP_BAD
	int manual, pflag, cflags, active;
	mb_t *m;

	if ((fin->fin_flx & FI_NOCKSUM) != 0)
		return 0;

	if ((fin->fin_flx & FI_SHORT) != 0)
		return 1;

	if (fin->fin_cksum != FI_CK_NEEDED)
		return (fin->fin_cksum > FI_CK_NEEDED) ? 0 : -1;

	manual = 0;
	m = fin->fin_m;
	if (m == NULL) {
		manual = 1;
		goto skipauto;
	}

	switch (fin->fin_p)
	{
	case IPPROTO_UDP :
		pflag = M_CSUM_UDPv4;
		break;
	case IPPROTO_TCP :
		pflag = M_CSUM_TCPv4;
		break;
	default :
		pflag = 0;
		manual = 1;
		break;
	}

	active = ((struct ifnet *)fin->fin_ifp)->if_csum_flags_rx & pflag;
	active |= M_CSUM_TCP_UDP_BAD | M_CSUM_DATA;
	cflags = m->m_pkthdr.csum_flags & active;

	if (pflag != 0) {
		if (cflags == (pflag | M_CSUM_TCP_UDP_BAD)) {
			fin->fin_flx |= FI_BAD;
			fin->fin_cksum = FI_CK_BAD;
		} else if (cflags == (pflag | M_CSUM_DATA)) {
			if ((m->m_pkthdr.csum_data ^ 0xffff) != 0) {
				fin->fin_flx |= FI_BAD;
				fin->fin_cksum = FI_CK_BAD;
			} else {
				fin->fin_cksum = FI_CK_SUMOK;
			}
		} else if (cflags == pflag) {
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
# ifdef M_CSUM_TCP_UDP_BAD
	int manual, pflag, cflags, active;
	mb_t *m;

	if ((fin->fin_flx & FI_NOCKSUM) != 0)
		return 0;

	if ((fin->fin_flx & FI_SHORT) != 0)
		return 1;

	if (fin->fin_cksum != FI_CK_NEEDED)
		return (fin->fin_cksum > FI_CK_NEEDED) ? 0 : -1;


	manual = 0;
	m = fin->fin_m;

	switch (fin->fin_p)
	{
	case IPPROTO_UDP :
		pflag = M_CSUM_UDPv6;
		break;
	case IPPROTO_TCP :
		pflag = M_CSUM_TCPv6;
		break;
	default :
		pflag = 0;
		manual = 1;
		break;
	}

	active = ((struct ifnet *)fin->fin_ifp)->if_csum_flags_rx & pflag;
	active |= M_CSUM_TCP_UDP_BAD | M_CSUM_DATA;
	cflags = m->m_pkthdr.csum_flags & active;

	if (pflag != 0) {
		if (cflags == (pflag | M_CSUM_TCP_UDP_BAD)) {
			fin->fin_flx |= FI_BAD;
			fin->fin_cksum = FI_CK_BAD;
		} else if (cflags == (pflag | M_CSUM_DATA)) {
			if ((m->m_pkthdr.csum_data ^ 0xffff) != 0) {
				fin->fin_flx |= FI_BAD;
				fin->fin_cksum = FI_CK_BAD;
			} else {
				fin->fin_cksum = FI_CK_SUMOK;
			}
		} else if (cflags == pflag) {
			fin->fin_cksum = FI_CK_SUMOK;
		} else {
			manual = 1;
		}
	}
	if (manual != 0)
		if (ipf_checkl4sum(fin) == -1) {
			fin->fin_flx |= FI_BAD;
			return -1;
		}
# else
	if (ipf_checkl4sum(fin) == -1) {
		fin->fin_flx |= FI_BAD;
		return -1;
	}
# endif
	return 0;
}
#endif /* USE_INET6 */


size_t mbufchainlen(m0)
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
			FREE_MB_T(*fin->fin_mp);
			m = NULL;
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


void *
getifp(name, v)
	char *name;
	int v;
{
	return NULL;
}


int
ipf_inject(fin, m)
	fr_info_t *fin;
	mb_t *m;
{

	FREE_MB_T(m);

	fin->fin_m = NULL;
	fin->fin_ip = NULL;

	return EINVAL;
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
