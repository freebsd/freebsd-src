/*
 * Copyright (C) 1995-2000 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#if !defined(lint)
static const char sccsid[] = "@(#)ip_state.c	1.8 6/5/96 (C) 1993-1995 Darren Reed";
static const char rcsid[] = "@(#)$FreeBSD: src/sys/netinet/ip_state.c,v 1.13.2.3 2000/07/19 23:27:55 darrenr Exp $";
#endif

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#if defined(__NetBSD__) && (NetBSD >= 199905) && !defined(IPFILTER_LKM) && \
    defined(_KERNEL)
# include "opt_ipfilter_log.h"
#endif
#if defined(_KERNEL) && defined(__FreeBSD_version) && \
    (__FreeBSD_version >= 400000) && !defined(KLD_MODULE)
#include "opt_inet6.h"
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
#if (defined(KERNEL) || defined(_KERNEL)) && (__FreeBSD_version >= 220000)
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
#if (defined(_KERNEL) || defined(KERNEL)) && !defined(linux)
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
#ifdef	USE_INET6
#include <netinet/icmp6.h>
#endif
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
static ipstate_t *ips_list = NULL;
static int	ips_num = 0;
static ips_stat_t ips_stats;
#if	(SOLARIS || defined(__sgi)) && defined(_KERNEL)
extern	KRWLOCK_T	ipf_state, ipf_mutex;
extern	kmutex_t	ipf_rw;
#endif

#ifdef	USE_INET6
static frentry_t *fr_checkicmp6matchingstate __P((ip6_t *, fr_info_t *));
#endif
static int fr_matchsrcdst __P((ipstate_t *, union i6addr, union i6addr,
			       fr_info_t *, tcphdr_t *));
static frentry_t *fr_checkicmpmatchingstate __P((ip_t *, fr_info_t *));
static int fr_matchicmpqueryreply __P((int, ipstate_t *, icmphdr_t *));
static int fr_state_flush __P((int));
static ips_stat_t *fr_statetstats __P((void));
static void fr_delstate __P((ipstate_t *));
static int fr_state_remove __P((caddr_t));
int fr_stputent __P((caddr_t));
int fr_stgetent __P((caddr_t));
void fr_stinsert __P((ipstate_t *));


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
int	fr_state_doflush = 0,
	fr_state_lock = 0;

static 	int icmpreplytype4[ICMP_MAXTYPE + 1];

int fr_stateinit()
{
	int i;

	KMALLOCS(ips_table, ipstate_t **, fr_statesize * sizeof(ipstate_t *));
	if (ips_table != NULL)
		bzero((char *)ips_table, fr_statesize * sizeof(ipstate_t *));
	else
		return -1;

	/* fill icmp reply type table */
	for (i = 0; i <= ICMP_MAXTYPE; i++)
		icmpreplytype4[i] = -1;
	icmpreplytype4[ICMP_ECHO] = ICMP_ECHOREPLY;
	icmpreplytype4[ICMP_TSTAMP] = ICMP_TSTAMPREPLY;
	icmpreplytype4[ICMP_IREQ] = ICMP_IREQREPLY;
	icmpreplytype4[ICMP_MASKREQ] = ICMP_MASKREPLY;

	return 0;
}


static ips_stat_t *fr_statetstats()
{
	ips_stats.iss_active = ips_num;
	ips_stats.iss_table = ips_table;
	ips_stats.iss_list = ips_list;
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
	register ipstate_t *is, **isp;
#if defined(_KERNEL) && !SOLARIS
	int s;
#endif
	int delete, removed = 0;

	SPL_NET(s);
	for (isp = &ips_list; (is = *isp); ) {
		delete = 0;

		switch (which)
		{
		case 0 :
			delete = 1;
			break;
		case 1 :
			if (is->is_p != IPPROTO_TCP)
				break;
			if ((is->is_state[0] != TCPS_ESTABLISHED) ||
			    (is->is_state[1] != TCPS_ESTABLISHED))
				delete = 1;
			break;
		}

		if (delete) {
			if (is->is_p == IPPROTO_TCP)
				ips_stats.iss_fin++;
			else
				ips_stats.iss_expire++;
#ifdef	IPFILTER_LOG
			ipstate_log(is, ISL_FLUSH);
#endif
			fr_delstate(is);
			removed++;
		} else
			isp = &is->is_next;
	}
	SPL_X(s);
	return removed;
}


static int fr_state_remove(data)
caddr_t data;
{
	ipstate_t *sp, st;
	int error;

	sp = &st;
	error = IRCOPYPTR(data, (caddr_t)&st, sizeof(st));
	if (error)
		return EFAULT;

	for (sp = ips_list; sp; sp = sp->is_next)
		if ((sp->is_p == st.is_p) && (sp->is_v == st.is_v) &&
		    !bcmp(&sp->is_src, &st.is_src, sizeof(st.is_src)) &&
		    !bcmp(&sp->is_dst, &st.is_src, sizeof(st.is_dst)) &&
		    !bcmp(&sp->is_ps, &st.is_ps, sizeof(st.is_ps))) {
			WRITE_ENTER(&ipf_state);
#ifdef	IPFILTER_LOG
			ipstate_log(sp, ISL_REMOVE);
#endif
			fr_delstate(sp);
			RWLOCK_EXIT(&ipf_state);
			return 0;
		}
	return ESRCH;
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
	int arg, ret, error = 0;

	switch (cmd)
	{
	case SIOCDELST :
		error = fr_state_remove(data);
		break;
	case SIOCIPFFL :
		error = IRCOPY(data, (caddr_t)&arg, sizeof(arg));
		if (error)
			break;
		if (arg == 0 || arg == 1) {
			WRITE_ENTER(&ipf_state);
			ret = fr_state_flush(arg);
			RWLOCK_EXIT(&ipf_state);
			error = IWCOPY((caddr_t)&ret, data, sizeof(ret));
		} else
			error = EINVAL;
		break;
#ifdef	IPFILTER_LOG
	case SIOCIPFFB :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			int tmp;

			tmp = ipflog_clear(IPL_LOGSTATE);
			IWCOPY((char *)&tmp, data, sizeof(tmp));
		}
		break;
#endif
	case SIOCGETFS :
		error = IWCOPYPTR((caddr_t)fr_statetstats(), data,
				  sizeof(ips_stat_t));
		break;
	case FIONREAD :
#ifdef	IPFILTER_LOG
		error = IWCOPY((caddr_t)&iplused[IPL_LOGSTATE], (caddr_t)data,
			       sizeof(iplused[IPL_LOGSTATE]));
#endif
		break;
	case SIOCSTLCK :
		error = fr_lock(data, &fr_state_lock);
		break;
	case SIOCSTPUT :
		if (!fr_state_lock) {
			error = EACCES;
			break;
		}
		error = fr_stputent(data);
		break;
	case SIOCSTGET :
		if (!fr_state_lock) {
			error = EACCES;
			break;
		}
		error = fr_stgetent(data);
		break;
	default :
		error = EINVAL;
		break;
	}
	return error;
}


int fr_stgetent(data)
caddr_t data;
{
	register ipstate_t *is, *isn;
	ipstate_save_t ips, *ipsp;
	int error;

	error = IRCOPY(data, (caddr_t)&ipsp, sizeof(ipsp));
	if (error)
		return EFAULT;
	error = IRCOPY((caddr_t)ipsp, (caddr_t)&ips, sizeof(ips));
	if (error)
		return EFAULT;

	isn = ips.ips_next;
	if (!isn) {
		isn = ips_list;
		if (isn == NULL) {
			if (ips.ips_next == NULL)
				return ENOENT;
			return 0;
		}
	} else {
		/*
		 * Make sure the pointer we're copying from exists in the
		 * current list of entries.  Security precaution to prevent
		 * copying of random kernel data.
		 */
		for (is = ips_list; is; is = is->is_next)
			if (is == isn)
				break;
		if (!is)
			return ESRCH;
	}
	ips.ips_next = isn->is_next;
	bcopy((char *)isn, (char *)&ips.ips_is, sizeof(ips.ips_is));
	if (isn->is_rule)
		bcopy((char *)isn->is_rule, (char *)&ips.ips_fr,
		      sizeof(ips.ips_fr));
	error = IWCOPY((caddr_t)&ips, ipsp, sizeof(ips));
	if (error)
		return EFAULT;
	return 0;
}


int fr_stputent(data)
caddr_t data;
{
	register ipstate_t *is, *isn;
	ipstate_save_t ips, *ipsp;
	int error, out;
	frentry_t *fr;

	error = IRCOPY(data, (caddr_t)&ipsp, sizeof(ipsp));
	if (error)
		return EFAULT;
	error = IRCOPY((caddr_t)ipsp, (caddr_t)&ips, sizeof(ips));
	if (error)
		return EFAULT;

	KMALLOC(isn, ipstate_t *);
	if (isn == NULL)
		return ENOMEM;

	bcopy((char *)&ips.ips_is, (char *)isn, sizeof(*isn));
	fr = isn->is_rule;
	if (fr != NULL) {
		if (isn->is_flags & FI_NEWFR) {
			KMALLOC(fr, frentry_t *);
			if (fr == NULL) {
				KFREE(isn);
				return ENOMEM;
			}
			bcopy((char *)&ips.ips_fr, (char *)fr, sizeof(*fr));
			out = fr->fr_flags & FR_OUTQUE ? 1 : 0;
			isn->is_rule = fr;
			ips.ips_is.is_rule = fr;
			if (*fr->fr_ifname) {
				fr->fr_ifa = GETUNIT(fr->fr_ifname, fr->fr_v);
				if (fr->fr_ifa == NULL)
					fr->fr_ifa = (void *)-1;
#ifdef	_KERNEL
				else {
					strncpy(isn->is_ifname[out],
						IFNAME(fr->fr_ifa), IFNAMSIZ);
					isn->is_ifp[out] = fr->fr_ifa;
				}
#endif
			} else
				fr->fr_ifa = NULL;
			/*
			 * send a copy back to userland of what we ended up
			 * to allow for verification.
			 */
			error = IWCOPY((caddr_t)&ips, ipsp, sizeof(ips));
			if (error) {
				KFREE(isn);
				KFREE(fr);
				return EFAULT;
			}
		} else {
			for (is = ips_list; is; is = is->is_next)
				if (is->is_rule == fr)
					break;
			if (!is) {
				KFREE(isn);
				return ESRCH;
			}
		}
	}
	fr_stinsert(isn);
	return 0;
}


void fr_stinsert(is)
register ipstate_t *is;
{
	register u_int hv = is->is_hv;

	MUTEX_INIT(&is->is_lock, "ipf state entry", NULL);

	is->is_ifname[0][sizeof(is->is_ifname[0]) - 1] = '\0';
	if (is->is_ifname[0][0] != '\0') {
		is->is_ifp[0] = GETUNIT(is->is_ifname[0], is->is_v);
	}
	is->is_ifname[1][sizeof(is->is_ifname[0]) - 1] = '\0';
	if (is->is_ifname[1][0] != '\0') {
		is->is_ifp[1] = GETUNIT(is->is_ifname[1], is->is_v);
	}

	/*
	 * add into list table.
	 */
	if (ips_list)
		ips_list->is_pnext = &is->is_next;
	is->is_pnext = &ips_list;
	is->is_next = ips_list;
	ips_list = is;
	if (ips_table[hv])
		ips_table[hv]->is_phnext = &is->is_hnext;
	else
		ips_stats.iss_inuse++;
	is->is_phnext = ips_table + hv;
	is->is_hnext = ips_table[hv];
	ips_table[hv] = is;
}


/*
 * Create a new ipstate structure and hang it off the hash table.
 */
ipstate_t *fr_addstate(ip, fin, flags)
ip_t *ip;
fr_info_t *fin;
u_int flags;
{
	register tcphdr_t *tcp = NULL;
	register ipstate_t *is;
	register u_int hv;
	ipstate_t ips;
	u_int pass;
	int out;

	if (fr_state_lock || (fin->fin_off & IP_OFFMASK) ||
	    (fin->fin_fi.fi_fl & FI_SHORT))
		return NULL;
	if (ips_num == fr_statemax) {
		ips_stats.iss_max++;
		fr_state_doflush = 1;
		return NULL;
	}
	out = fin->fin_out;
	is = &ips;
	bzero((char *)is, sizeof(*is));
	ips.is_age = 1;
	ips.is_state[0] = 0;
	ips.is_state[1] = 0;
	/*
	 * Copy and calculate...
	 */
	hv = (is->is_p = fin->fin_fi.fi_p);
	is->is_src = fin->fin_fi.fi_src;
	hv += is->is_saddr;
	is->is_dst = fin->fin_fi.fi_dst;
	hv += is->is_daddr;
#ifdef	USE_INET6
	if (fin->fin_v == 6) {
		if (is->is_p == IPPROTO_ICMPV6) {
			if (IN6_IS_ADDR_MULTICAST(&is->is_dst.in6))
				flags |= FI_W_DADDR;
			if (out)
				hv -= is->is_daddr;
			else
				hv -= is->is_saddr;
		}
	}
#endif

	switch (is->is_p)
	{
#ifdef	USE_INET6
	case IPPROTO_ICMPV6 :
#endif
	case IPPROTO_ICMP :
	    {
		struct icmp *ic = (struct icmp *)fin->fin_dp;

#ifdef	USE_INET6
		if ((is->is_p == IPPROTO_ICMPV6) &&
		    ((ic->icmp_type & ICMP6_INFOMSG_MASK) == 0))
			return NULL;
#endif
		switch (ic->icmp_type)
		{
#ifdef	USE_INET6
		case ICMP6_ECHO_REQUEST :
			is->is_icmp.ics_type = ICMP6_ECHO_REPLY;
			hv += (is->is_icmp.ics_id = ic->icmp_id);
			hv += (is->is_icmp.ics_seq = ic->icmp_seq);
			break;
		case ICMP6_MEMBERSHIP_QUERY :
		case ND_ROUTER_SOLICIT :
		case ND_NEIGHBOR_SOLICIT :
			is->is_icmp.ics_type = ic->icmp_type + 1;
			break;
			break;
#endif
		case ICMP_ECHO :
		case ICMP_TSTAMP :
		case ICMP_IREQ :
		case ICMP_MASKREQ :
			is->is_icmp.ics_type = ic->icmp_type;
			hv += (is->is_icmp.ics_id = ic->icmp_id);
			hv += (is->is_icmp.ics_seq = ic->icmp_seq);
			break;
		default :
			return NULL;
		}
		ATOMIC_INCL(ips_stats.iss_icmp);
		is->is_age = fr_icmptimeout;
		break;
	    }
	case IPPROTO_TCP :
	    {
		tcp = (tcphdr_t *)fin->fin_dp;

		if (tcp->th_flags & TH_RST)
			return NULL;
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
		is->is_send = ntohl(tcp->th_seq) + ip->ip_len -
			      fin->fin_hlen - (tcp->th_off << 2) +
			      ((tcp->th_flags & TH_SYN) ? 1 : 0) +
			      ((tcp->th_flags & TH_FIN) ? 1 : 0);
		is->is_maxsend = is->is_send;
		is->is_dend = 0;
		is->is_maxdwin = 1;
		is->is_maxswin = ntohs(tcp->th_win);
		if (is->is_maxswin == 0)
			is->is_maxswin = 1;
		/*
		 * If we're creating state for a starting connection, start the
		 * timer on it as we'll never see an error if it fails to
		 * connect.
		 */
		ATOMIC_INCL(ips_stats.iss_tcp);
		break;
	    }
	case IPPROTO_UDP :
	    {
		tcp = (tcphdr_t *)fin->fin_dp;

		is->is_dport = tcp->th_dport;
		is->is_sport = tcp->th_sport;
		if ((flags & (FI_W_DPORT|FI_W_SPORT)) == 0) {
			hv += tcp->th_dport;
			hv += tcp->th_sport;
		}
		ATOMIC_INCL(ips_stats.iss_udp);
		is->is_age = fr_udptimeout;
		break;
	    }
	default :
		return NULL;
	}

	KMALLOC(is, ipstate_t *);
	if (is == NULL) {
		ATOMIC_INCL(ips_stats.iss_nomem);
		return NULL;
	}
	bcopy((char *)&ips, (char *)is, sizeof(*is));
	hv %= fr_statesize;
	is->is_hv = hv;
	is->is_rule = fin->fin_fr;
	if (is->is_rule != NULL) {
		ATOMIC_INC32(is->is_rule->fr_ref);
		pass = is->is_rule->fr_flags;
	} else
		pass = fr_flags;
	WRITE_ENTER(&ipf_state);

	is->is_pass = pass;
	is->is_pkts = 1;
	is->is_bytes = fin->fin_dlen + fin->fin_hlen;
	/*
	 * We want to check everything that is a property of this packet,
	 * but we don't (automatically) care about it's fragment status as
	 * this may change.
	 */
	is->is_v = fin->fin_fi.fi_v;
	is->is_opt = fin->fin_fi.fi_optmsk;
	is->is_optmsk = 0xffffffff;
	is->is_sec = fin->fin_fi.fi_secmsk;
	is->is_secmsk = 0xffff;
	is->is_auth = fin->fin_fi.fi_auth;
	is->is_authmsk = 0xffff;
	is->is_flags = fin->fin_fi.fi_fl & FI_CMP;
	is->is_flags |= FI_CMP << 4;
	is->is_flags |= flags & (FI_WILDP|FI_WILDA);
	is->is_ifp[1 - out] = NULL;
	is->is_ifp[out] = fin->fin_ifp;
#ifdef	_KERNEL
	strncpy(is->is_ifname[out], IFNAME(fin->fin_ifp), IFNAMSIZ);
#endif
	is->is_ifname[1 - out][0] = '\0';
	if (pass & FR_LOGFIRST)
		is->is_pass &= ~(FR_LOGFIRST|FR_LOG);
	fr_stinsert(is);
	ips_num++;
	if (is->is_p == IPPROTO_TCP) {
		MUTEX_ENTER(&is->is_lock);
		fr_tcp_age(&is->is_age, is->is_state, fin,
			   tcp->th_sport == is->is_sport);
		MUTEX_EXIT(&is->is_lock);
	}
#ifdef	IPFILTER_LOG
	ipstate_log(is, ISL_NEW);
#endif
	RWLOCK_EXIT(&ipf_state);
	fin->fin_rev = IP6NEQ(is->is_dst, fin->fin_fi.fi_dst);
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
	source = IP6EQ(fin->fin_fi.fi_src, is->is_src);
	fdata = &is->is_tcp.ts_data[!source];
	tdata = &is->is_tcp.ts_data[source];
	seq = ntohl(tcp->th_seq);
	ack = ntohl(tcp->th_ack);
	win = ntohs(tcp->th_win);
	end = seq + fin->fin_dlen - (tcp->th_off << 2) +
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
	    (SEQ_GE(seq, fdata->td_end - maxwin)) &&
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

		ATOMIC_INCL(ips_stats.iss_hits);
		is->is_pkts++;
		is->is_bytes += fin->fin_dlen + fin->fin_hlen;
		/*
		 * Nearing end of connection, start timeout.
		 */
		MUTEX_ENTER(&is->is_lock);
		fr_tcp_age(&is->is_age, is->is_state, fin, source);
		MUTEX_EXIT(&is->is_lock);
		ret = 1;
	}
	return ret;
}


static int fr_matchsrcdst(is, src, dst, fin, tcp)
ipstate_t *is;
union i6addr src, dst;
fr_info_t *fin;
tcphdr_t *tcp;
{
	int ret = 0, rev, out, flags;
	u_short sp, dp;
	void *ifp;

	rev = fin->fin_rev = IP6NEQ(is->is_dst, dst);
	ifp = fin->fin_ifp;
	out = fin->fin_out;

	if (tcp != NULL) {
		flags = is->is_flags;
		sp = tcp->th_sport;
		dp = tcp->th_dport;
	} else {
		flags = is->is_flags & FI_WILDA;
		sp = 0;
		dp = 0;
	}

	if (rev == 0) {
		if (!out) {
			if (is->is_ifpin == NULL || is->is_ifpin == ifp)
				ret = 1;
		} else {
			if (is->is_ifpout == NULL || is->is_ifpout == ifp)
				ret = 1;
		}
	} else {
		if (out) {
			if (is->is_ifpin == NULL || is->is_ifpin == ifp)
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
		if (
		    (IP6EQ(is->is_dst, dst) || (flags & FI_W_DADDR)) &&
		    (IP6EQ(is->is_src, src) || (flags & FI_W_SADDR)) &&
		    (!tcp || ((sp == is->is_sport || flags & FI_W_SPORT) &&
		     (dp == is->is_dport || flags & FI_W_DPORT)))) {
			ret = 1;
		}
	} else {
		if (
		    (IP6EQ(is->is_dst, src) || (flags & FI_W_DADDR)) &&
		    (IP6EQ(is->is_src, dst) || (flags & FI_W_SADDR)) &&
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

	ret = -1;

	if (!rev) {
		if (out) {
			if (!is->is_ifpout)
				ret = 1;
		} else {
			if (!is->is_ifpin)
				ret = 0;
		}
	} else {
		if (out) {
			if (!is->is_ifpin)
				ret = 0;
		} else {
			if (!is->is_ifpout)
				ret = 1;
		}
	}

	if (ret >= 0) {
		is->is_ifp[ret] = ifp;
#ifdef	_KERNEL
		strncpy(is->is_ifname[out], IFNAME(fin->fin_ifp),
			sizeof(is->is_ifname[1]));
#endif
	}
#ifdef  _KERNEL
	if (ret >= 0) {
		strncpy(is->is_ifname[out], IFNAME(fin->fin_ifp),
			sizeof(is->is_ifname[1]));
	}
#endif
	return 1;
}

static int fr_matchicmpqueryreply(v, is, icmp)
int v;
ipstate_t *is;
icmphdr_t *icmp;
{
	if (v == 4) {
		/*
		 * If we matched its type on the way in, then when going out
		 * it will still be the same type.
		 */
		if (((icmp->icmp_type == is->is_type) ||
		     (icmpreplytype4[is->is_type] == icmp->icmp_type)) &&
		    (icmp->icmp_id == is->is_icmp.ics_id) &&
		    (icmp->icmp_seq == is->is_icmp.ics_seq)) {
			return 1;
		};
	}
#ifdef	USE_INET6
	else if (is->is_v == 6) {
		if ((is->is_type == ICMP6_ECHO_REPLY) &&
		    (icmp->icmp_type == ICMP6_ECHO_REQUEST) &&
		    (icmp->icmp_id == is->is_icmp.ics_id) &&
		    (icmp->icmp_seq == is->is_icmp.ics_seq)) {
			return 1;
		};
	}
#endif
	return 0;
}

static frentry_t *fr_checkicmpmatchingstate(ip, fin)
ip_t *ip;
fr_info_t *fin;
{
	register ipstate_t *is, **isp;
	register u_short sport, dport;
	register u_char	pr;
	union i6addr dst, src;
	struct icmp *ic;
	u_short savelen;
	fr_info_t ofin;
	tcphdr_t *tcp;
	icmphdr_t *icmp;
	frentry_t *fr;
	ip_t *oip;
	int type;
	u_int hv;

	/*
	 * Does it at least have the return (basic) IP header ?
	 * Only a basic IP header (no options) should be with
	 * an ICMP error header.
	 */
	if (((ip->ip_v != 4) && (ip->ip_hl != 5)) ||
	    (fin->fin_plen < ICMPERR_MINPKTLEN))
		return NULL;
	ic = (struct icmp *)fin->fin_dp;
	type = ic->icmp_type;
	/*
	 * If it's not an error type, then return
	 */
	if ((type != ICMP_UNREACH) && (type != ICMP_SOURCEQUENCH) &&
    	    (type != ICMP_REDIRECT) && (type != ICMP_TIMXCEED) &&
    	    (type != ICMP_PARAMPROB))
		return NULL;

	oip = (ip_t *)((char *)ic + ICMPERR_ICMPHLEN);
	if (fin->fin_plen < ICMPERR_MAXPKTLEN + ((oip->ip_hl - 5) << 2))
		return NULL;

	if (oip->ip_p == IPPROTO_ICMP) {
		icmp = (icmphdr_t *)((char *)oip + (oip->ip_hl << 2));

		/*
		 * a ICMP error can only be generated as a result of an
		 * ICMP query, not as the response on an ICMP error
		 *
		 * XXX theoretically ICMP_ECHOREP and the other reply's are
		 * ICMP query's as well, but adding them here seems strange XXX
		 */
		 if ((icmp->icmp_type != ICMP_ECHO) &&
		     (icmp->icmp_type != ICMP_TSTAMP) &&
		     (icmp->icmp_type != ICMP_IREQ) &&
		     (icmp->icmp_type != ICMP_MASKREQ))
		    	return NULL;

		/*
		 * perform a lookup of the ICMP packet in the state table
		 */
		hv = (pr = oip->ip_p);
		src.in4 = oip->ip_src;
		hv += src.in4.s_addr;
		dst.in4 = oip->ip_dst;
		hv += dst.in4.s_addr;
		hv += icmp->icmp_id;
		hv += icmp->icmp_seq;
		hv %= fr_statesize;

		oip->ip_len = ntohs(oip->ip_len);
		fr_makefrip(oip->ip_hl << 2, oip, &ofin);
		oip->ip_len = htons(oip->ip_len);
		ofin.fin_ifp = fin->fin_ifp;
		ofin.fin_out = !fin->fin_out;
		ofin.fin_mp = NULL; /* if dereferenced, panic XXX */

		READ_ENTER(&ipf_state);
		for (isp = &ips_table[hv]; (is = *isp); isp = &is->is_hnext)
			if ((is->is_p == pr) && (is->is_v == 4) &&
			    fr_matchsrcdst(is, src, dst, &ofin, NULL) &&
			    fr_matchicmpqueryreply(is->is_v, is, icmp)) {
				ips_stats.iss_hits++;
				is->is_pkts++;
				is->is_bytes += ip->ip_len;
				fr = is->is_rule;
				RWLOCK_EXIT(&ipf_state);
				return fr;
			}
		RWLOCK_EXIT(&ipf_state);
		return NULL;
	};

	if ((oip->ip_p != IPPROTO_TCP) && (oip->ip_p != IPPROTO_UDP))
		return NULL;

	tcp = (tcphdr_t *)((char *)oip + (oip->ip_hl << 2));
	dport = tcp->th_dport;
	sport = tcp->th_sport;

	hv = (pr = oip->ip_p);
	src.in4 = oip->ip_src;
	hv += src.in4.s_addr;
	dst.in4 = oip->ip_dst;
	hv += dst.in4.s_addr;
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
	savelen = oip->ip_len;
	oip->ip_len = ip->ip_len - (ip->ip_hl << 2) - ICMPERR_ICMPHLEN;
	fr_makefrip(oip->ip_hl << 2, oip, &ofin);
	oip->ip_len = savelen;
	ofin.fin_ifp = fin->fin_ifp;
	ofin.fin_out = !fin->fin_out;
	ofin.fin_mp = NULL; /* if dereferenced, panic XXX */
	READ_ENTER(&ipf_state);
	for (isp = &ips_table[hv]; (is = *isp); isp = &is->is_hnext) {
		/*
		 * Only allow this icmp though if the
		 * encapsulated packet was allowed through the
		 * other way around. Note that the minimal amount
		 * of info present does not allow for checking against
		 * tcp internals such as seq and ack numbers.
		 */
		if ((is->is_p == pr) && (is->is_v == 4) &&
		    fr_matchsrcdst(is, src, dst, &ofin, tcp)) {
			fr = is->is_rule;
			ips_stats.iss_hits++;
			/*
			 * we must swap src and dst here because the icmp
			 * comes the other way around
			 */
			is->is_pkts++;
			is->is_bytes += fin->fin_plen;
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
	union i6addr dst, src;
	register ipstate_t *is, **isp;
	register u_char pr;
	u_int hv, hvm, hlen, tryagain, pass, v;
	struct icmp *ic;
	frentry_t *fr;
	tcphdr_t *tcp;

	if (fr_state_lock || (fin->fin_off & IP_OFFMASK) ||
	    (fin->fin_fi.fi_fl & FI_SHORT))
		return NULL;

	is = NULL;
	hlen = fin->fin_hlen;
	tcp = (tcphdr_t *)((char *)ip + hlen);
	ic = (struct icmp *)tcp;
	hv = (pr = fin->fin_fi.fi_p);
	src = fin->fin_fi.fi_src;
	dst = fin->fin_fi.fi_dst;
	hv += src.in4.s_addr;
	hv += dst.in4.s_addr;

	/*
	 * Search the hash table for matching packet header info.
	 */
	v = fin->fin_fi.fi_v;
	switch (fin->fin_fi.fi_p)
	{
#ifdef	USE_INET6
	case IPPROTO_ICMPV6 :
		if (v == 6) {
			if (fin->fin_out)
				hv -= dst.in4.s_addr;
			else
				hv -= src.in4.s_addr;
			if ((ic->icmp_type == ICMP6_ECHO_REQUEST) ||
			    (ic->icmp_type == ICMP6_ECHO_REPLY)) {
				hv += ic->icmp_id;
				hv += ic->icmp_seq;
			}
		}
#endif
	case IPPROTO_ICMP :
		if (v == 4) {
			hv += ic->icmp_id;
			hv += ic->icmp_seq;
		}
		hv %= fr_statesize;
		READ_ENTER(&ipf_state);
		for (isp = &ips_table[hv]; (is = *isp); isp = &is->is_hnext) {
			if ((is->is_p == pr) && (is->is_v == v) &&
			    fr_matchsrcdst(is, src, dst, fin, NULL) &&
			    fr_matchicmpqueryreply(v, is, ic)) {
				is->is_age = fr_icmptimeout;
				break;
			}
		}
		if (is != NULL)
			break;
		RWLOCK_EXIT(&ipf_state);
		/*
		 * No matching icmp state entry. Perhaps this is a
		 * response to another state entry.
		 */
#ifdef	USE_INET6
		if (v == 6)
			fr = fr_checkicmp6matchingstate((ip6_t *)ip, fin);
		else
#endif
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
		     isp = &is->is_hnext)


			if ((is->is_p == pr) && (is->is_v == v) &&
			    fr_matchsrcdst(is, src, dst, fin, tcp)) {
				if (fr_tcpstate(is, fin, ip, tcp))
					break;
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
		for (is = ips_table[hvm]; is; is = is->is_hnext)
			if ((is->is_p == pr) && (is->is_v == v) &&
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
		ATOMIC_INCL(ips_stats.iss_miss);
		return NULL;
	}
	MUTEX_ENTER(&is->is_lock);
	is->is_bytes += fin->fin_plen;
	ips_stats.iss_hits++;
	is->is_pkts++;
	MUTEX_EXIT(&is->is_lock);
	fr = is->is_rule;
	fin->fin_fr = fr;
	pass = is->is_pass;
#ifndef	_KERNEL
	if (tcp->th_flags & TCP_CLOSE)
		fr_delstate(is);
#endif
	RWLOCK_EXIT(&ipf_state);
	if (fin->fin_fi.fi_fl & FI_FRAG)
		ipfr_newfrag(ip, fin, pass ^ FR_KEEPSTATE);
	return fr;
}


void ip_statesync(ifp)
void *ifp;
{
	register ipstate_t *is;

	WRITE_ENTER(&ipf_state);
	for (is = ips_list; is; is = is->is_next) {
		if (is->is_ifpin == ifp) {
			is->is_ifpin = GETUNIT(is->is_ifname[0], is->is_v);
			if (!is->is_ifpin)
				is->is_ifpin = (void *)-1;
		}
		if (is->is_ifpout == ifp) {
			is->is_ifpout = GETUNIT(is->is_ifname[1], is->is_v);
			if (!is->is_ifpout)
				is->is_ifpout = (void *)-1;
		}
	}
	RWLOCK_EXIT(&ipf_state);
}


static void fr_delstate(is)
ipstate_t *is;
{
	frentry_t *fr;

	if (is->is_next)
		is->is_next->is_pnext = is->is_pnext;
	*is->is_pnext = is->is_next;
	if (is->is_hnext)
		is->is_hnext->is_phnext = is->is_phnext;
	*is->is_phnext = is->is_hnext;
	if (ips_table[is->is_hv] == NULL)
		ips_stats.iss_inuse--;

	fr = is->is_rule;
	if (fr != NULL) {
		ATOMIC_DEC32(fr->fr_ref);
		if (fr->fr_ref == 0)
			KFREE(fr);
	}
#ifdef	_KERNEL
	MUTEX_DESTROY(&is->is_lock);
#endif
	KFREE(is);
	ips_num--;
}


/*
 * Free memory in use by all state info. kept.
 */
void fr_stateunload()
{
	register ipstate_t *is;

	WRITE_ENTER(&ipf_state);
	while ((is = ips_list))
		fr_delstate(is);
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
	register ipstate_t *is, **isp;
#if defined(_KERNEL) && !SOLARIS
	int s;
#endif

	SPL_NET(s);
	WRITE_ENTER(&ipf_state);
	for (isp = &ips_list; (is = *isp); )
		if (is->is_age && !--is->is_age) {
			if (is->is_p == IPPROTO_TCP)
				ips_stats.iss_fin++;
			else
				ips_stats.iss_expire++;
#ifdef	IPFILTER_LOG
			ipstate_log(is, ISL_EXPIRE);
#endif
			fr_delstate(is);
		} else
			isp = &is->is_next;
	RWLOCK_EXIT(&ipf_state);
	SPL_X(s);
	if (fr_state_doflush) {
		(void) fr_state_flush(1);
		fr_state_doflush = 0;
	}
}


/*
 * Original idea freom Pradeep Krishnan for use primarily with NAT code.
 * (pkrishna@netcom.com)
 */
void fr_tcp_age(age, state, fin, dir)
u_long *age;
u_char *state;
fr_info_t *fin;
int dir;
{
	tcphdr_t *tcp = (tcphdr_t *)fin->fin_dp;
	u_char flags = tcp->th_flags;
	int dlen, ostate;

	ostate = state[1 - dir];

	dlen = fin->fin_plen - fin->fin_hlen - (tcp->th_off << 2);

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
	ipsl.isl_v = is->is_v;
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


#ifdef	USE_INET6
frentry_t *fr_checkicmp6matchingstate(ip, fin)
ip6_t *ip;
fr_info_t *fin;
{
	register ipstate_t *is, **isp;
	register u_short sport, dport;
	register u_char	pr;
	struct icmp6_hdr *ic, *oic;
	union i6addr dst, src;
	u_short savelen;
	fr_info_t ofin;
	tcphdr_t *tcp;
	frentry_t *fr;
	ip6_t *oip;
	int type;
	u_int hv;

	/*
	 * Does it at least have the return (basic) IP header ?
	 * Only a basic IP header (no options) should be with
	 * an ICMP error header.
	 */
	if ((fin->fin_v != 6) || (fin->fin_plen < ICMP6ERR_MINPKTLEN))
		return NULL;
	ic = (struct icmp6_hdr *)fin->fin_dp;
	type = ic->icmp6_type;
	/*
	 * If it's not an error type, then return
	 */
	if ((type != ICMP6_DST_UNREACH) && (type != ICMP6_PACKET_TOO_BIG) &&
	    (type != ICMP6_TIME_EXCEEDED) && (type != ICMP6_PARAM_PROB))
		return NULL;

	oip = (ip6_t *)((char *)ic + ICMPERR_ICMPHLEN);
	if (fin->fin_plen < sizeof(*oip))
		return NULL;

	if (oip->ip6_nxt == IPPROTO_ICMPV6) {
		oic = (struct icmp6_hdr *)(oip + 1);
		/*
		 * a ICMP error can only be generated as a result of an
		 * ICMP query, not as the response on an ICMP error
		 *
		 * XXX theoretically ICMP_ECHOREP and the other reply's are
		 * ICMP query's as well, but adding them here seems strange XXX
		 */
		 if (!(oic->icmp6_type & ICMP6_INFOMSG_MASK))
		    	return NULL;

		/*
		 * perform a lookup of the ICMP packet in the state table
		 */
		hv = (pr = oip->ip6_nxt);
		src.in6 = oip->ip6_src;
		hv += src.in4.s_addr;
		dst.in6 = oip->ip6_dst;
		hv += dst.in4.s_addr;
		hv += oic->icmp6_id;
		hv += oic->icmp6_seq;
		hv %= fr_statesize;

		oip->ip6_plen = ntohs(oip->ip6_plen);
		fr_makefrip(sizeof(*oip), (ip_t *)oip, &ofin);
		oip->ip6_plen = htons(oip->ip6_plen);
		ofin.fin_ifp = fin->fin_ifp;
		ofin.fin_out = !fin->fin_out;
		ofin.fin_mp = NULL; /* if dereferenced, panic XXX */

		READ_ENTER(&ipf_state);
		for (isp = &ips_table[hv]; (is = *isp); isp = &is->is_hnext)
			if ((is->is_p == pr) &&
			    (oic->icmp6_id == is->is_icmp.ics_id) &&
			    (oic->icmp6_seq == is->is_icmp.ics_seq) &&
			    fr_matchsrcdst(is, src, dst, &ofin, NULL)) {
			    	/*
			    	 * in the state table ICMP query's are stored
			    	 * with the type of the corresponding ICMP
			    	 * response. Correct here
			    	 */
				if (((is->is_type == ICMP6_ECHO_REPLY) &&
				     (oic->icmp6_type == ICMP6_ECHO_REQUEST)) ||
				     (is->is_type - 1 == oic->icmp6_type )) {
				    	ips_stats.iss_hits++;
    					is->is_pkts++;
					is->is_bytes += fin->fin_plen;
					return is->is_rule;
				}
			}
		RWLOCK_EXIT(&ipf_state);

		return NULL;
	};

	if ((oip->ip6_nxt != IPPROTO_TCP) && (oip->ip6_nxt != IPPROTO_UDP))
		return NULL;
	tcp = (tcphdr_t *)(oip + 1);
	dport = tcp->th_dport;
	sport = tcp->th_sport;

	hv = (pr = oip->ip6_nxt);
	src.in6 = oip->ip6_src;
	hv += src.in4.s_addr;
	dst.in6 = oip->ip6_dst;
	hv += dst.in4.s_addr;
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
	savelen = oip->ip6_plen;
	oip->ip6_plen = ip->ip6_plen - sizeof(*ip) - ICMPERR_ICMPHLEN;
	ofin.fin_v = 6;
	fr_makefrip(sizeof(*oip), (ip_t *)oip, &ofin);
	oip->ip6_plen = savelen;
	ofin.fin_ifp = fin->fin_ifp;
	ofin.fin_out = !fin->fin_out;
	ofin.fin_mp = NULL; /* if dereferenced, panic XXX */
	READ_ENTER(&ipf_state);
	for (isp = &ips_table[hv]; (is = *isp); isp = &is->is_hnext) {
		/*
		 * Only allow this icmp though if the
		 * encapsulated packet was allowed through the
		 * other way around. Note that the minimal amount
		 * of info present does not allow for checking against
		 * tcp internals such as seq and ack numbers.
		 */
		if ((is->is_p == pr) && (is->is_v == 6) &&
		    fr_matchsrcdst(is, src, dst, &ofin, tcp)) {
			fr = is->is_rule;
			ips_stats.iss_hits++;
			/*
			 * we must swap src and dst here because the icmp
			 * comes the other way around
			 */
			is->is_pkts++;
			is->is_bytes += fin->fin_plen;
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
#endif
