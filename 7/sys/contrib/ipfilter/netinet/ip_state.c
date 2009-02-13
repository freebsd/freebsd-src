/*	$FreeBSD$	*/

/*
 * Copyright (C) 1995-2003 by Darren Reed.
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
#include <sys/file.h>
#if defined(__NetBSD__) && (NetBSD >= 199905) && !defined(IPFILTER_LKM) && \
    defined(_KERNEL)
# if (__NetBSD_Version__ < 399001400)
#  include "opt_ipfilter_log.h"
# else
#  include "opt_ipfilter.h"
# endif
#endif
#if defined(_KERNEL) && defined(__FreeBSD_version) && \
    (__FreeBSD_version >= 400000) && !defined(KLD_MODULE)
#include "opt_inet6.h"
#endif
#if !defined(_KERNEL) && !defined(__KERNEL__)
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# define _KERNEL
# ifdef __OpenBSD__
struct file;
# endif
# include <sys/uio.h>
# undef _KERNEL
#endif
#if defined(_KERNEL) && (__FreeBSD_version >= 220000)
# include <sys/filio.h>
# include <sys/fcntl.h>
# if (__FreeBSD_version >= 300000) && !defined(IPFILTER_LKM)
#  include "opt_ipfilter.h"
# endif
#else
# include <sys/ioctl.h>
#endif
#include <sys/time.h>
#if !defined(linux)
# include <sys/protosw.h>
#endif
#include <sys/socket.h>
#if defined(_KERNEL)
# include <sys/systm.h>
# if !defined(__SVR4) && !defined(__svr4__)
#  include <sys/mbuf.h>
# endif
#endif
#if defined(__SVR4) || defined(__svr4__)
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
#if !defined(linux)
# include <netinet/ip_var.h>
#endif
#if !defined(__hpux) && !defined(linux)
# include <netinet/tcp_fsm.h>
#endif
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include "netinet/ip_compat.h"
#include <netinet/tcpip.h>
#include "netinet/ip_fil.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_frag.h"
#include "netinet/ip_state.h"
#include "netinet/ip_proxy.h"
#ifdef	IPFILTER_SYNC
#include "netinet/ip_sync.h"
#endif
#ifdef	IPFILTER_SCAN
#include "netinet/ip_scan.h"
#endif
#ifdef	USE_INET6
#include <netinet/icmp6.h>
#endif
#if (__FreeBSD_version >= 300000)
# include <sys/malloc.h>
# if defined(_KERNEL) && !defined(IPFILTER_LKM)
#  include <sys/libkern.h>
#  include <sys/systm.h>
# endif
#endif
/* END OF INCLUDES */


#if !defined(lint)
static const char sccsid[] = "@(#)ip_state.c	1.8 6/5/96 (C) 1993-2000 Darren Reed";
static const char rcsid[] = "@(#)$Id: ip_state.c,v 2.186.2.80 2007/10/16 09:33:23 darrenr Exp $";
#endif

static	ipstate_t **ips_table = NULL;
static	u_long	*ips_seed = NULL;
static	int	ips_num = 0;
static	u_long ips_last_force_flush = 0;
ips_stat_t ips_stats;

#ifdef	USE_INET6
static ipstate_t *fr_checkicmp6matchingstate __P((fr_info_t *));
#endif
static ipstate_t *fr_matchsrcdst __P((fr_info_t *, ipstate_t *, i6addr_t *,
				      i6addr_t *, tcphdr_t *, u_32_t));
static ipstate_t *fr_checkicmpmatchingstate __P((fr_info_t *));
static int fr_state_flush __P((int, int));
static int fr_state_flush_entry __P((void *));
static ips_stat_t *fr_statetstats __P((void));
static int fr_delstate __P((ipstate_t *, int));
static int fr_state_remove __P((caddr_t));
static void fr_ipsmove __P((ipstate_t *, u_int));
static int fr_tcpstate __P((fr_info_t *, tcphdr_t *, ipstate_t *));
static int fr_tcpoptions __P((fr_info_t *, tcphdr_t *, tcpdata_t *));
static ipstate_t *fr_stclone __P((fr_info_t *, tcphdr_t *, ipstate_t *));
static void fr_fixinisn __P((fr_info_t *, ipstate_t *));
static void fr_fixoutisn __P((fr_info_t *, ipstate_t *));
static void fr_checknewisn __P((fr_info_t *, ipstate_t *));
static int fr_stateiter __P((ipftoken_t *, ipfgeniter_t *));
static int fr_stgettable __P((char *));

int fr_stputent __P((caddr_t));
int fr_stgetent __P((caddr_t));

#define	ONE_DAY		IPF_TTLVAL(1 * 86400)	/* 1 day */
#define	FIVE_DAYS	(5 * ONE_DAY)
#define	DOUBLE_HASH(x)	(((x) + ips_seed[(x) % fr_statesize]) % fr_statesize)

u_long	fr_tcpidletimeout = FIVE_DAYS,
	fr_tcpclosewait = IPF_TTLVAL(2 * TCP_MSL),
	fr_tcplastack = IPF_TTLVAL(30),
	fr_tcptimeout = IPF_TTLVAL(2 * TCP_MSL),
	fr_tcptimewait = IPF_TTLVAL(2 * TCP_MSL),
	fr_tcpclosed = IPF_TTLVAL(30),
	fr_tcphalfclosed = IPF_TTLVAL(2 * 3600),	/* 2 hours */
	fr_udptimeout = IPF_TTLVAL(120),
	fr_udpacktimeout = IPF_TTLVAL(12),
	fr_icmptimeout = IPF_TTLVAL(60),
	fr_icmpacktimeout = IPF_TTLVAL(6),
	fr_iptimeout = IPF_TTLVAL(60);
int	fr_statemax = IPSTATE_MAX,
	fr_statesize = IPSTATE_SIZE;
int	fr_state_doflush = 0,
	fr_state_lock = 0,
	fr_state_maxbucket = 0,
	fr_state_maxbucket_reset = 1,
	fr_state_init = 0;
ipftq_t	ips_tqtqb[IPF_TCP_NSTATES],
	ips_udptq,
	ips_udpacktq,
	ips_iptq,
	ips_icmptq,
	ips_icmpacktq,
	ips_deletetq,
	*ips_utqe = NULL;
#ifdef	IPFILTER_LOG
int	ipstate_logging = 1;
#else
int	ipstate_logging = 0;
#endif
ipstate_t *ips_list = NULL;


/* ------------------------------------------------------------------------ */
/* Function:    fr_stateinit                                                */
/* Returns:     int - 0 == success, -1 == failure                           */
/* Parameters:  Nil                                                         */
/*                                                                          */
/* Initialise all the global variables used within the state code.          */
/* This action also includes initiailising locks.                           */
/* ------------------------------------------------------------------------ */
int fr_stateinit()
{
	int i;

	KMALLOCS(ips_table, ipstate_t **, fr_statesize * sizeof(ipstate_t *));
	if (ips_table == NULL)
		return -1;
	bzero((char *)ips_table, fr_statesize * sizeof(ipstate_t *));

	KMALLOCS(ips_seed, u_long *, fr_statesize * sizeof(*ips_seed));
	if (ips_seed == NULL)
		return -2;
	for (i = 0; i < fr_statesize; i++) {
		/*
		 * XXX - ips_seed[X] should be a random number of sorts.
		 */
#if  (__FreeBSD_version >= 400000)
		ips_seed[i] = arc4random();
#else
		ips_seed[i] = ((u_long)ips_seed + i) * fr_statesize;
		ips_seed[i] ^= 0xa5a55a5a;
		ips_seed[i] *= (u_long)ips_seed;
		ips_seed[i] ^= 0x5a5aa5a5;
		ips_seed[i] *= fr_statemax;
#endif
	}

	/* fill icmp reply type table */
	for (i = 0; i <= ICMP_MAXTYPE; i++)
		icmpreplytype4[i] = -1;
	icmpreplytype4[ICMP_ECHO] = ICMP_ECHOREPLY;
	icmpreplytype4[ICMP_TSTAMP] = ICMP_TSTAMPREPLY;
	icmpreplytype4[ICMP_IREQ] = ICMP_IREQREPLY;
	icmpreplytype4[ICMP_MASKREQ] = ICMP_MASKREPLY;
#ifdef	USE_INET6
	/* fill icmp reply type table */
	for (i = 0; i <= ICMP6_MAXTYPE; i++)
		icmpreplytype6[i] = -1;
	icmpreplytype6[ICMP6_ECHO_REQUEST] = ICMP6_ECHO_REPLY;
	icmpreplytype6[ICMP6_MEMBERSHIP_QUERY] = ICMP6_MEMBERSHIP_REPORT;
	icmpreplytype6[ICMP6_NI_QUERY] = ICMP6_NI_REPLY;
	icmpreplytype6[ND_ROUTER_SOLICIT] = ND_ROUTER_ADVERT;
	icmpreplytype6[ND_NEIGHBOR_SOLICIT] = ND_NEIGHBOR_ADVERT;
#endif

	KMALLOCS(ips_stats.iss_bucketlen, u_long *,
		 fr_statesize * sizeof(u_long));
	if (ips_stats.iss_bucketlen == NULL)
		return -1;
	bzero((char *)ips_stats.iss_bucketlen, fr_statesize * sizeof(u_long));

	if (fr_state_maxbucket == 0) {
		for (i = fr_statesize; i > 0; i >>= 1)
			fr_state_maxbucket++;
		fr_state_maxbucket *= 2;
	}

	ips_stats.iss_tcptab = ips_tqtqb;
	fr_sttab_init(ips_tqtqb);
	ips_tqtqb[IPF_TCP_NSTATES - 1].ifq_next = &ips_udptq;
	ips_udptq.ifq_ttl = (u_long)fr_udptimeout;
	ips_udptq.ifq_ref = 1;
	ips_udptq.ifq_head = NULL;
	ips_udptq.ifq_tail = &ips_udptq.ifq_head;
	MUTEX_INIT(&ips_udptq.ifq_lock, "ipftq udp tab");
	ips_udptq.ifq_next = &ips_udpacktq;
	ips_udpacktq.ifq_ttl = (u_long)fr_udpacktimeout;
	ips_udpacktq.ifq_ref = 1;
	ips_udpacktq.ifq_head = NULL;
	ips_udpacktq.ifq_tail = &ips_udpacktq.ifq_head;
	MUTEX_INIT(&ips_udpacktq.ifq_lock, "ipftq udpack tab");
	ips_udpacktq.ifq_next = &ips_icmptq;
	ips_icmptq.ifq_ttl = (u_long)fr_icmptimeout;
	ips_icmptq.ifq_ref = 1;
	ips_icmptq.ifq_head = NULL;
	ips_icmptq.ifq_tail = &ips_icmptq.ifq_head;
	MUTEX_INIT(&ips_icmptq.ifq_lock, "ipftq icmp tab");
	ips_icmptq.ifq_next = &ips_icmpacktq;
	ips_icmpacktq.ifq_ttl = (u_long)fr_icmpacktimeout;
	ips_icmpacktq.ifq_ref = 1;
	ips_icmpacktq.ifq_head = NULL;
	ips_icmpacktq.ifq_tail = &ips_icmpacktq.ifq_head;
	MUTEX_INIT(&ips_icmpacktq.ifq_lock, "ipftq icmpack tab");
	ips_icmpacktq.ifq_next = &ips_iptq;
	ips_iptq.ifq_ttl = (u_long)fr_iptimeout;
	ips_iptq.ifq_ref = 1;
	ips_iptq.ifq_head = NULL;
	ips_iptq.ifq_tail = &ips_iptq.ifq_head;
	MUTEX_INIT(&ips_iptq.ifq_lock, "ipftq ip tab");
	ips_iptq.ifq_next = &ips_deletetq;
	ips_deletetq.ifq_ttl = (u_long)1;
	ips_deletetq.ifq_ref = 1;
	ips_deletetq.ifq_head = NULL;
	ips_deletetq.ifq_tail = &ips_deletetq.ifq_head;
	MUTEX_INIT(&ips_deletetq.ifq_lock, "state delete queue");
	ips_deletetq.ifq_next = NULL;

	RWLOCK_INIT(&ipf_state, "ipf IP state rwlock");
	MUTEX_INIT(&ipf_stinsert, "ipf state insert mutex");
	fr_state_init = 1;

	ips_last_force_flush = fr_ticks;
	return 0;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_stateunload                                              */
/* Returns:     Nil                                                         */
/* Parameters:  Nil                                                         */
/*                                                                          */
/* Release and destroy any resources acquired or initialised so that        */
/* IPFilter can be unloaded or re-initialised.                              */
/* ------------------------------------------------------------------------ */
void fr_stateunload()
{
	ipftq_t *ifq, *ifqnext;
	ipstate_t *is;

	while ((is = ips_list) != NULL)
		fr_delstate(is, ISL_UNLOAD);

	/*
	 * Proxy timeout queues are not cleaned here because although they
	 * exist on the state list, appr_unload is called after fr_stateunload
	 * and the proxies actually are responsible for them being created.
	 * Should the proxy timeouts have their own list?  There's no real
	 * justification as this is the only complicationA
	 */
	for (ifq = ips_utqe; ifq != NULL; ifq = ifqnext) {
		ifqnext = ifq->ifq_next;
		if (((ifq->ifq_flags & IFQF_PROXY) == 0) &&
		    (fr_deletetimeoutqueue(ifq) == 0))
			fr_freetimeoutqueue(ifq);
	}

	ips_stats.iss_inuse = 0;
	ips_num = 0;

	if (fr_state_init == 1) {
		fr_sttab_destroy(ips_tqtqb);
		MUTEX_DESTROY(&ips_udptq.ifq_lock);
		MUTEX_DESTROY(&ips_icmptq.ifq_lock);
		MUTEX_DESTROY(&ips_udpacktq.ifq_lock);
		MUTEX_DESTROY(&ips_icmpacktq.ifq_lock);
		MUTEX_DESTROY(&ips_iptq.ifq_lock);
		MUTEX_DESTROY(&ips_deletetq.ifq_lock);
	}

	if (ips_table != NULL) {
		KFREES(ips_table, fr_statesize * sizeof(*ips_table));
		ips_table = NULL;
	}

	if (ips_seed != NULL) {
		KFREES(ips_seed, fr_statesize * sizeof(*ips_seed));
		ips_seed = NULL;
	}

	if (ips_stats.iss_bucketlen != NULL) {
		KFREES(ips_stats.iss_bucketlen, fr_statesize * sizeof(u_long));
		ips_stats.iss_bucketlen = NULL;
	}

	if (fr_state_maxbucket_reset == 1)
		fr_state_maxbucket = 0;

	if (fr_state_init == 1) {
		fr_state_init = 0;
		RW_DESTROY(&ipf_state);
		MUTEX_DESTROY(&ipf_stinsert);
	}
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_statetstats                                              */
/* Returns:     ips_state_t* - pointer to state stats structure             */
/* Parameters:  Nil                                                         */
/*                                                                          */
/* Put all the current numbers and pointers into a single struct and return */
/* a pointer to it.                                                         */
/* ------------------------------------------------------------------------ */
static ips_stat_t *fr_statetstats()
{
	ips_stats.iss_active = ips_num;
	ips_stats.iss_statesize = fr_statesize;
	ips_stats.iss_statemax = fr_statemax;
	ips_stats.iss_table = ips_table;
	ips_stats.iss_list = ips_list;
	ips_stats.iss_ticks = fr_ticks;
	return &ips_stats;
}

/* ------------------------------------------------------------------------ */
/* Function:    fr_state_remove                                             */
/* Returns:     int - 0 == success, != 0 == failure                         */
/* Parameters:  data(I) - pointer to state structure to delete from table   */
/*                                                                          */
/* Search for a state structure that matches the one passed, according to   */
/* the IP addresses and other protocol specific information.                */
/* ------------------------------------------------------------------------ */
static int fr_state_remove(data)
caddr_t data;
{
	ipstate_t *sp, st;
	int error;

	sp = &st;
	error = fr_inobj(data, &st, IPFOBJ_IPSTATE);
	if (error)
		return EFAULT;

	WRITE_ENTER(&ipf_state);
	for (sp = ips_list; sp; sp = sp->is_next)
		if ((sp->is_p == st.is_p) && (sp->is_v == st.is_v) &&
		    !bcmp((caddr_t)&sp->is_src, (caddr_t)&st.is_src,
			  sizeof(st.is_src)) &&
		    !bcmp((caddr_t)&sp->is_dst, (caddr_t)&st.is_src,
			  sizeof(st.is_dst)) &&
		    !bcmp((caddr_t)&sp->is_ps, (caddr_t)&st.is_ps,
			  sizeof(st.is_ps))) {
			fr_delstate(sp, ISL_REMOVE);
			RWLOCK_EXIT(&ipf_state);
			return 0;
		}
	RWLOCK_EXIT(&ipf_state);
	return ESRCH;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_state_ioctl                                              */
/* Returns:     int - 0 == success, != 0 == failure                         */
/* Parameters:  data(I) - pointer to ioctl data                             */
/*              cmd(I)  - ioctl command integer                             */
/*              mode(I) - file mode bits used with open                     */
/*                                                                          */
/* Processes an ioctl call made to operate on the IP Filter state device.   */
/* ------------------------------------------------------------------------ */
int fr_state_ioctl(data, cmd, mode, uid, ctx)
caddr_t data;
ioctlcmd_t cmd;
int mode, uid;
void *ctx;
{
	int arg, ret, error = 0;
	SPL_INT(s);

	switch (cmd)
	{
	/*
	 * Delete an entry from the state table.
	 */
	case SIOCDELST :
		error = fr_state_remove(data);
		break;

	/*
	 * Flush the state table
	 */
	case SIOCIPFFL :
		error = BCOPYIN(data, (char *)&arg, sizeof(arg));
		if (error != 0) {
			error = EFAULT;
		} else {
			WRITE_ENTER(&ipf_state);
			ret = fr_state_flush(arg, 4);
			RWLOCK_EXIT(&ipf_state);
			error = BCOPYOUT((char *)&ret, data, sizeof(ret));
			if (error != 0)
				error = EFAULT;
		}
		break;

#ifdef	USE_INET6
	case SIOCIPFL6 :
		error = BCOPYIN(data, (char *)&arg, sizeof(arg));
		if (error != 0) {
			error = EFAULT;
		} else {
			WRITE_ENTER(&ipf_state);
			ret = fr_state_flush(arg, 6);
			RWLOCK_EXIT(&ipf_state);
			error = BCOPYOUT((char *)&ret, data, sizeof(ret));
			if (error != 0)
				error = EFAULT;
		}
		break;
#endif
#ifdef	IPFILTER_LOG
	/*
	 * Flush the state log.
	 */
	case SIOCIPFFB :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			int tmp;

			tmp = ipflog_clear(IPL_LOGSTATE);
			error = BCOPYOUT((char *)&tmp, data, sizeof(tmp));
			if (error != 0)
				error = EFAULT;
		}
		break;

	/*
	 * Turn logging of state information on/off.
	 */
	case SIOCSETLG :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			error = BCOPYIN((char *)data, (char *)&ipstate_logging,
					sizeof(ipstate_logging));
			if (error != 0)
				error = EFAULT;
		}
		break;

	/*
	 * Return the current state of logging.
	 */
	case SIOCGETLG :
		error = BCOPYOUT((char *)&ipstate_logging, (char *)data,
				 sizeof(ipstate_logging));
		if (error != 0)
			error = EFAULT;
		break;

	/*
	 * Return the number of bytes currently waiting to be read.
	 */
	case FIONREAD :
		arg = iplused[IPL_LOGSTATE];	/* returned in an int */
		error = BCOPYOUT((char *)&arg, data, sizeof(arg));
		if (error != 0)
			error = EFAULT;
		break;
#endif

	/*
	 * Get the current state statistics.
	 */
	case SIOCGETFS :
		error = fr_outobj(data, fr_statetstats(), IPFOBJ_STATESTAT);
		break;

	/*
	 * Lock/Unlock the state table.  (Locking prevents any changes, which
	 * means no packets match).
	 */
	case SIOCSTLCK :
		if (!(mode & FWRITE)) {
			error = EPERM;
		} else {
			error = fr_lock(data, &fr_state_lock);
		}
		break;

	/*
	 * Add an entry to the current state table.
	 */
	case SIOCSTPUT :
		if (!fr_state_lock || !(mode &FWRITE)) {
			error = EACCES;
			break;
		}
		error = fr_stputent(data);
		break;

	/*
	 * Get a state table entry.
	 */
	case SIOCSTGET :
		if (!fr_state_lock) {
			error = EACCES;
			break;
		}
		error = fr_stgetent(data);
		break;

	/*
	 * Return a copy of the hash table bucket lengths
	 */
	case SIOCSTAT1 :
		error = BCOPYOUT(ips_stats.iss_bucketlen, data,
				 fr_statesize * sizeof(u_long));
		if (error != 0)
			error = EFAULT;
		break;

	case SIOCGENITER :
	    {
		ipftoken_t *token;
		ipfgeniter_t iter;

		error = fr_inobj(data, &iter, IPFOBJ_GENITER);
		if (error != 0)
			break;

		SPL_SCHED(s);
		token = ipf_findtoken(IPFGENITER_STATE, uid, ctx);
		if (token != NULL)
			error = fr_stateiter(token, &iter);
		else
			error = ESRCH;
		RWLOCK_EXIT(&ipf_tokens);
		SPL_X(s);
		break;
	    }

	case SIOCGTABL :
		error = fr_stgettable(data);
		break;

	case SIOCIPFDELTOK :
		error = BCOPYIN(data, (char *)&arg, sizeof(arg));
		if (error != 0) {
			error = EFAULT;
		} else {
			SPL_SCHED(s);
			error = ipf_deltoken(arg, uid, ctx);
			SPL_X(s);
		}
		break;

	case SIOCGTQTAB :
		error = fr_outobj(data, ips_tqtqb, IPFOBJ_STATETQTAB);
		break;

	default :
		error = EINVAL;
		break;
	}
	return error;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_stgetent                                                 */
/* Returns:     int - 0 == success, != 0 == failure                         */
/* Parameters:  data(I) - pointer to state structure to retrieve from table */
/*                                                                          */
/* Copy out state information from the kernel to a user space process.  If  */
/* there is a filter rule associated with the state entry, copy that out    */
/* as well.  The entry to copy out is taken from the value of "ips_next" in */
/* the struct passed in and if not null and not found in the list of current*/
/* state entries, the retrieval fails.                                      */
/* ------------------------------------------------------------------------ */
int fr_stgetent(data)
caddr_t data;
{
	ipstate_t *is, *isn;
	ipstate_save_t ips;
	int error;

	error = fr_inobj(data, &ips, IPFOBJ_STATESAVE);
	if (error != 0)
		return error;

	isn = ips.ips_next;
	if (isn == NULL) {
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
	ips.ips_rule = isn->is_rule;
	if (isn->is_rule != NULL)
		bcopy((char *)isn->is_rule, (char *)&ips.ips_fr,
		      sizeof(ips.ips_fr));
	error = fr_outobj(data, &ips, IPFOBJ_STATESAVE);
	return error;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_stputent                                                 */
/* Returns:     int - 0 == success, != 0 == failure                         */
/* Parameters:  data(I) - pointer to state information struct               */
/*                                                                          */
/* This function implements the SIOCSTPUT ioctl: insert a state entry into  */
/* the state table.  If the state info. includes a pointer to a filter rule */
/* then also add in an orphaned rule (will not show up in any "ipfstat -io" */
/* output.                                                                  */
/* ------------------------------------------------------------------------ */
int fr_stputent(data)
caddr_t data;
{
	ipstate_t *is, *isn;
	ipstate_save_t ips;
	int error, out, i;
	frentry_t *fr;
	char *name;

	error = fr_inobj(data, &ips, IPFOBJ_STATESAVE);
	if (error)
		return EFAULT;

	KMALLOC(isn, ipstate_t *);
	if (isn == NULL)
		return ENOMEM;

	bcopy((char *)&ips.ips_is, (char *)isn, sizeof(*isn));
	bzero((char *)isn, offsetof(struct ipstate, is_pkts));
	isn->is_sti.tqe_pnext = NULL;
	isn->is_sti.tqe_next = NULL;
	isn->is_sti.tqe_ifq = NULL;
	isn->is_sti.tqe_parent = isn;
	isn->is_ifp[0] = NULL;
	isn->is_ifp[1] = NULL;
	isn->is_ifp[2] = NULL;
	isn->is_ifp[3] = NULL;
	isn->is_sync = NULL;
	fr = ips.ips_rule;

	if (fr == NULL) {
		READ_ENTER(&ipf_state);
		fr_stinsert(isn, 0);
		MUTEX_EXIT(&isn->is_lock);
		RWLOCK_EXIT(&ipf_state);
		return 0;
	}

	if (isn->is_flags & SI_NEWFR) {
		KMALLOC(fr, frentry_t *);
		if (fr == NULL) {
			KFREE(isn);
			return ENOMEM;
		}
		bcopy((char *)&ips.ips_fr, (char *)fr, sizeof(*fr));
		out = fr->fr_flags & FR_OUTQUE ? 1 : 0;
		isn->is_rule = fr;
		ips.ips_is.is_rule = fr;
		MUTEX_NUKE(&fr->fr_lock);
		MUTEX_INIT(&fr->fr_lock, "state filter rule lock");

		/*
		 * Look up all the interface names in the rule.
		 */
		for (i = 0; i < 4; i++) {
			name = fr->fr_ifnames[i];
			fr->fr_ifas[i] = fr_resolvenic(name, fr->fr_v);
			name = isn->is_ifname[i];
			isn->is_ifp[i] = fr_resolvenic(name, isn->is_v);
		}

		fr->fr_ref = 0;
		fr->fr_dsize = 0;
		fr->fr_data = NULL;
		fr->fr_type = FR_T_NONE;

		fr_resolvedest(&fr->fr_tifs[0], fr->fr_v);
		fr_resolvedest(&fr->fr_tifs[1], fr->fr_v);
		fr_resolvedest(&fr->fr_dif, fr->fr_v);

		/*
		 * send a copy back to userland of what we ended up
		 * to allow for verification.
		 */
		error = fr_outobj(data, &ips, IPFOBJ_STATESAVE);
		if (error) {
			KFREE(isn);
			MUTEX_DESTROY(&fr->fr_lock);
			KFREE(fr);
			return EFAULT;
		}
		READ_ENTER(&ipf_state);
		fr_stinsert(isn, 0);
		MUTEX_EXIT(&isn->is_lock);
		RWLOCK_EXIT(&ipf_state);

	} else {
		READ_ENTER(&ipf_state);
		for (is = ips_list; is; is = is->is_next)
			if (is->is_rule == fr) {
				fr_stinsert(isn, 0);
				MUTEX_EXIT(&isn->is_lock);
				break;
			}

		if (is == NULL) {
			KFREE(isn);
			isn = NULL;
		}
		RWLOCK_EXIT(&ipf_state);

		return (isn == NULL) ? ESRCH : 0;
	}

	return 0;
}


/* ------------------------------------------------------------------------ */
/* Function:   fr_stinsert                                                  */
/* Returns:    Nil                                                          */
/* Parameters: is(I)  - pointer to state structure                          */
/*             rev(I) - flag indicating forward/reverse direction of packet */
/*                                                                          */
/* Inserts a state structure into the hash table (for lookups) and the list */
/* of state entries (for enumeration).  Resolves all of the interface names */
/* to pointers and adjusts running stats for the hash table as appropriate. */
/*                                                                          */
/* Locking: it is assumed that some kind of lock on ipf_state is held.      */
/*          Exits with is_lock initialised and held.                        */
/* ------------------------------------------------------------------------ */
void fr_stinsert(is, rev)
ipstate_t *is;
int rev;
{
	frentry_t *fr;
	u_int hv;
	int i;

	MUTEX_INIT(&is->is_lock, "ipf state entry");

	fr = is->is_rule;
	if (fr != NULL) {
		MUTEX_ENTER(&fr->fr_lock);
		fr->fr_ref++;
		fr->fr_statecnt++;
		MUTEX_EXIT(&fr->fr_lock);
	}

	/*
	 * Look up all the interface names in the state entry.
	 */
	for (i = 0; i < 4; i++) {
		if (is->is_ifp[i] != NULL)
			continue;
		is->is_ifp[i] = fr_resolvenic(is->is_ifname[i], is->is_v);
	}

	/*
	 * If we could trust is_hv, then the modulous would not be needed, but
	 * when running with IPFILTER_SYNC, this stops bad values.
	 */
	hv = is->is_hv % fr_statesize;
	is->is_hv = hv;

	/*
	 * We need to get both of these locks...the first because it is
	 * possible that once the insert is complete another packet might
	 * come along, match the entry and want to update it.
	 */
	MUTEX_ENTER(&is->is_lock);
	MUTEX_ENTER(&ipf_stinsert);

	/*
	 * add into list table.
	 */
	if (ips_list != NULL)
		ips_list->is_pnext = &is->is_next;
	is->is_pnext = &ips_list;
	is->is_next = ips_list;
	ips_list = is;

	if (ips_table[hv] != NULL)
		ips_table[hv]->is_phnext = &is->is_hnext;
	else
		ips_stats.iss_inuse++;
	is->is_phnext = ips_table + hv;
	is->is_hnext = ips_table[hv];
	ips_table[hv] = is;
	ips_stats.iss_bucketlen[hv]++;
	ips_num++;
	MUTEX_EXIT(&ipf_stinsert);

	fr_setstatequeue(is, rev);
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_addstate                                                 */
/* Returns:     ipstate_t* - NULL == failure, else pointer to new state     */
/* Parameters:  fin(I)    - pointer to packet information                   */
/*              stsave(O) - pointer to place to save pointer to created     */
/*                          state structure.                                */
/*              flags(I)  - flags to use when creating the structure        */
/*                                                                          */
/* Creates a new IP state structure from the packet information collected.  */
/* Inserts it into the state table and appends to the bottom of the active  */
/* list.  If the capacity of the table has reached the maximum allowed then */
/* the call will fail and a flush is scheduled for the next timeout call.   */
/*                                                                          */
/* NOTE: The use of stsave to point to nat_state will result in memory      */
/*       corruption.  It should only be used to point to objects that will  */
/*       either outlive this (not expired) or will deref the ip_state_t     */
/*       when they are deleted.                                             */
/* ------------------------------------------------------------------------ */
ipstate_t *fr_addstate(fin, stsave, flags)
fr_info_t *fin;
ipstate_t **stsave;
u_int flags;
{
	ipstate_t *is, ips;
	struct icmp *ic;
	u_int pass, hv;
	frentry_t *fr;
	tcphdr_t *tcp;
	grehdr_t *gre;
	int out;

	if (fr_state_lock ||
	    (fin->fin_flx & (FI_SHORT|FI_STATE|FI_FRAGBODY|FI_BAD)))
		return NULL;

	if ((fin->fin_flx & FI_OOW) && !(fin->fin_tcpf & TH_SYN))
		return NULL;

	/*
	 * If a "keep state" rule has reached the maximum number of references
	 * to it, then schedule an automatic flush in case we can clear out
	 * some "dead old wood".  Note that because the lock isn't held on
	 * fr it is possible that we could overflow.  The cost of overflowing
	 * is being ignored here as the number by which it can overflow is
	 * a product of the number of simultaneous threads that could be
	 * executing in here, so a limit of 100 won't result in 200, but could
	 * result in 101 or 102.
	 */
	fr = fin->fin_fr;
	if (fr != NULL) {
		if ((ips_num >= fr_statemax) && (fr->fr_statemax == 0)) {
			ATOMIC_INCL(ips_stats.iss_max);
			fr_state_doflush = 1;
			return NULL;
		}
		if ((fr->fr_statemax != 0) &&
		    (fr->fr_statecnt >= fr->fr_statemax)) {
			ATOMIC_INCL(ips_stats.iss_maxref);
			return NULL;
		}
	}

	pass = (fr == NULL) ? 0 : fr->fr_flags;

	ic = NULL;
	tcp = NULL;
	out = fin->fin_out;
	is = &ips;
	bzero((char *)is, sizeof(*is));
	is->is_die = 1 + fr_ticks;

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
		/*
		 * For ICMPv6, we check to see if the destination address is
		 * a multicast address.  If it is, do not include it in the
		 * calculation of the hash because the correct reply will come
		 * back from a real address, not a multicast address.
		 */
		if ((is->is_p == IPPROTO_ICMPV6) &&
		    IN6_IS_ADDR_MULTICAST(&is->is_dst.in6)) {
			/*
			 * So you can do keep state with neighbour discovery.
			 *
			 * Here we could use the address from the neighbour
			 * solicit message to put in the state structure and
			 * we could use that without a wildcard flag too...
			 */
			flags |= SI_W_DADDR;
			hv -= is->is_daddr;
		} else {
			hv += is->is_dst.i6[1];
			hv += is->is_dst.i6[2];
			hv += is->is_dst.i6[3];
		}
		hv += is->is_src.i6[1];
		hv += is->is_src.i6[2];
		hv += is->is_src.i6[3];
	}
#endif
	if ((fin->fin_v == 4) &&
	    (fin->fin_flx & (FI_MULTICAST|FI_BROADCAST|FI_MBCAST))) {
		if (fin->fin_out == 0) {
			flags |= SI_W_DADDR|SI_CLONE;
			hv -= is->is_daddr;
		} else {
			flags |= SI_W_SADDR|SI_CLONE;
			hv -= is->is_saddr;
		}
	}

	switch (is->is_p)
	{
#ifdef	USE_INET6
	case IPPROTO_ICMPV6 :
		ic = fin->fin_dp;

		switch (ic->icmp_type)
		{
		case ICMP6_ECHO_REQUEST :
			is->is_icmp.ici_type = ic->icmp_type;
			hv += (is->is_icmp.ici_id = ic->icmp_id);
			break;
		case ICMP6_MEMBERSHIP_QUERY :
		case ND_ROUTER_SOLICIT :
		case ND_NEIGHBOR_SOLICIT :
		case ICMP6_NI_QUERY :
			is->is_icmp.ici_type = ic->icmp_type;
			break;
		default :
			return NULL;
		}
		ATOMIC_INCL(ips_stats.iss_icmp);
		break;
#endif
	case IPPROTO_ICMP :
		ic = fin->fin_dp;

		switch (ic->icmp_type)
		{
		case ICMP_ECHO :
		case ICMP_TSTAMP :
		case ICMP_IREQ :
		case ICMP_MASKREQ :
			is->is_icmp.ici_type = ic->icmp_type;
			hv += (is->is_icmp.ici_id = ic->icmp_id);
			break;
		default :
			return NULL;
		}
		ATOMIC_INCL(ips_stats.iss_icmp);
		break;

	case IPPROTO_GRE :
		gre = fin->fin_dp;

		is->is_gre.gs_flags = gre->gr_flags;
		is->is_gre.gs_ptype = gre->gr_ptype;
		if (GRE_REV(is->is_gre.gs_flags) == 1) {
			is->is_call[0] = fin->fin_data[0];
			is->is_call[1] = fin->fin_data[1];
		}
		break;

	case IPPROTO_TCP :
		tcp = fin->fin_dp;

		if (tcp->th_flags & TH_RST)
			return NULL;
		/*
		 * The endian of the ports doesn't matter, but the ack and
		 * sequence numbers do as we do mathematics on them later.
		 */
		is->is_sport = htons(fin->fin_data[0]);
		is->is_dport = htons(fin->fin_data[1]);
		if ((flags & (SI_W_DPORT|SI_W_SPORT)) == 0) {
			hv += is->is_sport;
			hv += is->is_dport;
		}

		/*
		 * If this is a real packet then initialise fields in the
		 * state information structure from the TCP header information.
		 */

		is->is_maxdwin = 1;
		is->is_maxswin = ntohs(tcp->th_win);
		if (is->is_maxswin == 0)
			is->is_maxswin = 1;

		if ((fin->fin_flx & FI_IGNORE) == 0) {
			is->is_send = ntohl(tcp->th_seq) + fin->fin_dlen -
				      (TCP_OFF(tcp) << 2) +
				      ((tcp->th_flags & TH_SYN) ? 1 : 0) +
				      ((tcp->th_flags & TH_FIN) ? 1 : 0);
			is->is_maxsend = is->is_send;

			/*
			 * Window scale option is only present in
			 * SYN/SYN-ACK packet.
			 */
			if ((tcp->th_flags & ~(TH_FIN|TH_ACK|TH_ECNALL)) ==
			    TH_SYN &&
			    (TCP_OFF(tcp) > (sizeof(tcphdr_t) >> 2))) {
				if (fr_tcpoptions(fin, tcp,
					      &is->is_tcp.ts_data[0]) == -1) {
					fin->fin_flx |= FI_BAD;
				}
			}

			if ((fin->fin_out != 0) && (pass & FR_NEWISN) != 0) {
				fr_checknewisn(fin, is);
				fr_fixoutisn(fin, is);
			}

			if ((tcp->th_flags & TH_OPENING) == TH_SYN)
				flags |= IS_TCPFSM;
			else {
				is->is_maxdwin = is->is_maxswin * 2;
				is->is_dend = ntohl(tcp->th_ack);
				is->is_maxdend = ntohl(tcp->th_ack);
				is->is_maxdwin *= 2;
			}
		}

		/*
		 * If we're creating state for a starting connection, start the
		 * timer on it as we'll never see an error if it fails to
		 * connect.
		 */
		ATOMIC_INCL(ips_stats.iss_tcp);
		break;

	case IPPROTO_UDP :
		tcp = fin->fin_dp;

		is->is_sport = htons(fin->fin_data[0]);
		is->is_dport = htons(fin->fin_data[1]);
		if ((flags & (SI_W_DPORT|SI_W_SPORT)) == 0) {
			hv += tcp->th_dport;
			hv += tcp->th_sport;
		}
		ATOMIC_INCL(ips_stats.iss_udp);
		break;

	default :
		break;
	}
	hv = DOUBLE_HASH(hv);
	is->is_hv = hv;
	is->is_rule = fr;
	is->is_flags = flags & IS_INHERITED;

	/*
	 * Look for identical state.
	 */
	for (is = ips_table[is->is_hv % fr_statesize]; is != NULL;
	     is = is->is_hnext) {
		if (bcmp(&ips.is_src, &is->is_src,
			 offsetof(struct ipstate, is_ps) -
			 offsetof(struct ipstate, is_src)) == 0)
			break;
	}
	if (is != NULL)
		return NULL;

	if (ips_stats.iss_bucketlen[hv] >= fr_state_maxbucket) {
		ATOMIC_INCL(ips_stats.iss_bucketfull);
		return NULL;
	}
	KMALLOC(is, ipstate_t *);
	if (is == NULL) {
		ATOMIC_INCL(ips_stats.iss_nomem);
		return NULL;
	}
	bcopy((char *)&ips, (char *)is, sizeof(*is));
	/*
	 * Do not do the modulous here, it is done in fr_stinsert().
	 */
	if (fr != NULL) {
		(void) strncpy(is->is_group, fr->fr_group, FR_GROUPLEN);
		if (fr->fr_age[0] != 0) {
			is->is_tqehead[0] = fr_addtimeoutqueue(&ips_utqe,
							       fr->fr_age[0]);
			is->is_sti.tqe_flags |= TQE_RULEBASED;
		}
		if (fr->fr_age[1] != 0) {
			is->is_tqehead[1] = fr_addtimeoutqueue(&ips_utqe,
							       fr->fr_age[1]);
			is->is_sti.tqe_flags |= TQE_RULEBASED;
		}

		is->is_tag = fr->fr_logtag;

		/*
		 * The name '-' is special for network interfaces and causes
		 * a NULL name to be present, always, allowing packets to
		 * match it, regardless of their interface.
		 */
		if ((fin->fin_ifp == NULL) ||
		    (fr->fr_ifnames[out << 1][0] == '-' &&
		     fr->fr_ifnames[out << 1][1] == '\0')) {
			is->is_ifp[out << 1] = fr->fr_ifas[0];
			strncpy(is->is_ifname[out << 1], fr->fr_ifnames[0],
				sizeof(fr->fr_ifnames[0]));
		} else {
			is->is_ifp[out << 1] = fin->fin_ifp;
			COPYIFNAME(is->is_v, fin->fin_ifp,
				   is->is_ifname[out << 1]);
		}

		is->is_ifp[(out << 1) + 1] = fr->fr_ifas[1];
		strncpy(is->is_ifname[(out << 1) + 1], fr->fr_ifnames[1],
			sizeof(fr->fr_ifnames[1]));

		is->is_ifp[(1 - out) << 1] = fr->fr_ifas[2];
		strncpy(is->is_ifname[((1 - out) << 1)], fr->fr_ifnames[2],
			sizeof(fr->fr_ifnames[2]));

		is->is_ifp[((1 - out) << 1) + 1] = fr->fr_ifas[3];
		strncpy(is->is_ifname[((1 - out) << 1) + 1], fr->fr_ifnames[3],
			sizeof(fr->fr_ifnames[3]));
	} else {
		pass = fr_flags;
		is->is_tag = FR_NOLOGTAG;

		if (fin->fin_ifp != NULL) {
			is->is_ifp[out << 1] = fin->fin_ifp;
			COPYIFNAME(is->is_v, fin->fin_ifp,
				   is->is_ifname[out << 1]);
		}
	}

	/*
	 * It may seem strange to set is_ref to 2, but fr_check() will call
	 * fr_statederef() after calling fr_addstate() and the idea is to
	 * have it exist at the end of fr_check() with is_ref == 1.
	 */
	is->is_ref = 2;
	is->is_pass = pass;
	is->is_pkts[0] = 0, is->is_bytes[0] = 0;
	is->is_pkts[1] = 0, is->is_bytes[1] = 0;
	is->is_pkts[2] = 0, is->is_bytes[2] = 0;
	is->is_pkts[3] = 0, is->is_bytes[3] = 0;
	if ((fin->fin_flx & FI_IGNORE) == 0) {
		is->is_pkts[out] = 1;
		is->is_bytes[out] = fin->fin_plen;
		is->is_flx[out][0] = fin->fin_flx & FI_CMP;
		is->is_flx[out][0] &= ~FI_OOW;
	}

	if (pass & FR_STSTRICT)
		is->is_flags |= IS_STRICT;

	if (pass & FR_STATESYNC)
		is->is_flags |= IS_STATESYNC;

	/*
	 * We want to check everything that is a property of this packet,
	 * but we don't (automatically) care about it's fragment status as
	 * this may change.
	 */
	is->is_v = fin->fin_v;
	is->is_opt[0] = fin->fin_optmsk;
	is->is_optmsk[0] = 0xffffffff;
	is->is_optmsk[1] = 0xffffffff;
	if (is->is_v == 6) {
		is->is_opt[0] &= ~0x8;
		is->is_optmsk[0] &= ~0x8;
		is->is_optmsk[1] &= ~0x8;
	}
	is->is_me = stsave;
	is->is_sec = fin->fin_secmsk;
	is->is_secmsk = 0xffff;
	is->is_auth = fin->fin_auth;
	is->is_authmsk = 0xffff;
	if (flags & (SI_WILDP|SI_WILDA)) {
		ATOMIC_INCL(ips_stats.iss_wild);
	}
	is->is_rulen = fin->fin_rule;


	if (pass & FR_LOGFIRST)
		is->is_pass &= ~(FR_LOGFIRST|FR_LOG);

	READ_ENTER(&ipf_state);

	fr_stinsert(is, fin->fin_rev);

	if (fin->fin_p == IPPROTO_TCP) {
		/*
		* If we're creating state for a starting connection, start the
		* timer on it as we'll never see an error if it fails to
		* connect.
		*/
		(void) fr_tcp_age(&is->is_sti, fin, ips_tqtqb, is->is_flags);
		MUTEX_EXIT(&is->is_lock);
#ifdef	IPFILTER_SCAN
		if ((is->is_flags & SI_CLONE) == 0)
			(void) ipsc_attachis(is);
#endif
	} else {
		MUTEX_EXIT(&is->is_lock);
	}
#ifdef	IPFILTER_SYNC
	if ((is->is_flags & IS_STATESYNC) && ((is->is_flags & SI_CLONE) == 0))
		is->is_sync = ipfsync_new(SMC_STATE, fin, is);
#endif
	if (ipstate_logging)
		ipstate_log(is, ISL_NEW);

	RWLOCK_EXIT(&ipf_state);
	fin->fin_state = is;
	fin->fin_rev = IP6_NEQ(&is->is_dst, &fin->fin_daddr);
	fin->fin_flx |= FI_STATE;
	if (fin->fin_flx & FI_FRAG)
		(void) fr_newfrag(fin, pass ^ FR_KEEPSTATE);

	return is;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_tcpoptions                                               */
/* Returns:     int - 1 == packet matches state entry, 0 == it does not,    */
/*                   -1 == packet has bad TCP options data                  */
/* Parameters:  fin(I) - pointer to packet information                      */
/*              tcp(I) - pointer to TCP packet header                       */
/*              td(I)  - pointer to TCP data held as part of the state      */
/*                                                                          */
/* Look after the TCP header for any options and deal with those that are   */
/* present.  Record details about those that we recogise.                   */
/* ------------------------------------------------------------------------ */
static int fr_tcpoptions(fin, tcp, td)
fr_info_t *fin;
tcphdr_t *tcp;
tcpdata_t *td;
{
	int off, mlen, ol, i, len, retval;
	char buf[64], *s, opt;
	mb_t *m = NULL;

	len = (TCP_OFF(tcp) << 2);
	if (fin->fin_dlen < len)
		return 0;
	len -= sizeof(*tcp);

	off = fin->fin_plen - fin->fin_dlen + sizeof(*tcp) + fin->fin_ipoff;

	m = fin->fin_m;
	mlen = MSGDSIZE(m) - off;
	if (len > mlen) {
		len = mlen;
		retval = 0;
	} else {
		retval = 1;
	}

	COPYDATA(m, off, len, buf);

	for (s = buf; len > 0; ) {
		opt = *s;
		if (opt == TCPOPT_EOL)
			break;
		else if (opt == TCPOPT_NOP)
			ol = 1;
		else {
			if (len < 2)
				break;
			ol = (int)*(s + 1);
			if (ol < 2 || ol > len)
				break;

			/*
			 * Extract the TCP options we are interested in out of
			 * the header and store them in the the tcpdata struct.
			 */
			switch (opt)
			{
			case TCPOPT_WINDOW :
				if (ol == TCPOLEN_WINDOW) {
					i = (int)*(s + 2);
					if (i > TCP_WSCALE_MAX)
						i = TCP_WSCALE_MAX;
					else if (i < 0)
						i = 0;
					td->td_winscale = i;
					td->td_winflags |= TCP_WSCALE_SEEN|
							   TCP_WSCALE_FIRST;
				} else
					retval = -1;
				break;
			case TCPOPT_MAXSEG :
				/*
				 * So, if we wanted to set the TCP MAXSEG,
				 * it should be done here...
				 */
				if (ol == TCPOLEN_MAXSEG) {
					i = (int)*(s + 2);
					i <<= 8;
					i += (int)*(s + 3);
					td->td_maxseg = i;
				} else
					retval = -1;
				break;
			case TCPOPT_SACK_PERMITTED :
				if (ol == TCPOLEN_SACK_PERMITTED)
					td->td_winflags |= TCP_SACK_PERMIT;
				else
					retval = -1;
				break;
			}
		}
		len -= ol;
		s += ol;
	}
	return retval;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_tcpstate                                                 */
/* Returns:     int - 1 == packet matches state entry, 0 == it does not     */
/* Parameters:  fin(I)   - pointer to packet information                    */
/*              tcp(I)   - pointer to TCP packet header                     */
/*              is(I)  - pointer to master state structure                  */
/*                                                                          */
/* Check to see if a packet with TCP headers fits within the TCP window.    */
/* Change timeout depending on whether new packet is a SYN-ACK returning    */
/* for a SYN or a RST or FIN which indicate time to close up shop.          */
/* ------------------------------------------------------------------------ */
static int fr_tcpstate(fin, tcp, is)
fr_info_t *fin;
tcphdr_t *tcp;
ipstate_t *is;
{
	int source, ret = 0, flags;
	tcpdata_t  *fdata, *tdata;

	source = !fin->fin_rev;
	if (((is->is_flags & IS_TCPFSM) != 0) && (source == 1) && 
	    (ntohs(is->is_sport) != fin->fin_data[0]))
		source = 0;
	fdata = &is->is_tcp.ts_data[!source];
	tdata = &is->is_tcp.ts_data[source];

	MUTEX_ENTER(&is->is_lock);

	/*
	 * If a SYN packet is received for a connection that is on the way out
	 * but hasn't yet departed then advance this session along the way.
	 */
	if ((tcp->th_flags & TH_OPENING) == TH_SYN) {
		if ((is->is_state[0] > IPF_TCPS_ESTABLISHED) &&
		    (is->is_state[1] > IPF_TCPS_ESTABLISHED)) {
			is->is_state[!source] = IPF_TCPS_CLOSED;
			fr_movequeue(&is->is_sti, is->is_sti.tqe_ifq,
				     &ips_deletetq);
			MUTEX_EXIT(&is->is_lock);
			return 0;
		}
	}

	ret = fr_tcpinwindow(fin, fdata, tdata, tcp, is->is_flags);
	if (ret > 0) {
#ifdef	IPFILTER_SCAN
		if (is->is_flags & (IS_SC_CLIENT|IS_SC_SERVER)) {
			ipsc_packet(fin, is);
			if (FR_ISBLOCK(is->is_pass)) {
				MUTEX_EXIT(&is->is_lock);
				return 1;
			}
		}
#endif

		/*
		 * Nearing end of connection, start timeout.
		 */
		ret = fr_tcp_age(&is->is_sti, fin, ips_tqtqb, is->is_flags);
		if (ret == 0) {
			MUTEX_EXIT(&is->is_lock);
			return 0;
		}

		/*
		 * set s0's as appropriate.  Use syn-ack packet as it
		 * contains both pieces of required information.
		 */
		/*
		 * Window scale option is only present in SYN/SYN-ACK packet.
		 * Compare with ~TH_FIN to mask out T/TCP setups.
		 */
		flags = tcp->th_flags & ~(TH_FIN|TH_ECNALL);
		if (flags == (TH_SYN|TH_ACK)) {
			is->is_s0[source] = ntohl(tcp->th_ack);
			is->is_s0[!source] = ntohl(tcp->th_seq) + 1;
			if ((TCP_OFF(tcp) > (sizeof(tcphdr_t) >> 2))) {
				if (fr_tcpoptions(fin, tcp, fdata) == -1)
					fin->fin_flx |= FI_BAD;
			}
			if ((fin->fin_out != 0) && (is->is_pass & FR_NEWISN))
				fr_checknewisn(fin, is);
		} else if (flags == TH_SYN) {
			is->is_s0[source] = ntohl(tcp->th_seq) + 1;
			if ((TCP_OFF(tcp) > (sizeof(tcphdr_t) >> 2))) {
				if (fr_tcpoptions(fin, tcp, fdata) == -1)
					fin->fin_flx |= FI_BAD;
			}

			if ((fin->fin_out != 0) && (is->is_pass & FR_NEWISN))
				fr_checknewisn(fin, is);

		}
		ret = 1;
	} else {
		fin->fin_flx |= FI_OOW;
	}
	MUTEX_EXIT(&is->is_lock);
	return ret;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_checknewisn                                              */
/* Returns:     Nil                                                         */
/* Parameters:  fin(I)   - pointer to packet information                    */
/*              is(I)  - pointer to master state structure                  */
/*                                                                          */
/* Check to see if this TCP connection is expecting and needs a new         */
/* sequence number for a particular direction of the connection.            */
/*                                                                          */
/* NOTE: This does not actually change the sequence numbers, only gets new  */
/* one ready.                                                               */
/* ------------------------------------------------------------------------ */
static void fr_checknewisn(fin, is)
fr_info_t *fin;
ipstate_t *is;
{
	u_32_t sumd, old, new;
	tcphdr_t *tcp;
	int i;

	i = fin->fin_rev;
	tcp = fin->fin_dp;

	if (((i == 0) && !(is->is_flags & IS_ISNSYN)) ||
	    ((i == 1) && !(is->is_flags & IS_ISNACK))) {
		old = ntohl(tcp->th_seq);
		new = fr_newisn(fin);
		is->is_isninc[i] = new - old;
		CALC_SUMD(old, new, sumd);
		is->is_sumd[i] = (sumd & 0xffff) + (sumd >> 16);

		is->is_flags |= ((i == 0) ? IS_ISNSYN : IS_ISNACK);
	}
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_tcpinwindow                                              */
/* Returns:     int - 1 == packet inside TCP "window", 0 == not inside,     */
/*                    2 == packet seq number matches next expected          */
/* Parameters:  fin(I)   - pointer to packet information                    */
/*              fdata(I) - pointer to tcp state informatio (forward)        */
/*              tdata(I) - pointer to tcp state informatio (reverse)        */
/*              tcp(I)   - pointer to TCP packet header                     */
/*                                                                          */
/* Given a packet has matched addresses and ports, check to see if it is    */
/* within the TCP data window.  In a show of generosity, allow packets that */
/* are within the window space behind the current sequence # as well.       */
/* ------------------------------------------------------------------------ */
int fr_tcpinwindow(fin, fdata, tdata, tcp, flags)
fr_info_t *fin;
tcpdata_t  *fdata, *tdata;
tcphdr_t *tcp;
int flags;
{
	tcp_seq seq, ack, end;
	int ackskew, tcpflags;
	u_32_t win, maxwin;
	int dsize, inseq;

	/*
	 * Find difference between last checked packet and this packet.
	 */
	tcpflags = tcp->th_flags;
	seq = ntohl(tcp->th_seq);
	ack = ntohl(tcp->th_ack);
	if (tcpflags & TH_SYN)
		win = ntohs(tcp->th_win);
	else
		win = ntohs(tcp->th_win) << fdata->td_winscale;

	/*
	 * A window of 0 produces undesirable behaviour from this function.
	 */
	if (win == 0)
		win = 1;

	dsize = fin->fin_dlen - (TCP_OFF(tcp) << 2) +
	        ((tcpflags & TH_SYN) ? 1 : 0) + ((tcpflags & TH_FIN) ? 1 : 0);

	/*
	 * if window scaling is present, the scaling is only allowed
	 * for windows not in the first SYN packet. In that packet the
	 * window is 65535 to specify the largest window possible
	 * for receivers not implementing the window scale option.
	 * Currently, we do not assume TTCP here. That means that
	 * if we see a second packet from a host (after the initial
	 * SYN), we can assume that the receiver of the SYN did
	 * already send back the SYN/ACK (and thus that we know if
	 * the receiver also does window scaling)
	 */
	if (!(tcpflags & TH_SYN) && (fdata->td_winflags & TCP_WSCALE_FIRST)) {
		fdata->td_winflags &= ~TCP_WSCALE_FIRST;
		fdata->td_maxwin = win;
	}

	end = seq + dsize;

	if ((fdata->td_end == 0) &&
	    (!(flags & IS_TCPFSM) ||
	     ((tcpflags & TH_OPENING) == TH_OPENING))) {
		/*
		 * Must be a (outgoing) SYN-ACK in reply to a SYN.
		 */
		fdata->td_end = end - 1;
		fdata->td_maxwin = 1;
		fdata->td_maxend = end + win;
	}

	if (!(tcpflags & TH_ACK)) {  /* Pretend an ack was sent */
		ack = tdata->td_end;
	} else if (((tcpflags & (TH_ACK|TH_RST)) == (TH_ACK|TH_RST)) &&
		   (ack == 0)) {
		/* gross hack to get around certain broken tcp stacks */
		ack = tdata->td_end;
	}

	maxwin = tdata->td_maxwin;
	ackskew = tdata->td_end - ack;

	/*
	 * Strict sequencing only allows in-order delivery.
	 */
	if (seq != fdata->td_end) {
		if ((flags & IS_STRICT) != 0) {
			return 0;
		}
	}

	inseq = 0;
	if ((SEQ_GE(fdata->td_maxend, end)) &&
	    (SEQ_GE(seq, fdata->td_end - maxwin)) &&
/* XXX what about big packets */
#define MAXACKWINDOW 66000
	    (-ackskew <= (MAXACKWINDOW)) &&
	    ( ackskew <= (MAXACKWINDOW << fdata->td_winscale))) {
		inseq = 1;
	/*
	 * Microsoft Windows will send the next packet to the right of the
	 * window if SACK is in use.
	 */
	} else if ((seq == fdata->td_maxend) && (ackskew == 0) &&
	    (fdata->td_winflags & TCP_SACK_PERMIT) &&
	    (tdata->td_winflags & TCP_SACK_PERMIT)) {
		inseq = 1;
	/*
	 * Sometimes a TCP RST will be generated with only the ACK field
	 * set to non-zero.
	 */
	} else if ((seq == 0) && (tcpflags == (TH_RST|TH_ACK)) &&
		   (ackskew >= -1) && (ackskew <= 1)) {
		inseq = 1;
	} else if (!(flags & IS_TCPFSM)) {
		int i;

		i = (fin->fin_rev << 1) + fin->fin_out;

#if 0
		if (is_pkts[i]0 == 0) {
			/*
			 * Picking up a connection in the middle, the "next"
			 * packet seen from a direction that is new should be
			 * accepted, even if it appears out of sequence.
			 */
			inseq = 1;
		} else 
#endif
		if (!(fdata->td_winflags &
			    (TCP_WSCALE_SEEN|TCP_WSCALE_FIRST))) {
			/*
			 * No TCPFSM and no window scaling, so make some
			 * extra guesses.
			 */
			if ((seq == fdata->td_maxend) && (ackskew == 0))
				inseq = 1;
			else if (SEQ_GE(seq + maxwin, fdata->td_end - maxwin))
				inseq = 1;
		}
	}

	/* TRACE(inseq, fdata, tdata, seq, end, ack, ackskew, win, maxwin) */

	if (inseq) {
		/* if ackskew < 0 then this should be due to fragmented
		 * packets. There is no way to know the length of the
		 * total packet in advance.
		 * We do know the total length from the fragment cache though.
		 * Note however that there might be more sessions with
		 * exactly the same source and destination parameters in the
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
		if (SEQ_GE(ack + win, tdata->td_maxend))
			tdata->td_maxend = ack + win;
		return 1;
	}
	return 0;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_stclone                                                  */
/* Returns:     ipstate_t* - NULL == cloning failed,                        */
/*                           else pointer to new state structure            */
/* Parameters:  fin(I) - pointer to packet information                      */
/*              tcp(I) - pointer to TCP/UDP header                          */
/*              is(I)  - pointer to master state structure                  */
/*                                                                          */
/* Create a "duplcate" state table entry from the master.                   */
/* ------------------------------------------------------------------------ */
static ipstate_t *fr_stclone(fin, tcp, is)
fr_info_t *fin;
tcphdr_t *tcp;
ipstate_t *is;
{
	ipstate_t *clone;
	u_32_t send;

	if (ips_num == fr_statemax) {
		ATOMIC_INCL(ips_stats.iss_max);
		fr_state_doflush = 1;
		return NULL;
	}
	KMALLOC(clone, ipstate_t *);
	if (clone == NULL)
		return NULL;
	bcopy((char *)is, (char *)clone, sizeof(*clone));

	MUTEX_NUKE(&clone->is_lock);

	clone->is_die = ONE_DAY + fr_ticks;
	clone->is_state[0] = 0;
	clone->is_state[1] = 0;
	send = ntohl(tcp->th_seq) + fin->fin_dlen - (TCP_OFF(tcp) << 2) +
		((tcp->th_flags & TH_SYN) ? 1 : 0) +
		((tcp->th_flags & TH_FIN) ? 1 : 0);

	if (fin->fin_rev == 1) {
		clone->is_dend = send;
		clone->is_maxdend = send;
		clone->is_send = 0;
		clone->is_maxswin = 1;
		clone->is_maxdwin = ntohs(tcp->th_win);
		if (clone->is_maxdwin == 0)
			clone->is_maxdwin = 1;
	} else {
		clone->is_send = send;
		clone->is_maxsend = send;
		clone->is_dend = 0;
		clone->is_maxdwin = 1;
		clone->is_maxswin = ntohs(tcp->th_win);
		if (clone->is_maxswin == 0)
			clone->is_maxswin = 1;
	}

	clone->is_flags &= ~SI_CLONE;
	clone->is_flags |= SI_CLONED;
	fr_stinsert(clone, fin->fin_rev);
	clone->is_ref = 2;
	if (clone->is_p == IPPROTO_TCP) {
		(void) fr_tcp_age(&clone->is_sti, fin, ips_tqtqb,
				  clone->is_flags);
	}
	MUTEX_EXIT(&clone->is_lock);
#ifdef	IPFILTER_SCAN
	(void) ipsc_attachis(is);
#endif
#ifdef	IPFILTER_SYNC
	if (is->is_flags & IS_STATESYNC)
		clone->is_sync = ipfsync_new(SMC_STATE, fin, clone);
#endif
	return clone;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_matchsrcdst                                              */
/* Returns:     Nil                                                         */
/* Parameters:  fin(I) - pointer to packet information                      */
/*              is(I)  - pointer to state structure                         */
/*              src(I) - pointer to source address                          */
/*              dst(I) - pointer to destination address                     */
/*              tcp(I) - pointer to TCP/UDP header                          */
/*                                                                          */
/* Match a state table entry against an IP packet.  The logic below is that */
/* ret gets set to one if the match succeeds, else remains 0.  If it is     */
/* still 0 after the test. no match.                                        */
/* ------------------------------------------------------------------------ */
static ipstate_t *fr_matchsrcdst(fin, is, src, dst, tcp, cmask)
fr_info_t *fin;
ipstate_t *is;
i6addr_t *src, *dst;
tcphdr_t *tcp;
u_32_t cmask;
{
	int ret = 0, rev, out, flags, flx = 0, idx;
	u_short sp, dp;
	u_32_t cflx;
	void *ifp;

	rev = IP6_NEQ(&is->is_dst, dst);
	ifp = fin->fin_ifp;
	out = fin->fin_out;
	flags = is->is_flags;
	sp = 0;
	dp = 0;

	if (tcp != NULL) {
		sp = htons(fin->fin_sport);
		dp = ntohs(fin->fin_dport);
	}
	if (!rev) {
		if (tcp != NULL) {
			if (!(flags & SI_W_SPORT) && (sp != is->is_sport))
				rev = 1;
			else if (!(flags & SI_W_DPORT) && (dp != is->is_dport))
				rev = 1;
		}
	}

	idx = (out << 1) + rev;

	/*
	 * If the interface for this 'direction' is set, make sure it matches.
	 * An interface name that is not set matches any, as does a name of *.
	 */
	if ((is->is_ifp[idx] == ifp) || (is->is_ifp[idx] == NULL &&
	    (*is->is_ifname[idx] == '\0' || *is->is_ifname[idx] == '-' ||
	     *is->is_ifname[idx] == '*')))
		ret = 1;

	if (ret == 0)
		return NULL;
	ret = 0;

	/*
	 * Match addresses and ports.
	 */
	if (rev == 0) {
		if ((IP6_EQ(&is->is_dst, dst) || (flags & SI_W_DADDR)) &&
		    (IP6_EQ(&is->is_src, src) || (flags & SI_W_SADDR))) {
			if (tcp) {
				if ((sp == is->is_sport || flags & SI_W_SPORT)&&
				    (dp == is->is_dport || flags & SI_W_DPORT))
					ret = 1;
			} else {
				ret = 1;
			}
		}
	} else {
		if ((IP6_EQ(&is->is_dst, src) || (flags & SI_W_DADDR)) &&
		    (IP6_EQ(&is->is_src, dst) || (flags & SI_W_SADDR))) {
			if (tcp) {
				if ((dp == is->is_sport || flags & SI_W_SPORT)&&
				    (sp == is->is_dport || flags & SI_W_DPORT))
					ret = 1;
			} else {
				ret = 1;
			}
		}
	}

	if (ret == 0)
		return NULL;

	/*
	 * Whether or not this should be here, is questionable, but the aim
	 * is to get this out of the main line.
	 */
	if (tcp == NULL)
		flags = is->is_flags & ~(SI_WILDP|SI_NEWFR|SI_CLONE|SI_CLONED);

	/*
	 * Only one of the source or destination address can be flaged as a
	 * wildcard.  Fill in the missing address, if set.
	 * For IPv6, if the address being copied in is multicast, then
	 * don't reset the wild flag - multicast causes it to be set in the
	 * first place!
	 */
	if ((flags & (SI_W_SADDR|SI_W_DADDR))) {
		fr_ip_t *fi = &fin->fin_fi;

		if ((flags & SI_W_SADDR) != 0) {
			if (rev == 0) {
#ifdef USE_INET6
				if (is->is_v == 6 &&
				    IN6_IS_ADDR_MULTICAST(&fi->fi_src.in6))
					/*EMPTY*/;
				else
#endif
				{
					is->is_src = fi->fi_src;
					is->is_flags &= ~SI_W_SADDR;
				}
			} else {
#ifdef USE_INET6
				if (is->is_v == 6 &&
				    IN6_IS_ADDR_MULTICAST(&fi->fi_dst.in6))
					/*EMPTY*/;
				else
#endif
				{
					is->is_src = fi->fi_dst;
					is->is_flags &= ~SI_W_SADDR;
				}
			}
		} else if ((flags & SI_W_DADDR) != 0) {
			if (rev == 0) {
#ifdef USE_INET6
				if (is->is_v == 6 &&
				    IN6_IS_ADDR_MULTICAST(&fi->fi_dst.in6))
					/*EMPTY*/;
				else
#endif
				{
					is->is_dst = fi->fi_dst;
					is->is_flags &= ~SI_W_DADDR;
				}
			} else {
#ifdef USE_INET6
				if (is->is_v == 6 &&
				    IN6_IS_ADDR_MULTICAST(&fi->fi_src.in6))
					/*EMPTY*/;
				else
#endif
				{
					is->is_dst = fi->fi_src;
					is->is_flags &= ~SI_W_DADDR;
				}
			}
		}
		if ((is->is_flags & (SI_WILDA|SI_WILDP)) == 0) {
			ATOMIC_DECL(ips_stats.iss_wild);
		}
	}

	flx = fin->fin_flx & cmask;
	cflx = is->is_flx[out][rev];

	/*
	 * Match up any flags set from IP options.
	 */
	if ((cflx && (flx != (cflx & cmask))) ||
	    ((fin->fin_optmsk & is->is_optmsk[rev]) != is->is_opt[rev]) ||
	    ((fin->fin_secmsk & is->is_secmsk) != is->is_sec) ||
	    ((fin->fin_auth & is->is_authmsk) != is->is_auth))
		return NULL;

	/*
	 * Only one of the source or destination port can be flagged as a
	 * wildcard.  When filling it in, fill in a copy of the matched entry
	 * if it has the cloning flag set.
	 */
	if ((fin->fin_flx & FI_IGNORE) != 0) {
		fin->fin_rev = rev;
		return is;
	}

	if ((flags & (SI_W_SPORT|SI_W_DPORT))) {
		if ((flags & SI_CLONE) != 0) {
			ipstate_t *clone;

			clone = fr_stclone(fin, tcp, is);
			if (clone == NULL)
				return NULL;
			is = clone;
		} else {
			ATOMIC_DECL(ips_stats.iss_wild);
		}

		if ((flags & SI_W_SPORT) != 0) {
			if (rev == 0) {
				is->is_sport = sp;
				is->is_send = ntohl(tcp->th_seq);
			} else {
				is->is_sport = dp;
				is->is_send = ntohl(tcp->th_ack);
			}
			is->is_maxsend = is->is_send + 1;
		} else if ((flags & SI_W_DPORT) != 0) {
			if (rev == 0) {
				is->is_dport = dp;
				is->is_dend = ntohl(tcp->th_ack);
			} else {
				is->is_dport = sp;
				is->is_dend = ntohl(tcp->th_seq);
			}
			is->is_maxdend = is->is_dend + 1;
		}
		is->is_flags &= ~(SI_W_SPORT|SI_W_DPORT);
		if ((flags & SI_CLONED) && ipstate_logging)
			ipstate_log(is, ISL_CLONE);
	}

	ret = -1;

	if (is->is_flx[out][rev] == 0) {
		is->is_flx[out][rev] = flx;
		is->is_opt[rev] = fin->fin_optmsk;
		if (is->is_v == 6) {
			is->is_opt[rev] &= ~0x8;
			is->is_optmsk[rev] &= ~0x8;
		}
	}

	/*
	 * Check if the interface name for this "direction" is set and if not,
	 * fill it in.
	 */
	if (is->is_ifp[idx] == NULL &&
	    (*is->is_ifname[idx] == '\0' || *is->is_ifname[idx] == '*')) {
		is->is_ifp[idx] = ifp;
		COPYIFNAME(is->is_v, ifp, is->is_ifname[idx]);
	}
	fin->fin_rev = rev;
	return is;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_checkicmpmatchingstate                                   */
/* Returns:     Nil                                                         */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* If we've got an ICMP error message, using the information stored in the  */
/* ICMP packet, look for a matching state table entry.                      */
/*                                                                          */
/* If we return NULL then no lock on ipf_state is held.                     */
/* If we return non-null then a read-lock on ipf_state is held.             */
/* ------------------------------------------------------------------------ */
static ipstate_t *fr_checkicmpmatchingstate(fin)
fr_info_t *fin;
{
	ipstate_t *is, **isp;
	u_short sport, dport;
	u_char	pr;
	int backward, i, oi;
	i6addr_t dst, src;
	struct icmp *ic;
	u_short savelen;
	icmphdr_t *icmp;
	fr_info_t ofin;
	tcphdr_t *tcp;
	int type, len;
	ip_t *oip;
	u_int hv;

	/*
	 * Does it at least have the return (basic) IP header ?
	 * Is it an actual recognised ICMP error type?
	 * Only a basic IP header (no options) should be with
	 * an ICMP error header.
	 */
	if ((fin->fin_v != 4) || (fin->fin_hlen != sizeof(ip_t)) ||
	    (fin->fin_plen < ICMPERR_MINPKTLEN) ||
	    !(fin->fin_flx & FI_ICMPERR))
		return NULL;
	ic = fin->fin_dp;
	type = ic->icmp_type;

	oip = (ip_t *)((char *)ic + ICMPERR_ICMPHLEN);
	/*
	 * Check if the at least the old IP header (with options) and
	 * 8 bytes of payload is present.
	 */
	if (fin->fin_plen < ICMPERR_MAXPKTLEN + ((IP_HL(oip) - 5) << 2))
		return NULL;

	/*
	 * Sanity Checks.
	 */
	len = fin->fin_dlen - ICMPERR_ICMPHLEN;
	if ((len <= 0) || ((IP_HL(oip) << 2) > len))
		return NULL;

	/*
	 * Is the buffer big enough for all of it ?  It's the size of the IP
	 * header claimed in the encapsulated part which is of concern.  It
	 * may be too big to be in this buffer but not so big that it's
	 * outside the ICMP packet, leading to TCP deref's causing problems.
	 * This is possible because we don't know how big oip_hl is when we
	 * do the pullup early in fr_check() and thus can't guarantee it is
	 * all here now.
	 */
#ifdef  _KERNEL
	{
	mb_t *m;

	m = fin->fin_m;
# if defined(MENTAT)
	if ((char *)oip + len > (char *)m->b_wptr)
		return NULL;
# else
	if ((char *)oip + len > (char *)fin->fin_ip + m->m_len)
		return NULL;
# endif
	}
#endif
	bcopy((char *)fin, (char *)&ofin, sizeof(*fin));

	/*
	 * in the IPv4 case we must zero the i6addr union otherwise
	 * the IP6_EQ and IP6_NEQ macros produce the wrong results because
	 * of the 'junk' in the unused part of the union
	 */
	bzero((char *)&src, sizeof(src));
	bzero((char *)&dst, sizeof(dst));

	/*
	 * we make an fin entry to be able to feed it to
	 * matchsrcdst note that not all fields are encessary
	 * but this is the cleanest way. Note further we fill
	 * in fin_mp such that if someone uses it we'll get
	 * a kernel panic. fr_matchsrcdst does not use this.
	 *
	 * watch out here, as ip is in host order and oip in network
	 * order. Any change we make must be undone afterwards, like
	 * oip->ip_off - it is still in network byte order so fix it.
	 */
	savelen = oip->ip_len;
	oip->ip_len = len;
	oip->ip_off = ntohs(oip->ip_off);

	ofin.fin_flx = FI_NOCKSUM;
	ofin.fin_v = 4;
	ofin.fin_ip = oip;
	ofin.fin_m = NULL;	/* if dereferenced, panic XXX */
	ofin.fin_mp = NULL;	/* if dereferenced, panic XXX */
	(void) fr_makefrip(IP_HL(oip) << 2, oip, &ofin);
	ofin.fin_ifp = fin->fin_ifp;
	ofin.fin_out = !fin->fin_out;
	/*
	 * Reset the short and bad flag here because in fr_matchsrcdst()
	 * the flags for the current packet (fin_flx) are compared against
	 * those for the existing session.
	 */
	ofin.fin_flx &= ~(FI_BAD|FI_SHORT);

	/*
	 * Put old values of ip_len and ip_off back as we don't know
	 * if we have to forward the packet (or process it again.
	 */
	oip->ip_len = savelen;
	oip->ip_off = htons(oip->ip_off);

	switch (oip->ip_p)
	{
	case IPPROTO_ICMP :
		/*
		 * an ICMP error can only be generated as a result of an
		 * ICMP query, not as the response on an ICMP error
		 *
		 * XXX theoretically ICMP_ECHOREP and the other reply's are
		 * ICMP query's as well, but adding them here seems strange XXX
		 */
		if ((ofin.fin_flx & FI_ICMPERR) != 0)
		    	return NULL;

		/*
		 * perform a lookup of the ICMP packet in the state table
		 */
		icmp = (icmphdr_t *)((char *)oip + (IP_HL(oip) << 2));
		hv = (pr = oip->ip_p);
		src.in4 = oip->ip_src;
		hv += src.in4.s_addr;
		dst.in4 = oip->ip_dst;
		hv += dst.in4.s_addr;
		hv += icmp->icmp_id;
		hv = DOUBLE_HASH(hv);

		READ_ENTER(&ipf_state);
		for (isp = &ips_table[hv]; ((is = *isp) != NULL); ) {
			isp = &is->is_hnext;
			if ((is->is_p != pr) || (is->is_v != 4))
				continue;
			if (is->is_pass & FR_NOICMPERR)
				continue;
			is = fr_matchsrcdst(&ofin, is, &src, &dst,
					    NULL, FI_ICMPCMP);
			if (is != NULL) {
				/*
				 * i  : the index of this packet (the icmp
				 *      unreachable)
				 * oi : the index of the original packet found
				 *      in the icmp header (i.e. the packet
				 *      causing this icmp)
				 * backward : original packet was backward
				 *      compared to the state
				 */
				backward = IP6_NEQ(&is->is_src, &src);
				fin->fin_rev = !backward;
				i = (!backward << 1) + fin->fin_out;
				oi = (backward << 1) + ofin.fin_out;
				if (is->is_icmppkts[i] > is->is_pkts[oi])
					continue;
				ips_stats.iss_hits++;
				is->is_icmppkts[i]++;
				return is;
			}
		}
		RWLOCK_EXIT(&ipf_state);
		return NULL;
	case IPPROTO_TCP :
	case IPPROTO_UDP :
		break;
	default :
		return NULL;
	}

	tcp = (tcphdr_t *)((char *)oip + (IP_HL(oip) << 2));
	dport = tcp->th_dport;
	sport = tcp->th_sport;

	hv = (pr = oip->ip_p);
	src.in4 = oip->ip_src;
	hv += src.in4.s_addr;
	dst.in4 = oip->ip_dst;
	hv += dst.in4.s_addr;
	hv += dport;
	hv += sport;
	hv = DOUBLE_HASH(hv);

	READ_ENTER(&ipf_state);
	for (isp = &ips_table[hv]; ((is = *isp) != NULL); ) {
		isp = &is->is_hnext;
		/*
		 * Only allow this icmp though if the
		 * encapsulated packet was allowed through the
		 * other way around. Note that the minimal amount
		 * of info present does not allow for checking against
		 * tcp internals such as seq and ack numbers.   Only the
		 * ports are known to be present and can be even if the
		 * short flag is set.
		 */
		if ((is->is_p == pr) && (is->is_v == 4) &&
		    (is = fr_matchsrcdst(&ofin, is, &src, &dst,
					 tcp, FI_ICMPCMP))) {
			/*
			 * i  : the index of this packet (the icmp unreachable)
			 * oi : the index of the original packet found in the
			 *      icmp header (i.e. the packet causing this icmp)
			 * backward : original packet was backward compared to
			 *            the state
			 */
			backward = IP6_NEQ(&is->is_src, &src);
			fin->fin_rev = !backward;
			i = (!backward << 1) + fin->fin_out;
			oi = (backward << 1) + ofin.fin_out;

			if (((is->is_pass & FR_NOICMPERR) != 0) ||
			    (is->is_icmppkts[i] > is->is_pkts[oi]))
				break;
			ips_stats.iss_hits++;
			is->is_icmppkts[i]++;
			/*
			 * we deliberately do not touch the timeouts
			 * for the accompanying state table entry.
			 * It remains to be seen if that is correct. XXX
			 */
			return is;
		}
	}
	RWLOCK_EXIT(&ipf_state);
	return NULL;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_ipsmove                                                  */
/* Returns:     Nil                                                         */
/* Parameters:  is(I) - pointer to state table entry                        */
/*              hv(I) - new hash value for state table entry                */
/* Write Locks: ipf_state                                                   */
/*                                                                          */
/* Move a state entry from one position in the hash table to another.       */
/* ------------------------------------------------------------------------ */
static void fr_ipsmove(is, hv)
ipstate_t *is;
u_int hv;
{
	ipstate_t **isp;
	u_int hvm;

	hvm = is->is_hv;
	/*
	 * Remove the hash from the old location...
	 */
	isp = is->is_phnext;
	if (is->is_hnext)
		is->is_hnext->is_phnext = isp;
	*isp = is->is_hnext;
	if (ips_table[hvm] == NULL)
		ips_stats.iss_inuse--;
	ips_stats.iss_bucketlen[hvm]--;

	/*
	 * ...and put the hash in the new one.
	 */
	hvm = DOUBLE_HASH(hv);
	is->is_hv = hvm;
	isp = &ips_table[hvm];
	if (*isp)
		(*isp)->is_phnext = &is->is_hnext;
	else
		ips_stats.iss_inuse++;
	ips_stats.iss_bucketlen[hvm]++;
	is->is_phnext = isp;
	is->is_hnext = *isp;
	*isp = is;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_stlookup                                                 */
/* Returns:     ipstate_t* - NULL == no matching state found,               */
/*                           else pointer to state information is returned  */
/* Parameters:  fin(I) - pointer to packet information                      */
/*              tcp(I) - pointer to TCP/UDP header.                         */
/*                                                                          */
/* Search the state table for a matching entry to the packet described by   */
/* the contents of *fin.                                                    */
/*                                                                          */
/* If we return NULL then no lock on ipf_state is held.                     */
/* If we return non-null then a read-lock on ipf_state is held.             */
/* ------------------------------------------------------------------------ */
ipstate_t *fr_stlookup(fin, tcp, ifqp)
fr_info_t *fin;
tcphdr_t *tcp;
ipftq_t **ifqp;
{
	u_int hv, hvm, pr, v, tryagain;
	ipstate_t *is, **isp;
	u_short dport, sport;
	i6addr_t src, dst;
	struct icmp *ic;
	ipftq_t *ifq;
	int oow;

	is = NULL;
	ifq = NULL;
	tcp = fin->fin_dp;
	ic = (struct icmp *)tcp;
	hv = (pr = fin->fin_fi.fi_p);
	src = fin->fin_fi.fi_src;
	dst = fin->fin_fi.fi_dst;
	hv += src.in4.s_addr;
	hv += dst.in4.s_addr;

	v = fin->fin_fi.fi_v;
#ifdef	USE_INET6
	if (v == 6) {
		hv  += fin->fin_fi.fi_src.i6[1];
		hv  += fin->fin_fi.fi_src.i6[2];
		hv  += fin->fin_fi.fi_src.i6[3];

		if ((fin->fin_p == IPPROTO_ICMPV6) &&
		    IN6_IS_ADDR_MULTICAST(&fin->fin_fi.fi_dst.in6)) {
			hv -= dst.in4.s_addr;
		} else {
			hv += fin->fin_fi.fi_dst.i6[1];
			hv += fin->fin_fi.fi_dst.i6[2];
			hv += fin->fin_fi.fi_dst.i6[3];
		}
	}
#endif
	if ((v == 4) &&
	    (fin->fin_flx & (FI_MULTICAST|FI_BROADCAST|FI_MBCAST))) {
		if (fin->fin_out == 0) {
			hv -= src.in4.s_addr;
		} else {
			hv -= dst.in4.s_addr;
		}
	}

	/*
	 * Search the hash table for matching packet header info.
	 */
	switch (pr)
	{
#ifdef	USE_INET6
	case IPPROTO_ICMPV6 :
		tryagain = 0;
		if (v == 6) {
			if ((ic->icmp_type == ICMP6_ECHO_REQUEST) ||
			    (ic->icmp_type == ICMP6_ECHO_REPLY)) {
				hv += ic->icmp_id;
			}
		}
		READ_ENTER(&ipf_state);
icmp6again:
		hvm = DOUBLE_HASH(hv);
		for (isp = &ips_table[hvm]; ((is = *isp) != NULL); ) {
			isp = &is->is_hnext;
			/*
			 * If a connection is about to be deleted, no packets
			 * are allowed to match it.
			 */
			if (is->is_sti.tqe_ifq == &ips_deletetq)
				continue;

			if ((is->is_p != pr) || (is->is_v != v))
				continue;
			is = fr_matchsrcdst(fin, is, &src, &dst, NULL, FI_CMP);
			if (is != NULL &&
			    fr_matchicmpqueryreply(v, &is->is_icmp,
						   ic, fin->fin_rev)) {
				if (fin->fin_rev)
					ifq = &ips_icmpacktq;
				else
					ifq = &ips_icmptq;
				break;
			}
		}

		if (is != NULL) {
			if ((tryagain != 0) && !(is->is_flags & SI_W_DADDR)) {
				hv += fin->fin_fi.fi_src.i6[0];
				hv += fin->fin_fi.fi_src.i6[1];
				hv += fin->fin_fi.fi_src.i6[2];
				hv += fin->fin_fi.fi_src.i6[3];
				fr_ipsmove(is, hv);
				MUTEX_DOWNGRADE(&ipf_state);
			}
			break;
		}
		RWLOCK_EXIT(&ipf_state);

		/*
		 * No matching icmp state entry. Perhaps this is a
		 * response to another state entry.
		 *
		 * XXX With some ICMP6 packets, the "other" address is already
		 * in the packet, after the ICMP6 header, and this could be
		 * used in place of the multicast address.  However, taking
		 * advantage of this requires some significant code changes
		 * to handle the specific types where that is the case.
		 */
		if ((ips_stats.iss_wild != 0) && (v == 6) && (tryagain == 0) &&
		    !IN6_IS_ADDR_MULTICAST(&fin->fin_fi.fi_src.in6)) {
			hv -= fin->fin_fi.fi_src.i6[0];
			hv -= fin->fin_fi.fi_src.i6[1];
			hv -= fin->fin_fi.fi_src.i6[2];
			hv -= fin->fin_fi.fi_src.i6[3];
			tryagain = 1;
			WRITE_ENTER(&ipf_state);
			goto icmp6again;
		}

		is = fr_checkicmp6matchingstate(fin);
		if (is != NULL)
			return is;
		break;
#endif

	case IPPROTO_ICMP :
		if (v == 4) {
			hv += ic->icmp_id;
		}
		hv = DOUBLE_HASH(hv);
		READ_ENTER(&ipf_state);
		for (isp = &ips_table[hv]; ((is = *isp) != NULL); ) {
			isp = &is->is_hnext;
			if ((is->is_p != pr) || (is->is_v != v))
				continue;
			is = fr_matchsrcdst(fin, is, &src, &dst, NULL, FI_CMP);
			if ((is != NULL) &&
			    (ic->icmp_id == is->is_icmp.ici_id) &&
			    fr_matchicmpqueryreply(v, &is->is_icmp,
						   ic, fin->fin_rev)) {
				if (fin->fin_rev)
					ifq = &ips_icmpacktq;
				else
					ifq = &ips_icmptq;
				break;
			}
		}
		if (is == NULL) {
			RWLOCK_EXIT(&ipf_state);
		}
		break;

	case IPPROTO_TCP :
	case IPPROTO_UDP :
		ifqp = NULL;
		sport = htons(fin->fin_data[0]);
		hv += sport;
		dport = htons(fin->fin_data[1]);
		hv += dport;
		oow = 0;
		tryagain = 0;
		READ_ENTER(&ipf_state);
retry_tcpudp:
		hvm = DOUBLE_HASH(hv);
		for (isp = &ips_table[hvm]; ((is = *isp) != NULL); ) {
			isp = &is->is_hnext;
			if ((is->is_p != pr) || (is->is_v != v))
				continue;
			fin->fin_flx &= ~FI_OOW;
			is = fr_matchsrcdst(fin, is, &src, &dst, tcp, FI_CMP);
			if (is != NULL) {
				if (pr == IPPROTO_TCP) {
					if (!fr_tcpstate(fin, tcp, is)) {
						oow |= fin->fin_flx & FI_OOW;
						continue;
					}
				}
				break;
			}
		}
		if (is != NULL) {
			if (tryagain &&
			    !(is->is_flags & (SI_CLONE|SI_WILDP|SI_WILDA))) {
				hv += dport;
				hv += sport;
				fr_ipsmove(is, hv);
				MUTEX_DOWNGRADE(&ipf_state);
			}
			break;
		}
		RWLOCK_EXIT(&ipf_state);

		if (ips_stats.iss_wild) {
			if (tryagain == 0) {
				hv -= dport;
				hv -= sport;
			} else if (tryagain == 1) {
				hv = fin->fin_fi.fi_p;
				/*
				 * If we try to pretend this is a reply to a
				 * multicast/broadcast packet then we need to
				 * exclude part of the address from the hash
				 * calculation.
				 */
				if (fin->fin_out == 0) {
					hv += src.in4.s_addr;
				} else {
					hv += dst.in4.s_addr;
				}
				hv += dport;
				hv += sport;
			}
			tryagain++;
			if (tryagain <= 2) {
				WRITE_ENTER(&ipf_state);
				goto retry_tcpudp;
			}
		}
		fin->fin_flx |= oow;
		break;

#if 0
	case IPPROTO_GRE :
		gre = fin->fin_dp;
		if (GRE_REV(gre->gr_flags) == 1) {
			hv += gre->gr_call;
		}
		/* FALLTHROUGH */
#endif
	default :
		ifqp = NULL;
		hvm = DOUBLE_HASH(hv);
		READ_ENTER(&ipf_state);
		for (isp = &ips_table[hvm]; ((is = *isp) != NULL); ) {
			isp = &is->is_hnext;
			if ((is->is_p != pr) || (is->is_v != v))
				continue;
			is = fr_matchsrcdst(fin, is, &src, &dst, NULL, FI_CMP);
			if (is != NULL) {
				ifq = &ips_iptq;
				break;
			}
		}
		if (is == NULL) {
			RWLOCK_EXIT(&ipf_state);
		}
		break;
	}

	if (is != NULL) {
		if (((is->is_sti.tqe_flags & TQE_RULEBASED) != 0) &&
		    (is->is_tqehead[fin->fin_rev] != NULL))
			ifq = is->is_tqehead[fin->fin_rev];
		if (ifq != NULL && ifqp != NULL)
			*ifqp = ifq;
	}
	return is;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_updatestate                                              */
/* Returns:     Nil                                                         */
/* Parameters:  fin(I) - pointer to packet information                      */
/*              is(I)  - pointer to state table entry                       */
/* Read Locks:  ipf_state                                                   */
/*                                                                          */
/* Updates packet and byte counters for a newly received packet.  Seeds the */
/* fragment cache with a new entry as required.                             */
/* ------------------------------------------------------------------------ */
void fr_updatestate(fin, is, ifq)
fr_info_t *fin;
ipstate_t *is;
ipftq_t *ifq;
{
	ipftqent_t *tqe;
	int i, pass;

	i = (fin->fin_rev << 1) + fin->fin_out;

	/*
	 * For TCP packets, ifq == NULL.  For all others, check if this new
	 * queue is different to the last one it was on and move it if so.
	 */
	tqe = &is->is_sti;
	MUTEX_ENTER(&is->is_lock);
	if ((tqe->tqe_flags & TQE_RULEBASED) != 0)
		ifq = is->is_tqehead[fin->fin_rev];

	if (ifq != NULL)
		fr_movequeue(tqe, tqe->tqe_ifq, ifq);

	is->is_pkts[i]++;
	is->is_bytes[i] += fin->fin_plen;
	MUTEX_EXIT(&is->is_lock);

#ifdef	IPFILTER_SYNC
	if (is->is_flags & IS_STATESYNC)
		ipfsync_update(SMC_STATE, fin, is->is_sync);
#endif

	ATOMIC_INCL(ips_stats.iss_hits);

	fin->fin_fr = is->is_rule;

	/*
	 * If this packet is a fragment and the rule says to track fragments,
	 * then create a new fragment cache entry.
	 */
	pass = is->is_pass;
	if ((fin->fin_flx & FI_FRAG) && FR_ISPASS(pass))
		(void) fr_newfrag(fin, pass ^ FR_KEEPSTATE);
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_checkstate                                               */
/* Returns:     frentry_t* - NULL == search failed,                         */
/*                           else pointer to rule for matching state        */
/* Parameters:  ifp(I)   - pointer to interface                             */
/*              passp(I) - pointer to filtering result flags                */
/*                                                                          */
/* Check if a packet is associated with an entry in the state table.        */
/* ------------------------------------------------------------------------ */
frentry_t *fr_checkstate(fin, passp)
fr_info_t *fin;
u_32_t *passp;
{
	ipstate_t *is;
	frentry_t *fr;
	tcphdr_t *tcp;
	ipftq_t *ifq;
	u_int pass;

	if (fr_state_lock || (ips_list == NULL) ||
	    (fin->fin_flx & (FI_SHORT|FI_STATE|FI_FRAGBODY|FI_BAD)))
		return NULL;

	is = NULL;
	if ((fin->fin_flx & FI_TCPUDP) ||
	    (fin->fin_fi.fi_p == IPPROTO_ICMP)
#ifdef	USE_INET6
	    || (fin->fin_fi.fi_p == IPPROTO_ICMPV6)
#endif
	    )
		tcp = fin->fin_dp;
	else
		tcp = NULL;

	/*
	 * Search the hash table for matching packet header info.
	 */
	ifq = NULL;
	is = fin->fin_state;
	if (is == NULL)
		is = fr_stlookup(fin, tcp, &ifq);
	switch (fin->fin_p)
	{
#ifdef	USE_INET6
	case IPPROTO_ICMPV6 :
		if (is != NULL)
			break;
		if (fin->fin_v == 6) {
			is = fr_checkicmp6matchingstate(fin);
			if (is != NULL)
				goto matched;
		}
		break;
#endif
	case IPPROTO_ICMP :
		if (is != NULL)
			break;
		/*
		 * No matching icmp state entry. Perhaps this is a
		 * response to another state entry.
		 */
		is = fr_checkicmpmatchingstate(fin);
		if (is != NULL)
			goto matched;
		break;
	case IPPROTO_TCP :
		if (is == NULL)
			break;

		if (is->is_pass & FR_NEWISN) {
			if (fin->fin_out == 0)
				fr_fixinisn(fin, is);
			else if (fin->fin_out == 1)
				fr_fixoutisn(fin, is);
		}
		break;
	default :
		if (fin->fin_rev)
			ifq = &ips_udpacktq;
		else
			ifq = &ips_udptq;
		break;
	}
	if (is == NULL) {
		ATOMIC_INCL(ips_stats.iss_miss);
		return NULL;
	}

matched:
	fr = is->is_rule;
	if (fr != NULL) {
		if ((fin->fin_out == 0) && (fr->fr_nattag.ipt_num[0] != 0)) {
			if (fin->fin_nattag == NULL)
				return NULL;
			if (fr_matchtag(&fr->fr_nattag, fin->fin_nattag) != 0)
				return NULL;
		}
		(void) strncpy(fin->fin_group, fr->fr_group, FR_GROUPLEN);
		fin->fin_icode = fr->fr_icode;
	}

	fin->fin_rule = is->is_rulen;
	pass = is->is_pass;
	fr_updatestate(fin, is, ifq);

	fin->fin_state = is;
	is->is_touched = fr_ticks;
	MUTEX_ENTER(&is->is_lock);
	is->is_ref++;
	MUTEX_EXIT(&is->is_lock);
	RWLOCK_EXIT(&ipf_state);
	fin->fin_flx |= FI_STATE;
	if ((pass & FR_LOGFIRST) != 0)
		pass &= ~(FR_LOGFIRST|FR_LOG);
	*passp = pass;
	return fr;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_fixoutisn                                                */
/* Returns:     Nil                                                         */
/* Parameters:  fin(I)   - pointer to packet information                    */
/*              is(I)  - pointer to master state structure                  */
/*                                                                          */
/* Called only for outbound packets, adjusts the sequence number and the    */
/* TCP checksum to match that change.                                       */
/* ------------------------------------------------------------------------ */
static void fr_fixoutisn(fin, is)
fr_info_t *fin;
ipstate_t *is;
{
	tcphdr_t *tcp;
	int rev;
	u_32_t seq;

	tcp = fin->fin_dp;
	rev = fin->fin_rev;
	if ((is->is_flags & IS_ISNSYN) != 0) {
		if (rev == 0) {
			seq = ntohl(tcp->th_seq);
			seq += is->is_isninc[0];
			tcp->th_seq = htonl(seq);
			fix_outcksum(fin, &tcp->th_sum, is->is_sumd[0]);
		}
	}
	if ((is->is_flags & IS_ISNACK) != 0) {
		if (rev == 1) {
			seq = ntohl(tcp->th_seq);
			seq += is->is_isninc[1];
			tcp->th_seq = htonl(seq);
			fix_outcksum(fin, &tcp->th_sum, is->is_sumd[1]);
		}
	}
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_fixinisn                                                 */
/* Returns:     Nil                                                         */
/* Parameters:  fin(I)   - pointer to packet information                    */
/*              is(I)  - pointer to master state structure                  */
/*                                                                          */
/* Called only for inbound packets, adjusts the acknowledge number and the  */
/* TCP checksum to match that change.                                       */
/* ------------------------------------------------------------------------ */
static void fr_fixinisn(fin, is)
fr_info_t *fin;
ipstate_t *is;
{
	tcphdr_t *tcp;
	int rev;
	u_32_t ack;

	tcp = fin->fin_dp;
	rev = fin->fin_rev;
	if ((is->is_flags & IS_ISNSYN) != 0) {
		if (rev == 1) {
			ack = ntohl(tcp->th_ack);
			ack -= is->is_isninc[0];
			tcp->th_ack = htonl(ack);
			fix_incksum(fin, &tcp->th_sum, is->is_sumd[0]);
		}
	}
	if ((is->is_flags & IS_ISNACK) != 0) {
		if (rev == 0) {
			ack = ntohl(tcp->th_ack);
			ack -= is->is_isninc[1];
			tcp->th_ack = htonl(ack);
			fix_incksum(fin, &tcp->th_sum, is->is_sumd[1]);
		}
	}
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_statesync                                                */
/* Returns:     Nil                                                         */
/* Parameters:  ifp(I) - pointer to interface                               */
/*                                                                          */
/* Walk through all state entries and if an interface pointer match is      */
/* found then look it up again, based on its name in case the pointer has   */
/* changed since last time.                                                 */
/*                                                                          */
/* If ifp is passed in as being non-null then we are only doing updates for */
/* existing, matching, uses of it.                                          */
/* ------------------------------------------------------------------------ */
void fr_statesync(ifp)
void *ifp;
{
	ipstate_t *is;
	int i;

	if (fr_running <= 0)
		return;

	WRITE_ENTER(&ipf_state);

	if (fr_running <= 0) {
		RWLOCK_EXIT(&ipf_state);
		return;
	}

	for (is = ips_list; is; is = is->is_next) {
		/*
		 * Look up all the interface names in the state entry.
		 */
		for (i = 0; i < 4; i++) {
			if (ifp == NULL || ifp == is->is_ifp[i])
				is->is_ifp[i] = fr_resolvenic(is->is_ifname[i],
							      is->is_v);
		}
	}
	RWLOCK_EXIT(&ipf_state);
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_delstate                                                 */
/* Returns:     int - 0 = entry deleted, else reference count on struct     */
/* Parameters:  is(I)  - pointer to state structure to delete               */
/*              why(I) - if not 0, log reason why it was deleted            */
/* Write Locks: ipf_state                                                   */
/*                                                                          */
/* Deletes a state entry from the enumerated list as well as the hash table */
/* and timeout queue lists.  Make adjustments to hash table statistics and  */
/* global counters as required.                                             */
/* ------------------------------------------------------------------------ */
static int fr_delstate(is, why)
ipstate_t *is;
int why;
{

	/*
	 * Since we want to delete this, remove it from the state table,
	 * where it can be found & used, first.
	 */
	if (is->is_phnext != NULL) {
		*is->is_phnext = is->is_hnext;
		if (is->is_hnext != NULL)
			is->is_hnext->is_phnext = is->is_phnext;
		if (ips_table[is->is_hv] == NULL)
			ips_stats.iss_inuse--;
		ips_stats.iss_bucketlen[is->is_hv]--;

		is->is_phnext = NULL;
		is->is_hnext = NULL;
	}

	/*
	 * Because ips_stats.iss_wild is a count of entries in the state
	 * table that have wildcard flags set, only decerement it once
	 * and do it here.
	 */
	if (is->is_flags & (SI_WILDP|SI_WILDA)) {
		if (!(is->is_flags & SI_CLONED)) {
			ATOMIC_DECL(ips_stats.iss_wild);
		}
		is->is_flags &= ~(SI_WILDP|SI_WILDA);
	}

	/*
	 * Next, remove it from the timeout queue it is in.
	 */
	if (is->is_sti.tqe_ifq != NULL)
		fr_deletequeueentry(&is->is_sti);

	if (is->is_me != NULL) {
		*is->is_me = NULL;
		is->is_me = NULL;
	}

	/*
	 * If it is still in use by something else, do not go any further,
	 * but note that at this point it is now an orphan.  How can this
	 * be?  fr_state_flush() calls fr_delete() directly because it wants
	 * to empty the table out and if something has a hold on a state
	 * entry (such as ipfstat), it'll do the deref path that'll bring
	 * us back here to do the real delete & free.
	 */
	MUTEX_ENTER(&is->is_lock);
	if (is->is_ref > 1) {
		is->is_ref--;
		MUTEX_EXIT(&is->is_lock);
		return is->is_ref;
	}
	MUTEX_EXIT(&is->is_lock);

	is->is_ref = 0;

	if (is->is_tqehead[0] != NULL) {
		if (fr_deletetimeoutqueue(is->is_tqehead[0]) == 0)
			fr_freetimeoutqueue(is->is_tqehead[0]);
	}
	if (is->is_tqehead[1] != NULL) {
		if (fr_deletetimeoutqueue(is->is_tqehead[1]) == 0)
			fr_freetimeoutqueue(is->is_tqehead[1]);
	}

#ifdef	IPFILTER_SYNC
	if (is->is_sync)
		ipfsync_del(is->is_sync);
#endif
#ifdef	IPFILTER_SCAN
	(void) ipsc_detachis(is);
#endif

	/*
	 * Now remove it from the linked list of known states
	 */
	if (is->is_pnext != NULL) {
		*is->is_pnext = is->is_next;

		if (is->is_next != NULL)
			is->is_next->is_pnext = is->is_pnext;

		is->is_pnext = NULL;
		is->is_next = NULL;
	}

	if (ipstate_logging != 0 && why != 0)
		ipstate_log(is, why);

	if (is->is_p == IPPROTO_TCP)
		ips_stats.iss_fin++;
	else
		ips_stats.iss_expire++;

	if (is->is_rule != NULL) {
		is->is_rule->fr_statecnt--;
		(void) fr_derefrule(&is->is_rule);
	}

	MUTEX_DESTROY(&is->is_lock);
	KFREE(is);
	ips_num--;

	return 0;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_timeoutstate                                             */
/* Returns:     Nil                                                         */
/* Parameters:  Nil                                                         */
/*                                                                          */
/* Slowly expire held state for thingslike UDP and ICMP.  The algorithm     */
/* used here is to keep the queue sorted with the oldest things at the top  */
/* and the youngest at the bottom.  So if the top one doesn't need to be    */
/* expired then neither will any under it.                                  */
/* ------------------------------------------------------------------------ */
void fr_timeoutstate()
{
	ipftq_t *ifq, *ifqnext;
	ipftqent_t *tqe, *tqn;
	ipstate_t *is;
	SPL_INT(s);

	SPL_NET(s);
	WRITE_ENTER(&ipf_state);
	for (ifq = ips_tqtqb; ifq != NULL; ifq = ifq->ifq_next)
		for (tqn = ifq->ifq_head; ((tqe = tqn) != NULL); ) {
			if (tqe->tqe_die > fr_ticks)
				break;
			tqn = tqe->tqe_next;
			is = tqe->tqe_parent;
			fr_delstate(is, ISL_EXPIRE);
		}

	for (ifq = ips_utqe; ifq != NULL; ifq = ifqnext) {
		ifqnext = ifq->ifq_next;

		for (tqn = ifq->ifq_head; ((tqe = tqn) != NULL); ) {
			if (tqe->tqe_die > fr_ticks)
				break;
			tqn = tqe->tqe_next;
			is = tqe->tqe_parent;
			fr_delstate(is, ISL_EXPIRE);
		}
	}

	for (ifq = ips_utqe; ifq != NULL; ifq = ifqnext) {
		ifqnext = ifq->ifq_next;

		if (((ifq->ifq_flags & IFQF_DELETE) != 0) &&
		    (ifq->ifq_ref == 0)) {
			fr_freetimeoutqueue(ifq);
		}
	}

	if (fr_state_doflush) {
		(void) fr_state_flush(2, 0);
		fr_state_doflush = 0;
	}

	RWLOCK_EXIT(&ipf_state);
	SPL_X(s);
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_state_flush                                              */
/* Returns:     int - 0 == success, -1 == failure                           */
/* Parameters:  Nil                                                         */
/* Write Locks: ipf_state                                                   */
/*                                                                          */
/* Flush state tables.  Three actions currently defined:                    */
/* which == 0 : flush all state table entries                               */
/* which == 1 : flush TCP connections which have started to close but are   */
/*	      stuck for some reason.                                        */
/* which == 2 : flush TCP connections which have been idle for a long time, */
/*	      starting at > 4 days idle and working back in successive half-*/
/*	      days to at most 12 hours old.  If this fails to free enough   */
/*            slots then work backwards in half hour slots to 30 minutes.   */
/*            If that too fails, then work backwards in 30 second intervals */
/*            for the last 30 minutes to at worst 30 seconds idle.          */
/* ------------------------------------------------------------------------ */
static int fr_state_flush(which, proto)
int which, proto;
{
	ipftq_t *ifq, *ifqnext;
	ipftqent_t *tqe, *tqn;
	ipstate_t *is, **isp;
	int removed;
	SPL_INT(s);

	removed = 0;

	SPL_NET(s);

	switch (which)
	{
	case 0 :
		/*
		 * Style 0 flush removes everything...
		 */
		for (isp = &ips_list; ((is = *isp) != NULL); ) {
			if ((proto != 0) && (is->is_v != proto)) {
				isp = &is->is_next;
				continue;
			}
			if (fr_delstate(is, ISL_FLUSH) == 0)
				removed++;
			else
				isp = &is->is_next;
		}
		break;

	case 1 :
		/*
		 * Since we're only interested in things that are closing,
		 * we can start with the appropriate timeout queue.
		 */
		for (ifq = ips_tqtqb + IPF_TCPS_CLOSE_WAIT; ifq != NULL;
		     ifq = ifq->ifq_next) {

			for (tqn = ifq->ifq_head; ((tqe = tqn) != NULL); ) {
				tqn = tqe->tqe_next;
				is = tqe->tqe_parent;
				if (is->is_p != IPPROTO_TCP)
					break;
				if (fr_delstate(is, ISL_EXPIRE) == 0)
					removed++;
			}
		}

		/*
		 * Also need to look through the user defined queues.
		 */
		for (ifq = ips_utqe; ifq != NULL; ifq = ifqnext) {
			ifqnext = ifq->ifq_next;
			for (tqn = ifq->ifq_head; ((tqe = tqn) != NULL); ) {
				tqn = tqe->tqe_next;
				is = tqe->tqe_parent;
				if (is->is_p != IPPROTO_TCP)
					continue;

				if ((is->is_state[0] > IPF_TCPS_ESTABLISHED) &&
				    (is->is_state[1] > IPF_TCPS_ESTABLISHED)) {
					if (fr_delstate(is, ISL_EXPIRE) == 0)
						removed++;
				}
			}
		}
		break;

	case 2 :
		break;

		/*  
		 * Args 5-11 correspond to flushing those particular states
		 * for TCP connections.
		 */
	case IPF_TCPS_CLOSE_WAIT :
	case IPF_TCPS_FIN_WAIT_1 :
	case IPF_TCPS_CLOSING :
	case IPF_TCPS_LAST_ACK :
	case IPF_TCPS_FIN_WAIT_2 :
	case IPF_TCPS_TIME_WAIT :
	case IPF_TCPS_CLOSED :
		tqn = ips_tqtqb[which].ifq_head;
		while (tqn != NULL) {
			tqe = tqn;
			tqn = tqe->tqe_next;
			is = tqe->tqe_parent;
			if (fr_delstate(is, ISL_FLUSH) == 0)
				removed++;
		}
		break;

	default :
		if (which < 30)
			break;

		/* 
		 * Take a large arbitrary number to mean the number of seconds
		 * for which which consider to be the maximum value we'll allow
		 * the expiration to be.
		 */
		which = IPF_TTLVAL(which);
		for (isp = &ips_list; ((is = *isp) != NULL); ) {
			if ((proto == 0) || (is->is_v == proto)) {
				if (fr_ticks - is->is_touched > which) {
					if (fr_delstate(is, ISL_FLUSH) == 0) {
						removed++;
						continue;
					}
				}
			}
			isp = &is->is_next;
		}
		break;
	}

	if (which != 2) {
		SPL_X(s);
		return removed;
	}

	/*
	 * Asked to remove inactive entries because the table is full.
	 */
	if (fr_ticks - ips_last_force_flush > IPF_TTLVAL(5)) {
		ips_last_force_flush = fr_ticks;
                removed = ipf_queueflush(fr_state_flush_entry, ips_tqtqb,
					 ips_utqe);
	}

	SPL_X(s);
	return removed;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_state_flush_entry                                        */
/* Returns:     int - 0 = entry deleted, else not deleted                   */
/* Parameters:  entry(I)  - pointer to state structure to delete            */
/* Write Locks: ipf_state                                                   */
/*                                                                          */
/* This function is a stepping stone between ipf_queueflush() and           */
/* fr_delstate().  It is used so we can provide a uniform interface via the */
/* ipf_queueflush() function.                                               */
/* ------------------------------------------------------------------------ */
static int fr_state_flush_entry(entry)
void *entry;
{
	return fr_delstate(entry, ISL_FLUSH);
}     


/* ------------------------------------------------------------------------ */
/* Function:    fr_tcp_age                                                  */
/* Returns:     int - 1 == state transition made, 0 == no change (rejected) */
/* Parameters:  tq(I)    - pointer to timeout queue information             */
/*              fin(I)   - pointer to packet information                    */
/*              tqtab(I) - TCP timeout queue table this is in               */
/*              flags(I) - flags from state/NAT entry                       */
/*                                                                          */
/* Rewritten by Arjan de Vet <Arjan.deVet@adv.iae.nl>, 2000-07-29:          */
/*                                                                          */
/* - (try to) base state transitions on real evidence only,                 */
/*   i.e. packets that are sent and have been received by ipfilter;         */
/*   diagram 18.12 of TCP/IP volume 1 by W. Richard Stevens was used.       */
/*                                                                          */
/* - deal with half-closed connections correctly;                           */
/*                                                                          */
/* - store the state of the source in state[0] such that ipfstat            */
/*   displays the state as source/dest instead of dest/source; the calls    */
/*   to fr_tcp_age have been changed accordingly.                           */
/*                                                                          */
/* Internal Parameters:                                                     */
/*                                                                          */
/*    state[0] = state of source (host that initiated connection)           */
/*    state[1] = state of dest   (host that accepted the connection)        */
/*                                                                          */
/*    dir == 0 : a packet from source to dest                               */
/*    dir == 1 : a packet from dest to source                               */
/*                                                                          */
/* A typical procession for a connection is as follows:                     */
/*                                                                          */
/* +--------------+-------------------+                                     */
/* | Side '0'     | Side '1'          |                                     */
/* +--------------+-------------------+                                     */
/* | 0 -> 1 (SYN) |                   |                                     */
/* |              | 0 -> 2 (SYN-ACK)  |                                     */
/* | 1 -> 3 (ACK) |                   |                                     */
/* |              | 2 -> 4 (ACK-PUSH) |                                     */
/* | 3 -> 4 (ACK) |                   |                                     */
/* |   ...        |   ...             |                                     */
/* |              | 4 -> 6 (FIN-ACK)  |                                     */
/* | 4 -> 5 (ACK) |                   |                                     */
/* |              | 6 -> 6 (ACK-PUSH) |                                     */
/* | 5 -> 5 (ACK) |                   |                                     */
/* | 5 -> 8 (FIN) |                   |                                     */
/* |              | 6 -> 10 (ACK)     |                                     */
/* +--------------+-------------------+                                     */
/*                                                                          */
/* Locking: it is assumed that the parent of the tqe structure is locked.   */
/* ------------------------------------------------------------------------ */
int fr_tcp_age(tqe, fin, tqtab, flags)
ipftqent_t *tqe;
fr_info_t *fin;
ipftq_t *tqtab;
int flags;
{
	int dlen, ostate, nstate, rval, dir;
	u_char tcpflags;
	tcphdr_t *tcp;

	tcp = fin->fin_dp;

	rval = 0;
	dir = fin->fin_rev;
	tcpflags = tcp->th_flags;
	dlen = fin->fin_dlen - (TCP_OFF(tcp) << 2);

	if (tcpflags & TH_RST) {
		if (!(tcpflags & TH_PUSH) && !dlen)
			nstate = IPF_TCPS_CLOSED;
		else
			nstate = IPF_TCPS_CLOSE_WAIT;
		rval = 1;
	} else {
		ostate = tqe->tqe_state[1 - dir];
		nstate = tqe->tqe_state[dir];

		switch (nstate)
		{
		case IPF_TCPS_LISTEN: /* 0 */
			if ((tcpflags & TH_OPENING) == TH_OPENING) {
				/*
				 * 'dir' received an S and sends SA in
				 * response, LISTEN -> SYN_RECEIVED
				 */
				nstate = IPF_TCPS_SYN_RECEIVED;
				rval = 1;
			} else if ((tcpflags & TH_OPENING) == TH_SYN) {
				/* 'dir' sent S, LISTEN -> SYN_SENT */
				nstate = IPF_TCPS_SYN_SENT;
				rval = 1;
			}
			/*
			 * the next piece of code makes it possible to get
			 * already established connections into the state table
			 * after a restart or reload of the filter rules; this
			 * does not work when a strict 'flags S keep state' is
			 * used for tcp connections of course
			 */
			if (((flags & IS_TCPFSM) == 0) &&
			    ((tcpflags & TH_ACKMASK) == TH_ACK)) {
				/*
				 * we saw an A, guess 'dir' is in ESTABLISHED
				 * mode
				 */
				switch (ostate)
				{
				case IPF_TCPS_LISTEN :
				case IPF_TCPS_SYN_RECEIVED :
					nstate = IPF_TCPS_HALF_ESTAB;
					rval = 1;
					break;
				case IPF_TCPS_HALF_ESTAB :
				case IPF_TCPS_ESTABLISHED :
					nstate = IPF_TCPS_ESTABLISHED;
					rval = 1;
					break;
				default :
					break;
				}
			}
			/*
			 * TODO: besides regular ACK packets we can have other
			 * packets as well; it is yet to be determined how we
			 * should initialize the states in those cases
			 */
			break;

		case IPF_TCPS_SYN_SENT: /* 1 */
			if ((tcpflags & ~(TH_ECN|TH_CWR)) == TH_SYN) {
				/*
				 * A retransmitted SYN packet.  We do not reset
				 * the timeout here to fr_tcptimeout because a
				 * connection connect timeout does not renew
				 * after every packet that is sent.  We need to
				 * set rval so as to indicate the packet has
				 * passed the check for its flags being valid
				 * in the TCP FSM.  Setting rval to 2 has the
				 * result of not resetting the timeout.
				 */
				rval = 2;
			} else if ((tcpflags & (TH_SYN|TH_FIN|TH_ACK)) ==
				   TH_ACK) {
				/*
				 * we see an A from 'dir' which is in SYN_SENT
				 * state: 'dir' sent an A in response to an SA
				 * which it received, SYN_SENT -> ESTABLISHED
				 */
				nstate = IPF_TCPS_ESTABLISHED;
				rval = 1;
			} else if (tcpflags & TH_FIN) {
				/*
				 * we see an F from 'dir' which is in SYN_SENT
				 * state and wants to close its side of the
				 * connection; SYN_SENT -> FIN_WAIT_1
				 */
				nstate = IPF_TCPS_FIN_WAIT_1;
				rval = 1;
			} else if ((tcpflags & TH_OPENING) == TH_OPENING) {
				/*
				 * we see an SA from 'dir' which is already in
				 * SYN_SENT state, this means we have a
				 * simultaneous open; SYN_SENT -> SYN_RECEIVED
				 */
				nstate = IPF_TCPS_SYN_RECEIVED;
				rval = 1;
			}
			break;

		case IPF_TCPS_SYN_RECEIVED: /* 2 */
			if ((tcpflags & (TH_SYN|TH_FIN|TH_ACK)) == TH_ACK) {
				/*
				 * we see an A from 'dir' which was in
				 * SYN_RECEIVED state so it must now be in
				 * established state, SYN_RECEIVED ->
				 * ESTABLISHED
				 */
				nstate = IPF_TCPS_ESTABLISHED;
				rval = 1;
			} else if ((tcpflags & ~(TH_ECN|TH_CWR)) ==
				   TH_OPENING) {
				/*
				 * We see an SA from 'dir' which is already in
				 * SYN_RECEIVED state.
				 */
				rval = 2;
			} else if (tcpflags & TH_FIN) {
				/*
				 * we see an F from 'dir' which is in
				 * SYN_RECEIVED state and wants to close its
				 * side of the connection; SYN_RECEIVED ->
				 * FIN_WAIT_1
				 */
				nstate = IPF_TCPS_FIN_WAIT_1;
				rval = 1;
			}
			break;

		case IPF_TCPS_HALF_ESTAB: /* 3 */
			if (tcpflags & TH_FIN) {
				nstate = IPF_TCPS_FIN_WAIT_1;
				rval = 1;
			} else if ((tcpflags & TH_ACKMASK) == TH_ACK) {
				/*
				 * If we've picked up a connection in mid
				 * flight, we could be looking at a follow on
				 * packet from the same direction as the one
				 * that created this state.  Recognise it but
				 * do not advance the entire connection's
				 * state.
				 */
				switch (ostate)
				{
				case IPF_TCPS_LISTEN :
				case IPF_TCPS_SYN_SENT :
				case IPF_TCPS_SYN_RECEIVED :
					rval = 1;
					break;
				case IPF_TCPS_HALF_ESTAB :
				case IPF_TCPS_ESTABLISHED :
					nstate = IPF_TCPS_ESTABLISHED;
					rval = 1;
					break;
				default :
					break;
				}
			}
			break;

		case IPF_TCPS_ESTABLISHED: /* 4 */
			rval = 1;
			if (tcpflags & TH_FIN) {
				/*
				 * 'dir' closed its side of the connection;
				 * this gives us a half-closed connection;
				 * ESTABLISHED -> FIN_WAIT_1
				 */
				if (ostate == IPF_TCPS_FIN_WAIT_1) {
					nstate = IPF_TCPS_CLOSING;
				} else {
					nstate = IPF_TCPS_FIN_WAIT_1;
				}
			} else if (tcpflags & TH_ACK) {
				/*
				 * an ACK, should we exclude other flags here?
				 */
				if (ostate == IPF_TCPS_FIN_WAIT_1) {
					/*
					 * We know the other side did an active
					 * close, so we are ACKing the recvd
					 * FIN packet (does the window matching
					 * code guarantee this?) and go into
					 * CLOSE_WAIT state; this gives us a
					 * half-closed connection
					 */
					nstate = IPF_TCPS_CLOSE_WAIT;
				} else if (ostate < IPF_TCPS_CLOSE_WAIT) {
					/*
					 * still a fully established
					 * connection reset timeout
					 */
					nstate = IPF_TCPS_ESTABLISHED;
				}
			}
			break;

		case IPF_TCPS_CLOSE_WAIT: /* 5 */
			rval = 1;
			if (tcpflags & TH_FIN) {
				/*
				 * application closed and 'dir' sent a FIN,
				 * we're now going into LAST_ACK state
				 */
				nstate = IPF_TCPS_LAST_ACK;
			} else {
				/*
				 * we remain in CLOSE_WAIT because the other
				 * side has closed already and we did not
				 * close our side yet; reset timeout
				 */
				nstate = IPF_TCPS_CLOSE_WAIT;
			}
			break;

		case IPF_TCPS_FIN_WAIT_1: /* 6 */
			rval = 1;
			if ((tcpflags & TH_ACK) &&
			    ostate > IPF_TCPS_CLOSE_WAIT) {
				/*
				 * if the other side is not active anymore
				 * it has sent us a FIN packet that we are
				 * ack'ing now with an ACK; this means both
				 * sides have now closed the connection and
				 * we go into TIME_WAIT
				 */
				/*
				 * XXX: how do we know we really are ACKing
				 * the FIN packet here? does the window code
				 * guarantee that?
				 */
				nstate = IPF_TCPS_TIME_WAIT;
			} else {
				/*
				 * we closed our side of the connection
				 * already but the other side is still active
				 * (ESTABLISHED/CLOSE_WAIT); continue with
				 * this half-closed connection
				 */
				nstate = IPF_TCPS_FIN_WAIT_1;
			}
			break;

		case IPF_TCPS_CLOSING: /* 7 */
			if ((tcpflags & (TH_FIN|TH_ACK)) == TH_ACK) {
				nstate = IPF_TCPS_TIME_WAIT;
			}
			rval = 2;
			break;

		case IPF_TCPS_LAST_ACK: /* 8 */
			if (tcpflags & TH_ACK) {
				if ((tcpflags & TH_PUSH) || dlen)
					/*
					 * there is still data to be delivered,
					 * reset timeout
					 */
					rval = 1;
				else
					rval = 2;
			}
			/*
			 * we cannot detect when we go out of LAST_ACK state to
			 * CLOSED because that is based on the reception of ACK
			 * packets; ipfilter can only detect that a packet
			 * has been sent by a host
			 */
			break;

		case IPF_TCPS_FIN_WAIT_2: /* 9 */
			/* NOT USED */
			break;

		case IPF_TCPS_TIME_WAIT: /* 10 */
			/* we're in 2MSL timeout now */
			if (ostate == IPF_TCPS_LAST_ACK) {
				nstate = IPF_TCPS_CLOSED;
			}
			rval = 1;
			break;

		case IPF_TCPS_CLOSED: /* 11 */
			rval = 2;
			break;

		default :
#if defined(_KERNEL)
# if SOLARIS
			cmn_err(CE_NOTE,
				"tcp %lx flags %x si %lx nstate %d ostate %d\n",
				(u_long)tcp, tcpflags, (u_long)tqe,
				nstate, ostate);
# else
			printf("tcp %lx flags %x si %lx nstate %d ostate %d\n",
				(u_long)tcp, tcpflags, (u_long)tqe,
				nstate, ostate);
# endif
#else
			abort();
#endif
			break;
		}
	}

	/*
	 * If rval == 2 then do not update the queue position, but treat the
	 * packet as being ok.
	 */
	if (rval == 2)
		rval = 1;
	else if (rval == 1) {
		tqe->tqe_state[dir] = nstate;
		if ((tqe->tqe_flags & TQE_RULEBASED) == 0)
			fr_movequeue(tqe, tqe->tqe_ifq, tqtab + nstate);
	}

	return rval;
}


/* ------------------------------------------------------------------------ */
/* Function:    ipstate_log                                                 */
/* Returns:     Nil                                                         */
/* Parameters:  is(I)   - pointer to state structure                        */
/*              type(I) - type of log entry to create                       */
/*                                                                          */
/* Creates a state table log entry using the state structure and type info. */
/* passed in.  Log packet/byte counts, source/destination address and other */
/* protocol specific information.                                           */
/* ------------------------------------------------------------------------ */
void ipstate_log(is, type)
struct ipstate *is;
u_int type;
{
#ifdef	IPFILTER_LOG
	struct	ipslog	ipsl;
	size_t sizes[1];
	void *items[1];
	int types[1];

	/*
	 * Copy information out of the ipstate_t structure and into the
	 * structure used for logging.
	 */
	ipsl.isl_type = type;
	ipsl.isl_pkts[0] = is->is_pkts[0] + is->is_icmppkts[0];
	ipsl.isl_bytes[0] = is->is_bytes[0];
	ipsl.isl_pkts[1] = is->is_pkts[1] + is->is_icmppkts[1];
	ipsl.isl_bytes[1] = is->is_bytes[1];
	ipsl.isl_pkts[2] = is->is_pkts[2] + is->is_icmppkts[2];
	ipsl.isl_bytes[2] = is->is_bytes[2];
	ipsl.isl_pkts[3] = is->is_pkts[3] + is->is_icmppkts[3];
	ipsl.isl_bytes[3] = is->is_bytes[3];
	ipsl.isl_src = is->is_src;
	ipsl.isl_dst = is->is_dst;
	ipsl.isl_p = is->is_p;
	ipsl.isl_v = is->is_v;
	ipsl.isl_flags = is->is_flags;
	ipsl.isl_tag = is->is_tag;
	ipsl.isl_rulen = is->is_rulen;
	(void) strncpy(ipsl.isl_group, is->is_group, FR_GROUPLEN);

	if (ipsl.isl_p == IPPROTO_TCP || ipsl.isl_p == IPPROTO_UDP) {
		ipsl.isl_sport = is->is_sport;
		ipsl.isl_dport = is->is_dport;
		if (ipsl.isl_p == IPPROTO_TCP) {
			ipsl.isl_state[0] = is->is_state[0];
			ipsl.isl_state[1] = is->is_state[1];
		}
	} else if (ipsl.isl_p == IPPROTO_ICMP) {
		ipsl.isl_itype = is->is_icmp.ici_type;
	} else if (ipsl.isl_p == IPPROTO_ICMPV6) {
		ipsl.isl_itype = is->is_icmp.ici_type;
	} else {
		ipsl.isl_ps.isl_filler[0] = 0;
		ipsl.isl_ps.isl_filler[1] = 0;
	}

	items[0] = &ipsl;
	sizes[0] = sizeof(ipsl);
	types[0] = 0;

	if (ipllog(IPL_LOGSTATE, NULL, items, sizes, types, 1)) {
		ATOMIC_INCL(ips_stats.iss_logged);
	} else {
		ATOMIC_INCL(ips_stats.iss_logfail);
	}
#endif
}


#ifdef	USE_INET6
/* ------------------------------------------------------------------------ */
/* Function:    fr_checkicmp6matchingstate                                  */
/* Returns:     ipstate_t* - NULL == no match found,                        */
/*                           else  pointer to matching state entry          */
/* Parameters:  fin(I) - pointer to packet information                      */
/* Locks:       NULL == no locks, else Read Lock on ipf_state               */
/*                                                                          */
/* If we've got an ICMPv6 error message, using the information stored in    */
/* the ICMPv6 packet, look for a matching state table entry.                */
/* ------------------------------------------------------------------------ */
static ipstate_t *fr_checkicmp6matchingstate(fin)
fr_info_t *fin;
{
	struct icmp6_hdr *ic6, *oic;
	int type, backward, i;
	ipstate_t *is, **isp;
	u_short sport, dport;
	i6addr_t dst, src;
	u_short savelen;
	icmpinfo_t *ic;
	fr_info_t ofin;
	tcphdr_t *tcp;
	ip6_t *oip6;
	u_char	pr;
	u_int hv;

	/*
	 * Does it at least have the return (basic) IP header ?
	 * Is it an actual recognised ICMP error type?
	 * Only a basic IP header (no options) should be with
	 * an ICMP error header.
	 */
	if ((fin->fin_v != 6) || (fin->fin_plen < ICMP6ERR_MINPKTLEN) ||
	    !(fin->fin_flx & FI_ICMPERR))
		return NULL;

	ic6 = fin->fin_dp;
	type = ic6->icmp6_type;

	oip6 = (ip6_t *)((char *)ic6 + ICMPERR_ICMPHLEN);
	if (fin->fin_plen < sizeof(*oip6))
		return NULL;

	bcopy((char *)fin, (char *)&ofin, sizeof(*fin));
	ofin.fin_v = 6;
	ofin.fin_ifp = fin->fin_ifp;
	ofin.fin_out = !fin->fin_out;
	ofin.fin_m = NULL;	/* if dereferenced, panic XXX */
	ofin.fin_mp = NULL;	/* if dereferenced, panic XXX */

	/*
	 * We make a fin entry to be able to feed it to
	 * matchsrcdst. Note that not all fields are necessary
	 * but this is the cleanest way. Note further we fill
	 * in fin_mp such that if someone uses it we'll get
	 * a kernel panic. fr_matchsrcdst does not use this.
	 *
	 * watch out here, as ip is in host order and oip6 in network
	 * order. Any change we make must be undone afterwards.
	 */
	savelen = oip6->ip6_plen;
	oip6->ip6_plen = fin->fin_dlen - ICMPERR_ICMPHLEN;
	ofin.fin_flx = FI_NOCKSUM;
	ofin.fin_ip = (ip_t *)oip6;
	(void) fr_makefrip(sizeof(*oip6), (ip_t *)oip6, &ofin);
	ofin.fin_flx &= ~(FI_BAD|FI_SHORT);
	oip6->ip6_plen = savelen;

	if (oip6->ip6_nxt == IPPROTO_ICMPV6) {
		oic = (struct icmp6_hdr *)(oip6 + 1);
		/*
		 * an ICMP error can only be generated as a result of an
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
		hv = (pr = oip6->ip6_nxt);
		src.in6 = oip6->ip6_src;
		hv += src.in4.s_addr;
		dst.in6 = oip6->ip6_dst;
		hv += dst.in4.s_addr;
		hv += oic->icmp6_id;
		hv += oic->icmp6_seq;
		hv = DOUBLE_HASH(hv);

		READ_ENTER(&ipf_state);
		for (isp = &ips_table[hv]; ((is = *isp) != NULL); ) {
			ic = &is->is_icmp;
			isp = &is->is_hnext;
			if ((is->is_p == pr) &&
			    !(is->is_pass & FR_NOICMPERR) &&
			    (oic->icmp6_id == ic->ici_id) &&
			    (oic->icmp6_seq == ic->ici_seq) &&
			    (is = fr_matchsrcdst(&ofin, is, &src,
						 &dst, NULL, FI_ICMPCMP))) {
			    	/*
			    	 * in the state table ICMP query's are stored
			    	 * with the type of the corresponding ICMP
			    	 * response. Correct here
			    	 */
				if (((ic->ici_type == ICMP6_ECHO_REPLY) &&
				     (oic->icmp6_type == ICMP6_ECHO_REQUEST)) ||
				     (ic->ici_type - 1 == oic->icmp6_type )) {
				    	ips_stats.iss_hits++;
					backward = IP6_NEQ(&is->is_dst, &src);
					fin->fin_rev = !backward;
					i = (backward << 1) + fin->fin_out;
    					is->is_icmppkts[i]++;
					return is;
				}
			}
		}
		RWLOCK_EXIT(&ipf_state);
		return NULL;
	}

	hv = (pr = oip6->ip6_nxt);
	src.in6 = oip6->ip6_src;
	hv += src.i6[0];
	hv += src.i6[1];
	hv += src.i6[2];
	hv += src.i6[3];
	dst.in6 = oip6->ip6_dst;
	hv += dst.i6[0];
	hv += dst.i6[1];
	hv += dst.i6[2];
	hv += dst.i6[3];

	if ((oip6->ip6_nxt == IPPROTO_TCP) || (oip6->ip6_nxt == IPPROTO_UDP)) {
		tcp = (tcphdr_t *)(oip6 + 1);
		dport = tcp->th_dport;
		sport = tcp->th_sport;
		hv += dport;
		hv += sport;
	} else
		tcp = NULL;
	hv = DOUBLE_HASH(hv);

	READ_ENTER(&ipf_state);
	for (isp = &ips_table[hv]; ((is = *isp) != NULL); ) {
		isp = &is->is_hnext;
		/*
		 * Only allow this icmp though if the
		 * encapsulated packet was allowed through the
		 * other way around. Note that the minimal amount
		 * of info present does not allow for checking against
		 * tcp internals such as seq and ack numbers.
		 */
		if ((is->is_p != pr) || (is->is_v != 6) ||
		    (is->is_pass & FR_NOICMPERR))
			continue;
		is = fr_matchsrcdst(&ofin, is, &src, &dst, tcp, FI_ICMPCMP);
		if (is != NULL) {
			ips_stats.iss_hits++;
			backward = IP6_NEQ(&is->is_dst, &src);
			fin->fin_rev = !backward;
			i = (backward << 1) + fin->fin_out;
			is->is_icmppkts[i]++;
			/*
			 * we deliberately do not touch the timeouts
			 * for the accompanying state table entry.
			 * It remains to be seen if that is correct. XXX
			 */
			return is;
		}
	}
	RWLOCK_EXIT(&ipf_state);
	return NULL;
}
#endif


/* ------------------------------------------------------------------------ */
/* Function:    fr_sttab_init                                               */
/* Returns:     Nil                                                         */
/* Parameters:  tqp(I) - pointer to an array of timeout queues for TCP      */
/*                                                                          */
/* Initialise the array of timeout queues for TCP.                          */
/* ------------------------------------------------------------------------ */
void fr_sttab_init(tqp)
ipftq_t *tqp;
{
	int i;

	for (i = IPF_TCP_NSTATES - 1; i >= 0; i--) {
		tqp[i].ifq_ttl = 0;
		tqp[i].ifq_ref = 1;
		tqp[i].ifq_head = NULL;
		tqp[i].ifq_tail = &tqp[i].ifq_head;
		tqp[i].ifq_next = tqp + i + 1;
		MUTEX_INIT(&tqp[i].ifq_lock, "ipftq tcp tab");
	}
	tqp[IPF_TCP_NSTATES - 1].ifq_next = NULL;
	tqp[IPF_TCPS_CLOSED].ifq_ttl = fr_tcpclosed;
	tqp[IPF_TCPS_LISTEN].ifq_ttl = fr_tcptimeout;
	tqp[IPF_TCPS_SYN_SENT].ifq_ttl = fr_tcptimeout;
	tqp[IPF_TCPS_SYN_RECEIVED].ifq_ttl = fr_tcptimeout;
	tqp[IPF_TCPS_ESTABLISHED].ifq_ttl = fr_tcpidletimeout;
	tqp[IPF_TCPS_CLOSE_WAIT].ifq_ttl = fr_tcphalfclosed;
	tqp[IPF_TCPS_FIN_WAIT_1].ifq_ttl = fr_tcphalfclosed;
	tqp[IPF_TCPS_CLOSING].ifq_ttl = fr_tcptimeout;
	tqp[IPF_TCPS_LAST_ACK].ifq_ttl = fr_tcplastack;
	tqp[IPF_TCPS_FIN_WAIT_2].ifq_ttl = fr_tcpclosewait;
	tqp[IPF_TCPS_TIME_WAIT].ifq_ttl = fr_tcptimewait;
	tqp[IPF_TCPS_HALF_ESTAB].ifq_ttl = fr_tcptimeout;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_sttab_destroy                                            */
/* Returns:     Nil                                                         */
/* Parameters:  tqp(I) - pointer to an array of timeout queues for TCP      */
/*                                                                          */
/* Do whatever is necessary to "destroy" each of the entries in the array   */
/* of timeout queues for TCP.                                               */
/* ------------------------------------------------------------------------ */
void fr_sttab_destroy(tqp)
ipftq_t *tqp;
{
	int i;

	for (i = IPF_TCP_NSTATES - 1; i >= 0; i--)
		MUTEX_DESTROY(&tqp[i].ifq_lock);
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_statederef                                               */
/* Returns:     Nil                                                         */
/* Parameters:  isp(I) - pointer to pointer to state table entry            */
/*                                                                          */
/* Decrement the reference counter for this state table entry and free it   */
/* if there are no more things using it.                                    */
/*                                                                          */
/* This function is only called when cleaning up after increasing is_ref by */
/* one earlier in the 'code path' so if is_ref is 1 when entering, we do    */
/* have an orphan, otherwise not.  However there is a possible race between */
/* the entry being deleted via flushing with an ioctl call (that calls the  */
/* delete function directly) and the tail end of packet processing so we    */
/* need to grab is_lock before doing the check to synchronise the two code  */
/* paths.                                                                   */
/*                                                                          */
/* When operating in userland (ipftest), we have no timers to clear a state */
/* entry.  Therefore, we make a few simple tests before deleting an entry   */
/* outright.  We compare states on each side looking for a combination of   */
/* TIME_WAIT (should really be FIN_WAIT_2?) and LAST_ACK.  Then we factor   */
/* in packet direction with the interface list to make sure we don't        */
/* prematurely delete an entry on a final inbound packet that's we're also  */
/* supposed to route elsewhere.                                             */
/*                                                                          */
/* Internal parameters:                                                     */
/*    state[0] = state of source (host that initiated connection)           */
/*    state[1] = state of dest   (host that accepted the connection)        */
/*                                                                          */
/*    dir == 0 : a packet from source to dest                               */
/*    dir == 1 : a packet from dest to source                               */
/* ------------------------------------------------------------------------ */
void fr_statederef(isp)
ipstate_t **isp;
{
	ipstate_t *is;

	is = *isp;
	*isp = NULL;

	MUTEX_ENTER(&is->is_lock);
	if (is->is_ref > 1) {
		is->is_ref--;
		MUTEX_EXIT(&is->is_lock);
#ifndef	_KERNEL
		if ((is->is_sti.tqe_state[0] > IPF_TCPS_ESTABLISHED) ||
		   (is->is_sti.tqe_state[1] > IPF_TCPS_ESTABLISHED)) {
			fr_delstate(is, ISL_ORPHAN);
		}
#endif
		return;
	}
	MUTEX_EXIT(&is->is_lock);

	WRITE_ENTER(&ipf_state);
	fr_delstate(is, ISL_EXPIRE);
	RWLOCK_EXIT(&ipf_state);
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_setstatequeue                                            */
/* Returns:     Nil                                                         */
/* Parameters:  is(I) - pointer to state structure                          */
/*              rev(I) - forward(0) or reverse(1) direction                 */
/* Locks:       ipf_state (read or write)                                   */
/*                                                                          */
/* Put the state entry on its default queue entry, using rev as a helped in */
/* determining which queue it should be placed on.                          */
/* ------------------------------------------------------------------------ */
void fr_setstatequeue(is, rev)
ipstate_t *is;
int rev;
{
	ipftq_t *oifq, *nifq;


	if ((is->is_sti.tqe_flags & TQE_RULEBASED) != 0)
		nifq = is->is_tqehead[rev];
	else
		nifq = NULL;

	if (nifq == NULL) {
		switch (is->is_p)
		{
#ifdef USE_INET6
		case IPPROTO_ICMPV6 :
			if (rev == 1)
				nifq = &ips_icmpacktq;
			else
				nifq = &ips_icmptq;
			break;
#endif
		case IPPROTO_ICMP :
			if (rev == 1)
				nifq = &ips_icmpacktq;
			else
				nifq = &ips_icmptq;
			break;
		case IPPROTO_TCP :
			nifq = ips_tqtqb + is->is_state[rev];
			break;

		case IPPROTO_UDP :
			if (rev == 1)
				nifq = &ips_udpacktq;
			else
				nifq = &ips_udptq;
			break;

		default :
			nifq = &ips_iptq;
			break;
		}
	}

	oifq = is->is_sti.tqe_ifq;
	/*
	 * If it's currently on a timeout queue, move it from one queue to
	 * another, else put it on the end of the newly determined queue.
	 */
	if (oifq != NULL)
		fr_movequeue(&is->is_sti, oifq, nifq);
	else
		fr_queueappend(&is->is_sti, nifq, is);
	return;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_stateiter                                                */
/* Returns:     int - 0 == success, else error                              */
/* Parameters:  token(I) - pointer to ipftoken structure                    */
/*              itp(I)   - pointer to ipfgeniter structure                  */
/*                                                                          */
/* This function handles the SIOCGENITER ioctl for the state tables and     */
/* walks through the list of entries in the state table list (ips_list.)    */
/* ------------------------------------------------------------------------ */
static int fr_stateiter(token, itp)
ipftoken_t *token;
ipfgeniter_t *itp;
{
	ipstate_t *is, *next, zero;
	int error, count;
	char *dst;

	if (itp->igi_data == NULL)
		return EFAULT;

	if (itp->igi_nitems < 1)
		return ENOSPC;

	if (itp->igi_type != IPFGENITER_STATE)
		return EINVAL;

	is = token->ipt_data;
	if (is == (void *)-1) {
		ipf_freetoken(token);
		return ESRCH;
	}

	error = 0;
	dst = itp->igi_data;

	READ_ENTER(&ipf_state);
	if (is == NULL) {
		next = ips_list;
	} else {
		next = is->is_next;
	}

	count = itp->igi_nitems;
	for (;;) {
		if (next != NULL) {
			/*
			 * If we find a state entry to use, bump its
			 * reference count so that it can be used for
			 * is_next when we come back.
			 */
			if (count == 1) {
				MUTEX_ENTER(&next->is_lock);
				next->is_ref++;
				MUTEX_EXIT(&next->is_lock);
				token->ipt_data = next;
			}
		} else {
			bzero(&zero, sizeof(zero));
			next = &zero;
			count = 1;
			token->ipt_data = NULL;
		}
		RWLOCK_EXIT(&ipf_state);

		/*
		 * This should arguably be via fr_outobj() so that the state
		 * structure can (if required) be massaged going out.
		 */
		error = COPYOUT(next, dst, sizeof(*next));
		if (error != 0)
			error = EFAULT;
		if ((count == 1) || (error != 0))
			break;

		dst += sizeof(*next);
		count--;

		READ_ENTER(&ipf_state);
		next = next->is_next;
	}

	if (is != NULL) {
		fr_statederef(&is);
	}

	return error;
}


/* ------------------------------------------------------------------------ */
/* Function:    fr_stgettable                                               */
/* Returns:     int     - 0 = success, else error                           */
/* Parameters:  data(I) - pointer to ioctl data                             */
/*                                                                          */
/* This function handles ioctl requests for tables of state information.    */
/* At present the only table it deals with is the hash bucket statistics.   */
/* ------------------------------------------------------------------------ */
static int fr_stgettable(data)
char *data;
{
	ipftable_t table;
	int error;

	error = fr_inobj(data, &table, IPFOBJ_GTABLE);
	if (error != 0)
		return error;

	if (table.ita_type != IPFTABLE_BUCKETS)
		return EINVAL;

	error = COPYOUT(ips_stats.iss_bucketlen, table.ita_table, 
			fr_statesize * sizeof(u_long));
	if (error != 0)
		error = EFAULT;
	return error;
}
