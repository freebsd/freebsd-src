/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#if !defined(lint)
static const char sccsid[] = "%W% %G% (C) 1993-2000 Darren Reed";
static const char rcsid[] = "@(#)$Id$";
#endif

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/cpuvar.h>
#include <sys/open.h>
#include <sys/ioctl.h>
#include <sys/filio.h>
#include <sys/systm.h>
#if SOLARIS2 >= 10
# include <sys/cred_impl.h>
#else
# include <sys/cred.h>
#endif
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ksynch.h>
#include <sys/kmem.h>
#include <sys/mkdev.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/dditypes.h>
#include <sys/cmn_err.h>
#include <net/if.h>
#include <net/af.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/tcpip.h>
#include <netinet/ip_icmp.h>
#include "netinet/ip_compat.h"
#ifdef	USE_INET6
# include <netinet/icmp6.h>
#endif
#include "netinet/ip_fil.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_frag.h"
#include "netinet/ip_state.h"
#include "netinet/ip_auth.h"
#include "netinet/ip_proxy.h"
#include "netinet/ip_lookup.h"
#include "netinet/ip_dstlist.h"
#include <inet/ip_ire.h>

#ifdef HAS_SYS_MD5_H
# include <sys/md5.h>
#else
# include "md5.h"
#endif

static	int	ipf_send_ip __P((fr_info_t *fin, mblk_t *m));
static	void	ipf_fixl4sum __P((fr_info_t *));
static	void	*ipf_routeto __P((fr_info_t *, int, void *));
static	int	ipf_sendpkt __P((ipf_main_softc_t *, int, void *, mblk_t *,
				 struct ip *, void *));
static	void	ipf_call_slow_timer __P((ipf_main_softc_t *));
#if (SOLARIS2 < 7)
static	void	ipf_timer_func __P((void));
#else
static	void	ipf_timer_func __P((void *));
#endif

static	u_short	ipid = 0;
#if !defined(FW_HOOKS)
# if SOLARIS2 >= 7
u_int		*ip_ttl_ptr = NULL;
u_int		*ip_mtudisc = NULL;
#  if SOLARIS2 >= 8
int		*ip_forwarding = NULL;
u_int		*ip6_forwarding = NULL;
#  else
u_int		*ip_forwarding = NULL;
# endif
# else
u_long		*ip_ttl_ptr = NULL;
u_long		*ip_mtudisc = NULL;
u_long		*ip_forwarding = NULL;
# endif
extern	ipf_main_softc_t	ipfmain;
#else
extern	void	ipf_attach_hooks __P((ipf_main_softc_t *));
extern	void	ipf_detach_hooks __P((ipf_main_softc_t *));
#endif


/* ------------------------------------------------------------------------ */
/* Function:    ipfdetach                                                   */
/* Returns:     int - 0 == success, else error.                             */
/* Parameters:  Nil                                                         */
/*                                                                          */
/* This function is responsible for undoing anything that might have been   */
/* done in a call to ipfattach().  It must be able to clean up from a call  */
/* to ipfattach() that did not succeed.  Why might that happen?  Someone    */
/* configures a table to be so large that we cannot allocate enough memory  */
/* for it.                                                                  */
/* ------------------------------------------------------------------------ */
int
ipfdetach(softc)
	ipf_main_softc_t *softc;
{

#if !defined(FW_HOOKS)
	if (softc->ipf_control_forwarding & 2) {
		if (ip_forwarding != NULL)
			*ip_forwarding = 0;
# if SOLARIS2 >= 8
		if (ip6_forwarding != NULL)
			*ip6_forwarding = 0;
# endif
	}

	ipf_pfil_hooks_remove();
#else
	ipf_detach_hooks(softc);
#endif

	if (softc->ipf_slow_ch != 0) {
		(void) untimeout(softc->ipf_slow_ch);
		softc->ipf_slow_ch = 0;
	}

#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "ipfdetach()\n");
#endif

        if (ipf_fini_all(softc) < 0)
		return EIO;

	return 0;
}


int
ipfattach(softc)
	ipf_main_softc_t *softc;
{
	int i;

#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "ipfattach()\n");
#endif

#if !defined(FW_HOOKS)
# if SOLARIS2 >= 8
	ip_forwarding = &ip_g_forward;
# endif
	/*
	 * XXX - There is no terminator for this array, so it is not possible
	 * to tell if what we are looking for is missing and go off the end
	 * of the array.
	 */

# if SOLARIS2 <= 8
	for (i = 0; ; i++) {
		if (!strcmp(ip_param_arr[i].ip_param_name, "ip_def_ttl")) {
			ip_ttl_ptr = &ip_param_arr[i].ip_param_value;
		} else if (!strcmp(ip_param_arr[i].ip_param_name,
			    "ip_path_mtu_discovery")) {
			ip_mtudisc = &ip_param_arr[i].ip_param_value;
		}
#  if SOLARIS2 < 8
		else if (!strcmp(ip_param_arr[i].ip_param_name,
			    "ip_forwarding")) {
			ip_forwarding = &ip_param_arr[i].ip_param_value;
		}
#  else
		else if (!strcmp(ip_param_arr[i].ip_param_name,
			    "ip6_forwarding")) {
			ip6_forwarding = &ip_param_arr[i].ip_param_value;
		}
#  endif

		if (ip_mtudisc != NULL && ip_ttl_ptr != NULL &&
#  if SOLARIS2 >= 8
		    ip6_forwarding != NULL &&
#  endif
		    ip_forwarding != NULL)
			break;
	}
# endif

	if (softc->ipf_control_forwarding & 1) {
		if (ip_forwarding != NULL)
			*ip_forwarding = 1;
# if SOLARIS2 >= 8
		if (ip6_forwarding != NULL)
			*ip6_forwarding = 1;
# endif
	}
#endif

        if (ipf_init_all(softc) < 0)
		return EIO;
	softc->ipf_slow_ch = timeout(ipf_timer_func, softc,
				     drv_usectohz(500000));

#if !defined(FW_HOOKS)
	ipf_set_pfil_hooks();
#else
	ipf_attach_hooks(softc);
#endif
	ipid = 0;

	return 0;
}


/*
 * Filter ioctl interface.
 */
/*ARGSUSED*/
int
ipfioctl(dev, cmd, data, mode, cp, rp)
	dev_t dev;
	int cmd;
#if SOLARIS2 >= 7
	intptr_t data;
#else
	int *data;
#endif
	int mode;
	cred_t *cp;
	int *rp;
{
	ipf_main_softc_t *softc;
	int error = 0;
	minor_t unit;

#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "ipfioctl(%x,%x,%x,%d,%x,%d)\n",
		dev, cmd, data, mode, cp, rp);
#endif
	unit = getminor(dev);
	if (IPL_LOGMAX < unit) {
		IPFERROR(130002);
		return ENXIO;
	}

	softc = GET_SOFTC(crgetzoneid(cp));

	if (softc->ipf_running <= 0) {
		if (unit != IPL_LOGIPF && cmd != SIOCIPFINTERROR) {
			IPFERROR(130003);
			return EIO;
		}
		if (cmd != SIOCIPFGETNEXT && cmd != SIOCIPFGET &&
		    cmd != SIOCIPFSET && cmd != SIOCFRENB &&
		    cmd != SIOCGETFS && cmd != SIOCGETFF &&
		    cmd != SIOCIPFINTERROR) {
			IPFERROR(130004);
			return EIO;
		}
	}

	error = ipf_ioctlswitch(softc, unit, (caddr_t)data, cmd, mode,
			       cp->cr_uid, curproc);
	if (error != -1) {
		return error;
	}

	return error;
}


void *
get_unit(soft, name, family)
	void *soft;
	char *name;
	int family;
{
	void *ifp;
#if !defined(FW_HOOKS)
	qif_t *qf;
	int sap;

	if (family == 4)
		sap = 0x0800;
	else if (family == 6)
		sap = 0x86dd;
	else
		return NULL;
	rw_enter(&pfil_rw, RW_READER);
	qf = qif_iflookup(name, sap);
	rw_exit(&pfil_rw);
	return qf;
#else
	ipf_main_softc_t *softc = soft;
	net_handle_t proto;

	if (family == 0) {
		ifp = (void *)net_phylookup(softc->ipf_nd_v4, name);
		if (ifp == NULL)
			ifp = (void *)net_phylookup(softc->ipf_nd_v6, name);
		return ifp;
	}
	if (family == 4)
		proto = softc->ipf_nd_v4;
	else if (family == 6)
		proto = softc->ipf_nd_v6;
	else
		return NULL;
	return (void *)net_phylookup(proto, name);
#endif
}


/*
 * ipf_send_reset - this could conceivably be a call to tcp_respond(), but
 * that requires a large amount of setting up and isn't any more efficient.
 */
int
ipf_send_reset(fr_info_t *fin)
{
	tcphdr_t *tcp, *tcp2;
	int tlen, hlen;
	mblk_t *m;
#ifdef	USE_INET6
	ip6_t *ip6;
#endif
	ip_t *ip;

	tcp = fin->fin_dp;
	if (tcp->th_flags & TH_RST)
		return -1;

	if (ipf_checkl4sum(fin) == -1)
		return -1;

	tlen = (tcp->th_flags & (TH_SYN|TH_FIN)) ? 1 : 0;
#ifdef	USE_INET6
	if (fin->fin_v == 6)
		hlen = sizeof(ip6_t);
	else
#endif
		hlen = sizeof(ip_t);
	hlen += sizeof(*tcp2);
	if ((m = (mblk_t *)allocb(hlen + 64, BPRI_HI)) == NULL)
		return -1;

	m->b_rptr += 64;
	MTYPE(m) = M_DATA;
	m->b_wptr = m->b_rptr + hlen;
	ip = (ip_t *)m->b_rptr;
	bzero((char *)ip, hlen);
	tcp2 = (struct tcphdr *)(m->b_rptr + hlen - sizeof(*tcp2));
	tcp2->th_dport = tcp->th_sport;
	tcp2->th_sport = tcp->th_dport;
	if (tcp->th_flags & TH_ACK) {
		tcp2->th_seq = tcp->th_ack;
		tcp2->th_flags = TH_RST;
	} else {
		tcp2->th_ack = ntohl(tcp->th_seq);
		tcp2->th_ack += tlen;
		tcp2->th_ack = htonl(tcp2->th_ack);
		tcp2->th_flags = TH_RST|TH_ACK;
	}
	tcp2->th_off = sizeof(struct tcphdr) >> 2;

	ip->ip_v = fin->fin_v;
#ifdef	USE_INET6
	if (fin->fin_v == 6) {
		ip6 = (ip6_t *)m->b_rptr;
		ip6->ip6_flow = ((ip6_t *)fin->fin_ip)->ip6_flow;
		ip6->ip6_src = fin->fin_dst6.in6;
		ip6->ip6_dst = fin->fin_src6.in6;
		ip6->ip6_plen = htons(sizeof(*tcp));
		ip6->ip6_nxt = IPPROTO_TCP;
	} else
#endif
	{
		ip->ip_src.s_addr = fin->fin_daddr;
		ip->ip_dst.s_addr = fin->fin_saddr;
		ip->ip_id = ipf_nextipid(fin);
		ip->ip_hl = sizeof(*ip) >> 2;
		ip->ip_p = IPPROTO_TCP;
		ip->ip_len = htons(sizeof(*ip) + sizeof(*tcp));
		ip->ip_tos = fin->fin_ip->ip_tos;
	}
	return ipf_send_ip(fin, m);
}


/*ARGSUSED*/
static int
ipf_send_ip(fr_info_t *fin, mblk_t *m)
{
#if !defined(FW_HOOKS)
	qif_t *qif;
#endif
	qpktinfo_t qpi, *qpip;
	fr_info_t fnew;
	ip_t *ip;
	int i, hlen;

	ip = (ip_t *)m->b_rptr;
	bzero((char *)&fnew, sizeof(fnew));
	fnew.fin_main_soft = fin->fin_main_soft;

#ifdef	USE_INET6
	if (fin->fin_v == 6) {
		ip6_t *ip6;

		ip6 = (ip6_t *)ip;
		ip6->ip6_vfc = 0x60;
		ip6->ip6_hlim = 127;
		fnew.fin_p = ip6->ip6_nxt;
		fnew.fin_v = 6;
		hlen = sizeof(*ip6);
		fnew.fin_plen = ntohs(ip6->ip6_plen) + hlen;
	} else
#endif
	{
		fnew.fin_v = 4;
#if !defined(FW_HOOKS)
		if (ip_ttl_ptr != NULL)
			ip->ip_ttl = (u_char)(*ip_ttl_ptr);
		else
#endif
			ip->ip_ttl = 63;
#if !defined(FW_HOOKS)
		if (ip_mtudisc != NULL)
			ip->ip_off = htons(*ip_mtudisc ? IP_DF : 0);
		else
#endif
			ip->ip_off = htons(IP_DF);
		fnew.fin_p = ip->ip_p;
		fnew.fin_plen = ntohs(ip->ip_len);
		ip->ip_sum = ipf_cksum((u_short *)ip, sizeof(*ip));
		hlen = sizeof(*ip);
	}

	qpip = fin->fin_qpi;
	qpi.qpi_q = qpip->qpi_q;
	qpi.qpi_off = 0;
#if defined(FW_HOOKS)
	qpi.qpi_real = qpip->qpi_real;
	qpi.qpi_ill = qpip->qpi_real;
#else
	qif = qpip->qpi_real;
	qpi.qpi_real = qif;
	qpi.qpi_ill = qif->qf_ill;
	qpi.qpi_flags = qif->qf_flags;
#endif
	qpi.qpi_m = m;
	qpi.qpi_data = ip;
	fnew.fin_qpi = &qpi;
	fnew.fin_ifp = fin->fin_ifp;
	fnew.fin_flx = FI_NOCKSUM;
	fnew.fin_m = m;
	fnew.fin_qfm = m;
	fnew.fin_ip = ip;
	fnew.fin_mp = &m;
	fnew.fin_hlen = hlen;
	fnew.fin_dp = (char *)ip + hlen;
	if (fnew.fin_p == IPPROTO_TCP) {
		tcphdr_t *tcp2 = fnew.fin_dp;
		tcp2->th_sum = fr_cksum(&fnew, ip, IPPROTO_TCP, tcp2);
	}
	if (ipf_makefrip(hlen, ip, &fnew) == -1)
		return -1;

	if (fin->fin_fr != NULL && fin->fin_fr->fr_type == FR_T_IPF) {
		frdest_t *fdp = &fin->fin_fr->fr_rif;

		if ((fdp->fd_ptr != NULL) &&
		    (fdp->fd_ptr != (struct ifnet *)-1))
			return ipf_fastroute(m, &m, &fnew, fdp);
	}

	i = ipf_fastroute(m, &m, &fnew, NULL);
	return i;
}


int
ipf_send_icmp_err(int type, fr_info_t *fin, int dst)
{
	ipf_main_softc_t *softc = fin->fin_main_soft;
	struct in_addr dst4;
	struct icmp *icmp;
	qpktinfo_t *qpi;
	int hlen, code;
	i6addr_t dst6;
	u_short sz;
#ifdef	USE_INET6
	mblk_t *mb;
#endif
	mblk_t *m;
#ifdef	USE_INET6
	ip6_t *ip6;
#endif
	ip_t *ip;

	if ((type < 0) || (type > ICMP_MAXTYPE))
		return -1;

	code = fin->fin_icode;
#ifdef USE_INET6
	if ((code < 0) || (code > sizeof(icmptoicmp6unreach)/sizeof(int)))
		return -1;
#endif

	if (ipf_checkl4sum(fin) == -1)
		return -1;

	qpi = fin->fin_qpi;

#ifdef	USE_INET6
	mb = fin->fin_qfm;

	if (fin->fin_v == 6) {
		sz = sizeof(ip6_t);
		sz += MIN(mb->b_wptr - mb->b_rptr, 512);
		hlen = sizeof(ip6_t);
		type = icmptoicmp6types[type];
		if (type == ICMP6_DST_UNREACH)
			code = icmptoicmp6unreach[code];
	} else
#endif
	{
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

		sz = sizeof(ip_t) * 2;
		sz += 8;		/* 64 bits of data */
		hlen = sizeof(ip_t);
	}

	sz += offsetof(struct icmp, icmp_ip);
	if ((m = (mblk_t *)allocb((size_t)sz + 64, BPRI_HI)) == NULL)
		return -1;
	MTYPE(m) = M_DATA;
	m->b_rptr += 64;
	m->b_wptr = m->b_rptr + sz;
	bzero((char *)m->b_rptr, (size_t)sz);
	ip = (ip_t *)m->b_rptr;
	ip->ip_v = fin->fin_v;
	icmp = (struct icmp *)(m->b_rptr + hlen);
	icmp->icmp_type = type & 0xff;
	icmp->icmp_code = code & 0xff;
#ifdef	icmp_nextmtu
	if (type == ICMP_UNREACH && fin->fin_icode == ICMP_UNREACH_NEEDFRAG) {
		if (fin->fin_mtu != 0) {
			icmp->icmp_nextmtu = htons(fin->fin_mtu);

		} else if (GETIFMTU_4(qpi->qpi_real) != 0) {
			icmp->icmp_nextmtu = GETIFMTU_4(qpi->qpi_real);

		} else {	/* Make up a number */
			icmp->icmp_nextmtu = htons(fin->fin_plen - 20);
		}
	}
#endif

#ifdef	USE_INET6
	if (fin->fin_v == 6) {
		int csz;

		if (dst == 0) {
			if (ipf_ifpaddr(softc, 6, FRI_NORMAL, qpi->qpi_real,
					&dst6, NULL) == -1) {
				FREE_MB_T(m);
				return -1;
			}
		} else
			dst6 = fin->fin_dst6;

		csz = sz;
		sz -= sizeof(ip6_t);
		ip6 = (ip6_t *)m->b_rptr;
		ip6->ip6_flow = ((ip6_t *)fin->fin_ip)->ip6_flow;
		ip6->ip6_plen = htons((u_short)sz);
		ip6->ip6_nxt = IPPROTO_ICMPV6;
		ip6->ip6_src = dst6.in6;
		ip6->ip6_dst = fin->fin_src6.in6;
		sz -= offsetof(struct icmp, icmp_ip);
		bcopy((char *)mb->b_rptr, (char *)&icmp->icmp_ip, sz);
		icmp->icmp_cksum = csz - sizeof(ip6_t);
	} else
#endif
	{
		ip->ip_hl = sizeof(*ip) >> 2;
		ip->ip_p = IPPROTO_ICMP;
		ip->ip_id = fin->fin_ip->ip_id;
		ip->ip_tos = fin->fin_ip->ip_tos;
		ip->ip_len = (u_short)sz;
		if (dst == 0) {
			if (ipf_ifpaddr(softc, 4, FRI_NORMAL, qpi->qpi_real,
					&dst6, NULL) == -1) {
				FREE_MB_T(m);
				return -1;
			}
			dst4 = dst6.in4;
		} else
			dst4 = fin->fin_dst;
		ip->ip_src = dst4;
		ip->ip_dst = fin->fin_src;
		bcopy((char *)fin->fin_ip, (char *)&icmp->icmp_ip,
		      sizeof(*fin->fin_ip));
		bcopy((char *)fin->fin_ip + fin->fin_hlen,
		      (char *)&icmp->icmp_ip + sizeof(*fin->fin_ip), 8);
		icmp->icmp_cksum = ipf_cksum((u_short *)icmp,
					     sz - sizeof(ip_t));
	}

	/*
	 * Need to exit out of these so we don't recursively call rw_enter
	 * from fr_qout.
	 */
	return ipf_send_ip(fin, m);
}


#if !defined(FW_HOOKS)
/*
 * return the first IP Address associated with an interface
 */
/*ARGSUSED*/
int
ipf_ifpaddr(softc, v, atype, qifptr, inp, inpmask)
	ipf_main_softc_t *softc;
	int v;
	int atype;
	void *qifptr;
	i6addr_t *inp, *inpmask;
{
# ifdef	USE_INET6
	struct sockaddr_in6 sin6, mask6;
# endif
	struct sockaddr_in sin, mask;
	qif_t *qif;

	if ((qifptr == NULL) || (qifptr == (void *)-1))
		return -1;

	qif = qifptr;
	if (qif->qf_ill == NULL)
		return -1;

# ifdef	USE_INET6
	if (v == 6) {
		in6_addr_t *inp6;
		ipif_t *ipif;
		ill_t *ill;

		ill = qif->qf_ill;

		/*
		 * First is always link local.
		 */
		for (ipif = ill->ill_ipif; ipif; ipif = ipif->ipif_next) {
			inp6 = &ipif->ipif_v6lcl_addr;
			if (!IN6_IS_ADDR_LINKLOCAL(inp6) &&
			    !IN6_IS_ADDR_LOOPBACK(inp6))
				break;
		}
		if (ipif == NULL)
			return -1;

		mask6.sin6_addr = ipif->ipif_v6net_mask;
		if (atype == FRI_BROADCAST)
			sin6.sin6_addr = ipif->ipif_v6brd_addr;
		else if (atype == FRI_PEERADDR)
			sin6.sin6_addr = ipif->ipif_v6pp_dst_addr;
		else
			sin6.sin6_addr = *inp6;
		return ipf_ifpfillv6addr(atype, &sin6, &mask6, inp, inpmask);
	}
# endif

	if (((ill_t *)qif->qf_ill)->ill_ipif == NULL)
		return -1;

	switch (atype)
	{
	case FRI_BROADCAST :
		sin.sin_addr.s_addr = QF_V4_BROADCAST(qif);
		break;
	case FRI_PEERADDR :
		sin.sin_addr.s_addr = QF_V4_PEERADDR(qif);
		break;
	default :
		sin.sin_addr.s_addr = QF_V4_ADDR(qif);
		break;
	}
	mask.sin_addr.s_addr = QF_V4_NETMASK(qif);

	return ipf_ifpfillv4addr(atype, &sin, &mask, &inp->in4, &inpmask->in4);
}
#else
/*ARGSUSED*/
int
ipf_ifpaddr(softc, v, atype, qifptr, inp, inpmask)
	ipf_main_softc_t *softc;
	int v;
	int atype;
	void *qifptr;
	i6addr_t *inp, *inpmask;
{
	struct sockaddr_in6 sin6[2];
	struct sockaddr_in sin[2];
	net_ifaddr_t types[2];
	void *array;
	int error;

	if ((qifptr == NULL) || (qifptr == (void *)-1))
		return -1;

	switch (atype)
	{
	case FRI_BROADCAST :
		types[0] = NA_BROADCAST;
		break;
	case FRI_PEERADDR :
		types[0] = NA_PEER;
		break;
	default :
		types[0] = NA_ADDRESS;
		break;
	}
	types[1] = NA_NETMASK;

	if (v == 4) {
		int logical = 0;

		array = sin;
		do {
			error = net_getlifaddr(softc->ipf_nd_v4,
					       (phy_if_t)qifptr, logical,
					       2, types, sin);
			logical++;
		} while (types[0] == NA_ADDRESS && error == 0 &&
			 sin[0].sin_addr.s_addr == 0);
		if (sin[0].sin_addr.s_addr == 0 && error != 0)
			 return error;
	} else {
		array = sin6;
		net_getlifaddr(softc->ipf_nd_v6, (phy_if_t)qifptr, 0,
			       2, types, sin6);
	}

	if (v == 6)
		return ipf_ifpfillv6addr(atype, &sin6[0], &sin6[1],
					 inp, inpmask);

	return ipf_ifpfillv4addr(atype, &sin[0], &sin[1],
				 &inp->in4, &inpmask->in4);
}
#endif


u_32_t
ipf_newisn(fr_info_t *fin)
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
/* Function:    ipf_nextipid                                                */
/* Returns:     int - 0 == success, -1 == error (packet should be droppped) */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* Returns the next IPv4 ID to use for this packet.                         */
/* ------------------------------------------------------------------------ */
u_short
ipf_nextipid(fr_info_t *fin)
{
	ipf_main_softc_t *softc = fin->fin_main_soft;
	ipstate_t *is;
	nat_t *nat;
	u_short id;

	MUTEX_ENTER(&softc->ipf_rw);
	if (fin->fin_pktnum != 0) {
		/*
		 * The -1 is for aligned test results.
		 */
		id = (fin->fin_pktnum - 1) & 0xffff;
		id = ipid++;
	} else {
		id = ipid++;
	}
	MUTEX_EXIT(&softc->ipf_rw);

	return id;
}


INLINE int
ipf_checkv4sum(fr_info_t *fin)
{
#if defined(NET_HCK_L4_FULL)
	ipf_main_softc_t *softc = fin->fin_main_soft;
#endif
	int ckbits;

	if ((fin->fin_flx & FI_NOCKSUM) != 0)
		return 0;

	if ((fin->fin_flx & FI_SHORT) != 0)
		return 1;

	if (fin->fin_cksum != FI_CK_NEEDED)
		return (fin->fin_cksum > FI_CK_NEEDED) ? 0 : -1;

#if defined(NET_HCK_L4_FULL)
	ckbits = net_ispartialchecksum(softc->ipf_nd_v4, fin->fin_m);
	if (ckbits & NET_HCK_L4_FULL) {
		fin->fin_cksum = FI_CK_L4FULL;
		return 0;
	} else if (ckbits & NET_HCK_L4_PART) {
		fin->fin_cksum = FI_CK_L4PART;
		return 0;
	}
#endif
#if SOLARIS && defined(_KERNEL) && (SOLARIS2 >= 6) && defined(ICK_VALID)
	if (dohwcksum && ((*fin->fin_mp)->b_ick_flag == ICK_VALID)) {
		fin->fin_cksum = FI_CK_SUMOK;
		return 0;
	}
#endif

	if (ipf_checkl4sum(fin) == -1) {
		DT1(bad_l4_sum, fr_info_t *, fin);
		fin->fin_flx |= FI_BAD;
		return -1;
	}
	return 0;
}


#ifdef USE_INET6
INLINE int
ipf_checkv6sum(fr_info_t *fin)
{
#if defined(NET_HCK_L4_FULL)
	ipf_main_softc_t *softc = fin->fin_main_soft;
#endif
	int ckbits;

	if ((fin->fin_flx & FI_NOCKSUM) != 0)
		return 0;

	if ((fin->fin_flx & FI_SHORT) != 0)
		return 1;

	if (fin->fin_cksum != FI_CK_NEEDED)
		return (fin->fin_cksum > FI_CK_NEEDED) ? 0 : -1;

#if defined(NET_HCK_L4_FULL)
	ckbits = net_ispartialchecksum(softc->ipf_nd_v6, fin->fin_m);
	if (ckbits & NET_HCK_L4_FULL) {
		fin->fin_cksum = FI_CK_L4FULL;
		return 0;
	} else if (ckbits & NET_HCK_L4_PART) {
		fin->fin_cksum = FI_CK_L4PART;
		return 0;
	}
#endif

	if (ipf_checkl4sum(fin) == -1) {
		DT1(bad_l4_sum, fr_info_t *, fin);
		fin->fin_flx |= FI_BAD;
		return -1;
	}
	return 0;
}
#endif /* USE_INET6 */


#if !defined(FW_HOOKS)
/*
 * Function:    ipf_verifysrc
 * Returns:     int (really boolean)
 * Parameters:  fin - packet information
 *
 * Check whether the packet has a valid source address for the interface on
 * which the packet arrived, implementing the "ipf_chksrc" feature.
 * Returns true iff the packet's source address is valid.
 * Pre-Solaris 10, we call into the routing code to make the determination.
 * On Solaris 10 and later, we have a valid address set from pfild to check
 * against.
 */
int
ipf_verifysrc(fin)
	fr_info_t *fin;
{
	ire_t *dir;
	int result;

#if SOLARIS2 >= 6
	dir = ire_route_lookup(fin->fin_saddr, 0xffffffff, 0, 0, NULL,
			       NULL, NULL,
# ifdef IP_ULP_OUT_LABELED
			       NULL,
# endif
			       MATCH_IRE_DSTONLY|MATCH_IRE_DEFAULT|
			       MATCH_IRE_RECURSIVE);
#else
	dir = ire_lookup(fin->fin_saddr);
#endif

	if (!dir)
		return 0;
	result = (ire_to_ill(dir) == fin->fin_ifp);
#if SOLARIS2 >= 8
	ire_refrele(dir);
#endif
	return result;
}
#else
int
ipf_verifysrc(fin)
	fr_info_t *fin;
{
	int result;

	result = (ipf_routeto(fin, fin->fin_v, &fin->fin_src) == fin->fin_ifp);
	return result;
}
#endif


#if (SOLARIS2 < 7)
static void
ipf_timer_func()
{
	ipf_call_slow_timer(&ipfmain);
}
#else
void
/*ARGSUSED*/
ipf_timer_func(ptr)
	void *ptr;
{
	ipf_call_slow_timer(ptr);
}
#endif

static void
ipf_call_slow_timer(softc)
	ipf_main_softc_t *softc;
{
	READ_ENTER(&softc->ipf_global);

	if (softc->ipf_running > 0)
		ipf_slowtimer(softc);

	if (softc->ipf_running == -1 || softc->ipf_running == 1)
		softc->ipf_slow_ch = timeout(ipf_timer_func, softc,
					     drv_usectohz(500000));
	else
		softc->ipf_slow_ch = NULL;
	RWLOCK_EXIT(&softc->ipf_global);
}


/*
 * Function:  ipf_fastroute
 * Returns:    0: success;
 *            -1: failed
 * Parameters:
 *    mb: the message block where ip head starts
 *    mpp: the pointer to the pointer of the orignal
 *            packet message
 *    fin: packet information
 *    fdp: destination interface information
 *    if it is NULL, no interface information provided.
 *
 * This function is for fastroute/to/dup-to rules. It calls
 * pfil_make_lay2_packet to search route, make lay-2 header
 * ,and identify output queue for the IP packet.
 * The destination address depends on the following conditions:
 * 1: for fastroute rule, fdp is passed in as NULL, so the
 *    destination address is the IP Packet's destination address
 * 2: for to/dup-to rule, if an ip address is specified after
 *    the interface name, this address is the as destination
 *    address. Otherwise IP Packet's destination address is used
 */
int
ipf_fastroute(mb, mpp, fin, fdp)
	mblk_t *mb, **mpp;
	fr_info_t *fin;
	frdest_t *fdp;
{
	ipf_main_softc_t *softc = fin->fin_main_soft;
	struct in_addr dst;
	qpktinfo_t *qpi;
	frdest_t node;
	frentry_t *fr;
	frdest_t fd;
	void *dstp;
	void *sifp;
	void *ifp;
	ip_t *ip;
#ifndef	sparc
	u_short __iplen, __ipoff;
#endif
#ifdef	USE_INET6
	ip6_t *ip6 = (ip6_t *)fin->fin_ip;
	struct in6_addr dst6;
#endif

	fr = fin->fin_fr;
	ip = fin->fin_ip;
	qpi = fin->fin_qpi;

	if ((fr != NULL) && !(fr->fr_flags & FR_KEEPSTATE) && (fdp != NULL) &&
	    (fdp->fd_type == FRD_DSTLIST)) {
		if (ipf_dstlist_select_node(fin, fdp->fd_ptr, NULL, &node) == 0)
			fdp = &node;
	}

	/*
	 * If this is a duplicate mblk then we want ip to point at that
	 * data, not the original, if and only if it is already pointing at
	 * the current mblk data.
	 */
	if (ip == (ip_t *)qpi->qpi_m->b_rptr && qpi->qpi_m != mb)
		ip = (ip_t *)mb->b_rptr;

	/*
	 * If there is another M_PROTO, we don't want it
	 */
	if (*mpp != mb) {
		mblk_t *mp;

		mp = unlinkb(*mpp);
		freeb(*mpp);
		*mpp = mp;
	}

	/*
	 * If the fdp is NULL then there is no set route for this packet.
	 */
	if (fdp == NULL) {
		ifp = fin->fin_ifp;

		switch (fin->fin_v)
		{
		case 4 :
			fd.fd_ip = ip->ip_dst;
			ifp = ipf_routeto(fin, 4, &ip->ip_dst);
			break;
#ifdef USE_INET6
		case 6 :
			fd.fd_ip6.in6 = ip6->ip6_dst;
			ifp = ipf_routeto(fin, 6, &ip6->ip6_dst);
			break;
#endif
		}
		fdp = &fd;
	} else {
		ifp = fdp->fd_ptr;

		if (ifp == NULL || ifp == (void *)-1)
			goto bad_fastroute;
	}

	/*
	 * In case we're here due to "to <if>" being used with
	 * "keep state", check that we're going in the correct
	 * direction.
	 */
	if ((fr != NULL) && (fin->fin_rev != 0)) {
		if ((ifp != NULL) && (fdp == &fr->fr_tif))
			return -1;
		dst.s_addr = fin->fin_fi.fi_daddr;
	} else {
		if (fin->fin_v == 4) {
			if (fdp->fd_ip.s_addr != 0)
				dst = fdp->fd_ip;
			else
				dst.s_addr = fin->fin_fi.fi_daddr;
			dstp = &dst;
		}
#ifdef USE_INET6
		else if (fin->fin_v == 6) {
			if (IP6_NOTZERO(&fdp->fd_ip))
				dst6 = fdp->fd_ip6.in6;
			else
				dst6 = fin->fin_dst6.in6;
		}
#endif
	}

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
			goto bad_fastroute;
			break;
		}

		fin->fin_out = 0;
		fin->fin_ifp = sifp;
	} else if (fin->fin_out == 1) {
#if SOLARIS2 >= 6
		/*
		 * We're taking a packet from an interface and putting it on
		 * another interface.  There's no guarantee that the other
		 * interface will have the same capabilities, so disable
		 * any flags that are set and do things manually for both
		 * IP and TCP/UDP
		 */
		if (mb->b_datap->db_struioflag) {
			mb->b_datap->db_struioflag = 0;

			if (fin->fin_v == 4) {
				ip->ip_sum = 0;
				ip->ip_sum = ipf_cksum((u_short *)ip,
						       sizeof(*ip));
			}
			ipf_fixl4sum(fin);
		}
#endif
	}

#ifndef sparc
	if (fin->fin_v == 4) {
		__iplen = (u_short)ip->ip_len,
		__ipoff = (u_short)ip->ip_off;

		ip->ip_len = htons(__iplen);
		ip->ip_off = htons(__ipoff);
	}
#endif
	if (ipf_sendpkt(softc, 4, ifp, mb, ip, dstp) == 0) {
		ATOMIC_INCL(softc->ipf_frouteok[0]);
	} else {
		ATOMIC_INCL(softc->ipf_frouteok[1]);
	}
	return 0;

bad_fastroute:
	ATOMIC_INCL(softc->ipf_frouteok[1]);
	freemsg(mb);
	return -1;
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
ipf_pullup(mb_t *xmin, fr_info_t *fin, int len)
{
	qpktinfo_t *qpi = fin->fin_qpi;
	int out = fin->fin_out, dpoff;
	char *ip;
	mb_t *m;

	if (xmin == NULL)
		return NULL;

	ip = (char *)fin->fin_ip;
	if ((fin->fin_flx & FI_COALESCE) != 0)
		return ip;

	len += fin->fin_ipoff;

	if (M_LEN(xmin) < len) {
		mblk_t *mnew = msgpullup(xmin, len);

		if (mnew == NULL) {
			FREE_MB_T(*fin->fin_mp);
			*fin->fin_mp = NULL;
			fin->fin_m = NULL;
			fin->fin_ip = NULL;
			fin->fin_dp = NULL;
			qpi->qpi_data = NULL;
			return NULL;
		}

		if (fin->fin_dp != NULL)
			dpoff = (char *)fin->fin_dp - (char *)ip;
		else
			dpoff = 0;

		if (*fin->fin_mp == xmin) {
			dblk_t *dt, *dm;

			/*
			 * These fields are not preserved by msgpullup and as
			 * they are used by hardware checksum code, preserving
			 * them is necessary or else the packet is dropped.
			 * This information is only stored in the first dblk
			 * in an mblk chain and because the change here is to
			 * how much data is in the dblk and not where the data
			 * is relative to the start of the dblk, copying the
			 * values is safe.
			 */
			dt = mnew->b_datap;
			dm = xmin->b_datap;
			dt->db_cksumstart = dm->db_cksumstart;
			dt->db_cksumend = dm->db_cksumend;
			dt->db_cksumstuff = dm->db_cksumstuff;
			dt->db_struioun = dm->db_struioun;
			freemsg(*fin->fin_mp);
			*fin->fin_mp = mnew;
		} else {
			for (m = *fin->fin_mp; m != NULL; m = m->b_cont)
				if (m->b_cont == xmin)
					break;
			freemsg(m->b_cont);
			m->b_cont = mnew;
		}

		fin->fin_qfm = mnew;
		fin->fin_m = mnew;
		ip = MTOD(mnew, char *) + fin->fin_ipoff;
		if (fin->fin_dp != NULL)
			fin->fin_dp = (char *)ip + dpoff;
		if (fin->fin_fraghdr != NULL)
			fin->fin_fraghdr = (char *)ip +
					   ((char *)fin->fin_fraghdr -
					    (char *)fin->fin_ip);
		fin->fin_ip = (ip_t *)ip;
		qpi->qpi_data = ip;
		qpi->qpi_m = mnew;
	}

	if (len == fin->fin_plen)
		fin->fin_flx |= FI_COALESCE;
	return ip;
}


int
ipf_inject(fr_info_t *fin, mb_t *m)
{
#if !defined(FW_HOOKS)
	qifpkt_t *qp;

	qp = kmem_alloc(sizeof(*qp), KM_NOSLEEP);
	if (qp == NULL) {
		freemsg(*fin->fin_mp);
		return ENOMEM;
	}

	qp->qp_mb = *fin->fin_mp;
	if (fin->fin_v == 4)
		qp->qp_sap = 0x800;
	else if (fin->fin_v == 6)
		qp->qp_sap = 0x86dd;
	qp->qp_inout = fin->fin_out;
	strncpy(qp->qp_ifname, fin->fin_ifname, LIFNAMSIZ);
	qif_addinject(qp);
#else
	ipf_main_softc_t *softc = fin->fin_main_soft;
	struct sockaddr_in6 *sin6;
	struct sockaddr_in *sin;
	net_inject_t inject;
	inject.ni_physical = (phy_if_t)fin->fin_ifp;
	inject.ni_packet = *fin->fin_mp;
	if (fin->fin_v == 4) {
		sin = (struct sockaddr_in *)&inject.ni_addr;
		sin->sin_family = AF_INET;
		sin->sin_addr = fin->fin_dst;
		if (fin->fin_out == 0)
			return net_inject(softc->ipf_nd_v4, NI_QUEUE_IN,
					  &inject);
		return net_inject(softc->ipf_nd_v4, NI_QUEUE_OUT, &inject);
	}

	sin6 = (struct sockaddr_in6 *)&inject.ni_addr;
	sin6->sin6_family = AF_INET6;
	memcpy(&sin6->sin6_addr, &fin->fin_dst6, sizeof(fin->fin_dst6));
	if (fin->fin_out == 0)
		return net_inject(softc->ipf_nd_v6, NI_QUEUE_IN, &inject);
	return net_inject(softc->ipf_nd_v6, NI_QUEUE_OUT, &inject);
#endif
}


static int
ipf_sendpkt(softc, v, ifp, mb, ip, dstp)
	ipf_main_softc_t *softc;
	int v;
	void *ifp;
	mblk_t *mb;
	struct ip *ip;
	void *dstp;
{
#if !defined(FW_HOOKS)
        return pfil_sendbuf(ifp, mb, ip, dstp);
#else
	struct sockaddr_in6 *sin6;
	struct sockaddr_in *sin;
	net_inject_t inject;

	inject.ni_physical = (phy_if_t)ifp;
	inject.ni_packet = mb;

	if (v == 4) {
		sin = (struct sockaddr_in *)&inject.ni_addr;
		sin->sin_family = AF_INET;
		memcpy(&sin->sin_addr, dstp, sizeof(sin->sin_addr));
		return net_inject(softc->ipf_nd_v4, NI_DIRECT_OUT, &inject);
	}

	sin6 = (struct sockaddr_in6 *)&inject.ni_addr;
	sin6->sin6_family = AF_INET6;
	memcpy(&sin6->sin6_addr, dstp, sizeof(sin6->sin6_addr));
	return net_inject(softc->ipf_nd_v6, NI_DIRECT_OUT, &inject);
#endif
}


static void
ipf_fixl4sum(fr_info_t *fin)
{
	u_short *csump;
	udphdr_t *udp;

	csump = NULL;

	switch (fin->fin_p)
	{
	case IPPROTO_TCP :
		csump = &((tcphdr_t *)fin->fin_dp)->th_sum;
		break;

	case IPPROTO_UDP :
		udp = fin->fin_dp;
		if (udp->uh_sum != 0)
			csump = &udp->uh_sum;
		break;

	default :
		break;
	}

	if (csump != NULL) {
		*csump = 0;
		*csump = fr_cksum(fin, fin->fin_ip, fin->fin_p, fin->fin_dp);
	}
}


mblk_t *
allocmbt(size_t len)
{
	mblk_t *m;

	/*
	 * +64 is to reverse some token amount of space so that we
	 * might have a good chance of copying over data from the
	 * front of the existing IP packet to this one.
	 */
	m = allocb(len + 128, BPRI_HI);
	if (m != NULL) {
		m->b_rptr += 128;
		m->b_wptr = m->b_rptr + len;
	}
	return m;
}


void
ipf_prependmbt(fr_info_t *fin, mblk_t *m)
{
	mblk_t *o = NULL, *top;
	mblk_t *n = *fin->fin_mp;
	qpktinfo_t *qpi;
	int x;

	qpi = fin->fin_qpi;
	qpi->qpi_m = m;
	qpi->qpi_data = m->b_rptr;

	if (MTYPE(n) == M_DATA) {
		/*
		 * The aim here is to copy x bytes of data from immediately
		 * preceding the IP packet in the original mblk to the new
		 * mblk that now precedes it.  In doing this, b_rptr in the
		 * original packet is moved so that we don't transmit data
		 * that has been moved.
		 */
		x = min(fin->fin_ipoff, m->b_rptr - m->b_datap->db_base);

		if (x > 0) {
			m->b_rptr -= x;
			bcopy(n->b_rptr, m->b_rptr, x);
			n->b_rptr += x;
		}
	} else {
		/*
		 * If there are special mblk's at the start of the current
		 * message, free them so we can put our own there.  It doesn't
		 * matter what they were as we're completely changing the
		 * nature of this packet.
		 */
		for (; n != fin->fin_m; n = o) {
			o = n->b_cont;
			n->b_cont = NULL;
			freemsg(n);
		}
	}

	m->b_cont = n;
	*fin->fin_mp = m;
	fin->fin_m = m;
}


static void *
ipf_routeto(fin, v, dstip)
	fr_info_t *fin;
	int v;
	void *dstip;
{
#if defined(FW_HOOKS)
	ipf_main_softc_t *softc = fin->fin_main_soft;
	struct sockaddr_in6 sin6;
	struct sockaddr_in sin;
	struct sockaddr *sock;
	net_handle_t proto;
	int result;

	switch (fin->fin_v)
	{
	case 4 :
		bzero((char *)&sin, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_addr = fin->fin_src;
		sock = (struct sockaddr *)&sin;
		proto = softc->ipf_nd_v4;
		break;

	case 6 :
		bzero((char *)&sin6, sizeof(sin6));
		sin6.sin6_family = AF_INET;
		sin6.sin6_addr = fin->fin_srcip6;
		proto = softc->ipf_nd_v6;
		break;
	default :
		return NULL;
	}
	return (void *)net_routeto(proto, sock, NULL);
#else
	return qif_illrouteto(v, dstip);
#endif
}


u_int
ipf_pcksum(fin, hlen, sum)
	fr_info_t *fin;
	int hlen;
	u_int sum;
{
	u_int sum2;

	sum2 = ip_cksum(fin->fin_m, fin->fin_ipoff + hlen, sum);
	sum2 = (~sum2 & 0xffff);
	return sum2;
}
