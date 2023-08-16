/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2007-2009
 * 	Swinburne University of Technology, Melbourne, Australia.
 * Copyright (c) 2009-2010, The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed at the Centre for Advanced
 * Internet Architectures, Swinburne University of Technology, Melbourne,
 * Australia by Lawrence Stewart under sponsorship from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/******************************************************
 * Statistical Information For TCP Research (SIFTR)
 *
 * A FreeBSD kernel module that adds very basic intrumentation to the
 * TCP stack, allowing internal stats to be recorded to a log file
 * for experimental, debugging and performance analysis purposes.
 *
 * SIFTR was first released in 2007 by James Healy and Lawrence Stewart whilst
 * working on the NewTCP research project at Swinburne University of
 * Technology's Centre for Advanced Internet Architectures, Melbourne,
 * Australia, which was made possible in part by a grant from the Cisco
 * University Research Program Fund at Community Foundation Silicon Valley.
 * More details are available at:
 *   http://caia.swin.edu.au/urp/newtcp/
 *
 * Work on SIFTR v1.2.x was sponsored by the FreeBSD Foundation as part of
 * the "Enhancing the FreeBSD TCP Implementation" project 2008-2009.
 * More details are available at:
 *   http://www.freebsdfoundation.org/
 *   http://caia.swin.edu.au/freebsd/etcp09/
 *
 * Lawrence Stewart is the current maintainer, and all contact regarding
 * SIFTR should be directed to him via email: lastewart@swin.edu.au
 *
 * Initial release date: June 2007
 * Most recent update: September 2010
 ******************************************************/

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/alq.h>
#include <sys/errno.h>
#include <sys/eventhandler.h>
#include <sys/hash.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
#include <sys/sdt.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/pfil.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_kdtrace.h>
#include <netinet/in_fib.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp_var.h>

#ifdef SIFTR_IPV6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_fib.h>
#include <netinet6/in6_pcb.h>
#endif /* SIFTR_IPV6 */

#include <machine/in_cksum.h>

/*
 * Three digit version number refers to X.Y.Z where:
 * X is the major version number
 * Y is bumped to mark backwards incompatible changes
 * Z is bumped to mark backwards compatible changes
 */
#define V_MAJOR		1
#define V_BACKBREAK	3
#define V_BACKCOMPAT	0
#define MODVERSION	__CONCAT(V_MAJOR, __CONCAT(V_BACKBREAK, V_BACKCOMPAT))
#define MODVERSION_STR	__XSTRING(V_MAJOR) "." __XSTRING(V_BACKBREAK) "." \
    __XSTRING(V_BACKCOMPAT)

#define HOOK 0
#define UNHOOK 1
#define SIFTR_EXPECTED_MAX_TCP_FLOWS 65536
#define SYS_NAME "FreeBSD"
#define PACKET_TAG_SIFTR 100
#define PACKET_COOKIE_SIFTR 21749576
#define SIFTR_LOG_FILE_MODE 0644
#define SIFTR_DISABLE 0
#define SIFTR_ENABLE 1

/*
 * Hard upper limit on the length of log messages. Bump this up if you add new
 * data fields such that the line length could exceed the below value.
 */
#define MAX_LOG_MSG_LEN 300
/* XXX: Make this a sysctl tunable. */
#define SIFTR_ALQ_BUFLEN (1000*MAX_LOG_MSG_LEN)

#ifdef SIFTR_IPV6
#define SIFTR_IPMODE 6
#else
#define SIFTR_IPMODE 4
#endif

static MALLOC_DEFINE(M_SIFTR, "siftr", "dynamic memory used by SIFTR");
static MALLOC_DEFINE(M_SIFTR_PKTNODE, "siftr_pktnode",
    "SIFTR pkt_node struct");
static MALLOC_DEFINE(M_SIFTR_HASHNODE, "siftr_hashnode",
    "SIFTR flow_hash_node struct");

/* Used as links in the pkt manager queue. */
struct pkt_node {
	/* Timestamp of pkt as noted in the pfil hook. */
	struct timeval		tval;
	/* Direction pkt is travelling. */
	enum {
		DIR_IN = 0,
		DIR_OUT = 1,
	}			direction;
	/* IP version pkt_node relates to; either INP_IPV4 or INP_IPV6. */
	uint8_t			ipver;
	/* Local TCP port. */
	uint16_t		lport;
	/* Foreign TCP port. */
	uint16_t		fport;
	/* Local address. */
	union in_dependaddr	laddr;
	/* Foreign address. */
	union in_dependaddr	faddr;
	/* Congestion Window (bytes). */
	uint32_t		snd_cwnd;
	/* Sending Window (bytes). */
	uint32_t		snd_wnd;
	/* Receive Window (bytes). */
	uint32_t		rcv_wnd;
	/* More tcpcb flags storage */
	uint32_t		t_flags2;
	/* Slow Start Threshold (bytes). */
	uint32_t		snd_ssthresh;
	/* Current state of the TCP FSM. */
	int			conn_state;
	/* Max Segment Size (bytes). */
	uint32_t		mss;
	/* Smoothed RTT (usecs). */
	uint32_t		srtt;
	/* Is SACK enabled? */
	u_char			sack_enabled;
	/* Window scaling for snd window. */
	u_char			snd_scale;
	/* Window scaling for recv window. */
	u_char			rcv_scale;
	/* TCP control block flags. */
	u_int			t_flags;
	/* Retransmission timeout (usec). */
	uint32_t		rto;
	/* Size of the TCP send buffer in bytes. */
	u_int			snd_buf_hiwater;
	/* Current num bytes in the send socket buffer. */
	u_int			snd_buf_cc;
	/* Size of the TCP receive buffer in bytes. */
	u_int			rcv_buf_hiwater;
	/* Current num bytes in the receive socket buffer. */
	u_int			rcv_buf_cc;
	/* Number of bytes inflight that we are waiting on ACKs for. */
	u_int			sent_inflight_bytes;
	/* Number of segments currently in the reassembly queue. */
	int			t_segqlen;
	/* Flowid for the connection. */
	u_int			flowid;
	/* Flow type for the connection. */
	u_int			flowtype;
	/* Link to next pkt_node in the list. */
	STAILQ_ENTRY(pkt_node)	nodes;
};

struct flow_info
{
#ifdef SIFTR_IPV6
	char	laddr[INET6_ADDRSTRLEN];	/* local IP address */
	char	faddr[INET6_ADDRSTRLEN];	/* foreign IP address */
#else
	char	laddr[INET_ADDRSTRLEN];		/* local IP address */
	char	faddr[INET_ADDRSTRLEN];		/* foreign IP address */
#endif
	uint16_t	lport;			/* local TCP port */
	uint16_t	fport;			/* foreign TCP port */
	uint32_t	key;			/* flowid of the connection */
};

struct flow_hash_node
{
	uint16_t counter;
	struct flow_info const_info;		/* constant connection info */
	LIST_ENTRY(flow_hash_node) nodes;
};

struct siftr_stats
{
	/* # TCP pkts seen by the SIFTR PFIL hooks, including any skipped. */
	uint64_t n_in;
	uint64_t n_out;
	/* # pkts skipped due to failed malloc calls. */
	uint32_t nskip_in_malloc;
	uint32_t nskip_out_malloc;
	/* # pkts skipped due to failed inpcb lookups. */
	uint32_t nskip_in_inpcb;
	uint32_t nskip_out_inpcb;
	/* # pkts skipped due to failed tcpcb lookups. */
	uint32_t nskip_in_tcpcb;
	uint32_t nskip_out_tcpcb;
	/* # pkts skipped due to stack reinjection. */
	uint32_t nskip_in_dejavu;
	uint32_t nskip_out_dejavu;
};

DPCPU_DEFINE_STATIC(struct siftr_stats, ss);

static volatile unsigned int siftr_exit_pkt_manager_thread = 0;
static unsigned int siftr_enabled = 0;
static unsigned int siftr_pkts_per_log = 1;
static uint16_t     siftr_port_filter = 0;
/* static unsigned int siftr_binary_log = 0; */
static char siftr_logfile[PATH_MAX] = "/var/log/siftr.log";
static char siftr_logfile_shadow[PATH_MAX] = "/var/log/siftr.log";
static u_long siftr_hashmask;
STAILQ_HEAD(pkthead, pkt_node) pkt_queue = STAILQ_HEAD_INITIALIZER(pkt_queue);
LIST_HEAD(listhead, flow_hash_node) *counter_hash;
static int wait_for_pkt;
static struct alq *siftr_alq = NULL;
static struct mtx siftr_pkt_queue_mtx;
static struct mtx siftr_pkt_mgr_mtx;
static struct thread *siftr_pkt_manager_thr = NULL;
static char direction[2] = {'i','o'};

/* Required function prototypes. */
static int siftr_sysctl_enabled_handler(SYSCTL_HANDLER_ARGS);
static int siftr_sysctl_logfile_name_handler(SYSCTL_HANDLER_ARGS);

/* Declare the net.inet.siftr sysctl tree and populate it. */

SYSCTL_DECL(_net_inet_siftr);

SYSCTL_NODE(_net_inet, OID_AUTO, siftr, CTLFLAG_RW | CTLFLAG_MPSAFE, NULL,
    "siftr related settings");

SYSCTL_PROC(_net_inet_siftr, OID_AUTO, enabled,
    CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &siftr_enabled, 0, &siftr_sysctl_enabled_handler, "IU",
    "switch siftr module operations on/off");

SYSCTL_PROC(_net_inet_siftr, OID_AUTO, logfile,
    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_NEEDGIANT, &siftr_logfile_shadow,
    sizeof(siftr_logfile_shadow), &siftr_sysctl_logfile_name_handler, "A",
    "file to save siftr log messages to");

SYSCTL_UINT(_net_inet_siftr, OID_AUTO, ppl, CTLFLAG_RW,
    &siftr_pkts_per_log, 1,
    "number of packets between generating a log message");

SYSCTL_U16(_net_inet_siftr, OID_AUTO, port_filter, CTLFLAG_RW,
    &siftr_port_filter, 0,
    "enable packet filter on a TCP port");

/* XXX: TODO
SYSCTL_UINT(_net_inet_siftr, OID_AUTO, binary, CTLFLAG_RW,
    &siftr_binary_log, 0,
    "write log files in binary instead of ascii");
*/

/* Begin functions. */

static inline struct flow_hash_node *
siftr_find_flow(struct listhead *counter_list, uint32_t id)
{
	struct flow_hash_node *hash_node;
	/*
	 * If the list is not empty i.e. the hash index has
	 * been used by another flow previously.
	 */
	if (LIST_FIRST(counter_list) != NULL) {
		/*
		 * Loop through the hash nodes in the list.
		 * There should normally only be 1 hash node in the list.
		 */
		LIST_FOREACH(hash_node, counter_list, nodes) {
			/*
			 * Check if the key for the pkt we are currently
			 * processing is the same as the key stored in the
			 * hash node we are currently processing.
			 * If they are the same, then we've found the
			 * hash node that stores the counter for the flow
			 * the pkt belongs to.
			 */
			if (hash_node->const_info.key == id) {
				return hash_node;
			}
		}
	}

	return NULL;
}

static inline struct flow_hash_node *
siftr_new_hash_node(struct flow_info info, int dir,
		    struct siftr_stats *ss)
{
	struct flow_hash_node *hash_node;
	struct listhead *counter_list;

	counter_list = counter_hash + (info.key & siftr_hashmask);
	/* Create a new hash node to store the flow's constant info. */
	hash_node = malloc(sizeof(struct flow_hash_node), M_SIFTR_HASHNODE,
			   M_NOWAIT|M_ZERO);

	if (hash_node != NULL) {
		/* Initialise our new hash node list entry. */
		hash_node->counter = 0;
		hash_node->const_info = info;
		LIST_INSERT_HEAD(counter_list, hash_node, nodes);
		return hash_node;
	} else {
		/* malloc failed */
		if (dir == DIR_IN)
			ss->nskip_in_malloc++;
		else
			ss->nskip_out_malloc++;

		return NULL;
	}
}

static void
siftr_process_pkt(struct pkt_node * pkt_node)
{
	struct flow_hash_node *hash_node;
	struct listhead *counter_list;
	struct ale *log_buf;

	if (pkt_node->flowid == 0) {
		panic("%s: flowid not available", __func__);
	}

	counter_list = counter_hash + (pkt_node->flowid & siftr_hashmask);
	hash_node = siftr_find_flow(counter_list, pkt_node->flowid);

	if (hash_node == NULL) {
		return;
	} else if (siftr_pkts_per_log > 1) {
		/*
		 * Taking the remainder of the counter divided
		 * by the current value of siftr_pkts_per_log
		 * and storing that in counter provides a neat
		 * way to modulate the frequency of log
		 * messages being written to the log file.
		 */
		hash_node->counter = (hash_node->counter + 1) %
				     siftr_pkts_per_log;
		/*
		 * If we have not seen enough packets since the last time
		 * we wrote a log message for this connection, return.
		 */
		if (hash_node->counter > 0)
			return;
	}

	log_buf = alq_getn(siftr_alq, MAX_LOG_MSG_LEN, ALQ_WAITOK);

	if (log_buf == NULL)
		return; /* Should only happen if the ALQ is shutting down. */

	/* Construct a log message. */
	log_buf->ae_bytesused = snprintf(log_buf->ae_data, MAX_LOG_MSG_LEN,
	    "%c,%jd.%06ld,%s,%hu,%s,%hu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,"
	    "%u,%u,%u,%u,%u,%u,%u,%u\n",
	    direction[pkt_node->direction],
	    (intmax_t)pkt_node->tval.tv_sec,
	    pkt_node->tval.tv_usec,
	    hash_node->const_info.laddr,
	    hash_node->const_info.lport,
	    hash_node->const_info.faddr,
	    hash_node->const_info.fport,
	    pkt_node->snd_ssthresh,
	    pkt_node->snd_cwnd,
	    pkt_node->t_flags2,
	    pkt_node->snd_wnd,
	    pkt_node->rcv_wnd,
	    pkt_node->snd_scale,
	    pkt_node->rcv_scale,
	    pkt_node->conn_state,
	    pkt_node->mss,
	    pkt_node->srtt,
	    pkt_node->sack_enabled,
	    pkt_node->t_flags,
	    pkt_node->rto,
	    pkt_node->snd_buf_hiwater,
	    pkt_node->snd_buf_cc,
	    pkt_node->rcv_buf_hiwater,
	    pkt_node->rcv_buf_cc,
	    pkt_node->sent_inflight_bytes,
	    pkt_node->t_segqlen,
	    pkt_node->flowid,
	    pkt_node->flowtype);

	alq_post_flags(siftr_alq, log_buf, 0);
}

static void
siftr_pkt_manager_thread(void *arg)
{
	STAILQ_HEAD(pkthead, pkt_node) tmp_pkt_queue =
	    STAILQ_HEAD_INITIALIZER(tmp_pkt_queue);
	struct pkt_node *pkt_node, *pkt_node_temp;
	uint8_t draining;

	draining = 2;

	mtx_lock(&siftr_pkt_mgr_mtx);

	/* draining == 0 when queue has been flushed and it's safe to exit. */
	while (draining) {
		/*
		 * Sleep until we are signalled to wake because thread has
		 * been told to exit or until 1 tick has passed.
		 */
		mtx_sleep(&wait_for_pkt, &siftr_pkt_mgr_mtx, PWAIT, "pktwait",
		    1);

		/* Gain exclusive access to the pkt_node queue. */
		mtx_lock(&siftr_pkt_queue_mtx);

		/*
		 * Move pkt_queue to tmp_pkt_queue, which leaves
		 * pkt_queue empty and ready to receive more pkt_nodes.
		 */
		STAILQ_CONCAT(&tmp_pkt_queue, &pkt_queue);

		/*
		 * We've finished making changes to the list. Unlock it
		 * so the pfil hooks can continue queuing pkt_nodes.
		 */
		mtx_unlock(&siftr_pkt_queue_mtx);

		/*
		 * We can't hold a mutex whilst calling siftr_process_pkt
		 * because ALQ might sleep waiting for buffer space.
		 */
		mtx_unlock(&siftr_pkt_mgr_mtx);

		/* Flush all pkt_nodes to the log file. */
		STAILQ_FOREACH_SAFE(pkt_node, &tmp_pkt_queue, nodes,
		    pkt_node_temp) {
			siftr_process_pkt(pkt_node);
			STAILQ_REMOVE_HEAD(&tmp_pkt_queue, nodes);
			free(pkt_node, M_SIFTR_PKTNODE);
		}

		KASSERT(STAILQ_EMPTY(&tmp_pkt_queue),
		    ("SIFTR tmp_pkt_queue not empty after flush"));

		mtx_lock(&siftr_pkt_mgr_mtx);

		/*
		 * If siftr_exit_pkt_manager_thread gets set during the window
		 * where we are draining the tmp_pkt_queue above, there might
		 * still be pkts in pkt_queue that need to be drained.
		 * Allow one further iteration to occur after
		 * siftr_exit_pkt_manager_thread has been set to ensure
		 * pkt_queue is completely empty before we kill the thread.
		 *
		 * siftr_exit_pkt_manager_thread is set only after the pfil
		 * hooks have been removed, so only 1 extra iteration
		 * is needed to drain the queue.
		 */
		if (siftr_exit_pkt_manager_thread)
			draining--;
	}

	mtx_unlock(&siftr_pkt_mgr_mtx);

	/* Calls wakeup on this thread's struct thread ptr. */
	kthread_exit();
}

/*
 * Check if a given mbuf has the SIFTR mbuf tag. If it does, log the fact that
 * it's a reinjected packet and return. If it doesn't, tag the mbuf and return.
 * Return value >0 means the caller should skip processing this mbuf.
 */
static inline int
siftr_chkreinject(struct mbuf *m, int dir, struct siftr_stats *ss)
{
	if (m_tag_locate(m, PACKET_COOKIE_SIFTR, PACKET_TAG_SIFTR, NULL)
	    != NULL) {
		if (dir == PFIL_IN)
			ss->nskip_in_dejavu++;
		else
			ss->nskip_out_dejavu++;

		return (1);
	} else {
		struct m_tag *tag = m_tag_alloc(PACKET_COOKIE_SIFTR,
		    PACKET_TAG_SIFTR, 0, M_NOWAIT);
		if (tag == NULL) {
			if (dir == PFIL_IN)
				ss->nskip_in_malloc++;
			else
				ss->nskip_out_malloc++;

			return (1);
		}

		m_tag_prepend(m, tag);
	}

	return (0);
}

/*
 * Look up an inpcb for a packet. Return the inpcb pointer if found, or NULL
 * otherwise.
 */
static inline struct inpcb *
siftr_findinpcb(int ipver, struct ip *ip, struct mbuf *m, uint16_t sport,
    uint16_t dport, int dir, struct siftr_stats *ss)
{
	struct inpcb *inp;

	/* We need the tcbinfo lock. */
	INP_INFO_WUNLOCK_ASSERT(&V_tcbinfo);

	if (dir == PFIL_IN)
		inp = (ipver == INP_IPV4 ?
		    in_pcblookup(&V_tcbinfo, ip->ip_src, sport, ip->ip_dst,
		    dport, INPLOOKUP_RLOCKPCB, m->m_pkthdr.rcvif)
		    :
#ifdef SIFTR_IPV6
		    in6_pcblookup(&V_tcbinfo,
		    &((struct ip6_hdr *)ip)->ip6_src, sport,
		    &((struct ip6_hdr *)ip)->ip6_dst, dport, INPLOOKUP_RLOCKPCB,
		    m->m_pkthdr.rcvif)
#else
		    NULL
#endif
		    );

	else
		inp = (ipver == INP_IPV4 ?
		    in_pcblookup(&V_tcbinfo, ip->ip_dst, dport, ip->ip_src,
		    sport, INPLOOKUP_RLOCKPCB, m->m_pkthdr.rcvif)
		    :
#ifdef SIFTR_IPV6
		    in6_pcblookup(&V_tcbinfo,
		    &((struct ip6_hdr *)ip)->ip6_dst, dport,
		    &((struct ip6_hdr *)ip)->ip6_src, sport, INPLOOKUP_RLOCKPCB,
		    m->m_pkthdr.rcvif)
#else
		    NULL
#endif
		    );

	/* If we can't find the inpcb, bail. */
	if (inp == NULL) {
		if (dir == PFIL_IN)
			ss->nskip_in_inpcb++;
		else
			ss->nskip_out_inpcb++;
	}

	return (inp);
}

static inline uint32_t
siftr_get_flowid(struct inpcb *inp, int ipver, uint32_t *phashtype)
{
	if (inp->inp_flowid == 0) {
#ifdef SIFTR_IPV6
		if (ipver == INP_IPV6) {
			return fib6_calc_packet_hash(&inp->in6p_laddr,
						     &inp->in6p_faddr,
						     inp->inp_lport,
						     inp->inp_fport,
						     IPPROTO_TCP,
						     phashtype);
		} else
#endif
		{
			return fib4_calc_packet_hash(inp->inp_laddr,
						     inp->inp_faddr,
						     inp->inp_lport,
						     inp->inp_fport,
						     IPPROTO_TCP,
						     phashtype);
		}
	} else {
		*phashtype = inp->inp_flowtype;
		return inp->inp_flowid;
	}
}

static inline void
siftr_siftdata(struct pkt_node *pn, struct inpcb *inp, struct tcpcb *tp,
    int ipver, int dir, int inp_locally_locked)
{
	pn->ipver = ipver;
	pn->lport = inp->inp_lport;
	pn->fport = inp->inp_fport;
	pn->laddr = inp->inp_inc.inc_ie.ie_dependladdr;
	pn->faddr = inp->inp_inc.inc_ie.ie_dependfaddr;
	pn->snd_cwnd = tp->snd_cwnd;
	pn->snd_wnd = tp->snd_wnd;
	pn->rcv_wnd = tp->rcv_wnd;
	pn->t_flags2 = tp->t_flags2;
	pn->snd_ssthresh = tp->snd_ssthresh;
	pn->snd_scale = tp->snd_scale;
	pn->rcv_scale = tp->rcv_scale;
	pn->conn_state = tp->t_state;
	pn->mss = tp->t_maxseg;
	pn->srtt = ((uint64_t)tp->t_srtt * tick) >> TCP_RTT_SHIFT;
	pn->sack_enabled = (tp->t_flags & TF_SACK_PERMIT) != 0;
	pn->t_flags = tp->t_flags;
	pn->rto = tp->t_rxtcur * tick;
	pn->snd_buf_hiwater = inp->inp_socket->so_snd.sb_hiwat;
	pn->snd_buf_cc = sbused(&inp->inp_socket->so_snd);
	pn->rcv_buf_hiwater = inp->inp_socket->so_rcv.sb_hiwat;
	pn->rcv_buf_cc = sbused(&inp->inp_socket->so_rcv);
	pn->sent_inflight_bytes = tp->snd_max - tp->snd_una;
	pn->t_segqlen = tp->t_segqlen;

	/* We've finished accessing the tcb so release the lock. */
	if (inp_locally_locked)
		INP_RUNLOCK(inp);

	pn->direction = (dir == PFIL_IN ? DIR_IN : DIR_OUT);

	/*
	 * Significantly more accurate than using getmicrotime(), but slower!
	 * Gives true microsecond resolution at the expense of a hit to
	 * maximum pps throughput processing when SIFTR is loaded and enabled.
	 */
	microtime(&pn->tval);
	TCP_PROBE1(siftr, pn);
}

/*
 * pfil hook that is called for each IPv4 packet making its way through the
 * stack in either direction.
 * The pfil subsystem holds a non-sleepable mutex somewhere when
 * calling our hook function, so we can't sleep at all.
 * It's very important to use the M_NOWAIT flag with all function calls
 * that support it so that they won't sleep, otherwise you get a panic.
 */
static pfil_return_t
siftr_chkpkt(struct mbuf **m, struct ifnet *ifp, int flags,
    void *ruleset __unused, struct inpcb *inp)
{
	struct pkt_node *pn;
	struct ip *ip;
	struct tcphdr *th;
	struct tcpcb *tp;
	struct siftr_stats *ss;
	unsigned int ip_hl;
	int inp_locally_locked, dir;
	uint32_t hash_id, hash_type;
	struct listhead *counter_list;
	struct flow_hash_node *hash_node;

	inp_locally_locked = 0;
	dir = PFIL_DIR(flags);
	ss = DPCPU_PTR(ss);

	/*
	 * m_pullup is not required here because ip_{input|output}
	 * already do the heavy lifting for us.
	 */

	ip = mtod(*m, struct ip *);

	/* Only continue processing if the packet is TCP. */
	if (ip->ip_p != IPPROTO_TCP)
		goto ret;

	/*
	 * Create a tcphdr struct starting at the correct offset
	 * in the IP packet. ip->ip_hl gives the ip header length
	 * in 4-byte words, so multiply it to get the size in bytes.
	 */
	ip_hl = (ip->ip_hl << 2);
	th = (struct tcphdr *)((caddr_t)ip + ip_hl);

	/*
	 * Only pkts selected by the tcp port filter
	 * can be inserted into the pkt_queue
	 */
	if ((siftr_port_filter != 0) &&
	    (siftr_port_filter != ntohs(th->th_sport)) &&
	    (siftr_port_filter != ntohs(th->th_dport))) {
		goto ret;
	}

	/*
	 * If a kernel subsystem reinjects packets into the stack, our pfil
	 * hook will be called multiple times for the same packet.
	 * Make sure we only process unique packets.
	 */
	if (siftr_chkreinject(*m, dir, ss))
		goto ret;

	if (dir == PFIL_IN)
		ss->n_in++;
	else
		ss->n_out++;

	/*
	 * If the pfil hooks don't provide a pointer to the
	 * inpcb, we need to find it ourselves and lock it.
	 */
	if (!inp) {
		/* Find the corresponding inpcb for this pkt. */
		inp = siftr_findinpcb(INP_IPV4, ip, *m, th->th_sport,
		    th->th_dport, dir, ss);

		if (inp == NULL)
			goto ret;
		else
			inp_locally_locked = 1;
	}

	INP_LOCK_ASSERT(inp);

	/* Find the TCP control block that corresponds with this packet */
	tp = intotcpcb(inp);

	/*
	 * If we can't find the TCP control block (happens occasionaly for a
	 * packet sent during the shutdown phase of a TCP connection), or the
	 * TCP control block has not initialized (happens during TCPS_SYN_SENT),
	 * bail.
	 */
	if (tp == NULL || tp->t_state < TCPS_ESTABLISHED) {
		if (dir == PFIL_IN)
			ss->nskip_in_tcpcb++;
		else
			ss->nskip_out_tcpcb++;

		goto inp_unlock;
	}

	hash_id = siftr_get_flowid(inp, INP_IPV4, &hash_type);
	counter_list = counter_hash + (hash_id & siftr_hashmask);
	hash_node = siftr_find_flow(counter_list, hash_id);

	/* If this flow hasn't been seen before, we create a new entry. */
	if (hash_node == NULL) {
		struct flow_info info;

		inet_ntoa_r(inp->inp_laddr, info.laddr);
		inet_ntoa_r(inp->inp_faddr, info.faddr);
		info.lport = ntohs(inp->inp_lport);
		info.fport = ntohs(inp->inp_fport);
		info.key = hash_id;

		hash_node = siftr_new_hash_node(info, dir, ss);
	}

	if (hash_node == NULL) {
		goto inp_unlock;
	}

	pn = malloc(sizeof(struct pkt_node), M_SIFTR_PKTNODE, M_NOWAIT|M_ZERO);

	if (pn == NULL) {
		if (dir == PFIL_IN)
			ss->nskip_in_malloc++;
		else
			ss->nskip_out_malloc++;

		goto inp_unlock;
	}

	pn->flowid = hash_id;
	pn->flowtype = hash_type;

	siftr_siftdata(pn, inp, tp, INP_IPV4, dir, inp_locally_locked);

	mtx_lock(&siftr_pkt_queue_mtx);
	STAILQ_INSERT_TAIL(&pkt_queue, pn, nodes);
	mtx_unlock(&siftr_pkt_queue_mtx);
	goto ret;

inp_unlock:
	if (inp_locally_locked)
		INP_RUNLOCK(inp);

ret:
	return (PFIL_PASS);
}

#ifdef SIFTR_IPV6
static pfil_return_t
siftr_chkpkt6(struct mbuf **m, struct ifnet *ifp, int flags,
    void *ruleset __unused, struct inpcb *inp)
{
	struct pkt_node *pn;
	struct ip6_hdr *ip6;
	struct tcphdr *th;
	struct tcpcb *tp;
	struct siftr_stats *ss;
	unsigned int ip6_hl;
	int inp_locally_locked, dir;
	uint32_t hash_id, hash_type;
	struct listhead *counter_list;
	struct flow_hash_node *hash_node;

	inp_locally_locked = 0;
	dir = PFIL_DIR(flags);
	ss = DPCPU_PTR(ss);

	/*
	 * m_pullup is not required here because ip6_{input|output}
	 * already do the heavy lifting for us.
	 */

	ip6 = mtod(*m, struct ip6_hdr *);

	/*
	 * Only continue processing if the packet is TCP
	 * XXX: We should follow the next header fields
	 * as shown on Pg 6 RFC 2460, but right now we'll
	 * only check pkts that have no extension headers.
	 */
	if (ip6->ip6_nxt != IPPROTO_TCP)
		goto ret6;

	/*
	 * Create a tcphdr struct starting at the correct offset
	 * in the ipv6 packet.
	 */
	ip6_hl = sizeof(struct ip6_hdr);
	th = (struct tcphdr *)((caddr_t)ip6 + ip6_hl);

	/*
	 * Only pkts selected by the tcp port filter
	 * can be inserted into the pkt_queue
	 */
	if ((siftr_port_filter != 0) &&
	    (siftr_port_filter != ntohs(th->th_sport)) &&
	    (siftr_port_filter != ntohs(th->th_dport))) {
		goto ret6;
	}

	/*
	 * If a kernel subsystem reinjects packets into the stack, our pfil
	 * hook will be called multiple times for the same packet.
	 * Make sure we only process unique packets.
	 */
	if (siftr_chkreinject(*m, dir, ss))
		goto ret6;

	if (dir == PFIL_IN)
		ss->n_in++;
	else
		ss->n_out++;

	/*
	 * For inbound packets, the pfil hooks don't provide a pointer to the
	 * inpcb, so we need to find it ourselves and lock it.
	 */
	if (!inp) {
		/* Find the corresponding inpcb for this pkt. */
		inp = siftr_findinpcb(INP_IPV6, (struct ip *)ip6, *m,
		    th->th_sport, th->th_dport, dir, ss);

		if (inp == NULL)
			goto ret6;
		else
			inp_locally_locked = 1;
	}

	/* Find the TCP control block that corresponds with this packet. */
	tp = intotcpcb(inp);

	/*
	 * If we can't find the TCP control block (happens occasionaly for a
	 * packet sent during the shutdown phase of a TCP connection), or the
	 * TCP control block has not initialized (happens during TCPS_SYN_SENT),
	 * bail.
	 */
	if (tp == NULL || tp->t_state < TCPS_ESTABLISHED) {
		if (dir == PFIL_IN)
			ss->nskip_in_tcpcb++;
		else
			ss->nskip_out_tcpcb++;

		goto inp_unlock6;
	}

	hash_id = siftr_get_flowid(inp, INP_IPV6, &hash_type);
	counter_list = counter_hash + (hash_id & siftr_hashmask);
	hash_node = siftr_find_flow(counter_list, hash_id);

	/* If this flow hasn't been seen before, we create a new entry. */
	if (!hash_node) {
		struct flow_info info;

		ip6_sprintf(info.laddr, &inp->in6p_laddr);
		ip6_sprintf(info.faddr, &inp->in6p_faddr);
		info.lport = ntohs(inp->inp_lport);
		info.fport = ntohs(inp->inp_fport);
		info.key = hash_id;

		hash_node = siftr_new_hash_node(info, dir, ss);
	}

	if (!hash_node) {
		goto inp_unlock6;
	}

	pn = malloc(sizeof(struct pkt_node), M_SIFTR_PKTNODE, M_NOWAIT|M_ZERO);

	if (pn == NULL) {
		if (dir == PFIL_IN)
			ss->nskip_in_malloc++;
		else
			ss->nskip_out_malloc++;

		goto inp_unlock6;
	}

	pn->flowid = hash_id;
	pn->flowtype = hash_type;

	siftr_siftdata(pn, inp, tp, INP_IPV6, dir, inp_locally_locked);

	mtx_lock(&siftr_pkt_queue_mtx);
	STAILQ_INSERT_TAIL(&pkt_queue, pn, nodes);
	mtx_unlock(&siftr_pkt_queue_mtx);
	goto ret6;

inp_unlock6:
	if (inp_locally_locked)
		INP_RUNLOCK(inp);

ret6:
	return (PFIL_PASS);
}
#endif /* #ifdef SIFTR_IPV6 */

VNET_DEFINE_STATIC(pfil_hook_t, siftr_inet_hook);
#define	V_siftr_inet_hook	VNET(siftr_inet_hook)
#ifdef SIFTR_IPV6
VNET_DEFINE_STATIC(pfil_hook_t, siftr_inet6_hook);
#define	V_siftr_inet6_hook	VNET(siftr_inet6_hook)
#endif
static int
siftr_pfil(int action)
{
	struct pfil_hook_args pha = {
		.pa_version = PFIL_VERSION,
		.pa_flags = PFIL_IN | PFIL_OUT,
		.pa_modname = "siftr",
		.pa_rulname = "default",
	};
	struct pfil_link_args pla = {
		.pa_version = PFIL_VERSION,
		.pa_flags = PFIL_IN | PFIL_OUT | PFIL_HEADPTR | PFIL_HOOKPTR,
	};

	VNET_ITERATOR_DECL(vnet_iter);

	VNET_LIST_RLOCK();
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET(vnet_iter);

		if (action == HOOK) {
			pha.pa_mbuf_chk = siftr_chkpkt;
			pha.pa_type = PFIL_TYPE_IP4;
			V_siftr_inet_hook = pfil_add_hook(&pha);
			pla.pa_hook = V_siftr_inet_hook;
			pla.pa_head = V_inet_pfil_head;
			(void)pfil_link(&pla);
#ifdef SIFTR_IPV6
			pha.pa_mbuf_chk = siftr_chkpkt6;
			pha.pa_type = PFIL_TYPE_IP6;
			V_siftr_inet6_hook = pfil_add_hook(&pha);
			pla.pa_hook = V_siftr_inet6_hook;
			pla.pa_head = V_inet6_pfil_head;
			(void)pfil_link(&pla);
#endif
		} else if (action == UNHOOK) {
			pfil_remove_hook(V_siftr_inet_hook);
#ifdef SIFTR_IPV6
			pfil_remove_hook(V_siftr_inet6_hook);
#endif
		}
		CURVNET_RESTORE();
	}
	VNET_LIST_RUNLOCK();

	return (0);
}

static int
siftr_sysctl_logfile_name_handler(SYSCTL_HANDLER_ARGS)
{
	struct alq *new_alq;
	int error;

	error = sysctl_handle_string(oidp, arg1, arg2, req);

	/* Check for error or same filename */
	if (error != 0 || req->newptr == NULL ||
	    strncmp(siftr_logfile, arg1, arg2) == 0)
		goto done;

	/* file name changed */
	error = alq_open(&new_alq, arg1, curthread->td_ucred,
	    SIFTR_LOG_FILE_MODE, SIFTR_ALQ_BUFLEN, 0);
	if (error != 0)
		goto done;

	/*
	 * If disabled, siftr_alq == NULL so we simply close
	 * the alq as we've proved it can be opened.
	 * If enabled, close the existing alq and switch the old
	 * for the new.
	 */
	if (siftr_alq == NULL) {
		alq_close(new_alq);
	} else {
		alq_close(siftr_alq);
		siftr_alq = new_alq;
	}

	/* Update filename upon success */
	strlcpy(siftr_logfile, arg1, arg2);
done:
	return (error);
}

static int
siftr_manage_ops(uint8_t action)
{
	struct siftr_stats totalss;
	struct timeval tval;
	struct flow_hash_node *counter, *tmp_counter;
	struct sbuf *s;
	int i, error;
	uint32_t bytes_to_write, total_skipped_pkts;

	error = 0;
	total_skipped_pkts = 0;

	/* Init an autosizing sbuf that initially holds 200 chars. */
	if ((s = sbuf_new(NULL, NULL, 200, SBUF_AUTOEXTEND)) == NULL)
		return (-1);

	if (action == SIFTR_ENABLE && siftr_pkt_manager_thr == NULL) {
		/*
		 * Create our alq
		 * XXX: We should abort if alq_open fails!
		 */
		alq_open(&siftr_alq, siftr_logfile, curthread->td_ucred,
		    SIFTR_LOG_FILE_MODE, SIFTR_ALQ_BUFLEN, 0);

		STAILQ_INIT(&pkt_queue);

		DPCPU_ZERO(ss);

		siftr_exit_pkt_manager_thread = 0;

		kthread_add(&siftr_pkt_manager_thread, NULL, NULL,
		    &siftr_pkt_manager_thr, RFNOWAIT, 0,
		    "siftr_pkt_manager_thr");

		siftr_pfil(HOOK);

		microtime(&tval);

		sbuf_printf(s,
		    "enable_time_secs=%jd\tenable_time_usecs=%06ld\t"
		    "siftrver=%s\tsysname=%s\tsysver=%u\tipmode=%u\n",
		    (intmax_t)tval.tv_sec, tval.tv_usec, MODVERSION_STR,
		    SYS_NAME, __FreeBSD_version, SIFTR_IPMODE);

		sbuf_finish(s);
		alq_writen(siftr_alq, sbuf_data(s), sbuf_len(s), ALQ_WAITOK);

	} else if (action == SIFTR_DISABLE && siftr_pkt_manager_thr != NULL) {
		/*
		 * Remove the pfil hook functions. All threads currently in
		 * the hook functions are allowed to exit before siftr_pfil()
		 * returns.
		 */
		siftr_pfil(UNHOOK);

		/* This will block until the pkt manager thread unlocks it. */
		mtx_lock(&siftr_pkt_mgr_mtx);

		/* Tell the pkt manager thread that it should exit now. */
		siftr_exit_pkt_manager_thread = 1;

		/*
		 * Wake the pkt_manager thread so it realises that
		 * siftr_exit_pkt_manager_thread == 1 and exits gracefully.
		 * The wakeup won't be delivered until we unlock
		 * siftr_pkt_mgr_mtx so this isn't racy.
		 */
		wakeup(&wait_for_pkt);

		/* Wait for the pkt_manager thread to exit. */
		mtx_sleep(siftr_pkt_manager_thr, &siftr_pkt_mgr_mtx, PWAIT,
		    "thrwait", 0);

		siftr_pkt_manager_thr = NULL;
		mtx_unlock(&siftr_pkt_mgr_mtx);

		totalss.n_in = DPCPU_VARSUM(ss, n_in);
		totalss.n_out = DPCPU_VARSUM(ss, n_out);
		totalss.nskip_in_malloc = DPCPU_VARSUM(ss, nskip_in_malloc);
		totalss.nskip_out_malloc = DPCPU_VARSUM(ss, nskip_out_malloc);
		totalss.nskip_in_tcpcb = DPCPU_VARSUM(ss, nskip_in_tcpcb);
		totalss.nskip_out_tcpcb = DPCPU_VARSUM(ss, nskip_out_tcpcb);
		totalss.nskip_in_inpcb = DPCPU_VARSUM(ss, nskip_in_inpcb);
		totalss.nskip_out_inpcb = DPCPU_VARSUM(ss, nskip_out_inpcb);

		total_skipped_pkts = totalss.nskip_in_malloc +
		    totalss.nskip_out_malloc + totalss.nskip_in_tcpcb +
		    totalss.nskip_out_tcpcb + totalss.nskip_in_inpcb +
		    totalss.nskip_out_inpcb;

		microtime(&tval);

		sbuf_printf(s,
		    "disable_time_secs=%jd\tdisable_time_usecs=%06ld\t"
		    "num_inbound_tcp_pkts=%ju\tnum_outbound_tcp_pkts=%ju\t"
		    "total_tcp_pkts=%ju\tnum_inbound_skipped_pkts_malloc=%u\t"
		    "num_outbound_skipped_pkts_malloc=%u\t"
		    "num_inbound_skipped_pkts_tcpcb=%u\t"
		    "num_outbound_skipped_pkts_tcpcb=%u\t"
		    "num_inbound_skipped_pkts_inpcb=%u\t"
		    "num_outbound_skipped_pkts_inpcb=%u\t"
		    "total_skipped_tcp_pkts=%u\tflow_list=",
		    (intmax_t)tval.tv_sec,
		    tval.tv_usec,
		    (uintmax_t)totalss.n_in,
		    (uintmax_t)totalss.n_out,
		    (uintmax_t)(totalss.n_in + totalss.n_out),
		    totalss.nskip_in_malloc,
		    totalss.nskip_out_malloc,
		    totalss.nskip_in_tcpcb,
		    totalss.nskip_out_tcpcb,
		    totalss.nskip_in_inpcb,
		    totalss.nskip_out_inpcb,
		    total_skipped_pkts);

		/*
		 * Iterate over the flow hash, printing a summary of each
		 * flow seen and freeing any malloc'd memory.
		 * The hash consists of an array of LISTs (man 3 queue).
		 */
		for (i = 0; i <= siftr_hashmask; i++) {
			LIST_FOREACH_SAFE(counter, counter_hash + i, nodes,
			    tmp_counter) {
				sbuf_printf(s, "%s;%hu-%s;%hu,",
					    counter->const_info.laddr,
					    counter->const_info.lport,
					    counter->const_info.faddr,
					    counter->const_info.fport);

				free(counter, M_SIFTR_HASHNODE);
			}

			LIST_INIT(counter_hash + i);
		}

		sbuf_printf(s, "\n");
		sbuf_finish(s);

		i = 0;
		do {
			bytes_to_write = min(SIFTR_ALQ_BUFLEN, sbuf_len(s)-i);
			alq_writen(siftr_alq, sbuf_data(s)+i, bytes_to_write, ALQ_WAITOK);
			i += bytes_to_write;
		} while (i < sbuf_len(s));

		alq_close(siftr_alq);
		siftr_alq = NULL;
	} else
		error = EINVAL;

	sbuf_delete(s);

	/*
	 * XXX: Should be using ret to check if any functions fail
	 * and set error appropriately
	 */

	return (error);
}

static int
siftr_sysctl_enabled_handler(SYSCTL_HANDLER_ARGS)
{
	int error;
	uint32_t new;

	new = siftr_enabled;
	error = sysctl_handle_int(oidp, &new, 0, req);
	if (error == 0 && req->newptr != NULL) {
		if (new > 1)
			return (EINVAL);
		else if (new != siftr_enabled) {
			if ((error = siftr_manage_ops(new)) == 0) {
				siftr_enabled = new;
			} else {
				siftr_manage_ops(SIFTR_DISABLE);
			}
		}
	}

	return (error);
}

static void
siftr_shutdown_handler(void *arg)
{
	if (siftr_enabled == 1) {
		siftr_manage_ops(SIFTR_DISABLE);
	}
}

/*
 * Module is being unloaded or machine is shutting down. Take care of cleanup.
 */
static int
deinit_siftr(void)
{
	/* Cleanup. */
	siftr_manage_ops(SIFTR_DISABLE);
	hashdestroy(counter_hash, M_SIFTR, siftr_hashmask);
	mtx_destroy(&siftr_pkt_queue_mtx);
	mtx_destroy(&siftr_pkt_mgr_mtx);

	return (0);
}

/*
 * Module has just been loaded into the kernel.
 */
static int
init_siftr(void)
{
	EVENTHANDLER_REGISTER(shutdown_pre_sync, siftr_shutdown_handler, NULL,
	    SHUTDOWN_PRI_FIRST);

	/* Initialise our flow counter hash table. */
	counter_hash = hashinit(SIFTR_EXPECTED_MAX_TCP_FLOWS, M_SIFTR,
	    &siftr_hashmask);

	mtx_init(&siftr_pkt_queue_mtx, "siftr_pkt_queue_mtx", NULL, MTX_DEF);
	mtx_init(&siftr_pkt_mgr_mtx, "siftr_pkt_mgr_mtx", NULL, MTX_DEF);

	/* Print message to the user's current terminal. */
	uprintf("\nStatistical Information For TCP Research (SIFTR) %s\n"
	    "          http://caia.swin.edu.au/urp/newtcp\n\n",
	    MODVERSION_STR);

	return (0);
}

/*
 * This is the function that is called to load and unload the module.
 * When the module is loaded, this function is called once with
 * "what" == MOD_LOAD
 * When the module is unloaded, this function is called twice with
 * "what" = MOD_QUIESCE first, followed by "what" = MOD_UNLOAD second
 * When the system is shut down e.g. CTRL-ALT-DEL or using the shutdown command,
 * this function is called once with "what" = MOD_SHUTDOWN
 * When the system is shut down, the handler isn't called until the very end
 * of the shutdown sequence i.e. after the disks have been synced.
 */
static int
siftr_load_handler(module_t mod, int what, void *arg)
{
	int ret;

	switch (what) {
	case MOD_LOAD:
		ret = init_siftr();
		break;

	case MOD_QUIESCE:
	case MOD_SHUTDOWN:
		ret = deinit_siftr();
		break;

	case MOD_UNLOAD:
		ret = 0;
		break;

	default:
		ret = EINVAL;
		break;
	}

	return (ret);
}

static moduledata_t siftr_mod = {
	.name = "siftr",
	.evhand = siftr_load_handler,
};

/*
 * Param 1: name of the kernel module
 * Param 2: moduledata_t struct containing info about the kernel module
 *          and the execution entry point for the module
 * Param 3: From sysinit_sub_id enumeration in /usr/include/sys/kernel.h
 *          Defines the module initialisation order
 * Param 4: From sysinit_elem_order enumeration in /usr/include/sys/kernel.h
 *          Defines the initialisation order of this kld relative to others
 *          within the same subsystem as defined by param 3
 */
DECLARE_MODULE(siftr, siftr_mod, SI_SUB_LAST, SI_ORDER_ANY);
MODULE_DEPEND(siftr, alq, 1, 1, 1);
MODULE_VERSION(siftr, MODVERSION);
