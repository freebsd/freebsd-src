/*
 * Copyright (C) 1995-1998 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#if !defined(lint)
static const char sccsid[] = "@(#)ip_state.c	1.8 6/5/96 (C) 1993-1995 Darren Reed";
/*static const char rcsid[] = "@(#)$Id: ip_state.c,v 2.3.2.9 1999/10/21 14:31:09 darrenr Exp $";*/
static const char rcsid[] = "@(#)$FreeBSD$";
#endif

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#if defined(__NetBSD__) && (NetBSD >= 199905) && !defined(IPFILTER_LKM) && \
    defined(_KERNEL)
# include "opt_ipfilter_log.h"
#endif
#if !defined(_KERNEL) && !defined(KERNEL) && !defined(__KERNEL__)
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
#else
# ifdef linux
#  include <linux/kernel.h>
#  include <linux/module.h>
# endif
#endif
#if defined(KERNEL) && (__FreeBSD_version >= 220000)
# include <sys/filio.h>
# include <sys/fcntl.h>
# if (__FreeBSD_version >= 300000) && !defined(IPFILTER_LKM)
#  include "opt_ipfilter.h"
# endif
#else
# include <sys/ioctl.h>
#endif
#include <sys/time.h>
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

#include <net/if.h>
#ifdef sun
# include <net/af.h>
#endif
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#ifndef linux
# include <netinet/ip_var.h>
# include <netinet/tcp_fsm.h>
#endif
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include "netinet/ip_compat.h"
#include <netinet/tcpip.h>
#include "netinet/ip_fil.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_frag.h"
#include "netinet/ip_proxy.h"
#include "netinet/ip_state.h"
#if (__FreeBSD_version >= 300000)
# include <sys/malloc.h>
# if (defined(_KERNEL) || defined(KERNEL)) && !defined(IPFILTER_LKM)
#  include <sys/libkern.h>
#  include <sys/systm.h>
# endif
#endif

#ifndef	MIN
# define	MIN(a,b)	(((a)<(b))?(a):(b))
#endif

#define	TCP_CLOSE	(TH_FIN|TH_RST)

static ipstate_t **ips_table = NULL;
static int	ips_num = 0;
static ips_stat_t ips_stats;
#if	(SOLARIS || defined(__sgi)) && defined(_KERNEL)
extern	KRWLOCK_T	ipf_state, ipf_mutex;
extern	kmutex_t	ipf_rw;
#endif

static int fr_matchsrcdst __P((ipstate_t *, struct in_addr, struct in_addr,
			       fr_info_t *, tcphdr_t *));
static frentry_t *fr_checkicmpmatchingstate __P((ip_t *, fr_info_t *));
static int fr_state_flush __P((int));
static ips_stat_t *fr_statetstats __P((void));
static void fr_delstate __P((ipstate_t *));


#define	FIVE_DAYS	(2 * 5 * 86400)	/* 5 days: half closed session */

#define	TCP_MSL	240			/* 2 minutes */
u_long	fr_tcpidletimeout = FIVE_DAYS,
	fr_tcpclosewait = 2 * TCP_MSL,
	fr_tcplastack = 2 * TCP_MSL,
	fr_tcptimeout = 2 * TCP_MSL,
	fr_tcpclosed = 1,
	fr_udptimeout = 240,
	fr_icmptimeout = 120;
int	fr_statemax = IPSTATE_MAX,
	fr_statesize = IPSTATE_SIZE;
int	fr_state_doflush = 0;


int fr_stateinit()
{
	KMALLOCS(ips_table, ipstate_t **, fr_statesize * sizeof(ipstate_t *));
	if (ips_table != NULL)
		bzero((char *)ips_table, fr_statesize * sizeof(ipstate_t *));
	else
		return -1;
	return 0;
}


static ips_stat_t *fr_statetstats()
{
	ips_stats.iss_active = ips_num;
	ips_stats.iss_table = ips_table;
	return &ips_stats;
}


/*
 * flush state tables.  two actions currently defined:
 * which == 0 : flush all state table entries
 * which == 1 : flush TCP connections which have started to close but are
 *              stuck for some reason.
 */
static int fr_state_flush(which)
int which;
{
	register int i;
	register ipstate_t *is, **isp;
#if defined(_KERNEL) && !SOLARIS
	int s;
#endif
	int delete, removed = 0;

	SPL_NET(s);
	WRITE_ENTER(&ipf_state);
	for (i = fr_statesize - 1; i >= 0; i--)
		for (isp = &ips_table[i]; (is = *isp); ) {
			delete = 0;

			switch (which)
			{
			case 0 :
				delete = 1;
				break;
			case 1 :
				if ((is->is_p == IPPROTO_TCP) &&
				    (((is->is_state[0] <= TCPS_ESTABLISHED) &&
				      (is->is_state[1] > TCPS_ESTABLISHED)) ||
				     ((is->is_state[1] <= TCPS_ESTABLISHED) &&
				      (is->is_state[0] > TCPS_ESTABLISHED))))
					delete = 1;
				break;
			}

			if (delete) {
				*isp = is->is_next;
				if (is->is_p == IPPROTO_TCP)
					ips_stats.iss_fin++;
				else
					ips_stats.iss_expire++;
				if (ips_table[i] == NULL)
					ips_stats.iss_inuse--;
#ifdef	IPFILTER_LOG
				ipstate_log(is, ISL_FLUSH);
#endif
				fr_delstate(is);
				ips_num--;
				removed++;
			} else
				isp = &is->is_next;
		}
	if (fr_state_doflush) {
		(void) fr_state_flush(1);
		fr_state_doflush = 0;
	}
	RWLOCK_EXIT(&ipf_state);
	SPL_X(s);
	return removed;
}


int fr_state_ioctl(data, cmd, mode)
caddr_t data;
#if defined(__NetBSD__) || defined(__OpenBSD__)
u_long cmd;
#else
int cmd;
#endif
int mode;
{
	int	arg, ret, error = 0;

	switch (cmd)
	{
	case SIOCIPFFL :
		IRCOPY(data, (caddr_t)&arg, sizeof(arg));
		if (arg == 0 || arg == 1) {
			ret = fr_state_flush(arg);
			IWCOPY((caddr_t)&ret, data, sizeof(ret));
		} else
			error = EINVAL;
		break;
	case SIOCGIPST :
		IWCOPY((caddr_t)fr_statetstats(), data, sizeof(ips_stat_t));
		break;
	case FIONREAD :
#ifdef	IPFILTER_LOG
		IWCOPY((caddr_t)&iplused[IPL_LOGSTATE], (caddr_t)data,
		       sizeof(iplused[IPL_LOGSTATE]));
#endif
		break;
	default :
		error = EINVAL;
		break;
	}
	return error;
}


/*
 * Create a new ipstate structure and hang it off the hash table.
 */
ipstate_t *fr_addstate(ip, fin, flags)
ip_t *ip;
fr_info_t *fin;
u_int flags;
{
	register ipstate_t *is;
	register u_int hv;
	ipstate_t ips;
	u_int pass;

	if ((ip->ip_off & IP_OFFMASK) || (fin->fin_fi.fi_fl & FI_SHORT))
		return NULL;
	if (ips_num == fr_statemax) {
		ips_stats.iss_max++;
		fr_state_doflush = 1;
		return NULL;
	}
	is = &ips;
	bzero((char *)is, sizeof(*is));
	ips.is_age = 1;
	ips.is_state[0] = 0;
	ips.is_state[1] = 0;
	/*
	 * Copy and calculate...
	 */
	hv = (is->is_p = ip->ip_p);
	hv += (is->is_src.s_addr = ip->ip_src.s_addr);
	hv += (is->is_dst.s_addr = ip->ip_dst.s_addr);

	switch (ip->ip_p)
	{
	case IPPROTO_ICMP :
	    {
		struct icmp *ic = (struct icmp *)fin->fin_dp;

		switch (ic->icmp_type)
		{
		case ICMP_ECHO :
			is->is_icmp.ics_type = ICMP_ECHOREPLY;	/* XXX */
			hv += (is->is_icmp.ics_id = ic->icmp_id);
			hv += (is->is_icmp.ics_seq = ic->icmp_seq);
			break;
		case ICMP_TSTAMP :
		case ICMP_IREQ :
		case ICMP_MASKREQ :
			is->is_icmp.ics_type = ic->icmp_type + 1;
			break;
		default :
			return NULL;
		}
		ATOMIC_INC(ips_stats.iss_icmp);
		is->is_age = fr_icmptimeout;
		break;
	    }
	case IPPROTO_TCP :
	    {
		register tcphdr_t *tcp = (tcphdr_t *)fin->fin_dp;

		/*
		 * The endian of the ports doesn't matter, but the ack and
		 * sequence numbers do as we do mathematics on them later.
		 */
		is->is_dport = tcp->th_dport;
		is->is_sport = tcp->th_sport;
		if ((flags & (FI_W_DPORT|FI_W_SPORT)) == 0) {
			hv += tcp->th_dport;
			hv += tcp->th_sport;
		}
		if (tcp->th_seq != 0) {
			is->is_send = ntohl(tcp->th_seq) + ip->ip_len -
				      fin->fin_hlen - (tcp->th_off << 2) +
				      ((tcp->th_flags & TH_SYN) ? 1 : 0) +
				      ((tcp->th_flags & TH_FIN) ? 1 : 0);
			is->is_maxsend = is->is_send + 1;
		}
		is->is_dend = 0;
		is->is_maxswin = ntohs(tcp->th_win);
		if (is->is_maxswin == 0)
			is->is_maxswin = 1;
		/*
		 * If we're creating state for a starting connection, start the
		 * timer on it as we'll never see an error if it fails to
		 * connect.
		 */
		MUTEX_ENTER(&ipf_rw);
		ips_stats.iss_tcp++;
		fr_tcp_age(&is->is_age, is->is_state, ip, fin,
			   tcp->th_sport == is->is_sport);
		MUTEX_EXIT(&ipf_rw);
		break;
	    }
	case IPPROTO_UDP :
	    {
		register tcphdr_t *tcp = (tcphdr_t *)fin->fin_dp;

		if ((flags & (FI_W_DPORT|FI_W_SPORT)) == 0) {
			hv += (is->is_dport = tcp->th_dport);
			hv += (is->is_sport = tcp->th_sport);
		}
		ATOMIC_INC(ips_stats.iss_udp);
		is->is_age = fr_udptimeout;
		break;
	    }
	default :
		return NULL;
	}

	KMALLOC(is, ipstate_t *);
	if (is == NULL) {
		ATOMIC_INC(ips_stats.iss_nomem);
		return NULL;
	}
	bcopy((char *)&ips, (char *)is, sizeof(*is));
	hv %= fr_statesize;
	RW_UPGRADE(&ipf_mutex);
	is->is_rule = fin->fin_fr;
	if (is->is_rule != NULL) {
		is->is_rule->fr_ref++;
		pass = is->is_rule->fr_flags;
	} else
		pass = fr_flags;
	MUTEX_DOWNGRADE(&ipf_mutex);
	WRITE_ENTER(&ipf_state);

	is->is_rout = pass & FR_OUTQUE ? 1 : 0;
	is->is_pass = pass;
	is->is_pkts = 1;
	is->is_bytes = ip->ip_len;
	/*
	 * We want to check everything that is a property of this packet,
	 * but we don't (automatically) care about it's fragment status as
	 * this may change.
	 */
	is->is_opt = fin->fin_fi.fi_optmsk;
	is->is_optmsk = 0xffffffff;
	is->is_sec = fin->fin_fi.fi_secmsk;
	is->is_secmsk = 0xffff;
	is->is_auth = fin->fin_fi.fi_auth;
	is->is_authmsk = 0xffff;
	is->is_flags = fin->fin_fi.fi_fl & FI_CMP;
	is->is_flags |= FI_CMP << 4;
	is->is_flags |= flags & (FI_W_DPORT|FI_W_SPORT);
	/*
	 * add into table.
	 */
	is->is_next = ips_table[hv];
	ips_table[hv] = is;
	if (is->is_next == NULL)
		ips_stats.iss_inuse++;
	if (fin->fin_out) {
		is->is_ifpin = NULL;
		is->is_ifpout = fin->fin_ifp;
	} else {
		is->is_ifpin = fin->fin_ifp;
		is->is_ifpout = NULL;
	}
	if (pass & FR_LOGFIRST)
		is->is_pass &= ~(FR_LOGFIRST|FR_LOG);
	ATOMIC_INC(ips_num);
#ifdef	IPFILTER_LOG
	ipstate_log(is, ISL_NEW);
#endif
	RWLOCK_EXIT(&ipf_state);
	fin->fin_rev = (is->is_dst.s_addr != ip->ip_dst.s_addr);
	if (fin->fin_fi.fi_fl & FI_FRAG)
		ipfr_newfrag(ip, fin, pass ^ FR_KEEPSTATE);
	return is;
}



/*
 * check to see if a packet with TCP headers fits within the TCP window.
 * change timeout depending on whether new packet is a SYN-ACK returning for a
 * SYN or a RST or FIN which indicate time to close up shop.
 */
int fr_tcpstate(is, fin, ip, tcp)
register ipstate_t *is;
fr_info_t *fin;
ip_t *ip;
tcphdr_t *tcp;
{
	register tcp_seq seq, ack, end;
	register int ackskew;
	tcpdata_t  *fdata, *tdata;
	u_short	win, maxwin;
	int ret = 0;
	int source;

	/*
	 * Find difference between last checked packet and this packet.
	 */
	source = (ip->ip_src.s_addr == is->is_src.s_addr);
	fdata = &is->is_tcp.ts_data[!source];
	tdata = &is->is_tcp.ts_data[source];
	seq = ntohl(tcp->th_seq);
	ack = ntohl(tcp->th_ack);
	win = ntohs(tcp->th_win);
	end = seq + ip->ip_len - fin->fin_hlen - (tcp->th_off << 2) +
	       ((tcp->th_flags & TH_SYN) ? 1 : 0) +
	       ((tcp->th_flags & TH_FIN) ? 1 : 0);      

	if (fdata->td_end == 0) {
		/*
		 * Must be a (outgoing) SYN-ACK in reply to a SYN.
		 */
		fdata->td_end = end;
		fdata->td_maxwin = 1;
		fdata->td_maxend = end + 1;
	}

	if (!(tcp->th_flags & TH_ACK)) {  /* Pretend an ack was sent */
		ack = tdata->td_end;
		win = 1;
	} else if (((tcp->th_flags & (TH_ACK|TH_RST)) == (TH_ACK|TH_RST)) &&
		   (ack == 0)) {
		/* gross hack to get around certain broken tcp stacks */
		ack = tdata->td_end;
	}

	if (seq == end)
		seq = end = fdata->td_end;

	maxwin = tdata->td_maxwin;
	ackskew = tdata->td_end - ack;

#define	SEQ_GE(a,b)	((int)((a) - (b)) >= 0)
#define	SEQ_GT(a,b)	((int)((a) - (b)) > 0)
	if ((SEQ_GE(fdata->td_maxend, end)) &&
	    (SEQ_GE(seq + maxwin, fdata->td_end - maxwin)) && 
/* XXX what about big packets */
#define MAXACKWINDOW 66000
	    (ackskew >= -MAXACKWINDOW) &&
	    (ackskew <= MAXACKWINDOW)) {
		/* if ackskew < 0 then this should be due to fragented
		 * packets. There is no way to know the length of the
		 * total packet in advance.
		 * We do know the total length from the fragment cache though.
		 * Note however that there might be more sessions with
		 * exactly the same source and destination paramters in the
		 * state cache (and source and destination is the only stuff
		 * that is saved in the fragment cache). Note further that
		 * some TCP connections in the state cache are hashed with
		 * sport and dport as well which makes it not worthwhile to
		 * look for them.
		 * Thus, when ackskew is negative but still seems to belong
		 * to this session, we bump up the destinations end value.
		 */
		if (ackskew < 0)
			tdata->td_end = ack;

		/* update max window seen */
		if (fdata->td_maxwin < win)
			fdata->td_maxwin = win;
		if (SEQ_GT(end, fdata->td_end))
			fdata->td_end = end;
		if (SEQ_GE(ack + win, tdata->td_maxend)) {
			tdata->td_maxend = ack + win;
			if (win == 0)
				tdata->td_maxend++;
		}

		ATOMIC_INC(ips_stats.iss_hits);
		is->is_pkts++;
		is->is_bytes += ip->ip_len;
		/*
		 * Nearing end of connection, start timeout.
		 */
		MUTEX_ENTER(&ipf_rw);
		fr_tcp_age(&is->is_age, is->is_state, ip, fin, source);
		MUTEX_EXIT(&ipf_rw);
		ret = 1;
	}
	return ret;
}


static int fr_matchsrcdst(is, src, dst, fin, tcp)
ipstate_t *is;
struct in_addr src, dst;
fr_info_t *fin;
tcphdr_t *tcp;
{
	int ret = 0, rev, out, flags;
	u_short sp, dp;
	void *ifp;

	rev = fin->fin_rev = (is->is_dst.s_addr != dst.s_addr);
	ifp = fin->fin_ifp;
	out = fin->fin_out;

	if (tcp != NULL) {
		flags = is->is_flags;
		sp = tcp->th_sport;
		dp = tcp->th_dport;
	} else {
		flags = 0;
		sp = 0;
		dp = 0;
	}

	if (rev == 0) {
		if (!out) {
			if (is->is_ifpin == ifp)
				ret = 1;
		} else {
			if (is->is_ifpout == NULL || is->is_ifpout == ifp)
				ret = 1;
		}
	} else {
		if (out) {
			if (is->is_ifpin == ifp)
				ret = 1;
		} else {
			if (is->is_ifpout == NULL || is->is_ifpout == ifp)
				ret = 1;
		}
	}
	if (ret == 0)
		return 0;
	ret = 0;

	if (rev == 0) {
		if ((is->is_dst.s_addr == dst.s_addr) &&
		    (is->is_src.s_addr == src.s_addr) &&
		    (!tcp || ((sp == is->is_sport || flags & FI_W_SPORT) &&
		     (dp == is->is_dport || flags & FI_W_DPORT)))) {
			ret = 1;
		}
	} else {
		if ((is->is_dst.s_addr == src.s_addr) &&
		    (is->is_src.s_addr == dst.s_addr) &&
		    (!tcp || ((sp == is->is_dport || flags & FI_W_DPORT) &&
		     (dp == is->is_sport || flags & FI_W_SPORT)))) {
			ret = 1;
		}
	}
	if (ret == 0)
		return 0;

	/*
	 * Whether or not this should be here, is questionable, but the aim
	 * is to get this out of the main line.
	 */
	if (tcp == NULL)
		flags = is->is_flags & (FI_CMP|(FI_CMP<<4));

	if (((fin->fin_fi.fi_fl & (flags >> 4)) != (flags & FI_CMP)) ||
	    ((fin->fin_fi.fi_optmsk & is->is_optmsk) != is->is_opt) ||
	    ((fin->fin_fi.fi_secmsk & is->is_secmsk) != is->is_sec) ||
	    ((fin->fin_fi.fi_auth & is->is_authmsk) != is->is_auth))
		return 0;

	if ((flags & (FI_W_SPORT|FI_W_DPORT))) {
		if ((flags & FI_W_SPORT) != 0) {
			if (rev == 0) {
				is->is_sport = sp;
				is->is_send = htonl(tcp->th_seq);
			} else {
				is->is_sport = dp;
				is->is_send = htonl(tcp->th_ack);
			}
			is->is_maxsend = is->is_send + 1;
		} else if ((flags & FI_W_DPORT) != 0) {
			if (rev == 0) {
				is->is_dport = dp;
				is->is_dend = htonl(tcp->th_ack);
			} else {
				is->is_dport = sp;
				is->is_dend = htonl(tcp->th_seq);
			}
			is->is_maxdend = is->is_dend + 1;
		}
		is->is_flags &= ~(FI_W_SPORT|FI_W_DPORT);
	}

	if (!rev) {
		if (out && (out == is->is_rout)) {
			if (!is->is_ifpout)
				is->is_ifpout = ifp;
		} else {
			if (!is->is_ifpin)
				is->is_ifpin = ifp;
		}
	} else {
		if (!out && (out != is->is_rout)) {
			if (!is->is_ifpin)
				is->is_ifpin = ifp;
		} else {
			if (!is->is_ifpout)
				is->is_ifpout = ifp;
		}
	}
	return 1;
}

frentry_t *fr_checkicmpmatchingstate(ip, fin)
ip_t *ip;
fr_info_t *fin;
{
	register struct in_addr	dst, src;
	register ipstate_t *is, **isp;
	register u_short sport, dport;
	register u_char	pr;
	struct icmp *ic;
	fr_info_t ofin;
	u_int hv, dest;
	tcphdr_t *tcp;
	frentry_t *fr;
	ip_t *oip;
	int type;

	/* 
	 * Does it at least have the return (basic) IP header ? 
	 * Only a basic IP header (no options) should be with
	 * an ICMP error header.
	 */
	if ((ip->ip_hl != 5) || (ip->ip_len < ICMPERR_MINPKTLEN))
		return NULL;
	ic = (struct icmp *)((char *)ip + fin->fin_hlen);
	type = ic->icmp_type;
	/*
	 * If it's not an error type, then return
	 */
	if ((type != ICMP_UNREACH) && (type != ICMP_SOURCEQUENCH) &&
    	    (type != ICMP_REDIRECT) && (type != ICMP_TIMXCEED) &&
    	    (type != ICMP_PARAMPROB))
		return NULL;

	oip = (ip_t *)((char *)fin->fin_dp + ICMPERR_ICMPHLEN);
	if (ip->ip_len < ICMPERR_MAXPKTLEN + ((oip->ip_hl - 5) << 2))
		return NULL;
	if ((oip->ip_p != IPPROTO_TCP) && (oip->ip_p != IPPROTO_UDP))
		return NULL;

	tcp = (tcphdr_t *)((char *)oip + (oip->ip_hl << 2));
	dport = tcp->th_dport;
	sport = tcp->th_sport;

	hv = (pr = oip->ip_p);
	hv += (src.s_addr = oip->ip_src.s_addr);
	hv += (dst.s_addr = oip->ip_dst.s_addr);
	hv += dport;
	hv += sport;
	hv %= fr_statesize;
	/*
	 * we make an fin entry to be able to feed it to
	 * matchsrcdst note that not all fields are encessary
	 * but this is the cleanest way. Note further we fill
	 * in fin_mp such that if someone uses it we'll get
	 * a kernel panic. fr_matchsrcdst does not use this.
	 *
	 * watch out here, as ip is in host order and oip in network
	 * order. Any change we make must be undone afterwards.
	 */
	oip->ip_len = ntohs(oip->ip_len);
	fr_makefrip(oip->ip_hl << 2, oip, &ofin);
	oip->ip_len = htons(oip->ip_len);
	ofin.fin_ifp = fin->fin_ifp;
	ofin.fin_out = !fin->fin_out;
	ofin.fin_mp = NULL; /* if dereferenced, panic XXX */
	READ_ENTER(&ipf_state);
	for (isp = &ips_table[hv]; (is = *isp); isp = &is->is_next) {
		/*
		 * Only allow this icmp though if the
		 * encapsulated packet was allowed through the
		 * other way around. Note that the minimal amount
		 * of info present does not allow for checking against
		 * tcp internals such as seq and ack numbers.
		 */
		if ((is->is_p == pr) &&
		    fr_matchsrcdst(is, src, dst, &ofin, tcp)) {
			fr = is->is_rule;
			ips_stats.iss_hits++;
			/*
			 * we must swap src and dst here because the icmp
			 * comes the other way around
			 */
			dest = (is->is_dst.s_addr != src.s_addr);
			is->is_pkts++;
			is->is_bytes += ip->ip_len;     
			/*
			 * we deliberately do not touch the timeouts
			 * for the accompanying state table entry.
			 * It remains to be seen if that is correct. XXX
			 */
			RWLOCK_EXIT(&ipf_state);
			return fr;
		}
	}
	RWLOCK_EXIT(&ipf_state);
	return NULL;
}

/*
 * Check if a packet has a registered state.
 */
frentry_t *fr_checkstate(ip, fin)
ip_t *ip;
fr_info_t *fin;
{
	register struct in_addr dst, src;
	register ipstate_t *is, **isp;
	register u_char pr;
	u_int hv, hvm, hlen, tryagain, pass;
	struct icmp *ic;
	frentry_t *fr;
	tcphdr_t *tcp;

	if ((ip->ip_off & IP_OFFMASK) || (fin->fin_fi.fi_fl & FI_SHORT))
		return NULL;

	is = NULL;
	hlen = fin->fin_hlen;
	tcp = (tcphdr_t *)((char *)ip + hlen);
	ic = (struct icmp *)tcp;
	hv = (pr = ip->ip_p);
	hv += (src.s_addr = ip->ip_src.s_addr);
	hv += (dst.s_addr = ip->ip_dst.s_addr);

	/*
	 * Search the hash table for matching packet header info.
	 */
	switch (ip->ip_p)
	{
	case IPPROTO_ICMP :
		hv += ic->icmp_id;
		hv += ic->icmp_seq;
		hv %= fr_statesize;
		READ_ENTER(&ipf_state);
		for (isp = &ips_table[hv]; (is = *isp); isp = &is->is_next)
			if ((is->is_p == pr) &&
			    (ic->icmp_id == is->is_icmp.ics_id) &&
			    (ic->icmp_seq == is->is_icmp.ics_seq) &&
			    fr_matchsrcdst(is, src, dst, fin, NULL)) {
				if ((is->is_type == ICMP_ECHOREPLY) &&
				    (ic->icmp_type == ICMP_ECHO))
					;
				else if (is->is_type != ic->icmp_type)
					continue;
				is->is_age = fr_icmptimeout;
				break;
			}
		if (is != NULL)
			break;
		RWLOCK_EXIT(&ipf_state);
		/*
		 * No matching icmp state entry. Perhaps this is a
		 * response to another state entry.
		 */
		fr = fr_checkicmpmatchingstate(ip, fin);
		if (fr)
			return fr;
		break;
	case IPPROTO_TCP :
	    {
		register u_short dport = tcp->th_dport, sport = tcp->th_sport;

		tryagain = 0;
retry_tcp:
		hvm = hv % fr_statesize;
		WRITE_ENTER(&ipf_state);
		for (isp = &ips_table[hvm]; (is = *isp);
		     isp = &is->is_next)
			if ((is->is_p == pr) &&
			    fr_matchsrcdst(is, src, dst, fin, tcp)) {
				if (fr_tcpstate(is, fin, ip, tcp)) {
					break;
#ifndef	_KERNEL
					if (tcp->th_flags & TCP_CLOSE) {
						*isp = is->is_next;
						isp = &ips_table[hvm];
						if (ips_table[hvm] == NULL)
							ips_stats.iss_inuse--;
						fr_delstate(is);
						ips_num--;
					}
#endif
					break;
				}
				is = NULL;
				break;
			}
		if (is != NULL)
			break;
		RWLOCK_EXIT(&ipf_state);
		hv += dport;
		hv += sport;
		if (tryagain == 0) {
			tryagain = 1;
			goto retry_tcp;
		}
		break;
	    }
	case IPPROTO_UDP :
	    {
		register u_short dport = tcp->th_dport, sport = tcp->th_sport;

		tryagain = 0;
retry_udp:
		hvm = hv % fr_statesize;
		/*
		 * Nothing else to match on but ports. and IP#'s
		 */
		READ_ENTER(&ipf_state);
		for (is = ips_table[hvm]; is; is = is->is_next)
			if ((is->is_p == pr) &&
			    fr_matchsrcdst(is, src, dst, fin, tcp)) {
				is->is_age = fr_udptimeout;
				break;
			}
		if (is != NULL)
			break;
		RWLOCK_EXIT(&ipf_state);
		hv += dport;
		hv += sport;
		if (tryagain == 0) {
			tryagain = 1;
			goto retry_udp;
		}
		break;
	    }
	default :
		break;
	}
	if (is == NULL) {
		ATOMIC_INC(ips_stats.iss_miss);
		return NULL;
	}
	MUTEX_ENTER(&ipf_rw);
	is->is_bytes += ip->ip_len;
	ips_stats.iss_hits++;
	is->is_pkts++;
	MUTEX_EXIT(&ipf_rw);
	fr = is->is_rule;
	fin->fin_fr = fr;
	pass = is->is_pass;
	RWLOCK_EXIT(&ipf_state);
	if (fin->fin_fi.fi_fl & FI_FRAG)
		ipfr_newfrag(ip, fin, pass ^ FR_KEEPSTATE);
	return fr;
}


static void fr_delstate(is)
ipstate_t *is;
{
	frentry_t *fr;

	fr = is->is_rule;
	if (fr != NULL) {
		ATOMIC_DEC(fr->fr_ref);
		if (fr->fr_ref == 0)
			KFREE(fr);
	}
	KFREE(is);
}


/*
 * Free memory in use by all state info. kept.
 */
void fr_stateunload()
{
	register int i;
	register ipstate_t *is, **isp;

	WRITE_ENTER(&ipf_state);
	for (i = fr_statesize - 1; i >= 0; i--)
		for (isp = &ips_table[i]; (is = *isp); ) {
			*isp = is->is_next;
			fr_delstate(is);
			ips_num--;
		}
	ips_stats.iss_inuse = 0;
	ips_num = 0;
	RWLOCK_EXIT(&ipf_state);
	KFREES(ips_table, fr_statesize * sizeof(ipstate_t *));
	ips_table = NULL;
}


/*
 * Slowly expire held state for thingslike UDP and ICMP.  Timeouts are set
 * in expectation of this being called twice per second.
 */
void fr_timeoutstate()
{
	register int i;
	register ipstate_t *is, **isp;
#if defined(_KERNEL) && !SOLARIS
	int s;
#endif

	SPL_NET(s);
	WRITE_ENTER(&ipf_state);
	for (i = fr_statesize - 1; i >= 0; i--)
		for (isp = &ips_table[i]; (is = *isp); )
			if (is->is_age && !--is->is_age) {
				*isp = is->is_next;
				if (is->is_p == IPPROTO_TCP)
					ips_stats.iss_fin++;
				else
					ips_stats.iss_expire++;
				if (ips_table[i] == NULL)
					ips_stats.iss_inuse--;
#ifdef	IPFILTER_LOG
				ipstate_log(is, ISL_EXPIRE);
#endif
				fr_delstate(is);
				ips_num--;
			} else
				isp = &is->is_next;
	RWLOCK_EXIT(&ipf_state);
	SPL_X(s);
}


/*
 * Original idea freom Pradeep Krishnan for use primarily with NAT code.
 * (pkrishna@netcom.com)
 */
void fr_tcp_age(age, state, ip, fin, dir)
u_long *age;
u_char *state;
ip_t *ip;
fr_info_t *fin;
int dir;
{
	tcphdr_t *tcp = (tcphdr_t *)fin->fin_dp;
	u_char flags = tcp->th_flags;
	int dlen, ostate;

	ostate = state[1 - dir];

	dlen = ip->ip_len - fin->fin_hlen - (tcp->th_off << 2);

	if (flags & TH_RST) {
		if (!(tcp->th_flags & TH_PUSH) && !dlen) {
			*age = fr_tcpclosed;
			state[dir] = TCPS_CLOSED;
		} else {
			*age = fr_tcpclosewait;
			state[dir] = TCPS_CLOSE_WAIT;
		}
		return;
	}

	*age = fr_tcptimeout; /* 1 min */

	switch(state[dir])
	{
	case TCPS_CLOSED:
		if ((flags & (TH_FIN|TH_SYN|TH_RST|TH_ACK)) == TH_ACK) {
			state[dir] = TCPS_ESTABLISHED;
			*age = fr_tcpidletimeout;
		}
	case TCPS_FIN_WAIT_2:
		if ((flags & TH_OPENING) == TH_OPENING)
			state[dir] = TCPS_SYN_RECEIVED;
		else if (flags & TH_SYN)
			state[dir] = TCPS_SYN_SENT;
		break;
	case TCPS_SYN_RECEIVED:
	case TCPS_SYN_SENT:
		if ((flags & (TH_FIN|TH_ACK)) == TH_ACK) {
			state[dir] = TCPS_ESTABLISHED;
			*age = fr_tcpidletimeout;
		} else if ((flags & (TH_FIN|TH_ACK)) == (TH_FIN|TH_ACK)) {
			state[dir] = TCPS_CLOSE_WAIT;
			if (!(flags & TH_PUSH) && !dlen &&
			    ostate > TCPS_ESTABLISHED)
				*age  = fr_tcplastack;
			else
				*age  = fr_tcpclosewait;
		}
		break;
	case TCPS_ESTABLISHED:
		if (flags & TH_FIN) {
			state[dir] = TCPS_CLOSE_WAIT;
			if (!(flags & TH_PUSH) && !dlen &&
			    ostate > TCPS_ESTABLISHED)
				*age  = fr_tcplastack;
			else
				*age  = fr_tcpclosewait;
		} else {
			if (ostate < TCPS_CLOSE_WAIT)
				*age = fr_tcpidletimeout;
		}
		break;
	case TCPS_CLOSE_WAIT:
		if ((flags & TH_FIN) && !(flags & TH_PUSH) && !dlen &&
		    ostate > TCPS_ESTABLISHED) {
			*age  = fr_tcplastack;
			state[dir] = TCPS_LAST_ACK;
		} else
			*age  = fr_tcpclosewait;
		break;
	case TCPS_LAST_ACK:
		if (flags & TH_ACK) {
			state[dir] = TCPS_FIN_WAIT_2;
			if (!(flags & TH_PUSH) && !dlen &&
			    ostate > TCPS_ESTABLISHED)
				*age  = fr_tcplastack;
			else {
				*age  = fr_tcpclosewait;
				state[dir] = TCPS_CLOSE_WAIT;
			}
		}
		break;
	}
}


#ifdef	IPFILTER_LOG
void ipstate_log(is, type)
struct ipstate *is;
u_int type;
{
	struct	ipslog	ipsl;
	void *items[1];
	size_t sizes[1];
	int types[1];

	ipsl.isl_type = type;
	ipsl.isl_pkts = is->is_pkts;
	ipsl.isl_bytes = is->is_bytes;
	ipsl.isl_src = is->is_src;
	ipsl.isl_dst = is->is_dst;
	ipsl.isl_p = is->is_p;
	ipsl.isl_flags = is->is_flags;
	if (ipsl.isl_p == IPPROTO_TCP || ipsl.isl_p == IPPROTO_UDP) {
		ipsl.isl_sport = is->is_sport;
		ipsl.isl_dport = is->is_dport;
		if (ipsl.isl_p == IPPROTO_TCP) {
			ipsl.isl_state[0] = is->is_state[0];
			ipsl.isl_state[1] = is->is_state[1];
		}
	} else if (ipsl.isl_p == IPPROTO_ICMP)
		ipsl.isl_itype = is->is_icmp.ics_type;
	else {
		ipsl.isl_ps.isl_filler[0] = 0;
		ipsl.isl_ps.isl_filler[1] = 0;
	}
	items[0] = &ipsl;
	sizes[0] = sizeof(ipsl);
	types[0] = 0;

	(void) ipllog(IPL_LOGSTATE, NULL, items, sizes, types, 1);
}
#endif
