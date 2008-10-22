/*-
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1994, 1995
 *	The Regents of the University of California.
 * Copyright (c) 2008 Swinburne University of Technology, Melbourne, Australia
 * All rights reserved.
 *
 * The majority of this software was developed at the Centre for
 * Advanced Internet Architectures, Swinburne University, by Lawrence Stewart
 * and James Healy, made possible in part by a grant from the Cisco University
 * Research Program Fund at Community Foundation Silicon Valley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sbuf.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_fsm.h>
#include <netinet/cc.h>


/* list of available cc algorithms on the current system */
struct cc_head cc_list = STAILQ_HEAD_INITIALIZER(cc_list); 

struct rwlock cc_list_lock;

MALLOC_DECLARE(M_STRING);
MALLOC_DEFINE(M_STRING, "string", "a string");

/* create a struct to point to our newreno functions */
struct cc_algo newreno_cc_algo = {
	.name = "newreno",
	.init = newreno_init,
	.deinit = NULL,
	.cwnd_init = newreno_cwnd_init,
	.ack_received = newreno_ack_received,
	.pre_fr = newreno_pre_fr,
	.post_fr = newreno_post_fr,
	.after_idle = newreno_after_idle,
	.after_timeout = newreno_after_timeout
};

/* the system wide default cc algorithm */
char cc_algorithm[TCP_CA_NAME_MAX];

/*
 * sysctl handler that allows the default cc algorithm for the system to be
 * viewed and changed
 */
static int
cc_default_algorithm(SYSCTL_HANDLER_ARGS)
{
	struct cc_algo *funcs;

	if (!req->newptr)
		goto skip;

	CC_LIST_RLOCK();
	STAILQ_FOREACH(funcs, &cc_list, entries) {
		if (strncmp((char *)req->newptr, funcs->name, TCP_CA_NAME_MAX) == 0)
			goto reorder;
	}
	CC_LIST_RUNLOCK();

	return 1;

reorder:
	/*
	 * Make the selected system default cc algorithm
	 * the first element in the list if it isn't already
	 */
	CC_LIST_RUNLOCK();
	CC_LIST_WLOCK();
	if (funcs != STAILQ_FIRST(&cc_list)) {
		STAILQ_REMOVE(&cc_list, funcs, cc_algo, entries);
		STAILQ_INSERT_HEAD(&cc_list, funcs, entries);
	}
	CC_LIST_WUNLOCK();

skip:
	return sysctl_handle_string(oidp, arg1, arg2, req);
}

/*
 * sysctl handler that displays the available cc algorithms as a read
 * only value
 */
static int
cc_list_available(SYSCTL_HANDLER_ARGS)
{
	struct cc_algo *algo;
	int error = 0, first = 1;
	struct sbuf *s = NULL;

	if ((s = sbuf_new(NULL, NULL, TCP_CA_NAME_MAX, SBUF_AUTOEXTEND)) == NULL)
		return -1;

	CC_LIST_RLOCK();
	STAILQ_FOREACH(algo, &cc_list, entries) {
		error = sbuf_printf(s, (first) ? "%s" : ", %s", algo->name);
		if (error != 0)
			break;
		first = 0;
	}
	CC_LIST_RUNLOCK();

	if (!error) {
		sbuf_finish(s);
		error = sysctl_handle_string(oidp, sbuf_data(s), 1, req);
	}

	sbuf_delete(s);
	return error;
}

/*
 * Initialise cc on system boot
 */
void 
cc_init()
{
	/* initialise the lock that will protect read/write access to our linked list */
	CC_LIST_LOCK_INIT();

	/* initilize list of cc algorithms */
	STAILQ_INIT(&cc_list);

	/* add newreno to the list of available algorithms */
	cc_register_algorithm(&newreno_cc_algo);

	/* set newreno to the system default */
	strlcpy(cc_algorithm, newreno_cc_algo.name, TCP_CA_NAME_MAX);
}

/*
 * Returns 1 on success, 0 on failure
 */
int
cc_deregister_algorithm(struct cc_algo *remove_cc)
{
	struct cc_algo *funcs, *tmpfuncs;
	register struct tcpcb *tp = NULL;
	register struct inpcb *inp = NULL;
	int success = 0;

	/* remove the algorithm from the list available to the system */
	CC_LIST_RLOCK();
	STAILQ_FOREACH_SAFE(funcs, &cc_list, entries, tmpfuncs) {
		if (funcs == remove_cc) {
			if (CC_LIST_TRY_WLOCK()) {
				/* if this algorithm is the system default, reset the default to newreno */
				if (strncmp(cc_algorithm, remove_cc->name, TCP_CA_NAME_MAX) == 0)
					snprintf(cc_algorithm, TCP_CA_NAME_MAX, "%s", newreno_cc_algo.name);

				STAILQ_REMOVE(&cc_list, funcs, cc_algo, entries);
				success = 1;
				CC_LIST_W2RLOCK();
			}
			break;
		}
	}
	CC_LIST_RUNLOCK();

	if (success) {
		/*
		 * check all active control blocks and change any that are using this
		 * algorithm back to newreno. If the algorithm that was in use requires
		 * deinit code to be run, call it
		 */
		INP_INFO_RLOCK(&tcbinfo);
		LIST_FOREACH(inp, &tcb, inp_list) {
			/* skip tcptw structs */
			if (inp->inp_vflag & INP_TIMEWAIT)
				continue;
			INP_WLOCK(inp);
			if ((tp = intotcpcb(inp)) != NULL) {
				if (strncmp(CC_ALGO(tp)->name, remove_cc->name, TCP_CA_NAME_MAX) == 0 ) {
					tmpfuncs = CC_ALGO(tp);
					CC_ALGO(tp) = &newreno_cc_algo;
					/*
					 * XXX: We should stall here until
					 * we're sure the tcb has stopped
					 * using the deregistered algo's functions...
					 * Not sure how to do that yet!
					 */
					if(CC_ALGO(tp)->init)
						CC_ALGO(tp)->init(tp);
					if (tmpfuncs->deinit)
						tmpfuncs->deinit(tp);
				}
			}
			INP_WUNLOCK(inp);
		}
		INP_INFO_RUNLOCK(&tcbinfo);
	}

	return success;
}

int
cc_register_algorithm(struct cc_algo *add_cc)
{
	CC_LIST_WLOCK();
	STAILQ_INSERT_TAIL(&cc_list, add_cc, entries);
	CC_LIST_WUNLOCK();
	return 1;
}

/*
 * NEW RENO
 */

int
newreno_init(struct tcpcb *tp)
{
	printf("initialising tcp connection with newreno congestion control\n");
	return 0;
}

/*
 * update ssthresh to approx 1/2 of cwnd
 */
void
newreno_ssthresh_update(struct tcpcb *tp)
{
	u_int win;

	/* reset ssthresh */
	win = min(tp->snd_wnd, tp->snd_cwnd) / 2 / tp->t_maxseg;

	if (win < 2)
		win = 2;

	tp->snd_ssthresh = win * tp->t_maxseg;
}

/*
 * initial cwnd at the start of a connection
 * if there is a hostcache entry for the foreign host, base cwnd on that
 * if rfc3390 is enabled, set cwnd to approx 4 MSS as recommended
 * otherwise use the sysctl variables configured by the administrator
 */
void
newreno_cwnd_init(struct tcpcb *tp)
{
	struct hc_metrics_lite metrics;
	struct inpcb *inp = tp->t_inpcb;
	struct socket *so = inp->inp_socket;

	/*
	 * Set the slow-start flight size depending on whether this
	 * is a local network or not.
	 *
	 * Extend this so we cache the cwnd too and retrieve it here.
	 * Make cwnd even bigger than RFC3390 suggests but only if we
	 * have previous experience with the remote host. Be careful
	 * not make cwnd bigger than remote receive window or our own
	 * send socket buffer. Maybe put some additional upper bound
	 * on the retrieved cwnd. Should do incremental updates to
	 * hostcache when cwnd collapses so next connection doesn't
	 * overloads the path again.
	 *
	 * RFC3390 says only do this if SYN or SYN/ACK didn't got lost.
	 * We currently check only in syncache_socket for that.
	 */

	tcp_hc_get(&inp->inp_inc, &metrics);

#define TCP_METRICS_CWND
#ifdef TCP_METRICS_CWND
	if (metrics.rmx_cwnd)
		tp->snd_cwnd = max(tp->t_maxseg,
				min(metrics.rmx_cwnd / 2,
				 min(tp->snd_wnd, so->so_snd.sb_hiwat)));
	else
#endif
	if (tcp_do_rfc3390)
		tp->snd_cwnd = min(4 * tp->t_maxseg, max(2 * tp->t_maxseg, 4380));
#ifdef INET6
	else if ((isipv6 && in6_localaddr(&inp->in6p_faddr)) ||
		 (!isipv6 && in_localaddr(inp->inp_faddr)))
#else
	else if (in_localaddr(inp->inp_faddr))
#endif
		tp->snd_cwnd = tp->t_maxseg * ss_fltsz_local;
	else
		tp->snd_cwnd = tp->t_maxseg * ss_fltsz;
}

/*
 * increase cwnd on receipt of a successful ACK
 * if cwnd <= ssthresh, increases by 1 MSS per ACK
 * if cwnd > ssthresh, increase by ~1 MSS per RTT
 */
void
newreno_ack_received(struct tcpcb *tp, struct tcphdr *th)
{
	u_int cw = tp->snd_cwnd;
	u_int incr = tp->t_maxseg;

	/*
	 * If cwnd <= ssthresh, open exponentially (maxseg per packet).
	 * Otherwise, open linearly (approx. maxseg per RTT
	 * i.e. maxseg^2 / cwnd per ACK received).
	 * If cwnd > maxseg^2, fix the cwnd increment at 1 byte
	 * to avoid capping cwnd (as suggested in RFC2581).
	 */
	if (cw > tp->snd_ssthresh)
		incr = max((incr * incr / cw), 1);

	tp->snd_cwnd = min(cw+incr, TCP_MAXWIN<<tp->snd_scale);
}

/*
 * update the value of ssthresh before entering FR
 */
void 
newreno_pre_fr(struct tcpcb *tp, struct tcphdr *th)
{
  newreno_ssthresh_update(tp);
}

/*
 * decrease the cwnd in response to packet loss or a transmit timeout.
 * th can be null, in which case cwnd will be set according to reno instead
 * of new reno.
 */
void 
newreno_post_fr(struct tcpcb *tp, struct tcphdr *th)
{
	/*
	* Out of fast recovery.
	* Window inflation should have left us
	* with approximately snd_ssthresh
	* outstanding data.
	* But in case we would be inclined to
	* send a burst, better to do it via
	* the slow start mechanism.
	*/
	if (th && SEQ_GT(th->th_ack + tp->snd_ssthresh, tp->snd_max))
		tp->snd_cwnd = tp->snd_max - th->th_ack + tp->t_maxseg;
	else
		tp->snd_cwnd = tp->snd_ssthresh;
}

/*
 * if a connection has been idle for a while and more data is ready to be sent,
 * reset cwnd
 */
void
newreno_after_idle(struct tcpcb *tp)
{
	/*
	* We have been idle for "a while" and no acks are
	* expected to clock out any data we send --
	* slow start to get ack "clock" running again.
	*
	* Set the slow-start flight size depending on whether
	* this is a local network or not.
	*
	* Set the slow-start flight size depending on whether
	* this is a local network or not.
	*/
	int ss = ss_fltsz;

#ifdef INET6
	if (isipv6) {
		if (in6_localaddr(&tp->t_inpcb->in6p_faddr))
			ss = ss_fltsz_local;
	} else
#endif /* INET6 */

	if (in_localaddr(tp->t_inpcb->inp_faddr))
		ss = ss_fltsz_local;

	tp->snd_cwnd = tp->t_maxseg * ss;
}

/*
 * reset the cwnd after a transmission timeout.
 */
void
newreno_after_timeout(struct tcpcb *tp)
{
	newreno_ssthresh_update(tp);

	/*
	 * Close the congestion window down to one segment
	 * (we'll open it by one segment for each ack we get).
	 * Since we probably have a window's worth of unacked
	 * data accumulated, this "slow start" keeps us from
	 * dumping all that data as back-to-back packets (which
	 * might overwhelm an intermediate gateway).
	 *
	 * There are two phases to the opening: Initially we
	 * open by one mss on each ack.  This makes the window
	 * size increase exponentially with time.  If the
	 * window is larger than the path can handle, this
	 * exponential growth results in dropped packet(s)
	 * almost immediately.  To get more time between
	 * drops but still "push" the network to take advantage
	 * of improving conditions, we switch from exponential
	 * to linear window opening at some threshhold size.
	 * For a threshhold, we use half the current window
	 * size, truncated to a multiple of the mss.
	 *
	 * (the minimum cwnd that will give us exponential
	 * growth is 2 mss.  We don't allow the threshhold
	 * to go below this.)
	 */
	tp->snd_cwnd = tp->t_maxseg;
}

SYSCTL_NODE(_net_inet_tcp, OID_AUTO, cc, CTLFLAG_RW, NULL,
	"congestion control related settings");

SYSCTL_PROC(_net_inet_tcp_cc, OID_AUTO, algorithm, CTLTYPE_STRING|CTLFLAG_RW,
	&cc_algorithm, sizeof(cc_algorithm), cc_default_algorithm, "A",
	"default congestion control algorithm");

SYSCTL_PROC(_net_inet_tcp_cc, OID_AUTO, available, CTLTYPE_STRING|CTLFLAG_RD,
	NULL, 0, cc_list_available, "A",
	"list available congestion control algorithms");
