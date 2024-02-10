/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_kern_tls.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/arb.h>
#include <sys/callout.h>
#include <sys/eventhandler.h>
#ifdef TCP_HHOOK
#include <sys/hhook.h>
#endif
#include <sys/kernel.h>
#ifdef TCP_HHOOK
#include <sys/khelp.h>
#endif
#ifdef KERN_TLS
#include <sys/ktls.h>
#endif
#include <sys/qmath.h>
#include <sys/stats.h>
#include <sys/sysctl.h>
#include <sys/jail.h>
#include <sys/malloc.h>
#include <sys/refcount.h>
#include <sys/mbuf.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/sdt.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/random.h>

#include <vm/uma.h>

#include <net/route.h>
#include <net/route/nhop.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_private.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_fib.h>
#include <netinet/in_kdtrace.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#ifdef INET6
#include <netinet/icmp6.h>
#include <netinet/ip6.h>
#include <netinet6/in6_fib.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#include <netinet6/nd6.h>
#endif

#include <netinet/tcp.h>
#ifdef INVARIANTS
#define TCPSTATES
#endif
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_ecn.h>
#include <netinet/tcp_log_buf.h>
#include <netinet/tcp_syncache.h>
#include <netinet/tcp_hpts.h>
#include <netinet/tcp_lro.h>
#include <netinet/cc/cc.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_fastopen.h>
#include <netinet/tcp_accounting.h>
#ifdef TCPPCAP
#include <netinet/tcp_pcap.h>
#endif
#ifdef TCP_OFFLOAD
#include <netinet/tcp_offload.h>
#endif
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#ifdef INET6
#include <netinet6/tcp6_var.h>
#endif

#include <netipsec/ipsec_support.h>

#include <machine/in_cksum.h>
#include <crypto/siphash/siphash.h>

#include <security/mac/mac_framework.h>

#ifdef INET6
static ip6proto_ctlinput_t tcp6_ctlinput;
static udp_tun_icmp_t tcp6_ctlinput_viaudp;
#endif

VNET_DEFINE(int, tcp_mssdflt) = TCP_MSS;
#ifdef INET6
VNET_DEFINE(int, tcp_v6mssdflt) = TCP6_MSS;
#endif

#ifdef TCP_SAD_DETECTION
/*  Sack attack detection thresholds and such */
SYSCTL_NODE(_net_inet_tcp, OID_AUTO, sack_attack,
    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Sack Attack detection thresholds");
int32_t tcp_force_detection = 0;
SYSCTL_INT(_net_inet_tcp_sack_attack, OID_AUTO, force_detection,
    CTLFLAG_RW,
    &tcp_force_detection, 0,
    "Do we force detection even if the INP has it off?");
int32_t tcp_sad_limit = 10000;
SYSCTL_INT(_net_inet_tcp_sack_attack, OID_AUTO, limit,
    CTLFLAG_RW,
    &tcp_sad_limit, 10000,
    "If SaD is enabled, what is the limit to sendmap entries (0 = unlimited)?");
int32_t tcp_sack_to_ack_thresh = 700;	/* 70 % */
SYSCTL_INT(_net_inet_tcp_sack_attack, OID_AUTO, sack_to_ack_thresh,
    CTLFLAG_RW,
    &tcp_sack_to_ack_thresh, 700,
    "Percentage of sacks to acks we must see above (10.1 percent is 101)?");
int32_t tcp_sack_to_move_thresh = 600;	/* 60 % */
SYSCTL_INT(_net_inet_tcp_sack_attack, OID_AUTO, move_thresh,
    CTLFLAG_RW,
    &tcp_sack_to_move_thresh, 600,
    "Percentage of sack moves we must see above (10.1 percent is 101)");
int32_t tcp_restoral_thresh = 450;	/* 45 % (sack:2:ack -25%) (mv:ratio -15%) **/
SYSCTL_INT(_net_inet_tcp_sack_attack, OID_AUTO, restore_thresh,
    CTLFLAG_RW,
    &tcp_restoral_thresh, 450,
    "Percentage of sack to ack percentage we must see below to restore(10.1 percent is 101)");
int32_t tcp_sad_decay_val = 800;
SYSCTL_INT(_net_inet_tcp_sack_attack, OID_AUTO, decay_per,
    CTLFLAG_RW,
    &tcp_sad_decay_val, 800,
    "The decay percentage (10.1 percent equals 101 )");
int32_t tcp_map_minimum = 500;
SYSCTL_INT(_net_inet_tcp_sack_attack, OID_AUTO, nummaps,
    CTLFLAG_RW,
    &tcp_map_minimum, 500,
    "Number of Map enteries before we start detection");
int32_t tcp_sad_pacing_interval = 2000;
SYSCTL_INT(_net_inet_tcp_sack_attack, OID_AUTO, sad_pacing_int,
    CTLFLAG_RW,
    &tcp_sad_pacing_interval, 2000,
    "What is the minimum pacing interval for a classified attacker?");

int32_t tcp_sad_low_pps = 100;
SYSCTL_INT(_net_inet_tcp_sack_attack, OID_AUTO, sad_low_pps,
    CTLFLAG_RW,
    &tcp_sad_low_pps, 100,
    "What is the input pps that below which we do not decay?");
#endif
uint32_t tcp_ack_war_time_window = 1000;
SYSCTL_UINT(_net_inet_tcp, OID_AUTO, ack_war_timewindow,
    CTLFLAG_RW,
    &tcp_ack_war_time_window, 1000,
   "If the tcp_stack does ack-war prevention how many milliseconds are in its time window?");
uint32_t tcp_ack_war_cnt = 5;
SYSCTL_UINT(_net_inet_tcp, OID_AUTO, ack_war_cnt,
    CTLFLAG_RW,
    &tcp_ack_war_cnt, 5,
   "If the tcp_stack does ack-war prevention how many acks can be sent in its time window?");

struct rwlock tcp_function_lock;

static int
sysctl_net_inet_tcp_mss_check(SYSCTL_HANDLER_ARGS)
{
	int error, new;

	new = V_tcp_mssdflt;
	error = sysctl_handle_int(oidp, &new, 0, req);
	if (error == 0 && req->newptr) {
		if (new < TCP_MINMSS)
			error = EINVAL;
		else
			V_tcp_mssdflt = new;
	}
	return (error);
}

SYSCTL_PROC(_net_inet_tcp, TCPCTL_MSSDFLT, mssdflt,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &VNET_NAME(tcp_mssdflt), 0, &sysctl_net_inet_tcp_mss_check, "I",
    "Default TCP Maximum Segment Size");

#ifdef INET6
static int
sysctl_net_inet_tcp_mss_v6_check(SYSCTL_HANDLER_ARGS)
{
	int error, new;

	new = V_tcp_v6mssdflt;
	error = sysctl_handle_int(oidp, &new, 0, req);
	if (error == 0 && req->newptr) {
		if (new < TCP_MINMSS)
			error = EINVAL;
		else
			V_tcp_v6mssdflt = new;
	}
	return (error);
}

SYSCTL_PROC(_net_inet_tcp, TCPCTL_V6MSSDFLT, v6mssdflt,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &VNET_NAME(tcp_v6mssdflt), 0, &sysctl_net_inet_tcp_mss_v6_check, "I",
   "Default TCP Maximum Segment Size for IPv6");
#endif /* INET6 */

/*
 * Minimum MSS we accept and use. This prevents DoS attacks where
 * we are forced to a ridiculous low MSS like 20 and send hundreds
 * of packets instead of one. The effect scales with the available
 * bandwidth and quickly saturates the CPU and network interface
 * with packet generation and sending. Set to zero to disable MINMSS
 * checking. This setting prevents us from sending too small packets.
 */
VNET_DEFINE(int, tcp_minmss) = TCP_MINMSS;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, minmss, CTLFLAG_VNET | CTLFLAG_RW,
     &VNET_NAME(tcp_minmss), 0,
    "Minimum TCP Maximum Segment Size");

VNET_DEFINE(int, tcp_do_rfc1323) = 1;
SYSCTL_INT(_net_inet_tcp, TCPCTL_DO_RFC1323, rfc1323, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(tcp_do_rfc1323), 0,
    "Enable rfc1323 (high performance TCP) extensions");

/*
 * As of June 2021, several TCP stacks violate RFC 7323 from September 2014.
 * Some stacks negotiate TS, but never send them after connection setup. Some
 * stacks negotiate TS, but don't send them when sending keep-alive segments.
 * These include modern widely deployed TCP stacks.
 * Therefore tolerating violations for now...
 */
VNET_DEFINE(int, tcp_tolerate_missing_ts) = 1;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, tolerate_missing_ts, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(tcp_tolerate_missing_ts), 0,
    "Tolerate missing TCP timestamps");

VNET_DEFINE(int, tcp_ts_offset_per_conn) = 1;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, ts_offset_per_conn, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(tcp_ts_offset_per_conn), 0,
    "Initialize TCP timestamps per connection instead of per host pair");

/* How many connections are pacing */
static volatile uint32_t number_of_tcp_connections_pacing = 0;
static uint32_t shadow_num_connections = 0;
static counter_u64_t tcp_pacing_failures;

static int tcp_pacing_limit = 10000;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, pacing_limit, CTLFLAG_RW,
    &tcp_pacing_limit, 1000,
    "If the TCP stack does pacing, is there a limit (-1 = no, 0 = no pacing N = number of connections)");

SYSCTL_UINT(_net_inet_tcp, OID_AUTO, pacing_count, CTLFLAG_RD,
    &shadow_num_connections, 0, "Number of TCP connections being paced");

SYSCTL_COUNTER_U64(_net_inet_tcp, OID_AUTO, pacing_failures, CTLFLAG_RD,
    &tcp_pacing_failures, "Number of times we failed to enable pacing to avoid exceeding the limit");

static int	tcp_log_debug = 0;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, log_debug, CTLFLAG_RW,
    &tcp_log_debug, 0, "Log errors caused by incoming TCP segments");

/*
 * Target size of TCP PCB hash tables. Must be a power of two.
 *
 * Note that this can be overridden by the kernel environment
 * variable net.inet.tcp.tcbhashsize
 */
#ifndef TCBHASHSIZE
#define TCBHASHSIZE	0
#endif
static int	tcp_tcbhashsize = TCBHASHSIZE;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, tcbhashsize, CTLFLAG_RDTUN,
    &tcp_tcbhashsize, 0, "Size of TCP control-block hashtable");

static int	do_tcpdrain = 1;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, do_tcpdrain, CTLFLAG_RW, &do_tcpdrain, 0,
    "Enable tcp_drain routine for extra help when low on mbufs");

SYSCTL_UINT(_net_inet_tcp, OID_AUTO, pcbcount, CTLFLAG_VNET | CTLFLAG_RD,
    &VNET_NAME(tcbinfo.ipi_count), 0, "Number of active PCBs");

VNET_DEFINE_STATIC(int, icmp_may_rst) = 1;
#define	V_icmp_may_rst			VNET(icmp_may_rst)
SYSCTL_INT(_net_inet_tcp, OID_AUTO, icmp_may_rst, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(icmp_may_rst), 0,
    "Certain ICMP unreachable messages may abort connections in SYN_SENT");

VNET_DEFINE_STATIC(int, tcp_isn_reseed_interval) = 0;
#define	V_tcp_isn_reseed_interval	VNET(tcp_isn_reseed_interval)
SYSCTL_INT(_net_inet_tcp, OID_AUTO, isn_reseed_interval, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(tcp_isn_reseed_interval), 0,
    "Seconds between reseeding of ISN secret");

static int	tcp_soreceive_stream;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, soreceive_stream, CTLFLAG_RDTUN,
    &tcp_soreceive_stream, 0, "Using soreceive_stream for TCP sockets");

VNET_DEFINE(uma_zone_t, sack_hole_zone);
#define	V_sack_hole_zone		VNET(sack_hole_zone)
VNET_DEFINE(uint32_t, tcp_map_entries_limit) = 0;	/* unlimited */
static int
sysctl_net_inet_tcp_map_limit_check(SYSCTL_HANDLER_ARGS)
{
	int error;
	uint32_t new;

	new = V_tcp_map_entries_limit;
	error = sysctl_handle_int(oidp, &new, 0, req);
	if (error == 0 && req->newptr) {
		/* only allow "0" and value > minimum */
		if (new > 0 && new < TCP_MIN_MAP_ENTRIES_LIMIT)
			error = EINVAL;
		else
			V_tcp_map_entries_limit = new;
	}
	return (error);
}
SYSCTL_PROC(_net_inet_tcp, OID_AUTO, map_limit,
    CTLFLAG_VNET | CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    &VNET_NAME(tcp_map_entries_limit), 0,
    &sysctl_net_inet_tcp_map_limit_check, "IU",
    "Total sendmap entries limit");

VNET_DEFINE(uint32_t, tcp_map_split_limit) = 0;	/* unlimited */
SYSCTL_UINT(_net_inet_tcp, OID_AUTO, split_limit, CTLFLAG_VNET | CTLFLAG_RW,
     &VNET_NAME(tcp_map_split_limit), 0,
    "Total sendmap split entries limit");

#ifdef TCP_HHOOK
VNET_DEFINE(struct hhook_head *, tcp_hhh[HHOOK_TCP_LAST+1]);
#endif

#define TS_OFFSET_SECRET_LENGTH SIPHASH_KEY_LENGTH
VNET_DEFINE_STATIC(u_char, ts_offset_secret[TS_OFFSET_SECRET_LENGTH]);
#define	V_ts_offset_secret	VNET(ts_offset_secret)

static int	tcp_default_fb_init(struct tcpcb *tp, void **ptr);
static void	tcp_default_fb_fini(struct tcpcb *tp, int tcb_is_purged);
static int	tcp_default_handoff_ok(struct tcpcb *tp);
static struct inpcb *tcp_notify(struct inpcb *, int);
static struct inpcb *tcp_mtudisc_notify(struct inpcb *, int);
static struct inpcb *tcp_mtudisc(struct inpcb *, int);
static struct inpcb *tcp_drop_syn_sent(struct inpcb *, int);
static char *	tcp_log_addr(struct in_conninfo *inc, struct tcphdr *th,
		    const void *ip4hdr, const void *ip6hdr);
static void	tcp_default_switch_failed(struct tcpcb *tp);
static ipproto_ctlinput_t	tcp_ctlinput;
static udp_tun_icmp_t		tcp_ctlinput_viaudp;

static struct tcp_function_block tcp_def_funcblk = {
	.tfb_tcp_block_name = "freebsd",
	.tfb_tcp_output = tcp_default_output,
	.tfb_tcp_do_segment = tcp_do_segment,
	.tfb_tcp_ctloutput = tcp_default_ctloutput,
	.tfb_tcp_handoff_ok = tcp_default_handoff_ok,
	.tfb_tcp_fb_init = tcp_default_fb_init,
	.tfb_tcp_fb_fini = tcp_default_fb_fini,
	.tfb_switch_failed = tcp_default_switch_failed,
};

static int tcp_fb_cnt = 0;
struct tcp_funchead t_functions;
VNET_DEFINE_STATIC(struct tcp_function_block *, tcp_func_set_ptr) = &tcp_def_funcblk;
#define	V_tcp_func_set_ptr VNET(tcp_func_set_ptr)

void
tcp_record_dsack(struct tcpcb *tp, tcp_seq start, tcp_seq end, int tlp)
{
	TCPSTAT_INC(tcps_dsack_count);
	tp->t_dsack_pack++;
	if (tlp == 0) {
		if (SEQ_GT(end, start)) {
			tp->t_dsack_bytes += (end - start);
			TCPSTAT_ADD(tcps_dsack_bytes, (end - start));
		} else {
			tp->t_dsack_tlp_bytes += (start - end);
			TCPSTAT_ADD(tcps_dsack_bytes, (start - end));
		}
	} else {
		if (SEQ_GT(end, start)) {
			tp->t_dsack_bytes += (end - start);
			TCPSTAT_ADD(tcps_dsack_tlp_bytes, (end - start));
		} else {
			tp->t_dsack_tlp_bytes += (start - end);
			TCPSTAT_ADD(tcps_dsack_tlp_bytes, (start - end));
		}
	}
}

static struct tcp_function_block *
find_tcp_functions_locked(struct tcp_function_set *fs)
{
	struct tcp_function *f;
	struct tcp_function_block *blk=NULL;

	TAILQ_FOREACH(f, &t_functions, tf_next) {
		if (strcmp(f->tf_name, fs->function_set_name) == 0) {
			blk = f->tf_fb;
			break;
		}
	}
	return(blk);
}

static struct tcp_function_block *
find_tcp_fb_locked(struct tcp_function_block *blk, struct tcp_function **s)
{
	struct tcp_function_block *rblk=NULL;
	struct tcp_function *f;

	TAILQ_FOREACH(f, &t_functions, tf_next) {
		if (f->tf_fb == blk) {
			rblk = blk;
			if (s) {
				*s = f;
			}
			break;
		}
	}
	return (rblk);
}

struct tcp_function_block *
find_and_ref_tcp_functions(struct tcp_function_set *fs)
{
	struct tcp_function_block *blk;

	rw_rlock(&tcp_function_lock);
	blk = find_tcp_functions_locked(fs);
	if (blk)
		refcount_acquire(&blk->tfb_refcnt);
	rw_runlock(&tcp_function_lock);
	return(blk);
}

struct tcp_function_block *
find_and_ref_tcp_fb(struct tcp_function_block *blk)
{
	struct tcp_function_block *rblk;

	rw_rlock(&tcp_function_lock);
	rblk = find_tcp_fb_locked(blk, NULL);
	if (rblk)
		refcount_acquire(&rblk->tfb_refcnt);
	rw_runlock(&tcp_function_lock);
	return(rblk);
}

/* Find a matching alias for the given tcp_function_block. */
int
find_tcp_function_alias(struct tcp_function_block *blk,
    struct tcp_function_set *fs)
{
	struct tcp_function *f;
	int found;

	found = 0;
	rw_rlock(&tcp_function_lock);
	TAILQ_FOREACH(f, &t_functions, tf_next) {
		if ((f->tf_fb == blk) &&
		    (strncmp(f->tf_name, blk->tfb_tcp_block_name,
		        TCP_FUNCTION_NAME_LEN_MAX) != 0)) {
			/* Matching function block with different name. */
			strncpy(fs->function_set_name, f->tf_name,
			    TCP_FUNCTION_NAME_LEN_MAX);
			found = 1;
			break;
		}
	}
	/* Null terminate the string appropriately. */
	if (found) {
		fs->function_set_name[TCP_FUNCTION_NAME_LEN_MAX - 1] = '\0';
	} else {
		fs->function_set_name[0] = '\0';
	}
	rw_runlock(&tcp_function_lock);
	return (found);
}

static struct tcp_function_block *
find_and_ref_tcp_default_fb(void)
{
	struct tcp_function_block *rblk;

	rw_rlock(&tcp_function_lock);
	rblk = V_tcp_func_set_ptr;
	refcount_acquire(&rblk->tfb_refcnt);
	rw_runlock(&tcp_function_lock);
	return (rblk);
}

void
tcp_switch_back_to_default(struct tcpcb *tp)
{
	struct tcp_function_block *tfb;
	void *ptr = NULL;

	KASSERT(tp->t_fb != &tcp_def_funcblk,
	    ("%s: called by the built-in default stack", __func__));

	if (tp->t_fb->tfb_tcp_timer_stop_all != NULL)
		tp->t_fb->tfb_tcp_timer_stop_all(tp);

	/*
	 * Now, we'll find a new function block to use.
	 * Start by trying the current user-selected
	 * default, unless this stack is the user-selected
	 * default.
	 */
	tfb = find_and_ref_tcp_default_fb();
	if (tfb == tp->t_fb) {
		refcount_release(&tfb->tfb_refcnt);
		tfb = NULL;
	}
	/* Does the stack accept this connection? */
	if (tfb != NULL && tfb->tfb_tcp_handoff_ok != NULL &&
	    (*tfb->tfb_tcp_handoff_ok)(tp)) {
		refcount_release(&tfb->tfb_refcnt);
		tfb = NULL;
	}
	/* Try to use that stack. */
	if (tfb != NULL) {
		/* Initialize the new stack. If it succeeds, we are done. */
		if (tfb->tfb_tcp_fb_init == NULL ||
		    (*tfb->tfb_tcp_fb_init)(tp, &ptr) == 0) {
			/* Release the old stack */
			if (tp->t_fb->tfb_tcp_fb_fini != NULL)
				(*tp->t_fb->tfb_tcp_fb_fini)(tp, 0);
			refcount_release(&tp->t_fb->tfb_refcnt);
			/* Now set in all the pointers */
			tp->t_fb = tfb;
			tp->t_fb_ptr = ptr;
			return;
		}
		/*
		 * Initialization failed. Release the reference count on
		 * the looked up default stack.
		 */
		refcount_release(&tfb->tfb_refcnt);
	}

	/*
	 * If that wasn't feasible, use the built-in default
	 * stack which is not allowed to reject anyone.
	 */
	tfb = find_and_ref_tcp_fb(&tcp_def_funcblk);
	if (tfb == NULL) {
		/* there always should be a default */
		panic("Can't refer to tcp_def_funcblk");
	}
	if (tfb->tfb_tcp_handoff_ok != NULL) {
		if ((*tfb->tfb_tcp_handoff_ok) (tp)) {
			/* The default stack cannot say no */
			panic("Default stack rejects a new session?");
		}
	}
	if (tfb->tfb_tcp_fb_init != NULL &&
	    (*tfb->tfb_tcp_fb_init)(tp, &ptr)) {
		/* The default stack cannot fail */
		panic("Default stack initialization failed");
	}
	/* Now release the old stack */
	if (tp->t_fb->tfb_tcp_fb_fini != NULL)
		(*tp->t_fb->tfb_tcp_fb_fini)(tp, 0);
	refcount_release(&tp->t_fb->tfb_refcnt);
	/* And set in the pointers to the new */
	tp->t_fb = tfb;
	tp->t_fb_ptr = ptr;
}

static bool
tcp_recv_udp_tunneled_packet(struct mbuf *m, int off, struct inpcb *inp,
    const struct sockaddr *sa, void *ctx)
{
	struct ip *iph;
#ifdef INET6
	struct ip6_hdr *ip6;
#endif
	struct udphdr *uh;
	struct tcphdr *th;
	int thlen;
	uint16_t port;

	TCPSTAT_INC(tcps_tunneled_pkts);
	if ((m->m_flags & M_PKTHDR) == 0) {
		/* Can't handle one that is not a pkt hdr */
		TCPSTAT_INC(tcps_tunneled_errs);
		goto out;
	}
	thlen = sizeof(struct tcphdr);
	if (m->m_len < off + sizeof(struct udphdr) + thlen &&
	    (m =  m_pullup(m, off + sizeof(struct udphdr) + thlen)) == NULL) {
		TCPSTAT_INC(tcps_tunneled_errs);
		goto out;
	}
	iph = mtod(m, struct ip *);
	uh = (struct udphdr *)((caddr_t)iph + off);
	th = (struct tcphdr *)(uh + 1);
	thlen = th->th_off << 2;
	if (m->m_len < off + sizeof(struct udphdr) + thlen) {
		m =  m_pullup(m, off + sizeof(struct udphdr) + thlen);
		if (m == NULL) {
			TCPSTAT_INC(tcps_tunneled_errs);
			goto out;
		} else {
			iph = mtod(m, struct ip *);
			uh = (struct udphdr *)((caddr_t)iph + off);
			th = (struct tcphdr *)(uh + 1);
		}
	}
	m->m_pkthdr.tcp_tun_port = port = uh->uh_sport;
	bcopy(th, uh, m->m_len - off);
	m->m_len -= sizeof(struct udphdr);
	m->m_pkthdr.len -= sizeof(struct udphdr);
	/*
	 * We use the same algorithm for
	 * both UDP and TCP for c-sum. So
	 * the code in tcp_input will skip
	 * the checksum. So we do nothing
	 * with the flag (m->m_pkthdr.csum_flags).
	 */
	switch (iph->ip_v) {
#ifdef INET
	case IPVERSION:
		iph->ip_len = htons(ntohs(iph->ip_len) - sizeof(struct udphdr));
		tcp_input_with_port(&m, &off, IPPROTO_TCP, port);
		break;
#endif
#ifdef INET6
	case IPV6_VERSION >> 4:
		ip6 = mtod(m, struct ip6_hdr *);
		ip6->ip6_plen = htons(ntohs(ip6->ip6_plen) - sizeof(struct udphdr));
		tcp6_input_with_port(&m, &off, IPPROTO_TCP, port);
		break;
#endif
	default:
		goto out;
		break;
	}
	return (true);
out:
	m_freem(m);

	return (true);
}

static int
sysctl_net_inet_default_tcp_functions(SYSCTL_HANDLER_ARGS)
{
	int error=ENOENT;
	struct tcp_function_set fs;
	struct tcp_function_block *blk;

	memset(&fs, 0, sizeof(fs));
	rw_rlock(&tcp_function_lock);
	blk = find_tcp_fb_locked(V_tcp_func_set_ptr, NULL);
	if (blk) {
		/* Found him */
		strcpy(fs.function_set_name, blk->tfb_tcp_block_name);
		fs.pcbcnt = blk->tfb_refcnt;
	}
	rw_runlock(&tcp_function_lock);
	error = sysctl_handle_string(oidp, fs.function_set_name,
				     sizeof(fs.function_set_name), req);

	/* Check for error or no change */
	if (error != 0 || req->newptr == NULL)
		return(error);

	rw_wlock(&tcp_function_lock);
	blk = find_tcp_functions_locked(&fs);
	if ((blk == NULL) ||
	    (blk->tfb_flags & TCP_FUNC_BEING_REMOVED)) {
		error = ENOENT;
		goto done;
	}
	V_tcp_func_set_ptr = blk;
done:
	rw_wunlock(&tcp_function_lock);
	return (error);
}

SYSCTL_PROC(_net_inet_tcp, OID_AUTO, functions_default,
    CTLFLAG_VNET | CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
    NULL, 0, sysctl_net_inet_default_tcp_functions, "A",
    "Set/get the default TCP functions");

static int
sysctl_net_inet_list_available(SYSCTL_HANDLER_ARGS)
{
	int error, cnt, linesz;
	struct tcp_function *f;
	char *buffer, *cp;
	size_t bufsz, outsz;
	bool alias;

	cnt = 0;
	rw_rlock(&tcp_function_lock);
	TAILQ_FOREACH(f, &t_functions, tf_next) {
		cnt++;
	}
	rw_runlock(&tcp_function_lock);

	bufsz = (cnt+2) * ((TCP_FUNCTION_NAME_LEN_MAX * 2) + 13) + 1;
	buffer = malloc(bufsz, M_TEMP, M_WAITOK);

	error = 0;
	cp = buffer;

	linesz = snprintf(cp, bufsz, "\n%-32s%c %-32s %s\n", "Stack", 'D',
	    "Alias", "PCB count");
	cp += linesz;
	bufsz -= linesz;
	outsz = linesz;

	rw_rlock(&tcp_function_lock);
	TAILQ_FOREACH(f, &t_functions, tf_next) {
		alias = (f->tf_name != f->tf_fb->tfb_tcp_block_name);
		linesz = snprintf(cp, bufsz, "%-32s%c %-32s %u\n",
		    f->tf_fb->tfb_tcp_block_name,
		    (f->tf_fb == V_tcp_func_set_ptr) ? '*' : ' ',
		    alias ? f->tf_name : "-",
		    f->tf_fb->tfb_refcnt);
		if (linesz >= bufsz) {
			error = EOVERFLOW;
			break;
		}
		cp += linesz;
		bufsz -= linesz;
		outsz += linesz;
	}
	rw_runlock(&tcp_function_lock);
	if (error == 0)
		error = sysctl_handle_string(oidp, buffer, outsz + 1, req);
	free(buffer, M_TEMP);
	return (error);
}

SYSCTL_PROC(_net_inet_tcp, OID_AUTO, functions_available,
    CTLFLAG_VNET | CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NEEDGIANT,
    NULL, 0, sysctl_net_inet_list_available, "A",
    "list available TCP Function sets");

VNET_DEFINE(int, tcp_udp_tunneling_port) = TCP_TUNNELING_PORT_DEFAULT;

#ifdef INET
VNET_DEFINE(struct socket *, udp4_tun_socket) = NULL;
#define	V_udp4_tun_socket	VNET(udp4_tun_socket)
#endif
#ifdef INET6
VNET_DEFINE(struct socket *, udp6_tun_socket) = NULL;
#define	V_udp6_tun_socket	VNET(udp6_tun_socket)
#endif

static struct sx tcpoudp_lock;

static void
tcp_over_udp_stop(void)
{

	sx_assert(&tcpoudp_lock, SA_XLOCKED);

#ifdef INET
	if (V_udp4_tun_socket != NULL) {
		soclose(V_udp4_tun_socket);
		V_udp4_tun_socket = NULL;
	}
#endif
#ifdef INET6
	if (V_udp6_tun_socket != NULL) {
		soclose(V_udp6_tun_socket);
		V_udp6_tun_socket = NULL;
	}
#endif
}

static int
tcp_over_udp_start(void)
{
	uint16_t port;
	int ret;
#ifdef INET
	struct sockaddr_in sin;
#endif
#ifdef INET6
	struct sockaddr_in6 sin6;
#endif

	sx_assert(&tcpoudp_lock, SA_XLOCKED);

	port = V_tcp_udp_tunneling_port;
	if (ntohs(port) == 0) {
		/* Must have a port set */
		return (EINVAL);
	}
#ifdef INET
	if (V_udp4_tun_socket != NULL) {
		/* Already running -- must stop first */
		return (EALREADY);
	}
#endif
#ifdef INET6
	if (V_udp6_tun_socket != NULL) {
		/* Already running -- must stop first */
		return (EALREADY);
	}
#endif
#ifdef INET
	if ((ret = socreate(PF_INET, &V_udp4_tun_socket,
	    SOCK_DGRAM, IPPROTO_UDP,
	    curthread->td_ucred, curthread))) {
		tcp_over_udp_stop();
		return (ret);
	}
	/* Call the special UDP hook. */
	if ((ret = udp_set_kernel_tunneling(V_udp4_tun_socket,
	    tcp_recv_udp_tunneled_packet,
	    tcp_ctlinput_viaudp,
	    NULL))) {
		tcp_over_udp_stop();
		return (ret);
	}
	/* Ok, we have a socket, bind it to the port. */
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_len = sizeof(struct sockaddr_in);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	if ((ret = sobind(V_udp4_tun_socket,
	    (struct sockaddr *)&sin, curthread))) {
		tcp_over_udp_stop();
		return (ret);
	}
#endif
#ifdef INET6
	if ((ret = socreate(PF_INET6, &V_udp6_tun_socket,
	    SOCK_DGRAM, IPPROTO_UDP,
	    curthread->td_ucred, curthread))) {
		tcp_over_udp_stop();
		return (ret);
	}
	/* Call the special UDP hook. */
	if ((ret = udp_set_kernel_tunneling(V_udp6_tun_socket,
	    tcp_recv_udp_tunneled_packet,
	    tcp6_ctlinput_viaudp,
	    NULL))) {
		tcp_over_udp_stop();
		return (ret);
	}
	/* Ok, we have a socket, bind it to the port. */
	memset(&sin6, 0, sizeof(struct sockaddr_in6));
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(port);
	if ((ret = sobind(V_udp6_tun_socket,
	    (struct sockaddr *)&sin6, curthread))) {
		tcp_over_udp_stop();
		return (ret);
	}
#endif
	return (0);
}

static int
sysctl_net_inet_tcp_udp_tunneling_port_check(SYSCTL_HANDLER_ARGS)
{
	int error;
	uint32_t old, new;

	old = V_tcp_udp_tunneling_port;
	new = old;
	error = sysctl_handle_int(oidp, &new, 0, req);
	if ((error == 0) &&
	    (req->newptr != NULL)) {
		if ((new < TCP_TUNNELING_PORT_MIN) ||
		    (new > TCP_TUNNELING_PORT_MAX)) {
			error = EINVAL;
		} else {
			sx_xlock(&tcpoudp_lock);
			V_tcp_udp_tunneling_port = new;
			if (old != 0) {
				tcp_over_udp_stop();
			}
			if (new != 0) {
				error = tcp_over_udp_start();
				if (error != 0) {
					V_tcp_udp_tunneling_port = 0;
				}
			}
			sx_xunlock(&tcpoudp_lock);
		}
	}
	return (error);
}

SYSCTL_PROC(_net_inet_tcp, OID_AUTO, udp_tunneling_port,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    &VNET_NAME(tcp_udp_tunneling_port),
    0, &sysctl_net_inet_tcp_udp_tunneling_port_check, "IU",
    "Tunneling port for tcp over udp");

VNET_DEFINE(int, tcp_udp_tunneling_overhead) = TCP_TUNNELING_OVERHEAD_DEFAULT;

static int
sysctl_net_inet_tcp_udp_tunneling_overhead_check(SYSCTL_HANDLER_ARGS)
{
	int error, new;

	new = V_tcp_udp_tunneling_overhead;
	error = sysctl_handle_int(oidp, &new, 0, req);
	if (error == 0 && req->newptr) {
		if ((new < TCP_TUNNELING_OVERHEAD_MIN) ||
		    (new > TCP_TUNNELING_OVERHEAD_MAX))
			error = EINVAL;
		else
			V_tcp_udp_tunneling_overhead = new;
	}
	return (error);
}

SYSCTL_PROC(_net_inet_tcp, OID_AUTO, udp_tunneling_overhead,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    &VNET_NAME(tcp_udp_tunneling_overhead),
    0, &sysctl_net_inet_tcp_udp_tunneling_overhead_check, "IU",
    "MSS reduction when using tcp over udp");

/*
 * Exports one (struct tcp_function_info) for each alias/name.
 */
static int
sysctl_net_inet_list_func_info(SYSCTL_HANDLER_ARGS)
{
	int cnt, error;
	struct tcp_function *f;
	struct tcp_function_info tfi;

	/*
	 * We don't allow writes.
	 */
	if (req->newptr != NULL)
		return (EINVAL);

	/*
	 * Wire the old buffer so we can directly copy the functions to
	 * user space without dropping the lock.
	 */
	if (req->oldptr != NULL) {
		error = sysctl_wire_old_buffer(req, 0);
		if (error)
			return (error);
	}

	/*
	 * Walk the list and copy out matching entries. If INVARIANTS
	 * is compiled in, also walk the list to verify the length of
	 * the list matches what we have recorded.
	 */
	rw_rlock(&tcp_function_lock);

	cnt = 0;
#ifndef INVARIANTS
	if (req->oldptr == NULL) {
		cnt = tcp_fb_cnt;
		goto skip_loop;
	}
#endif
	TAILQ_FOREACH(f, &t_functions, tf_next) {
#ifdef INVARIANTS
		cnt++;
#endif
		if (req->oldptr != NULL) {
			bzero(&tfi, sizeof(tfi));
			tfi.tfi_refcnt = f->tf_fb->tfb_refcnt;
			tfi.tfi_id = f->tf_fb->tfb_id;
			(void)strlcpy(tfi.tfi_alias, f->tf_name,
			    sizeof(tfi.tfi_alias));
			(void)strlcpy(tfi.tfi_name,
			    f->tf_fb->tfb_tcp_block_name, sizeof(tfi.tfi_name));
			error = SYSCTL_OUT(req, &tfi, sizeof(tfi));
			/*
			 * Don't stop on error, as that is the
			 * mechanism we use to accumulate length
			 * information if the buffer was too short.
			 */
		}
	}
	KASSERT(cnt == tcp_fb_cnt,
	    ("%s: cnt (%d) != tcp_fb_cnt (%d)", __func__, cnt, tcp_fb_cnt));
#ifndef INVARIANTS
skip_loop:
#endif
	rw_runlock(&tcp_function_lock);
	if (req->oldptr == NULL)
		error = SYSCTL_OUT(req, NULL,
		    (cnt + 1) * sizeof(struct tcp_function_info));

	return (error);
}

SYSCTL_PROC(_net_inet_tcp, OID_AUTO, function_info,
	    CTLTYPE_OPAQUE | CTLFLAG_SKIP | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    NULL, 0, sysctl_net_inet_list_func_info, "S,tcp_function_info",
	    "List TCP function block name-to-ID mappings");

/*
 * tfb_tcp_handoff_ok() function for the default stack.
 * Note that we'll basically try to take all comers.
 */
static int
tcp_default_handoff_ok(struct tcpcb *tp)
{

	return (0);
}

/*
 * tfb_tcp_fb_init() function for the default stack.
 *
 * This handles making sure we have appropriate timers set if you are
 * transitioning a socket that has some amount of setup done.
 *
 * The init() fuction from the default can *never* return non-zero i.e.
 * it is required to always succeed since it is the stack of last resort!
 */
static int
tcp_default_fb_init(struct tcpcb *tp, void **ptr)
{
	struct socket *so = tptosocket(tp);
	int rexmt;

	INP_WLOCK_ASSERT(tptoinpcb(tp));
	/* We don't use the pointer */
	*ptr = NULL;

	KASSERT(tp->t_state >= 0 && tp->t_state < TCPS_TIME_WAIT,
	    ("%s: connection %p in unexpected state %d", __func__, tp,
	    tp->t_state));

	/* Make sure we get no interesting mbuf queuing behavior */
	/* All mbuf queue/ack compress flags should be off */
	tcp_lro_features_off(tp);

	/* Cancel the GP measurement in progress */
	tp->t_flags &= ~TF_GPUTINPROG;
	/* Validate the timers are not in usec, if they are convert */
	tcp_change_time_units(tp, TCP_TMR_GRANULARITY_TICKS);
	if ((tp->t_state == TCPS_SYN_SENT) ||
	    (tp->t_state == TCPS_SYN_RECEIVED))
		rexmt = tcp_rexmit_initial * tcp_backoff[tp->t_rxtshift];
	else
		rexmt = TCP_REXMTVAL(tp) * tcp_backoff[tp->t_rxtshift];
	if (tp->t_rxtshift == 0)
		tp->t_rxtcur = rexmt;
	else
		TCPT_RANGESET(tp->t_rxtcur, rexmt, tp->t_rttmin, TCPTV_REXMTMAX);

	/*
	 * Nothing to do for ESTABLISHED or LISTEN states. And, we don't
	 * know what to do for unexpected states (which includes TIME_WAIT).
	 */
	if (tp->t_state <= TCPS_LISTEN || tp->t_state >= TCPS_TIME_WAIT)
		return (0);

	/*
	 * Make sure some kind of transmission timer is set if there is
	 * outstanding data.
	 */
	if ((!TCPS_HAVEESTABLISHED(tp->t_state) || sbavail(&so->so_snd) ||
	    tp->snd_una != tp->snd_max) && !(tcp_timer_active(tp, TT_REXMT) ||
	    tcp_timer_active(tp, TT_PERSIST))) {
		/*
		 * If the session has established and it looks like it should
		 * be in the persist state, set the persist timer. Otherwise,
		 * set the retransmit timer.
		 */
		if (TCPS_HAVEESTABLISHED(tp->t_state) && tp->snd_wnd == 0 &&
		    (int32_t)(tp->snd_nxt - tp->snd_una) <
		    (int32_t)sbavail(&so->so_snd))
			tcp_setpersist(tp);
		else
			tcp_timer_activate(tp, TT_REXMT, TP_RXTCUR(tp));
	}

	/* All non-embryonic sessions get a keepalive timer. */
	if (!tcp_timer_active(tp, TT_KEEP))
		tcp_timer_activate(tp, TT_KEEP,
		    TCPS_HAVEESTABLISHED(tp->t_state) ? TP_KEEPIDLE(tp) :
		    TP_KEEPINIT(tp));

	/*
	 * Make sure critical variables are initialized
	 * if transitioning while in Recovery.
	 */
	if IN_FASTRECOVERY(tp->t_flags) {
		if (tp->sackhint.recover_fs == 0)
			tp->sackhint.recover_fs = max(1,
			    tp->snd_nxt - tp->snd_una);
	}

	return (0);
}

/*
 * tfb_tcp_fb_fini() function for the default stack.
 *
 * This changes state as necessary (or prudent) to prepare for another stack
 * to assume responsibility for the connection.
 */
static void
tcp_default_fb_fini(struct tcpcb *tp, int tcb_is_purged)
{

	INP_WLOCK_ASSERT(tptoinpcb(tp));

#ifdef TCP_BLACKBOX
	tcp_log_flowend(tp);
#endif
	tp->t_acktime = 0;
	return;
}

MALLOC_DEFINE(M_TCPLOG, "tcplog", "TCP address and flags print buffers");
MALLOC_DEFINE(M_TCPFUNCTIONS, "tcpfunc", "TCP function set memory");

static struct mtx isn_mtx;

#define	ISN_LOCK_INIT()	mtx_init(&isn_mtx, "isn_mtx", NULL, MTX_DEF)
#define	ISN_LOCK()	mtx_lock(&isn_mtx)
#define	ISN_UNLOCK()	mtx_unlock(&isn_mtx)

INPCBSTORAGE_DEFINE(tcpcbstor, tcpcb, "tcpinp", "tcp_inpcb", "tcp", "tcphash");

/*
 * Take a value and get the next power of 2 that doesn't overflow.
 * Used to size the tcp_inpcb hash buckets.
 */
static int
maketcp_hashsize(int size)
{
	int hashsize;

	/*
	 * auto tune.
	 * get the next power of 2 higher than maxsockets.
	 */
	hashsize = 1 << fls(size);
	/* catch overflow, and just go one power of 2 smaller */
	if (hashsize < size) {
		hashsize = 1 << (fls(size) - 1);
	}
	return (hashsize);
}

static volatile int next_tcp_stack_id = 1;

/*
 * Register a TCP function block with the name provided in the names
 * array.  (Note that this function does NOT automatically register
 * blk->tfb_tcp_block_name as a stack name.  Therefore, you should
 * explicitly include blk->tfb_tcp_block_name in the list of names if
 * you wish to register the stack with that name.)
 *
 * Either all name registrations will succeed or all will fail.  If
 * a name registration fails, the function will update the num_names
 * argument to point to the array index of the name that encountered
 * the failure.
 *
 * Returns 0 on success, or an error code on failure.
 */
int
register_tcp_functions_as_names(struct tcp_function_block *blk, int wait,
    const char *names[], int *num_names)
{
	struct tcp_function *n;
	struct tcp_function_set fs;
	int error, i;

	KASSERT(names != NULL && *num_names > 0,
	    ("%s: Called with 0-length name list", __func__));
	KASSERT(names != NULL, ("%s: Called with NULL name list", __func__));
	KASSERT(rw_initialized(&tcp_function_lock),
	    ("%s: called too early", __func__));

	if ((blk->tfb_tcp_output == NULL) ||
	    (blk->tfb_tcp_do_segment == NULL) ||
	    (blk->tfb_tcp_ctloutput == NULL) ||
	    (strlen(blk->tfb_tcp_block_name) == 0)) {
		/*
		 * These functions are required and you
		 * need a name.
		 */
		*num_names = 0;
		return (EINVAL);
	}

	if (blk->tfb_flags & TCP_FUNC_BEING_REMOVED) {
		*num_names = 0;
		return (EINVAL);
	}

	refcount_init(&blk->tfb_refcnt, 0);
	blk->tfb_id = atomic_fetchadd_int(&next_tcp_stack_id, 1);
	for (i = 0; i < *num_names; i++) {
		n = malloc(sizeof(struct tcp_function), M_TCPFUNCTIONS, wait);
		if (n == NULL) {
			error = ENOMEM;
			goto cleanup;
		}
		n->tf_fb = blk;

		(void)strlcpy(fs.function_set_name, names[i],
		    sizeof(fs.function_set_name));
		rw_wlock(&tcp_function_lock);
		if (find_tcp_functions_locked(&fs) != NULL) {
			/* Duplicate name space not allowed */
			rw_wunlock(&tcp_function_lock);
			free(n, M_TCPFUNCTIONS);
			error = EALREADY;
			goto cleanup;
		}
		(void)strlcpy(n->tf_name, names[i], sizeof(n->tf_name));
		TAILQ_INSERT_TAIL(&t_functions, n, tf_next);
		tcp_fb_cnt++;
		rw_wunlock(&tcp_function_lock);
	}
	return(0);

cleanup:
	/*
	 * Deregister the names we just added. Because registration failed
	 * for names[i], we don't need to deregister that name.
	 */
	*num_names = i;
	rw_wlock(&tcp_function_lock);
	while (--i >= 0) {
		TAILQ_FOREACH(n, &t_functions, tf_next) {
			if (!strncmp(n->tf_name, names[i],
			    TCP_FUNCTION_NAME_LEN_MAX)) {
				TAILQ_REMOVE(&t_functions, n, tf_next);
				tcp_fb_cnt--;
				n->tf_fb = NULL;
				free(n, M_TCPFUNCTIONS);
				break;
			}
		}
	}
	rw_wunlock(&tcp_function_lock);
	return (error);
}

/*
 * Register a TCP function block using the name provided in the name
 * argument.
 *
 * Returns 0 on success, or an error code on failure.
 */
int
register_tcp_functions_as_name(struct tcp_function_block *blk, const char *name,
    int wait)
{
	const char *name_list[1];
	int num_names, rv;

	num_names = 1;
	if (name != NULL)
		name_list[0] = name;
	else
		name_list[0] = blk->tfb_tcp_block_name;
	rv = register_tcp_functions_as_names(blk, wait, name_list, &num_names);
	return (rv);
}

/*
 * Register a TCP function block using the name defined in
 * blk->tfb_tcp_block_name.
 *
 * Returns 0 on success, or an error code on failure.
 */
int
register_tcp_functions(struct tcp_function_block *blk, int wait)
{

	return (register_tcp_functions_as_name(blk, NULL, wait));
}

/*
 * Deregister all names associated with a function block. This
 * functionally removes the function block from use within the system.
 *
 * When called with a true quiesce argument, mark the function block
 * as being removed so no more stacks will use it and determine
 * whether the removal would succeed.
 *
 * When called with a false quiesce argument, actually attempt the
 * removal.
 *
 * When called with a force argument, attempt to switch all TCBs to
 * use the default stack instead of returning EBUSY.
 *
 * Returns 0 on success (or if the removal would succeed), or an error
 * code on failure.
 */
int
deregister_tcp_functions(struct tcp_function_block *blk, bool quiesce,
    bool force)
{
	struct tcp_function *f;
	VNET_ITERATOR_DECL(vnet_iter);

	if (blk == &tcp_def_funcblk) {
		/* You can't un-register the default */
		return (EPERM);
	}
	rw_wlock(&tcp_function_lock);
	VNET_LIST_RLOCK_NOSLEEP();
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET(vnet_iter);
		if (blk == V_tcp_func_set_ptr) {
			/* You can't free the current default in some vnet. */
			CURVNET_RESTORE();
			VNET_LIST_RUNLOCK_NOSLEEP();
			rw_wunlock(&tcp_function_lock);
			return (EBUSY);
		}
		CURVNET_RESTORE();
	}
	VNET_LIST_RUNLOCK_NOSLEEP();
	/* Mark the block so no more stacks can use it. */
	blk->tfb_flags |= TCP_FUNC_BEING_REMOVED;
	/*
	 * If TCBs are still attached to the stack, attempt to switch them
	 * to the default stack.
	 */
	if (force && blk->tfb_refcnt) {
		struct inpcb *inp;
		struct tcpcb *tp;
		VNET_ITERATOR_DECL(vnet_iter);

		rw_wunlock(&tcp_function_lock);

		VNET_LIST_RLOCK();
		VNET_FOREACH(vnet_iter) {
			CURVNET_SET(vnet_iter);
			struct inpcb_iterator inpi = INP_ALL_ITERATOR(&V_tcbinfo,
			    INPLOOKUP_WLOCKPCB);

			while ((inp = inp_next(&inpi)) != NULL) {
				tp = intotcpcb(inp);
				if (tp == NULL || tp->t_fb != blk)
					continue;
				tcp_switch_back_to_default(tp);
			}
			CURVNET_RESTORE();
		}
		VNET_LIST_RUNLOCK();

		rw_wlock(&tcp_function_lock);
	}
	if (blk->tfb_refcnt) {
		/* TCBs still attached. */
		rw_wunlock(&tcp_function_lock);
		return (EBUSY);
	}
	if (quiesce) {
		/* Skip removal. */
		rw_wunlock(&tcp_function_lock);
		return (0);
	}
	/* Remove any function names that map to this function block. */
	while (find_tcp_fb_locked(blk, &f) != NULL) {
		TAILQ_REMOVE(&t_functions, f, tf_next);
		tcp_fb_cnt--;
		f->tf_fb = NULL;
		free(f, M_TCPFUNCTIONS);
	}
	rw_wunlock(&tcp_function_lock);
	return (0);
}

static void
tcp_drain(void)
{
	struct epoch_tracker et;
	VNET_ITERATOR_DECL(vnet_iter);

	if (!do_tcpdrain)
		return;

	NET_EPOCH_ENTER(et);
	VNET_LIST_RLOCK_NOSLEEP();
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET(vnet_iter);
		struct inpcb_iterator inpi = INP_ALL_ITERATOR(&V_tcbinfo,
		    INPLOOKUP_WLOCKPCB);
		struct inpcb *inpb;
		struct tcpcb *tcpb;

	/*
	 * Walk the tcpbs, if existing, and flush the reassembly queue,
	 * if there is one...
	 * XXX: The "Net/3" implementation doesn't imply that the TCP
	 *      reassembly queue should be flushed, but in a situation
	 *	where we're really low on mbufs, this is potentially
	 *	useful.
	 */
		while ((inpb = inp_next(&inpi)) != NULL) {
			if ((tcpb = intotcpcb(inpb)) != NULL) {
				tcp_reass_flush(tcpb);
				tcp_clean_sackreport(tcpb);
#ifdef TCP_BLACKBOX
				tcp_log_drain(tcpb);
#endif
#ifdef TCPPCAP
				if (tcp_pcap_aggressive_free) {
					/* Free the TCP PCAP queues. */
					tcp_pcap_drain(&(tcpb->t_inpkts));
					tcp_pcap_drain(&(tcpb->t_outpkts));
				}
#endif
			}
		}
		CURVNET_RESTORE();
	}
	VNET_LIST_RUNLOCK_NOSLEEP();
	NET_EPOCH_EXIT(et);
}

static void
tcp_vnet_init(void *arg __unused)
{

#ifdef TCP_HHOOK
	if (hhook_head_register(HHOOK_TYPE_TCP, HHOOK_TCP_EST_IN,
	    &V_tcp_hhh[HHOOK_TCP_EST_IN], HHOOK_NOWAIT|HHOOK_HEADISINVNET) != 0)
		printf("%s: WARNING: unable to register helper hook\n", __func__);
	if (hhook_head_register(HHOOK_TYPE_TCP, HHOOK_TCP_EST_OUT,
	    &V_tcp_hhh[HHOOK_TCP_EST_OUT], HHOOK_NOWAIT|HHOOK_HEADISINVNET) != 0)
		printf("%s: WARNING: unable to register helper hook\n", __func__);
#endif
#ifdef STATS
	if (tcp_stats_init())
		printf("%s: WARNING: unable to initialise TCP stats\n",
		    __func__);
#endif
	in_pcbinfo_init(&V_tcbinfo, &tcpcbstor, tcp_tcbhashsize,
	    tcp_tcbhashsize);

	syncache_init();
	tcp_hc_init();

	TUNABLE_INT_FETCH("net.inet.tcp.sack.enable", &V_tcp_do_sack);
	V_sack_hole_zone = uma_zcreate("sackhole", sizeof(struct sackhole),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);

	tcp_fastopen_init();

	COUNTER_ARRAY_ALLOC(V_tcps_states, TCP_NSTATES, M_WAITOK);
	VNET_PCPUSTAT_ALLOC(tcpstat, M_WAITOK);

	V_tcp_msl = TCPTV_MSL;
}
VNET_SYSINIT(tcp_vnet_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_FOURTH,
    tcp_vnet_init, NULL);

static void
tcp_init(void *arg __unused)
{
	int hashsize;

	tcp_reass_global_init();

	/* XXX virtualize those below? */
	tcp_delacktime = TCPTV_DELACK;
	tcp_keepinit = TCPTV_KEEP_INIT;
	tcp_keepidle = TCPTV_KEEP_IDLE;
	tcp_keepintvl = TCPTV_KEEPINTVL;
	tcp_maxpersistidle = TCPTV_KEEP_IDLE;
	tcp_rexmit_initial = TCPTV_RTOBASE;
	if (tcp_rexmit_initial < 1)
		tcp_rexmit_initial = 1;
	tcp_rexmit_min = TCPTV_MIN;
	if (tcp_rexmit_min < 1)
		tcp_rexmit_min = 1;
	tcp_persmin = TCPTV_PERSMIN;
	tcp_persmax = TCPTV_PERSMAX;
	tcp_rexmit_slop = TCPTV_CPU_VAR;
	tcp_finwait2_timeout = TCPTV_FINWAIT2_TIMEOUT;

	/* Setup the tcp function block list */
	TAILQ_INIT(&t_functions);
	rw_init(&tcp_function_lock, "tcp_func_lock");
	register_tcp_functions(&tcp_def_funcblk, M_WAITOK);
	sx_init(&tcpoudp_lock, "TCP over UDP configuration");
#ifdef TCP_BLACKBOX
	/* Initialize the TCP logging data. */
	tcp_log_init();
#endif
	arc4rand(&V_ts_offset_secret, sizeof(V_ts_offset_secret), 0);

	if (tcp_soreceive_stream) {
#ifdef INET
		tcp_protosw.pr_soreceive = soreceive_stream;
#endif
#ifdef INET6
		tcp6_protosw.pr_soreceive = soreceive_stream;
#endif /* INET6 */
	}

#ifdef INET6
	max_protohdr_grow(sizeof(struct ip6_hdr) + sizeof(struct tcphdr));
#else /* INET6 */
	max_protohdr_grow(sizeof(struct tcpiphdr));
#endif /* INET6 */

	ISN_LOCK_INIT();
	EVENTHANDLER_REGISTER(shutdown_pre_sync, tcp_fini, NULL,
		SHUTDOWN_PRI_DEFAULT);
	EVENTHANDLER_REGISTER(vm_lowmem, tcp_drain, NULL, LOWMEM_PRI_DEFAULT);
	EVENTHANDLER_REGISTER(mbuf_lowmem, tcp_drain, NULL, LOWMEM_PRI_DEFAULT);

	tcp_inp_lro_direct_queue = counter_u64_alloc(M_WAITOK);
	tcp_inp_lro_wokeup_queue = counter_u64_alloc(M_WAITOK);
	tcp_inp_lro_compressed = counter_u64_alloc(M_WAITOK);
	tcp_inp_lro_locks_taken = counter_u64_alloc(M_WAITOK);
	tcp_extra_mbuf = counter_u64_alloc(M_WAITOK);
	tcp_would_have_but = counter_u64_alloc(M_WAITOK);
	tcp_comp_total = counter_u64_alloc(M_WAITOK);
	tcp_uncomp_total = counter_u64_alloc(M_WAITOK);
	tcp_bad_csums = counter_u64_alloc(M_WAITOK);
	tcp_pacing_failures = counter_u64_alloc(M_WAITOK);
#ifdef TCPPCAP
	tcp_pcap_init();
#endif

	hashsize = tcp_tcbhashsize;
	if (hashsize == 0) {
		/*
		 * Auto tune the hash size based on maxsockets.
		 * A perfect hash would have a 1:1 mapping
		 * (hashsize = maxsockets) however it's been
		 * suggested that O(2) average is better.
		 */
		hashsize = maketcp_hashsize(maxsockets / 4);
		/*
		 * Our historical default is 512,
		 * do not autotune lower than this.
		 */
		if (hashsize < 512)
			hashsize = 512;
		if (bootverbose)
			printf("%s: %s auto tuned to %d\n", __func__,
			    "net.inet.tcp.tcbhashsize", hashsize);
	}
	/*
	 * We require a hashsize to be a power of two.
	 * Previously if it was not a power of two we would just reset it
	 * back to 512, which could be a nasty surprise if you did not notice
	 * the error message.
	 * Instead what we do is clip it to the closest power of two lower
	 * than the specified hash value.
	 */
	if (!powerof2(hashsize)) {
		int oldhashsize = hashsize;

		hashsize = maketcp_hashsize(hashsize);
		/* prevent absurdly low value */
		if (hashsize < 16)
			hashsize = 16;
		printf("%s: WARNING: TCB hash size not a power of 2, "
		    "clipped from %d to %d.\n", __func__, oldhashsize,
		    hashsize);
	}
	tcp_tcbhashsize = hashsize;

#ifdef INET
	IPPROTO_REGISTER(IPPROTO_TCP, tcp_input, tcp_ctlinput);
#endif
#ifdef INET6
	IP6PROTO_REGISTER(IPPROTO_TCP, tcp6_input, tcp6_ctlinput);
#endif
}
SYSINIT(tcp_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, tcp_init, NULL);

#ifdef VIMAGE
static void
tcp_destroy(void *unused __unused)
{
	int n;
#ifdef TCP_HHOOK
	int error;
#endif

	/*
	 * All our processes are gone, all our sockets should be cleaned
	 * up, which means, we should be past the tcp_discardcb() calls.
	 * Sleep to let all tcpcb timers really disappear and cleanup.
	 */
	for (;;) {
		INP_INFO_WLOCK(&V_tcbinfo);
		n = V_tcbinfo.ipi_count;
		INP_INFO_WUNLOCK(&V_tcbinfo);
		if (n == 0)
			break;
		pause("tcpdes", hz / 10);
	}
	tcp_hc_destroy();
	syncache_destroy();
	in_pcbinfo_destroy(&V_tcbinfo);
	/* tcp_discardcb() clears the sack_holes up. */
	uma_zdestroy(V_sack_hole_zone);

	/*
	 * Cannot free the zone until all tcpcbs are released as we attach
	 * the allocations to them.
	 */
	tcp_fastopen_destroy();

	COUNTER_ARRAY_FREE(V_tcps_states, TCP_NSTATES);
	VNET_PCPUSTAT_FREE(tcpstat);

#ifdef TCP_HHOOK
	error = hhook_head_deregister(V_tcp_hhh[HHOOK_TCP_EST_IN]);
	if (error != 0) {
		printf("%s: WARNING: unable to deregister helper hook "
		    "type=%d, id=%d: error %d returned\n", __func__,
		    HHOOK_TYPE_TCP, HHOOK_TCP_EST_IN, error);
	}
	error = hhook_head_deregister(V_tcp_hhh[HHOOK_TCP_EST_OUT]);
	if (error != 0) {
		printf("%s: WARNING: unable to deregister helper hook "
		    "type=%d, id=%d: error %d returned\n", __func__,
		    HHOOK_TYPE_TCP, HHOOK_TCP_EST_OUT, error);
	}
#endif
}
VNET_SYSUNINIT(tcp, SI_SUB_PROTO_DOMAIN, SI_ORDER_FOURTH, tcp_destroy, NULL);
#endif

void
tcp_fini(void *xtp)
{

}

/*
 * Fill in the IP and TCP headers for an outgoing packet, given the tcpcb.
 * tcp_template used to store this data in mbufs, but we now recopy it out
 * of the tcpcb each time to conserve mbufs.
 */
void
tcpip_fillheaders(struct inpcb *inp, uint16_t port, void *ip_ptr, void *tcp_ptr)
{
	struct tcphdr *th = (struct tcphdr *)tcp_ptr;

	INP_WLOCK_ASSERT(inp);

#ifdef INET6
	if ((inp->inp_vflag & INP_IPV6) != 0) {
		struct ip6_hdr *ip6;

		ip6 = (struct ip6_hdr *)ip_ptr;
		ip6->ip6_flow = (ip6->ip6_flow & ~IPV6_FLOWINFO_MASK) |
			(inp->inp_flow & IPV6_FLOWINFO_MASK);
		ip6->ip6_vfc = (ip6->ip6_vfc & ~IPV6_VERSION_MASK) |
			(IPV6_VERSION & IPV6_VERSION_MASK);
		if (port == 0)
			ip6->ip6_nxt = IPPROTO_TCP;
		else
			ip6->ip6_nxt = IPPROTO_UDP;
		ip6->ip6_plen = htons(sizeof(struct tcphdr));
		ip6->ip6_src = inp->in6p_laddr;
		ip6->ip6_dst = inp->in6p_faddr;
	}
#endif /* INET6 */
#if defined(INET6) && defined(INET)
	else
#endif
#ifdef INET
	{
		struct ip *ip;

		ip = (struct ip *)ip_ptr;
		ip->ip_v = IPVERSION;
		ip->ip_hl = 5;
		ip->ip_tos = inp->inp_ip_tos;
		ip->ip_len = 0;
		ip->ip_id = 0;
		ip->ip_off = 0;
		ip->ip_ttl = inp->inp_ip_ttl;
		ip->ip_sum = 0;
		if (port == 0)
			ip->ip_p = IPPROTO_TCP;
		else
			ip->ip_p = IPPROTO_UDP;
		ip->ip_src = inp->inp_laddr;
		ip->ip_dst = inp->inp_faddr;
	}
#endif /* INET */
	th->th_sport = inp->inp_lport;
	th->th_dport = inp->inp_fport;
	th->th_seq = 0;
	th->th_ack = 0;
	th->th_off = 5;
	tcp_set_flags(th, 0);
	th->th_win = 0;
	th->th_urp = 0;
	th->th_sum = 0;		/* in_pseudo() is called later for ipv4 */
}

/*
 * Create template to be used to send tcp packets on a connection.
 * Allocates an mbuf and fills in a skeletal tcp/ip header.  The only
 * use for this function is in keepalives, which use tcp_respond.
 */
struct tcptemp *
tcpip_maketemplate(struct inpcb *inp)
{
	struct tcptemp *t;

	t = malloc(sizeof(*t), M_TEMP, M_NOWAIT);
	if (t == NULL)
		return (NULL);
	tcpip_fillheaders(inp, 0, (void *)&t->tt_ipgen, (void *)&t->tt_t);
	return (t);
}

/*
 * Send a single message to the TCP at address specified by
 * the given TCP/IP header.  If m == NULL, then we make a copy
 * of the tcpiphdr at th and send directly to the addressed host.
 * This is used to force keep alive messages out using the TCP
 * template for a connection.  If flags are given then we send
 * a message back to the TCP which originated the segment th,
 * and discard the mbuf containing it and any other attached mbufs.
 *
 * In any case the ack and sequence number of the transmitted
 * segment are as specified by the parameters.
 *
 * NOTE: If m != NULL, then th must point to *inside* the mbuf.
 */
void
tcp_respond(struct tcpcb *tp, void *ipgen, struct tcphdr *th, struct mbuf *m,
    tcp_seq ack, tcp_seq seq, uint16_t flags)
{
	struct tcpopt to;
	struct inpcb *inp;
	struct ip *ip;
	struct mbuf *optm;
	struct udphdr *uh = NULL;
	struct tcphdr *nth;
	struct tcp_log_buffer *lgb;
	u_char *optp;
#ifdef INET6
	struct ip6_hdr *ip6;
	int isipv6;
#endif /* INET6 */
	int optlen, tlen, win, ulen;
	int ect = 0;
	bool incl_opts;
	uint16_t port;
	int output_ret;
#ifdef INVARIANTS
	int thflags = tcp_get_flags(th);
#endif

	KASSERT(tp != NULL || m != NULL, ("tcp_respond: tp and m both NULL"));
	NET_EPOCH_ASSERT();

#ifdef INET6
	isipv6 = ((struct ip *)ipgen)->ip_v == (IPV6_VERSION >> 4);
	ip6 = ipgen;
#endif /* INET6 */
	ip = ipgen;

	if (tp != NULL) {
		inp = tptoinpcb(tp);
		INP_LOCK_ASSERT(inp);
	} else
		inp = NULL;

	if (m != NULL) {
#ifdef INET6
		if (isipv6 && ip6 && (ip6->ip6_nxt == IPPROTO_UDP))
			port = m->m_pkthdr.tcp_tun_port;
		else
#endif
		if (ip && (ip->ip_p == IPPROTO_UDP))
			port = m->m_pkthdr.tcp_tun_port;
		else
			port = 0;
	} else
		port = tp->t_port;

	incl_opts = false;
	win = 0;
	if (tp != NULL) {
		if (!(flags & TH_RST)) {
			win = sbspace(&inp->inp_socket->so_rcv);
			if (win > TCP_MAXWIN << tp->rcv_scale)
				win = TCP_MAXWIN << tp->rcv_scale;
		}
		if ((tp->t_flags & TF_NOOPT) == 0)
			incl_opts = true;
	}
	if (m == NULL) {
		m = m_gethdr(M_NOWAIT, MT_DATA);
		if (m == NULL)
			return;
		m->m_data += max_linkhdr;
#ifdef INET6
		if (isipv6) {
			bcopy((caddr_t)ip6, mtod(m, caddr_t),
			      sizeof(struct ip6_hdr));
			ip6 = mtod(m, struct ip6_hdr *);
			nth = (struct tcphdr *)(ip6 + 1);
			if (port) {
				/* Insert a UDP header */
				uh = (struct udphdr *)nth;
				uh->uh_sport = htons(V_tcp_udp_tunneling_port);
				uh->uh_dport = port;
				nth = (struct tcphdr *)(uh + 1);
			}
		} else
#endif /* INET6 */
		{
			bcopy((caddr_t)ip, mtod(m, caddr_t), sizeof(struct ip));
			ip = mtod(m, struct ip *);
			nth = (struct tcphdr *)(ip + 1);
			if (port) {
				/* Insert a UDP header */
				uh = (struct udphdr *)nth;
				uh->uh_sport = htons(V_tcp_udp_tunneling_port);
				uh->uh_dport = port;
				nth = (struct tcphdr *)(uh + 1);
			}
		}
		bcopy((caddr_t)th, (caddr_t)nth, sizeof(struct tcphdr));
		flags = TH_ACK;
	} else if ((!M_WRITABLE(m)) || (port != 0)) {
		struct mbuf *n;

		/* Can't reuse 'm', allocate a new mbuf. */
		n = m_gethdr(M_NOWAIT, MT_DATA);
		if (n == NULL) {
			m_freem(m);
			return;
		}

		if (!m_dup_pkthdr(n, m, M_NOWAIT)) {
			m_freem(m);
			m_freem(n);
			return;
		}

		n->m_data += max_linkhdr;
		/* m_len is set later */
#define xchg(a,b,type) { type t; t=a; a=b; b=t; }
#ifdef INET6
		if (isipv6) {
			bcopy((caddr_t)ip6, mtod(n, caddr_t),
			      sizeof(struct ip6_hdr));
			ip6 = mtod(n, struct ip6_hdr *);
			xchg(ip6->ip6_dst, ip6->ip6_src, struct in6_addr);
			nth = (struct tcphdr *)(ip6 + 1);
			if (port) {
				/* Insert a UDP header */
				uh = (struct udphdr *)nth;
				uh->uh_sport = htons(V_tcp_udp_tunneling_port);
				uh->uh_dport = port;
				nth = (struct tcphdr *)(uh + 1);
			}
		} else
#endif /* INET6 */
		{
			bcopy((caddr_t)ip, mtod(n, caddr_t), sizeof(struct ip));
			ip = mtod(n, struct ip *);
			xchg(ip->ip_dst.s_addr, ip->ip_src.s_addr, uint32_t);
			nth = (struct tcphdr *)(ip + 1);
			if (port) {
				/* Insert a UDP header */
				uh = (struct udphdr *)nth;
				uh->uh_sport = htons(V_tcp_udp_tunneling_port);
				uh->uh_dport = port;
				nth = (struct tcphdr *)(uh + 1);
			}
		}
		bcopy((caddr_t)th, (caddr_t)nth, sizeof(struct tcphdr));
		xchg(nth->th_dport, nth->th_sport, uint16_t);
		th = nth;
		m_freem(m);
		m = n;
	} else {
		/*
		 *  reuse the mbuf.
		 * XXX MRT We inherit the FIB, which is lucky.
		 */
		m_freem(m->m_next);
		m->m_next = NULL;
		m->m_data = (caddr_t)ipgen;
		/* clear any receive flags for proper bpf timestamping */
		m->m_flags &= ~(M_TSTMP | M_TSTMP_LRO);
		/* m_len is set later */
#ifdef INET6
		if (isipv6) {
			xchg(ip6->ip6_dst, ip6->ip6_src, struct in6_addr);
			nth = (struct tcphdr *)(ip6 + 1);
		} else
#endif /* INET6 */
		{
			xchg(ip->ip_dst.s_addr, ip->ip_src.s_addr, uint32_t);
			nth = (struct tcphdr *)(ip + 1);
		}
		if (th != nth) {
			/*
			 * this is usually a case when an extension header
			 * exists between the IPv6 header and the
			 * TCP header.
			 */
			nth->th_sport = th->th_sport;
			nth->th_dport = th->th_dport;
		}
		xchg(nth->th_dport, nth->th_sport, uint16_t);
#undef xchg
	}
	tlen = 0;
#ifdef INET6
	if (isipv6)
		tlen = sizeof (struct ip6_hdr) + sizeof (struct tcphdr);
#endif
#if defined(INET) && defined(INET6)
	else
#endif
#ifdef INET
		tlen = sizeof (struct tcpiphdr);
#endif
	if (port)
		tlen += sizeof (struct udphdr);
#ifdef INVARIANTS
	m->m_len = 0;
	KASSERT(M_TRAILINGSPACE(m) >= tlen,
	    ("Not enough trailing space for message (m=%p, need=%d, have=%ld)",
	    m, tlen, (long)M_TRAILINGSPACE(m)));
#endif
	m->m_len = tlen;
	to.to_flags = 0;
	if (incl_opts) {
		ect = tcp_ecn_output_established(tp, &flags, 0, false);
		/* Make sure we have room. */
		if (M_TRAILINGSPACE(m) < TCP_MAXOLEN) {
			m->m_next = m_get(M_NOWAIT, MT_DATA);
			if (m->m_next) {
				optp = mtod(m->m_next, u_char *);
				optm = m->m_next;
			} else
				incl_opts = false;
		} else {
			optp = (u_char *) (nth + 1);
			optm = m;
		}
	}
	if (incl_opts) {
		/* Timestamps. */
		if (tp->t_flags & TF_RCVD_TSTMP) {
			to.to_tsval = tcp_ts_getticks() + tp->ts_offset;
			to.to_tsecr = tp->ts_recent;
			to.to_flags |= TOF_TS;
		}
#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
		/* TCP-MD5 (RFC2385). */
		if (tp->t_flags & TF_SIGNATURE)
			to.to_flags |= TOF_SIGNATURE;
#endif
		/* Add the options. */
		tlen += optlen = tcp_addoptions(&to, optp);

		/* Update m_len in the correct mbuf. */
		optm->m_len += optlen;
	} else
		optlen = 0;
#ifdef INET6
	if (isipv6) {
		if (uh) {
			ulen = tlen - sizeof(struct ip6_hdr);
			uh->uh_ulen = htons(ulen);
		}
		ip6->ip6_flow = htonl(ect << IPV6_FLOWLABEL_LEN);
		ip6->ip6_vfc = IPV6_VERSION;
		if (port)
			ip6->ip6_nxt = IPPROTO_UDP;
		else
			ip6->ip6_nxt = IPPROTO_TCP;
		ip6->ip6_plen = htons(tlen - sizeof(*ip6));
	}
#endif
#if defined(INET) && defined(INET6)
	else
#endif
#ifdef INET
	{
		if (uh) {
			ulen = tlen - sizeof(struct ip);
			uh->uh_ulen = htons(ulen);
		}
		ip->ip_len = htons(tlen);
		if (inp != NULL) {
			ip->ip_tos = inp->inp_ip_tos & ~IPTOS_ECN_MASK;
			ip->ip_ttl = inp->inp_ip_ttl;
		} else {
			ip->ip_tos = 0;
			ip->ip_ttl = V_ip_defttl;
		}
		ip->ip_tos |= ect;
		if (port) {
			ip->ip_p = IPPROTO_UDP;
		} else {
			ip->ip_p = IPPROTO_TCP;
		}
		if (V_path_mtu_discovery)
			ip->ip_off |= htons(IP_DF);
	}
#endif
	m->m_pkthdr.len = tlen;
	m->m_pkthdr.rcvif = NULL;
#ifdef MAC
	if (inp != NULL) {
		/*
		 * Packet is associated with a socket, so allow the
		 * label of the response to reflect the socket label.
		 */
		INP_LOCK_ASSERT(inp);
		mac_inpcb_create_mbuf(inp, m);
	} else {
		/*
		 * Packet is not associated with a socket, so possibly
		 * update the label in place.
		 */
		mac_netinet_tcp_reply(m);
	}
#endif
	nth->th_seq = htonl(seq);
	nth->th_ack = htonl(ack);
	nth->th_off = (sizeof (struct tcphdr) + optlen) >> 2;
	tcp_set_flags(nth, flags);
	if (tp && (flags & TH_RST)) {
		/* Log the reset */
		tcp_log_end_status(tp, TCP_EI_STATUS_SERVER_RST);
	}
	if (tp != NULL)
		nth->th_win = htons((u_short) (win >> tp->rcv_scale));
	else
		nth->th_win = htons((u_short)win);
	nth->th_urp = 0;

#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
	if (to.to_flags & TOF_SIGNATURE) {
		if (!TCPMD5_ENABLED() ||
		    TCPMD5_OUTPUT(m, nth, to.to_signature) != 0) {
			m_freem(m);
			return;
		}
	}
#endif

#ifdef INET6
	if (isipv6) {
		if (port) {
			m->m_pkthdr.csum_flags = CSUM_UDP_IPV6;
			m->m_pkthdr.csum_data = offsetof(struct udphdr, uh_sum);
			uh->uh_sum = in6_cksum_pseudo(ip6, ulen, IPPROTO_UDP, 0);
			nth->th_sum = 0;
		} else {
			m->m_pkthdr.csum_flags = CSUM_TCP_IPV6;
			m->m_pkthdr.csum_data = offsetof(struct tcphdr, th_sum);
			nth->th_sum = in6_cksum_pseudo(ip6,
			    tlen - sizeof(struct ip6_hdr), IPPROTO_TCP, 0);
		}
		ip6->ip6_hlim = in6_selecthlim(inp, NULL);
	}
#endif /* INET6 */
#if defined(INET6) && defined(INET)
	else
#endif
#ifdef INET
	{
		if (port) {
			uh->uh_sum = in_pseudo(ip->ip_src.s_addr, ip->ip_dst.s_addr,
			    htons(ulen + IPPROTO_UDP));
			m->m_pkthdr.csum_flags = CSUM_UDP;
			m->m_pkthdr.csum_data = offsetof(struct udphdr, uh_sum);
			nth->th_sum = 0;
		} else {
			m->m_pkthdr.csum_flags = CSUM_TCP;
			m->m_pkthdr.csum_data = offsetof(struct tcphdr, th_sum);
			nth->th_sum = in_pseudo(ip->ip_src.s_addr, ip->ip_dst.s_addr,
			    htons((u_short)(tlen - sizeof(struct ip) + ip->ip_p)));
		}
	}
#endif /* INET */
	TCP_PROBE3(debug__output, tp, th, m);
	if (flags & TH_RST)
		TCP_PROBE5(accept__refused, NULL, NULL, m, tp, nth);
	lgb = NULL;
	if ((tp != NULL) && tcp_bblogging_on(tp)) {
		if (INP_WLOCKED(inp)) {
			union tcp_log_stackspecific log;
			struct timeval tv;

			memset(&log.u_bbr, 0, sizeof(log.u_bbr));
			log.u_bbr.inhpts = tcp_in_hpts(tp);
			log.u_bbr.flex8 = 4;
			log.u_bbr.pkts_out = tp->t_maxseg;
			log.u_bbr.timeStamp = tcp_get_usecs(&tv);
			log.u_bbr.delivered = 0;
			lgb = tcp_log_event(tp, nth, NULL, NULL, TCP_LOG_OUT,
			    ERRNO_UNK, 0, &log, false, NULL, NULL, 0, &tv);
		} else {
			/*
			 * We can not log the packet, since we only own the
			 * read lock, but a write lock is needed. The read lock
			 * is not upgraded to a write lock, since only getting
			 * the read lock was done intentionally to improve the
			 * handling of SYN flooding attacks.
			 * This happens only for pure SYN segments received in
			 * the initial CLOSED state, or received in a more
			 * advanced state than listen and the UDP encapsulation
			 * port is unexpected.
			 * The incoming SYN segments do not really belong to
			 * the TCP connection and the handling does not change
			 * the state of the TCP connection. Therefore, the
			 * sending of the RST segments is not logged. Please
			 * note that also the incoming SYN segments are not
			 * logged.
			 *
			 * The following code ensures that the above description
			 * is and stays correct.
			 */
			KASSERT((thflags & (TH_ACK|TH_SYN)) == TH_SYN &&
			    (tp->t_state == TCPS_CLOSED ||
			    (tp->t_state > TCPS_LISTEN && tp->t_port != port)),
			    ("%s: Logging of TCP segment with flags 0x%b and "
			    "UDP encapsulation port %u skipped in state %s",
			    __func__, thflags, PRINT_TH_FLAGS,
			    ntohs(port), tcpstates[tp->t_state]));
		}
	}

	if (flags & TH_ACK)
		TCPSTAT_INC(tcps_sndacks);
	else if (flags & (TH_SYN|TH_FIN|TH_RST))
		TCPSTAT_INC(tcps_sndctrl);
	TCPSTAT_INC(tcps_sndtotal);

#ifdef INET6
	if (isipv6) {
		TCP_PROBE5(send, NULL, tp, ip6, tp, nth);
		output_ret = ip6_output(m, inp ? inp->in6p_outputopts : NULL,
		    NULL, 0, NULL, NULL, inp);
	}
#endif /* INET6 */
#if defined(INET) && defined(INET6)
	else
#endif
#ifdef INET
	{
		TCP_PROBE5(send, NULL, tp, ip, tp, nth);
		output_ret = ip_output(m, NULL, NULL, 0, NULL, inp);
	}
#endif
	if (lgb != NULL)
		lgb->tlb_errno = output_ret;
}

/*
 * Create a new TCP control block, making an empty reassembly queue and hooking
 * it to the argument protocol control block.  The `inp' parameter must have
 * come from the zone allocator set up by tcpcbstor declaration.
 */
struct tcpcb *
tcp_newtcpcb(struct inpcb *inp)
{
	struct tcpcb *tp = intotcpcb(inp);
#ifdef INET6
	int isipv6 = (inp->inp_vflag & INP_IPV6) != 0;
#endif /* INET6 */

	/*
	 * Historically allocation was done with M_ZERO.  There is a lot of
	 * code that rely on that.  For now take safe approach and zero whole
	 * tcpcb.  This definitely can be optimized.
	 */
	bzero(&tp->t_start_zero, t_zero_size);

	/* Initialise cc_var struct for this tcpcb. */
	tp->t_ccv.type = IPPROTO_TCP;
	tp->t_ccv.ccvc.tcp = tp;
	rw_rlock(&tcp_function_lock);
	tp->t_fb = V_tcp_func_set_ptr;
	refcount_acquire(&tp->t_fb->tfb_refcnt);
	rw_runlock(&tcp_function_lock);
	/*
	 * Use the current system default CC algorithm.
	 */
	cc_attach(tp, CC_DEFAULT_ALGO());

	if (CC_ALGO(tp)->cb_init != NULL)
		if (CC_ALGO(tp)->cb_init(&tp->t_ccv, NULL) > 0) {
			cc_detach(tp);
			if (tp->t_fb->tfb_tcp_fb_fini)
				(*tp->t_fb->tfb_tcp_fb_fini)(tp, 1);
			refcount_release(&tp->t_fb->tfb_refcnt);
			return (NULL);
		}

#ifdef TCP_HHOOK
	if (khelp_init_osd(HELPER_CLASS_TCP, &tp->t_osd)) {
		if (tp->t_fb->tfb_tcp_fb_fini)
			(*tp->t_fb->tfb_tcp_fb_fini)(tp, 1);
		refcount_release(&tp->t_fb->tfb_refcnt);
		return (NULL);
	}
#endif

	TAILQ_INIT(&tp->t_segq);
	STAILQ_INIT(&tp->t_inqueue);
	tp->t_maxseg =
#ifdef INET6
		isipv6 ? V_tcp_v6mssdflt :
#endif /* INET6 */
		V_tcp_mssdflt;

	/* All mbuf queue/ack compress flags should be off */
	tcp_lro_features_off(tp);

	tp->t_hpts_cpu = HPTS_CPU_NONE;
	tp->t_lro_cpu = HPTS_CPU_NONE;

	callout_init_rw(&tp->t_callout, &inp->inp_lock, CALLOUT_RETURNUNLOCKED);
	for (int i = 0; i < TT_N; i++)
		tp->t_timers[i] = SBT_MAX;

	switch (V_tcp_do_rfc1323) {
		case 0:
			break;
		default:
		case 1:
			tp->t_flags = (TF_REQ_SCALE|TF_REQ_TSTMP);
			break;
		case 2:
			tp->t_flags = TF_REQ_SCALE;
			break;
		case 3:
			tp->t_flags = TF_REQ_TSTMP;
			break;
	}
	if (V_tcp_do_sack)
		tp->t_flags |= TF_SACK_PERMIT;
	TAILQ_INIT(&tp->snd_holes);

	/*
	 * Init srtt to TCPTV_SRTTBASE (0), so we can tell that we have no
	 * rtt estimate.  Set rttvar so that srtt + 4 * rttvar gives
	 * reasonable initial retransmit time.
	 */
	tp->t_srtt = TCPTV_SRTTBASE;
	tp->t_rttvar = ((tcp_rexmit_initial - TCPTV_SRTTBASE) << TCP_RTTVAR_SHIFT) / 4;
	tp->t_rttmin = tcp_rexmit_min;
	tp->t_rxtcur = tcp_rexmit_initial;
	tp->snd_cwnd = TCP_MAXWIN << TCP_MAX_WINSHIFT;
	tp->snd_ssthresh = TCP_MAXWIN << TCP_MAX_WINSHIFT;
	tp->t_rcvtime = ticks;
	/* We always start with ticks granularity */
	tp->t_tmr_granularity = TCP_TMR_GRANULARITY_TICKS;
	/*
	 * IPv4 TTL initialization is necessary for an IPv6 socket as well,
	 * because the socket may be bound to an IPv6 wildcard address,
	 * which may match an IPv4-mapped IPv6 address.
	 */
	inp->inp_ip_ttl = V_ip_defttl;
#ifdef TCPPCAP
	/*
	 * Init the TCP PCAP queues.
	 */
	tcp_pcap_tcpcb_init(tp);
#endif
#ifdef TCP_BLACKBOX
	/* Initialize the per-TCPCB log data. */
	tcp_log_tcpcbinit(tp);
#endif
	tp->t_pacing_rate = -1;
	if (tp->t_fb->tfb_tcp_fb_init) {
		if ((*tp->t_fb->tfb_tcp_fb_init)(tp, &tp->t_fb_ptr)) {
			refcount_release(&tp->t_fb->tfb_refcnt);
			return (NULL);
		}
	}
#ifdef STATS
	if (V_tcp_perconn_stats_enable == 1)
		tp->t_stats = stats_blob_alloc(V_tcp_perconn_stats_dflt_tpl, 0);
#endif
	if (V_tcp_do_lrd)
		tp->t_flags |= TF_LRD;

	return (tp);
}

/*
 * Drop a TCP connection, reporting
 * the specified error.  If connection is synchronized,
 * then send a RST to peer.
 */
struct tcpcb *
tcp_drop(struct tcpcb *tp, int errno)
{
	struct socket *so = tptosocket(tp);

	NET_EPOCH_ASSERT();
	INP_WLOCK_ASSERT(tptoinpcb(tp));

	if (TCPS_HAVERCVDSYN(tp->t_state)) {
		tcp_state_change(tp, TCPS_CLOSED);
		/* Don't use tcp_output() here due to possible recursion. */
		(void)tcp_output_nodrop(tp);
		TCPSTAT_INC(tcps_drops);
	} else
		TCPSTAT_INC(tcps_conndrops);
	if (errno == ETIMEDOUT && tp->t_softerror)
		errno = tp->t_softerror;
	so->so_error = errno;
	return (tcp_close(tp));
}

void
tcp_discardcb(struct tcpcb *tp)
{
	struct inpcb *inp = tptoinpcb(tp);
	struct socket *so = tptosocket(tp);
	struct mbuf *m;
#ifdef INET6
	bool isipv6 = (inp->inp_vflag & INP_IPV6) != 0;
#endif

	INP_WLOCK_ASSERT(inp);
	MPASS(!callout_active(&tp->t_callout));
	MPASS(TAILQ_EMPTY(&tp->snd_holes));

	/* free the reassembly queue, if any */
	tcp_reass_flush(tp);

#ifdef TCP_OFFLOAD
	/* Disconnect offload device, if any. */
	if (tp->t_flags & TF_TOE)
		tcp_offload_detach(tp);
#endif
#ifdef TCPPCAP
	/* Free the TCP PCAP queues. */
	tcp_pcap_drain(&(tp->t_inpkts));
	tcp_pcap_drain(&(tp->t_outpkts));
#endif

	/* Allow the CC algorithm to clean up after itself. */
	if (CC_ALGO(tp)->cb_destroy != NULL)
		CC_ALGO(tp)->cb_destroy(&tp->t_ccv);
	CC_DATA(tp) = NULL;
	/* Detach from the CC algorithm */
	cc_detach(tp);

#ifdef TCP_HHOOK
	khelp_destroy_osd(&tp->t_osd);
#endif
#ifdef STATS
	stats_blob_destroy(tp->t_stats);
#endif

	CC_ALGO(tp) = NULL;
	if ((m = STAILQ_FIRST(&tp->t_inqueue)) != NULL) {
		struct mbuf *prev;

		STAILQ_INIT(&tp->t_inqueue);
		STAILQ_FOREACH_FROM_SAFE(m, &tp->t_inqueue, m_stailqpkt, prev)
			m_freem(m);
	}
	TCPSTATES_DEC(tp->t_state);

	if (tp->t_fb->tfb_tcp_fb_fini)
		(*tp->t_fb->tfb_tcp_fb_fini)(tp, 1);
	MPASS(!tcp_in_hpts(tp));
#ifdef TCP_BLACKBOX
	tcp_log_tcpcbfini(tp);
#endif

	/*
	 * If we got enough samples through the srtt filter,
	 * save the rtt and rttvar in the routing entry.
	 * 'Enough' is arbitrarily defined as 4 rtt samples.
	 * 4 samples is enough for the srtt filter to converge
	 * to within enough % of the correct value; fewer samples
	 * and we could save a bogus rtt. The danger is not high
	 * as tcp quickly recovers from everything.
	 * XXX: Works very well but needs some more statistics!
	 *
	 * XXXRRS: Updating must be after the stack fini() since
	 * that may be converting some internal representation of
	 * say srtt etc into the general one used by other stacks.
	 * Lets also at least protect against the so being NULL
	 * as RW stated below.
	 */
	if ((tp->t_rttupdated >= 4) && (so != NULL)) {
		struct hc_metrics_lite metrics;
		uint32_t ssthresh;

		bzero(&metrics, sizeof(metrics));
		/*
		 * Update the ssthresh always when the conditions below
		 * are satisfied. This gives us better new start value
		 * for the congestion avoidance for new connections.
		 * ssthresh is only set if packet loss occurred on a session.
		 *
		 * XXXRW: 'so' may be NULL here, and/or socket buffer may be
		 * being torn down.  Ideally this code would not use 'so'.
		 */
		ssthresh = tp->snd_ssthresh;
		if (ssthresh != 0 && ssthresh < so->so_snd.sb_hiwat / 2) {
			/*
			 * convert the limit from user data bytes to
			 * packets then to packet data bytes.
			 */
			ssthresh = (ssthresh + tp->t_maxseg / 2) / tp->t_maxseg;
			if (ssthresh < 2)
				ssthresh = 2;
			ssthresh *= (tp->t_maxseg +
#ifdef INET6
			    (isipv6 ? sizeof (struct ip6_hdr) +
			    sizeof (struct tcphdr) :
#endif
			    sizeof (struct tcpiphdr)
#ifdef INET6
			    )
#endif
			    );
		} else
			ssthresh = 0;
		metrics.rmx_ssthresh = ssthresh;

		metrics.rmx_rtt = tp->t_srtt;
		metrics.rmx_rttvar = tp->t_rttvar;
		metrics.rmx_cwnd = tp->snd_cwnd;
		metrics.rmx_sendpipe = 0;
		metrics.rmx_recvpipe = 0;

		tcp_hc_update(&inp->inp_inc, &metrics);
	}

	refcount_release(&tp->t_fb->tfb_refcnt);
}

/*
 * Attempt to close a TCP control block, marking it as dropped, and freeing
 * the socket if we hold the only reference.
 */
struct tcpcb *
tcp_close(struct tcpcb *tp)
{
	struct inpcb *inp = tptoinpcb(tp);
	struct socket *so = tptosocket(tp);

	INP_WLOCK_ASSERT(inp);

#ifdef TCP_OFFLOAD
	if (tp->t_state == TCPS_LISTEN)
		tcp_offload_listen_stop(tp);
#endif
	/*
	 * This releases the TFO pending counter resource for TFO listen
	 * sockets as well as passively-created TFO sockets that transition
	 * from SYN_RECEIVED to CLOSED.
	 */
	if (tp->t_tfo_pending) {
		tcp_fastopen_decrement_counter(tp->t_tfo_pending);
		tp->t_tfo_pending = NULL;
	}
	tcp_timer_stop(tp);
	if (tp->t_fb->tfb_tcp_timer_stop_all != NULL)
		tp->t_fb->tfb_tcp_timer_stop_all(tp);
	in_pcbdrop(inp);
	TCPSTAT_INC(tcps_closed);
	if (tp->t_state != TCPS_CLOSED)
		tcp_state_change(tp, TCPS_CLOSED);
	KASSERT(inp->inp_socket != NULL, ("tcp_close: inp_socket NULL"));
	tcp_free_sackholes(tp);
	soisdisconnected(so);
	if (inp->inp_flags & INP_SOCKREF) {
		inp->inp_flags &= ~INP_SOCKREF;
		INP_WUNLOCK(inp);
		sorele(so);
		return (NULL);
	}
	return (tp);
}

/*
 * Notify a tcp user of an asynchronous error;
 * store error as soft error, but wake up user
 * (for now, won't do anything until can select for soft error).
 *
 * Do not wake up user since there currently is no mechanism for
 * reporting soft errors (yet - a kqueue filter may be added).
 */
static struct inpcb *
tcp_notify(struct inpcb *inp, int error)
{
	struct tcpcb *tp;

	INP_WLOCK_ASSERT(inp);

	tp = intotcpcb(inp);
	KASSERT(tp != NULL, ("tcp_notify: tp == NULL"));

	/*
	 * Ignore some errors if we are hooked up.
	 * If connection hasn't completed, has retransmitted several times,
	 * and receives a second error, give up now.  This is better
	 * than waiting a long time to establish a connection that
	 * can never complete.
	 */
	if (tp->t_state == TCPS_ESTABLISHED &&
	    (error == EHOSTUNREACH || error == ENETUNREACH ||
	     error == EHOSTDOWN)) {
		if (inp->inp_route.ro_nh) {
			NH_FREE(inp->inp_route.ro_nh);
			inp->inp_route.ro_nh = (struct nhop_object *)NULL;
		}
		return (inp);
	} else if (tp->t_state < TCPS_ESTABLISHED && tp->t_rxtshift > 3 &&
	    tp->t_softerror) {
		tp = tcp_drop(tp, error);
		if (tp != NULL)
			return (inp);
		else
			return (NULL);
	} else {
		tp->t_softerror = error;
		return (inp);
	}
#if 0
	wakeup( &so->so_timeo);
	sorwakeup(so);
	sowwakeup(so);
#endif
}

static int
tcp_pcblist(SYSCTL_HANDLER_ARGS)
{
	struct inpcb_iterator inpi = INP_ALL_ITERATOR(&V_tcbinfo,
	    INPLOOKUP_RLOCKPCB);
	struct xinpgen xig;
	struct inpcb *inp;
	int error;

	if (req->newptr != NULL)
		return (EPERM);

	if (req->oldptr == NULL) {
		int n;

		n = V_tcbinfo.ipi_count +
		    counter_u64_fetch(V_tcps_states[TCPS_SYN_RECEIVED]);
		n += imax(n / 8, 10);
		req->oldidx = 2 * (sizeof xig) + n * sizeof(struct xtcpcb);
		return (0);
	}

	if ((error = sysctl_wire_old_buffer(req, 0)) != 0)
		return (error);

	bzero(&xig, sizeof(xig));
	xig.xig_len = sizeof xig;
	xig.xig_count = V_tcbinfo.ipi_count +
	    counter_u64_fetch(V_tcps_states[TCPS_SYN_RECEIVED]);
	xig.xig_gen = V_tcbinfo.ipi_gencnt;
	xig.xig_sogen = so_gencnt;
	error = SYSCTL_OUT(req, &xig, sizeof xig);
	if (error)
		return (error);

	error = syncache_pcblist(req);
	if (error)
		return (error);

	while ((inp = inp_next(&inpi)) != NULL) {
		if (inp->inp_gencnt <= xig.xig_gen &&
		    cr_canseeinpcb(req->td->td_ucred, inp) == 0) {
			struct xtcpcb xt;

			tcp_inptoxtp(inp, &xt);
			error = SYSCTL_OUT(req, &xt, sizeof xt);
			if (error) {
				INP_RUNLOCK(inp);
				break;
			} else
				continue;
		}
	}

	if (!error) {
		/*
		 * Give the user an updated idea of our state.
		 * If the generation differs from what we told
		 * her before, she knows that something happened
		 * while we were processing this request, and it
		 * might be necessary to retry.
		 */
		xig.xig_gen = V_tcbinfo.ipi_gencnt;
		xig.xig_sogen = so_gencnt;
		xig.xig_count = V_tcbinfo.ipi_count +
		    counter_u64_fetch(V_tcps_states[TCPS_SYN_RECEIVED]);
		error = SYSCTL_OUT(req, &xig, sizeof xig);
	}

	return (error);
}

SYSCTL_PROC(_net_inet_tcp, TCPCTL_PCBLIST, pcblist,
    CTLTYPE_OPAQUE | CTLFLAG_RD | CTLFLAG_NEEDGIANT,
    NULL, 0, tcp_pcblist, "S,xtcpcb",
    "List of active TCP connections");

#ifdef INET
static int
tcp_getcred(SYSCTL_HANDLER_ARGS)
{
	struct xucred xuc;
	struct sockaddr_in addrs[2];
	struct epoch_tracker et;
	struct inpcb *inp;
	int error;

	error = priv_check(req->td, PRIV_NETINET_GETCRED);
	if (error)
		return (error);
	error = SYSCTL_IN(req, addrs, sizeof(addrs));
	if (error)
		return (error);
	NET_EPOCH_ENTER(et);
	inp = in_pcblookup(&V_tcbinfo, addrs[1].sin_addr, addrs[1].sin_port,
	    addrs[0].sin_addr, addrs[0].sin_port, INPLOOKUP_RLOCKPCB, NULL);
	NET_EPOCH_EXIT(et);
	if (inp != NULL) {
		if (error == 0)
			error = cr_canseeinpcb(req->td->td_ucred, inp);
		if (error == 0)
			cru2x(inp->inp_cred, &xuc);
		INP_RUNLOCK(inp);
	} else
		error = ENOENT;
	if (error == 0)
		error = SYSCTL_OUT(req, &xuc, sizeof(struct xucred));
	return (error);
}

SYSCTL_PROC(_net_inet_tcp, OID_AUTO, getcred,
    CTLTYPE_OPAQUE | CTLFLAG_RW | CTLFLAG_PRISON | CTLFLAG_NEEDGIANT,
    0, 0, tcp_getcred, "S,xucred",
    "Get the xucred of a TCP connection");
#endif /* INET */

#ifdef INET6
static int
tcp6_getcred(SYSCTL_HANDLER_ARGS)
{
	struct epoch_tracker et;
	struct xucred xuc;
	struct sockaddr_in6 addrs[2];
	struct inpcb *inp;
	int error;
#ifdef INET
	int mapped = 0;
#endif

	error = priv_check(req->td, PRIV_NETINET_GETCRED);
	if (error)
		return (error);
	error = SYSCTL_IN(req, addrs, sizeof(addrs));
	if (error)
		return (error);
	if ((error = sa6_embedscope(&addrs[0], V_ip6_use_defzone)) != 0 ||
	    (error = sa6_embedscope(&addrs[1], V_ip6_use_defzone)) != 0) {
		return (error);
	}
	if (IN6_IS_ADDR_V4MAPPED(&addrs[0].sin6_addr)) {
#ifdef INET
		if (IN6_IS_ADDR_V4MAPPED(&addrs[1].sin6_addr))
			mapped = 1;
		else
#endif
			return (EINVAL);
	}

	NET_EPOCH_ENTER(et);
#ifdef INET
	if (mapped == 1)
		inp = in_pcblookup(&V_tcbinfo,
			*(struct in_addr *)&addrs[1].sin6_addr.s6_addr[12],
			addrs[1].sin6_port,
			*(struct in_addr *)&addrs[0].sin6_addr.s6_addr[12],
			addrs[0].sin6_port, INPLOOKUP_RLOCKPCB, NULL);
	else
#endif
		inp = in6_pcblookup(&V_tcbinfo,
			&addrs[1].sin6_addr, addrs[1].sin6_port,
			&addrs[0].sin6_addr, addrs[0].sin6_port,
			INPLOOKUP_RLOCKPCB, NULL);
	NET_EPOCH_EXIT(et);
	if (inp != NULL) {
		if (error == 0)
			error = cr_canseeinpcb(req->td->td_ucred, inp);
		if (error == 0)
			cru2x(inp->inp_cred, &xuc);
		INP_RUNLOCK(inp);
	} else
		error = ENOENT;
	if (error == 0)
		error = SYSCTL_OUT(req, &xuc, sizeof(struct xucred));
	return (error);
}

SYSCTL_PROC(_net_inet6_tcp6, OID_AUTO, getcred,
    CTLTYPE_OPAQUE | CTLFLAG_RW | CTLFLAG_PRISON | CTLFLAG_NEEDGIANT,
    0, 0, tcp6_getcred, "S,xucred",
    "Get the xucred of a TCP6 connection");
#endif /* INET6 */

#ifdef INET
/* Path MTU to try next when a fragmentation-needed message is received. */
static inline int
tcp_next_pmtu(const struct icmp *icp, const struct ip *ip)
{
	int mtu = ntohs(icp->icmp_nextmtu);

	/* If no alternative MTU was proposed, try the next smaller one. */
	if (!mtu)
		mtu = ip_next_mtu(ntohs(ip->ip_len), 1);
	if (mtu < V_tcp_minmss + sizeof(struct tcpiphdr))
		mtu = V_tcp_minmss + sizeof(struct tcpiphdr);

	return (mtu);
}

static void
tcp_ctlinput_with_port(struct icmp *icp, uint16_t port)
{
	struct ip *ip;
	struct tcphdr *th;
	struct inpcb *inp;
	struct tcpcb *tp;
	struct inpcb *(*notify)(struct inpcb *, int);
	struct in_conninfo inc;
	tcp_seq icmp_tcp_seq;
	int errno, mtu;

	errno = icmp_errmap(icp);
	switch (errno) {
	case 0:
		return;
	case EMSGSIZE:
		notify = tcp_mtudisc_notify;
		break;
	case ECONNREFUSED:
		if (V_icmp_may_rst)
			notify = tcp_drop_syn_sent;
		else
			notify = tcp_notify;
		break;
	case EHOSTUNREACH:
		if (V_icmp_may_rst && icp->icmp_type == ICMP_TIMXCEED)
			notify = tcp_drop_syn_sent;
		else
			notify = tcp_notify;
		break;
	default:
		notify = tcp_notify;
	}

	ip = &icp->icmp_ip;
	th = (struct tcphdr *)((caddr_t)ip + (ip->ip_hl << 2));
	icmp_tcp_seq = th->th_seq;
	inp = in_pcblookup(&V_tcbinfo, ip->ip_dst, th->th_dport, ip->ip_src,
	    th->th_sport, INPLOOKUP_WLOCKPCB, NULL);
	if (inp != NULL)  {
		tp = intotcpcb(inp);
#ifdef TCP_OFFLOAD
		if (tp->t_flags & TF_TOE && errno == EMSGSIZE) {
			/*
			 * MTU discovery for offloaded connections.  Let
			 * the TOE driver verify seq# and process it.
			 */
			mtu = tcp_next_pmtu(icp, ip);
			tcp_offload_pmtu_update(tp, icmp_tcp_seq, mtu);
			goto out;
		}
#endif
		if (tp->t_port != port)
			goto out;
		if (SEQ_GEQ(ntohl(icmp_tcp_seq), tp->snd_una) &&
		    SEQ_LT(ntohl(icmp_tcp_seq), tp->snd_max)) {
			if (errno == EMSGSIZE) {
				/*
				 * MTU discovery: we got a needfrag and
				 * will potentially try a lower MTU.
				 */
				mtu = tcp_next_pmtu(icp, ip);

				/*
				 * Only process the offered MTU if it
				 * is smaller than the current one.
				 */
				if (mtu < tp->t_maxseg +
				    sizeof(struct tcpiphdr)) {
					bzero(&inc, sizeof(inc));
					inc.inc_faddr = ip->ip_dst;
					inc.inc_fibnum =
					    inp->inp_inc.inc_fibnum;
					tcp_hc_updatemtu(&inc, mtu);
					inp = tcp_mtudisc(inp, mtu);
				}
			} else
				inp = (*notify)(inp, errno);
		}
	} else {
		bzero(&inc, sizeof(inc));
		inc.inc_fport = th->th_dport;
		inc.inc_lport = th->th_sport;
		inc.inc_faddr = ip->ip_dst;
		inc.inc_laddr = ip->ip_src;
		syncache_unreach(&inc, icmp_tcp_seq, port);
	}
out:
	if (inp != NULL)
		INP_WUNLOCK(inp);
}

static void
tcp_ctlinput(struct icmp *icmp)
{
	tcp_ctlinput_with_port(icmp, htons(0));
}

static void
tcp_ctlinput_viaudp(udp_tun_icmp_param_t param)
{
	/* Its a tunneled TCP over UDP icmp */
	struct icmp *icmp = param.icmp;
	struct ip *outer_ip, *inner_ip;
	struct udphdr *udp;
	struct tcphdr *th, ttemp;
	int i_hlen, o_len;
	uint16_t port;

	outer_ip = (struct ip *)((caddr_t)icmp - sizeof(struct ip));
	inner_ip = &icmp->icmp_ip;
	i_hlen = inner_ip->ip_hl << 2;
	o_len = ntohs(outer_ip->ip_len);
	if (o_len <
	    (sizeof(struct ip) + 8 + i_hlen + sizeof(struct udphdr) + offsetof(struct tcphdr, th_ack))) {
		/* Not enough data present */
		return;
	}
	/* Ok lets strip out the inner udphdr header by copying up on top of it the tcp hdr */
	udp = (struct udphdr *)(((caddr_t)inner_ip) + i_hlen);
	if (ntohs(udp->uh_sport) != V_tcp_udp_tunneling_port) {
		return;
	}
	port = udp->uh_dport;
	th = (struct tcphdr *)(udp + 1);
	memcpy(&ttemp, th, sizeof(struct tcphdr));
	memcpy(udp, &ttemp, sizeof(struct tcphdr));
	/* Now adjust down the size of the outer IP header */
	o_len -= sizeof(struct udphdr);
	outer_ip->ip_len = htons(o_len);
	/* Now call in to the normal handling code */
	tcp_ctlinput_with_port(icmp, port);
}
#endif /* INET */

#ifdef INET6
static inline int
tcp6_next_pmtu(const struct icmp6_hdr *icmp6)
{
	int mtu = ntohl(icmp6->icmp6_mtu);

	/*
	 * If no alternative MTU was proposed, or the proposed MTU was too
	 * small, set to the min.
	 */
	if (mtu < IPV6_MMTU)
		mtu = IPV6_MMTU - 8;	/* XXXNP: what is the adjustment for? */
	return (mtu);
}

static void
tcp6_ctlinput_with_port(struct ip6ctlparam *ip6cp, uint16_t port)
{
	struct in6_addr *dst;
	struct inpcb *(*notify)(struct inpcb *, int);
	struct ip6_hdr *ip6;
	struct mbuf *m;
	struct inpcb *inp;
	struct tcpcb *tp;
	struct icmp6_hdr *icmp6;
	struct in_conninfo inc;
	struct tcp_ports {
		uint16_t th_sport;
		uint16_t th_dport;
	} t_ports;
	tcp_seq icmp_tcp_seq;
	unsigned int mtu;
	unsigned int off;
	int errno;

	icmp6 = ip6cp->ip6c_icmp6;
	m = ip6cp->ip6c_m;
	ip6 = ip6cp->ip6c_ip6;
	off = ip6cp->ip6c_off;
	dst = &ip6cp->ip6c_finaldst->sin6_addr;

	errno = icmp6_errmap(icmp6);
	switch (errno) {
	case 0:
		return;
	case EMSGSIZE:
		notify = tcp_mtudisc_notify;
		break;
	case ECONNREFUSED:
		if (V_icmp_may_rst)
			notify = tcp_drop_syn_sent;
		else
			notify = tcp_notify;
		break;
	case EHOSTUNREACH:
		/*
		 * There are only four ICMPs that may reset connection:
		 * - administratively prohibited
		 * - port unreachable
		 * - time exceeded in transit
		 * - unknown next header
		 */
		if (V_icmp_may_rst &&
		    ((icmp6->icmp6_type == ICMP6_DST_UNREACH &&
		     (icmp6->icmp6_code == ICMP6_DST_UNREACH_ADMIN ||
		      icmp6->icmp6_code == ICMP6_DST_UNREACH_NOPORT)) ||
		    (icmp6->icmp6_type == ICMP6_TIME_EXCEEDED &&
		      icmp6->icmp6_code == ICMP6_TIME_EXCEED_TRANSIT) ||
		    (icmp6->icmp6_type == ICMP6_PARAM_PROB &&
		      icmp6->icmp6_code == ICMP6_PARAMPROB_NEXTHEADER)))
			notify = tcp_drop_syn_sent;
		else
			notify = tcp_notify;
		break;
	default:
		notify = tcp_notify;
	}

	/* Check if we can safely get the ports from the tcp hdr */
	if (m == NULL ||
	    (m->m_pkthdr.len <
		(int32_t) (off + sizeof(struct tcp_ports)))) {
		return;
	}
	bzero(&t_ports, sizeof(struct tcp_ports));
	m_copydata(m, off, sizeof(struct tcp_ports), (caddr_t)&t_ports);
	inp = in6_pcblookup(&V_tcbinfo, &ip6->ip6_dst, t_ports.th_dport,
	    &ip6->ip6_src, t_ports.th_sport, INPLOOKUP_WLOCKPCB, NULL);
	off += sizeof(struct tcp_ports);
	if (m->m_pkthdr.len < (int32_t) (off + sizeof(tcp_seq))) {
		goto out;
	}
	m_copydata(m, off, sizeof(tcp_seq), (caddr_t)&icmp_tcp_seq);
	if (inp != NULL)  {
		tp = intotcpcb(inp);
#ifdef TCP_OFFLOAD
		if (tp->t_flags & TF_TOE && errno == EMSGSIZE) {
			/* MTU discovery for offloaded connections. */
			mtu = tcp6_next_pmtu(icmp6);
			tcp_offload_pmtu_update(tp, icmp_tcp_seq, mtu);
			goto out;
		}
#endif
		if (tp->t_port != port)
			goto out;
		if (SEQ_GEQ(ntohl(icmp_tcp_seq), tp->snd_una) &&
		    SEQ_LT(ntohl(icmp_tcp_seq), tp->snd_max)) {
			if (errno == EMSGSIZE) {
				/*
				 * MTU discovery:
				 * If we got a needfrag set the MTU
				 * in the route to the suggested new
				 * value (if given) and then notify.
				 */
				mtu = tcp6_next_pmtu(icmp6);

				bzero(&inc, sizeof(inc));
				inc.inc_fibnum = M_GETFIB(m);
				inc.inc_flags |= INC_ISIPV6;
				inc.inc6_faddr = *dst;
				if (in6_setscope(&inc.inc6_faddr,
					m->m_pkthdr.rcvif, NULL))
					goto out;
				/*
				 * Only process the offered MTU if it
				 * is smaller than the current one.
				 */
				if (mtu < tp->t_maxseg +
				    sizeof (struct tcphdr) +
				    sizeof (struct ip6_hdr)) {
					tcp_hc_updatemtu(&inc, mtu);
					tcp_mtudisc(inp, mtu);
					ICMP6STAT_INC(icp6s_pmtuchg);
				}
			} else
				inp = (*notify)(inp, errno);
		}
	} else {
		bzero(&inc, sizeof(inc));
		inc.inc_fibnum = M_GETFIB(m);
		inc.inc_flags |= INC_ISIPV6;
		inc.inc_fport = t_ports.th_dport;
		inc.inc_lport = t_ports.th_sport;
		inc.inc6_faddr = *dst;
		inc.inc6_laddr = ip6->ip6_src;
		syncache_unreach(&inc, icmp_tcp_seq, port);
	}
out:
	if (inp != NULL)
		INP_WUNLOCK(inp);
}

static void
tcp6_ctlinput(struct ip6ctlparam *ctl)
{
	tcp6_ctlinput_with_port(ctl, htons(0));
}

static void
tcp6_ctlinput_viaudp(udp_tun_icmp_param_t param)
{
	struct ip6ctlparam *ip6cp = param.ip6cp;
	struct mbuf *m;
	struct udphdr *udp;
	uint16_t port;

	m = m_pulldown(ip6cp->ip6c_m, ip6cp->ip6c_off, sizeof(struct udphdr), NULL);
	if (m == NULL) {
		return;
	}
	udp = mtod(m, struct udphdr *);
	if (ntohs(udp->uh_sport) != V_tcp_udp_tunneling_port) {
		return;
	}
	port = udp->uh_dport;
	m_adj(m, sizeof(struct udphdr));
	if ((m->m_flags & M_PKTHDR) == 0) {
		ip6cp->ip6c_m->m_pkthdr.len -= sizeof(struct udphdr);
	}
	/* Now call in to the normal handling code */
	tcp6_ctlinput_with_port(ip6cp, port);
}

#endif /* INET6 */

static uint32_t
tcp_keyed_hash(struct in_conninfo *inc, u_char *key, u_int len)
{
	SIPHASH_CTX ctx;
	uint32_t hash[2];

	KASSERT(len >= SIPHASH_KEY_LENGTH,
	    ("%s: keylen %u too short ", __func__, len));
	SipHash24_Init(&ctx);
	SipHash_SetKey(&ctx, (uint8_t *)key);
	SipHash_Update(&ctx, &inc->inc_fport, sizeof(uint16_t));
	SipHash_Update(&ctx, &inc->inc_lport, sizeof(uint16_t));
	switch (inc->inc_flags & INC_ISIPV6) {
#ifdef INET
	case 0:
		SipHash_Update(&ctx, &inc->inc_faddr, sizeof(struct in_addr));
		SipHash_Update(&ctx, &inc->inc_laddr, sizeof(struct in_addr));
		break;
#endif
#ifdef INET6
	case INC_ISIPV6:
		SipHash_Update(&ctx, &inc->inc6_faddr, sizeof(struct in6_addr));
		SipHash_Update(&ctx, &inc->inc6_laddr, sizeof(struct in6_addr));
		break;
#endif
	}
	SipHash_Final((uint8_t *)hash, &ctx);

	return (hash[0] ^ hash[1]);
}

uint32_t
tcp_new_ts_offset(struct in_conninfo *inc)
{
	struct in_conninfo inc_store, *local_inc;

	if (!V_tcp_ts_offset_per_conn) {
		memcpy(&inc_store, inc, sizeof(struct in_conninfo));
		inc_store.inc_lport = 0;
		inc_store.inc_fport = 0;
		local_inc = &inc_store;
	} else {
		local_inc = inc;
	}
	return (tcp_keyed_hash(local_inc, V_ts_offset_secret,
	    sizeof(V_ts_offset_secret)));
}

/*
 * Following is where TCP initial sequence number generation occurs.
 *
 * There are two places where we must use initial sequence numbers:
 * 1.  In SYN-ACK packets.
 * 2.  In SYN packets.
 *
 * All ISNs for SYN-ACK packets are generated by the syncache.  See
 * tcp_syncache.c for details.
 *
 * The ISNs in SYN packets must be monotonic; TIME_WAIT recycling
 * depends on this property.  In addition, these ISNs should be
 * unguessable so as to prevent connection hijacking.  To satisfy
 * the requirements of this situation, the algorithm outlined in
 * RFC 1948 is used, with only small modifications.
 *
 * Implementation details:
 *
 * Time is based off the system timer, and is corrected so that it
 * increases by one megabyte per second.  This allows for proper
 * recycling on high speed LANs while still leaving over an hour
 * before rollover.
 *
 * As reading the *exact* system time is too expensive to be done
 * whenever setting up a TCP connection, we increment the time
 * offset in two ways.  First, a small random positive increment
 * is added to isn_offset for each connection that is set up.
 * Second, the function tcp_isn_tick fires once per clock tick
 * and increments isn_offset as necessary so that sequence numbers
 * are incremented at approximately ISN_BYTES_PER_SECOND.  The
 * random positive increments serve only to ensure that the same
 * exact sequence number is never sent out twice (as could otherwise
 * happen when a port is recycled in less than the system tick
 * interval.)
 *
 * net.inet.tcp.isn_reseed_interval controls the number of seconds
 * between seeding of isn_secret.  This is normally set to zero,
 * as reseeding should not be necessary.
 *
 * Locking of the global variables isn_secret, isn_last_reseed, isn_offset,
 * isn_offset_old, and isn_ctx is performed using the ISN lock.  In
 * general, this means holding an exclusive (write) lock.
 */

#define ISN_BYTES_PER_SECOND 1048576
#define ISN_STATIC_INCREMENT 4096
#define ISN_RANDOM_INCREMENT (4096 - 1)
#define ISN_SECRET_LENGTH    SIPHASH_KEY_LENGTH

VNET_DEFINE_STATIC(u_char, isn_secret[ISN_SECRET_LENGTH]);
VNET_DEFINE_STATIC(int, isn_last);
VNET_DEFINE_STATIC(int, isn_last_reseed);
VNET_DEFINE_STATIC(u_int32_t, isn_offset);
VNET_DEFINE_STATIC(u_int32_t, isn_offset_old);

#define	V_isn_secret			VNET(isn_secret)
#define	V_isn_last			VNET(isn_last)
#define	V_isn_last_reseed		VNET(isn_last_reseed)
#define	V_isn_offset			VNET(isn_offset)
#define	V_isn_offset_old		VNET(isn_offset_old)

tcp_seq
tcp_new_isn(struct in_conninfo *inc)
{
	tcp_seq new_isn;
	u_int32_t projected_offset;

	ISN_LOCK();
	/* Seed if this is the first use, reseed if requested. */
	if ((V_isn_last_reseed == 0) || ((V_tcp_isn_reseed_interval > 0) &&
	     (((u_int)V_isn_last_reseed + (u_int)V_tcp_isn_reseed_interval*hz)
		< (u_int)ticks))) {
		arc4rand(&V_isn_secret, sizeof(V_isn_secret), 0);
		V_isn_last_reseed = ticks;
	}

	/* Compute the hash and return the ISN. */
	new_isn = (tcp_seq)tcp_keyed_hash(inc, V_isn_secret,
	    sizeof(V_isn_secret));
	V_isn_offset += ISN_STATIC_INCREMENT +
		(arc4random() & ISN_RANDOM_INCREMENT);
	if (ticks != V_isn_last) {
		projected_offset = V_isn_offset_old +
		    ISN_BYTES_PER_SECOND / hz * (ticks - V_isn_last);
		if (SEQ_GT(projected_offset, V_isn_offset))
			V_isn_offset = projected_offset;
		V_isn_offset_old = V_isn_offset;
		V_isn_last = ticks;
	}
	new_isn += V_isn_offset;
	ISN_UNLOCK();
	return (new_isn);
}

/*
 * When a specific ICMP unreachable message is received and the
 * connection state is SYN-SENT, drop the connection.  This behavior
 * is controlled by the icmp_may_rst sysctl.
 */
static struct inpcb *
tcp_drop_syn_sent(struct inpcb *inp, int errno)
{
	struct tcpcb *tp;

	NET_EPOCH_ASSERT();
	INP_WLOCK_ASSERT(inp);

	tp = intotcpcb(inp);
	if (tp->t_state != TCPS_SYN_SENT)
		return (inp);

	if (IS_FASTOPEN(tp->t_flags))
		tcp_fastopen_disable_path(tp);

	tp = tcp_drop(tp, errno);
	if (tp != NULL)
		return (inp);
	else
		return (NULL);
}

/*
 * When `need fragmentation' ICMP is received, update our idea of the MSS
 * based on the new value. Also nudge TCP to send something, since we
 * know the packet we just sent was dropped.
 * This duplicates some code in the tcp_mss() function in tcp_input.c.
 */
static struct inpcb *
tcp_mtudisc_notify(struct inpcb *inp, int error)
{

	return (tcp_mtudisc(inp, -1));
}

static struct inpcb *
tcp_mtudisc(struct inpcb *inp, int mtuoffer)
{
	struct tcpcb *tp;
	struct socket *so;

	INP_WLOCK_ASSERT(inp);

	tp = intotcpcb(inp);
	KASSERT(tp != NULL, ("tcp_mtudisc: tp == NULL"));

	tcp_mss_update(tp, -1, mtuoffer, NULL, NULL);

	so = inp->inp_socket;
	SOCKBUF_LOCK(&so->so_snd);
	/* If the mss is larger than the socket buffer, decrease the mss. */
	if (so->so_snd.sb_hiwat < tp->t_maxseg)
		tp->t_maxseg = so->so_snd.sb_hiwat;
	SOCKBUF_UNLOCK(&so->so_snd);

	TCPSTAT_INC(tcps_mturesent);
	tp->t_rtttime = 0;
	tp->snd_nxt = tp->snd_una;
	tcp_free_sackholes(tp);
	tp->snd_recover = tp->snd_max;
	if (tp->t_flags & TF_SACK_PERMIT)
		EXIT_FASTRECOVERY(tp->t_flags);
	if (tp->t_fb->tfb_tcp_mtu_chg != NULL) {
		/*
		 * Conceptually the snd_nxt setting
		 * and freeing sack holes should
		 * be done by the default stacks
		 * own tfb_tcp_mtu_chg().
		 */
		tp->t_fb->tfb_tcp_mtu_chg(tp);
	}
	if (tcp_output(tp) < 0)
		return (NULL);
	else
		return (inp);
}

#ifdef INET
/*
 * Look-up the routing entry to the peer of this inpcb.  If no route
 * is found and it cannot be allocated, then return 0.  This routine
 * is called by TCP routines that access the rmx structure and by
 * tcp_mss_update to get the peer/interface MTU.
 */
uint32_t
tcp_maxmtu(struct in_conninfo *inc, struct tcp_ifcap *cap)
{
	struct nhop_object *nh;
	struct ifnet *ifp;
	uint32_t maxmtu = 0;

	KASSERT(inc != NULL, ("tcp_maxmtu with NULL in_conninfo pointer"));

	if (inc->inc_faddr.s_addr != INADDR_ANY) {
		nh = fib4_lookup(inc->inc_fibnum, inc->inc_faddr, 0, NHR_NONE, 0);
		if (nh == NULL)
			return (0);

		ifp = nh->nh_ifp;
		maxmtu = nh->nh_mtu;

		/* Report additional interface capabilities. */
		if (cap != NULL) {
			if (ifp->if_capenable & IFCAP_TSO4 &&
			    ifp->if_hwassist & CSUM_TSO) {
				cap->ifcap |= CSUM_TSO;
				cap->tsomax = ifp->if_hw_tsomax;
				cap->tsomaxsegcount = ifp->if_hw_tsomaxsegcount;
				cap->tsomaxsegsize = ifp->if_hw_tsomaxsegsize;
			}
		}
	}
	return (maxmtu);
}
#endif /* INET */

#ifdef INET6
uint32_t
tcp_maxmtu6(struct in_conninfo *inc, struct tcp_ifcap *cap)
{
	struct nhop_object *nh;
	struct in6_addr dst6;
	uint32_t scopeid;
	struct ifnet *ifp;
	uint32_t maxmtu = 0;

	KASSERT(inc != NULL, ("tcp_maxmtu6 with NULL in_conninfo pointer"));

	if (inc->inc_flags & INC_IPV6MINMTU)
		return (IPV6_MMTU);

	if (!IN6_IS_ADDR_UNSPECIFIED(&inc->inc6_faddr)) {
		in6_splitscope(&inc->inc6_faddr, &dst6, &scopeid);
		nh = fib6_lookup(inc->inc_fibnum, &dst6, scopeid, NHR_NONE, 0);
		if (nh == NULL)
			return (0);

		ifp = nh->nh_ifp;
		maxmtu = nh->nh_mtu;

		/* Report additional interface capabilities. */
		if (cap != NULL) {
			if (ifp->if_capenable & IFCAP_TSO6 &&
			    ifp->if_hwassist & CSUM_TSO) {
				cap->ifcap |= CSUM_TSO;
				cap->tsomax = ifp->if_hw_tsomax;
				cap->tsomaxsegcount = ifp->if_hw_tsomaxsegcount;
				cap->tsomaxsegsize = ifp->if_hw_tsomaxsegsize;
			}
		}
	}

	return (maxmtu);
}

/*
 * Handle setsockopt(IPV6_USE_MIN_MTU) by a TCP stack.
 *
 * XXXGL: we are updating inpcb here with INC_IPV6MINMTU flag.
 * The right place to do that is ip6_setpktopt() that has just been
 * executed.  By the way it just filled ip6po_minmtu for us.
 */
void
tcp6_use_min_mtu(struct tcpcb *tp)
{
	struct inpcb *inp = tptoinpcb(tp);

	INP_WLOCK_ASSERT(inp);
	/*
	 * In case of the IPV6_USE_MIN_MTU socket
	 * option, the INC_IPV6MINMTU flag to announce
	 * a corresponding MSS during the initial
	 * handshake.  If the TCP connection is not in
	 * the front states, just reduce the MSS being
	 * used.  This avoids the sending of TCP
	 * segments which will be fragmented at the
	 * IPv6 layer.
	 */
	inp->inp_inc.inc_flags |= INC_IPV6MINMTU;
	if ((tp->t_state >= TCPS_SYN_SENT) &&
	    (inp->inp_inc.inc_flags & INC_ISIPV6)) {
		struct ip6_pktopts *opt;

		opt = inp->in6p_outputopts;
		if (opt != NULL && opt->ip6po_minmtu == IP6PO_MINMTU_ALL &&
		    tp->t_maxseg > TCP6_MSS)
			tp->t_maxseg = TCP6_MSS;
	}
}
#endif /* INET6 */

/*
 * Calculate effective SMSS per RFC5681 definition for a given TCP
 * connection at its current state, taking into account SACK and etc.
 */
u_int
tcp_maxseg(const struct tcpcb *tp)
{
	u_int optlen;

	if (tp->t_flags & TF_NOOPT)
		return (tp->t_maxseg);

	/*
	 * Here we have a simplified code from tcp_addoptions(),
	 * without a proper loop, and having most of paddings hardcoded.
	 * We might make mistakes with padding here in some edge cases,
	 * but this is harmless, since result of tcp_maxseg() is used
	 * only in cwnd and ssthresh estimations.
	 */
	if (TCPS_HAVEESTABLISHED(tp->t_state)) {
		if (tp->t_flags & TF_RCVD_TSTMP)
			optlen = TCPOLEN_TSTAMP_APPA;
		else
			optlen = 0;
#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
		if (tp->t_flags & TF_SIGNATURE)
			optlen += PADTCPOLEN(TCPOLEN_SIGNATURE);
#endif
		if ((tp->t_flags & TF_SACK_PERMIT) && tp->rcv_numsacks > 0) {
			optlen += TCPOLEN_SACKHDR;
			optlen += tp->rcv_numsacks * TCPOLEN_SACK;
			optlen = PADTCPOLEN(optlen);
		}
	} else {
		if (tp->t_flags & TF_REQ_TSTMP)
			optlen = TCPOLEN_TSTAMP_APPA;
		else
			optlen = PADTCPOLEN(TCPOLEN_MAXSEG);
		if (tp->t_flags & TF_REQ_SCALE)
			optlen += PADTCPOLEN(TCPOLEN_WINDOW);
#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
		if (tp->t_flags & TF_SIGNATURE)
			optlen += PADTCPOLEN(TCPOLEN_SIGNATURE);
#endif
		if (tp->t_flags & TF_SACK_PERMIT)
			optlen += PADTCPOLEN(TCPOLEN_SACK_PERMITTED);
	}
#undef PAD
	optlen = min(optlen, TCP_MAXOLEN);
	return (tp->t_maxseg - optlen);
}


u_int
tcp_fixed_maxseg(const struct tcpcb *tp)
{
	int optlen;

	if (tp->t_flags & TF_NOOPT)
		return (tp->t_maxseg);

	/*
	 * Here we have a simplified code from tcp_addoptions(),
	 * without a proper loop, and having most of paddings hardcoded.
	 * We only consider fixed options that we would send every
	 * time I.e. SACK is not considered. This is important
	 * for cc modules to figure out what the modulo of the
	 * cwnd should be.
	 */
#define	PAD(len)	((((len) / 4) + !!((len) % 4)) * 4)
	if (TCPS_HAVEESTABLISHED(tp->t_state)) {
		if (tp->t_flags & TF_RCVD_TSTMP)
			optlen = TCPOLEN_TSTAMP_APPA;
		else
			optlen = 0;
#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
		if (tp->t_flags & TF_SIGNATURE)
			optlen += PAD(TCPOLEN_SIGNATURE);
#endif
	} else {
		if (tp->t_flags & TF_REQ_TSTMP)
			optlen = TCPOLEN_TSTAMP_APPA;
		else
			optlen = PAD(TCPOLEN_MAXSEG);
		if (tp->t_flags & TF_REQ_SCALE)
			optlen += PAD(TCPOLEN_WINDOW);
#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
		if (tp->t_flags & TF_SIGNATURE)
			optlen += PAD(TCPOLEN_SIGNATURE);
#endif
		if (tp->t_flags & TF_SACK_PERMIT)
			optlen += PAD(TCPOLEN_SACK_PERMITTED);
	}
#undef PAD
	optlen = min(optlen, TCP_MAXOLEN);
	return (tp->t_maxseg - optlen);
}



static int
sysctl_drop(SYSCTL_HANDLER_ARGS)
{
	/* addrs[0] is a foreign socket, addrs[1] is a local one. */
	struct sockaddr_storage addrs[2];
	struct inpcb *inp;
	struct tcpcb *tp;
#ifdef INET
	struct sockaddr_in *fin = NULL, *lin = NULL;
#endif
	struct epoch_tracker et;
#ifdef INET6
	struct sockaddr_in6 *fin6, *lin6;
#endif
	int error;

	inp = NULL;
#ifdef INET6
	fin6 = lin6 = NULL;
#endif
	error = 0;

	if (req->oldptr != NULL || req->oldlen != 0)
		return (EINVAL);
	if (req->newptr == NULL)
		return (EPERM);
	if (req->newlen < sizeof(addrs))
		return (ENOMEM);
	error = SYSCTL_IN(req, &addrs, sizeof(addrs));
	if (error)
		return (error);

	switch (addrs[0].ss_family) {
#ifdef INET6
	case AF_INET6:
		fin6 = (struct sockaddr_in6 *)&addrs[0];
		lin6 = (struct sockaddr_in6 *)&addrs[1];
		if (fin6->sin6_len != sizeof(struct sockaddr_in6) ||
		    lin6->sin6_len != sizeof(struct sockaddr_in6))
			return (EINVAL);
		if (IN6_IS_ADDR_V4MAPPED(&fin6->sin6_addr)) {
			if (!IN6_IS_ADDR_V4MAPPED(&lin6->sin6_addr))
				return (EINVAL);
			in6_sin6_2_sin_in_sock((struct sockaddr *)&addrs[0]);
			in6_sin6_2_sin_in_sock((struct sockaddr *)&addrs[1]);
#ifdef INET
			fin = (struct sockaddr_in *)&addrs[0];
			lin = (struct sockaddr_in *)&addrs[1];
#endif
			break;
		}
		error = sa6_embedscope(fin6, V_ip6_use_defzone);
		if (error)
			return (error);
		error = sa6_embedscope(lin6, V_ip6_use_defzone);
		if (error)
			return (error);
		break;
#endif
#ifdef INET
	case AF_INET:
		fin = (struct sockaddr_in *)&addrs[0];
		lin = (struct sockaddr_in *)&addrs[1];
		if (fin->sin_len != sizeof(struct sockaddr_in) ||
		    lin->sin_len != sizeof(struct sockaddr_in))
			return (EINVAL);
		break;
#endif
	default:
		return (EINVAL);
	}
	NET_EPOCH_ENTER(et);
	switch (addrs[0].ss_family) {
#ifdef INET6
	case AF_INET6:
		inp = in6_pcblookup(&V_tcbinfo, &fin6->sin6_addr,
		    fin6->sin6_port, &lin6->sin6_addr, lin6->sin6_port,
		    INPLOOKUP_WLOCKPCB, NULL);
		break;
#endif
#ifdef INET
	case AF_INET:
		inp = in_pcblookup(&V_tcbinfo, fin->sin_addr, fin->sin_port,
		    lin->sin_addr, lin->sin_port, INPLOOKUP_WLOCKPCB, NULL);
		break;
#endif
	}
	if (inp != NULL) {
		if (!SOLISTENING(inp->inp_socket)) {
			tp = intotcpcb(inp);
			tp = tcp_drop(tp, ECONNABORTED);
			if (tp != NULL)
				INP_WUNLOCK(inp);
		} else
			INP_WUNLOCK(inp);
	} else
		error = ESRCH;
	NET_EPOCH_EXIT(et);
	return (error);
}

SYSCTL_PROC(_net_inet_tcp, TCPCTL_DROP, drop,
    CTLFLAG_VNET | CTLTYPE_STRUCT | CTLFLAG_WR | CTLFLAG_SKIP |
    CTLFLAG_NEEDGIANT, NULL, 0, sysctl_drop, "",
    "Drop TCP connection");

static int
tcp_sysctl_setsockopt(SYSCTL_HANDLER_ARGS)
{
	return (sysctl_setsockopt(oidp, arg1, arg2, req, &V_tcbinfo,
	    &tcp_ctloutput_set));
}

SYSCTL_PROC(_net_inet_tcp, OID_AUTO, setsockopt,
    CTLFLAG_VNET | CTLTYPE_STRUCT | CTLFLAG_WR | CTLFLAG_SKIP |
    CTLFLAG_MPSAFE, NULL, 0, tcp_sysctl_setsockopt, "",
    "Set socket option for TCP endpoint");

#ifdef KERN_TLS
static int
sysctl_switch_tls(SYSCTL_HANDLER_ARGS)
{
	/* addrs[0] is a foreign socket, addrs[1] is a local one. */
	struct sockaddr_storage addrs[2];
	struct inpcb *inp;
#ifdef INET
	struct sockaddr_in *fin = NULL, *lin = NULL;
#endif
	struct epoch_tracker et;
#ifdef INET6
	struct sockaddr_in6 *fin6, *lin6;
#endif
	int error;

	inp = NULL;
#ifdef INET6
	fin6 = lin6 = NULL;
#endif
	error = 0;

	if (req->oldptr != NULL || req->oldlen != 0)
		return (EINVAL);
	if (req->newptr == NULL)
		return (EPERM);
	if (req->newlen < sizeof(addrs))
		return (ENOMEM);
	error = SYSCTL_IN(req, &addrs, sizeof(addrs));
	if (error)
		return (error);

	switch (addrs[0].ss_family) {
#ifdef INET6
	case AF_INET6:
		fin6 = (struct sockaddr_in6 *)&addrs[0];
		lin6 = (struct sockaddr_in6 *)&addrs[1];
		if (fin6->sin6_len != sizeof(struct sockaddr_in6) ||
		    lin6->sin6_len != sizeof(struct sockaddr_in6))
			return (EINVAL);
		if (IN6_IS_ADDR_V4MAPPED(&fin6->sin6_addr)) {
			if (!IN6_IS_ADDR_V4MAPPED(&lin6->sin6_addr))
				return (EINVAL);
			in6_sin6_2_sin_in_sock((struct sockaddr *)&addrs[0]);
			in6_sin6_2_sin_in_sock((struct sockaddr *)&addrs[1]);
#ifdef INET
			fin = (struct sockaddr_in *)&addrs[0];
			lin = (struct sockaddr_in *)&addrs[1];
#endif
			break;
		}
		error = sa6_embedscope(fin6, V_ip6_use_defzone);
		if (error)
			return (error);
		error = sa6_embedscope(lin6, V_ip6_use_defzone);
		if (error)
			return (error);
		break;
#endif
#ifdef INET
	case AF_INET:
		fin = (struct sockaddr_in *)&addrs[0];
		lin = (struct sockaddr_in *)&addrs[1];
		if (fin->sin_len != sizeof(struct sockaddr_in) ||
		    lin->sin_len != sizeof(struct sockaddr_in))
			return (EINVAL);
		break;
#endif
	default:
		return (EINVAL);
	}
	NET_EPOCH_ENTER(et);
	switch (addrs[0].ss_family) {
#ifdef INET6
	case AF_INET6:
		inp = in6_pcblookup(&V_tcbinfo, &fin6->sin6_addr,
		    fin6->sin6_port, &lin6->sin6_addr, lin6->sin6_port,
		    INPLOOKUP_WLOCKPCB, NULL);
		break;
#endif
#ifdef INET
	case AF_INET:
		inp = in_pcblookup(&V_tcbinfo, fin->sin_addr, fin->sin_port,
		    lin->sin_addr, lin->sin_port, INPLOOKUP_WLOCKPCB, NULL);
		break;
#endif
	}
	NET_EPOCH_EXIT(et);
	if (inp != NULL) {
		struct socket *so;

		so = inp->inp_socket;
		soref(so);
		error = ktls_set_tx_mode(so,
		    arg2 == 0 ? TCP_TLS_MODE_SW : TCP_TLS_MODE_IFNET);
		INP_WUNLOCK(inp);
		sorele(so);
	} else
		error = ESRCH;
	return (error);
}

SYSCTL_PROC(_net_inet_tcp, OID_AUTO, switch_to_sw_tls,
    CTLFLAG_VNET | CTLTYPE_STRUCT | CTLFLAG_WR | CTLFLAG_SKIP |
    CTLFLAG_NEEDGIANT, NULL, 0, sysctl_switch_tls, "",
    "Switch TCP connection to SW TLS");
SYSCTL_PROC(_net_inet_tcp, OID_AUTO, switch_to_ifnet_tls,
    CTLFLAG_VNET | CTLTYPE_STRUCT | CTLFLAG_WR | CTLFLAG_SKIP |
    CTLFLAG_NEEDGIANT, NULL, 1, sysctl_switch_tls, "",
    "Switch TCP connection to ifnet TLS");
#endif

/*
 * Generate a standardized TCP log line for use throughout the
 * tcp subsystem.  Memory allocation is done with M_NOWAIT to
 * allow use in the interrupt context.
 *
 * NB: The caller MUST free(s, M_TCPLOG) the returned string.
 * NB: The function may return NULL if memory allocation failed.
 *
 * Due to header inclusion and ordering limitations the struct ip
 * and ip6_hdr pointers have to be passed as void pointers.
 */
char *
tcp_log_vain(struct in_conninfo *inc, struct tcphdr *th, const void *ip4hdr,
    const void *ip6hdr)
{

	/* Is logging enabled? */
	if (V_tcp_log_in_vain == 0)
		return (NULL);

	return (tcp_log_addr(inc, th, ip4hdr, ip6hdr));
}

char *
tcp_log_addrs(struct in_conninfo *inc, struct tcphdr *th, const void *ip4hdr,
    const void *ip6hdr)
{

	/* Is logging enabled? */
	if (tcp_log_debug == 0)
		return (NULL);

	return (tcp_log_addr(inc, th, ip4hdr, ip6hdr));
}

static char *
tcp_log_addr(struct in_conninfo *inc, struct tcphdr *th, const void *ip4hdr,
    const void *ip6hdr)
{
	char *s, *sp;
	size_t size;
#ifdef INET
	const struct ip *ip = (const struct ip *)ip4hdr;
#endif
#ifdef INET6
	const struct ip6_hdr *ip6 = (const struct ip6_hdr *)ip6hdr;
#endif /* INET6 */

	/*
	 * The log line looks like this:
	 * "TCP: [1.2.3.4]:50332 to [1.2.3.4]:80 tcpflags 0x2<SYN>"
	 */
	size = sizeof("TCP: []:12345 to []:12345 tcpflags 0x2<>") +
	    sizeof(PRINT_TH_FLAGS) + 1 +
#ifdef INET6
	    2 * INET6_ADDRSTRLEN;
#else
	    2 * INET_ADDRSTRLEN;
#endif /* INET6 */

	s = malloc(size, M_TCPLOG, M_ZERO|M_NOWAIT);
	if (s == NULL)
		return (NULL);

	strcat(s, "TCP: [");
	sp = s + strlen(s);

	if (inc && ((inc->inc_flags & INC_ISIPV6) == 0)) {
		inet_ntoa_r(inc->inc_faddr, sp);
		sp = s + strlen(s);
		sprintf(sp, "]:%i to [", ntohs(inc->inc_fport));
		sp = s + strlen(s);
		inet_ntoa_r(inc->inc_laddr, sp);
		sp = s + strlen(s);
		sprintf(sp, "]:%i", ntohs(inc->inc_lport));
#ifdef INET6
	} else if (inc) {
		ip6_sprintf(sp, &inc->inc6_faddr);
		sp = s + strlen(s);
		sprintf(sp, "]:%i to [", ntohs(inc->inc_fport));
		sp = s + strlen(s);
		ip6_sprintf(sp, &inc->inc6_laddr);
		sp = s + strlen(s);
		sprintf(sp, "]:%i", ntohs(inc->inc_lport));
	} else if (ip6 && th) {
		ip6_sprintf(sp, &ip6->ip6_src);
		sp = s + strlen(s);
		sprintf(sp, "]:%i to [", ntohs(th->th_sport));
		sp = s + strlen(s);
		ip6_sprintf(sp, &ip6->ip6_dst);
		sp = s + strlen(s);
		sprintf(sp, "]:%i", ntohs(th->th_dport));
#endif /* INET6 */
#ifdef INET
	} else if (ip && th) {
		inet_ntoa_r(ip->ip_src, sp);
		sp = s + strlen(s);
		sprintf(sp, "]:%i to [", ntohs(th->th_sport));
		sp = s + strlen(s);
		inet_ntoa_r(ip->ip_dst, sp);
		sp = s + strlen(s);
		sprintf(sp, "]:%i", ntohs(th->th_dport));
#endif /* INET */
	} else {
		free(s, M_TCPLOG);
		return (NULL);
	}
	sp = s + strlen(s);
	if (th)
		sprintf(sp, " tcpflags 0x%b", tcp_get_flags(th), PRINT_TH_FLAGS);
	if (*(s + size - 1) != '\0')
		panic("%s: string too long", __func__);
	return (s);
}

/*
 * A subroutine which makes it easy to track TCP state changes with DTrace.
 * This function shouldn't be called for t_state initializations that don't
 * correspond to actual TCP state transitions.
 */
void
tcp_state_change(struct tcpcb *tp, int newstate)
{
#if defined(KDTRACE_HOOKS)
	int pstate = tp->t_state;
#endif

	TCPSTATES_DEC(tp->t_state);
	TCPSTATES_INC(newstate);
	tp->t_state = newstate;
	TCP_PROBE6(state__change, NULL, tp, NULL, tp, NULL, pstate);
}

/*
 * Create an external-format (``xtcpcb'') structure using the information in
 * the kernel-format tcpcb structure pointed to by tp.  This is done to
 * reduce the spew of irrelevant information over this interface, to isolate
 * user code from changes in the kernel structure, and potentially to provide
 * information-hiding if we decide that some of this information should be
 * hidden from users.
 */
void
tcp_inptoxtp(const struct inpcb *inp, struct xtcpcb *xt)
{
	struct tcpcb *tp = intotcpcb(inp);
	sbintime_t now;

	bzero(xt, sizeof(*xt));
	xt->t_state = tp->t_state;
	xt->t_logstate = tcp_get_bblog_state(tp);
	xt->t_flags = tp->t_flags;
	xt->t_sndzerowin = tp->t_sndzerowin;
	xt->t_sndrexmitpack = tp->t_sndrexmitpack;
	xt->t_rcvoopack = tp->t_rcvoopack;
	xt->t_rcv_wnd = tp->rcv_wnd;
	xt->t_snd_wnd = tp->snd_wnd;
	xt->t_snd_cwnd = tp->snd_cwnd;
	xt->t_snd_ssthresh = tp->snd_ssthresh;
	xt->t_dsack_bytes = tp->t_dsack_bytes;
	xt->t_dsack_tlp_bytes = tp->t_dsack_tlp_bytes;
	xt->t_dsack_pack = tp->t_dsack_pack;
	xt->t_maxseg = tp->t_maxseg;
	xt->xt_ecn = (tp->t_flags2 & TF2_ECN_PERMIT) ? 1 : 0 +
		     (tp->t_flags2 & TF2_ACE_PERMIT) ? 2 : 0;

	now = getsbinuptime();
#define	COPYTIMER(which,where)	do {					\
	if (tp->t_timers[which] != SBT_MAX)				\
		xt->where = (tp->t_timers[which] - now) / SBT_1MS;	\
	else								\
		xt->where = 0;						\
} while (0)
	COPYTIMER(TT_DELACK, tt_delack);
	COPYTIMER(TT_REXMT, tt_rexmt);
	COPYTIMER(TT_PERSIST, tt_persist);
	COPYTIMER(TT_KEEP, tt_keep);
	COPYTIMER(TT_2MSL, tt_2msl);
#undef COPYTIMER
	xt->t_rcvtime = 1000 * (ticks - tp->t_rcvtime) / hz;

	xt->xt_encaps_port = tp->t_port;
	bcopy(tp->t_fb->tfb_tcp_block_name, xt->xt_stack,
	    TCP_FUNCTION_NAME_LEN_MAX);
	bcopy(CC_ALGO(tp)->name, xt->xt_cc, TCP_CA_NAME_MAX);
#ifdef TCP_BLACKBOX
	(void)tcp_log_get_id(tp, xt->xt_logid);
#endif

	xt->xt_len = sizeof(struct xtcpcb);
	in_pcbtoxinpcb(inp, &xt->xt_inp);
	/*
	 * TCP doesn't use inp_ppcb pointer, we embed inpcb into tcpcb.
	 * Fixup the pointer that in_pcbtoxinpcb() has set.  When printing
	 * TCP netstat(1) used to use this pointer, so this fixup needs to
	 * stay for stable/14.
	 */
	xt->xt_inp.inp_ppcb = (uintptr_t)tp;
}

void
tcp_log_end_status(struct tcpcb *tp, uint8_t status)
{
	uint32_t bit, i;

	if ((tp == NULL) ||
	    (status > TCP_EI_STATUS_MAX_VALUE) ||
	    (status == 0)) {
		/* Invalid */
		return;
	}
	if (status > (sizeof(uint32_t) * 8)) {
		/* Should this be a KASSERT? */
		return;
	}
	bit = 1U << (status - 1);
	if (bit & tp->t_end_info_status) {
		/* already logged */
		return;
	}
	for (i = 0; i < TCP_END_BYTE_INFO; i++) {
		if (tp->t_end_info_bytes[i] == TCP_EI_EMPTY_SLOT) {
			tp->t_end_info_bytes[i] = status;
			tp->t_end_info_status |= bit;
			break;
		}
	}
}

int
tcp_can_enable_pacing(void)
{

	if ((tcp_pacing_limit == -1) ||
	    (tcp_pacing_limit > number_of_tcp_connections_pacing)) {
		atomic_fetchadd_int(&number_of_tcp_connections_pacing, 1);
		shadow_num_connections = number_of_tcp_connections_pacing;
		return (1);
	} else {
		counter_u64_add(tcp_pacing_failures, 1);
		return (0);
	}
}

static uint8_t tcp_pacing_warning = 0;

void
tcp_decrement_paced_conn(void)
{
	uint32_t ret;

	ret = atomic_fetchadd_int(&number_of_tcp_connections_pacing, -1);
	shadow_num_connections = number_of_tcp_connections_pacing;
	KASSERT(ret != 0, ("tcp_paced_connection_exits -1 would cause wrap?"));
	if (ret == 0) {
		if (tcp_pacing_limit != -1) {
			printf("Warning all pacing is now disabled, count decrements invalidly!\n");
			tcp_pacing_limit = 0;
		} else if (tcp_pacing_warning == 0) {
			printf("Warning pacing count is invalid, invalid decrement\n");
			tcp_pacing_warning = 1;
		}
	}
}

static void
tcp_default_switch_failed(struct tcpcb *tp)
{
	/*
	 * If a switch fails we only need to
	 * care about two things:
	 * a) The t_flags2
	 * and
	 * b) The timer granularity.
	 * Timeouts, at least for now, don't use the
	 * old callout system in the other stacks so
	 * those are hopefully safe.
	 */
	tcp_lro_features_off(tp);
	tcp_change_time_units(tp, TCP_TMR_GRANULARITY_TICKS);
}

#ifdef TCP_ACCOUNTING
int
tcp_do_ack_accounting(struct tcpcb *tp, struct tcphdr *th, struct tcpopt *to, uint32_t tiwin, int mss)
{
	if (SEQ_LT(th->th_ack, tp->snd_una)) {
		/* Do we have a SACK? */
		if (to->to_flags & TOF_SACK) {
			if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
				tp->tcp_cnt_counters[ACK_SACK]++;
			}
			return (ACK_SACK);
		} else {
			if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
				tp->tcp_cnt_counters[ACK_BEHIND]++;
			}
			return (ACK_BEHIND);
		}
	} else if (th->th_ack == tp->snd_una) {
		/* Do we have a SACK? */
		if (to->to_flags & TOF_SACK) {
			if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
				tp->tcp_cnt_counters[ACK_SACK]++;
			}
			return (ACK_SACK);
		} else if (tiwin != tp->snd_wnd) {
			if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
				tp->tcp_cnt_counters[ACK_RWND]++;
			}
			return (ACK_RWND);
		} else {
			if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
				tp->tcp_cnt_counters[ACK_DUPACK]++;
			}
			return (ACK_DUPACK);
		}
	} else {
		if (!SEQ_GT(th->th_ack, tp->snd_max)) {
			if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
				tp->tcp_cnt_counters[CNT_OF_ACKS_IN] += (((th->th_ack - tp->snd_una) + mss - 1)/mss);
			}
		}
		if (to->to_flags & TOF_SACK) {
			if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
				tp->tcp_cnt_counters[ACK_CUMACK_SACK]++;
			}
			return (ACK_CUMACK_SACK);
		} else {
			if (tp->t_flags2 & TF2_TCP_ACCOUNTING) {
				tp->tcp_cnt_counters[ACK_CUMACK]++;
			}
			return (ACK_CUMACK);
		}
	}
}
#endif

void
tcp_change_time_units(struct tcpcb *tp, int granularity)
{
	if (tp->t_tmr_granularity == granularity) {
		/* We are there */
		return;
	}
	if (granularity == TCP_TMR_GRANULARITY_USEC) {
		KASSERT((tp->t_tmr_granularity == TCP_TMR_GRANULARITY_TICKS),
			("Granularity is not TICKS its %u in tp:%p",
			 tp->t_tmr_granularity, tp));
		tp->t_rttlow = TICKS_2_USEC(tp->t_rttlow);
		if (tp->t_srtt > 1) {
			uint32_t val, frac;

			val = tp->t_srtt >> TCP_RTT_SHIFT;
			frac = tp->t_srtt & 0x1f;
			tp->t_srtt = TICKS_2_USEC(val);
			/*
			 * frac is the fractional part of the srtt (if any)
			 * but its in ticks and every bit represents
			 * 1/32nd of a hz.
			 */
			if (frac) {
				if (hz == 1000) {
					frac = (((uint64_t)frac * (uint64_t)HPTS_USEC_IN_MSEC) / (uint64_t)TCP_RTT_SCALE);
				} else {
					frac = (((uint64_t)frac * (uint64_t)HPTS_USEC_IN_SEC) / ((uint64_t)(hz) * (uint64_t)TCP_RTT_SCALE));
				}
				tp->t_srtt += frac;
			}
		}
		if (tp->t_rttvar) {
			uint32_t val, frac;

			val = tp->t_rttvar >> TCP_RTTVAR_SHIFT;
			frac = tp->t_rttvar & 0x1f;
			tp->t_rttvar = TICKS_2_USEC(val);
			/*
			 * frac is the fractional part of the srtt (if any)
			 * but its in ticks and every bit represents
			 * 1/32nd of a hz.
			 */
			if (frac) {
				if (hz == 1000) {
					frac = (((uint64_t)frac * (uint64_t)HPTS_USEC_IN_MSEC) / (uint64_t)TCP_RTT_SCALE);
				} else {
					frac = (((uint64_t)frac * (uint64_t)HPTS_USEC_IN_SEC) / ((uint64_t)(hz) * (uint64_t)TCP_RTT_SCALE));
				}
				tp->t_rttvar += frac;
			}
		}
		tp->t_tmr_granularity = TCP_TMR_GRANULARITY_USEC;
	} else if (granularity == TCP_TMR_GRANULARITY_TICKS) {
		/* Convert back to ticks, with  */
		KASSERT((tp->t_tmr_granularity == TCP_TMR_GRANULARITY_USEC),
			("Granularity is not USEC its %u in tp:%p",
			 tp->t_tmr_granularity, tp));
		if (tp->t_srtt > 1) {
			uint32_t val, frac;

			val = USEC_2_TICKS(tp->t_srtt);
			frac = tp->t_srtt % (HPTS_USEC_IN_SEC / hz);
			tp->t_srtt = val << TCP_RTT_SHIFT;
			/*
			 * frac is the fractional part here is left
			 * over from converting to hz and shifting.
			 * We need to convert this to the 5 bit
			 * remainder.
			 */
			if (frac) {
				if (hz == 1000) {
					frac = (((uint64_t)frac *  (uint64_t)TCP_RTT_SCALE) / (uint64_t)HPTS_USEC_IN_MSEC);
				} else {
					frac = (((uint64_t)frac * (uint64_t)(hz) * (uint64_t)TCP_RTT_SCALE) /(uint64_t)HPTS_USEC_IN_SEC);
				}
				tp->t_srtt += frac;
			}
		}
		if (tp->t_rttvar) {
			uint32_t val, frac;

			val = USEC_2_TICKS(tp->t_rttvar);
			frac = tp->t_srtt % (HPTS_USEC_IN_SEC / hz);
			tp->t_rttvar = val <<  TCP_RTTVAR_SHIFT;
			/*
			 * frac is the fractional part here is left
			 * over from converting to hz and shifting.
			 * We need to convert this to the 5 bit
			 * remainder.
			 */
			if (frac) {
				if (hz == 1000) {
					frac = (((uint64_t)frac *  (uint64_t)TCP_RTT_SCALE) / (uint64_t)HPTS_USEC_IN_MSEC);
				} else {
					frac = (((uint64_t)frac * (uint64_t)(hz) * (uint64_t)TCP_RTT_SCALE) /(uint64_t)HPTS_USEC_IN_SEC);
				}
				tp->t_rttvar += frac;
			}
		}
		tp->t_rttlow = USEC_2_TICKS(tp->t_rttlow);
		tp->t_tmr_granularity = TCP_TMR_GRANULARITY_TICKS;
	}
#ifdef INVARIANTS
	else {
		panic("Unknown granularity:%d tp:%p",
		      granularity, tp);
	}
#endif	
}

void
tcp_handle_orphaned_packets(struct tcpcb *tp)
{
	struct mbuf *save, *m, *prev;
	/*
	 * Called when a stack switch is occuring from the fini()
	 * of the old stack. We assue the init() as already been
	 * run of the new stack and it has set the t_flags2 to
	 * what it supports. This function will then deal with any
	 * differences i.e. cleanup packets that maybe queued that
	 * the newstack does not support.
	 */

	if (tp->t_flags2 & TF2_MBUF_L_ACKS)
		return;
	if ((tp->t_flags2 & TF2_SUPPORTS_MBUFQ) == 0 &&
	    !STAILQ_EMPTY(&tp->t_inqueue)) {
		/*
		 * It is unsafe to process the packets since a
		 * reset may be lurking in them (its rare but it
		 * can occur). If we were to find a RST, then we
		 * would end up dropping the connection and the
		 * INP lock, so when we return the caller (tcp_usrreq)
		 * will blow up when it trys to unlock the inp.
		 * This new stack does not do any fancy LRO features
		 * so all we can do is toss the packets.
		 */
		m = STAILQ_FIRST(&tp->t_inqueue);
		STAILQ_INIT(&tp->t_inqueue);
		STAILQ_FOREACH_FROM_SAFE(m, &tp->t_inqueue, m_stailqpkt, save)
			m_freem(m);
	} else {
		/*
		 * Here we have a stack that does mbuf queuing but
		 * does not support compressed ack's. We must
		 * walk all the mbufs and discard any compressed acks.
		 */
		STAILQ_FOREACH_SAFE(m, &tp->t_inqueue, m_stailqpkt, save) {
			if (m->m_flags & M_ACKCMP) {
				if (m == STAILQ_FIRST(&tp->t_inqueue))
					STAILQ_REMOVE_HEAD(&tp->t_inqueue,
					    m_stailqpkt);
				else
					STAILQ_REMOVE_AFTER(&tp->t_inqueue,
					    prev, m_stailqpkt);
				m_freem(m);
			} else
				prev = m;
		}
	}
}

#ifdef TCP_REQUEST_TRK
uint32_t
tcp_estimate_tls_overhead(struct socket *so, uint64_t tls_usr_bytes)
{
#ifdef KERN_TLS
	struct ktls_session *tls;
	uint32_t rec_oh, records;

	tls = so->so_snd.sb_tls_info;
	if (tls == NULL)
	    return (0);

	rec_oh = tls->params.tls_hlen + tls->params.tls_tlen;
	records = ((tls_usr_bytes + tls->params.max_frame_len - 1)/tls->params.max_frame_len);
	return (records * rec_oh);
#else
	return (0);
#endif
}

extern uint32_t tcp_stale_entry_time;
uint32_t tcp_stale_entry_time = 250000;
SYSCTL_UINT(_net_inet_tcp, OID_AUTO, usrlog_stale, CTLFLAG_RW,
    &tcp_stale_entry_time, 250000, "Time that a tcpreq entry without a sendfile ages out");

void
tcp_req_log_req_info(struct tcpcb *tp, struct tcp_sendfile_track *req,
    uint16_t slot, uint8_t val, uint64_t offset, uint64_t nbytes)
{
	if (tcp_bblogging_on(tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv;

		memset(&log.u_bbr, 0, sizeof(log.u_bbr));
		log.u_bbr.inhpts = tcp_in_hpts(tp);
		log.u_bbr.flex8 = val;
		log.u_bbr.rttProp = req->timestamp;
		log.u_bbr.delRate = req->start;
		log.u_bbr.cur_del_rate = req->end;
		log.u_bbr.flex1 = req->start_seq;
		log.u_bbr.flex2 = req->end_seq;
		log.u_bbr.flex3 = req->flags;
		log.u_bbr.flex4 = ((req->localtime >> 32) & 0x00000000ffffffff);
		log.u_bbr.flex5 = (req->localtime & 0x00000000ffffffff);
		log.u_bbr.flex7 = slot;
		log.u_bbr.bw_inuse = offset;
		/* nbytes = flex6 | epoch */
		log.u_bbr.flex6 = ((nbytes >> 32) & 0x00000000ffffffff);
		log.u_bbr.epoch = (nbytes & 0x00000000ffffffff);
		/* cspr =  lt_epoch | pkts_out */
		log.u_bbr.lt_epoch = ((req->cspr >> 32) & 0x00000000ffffffff);
		log.u_bbr.pkts_out |= (req->cspr & 0x00000000ffffffff);
		log.u_bbr.applimited = tp->t_tcpreq_closed;
		log.u_bbr.applimited <<= 8;
		log.u_bbr.applimited |= tp->t_tcpreq_open;
		log.u_bbr.applimited <<= 8;
		log.u_bbr.applimited |= tp->t_tcpreq_req;
		log.u_bbr.timeStamp = tcp_get_usecs(&tv);
		TCP_LOG_EVENTP(tp, NULL,
		    &tptosocket(tp)->so_rcv,
		    &tptosocket(tp)->so_snd,
		    TCP_LOG_REQ_T, 0,
		    0, &log, false, &tv);
	}
}

void
tcp_req_free_a_slot(struct tcpcb *tp, struct tcp_sendfile_track *ent)
{
	if (tp->t_tcpreq_req > 0)
		tp->t_tcpreq_req--;
	if (ent->flags & TCP_TRK_TRACK_FLG_OPEN) {
		if (tp->t_tcpreq_open > 0)
			tp->t_tcpreq_open--;
	} else {
		if (tp->t_tcpreq_closed > 0)
			tp->t_tcpreq_closed--;
	}
	ent->flags = TCP_TRK_TRACK_FLG_EMPTY;
}

static void
tcp_req_check_for_stale_entries(struct tcpcb *tp, uint64_t ts, int rm_oldest)
{
	struct tcp_sendfile_track *ent;
	uint64_t time_delta, oldest_delta;
	int i, oldest, oldest_set = 0, cnt_rm = 0;

	for(i = 0; i < MAX_TCP_TRK_REQ; i++) {
		ent = &tp->t_tcpreq_info[i];
		if (ent->flags != TCP_TRK_TRACK_FLG_USED) {
			/*
			 * We only care about closed end ranges
			 * that are allocated and have no sendfile
			 * ever touching them. They would be in
			 * state USED.
			 */
			continue;
		}
		if (ts >= ent->localtime)
			time_delta = ts - ent->localtime;
		else
			time_delta = 0;
		if (time_delta &&
		    ((oldest_delta < time_delta) || (oldest_set == 0))) {
			oldest_set = 1;
			oldest = i;
			oldest_delta = time_delta;
		}
		if (tcp_stale_entry_time && (time_delta >= tcp_stale_entry_time)) {
			/*
			 * No sendfile in a our time-limit
			 * time to purge it.
			 */
			cnt_rm++;
			tcp_req_log_req_info(tp, &tp->t_tcpreq_info[i], i, TCP_TRK_REQ_LOG_STALE,
					      time_delta, 0);
			tcp_req_free_a_slot(tp, ent);
		}
	}
	if ((cnt_rm == 0) && rm_oldest && oldest_set) {
		ent = &tp->t_tcpreq_info[oldest];
		tcp_req_log_req_info(tp, &tp->t_tcpreq_info[i], i, TCP_TRK_REQ_LOG_STALE,
				      oldest_delta, 1);
		tcp_req_free_a_slot(tp, ent);
	}
}

int
tcp_req_check_for_comp(struct tcpcb *tp, tcp_seq ack_point)
{
	int i, ret=0;
	struct tcp_sendfile_track *ent;

	/* Clean up any old closed end requests that are now completed */
	if (tp->t_tcpreq_req == 0)
		return(0);
	if (tp->t_tcpreq_closed == 0)
		return(0);
	for(i = 0; i < MAX_TCP_TRK_REQ; i++) {
		ent = &tp->t_tcpreq_info[i];
		/* Skip empty ones */
		if (ent->flags == TCP_TRK_TRACK_FLG_EMPTY)
			continue;
		/* Skip open ones */
		if (ent->flags & TCP_TRK_TRACK_FLG_OPEN)
			continue;
		if (SEQ_GEQ(ack_point, ent->end_seq)) {
			/* We are past it -- free it */
			tcp_req_log_req_info(tp, ent,
					      i, TCP_TRK_REQ_LOG_FREED, 0, 0);
			tcp_req_free_a_slot(tp, ent);
			ret++;
		}
	}
	return (ret);
}

int
tcp_req_is_entry_comp(struct tcpcb *tp, struct tcp_sendfile_track *ent, tcp_seq ack_point)
{
	if (tp->t_tcpreq_req == 0)
		return(-1);
	if (tp->t_tcpreq_closed == 0)
		return(-1);
	if (ent->flags == TCP_TRK_TRACK_FLG_EMPTY)
		return(-1);
	if (SEQ_GEQ(ack_point, ent->end_seq)) {
		return (1);
	}
	return (0);
}

struct tcp_sendfile_track *
tcp_req_find_a_req_that_is_completed_by(struct tcpcb *tp, tcp_seq th_ack, int *ip)
{
	/*
	 * Given an ack point (th_ack) walk through our entries and
	 * return the first one found that th_ack goes past the
	 * end_seq.
	 */
	struct tcp_sendfile_track *ent;
	int i;

	if (tp->t_tcpreq_req == 0) {
		/* none open */
		return (NULL);
	}
	for(i = 0; i < MAX_TCP_TRK_REQ; i++) {
		ent = &tp->t_tcpreq_info[i];
		if (ent->flags == TCP_TRK_TRACK_FLG_EMPTY)
			continue;
		if ((ent->flags & TCP_TRK_TRACK_FLG_OPEN) == 0) {
			if (SEQ_GEQ(th_ack, ent->end_seq)) {
				*ip = i;
				return (ent);
			}
		}
	}
	return (NULL);
}

struct tcp_sendfile_track *
tcp_req_find_req_for_seq(struct tcpcb *tp, tcp_seq seq)
{
	struct tcp_sendfile_track *ent;
	int i;

	if (tp->t_tcpreq_req == 0) {
		/* none open */
		return (NULL);
	}
	for(i = 0; i < MAX_TCP_TRK_REQ; i++) {
		ent = &tp->t_tcpreq_info[i];
		tcp_req_log_req_info(tp, ent, i, TCP_TRK_REQ_LOG_SEARCH,
				      (uint64_t)seq, 0);
		if (ent->flags == TCP_TRK_TRACK_FLG_EMPTY) {
			continue;
		}
		if (ent->flags & TCP_TRK_TRACK_FLG_OPEN) {
			/*
			 * An open end request only needs to
			 * match the beginning seq or be
			 * all we have (once we keep going on
			 * a open end request we may have a seq
			 * wrap).
			 */
			if ((SEQ_GEQ(seq, ent->start_seq)) ||
			    (tp->t_tcpreq_closed == 0))
				return (ent);
		} else {
			/*
			 * For this one we need to
			 * be a bit more careful if its
			 * completed at least.
			 */
			if ((SEQ_GEQ(seq, ent->start_seq)) &&
			    (SEQ_LT(seq, ent->end_seq))) {
				return (ent);
			}
		}
	}
	return (NULL);
}

/* Should this be in its own file tcp_req.c ? */
struct tcp_sendfile_track *
tcp_req_alloc_req_full(struct tcpcb *tp, struct tcp_snd_req *req, uint64_t ts, int rec_dups)
{
	struct tcp_sendfile_track *fil;
	int i, allocated;

	/* In case the stack does not check for completions do so now */
	tcp_req_check_for_comp(tp, tp->snd_una);
	/* Check for stale entries */
	if (tp->t_tcpreq_req)
		tcp_req_check_for_stale_entries(tp, ts,
		    (tp->t_tcpreq_req >= MAX_TCP_TRK_REQ));
	/* Check to see if this is a duplicate of one not started */
	if (tp->t_tcpreq_req) {
		for(i = 0, allocated = 0; i < MAX_TCP_TRK_REQ; i++) {
			fil = &tp->t_tcpreq_info[i];
			if (fil->flags != TCP_TRK_TRACK_FLG_USED)
				continue;
			if ((fil->timestamp == req->timestamp) &&
			    (fil->start == req->start) &&
			    ((fil->flags & TCP_TRK_TRACK_FLG_OPEN) ||
			     (fil->end == req->end))) {
				/*
				 * We already have this request
				 * and it has not been started with sendfile.
				 * This probably means the user was returned
				 * a 4xx of some sort and its going to age
				 * out, lets not duplicate it.
				 */
				return(fil);
			}
		}
	}
	/* Ok if there is no room at the inn we are in trouble */
	if (tp->t_tcpreq_req >= MAX_TCP_TRK_REQ) {
		tcp_trace_point(tp, TCP_TP_REQ_LOG_FAIL);
		for(i = 0; i < MAX_TCP_TRK_REQ; i++) {
			tcp_req_log_req_info(tp, &tp->t_tcpreq_info[i],
			    i, TCP_TRK_REQ_LOG_ALLOCFAIL, 0, 0);
		}
		return (NULL);
	}
	for(i = 0, allocated = 0; i < MAX_TCP_TRK_REQ; i++) {
		fil = &tp->t_tcpreq_info[i];
		if (fil->flags == TCP_TRK_TRACK_FLG_EMPTY) {
			allocated = 1;
			fil->flags = TCP_TRK_TRACK_FLG_USED;
			fil->timestamp = req->timestamp;
			fil->localtime = ts;
			fil->start = req->start;
			if (req->flags & TCP_LOG_HTTPD_RANGE_END) {
				fil->end = req->end;
			} else {
				fil->end = 0;
				fil->flags |= TCP_TRK_TRACK_FLG_OPEN;
			}
			/*
			 * We can set the min boundaries to the TCP Sequence space,
			 * but it might be found to be further up when sendfile
			 * actually runs on this range (if it ever does).
			 */
			fil->sbcc_at_s = tptosocket(tp)->so_snd.sb_ccc;
			fil->start_seq = tp->snd_una +
			    tptosocket(tp)->so_snd.sb_ccc;
			fil->end_seq = (fil->start_seq + ((uint32_t)(fil->end - fil->start)));
			if (tptosocket(tp)->so_snd.sb_tls_info) {
				/*
				 * This session is doing TLS. Take a swag guess
				 * at the overhead.
				 */
				fil->end_seq += tcp_estimate_tls_overhead(
				    tptosocket(tp), (fil->end - fil->start));
			}
			tp->t_tcpreq_req++;
			if (fil->flags & TCP_TRK_TRACK_FLG_OPEN)
				tp->t_tcpreq_open++;
			else
				tp->t_tcpreq_closed++;
			tcp_req_log_req_info(tp, fil, i,
			    TCP_TRK_REQ_LOG_NEW, 0, 0);
			break;
		} else
			fil = NULL;
	}
	return (fil);
}

void
tcp_req_alloc_req(struct tcpcb *tp, union tcp_log_userdata *user, uint64_t ts)
{
	(void)tcp_req_alloc_req_full(tp, &user->tcp_req, ts, 1);
}
#endif

void
tcp_log_socket_option(struct tcpcb *tp, uint32_t option_num, uint32_t option_val, int err)
{
	if (tcp_bblogging_on(tp)) {
		struct tcp_log_buffer *l;

		l = tcp_log_event(tp, NULL,
		        &tptosocket(tp)->so_rcv,
		        &tptosocket(tp)->so_snd,
		        TCP_LOG_SOCKET_OPT,
		        err, 0, NULL, 1,
		        NULL, NULL, 0, NULL);
		if (l) {
			l->tlb_flex1 = option_num;
			l->tlb_flex2 = option_val;
		}
	}
}

uint32_t
tcp_get_srtt(struct tcpcb *tp, int granularity)
{
	uint32_t srtt;

	KASSERT(granularity == TCP_TMR_GRANULARITY_USEC ||
	    granularity == TCP_TMR_GRANULARITY_TICKS,
	    ("%s: called with unexpected granularity %d", __func__,
	    granularity));

	srtt = tp->t_srtt;

	/*
	 * We only support two granularities. If the stored granularity
	 * does not match the granularity requested by the caller,
	 * convert the stored value to the requested unit of granularity.
	 */
	if (tp->t_tmr_granularity != granularity) {
		if (granularity == TCP_TMR_GRANULARITY_USEC)
			srtt = TICKS_2_USEC(srtt);
		else
			srtt = USEC_2_TICKS(srtt);
	}

	/*
	 * If the srtt is stored with ticks granularity, we need to
	 * unshift to get the actual value. We do this after the
	 * conversion above (if one was necessary) in order to maximize
	 * precision.
	 */
	if (tp->t_tmr_granularity == TCP_TMR_GRANULARITY_TICKS)
		srtt = srtt >> TCP_RTT_SHIFT;

	return (srtt);
}

void
tcp_account_for_send(struct tcpcb *tp, uint32_t len, uint8_t is_rxt,
    uint8_t is_tlp, bool hw_tls)
{

	if (is_tlp) {
		tp->t_sndtlppack++;
		tp->t_sndtlpbyte += len;
	}
	/* To get total bytes sent you must add t_snd_rxt_bytes to t_sndbytes */
	if (is_rxt)
		tp->t_snd_rxt_bytes += len;
	else
		tp->t_sndbytes += len;

#ifdef KERN_TLS
	if (hw_tls && is_rxt && len != 0) {
		uint64_t rexmit_percent;

		rexmit_percent = (1000ULL * tp->t_snd_rxt_bytes) /
		    (10ULL * (tp->t_snd_rxt_bytes + tp->t_sndbytes));
		if (rexmit_percent > ktls_ifnet_max_rexmit_pct)
			ktls_disable_ifnet(tp);
	}
#endif
}
