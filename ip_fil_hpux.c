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
#include <sys/ioctl.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/cmn_err.h>
#include <sys/kernel.h>
#include <sys/callout.h>
#include <net/if.h>
#include <net/af.h>
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
#ifdef	USE_INET6
# include <netinet/icmp6.h>
#endif
#include "ip_fil.h"
#include "ip_state.h"
#include "ip_nat.h"
#include "ip_frag.h"
#include "ip_auth.h"
#include "ip_lookup.h"
#include "ip_dstlist.h"

#include "md5.h"

/*
 * From Solaris <inet/ip.h>, except HP-UX uses int.
 */
typedef struct ipparam_s {
	int	ip_param_min;
	int	ip_param_max;
	int	ip_param_value;
	char	*ip_param_name;
} ipparam_t;
extern	ipparam_t	*ip_param_arr;

#undef	IPFDEBUG
extern	int	ipf_flags, ipf_active;
extern	struct	callout	*ipf_timer_id;

static	int	ipf_send_ip(fr_info_t *, mblk_t *);
ipfmutex_t	ipl_mutex, ipf_auth_mx, ipf_rw, ipf_stinsert;
ipfmutex_t	ipf_nat_new, ipf_natio, ipf_timeoutlock;
ipfrwlock_t	ipf_mutex, ipf_global, ipf_ipidfrag, ipf_frcache, ipf_tokens;
ipfrwlock_t	ipf_frag, ipf_state, ipf_nat, ipf_natfrag, ipf_authlk;
int		*ip_ttl_ptr;
int		*ip_mtudisc;
int		*ip_forwarding;

static	u_short	ipid = 0;

int
ipfdetach()
{

	if (ipf_control_forwarding & 2)
		ip_forwarding = 0;
#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "ipfdetach()\n");
#endif
	ipf_deinitialise();

	(void) frflush(IPL_LOGIPF, FR_INQUE|FR_OUTQUE|FR_INACTIVE);
	(void) frflush(IPL_LOGIPF, FR_INQUE|FR_OUTQUE);

	RW_DESTROY(&ipf_tokens);
	RW_DESTROY(&ipf_ipidfrag);
	MUTEX_DESTROY(&ipf_timeoutlock);
	MUTEX_DESTROY(&ipf_rw);

	return 0;
}


int
ipfattach __P((void))
{
	int i;

#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "ipfattach()\n");
#endif
	bzero((char *)ipf_cache, sizeof(ipf_cache));
	MUTEX_INIT(&ipf_rw, "ipf_rw");
	MUTEX_INIT(&ipf_timeoutlock, "ipf_timeoutlock");
	RWLOCK_INIT(&ipf_ipidfrag, "ipf IP NAT-Frag rwlock");
	RWLOCK_INIT(&ipf_tokens, "ipf token rwlock");

	if (ipf_initialise() < 0)
		return -1;

	/*
	 * XXX - There is no terminator for this array, so it is not possible
	 * to tell if what we are looking for is missing and go off the end
	 * of the array.
	 */
	for (i = 0; ; i++) {
		if (!strcmp(ip_param_arr[i].ip_param_name, "ip_def_ttl")) {
			ip_ttl_ptr = &ip_param_arr[i].ip_param_value;
		} else if (!strcmp(ip_param_arr[i].ip_param_name,
			    "ip_pmtu_strategy")) {
			ip_mtudisc = &ip_param_arr[i].ip_param_value;
		} else if (!strcmp(ip_param_arr[i].ip_param_name,
			    "ip_forwarding")) {
			ip_forwarding = &ip_param_arr[i].ip_param_value;
		}

		if (ip_mtudisc != NULL && ip_ttl_ptr != NULL &&
		    ip_forwarding != NULL)
			break;
	}

#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "ipfattach() - success!\n");
#endif
	if (ipf_control_forwarding & 1)
		*ip_forwarding = 1;

	ipid = 0;

	return 0;
}


/*
 * Filter ioctl interface.
 */
int
ipfioctl(dev, cmd, data, flags)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flags;
{
	int error = 0;
	minor_t unit;

#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "ipfioctl(%x,%x,%x,%x)\n",
		dev, cmd, data, flags);
#endif

	unit = getminor(dev);
	if (IPL_LOGMAX < unit)
		return ENXIO;

	if (ipf_running <= 0) {
		if (unit != IPL_LOGIPF && cmd != SIOCIPFINTERROR)
			return EIO;
		if (cmd != SIOCIPFGETNEXT && cmd != SIOCIPFGET &&
		    cmd != SIOCIPFSET && cmd != SIOCFRENB &&
		    cmd != SIOCGETFS && cmd != SIOCGETFF &&
		    cmd != SIOCIPFINTERROR)
			return EIO;
	}

	error = ipf_ioctlswitch(unit, data, cmd, flags, curproc->p_uid,
				curproc);
	if (error != -1) {
		return error;
	}

	return error;
}


void *
get_unit(name, family)
	char *name;
	int family;
{
	size_t len = strlen(name) + 1;	/* includes \0 */
	qif_t *qf;
	int sap;

	if (family == 4)
		sap = 0x0800;
	else if (family == 6)
		return NULL;
	spinlock(&pfil_rw);
	qf = qif_iflookup(name, sap);
	spinunlock(&pfil_rw);
	return qf;
}


/*
 * ipf_send_reset - this could conceivably be a call to tcp_respond(), but that
 * requires a large amount of setting up and isn't any more efficient.
 */
int
ipf_send_reset(fin)
	fr_info_t *fin;
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
	if ((m = (mblk_t *)allocb(hlen + 16, BPRI_HI)) == NULL)
		return -1;

	m->b_rptr += 16;
	MTYPE(m) = M_DATA;
	m->b_wptr = m->b_rptr + hlen;
	bzero((char *)m->b_rptr, hlen);
	ip = (ip_t *)m->b_rptr;
	bzero((char *)ip, hlen);
	ip->ip_v = fin->fin_v;
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

	/*
	 * This is to get around a bug in the Solaris 2.4/2.5 TCP checksum
	 * computation that is done by their put routine.
	 */
#ifdef	USE_INET6
	if (fin->fin_v == 6) {
		ip6 = (ip6_t *)m->b_rptr;
		ip6->ip6_flow = ((ip6_t *)fin->fin_ip)->ip6_flow;
		ip6->ip6_src = fin->fin_dst6;
		ip6->ip6_dst = fin->fin_src6;
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
		tcp2->th_sum = ipf_cksum(m, ip, IPPROTO_TCP, tcp2,
					 ntohs(ip->ip_len));
	}
	return ipf_send_ip(fin, m);
}


/*
 * On input, ip_len is in network byte order.
 */
static int
ipf_send_ip(fr_info_t *fin, mblk_t *m)
{
	int i;

	RWLOCK_EXIT(&ipf_global);
#ifdef	USE_INET6
	if (fin->fin_v == 6) {
		ip6_t *ip6;

		ip6 = (ip6_t *)m->b_rptr;
		ip6->ip6_vfc = 0x60;
		ip6->ip6_hlim = 127;
	} else
#endif
	{
		ip_t *ip;

		ip = (ip_t *)m->b_rptr;
		ip->ip_v = IPVERSION;
		ip->ip_ttl = *ip_ttl_ptr;
		ip->ip_off = htons(*ip_mtudisc == 1 ? IP_DF : 0);
		ip->ip_sum = ipf_cksum((u_short *)ip, sizeof(*ip));
	}
	i = pfil_sendbuf(m);
	READ_ENTER(&ipf_global);
	return i;
}


int
ipf_send_icmp_err(type, fin, dst)
	int type;
	fr_info_t *fin;
	int dst;
{
	struct in_addr dst4;
	struct icmp *icmp;
	mblk_t *m, *mb;
	int hlen, code;
	qpktinfo_t *qpi;
	i6addr_t dst6;
	u_short sz;
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
	mb = fin->fin_qfm;

#ifdef	USE_INET6
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
	if ((m = (mblk_t *)allocb((size_t)sz + 16, BPRI_HI)) == NULL)
		return -1;
	MTYPE(m) = M_DATA;
	m->b_rptr += 16;
	m->b_wptr = m->b_rptr + sz;
	bzero((char *)m->b_rptr, (size_t)sz);
	ip = (ip_t *)m->b_rptr;
	ip->ip_v = fin->fin_v;
	icmp = (struct icmp *)(m->b_rptr + hlen);
	icmp->icmp_type = type;
	icmp->icmp_code = code;

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
		ip6->ip6_src = dst6;
		ip6->ip6_dst = fin->fin_src6;
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


/*
 * return the first IP Address associated with an interface
 */
int
ipf_ifpaddr(softc, v, atype, qifptr, inp, inpmask)
	ipf_main_softc_t *softc;
	int v, atype;
	void *qifptr;
	i6addr_t *inp, *inpmask;
{
#ifdef	USE_INET6
	struct sockaddr_in6 sin6, mask6;
#endif
	struct sockaddr_in sin, mask;
	qif_t *qif = qifptr;

	if ((qifptr == NULL) || (qifptr == (void *)-1))
		return -1;

#ifdef	USE_INET6
	if (v == 6) {
		return ENOTSUP;
	}
#endif

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


#ifdef	IPL_SELECT
extern	iplog_select_t	iplog_ss[IPL_LOGSIZE];
extern	int		selwait;

/*
 * iplog_input_ready and ipflog_select are both submissions from HP.
 */
void
iplog_input_ready(unit)
	minor_t unit;
{
	if (iplog_ss[unit].read_waiter) {
		selwakeup(iplog_ss[unit].read_waiter,
			  iplog_ss[unit].state & READ_COLLISION);
		iplog_ss[unit].read_waiter = 0;
		iplog_ss[unit].state &= READ_COLLISION;
	}
}


int
iplselect(unit, flag)
	minor_t unit;
	int flag;
{
	kthread_t * t;

	MUTEX_ENTER(&ipl_mutex);
	switch (flag)
	{
	case FREAD:
		if (softc->ipf_iplused[unit]) {
			MUTEX_EXIT(&ipl_mutex);
			return 1;
		}
		if ((t = iplog_ss[unit].read_waiter) &&
# if HPUXREV >= 1111
		    waiting_in_select(t)
# else
		    (kt_wchan(t) == (caddr_t)&selwait)
# endif
		    ) {
			iplog_ss[unit].state |= READ_COLLISION;
		} else {
			iplog_ss[unit].read_waiter = u.u_kthreadp;
		}
		break;
	}
	MUTEX_EXIT(&ipl_mutex);
	return 0;
}
#endif


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
	return newiss;
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
/* We assume that 'xmin' is a pointer to a buffer that is part of the chain */
/* of buffers that starts at *fin->fin_mp.                                  */
/* ------------------------------------------------------------------------ */
void *
ipf_pullup(xmin, fin, len)
	mb_t *xmin;
	fr_info_t *fin;
	int len;
{
	qpktinfo_t *qpi = fin->fin_qpi;
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
		int inc = 0;

		if (ipoff > 0) {
			if ((ipoff & 3) != 0) {
				inc = 4 - (ipoff & 3);
				if (m->b_rptr - inc >= m->b_datap->db_base)
					m->b_rptr -= inc;
				else
					inc = 0;
			}
		}
		m = msgpullup(xmin, len + ipoff + inc);
		if (m == NULL) {
			FREE_MB_T(*fin->fin_mp);
			*fin->fin_mp = NULL;
			fin->fin_m = NULL;
			return NULL;
		}

		/*
		 * Because msgpullup allocates a new mblk, we need to delink
		 * (and free) the old one and link on the new one.
		 */
		if (xmin == *fin->fin_mp) {	/* easy case 1st */
			FREE_MB_T(*fin->fin_mp);
			*fin->fin_mp = m;
		} else {
			mb_t *m2;

			for (m2 = *fin->fin_mp; m2 != NULL; m2 = m2->b_next)
				if (m2->b_next == xmin)
					break;
			if (m2 == NULL) {
				FREE_MB_T(*fin->fin_mp);
				FREE_MB_T(m);
				return NULL;
			}
			FREE_MB_T(xmin);
			m2->b_next = m;
		}

		fin->fin_m = m;
		m->b_rptr += inc;
		ip = MTOD(m, char *) + ipoff;
		qpi->qpi_data = ip;

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


/*
 * m0 - pointer to mbuf where the IP packet starts
 * mpp - pointer to the mbuf pointer that is the start of the mbuf chain
 */
int
ipf_fastroute(mb, mpp, fin, fdp)
	mblk_t *mb, **mpp;
	fr_info_t *fin;
	frdest_t *fdp;
{
#ifdef	USE_INET6
	ip6_t *ip6 = (ip6_t *)fin->fin_ip;
#endif
	struct in_addr dst, src;
	ifinfo_t *ifp, *sifp;
	mblk_t *mp, **mps;
	size_t hlen = 0;
	qpktinfo_t *qpi;
	frdest_t node;
	frentry_t *fr;
	irinfo_t ir;
	queue_t *q;
	u_char *s;
	ip_t *ip;
	int p, i;

	fr = fin->fin_fr;
	ip = fin->fin_ip;
	qpi = fin->fin_qpi;
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
		mp = *mpp;
		(void) unlinkb(mp);
		mp = (*mpp)->b_cont;
		(*mpp)->b_cont = NULL;
		(*mpp)->b_prev = NULL;
		freemsg(*mpp);
		*mpp = mp;
	}

	if ((fr != NULL) && !(fr->fr_flags & FR_KEEPSTATE) && (fdp != NULL) &&
	    (fdp->fd_type == FRD_DSTLIST)) {
		if (ipf_dstlist_select_node(fin, fdp->fd_ptr, NULL, &node) == 0)
			fdp = &node;
	}
	ifp = (ifinfo_t *)fdp->fd_ptr;
	if (fdp && fdp->fd_ip.s_addr)
		dst = fdp->fd_ip;
	else
		dst.s_addr = fin->fin_fi.fi_daddr;

	src.s_addr = 0;
	bzero((char *)&ir, sizeof(ir));
	i = ir_lookup(&ir, &dst.s_addr, &src.s_addr,
		      ~(IR_ROUTE|IR_ROUTE_ASSOC|IR_ROUTE_REDIRECT), 4);
	mp = ir.ir_ll_hdr_mp;
	hlen = ir.ir_ll_hdr_length;
	if (!mp || !hlen || (i == 0))
		goto bad_fastroute;

	if ((ifp || (fr && (fr->fr_flags & FR_FASTROUTE)))) {
		if (ifp && (ir_to_ill(&ir) != ifp))
			goto bad_fastroute;

		if (fin->fin_out == 0) {
			sifp = fin->fin_ifp;
			fin->fin_ifp = ir_to_ill(&ir);
			fin->fin_out = 1;
			(void) ipf_acctpkt(fin, NULL);
			fin->fin_fr = NULL;
			if (!fr || !(fr->fr_flags & FR_RETMASK)) {
				u_32_t pass;

				fin->fin_flx &= ~FI_STATE;
				(void) ipf_state_check(fin, &pass);
			}

			switch (ipf_nat_checkout(fin, NULL))
			{
			case 0 :
				break;
			case 1 :
				break;
			case -1 :
				goto bad_fastroute;
				break;
			}

			fin->fin_out = 0;
			fin->fin_ifp = sifp;
		}

		s = mb->b_rptr;
		if ((hlen && (s - mb->b_datap->db_base) >= hlen)) {
			s -= hlen;
			mb->b_rptr = (u_char *)s;
			bcopy((char *)mp->b_rptr, (char *)s, hlen);
			freeb(mp);
			mp = NULL;
		} else {
			mblk_t	*mp2;

			linkb(mp, *mpp);
			*mpp = mp;
			mb = mp;
			mp = NULL;
		}

		q = NULL;
		if (ir.ir_stq)
			q = ir.ir_stq;
		else if (ir.ir_rfq)
			q = WR(ir.ir_rfq);
		if (q)
			q = q->q_next;
		if (q) {
			mb->b_prev = NULL;
			RWLOCK_EXIT(&ipf_global);
			putnext(q, mb);
			READ_ENTER(&ipf_global);
			ipf_frouteok[0]++;
			return 0;
		}
	}
bad_fastroute:
	if (mp)
		freeb(mp);
	mb->b_prev = NULL;
	freemsg(*mpp);
	ipf_frouteok[1]++;
	return -1;
}


int
ipf_verifysrc(fin)
	fr_info_t *fin;
{
	struct in_addr ips, ipa;
	irinfo_t ir, *dir, *gw;
	qif_t *qif;
	int i;

	ips.s_addr = 0;
	ipa = fin->fin_src;

	if (ir_lookup(&ir, (uint32_t *)&ipa, (uint32_t *)&ips, 0, 4) == 0)
		return 1;
	i = (ir_to_ill(&ir) == fin->fin_ifp);
	if (ir.ir_ll_hdr_mp)
		freeb(ir.ir_ll_hdr_mp);
	return i;
}
