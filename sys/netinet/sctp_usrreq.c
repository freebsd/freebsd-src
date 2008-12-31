/*-
 * Copyright (c) 2001-2007, by Cisco Systems, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * a) Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * b) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the distribution.
 *
 * c) Neither the name of Cisco Systems, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $KAME: sctp_usrreq.c,v 1.48 2005/03/07 23:26:08 itojun Exp $	 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/netinet/sctp_usrreq.c,v 1.48.2.10.2.1 2008/11/25 02:59:29 kensmith Exp $");
#include <netinet/sctp_os.h>
#include <sys/proc.h>
#include <netinet/sctp_pcb.h>
#include <netinet/sctp_header.h>
#include <netinet/sctp_var.h>
#if defined(INET6)
#endif
#include <netinet/sctp_sysctl.h>
#include <netinet/sctp_output.h>
#include <netinet/sctp_uio.h>
#include <netinet/sctp_asconf.h>
#include <netinet/sctputil.h>
#include <netinet/sctp_indata.h>
#include <netinet/sctp_timer.h>
#include <netinet/sctp_auth.h>
#include <netinet/sctp_bsd_addr.h>
#include <netinet/sctp_cc_functions.h>




void
sctp_init(void)
{
	u_long sb_max_adj;

	bzero(&SCTP_BASE_STATS, sizeof(struct sctpstat));

	/* Initialize and modify the sysctled variables */
	sctp_init_sysctls();
	if ((nmbclusters / 8) > SCTP_ASOC_MAX_CHUNKS_ON_QUEUE)
		SCTP_BASE_SYSCTL(sctp_max_chunks_on_queue) = (nmbclusters / 8);
	/*
	 * Allow a user to take no more than 1/2 the number of clusters or
	 * the SB_MAX whichever is smaller for the send window.
	 */
	sb_max_adj = (u_long)((u_quad_t) (SB_MAX) * MCLBYTES / (MSIZE + MCLBYTES));
	SCTP_BASE_SYSCTL(sctp_sendspace) = min(sb_max_adj,
	    (((uint32_t) nmbclusters / 2) * SCTP_DEFAULT_MAXSEGMENT));
	/*
	 * Now for the recv window, should we take the same amount? or
	 * should I do 1/2 the SB_MAX instead in the SB_MAX min above. For
	 * now I will just copy.
	 */
	SCTP_BASE_SYSCTL(sctp_recvspace) = SCTP_BASE_SYSCTL(sctp_sendspace);

	SCTP_BASE_VAR(first_time) = 0;
	SCTP_BASE_VAR(sctp_pcb_initialized) = 0;
	sctp_pcb_init();
#if defined(SCTP_PACKET_LOGGING)
	SCTP_BASE_VAR(packet_log_writers) = 0;
	SCTP_BASE_VAR(packet_log_end) = 0;
	bzero(&SCTP_BASE_VAR(packet_log_buffer), SCTP_PACKET_LOG_SIZE);
#endif


}

void
sctp_finish(void)
{
	sctp_pcb_finish();
}



void
sctp_pathmtu_adjustment(struct sctp_inpcb *inp,
    struct sctp_tcb *stcb,
    struct sctp_nets *net,
    uint16_t nxtsz)
{
	struct sctp_tmit_chunk *chk;

	/* Adjust that too */
	stcb->asoc.smallest_mtu = nxtsz;
	/* now off to subtract IP_DF flag if needed */
#ifdef SCTP_PRINT_FOR_B_AND_M
	SCTP_PRINTF("sctp_pathmtu_adjust called inp:%p stcb:%p net:%p nxtsz:%d\n",
	    inp, stcb, net, nxtsz);
#endif
	TAILQ_FOREACH(chk, &stcb->asoc.send_queue, sctp_next) {
		if ((chk->send_size + IP_HDR_SIZE) > nxtsz) {
			chk->flags |= CHUNK_FLAGS_FRAGMENT_OK;
		}
	}
	TAILQ_FOREACH(chk, &stcb->asoc.sent_queue, sctp_next) {
		if ((chk->send_size + IP_HDR_SIZE) > nxtsz) {
			/*
			 * For this guy we also mark for immediate resend
			 * since we sent to big of chunk
			 */
			chk->flags |= CHUNK_FLAGS_FRAGMENT_OK;
			if (chk->sent != SCTP_DATAGRAM_RESEND) {
				sctp_ucount_incr(stcb->asoc.sent_queue_retran_cnt);
			}
			chk->sent = SCTP_DATAGRAM_RESEND;
			chk->rec.data.doing_fast_retransmit = 0;
			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FLIGHT_LOGGING_ENABLE) {
				sctp_misc_ints(SCTP_FLIGHT_LOG_DOWN_PMTU,
				    chk->whoTo->flight_size,
				    chk->book_size,
				    (uintptr_t) chk->whoTo,
				    chk->rec.data.TSN_seq);
			}
			/* Clear any time so NO RTT is being done */
			chk->do_rtt = 0;
			sctp_flight_size_decrease(chk);
			sctp_total_flight_decrease(stcb, chk);
		}
	}
}

static void
sctp_notify_mbuf(struct sctp_inpcb *inp,
    struct sctp_tcb *stcb,
    struct sctp_nets *net,
    struct ip *ip,
    struct sctphdr *sh)
{
	struct icmp *icmph;
	int totsz, tmr_stopped = 0;
	uint16_t nxtsz;

	/* protection */
	if ((inp == NULL) || (stcb == NULL) || (net == NULL) ||
	    (ip == NULL) || (sh == NULL)) {
		if (stcb != NULL) {
			SCTP_TCB_UNLOCK(stcb);
		}
		return;
	}
	/* First job is to verify the vtag matches what I would send */
	if (ntohl(sh->v_tag) != (stcb->asoc.peer_vtag)) {
		SCTP_TCB_UNLOCK(stcb);
		return;
	}
	icmph = (struct icmp *)((caddr_t)ip - (sizeof(struct icmp) -
	    sizeof(struct ip)));
	if (icmph->icmp_type != ICMP_UNREACH) {
		/* We only care about unreachable */
		SCTP_TCB_UNLOCK(stcb);
		return;
	}
	if (icmph->icmp_code != ICMP_UNREACH_NEEDFRAG) {
		/* not a unreachable message due to frag. */
		SCTP_TCB_UNLOCK(stcb);
		return;
	}
	totsz = ip->ip_len;

	nxtsz = ntohs(icmph->icmp_nextmtu);
	if (nxtsz == 0) {
		/*
		 * old type router that does not tell us what the next size
		 * mtu is. Rats we will have to guess (in a educated fashion
		 * of course)
		 */
		nxtsz = find_next_best_mtu(totsz);
	}
	/* Stop any PMTU timer */
	if (SCTP_OS_TIMER_PENDING(&net->pmtu_timer.timer)) {
		tmr_stopped = 1;
		sctp_timer_stop(SCTP_TIMER_TYPE_PATHMTURAISE, inp, stcb, net,
		    SCTP_FROM_SCTP_USRREQ + SCTP_LOC_1);
	}
	/* Adjust destination size limit */
	if (net->mtu > nxtsz) {
		net->mtu = nxtsz;
	}
	/* now what about the ep? */
	if (stcb->asoc.smallest_mtu > nxtsz) {
#ifdef SCTP_PRINT_FOR_B_AND_M
		SCTP_PRINTF("notify_mbuf (ICMP) calls sctp_pathmtu_adjust mtu:%d\n",
		    nxtsz);
#endif
		sctp_pathmtu_adjustment(inp, stcb, net, nxtsz);
	}
	if (tmr_stopped)
		sctp_timer_start(SCTP_TIMER_TYPE_PATHMTURAISE, inp, stcb, net);

	SCTP_TCB_UNLOCK(stcb);
}


void
sctp_notify(struct sctp_inpcb *inp,
    struct ip *ip,
    struct sctphdr *sh,
    struct sockaddr *to,
    struct sctp_tcb *stcb,
    struct sctp_nets *net)
{
#if defined (__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
	struct socket *so;

#endif
	/* protection */
	int reason;
	struct icmp *icmph;


	if ((inp == NULL) || (stcb == NULL) || (net == NULL) ||
	    (sh == NULL) || (to == NULL)) {
		if (stcb)
			SCTP_TCB_UNLOCK(stcb);
		return;
	}
	/* First job is to verify the vtag matches what I would send */
	if (ntohl(sh->v_tag) != (stcb->asoc.peer_vtag)) {
		SCTP_TCB_UNLOCK(stcb);
		return;
	}
	icmph = (struct icmp *)((caddr_t)ip - (sizeof(struct icmp) -
	    sizeof(struct ip)));
	if (icmph->icmp_type != ICMP_UNREACH) {
		/* We only care about unreachable */
		SCTP_TCB_UNLOCK(stcb);
		return;
	}
	if ((icmph->icmp_code == ICMP_UNREACH_NET) ||
	    (icmph->icmp_code == ICMP_UNREACH_HOST) ||
	    (icmph->icmp_code == ICMP_UNREACH_NET_UNKNOWN) ||
	    (icmph->icmp_code == ICMP_UNREACH_HOST_UNKNOWN) ||
	    (icmph->icmp_code == ICMP_UNREACH_ISOLATED) ||
	    (icmph->icmp_code == ICMP_UNREACH_NET_PROHIB) ||
	    (icmph->icmp_code == ICMP_UNREACH_HOST_PROHIB) ||
	    (icmph->icmp_code == ICMP_UNREACH_FILTER_PROHIB)) {

		/*
		 * Hmm reachablity problems we must examine closely. If its
		 * not reachable, we may have lost a network. Or if there is
		 * NO protocol at the other end named SCTP. well we consider
		 * it a OOTB abort.
		 */
		if (net->dest_state & SCTP_ADDR_REACHABLE) {
			/* Ok that destination is NOT reachable */
			SCTP_PRINTF("ICMP (thresh %d/%d) takes interface %p down\n",
			    net->error_count,
			    net->failure_threshold,
			    net);

			net->dest_state &= ~SCTP_ADDR_REACHABLE;
			net->dest_state |= SCTP_ADDR_NOT_REACHABLE;
			/*
			 * JRS 5/14/07 - If a destination is unreachable,
			 * the PF bit is turned off.  This allows an
			 * unambiguous use of the PF bit for destinations
			 * that are reachable but potentially failed. If the
			 * destination is set to the unreachable state, also
			 * set the destination to the PF state.
			 */
			/*
			 * Add debug message here if destination is not in
			 * PF state.
			 */
			/* Stop any running T3 timers here? */
			if (SCTP_BASE_SYSCTL(sctp_cmt_on_off) && SCTP_BASE_SYSCTL(sctp_cmt_pf)) {
				net->dest_state &= ~SCTP_ADDR_PF;
				SCTPDBG(SCTP_DEBUG_TIMER4, "Destination %p moved from PF to unreachable.\n",
				    net);
			}
			net->error_count = net->failure_threshold + 1;
			sctp_ulp_notify(SCTP_NOTIFY_INTERFACE_DOWN,
			    stcb, SCTP_FAILED_THRESHOLD,
			    (void *)net, SCTP_SO_NOT_LOCKED);
		}
		SCTP_TCB_UNLOCK(stcb);
	} else if ((icmph->icmp_code == ICMP_UNREACH_PROTOCOL) ||
	    (icmph->icmp_code == ICMP_UNREACH_PORT)) {
		/*
		 * Here the peer is either playing tricks on us, including
		 * an address that belongs to someone who does not support
		 * SCTP OR was a userland implementation that shutdown and
		 * now is dead. In either case treat it like a OOTB abort
		 * with no TCB
		 */
		reason = SCTP_PEER_FAULTY;
		sctp_abort_notification(stcb, reason, SCTP_SO_NOT_LOCKED);
#if defined (__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
		so = SCTP_INP_SO(inp);
		atomic_add_int(&stcb->asoc.refcnt, 1);
		SCTP_TCB_UNLOCK(stcb);
		SCTP_SOCKET_LOCK(so, 1);
		SCTP_TCB_LOCK(stcb);
		atomic_subtract_int(&stcb->asoc.refcnt, 1);
#endif
		(void)sctp_free_assoc(inp, stcb, SCTP_NORMAL_PROC, SCTP_FROM_SCTP_USRREQ + SCTP_LOC_2);
#if defined (__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
		SCTP_SOCKET_UNLOCK(so, 1);
		/* SCTP_TCB_UNLOCK(stcb); MT: I think this is not needed. */
#endif
		/* no need to unlock here, since the TCB is gone */
	} else {
		SCTP_TCB_UNLOCK(stcb);
	}
}

void
sctp_ctlinput(cmd, sa, vip)
	int cmd;
	struct sockaddr *sa;
	void *vip;
{
	struct ip *ip = vip;
	struct sctphdr *sh;
	uint32_t vrf_id;

	/* FIX, for non-bsd is this right? */
	vrf_id = SCTP_DEFAULT_VRFID;
	if (sa->sa_family != AF_INET ||
	    ((struct sockaddr_in *)sa)->sin_addr.s_addr == INADDR_ANY) {
		return;
	}
	if (PRC_IS_REDIRECT(cmd)) {
		ip = 0;
	} else if ((unsigned)cmd >= PRC_NCMDS || inetctlerrmap[cmd] == 0) {
		return;
	}
	if (ip) {
		struct sctp_inpcb *inp = NULL;
		struct sctp_tcb *stcb = NULL;
		struct sctp_nets *net = NULL;
		struct sockaddr_in to, from;

		sh = (struct sctphdr *)((caddr_t)ip + (ip->ip_hl << 2));
		bzero(&to, sizeof(to));
		bzero(&from, sizeof(from));
		from.sin_family = to.sin_family = AF_INET;
		from.sin_len = to.sin_len = sizeof(to);
		from.sin_port = sh->src_port;
		from.sin_addr = ip->ip_src;
		to.sin_port = sh->dest_port;
		to.sin_addr = ip->ip_dst;

		/*
		 * 'to' holds the dest of the packet that failed to be sent.
		 * 'from' holds our local endpoint address. Thus we reverse
		 * the to and the from in the lookup.
		 */
		stcb = sctp_findassociation_addr_sa((struct sockaddr *)&from,
		    (struct sockaddr *)&to,
		    &inp, &net, 1, vrf_id);
		if (stcb != NULL && inp && (inp->sctp_socket != NULL)) {
			if (cmd != PRC_MSGSIZE) {
				sctp_notify(inp, ip, sh,
				    (struct sockaddr *)&to, stcb,
				    net);
			} else {
				/* handle possible ICMP size messages */
				sctp_notify_mbuf(inp, stcb, net, ip, sh);
			}
		} else {
			if ((stcb == NULL) && (inp != NULL)) {
				/* reduce ref-count */
				SCTP_INP_WLOCK(inp);
				SCTP_INP_DECR_REF(inp);
				SCTP_INP_WUNLOCK(inp);
			}
		}
	}
	return;
}

static int
sctp_getcred(SYSCTL_HANDLER_ARGS)
{
	struct xucred xuc;
	struct sockaddr_in addrs[2];
	struct sctp_inpcb *inp;
	struct sctp_nets *net;
	struct sctp_tcb *stcb;
	int error;
	uint32_t vrf_id;

	/* FIX, for non-bsd is this right? */
	vrf_id = SCTP_DEFAULT_VRFID;

	error = priv_check(req->td, PRIV_NETINET_GETCRED);

	if (error)
		return (error);

	error = SYSCTL_IN(req, addrs, sizeof(addrs));
	if (error)
		return (error);

	stcb = sctp_findassociation_addr_sa(sintosa(&addrs[0]),
	    sintosa(&addrs[1]),
	    &inp, &net, 1, vrf_id);
	if (stcb == NULL || inp == NULL || inp->sctp_socket == NULL) {
		if ((inp != NULL) && (stcb == NULL)) {
			/* reduce ref-count */
			SCTP_INP_WLOCK(inp);
			SCTP_INP_DECR_REF(inp);
			goto cred_can_cont;
		}
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOENT);
		error = ENOENT;
		goto out;
	}
	SCTP_TCB_UNLOCK(stcb);
	/*
	 * We use the write lock here, only since in the error leg we need
	 * it. If we used RLOCK, then we would have to
	 * wlock/decr/unlock/rlock. Which in theory could create a hole.
	 * Better to use higher wlock.
	 */
	SCTP_INP_WLOCK(inp);
cred_can_cont:
	error = cr_canseesocket(req->td->td_ucred, inp->sctp_socket);
	if (error) {
		SCTP_INP_WUNLOCK(inp);
		goto out;
	}
	cru2x(inp->sctp_socket->so_cred, &xuc);
	SCTP_INP_WUNLOCK(inp);
	error = SYSCTL_OUT(req, &xuc, sizeof(struct xucred));
out:
	return (error);
}

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, getcred, CTLTYPE_OPAQUE | CTLFLAG_RW,
    0, 0, sctp_getcred, "S,ucred", "Get the ucred of a SCTP connection");


static void
sctp_abort(struct socket *so)
{
	struct sctp_inpcb *inp;
	uint32_t flags;

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0) {
		return;
	}
sctp_must_try_again:
	flags = inp->sctp_flags;
#ifdef SCTP_LOG_CLOSING
	sctp_log_closing(inp, NULL, 17);
#endif
	if (((flags & SCTP_PCB_FLAGS_SOCKET_GONE) == 0) &&
	    (atomic_cmpset_int(&inp->sctp_flags, flags, (flags | SCTP_PCB_FLAGS_SOCKET_GONE | SCTP_PCB_FLAGS_CLOSE_IP)))) {
#ifdef SCTP_LOG_CLOSING
		sctp_log_closing(inp, NULL, 16);
#endif
		sctp_inpcb_free(inp, SCTP_FREE_SHOULD_USE_ABORT,
		    SCTP_CALLED_AFTER_CMPSET_OFCLOSE);
		SOCK_LOCK(so);
		SCTP_SB_CLEAR(so->so_snd);
		/*
		 * same for the rcv ones, they are only here for the
		 * accounting/select.
		 */
		SCTP_SB_CLEAR(so->so_rcv);

		/* Now null out the reference, we are completely detached. */
		so->so_pcb = NULL;
		SOCK_UNLOCK(so);
	} else {
		flags = inp->sctp_flags;
		if ((flags & SCTP_PCB_FLAGS_SOCKET_GONE) == 0) {
			goto sctp_must_try_again;
		}
	}
	return;
}

static int
sctp_attach(struct socket *so, int proto, struct thread *p)
{
	struct sctp_inpcb *inp;
	struct inpcb *ip_inp;
	int error;
	uint32_t vrf_id = SCTP_DEFAULT_VRFID;

#ifdef IPSEC
	uint32_t flags;

#endif
	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp != 0) {
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		return EINVAL;
	}
	error = SCTP_SORESERVE(so, SCTP_BASE_SYSCTL(sctp_sendspace), SCTP_BASE_SYSCTL(sctp_recvspace));
	if (error) {
		return error;
	}
	error = sctp_inpcb_alloc(so, vrf_id);
	if (error) {
		return error;
	}
	inp = (struct sctp_inpcb *)so->so_pcb;
	SCTP_INP_WLOCK(inp);
	inp->sctp_flags &= ~SCTP_PCB_FLAGS_BOUND_V6;	/* I'm not v6! */
	ip_inp = &inp->ip_inp.inp;
	ip_inp->inp_vflag |= INP_IPV4;
	ip_inp->inp_ip_ttl = MODULE_GLOBAL(MOD_INET, ip_defttl);
#ifdef IPSEC
	error = ipsec_init_policy(so, &ip_inp->inp_sp);
#ifdef SCTP_LOG_CLOSING
	sctp_log_closing(inp, NULL, 17);
#endif
	if (error != 0) {
		flags = inp->sctp_flags;
		if (((flags & SCTP_PCB_FLAGS_SOCKET_GONE) == 0) &&
		    (atomic_cmpset_int(&inp->sctp_flags, flags, (flags | SCTP_PCB_FLAGS_SOCKET_GONE | SCTP_PCB_FLAGS_CLOSE_IP)))) {
#ifdef SCTP_LOG_CLOSING
			sctp_log_closing(inp, NULL, 15);
#endif
			SCTP_INP_WUNLOCK(inp);
			sctp_inpcb_free(inp, SCTP_FREE_SHOULD_USE_ABORT,
			    SCTP_CALLED_AFTER_CMPSET_OFCLOSE);
		} else {
			SCTP_INP_WUNLOCK(inp);
		}
		return error;
	}
#endif				/* IPSEC */
	SCTP_INP_WUNLOCK(inp);
	return 0;
}

static int
sctp_bind(struct socket *so, struct sockaddr *addr, struct thread *p)
{
	struct sctp_inpcb *inp = NULL;
	int error;

#ifdef INET6
	if (addr && addr->sa_family != AF_INET) {
		/* must be a v4 address! */
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		return EINVAL;
	}
#endif				/* INET6 */
	if (addr && (addr->sa_len != sizeof(struct sockaddr_in))) {
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		return EINVAL;
	}
	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0) {
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		return EINVAL;
	}
	error = sctp_inpcb_bind(so, addr, NULL, p);
	return error;
}

void
sctp_close(struct socket *so)
{
	struct sctp_inpcb *inp;
	uint32_t flags;

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0)
		return;

	/*
	 * Inform all the lower layer assoc that we are done.
	 */
sctp_must_try_again:
	flags = inp->sctp_flags;
#ifdef SCTP_LOG_CLOSING
	sctp_log_closing(inp, NULL, 17);
#endif
	if (((flags & SCTP_PCB_FLAGS_SOCKET_GONE) == 0) &&
	    (atomic_cmpset_int(&inp->sctp_flags, flags, (flags | SCTP_PCB_FLAGS_SOCKET_GONE | SCTP_PCB_FLAGS_CLOSE_IP)))) {
		if (((so->so_options & SO_LINGER) && (so->so_linger == 0)) ||
		    (so->so_rcv.sb_cc > 0)) {
#ifdef SCTP_LOG_CLOSING
			sctp_log_closing(inp, NULL, 13);
#endif
			sctp_inpcb_free(inp, SCTP_FREE_SHOULD_USE_ABORT,
			    SCTP_CALLED_AFTER_CMPSET_OFCLOSE);
		} else {
#ifdef SCTP_LOG_CLOSING
			sctp_log_closing(inp, NULL, 14);
#endif
			sctp_inpcb_free(inp, SCTP_FREE_SHOULD_USE_GRACEFUL_CLOSE,
			    SCTP_CALLED_AFTER_CMPSET_OFCLOSE);
		}
		/*
		 * The socket is now detached, no matter what the state of
		 * the SCTP association.
		 */
		SOCK_LOCK(so);
		SCTP_SB_CLEAR(so->so_snd);
		/*
		 * same for the rcv ones, they are only here for the
		 * accounting/select.
		 */
		SCTP_SB_CLEAR(so->so_rcv);

		/* Now null out the reference, we are completely detached. */
		so->so_pcb = NULL;
		SOCK_UNLOCK(so);
	} else {
		flags = inp->sctp_flags;
		if ((flags & SCTP_PCB_FLAGS_SOCKET_GONE) == 0) {
			goto sctp_must_try_again;
		}
	}
	return;
}


int
sctp_sendm(struct socket *so, int flags, struct mbuf *m, struct sockaddr *addr,
    struct mbuf *control, struct thread *p);


int
sctp_sendm(struct socket *so, int flags, struct mbuf *m, struct sockaddr *addr,
    struct mbuf *control, struct thread *p)
{
	struct sctp_inpcb *inp;
	int error;

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0) {
		if (control) {
			sctp_m_freem(control);
			control = NULL;
		}
		SCTP_LTRACE_ERR_RET_PKT(m, inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		sctp_m_freem(m);
		return EINVAL;
	}
	/* Got to have an to address if we are NOT a connected socket */
	if ((addr == NULL) &&
	    ((inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) ||
	    (inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE))
	    ) {
		goto connected_type;
	} else if (addr == NULL) {
		SCTP_LTRACE_ERR_RET_PKT(m, inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EDESTADDRREQ);
		error = EDESTADDRREQ;
		sctp_m_freem(m);
		if (control) {
			sctp_m_freem(control);
			control = NULL;
		}
		return (error);
	}
#ifdef INET6
	if (addr->sa_family != AF_INET) {
		/* must be a v4 address! */
		SCTP_LTRACE_ERR_RET_PKT(m, inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EDESTADDRREQ);
		sctp_m_freem(m);
		if (control) {
			sctp_m_freem(control);
			control = NULL;
		}
		error = EDESTADDRREQ;
		return EDESTADDRREQ;
	}
#endif				/* INET6 */
connected_type:
	/* now what about control */
	if (control) {
		if (inp->control) {
			SCTP_PRINTF("huh? control set?\n");
			sctp_m_freem(inp->control);
			inp->control = NULL;
		}
		inp->control = control;
	}
	/* Place the data */
	if (inp->pkt) {
		SCTP_BUF_NEXT(inp->pkt_last) = m;
		inp->pkt_last = m;
	} else {
		inp->pkt_last = inp->pkt = m;
	}
	if (
	/* FreeBSD uses a flag passed */
	    ((flags & PRUS_MORETOCOME) == 0)
	    ) {
		/*
		 * note with the current version this code will only be used
		 * by OpenBSD-- NetBSD, FreeBSD, and MacOS have methods for
		 * re-defining sosend to use the sctp_sosend. One can
		 * optionally switch back to this code (by changing back the
		 * definitions) but this is not advisable. This code is used
		 * by FreeBSD when sending a file with sendfile() though.
		 */
		int ret;

		ret = sctp_output(inp, inp->pkt, addr, inp->control, p, flags);
		inp->pkt = NULL;
		inp->control = NULL;
		return (ret);
	} else {
		return (0);
	}
}

int
sctp_disconnect(struct socket *so)
{
	struct sctp_inpcb *inp;

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == NULL) {
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOTCONN);
		return (ENOTCONN);
	}
	SCTP_INP_RLOCK(inp);
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) ||
	    (inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL)) {
		if (SCTP_LIST_EMPTY(&inp->sctp_asoc_list)) {
			/* No connection */
			SCTP_INP_RUNLOCK(inp);
			return (0);
		} else {
			struct sctp_association *asoc;
			struct sctp_tcb *stcb;

			stcb = LIST_FIRST(&inp->sctp_asoc_list);
			if (stcb == NULL) {
				SCTP_INP_RUNLOCK(inp);
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
				return (EINVAL);
			}
			SCTP_TCB_LOCK(stcb);
			asoc = &stcb->asoc;
			if (stcb->asoc.state & SCTP_STATE_ABOUT_TO_BE_FREED) {
				/* We are about to be freed, out of here */
				SCTP_TCB_UNLOCK(stcb);
				SCTP_INP_RUNLOCK(inp);
				return (0);
			}
			if (((so->so_options & SO_LINGER) &&
			    (so->so_linger == 0)) ||
			    (so->so_rcv.sb_cc > 0)) {
				if (SCTP_GET_STATE(asoc) !=
				    SCTP_STATE_COOKIE_WAIT) {
					/* Left with Data unread */
					struct mbuf *err;

					err = sctp_get_mbuf_for_msg(sizeof(struct sctp_paramhdr), 0, M_DONTWAIT, 1, MT_DATA);
					if (err) {
						/*
						 * Fill in the user
						 * initiated abort
						 */
						struct sctp_paramhdr *ph;

						ph = mtod(err, struct sctp_paramhdr *);
						SCTP_BUF_LEN(err) = sizeof(struct sctp_paramhdr);
						ph->param_type = htons(SCTP_CAUSE_USER_INITIATED_ABT);
						ph->param_length = htons(SCTP_BUF_LEN(err));
					}
#if defined(SCTP_PANIC_ON_ABORT)
					panic("disconnect does an abort");
#endif
					sctp_send_abort_tcb(stcb, err, SCTP_SO_LOCKED);
					SCTP_STAT_INCR_COUNTER32(sctps_aborted);
				}
				SCTP_INP_RUNLOCK(inp);
				if ((SCTP_GET_STATE(&stcb->asoc) == SCTP_STATE_OPEN) ||
				    (SCTP_GET_STATE(&stcb->asoc) == SCTP_STATE_SHUTDOWN_RECEIVED)) {
					SCTP_STAT_DECR_GAUGE32(sctps_currestab);
				}
				(void)sctp_free_assoc(inp, stcb, SCTP_NORMAL_PROC, SCTP_FROM_SCTP_USRREQ + SCTP_LOC_3);
				/* No unlock tcb assoc is gone */
				return (0);
			}
			if (TAILQ_EMPTY(&asoc->send_queue) &&
			    TAILQ_EMPTY(&asoc->sent_queue) &&
			    (asoc->stream_queue_cnt == 0)) {
				/* there is nothing queued to send, so done */
				if (asoc->locked_on_sending) {
					goto abort_anyway;
				}
				if ((SCTP_GET_STATE(asoc) != SCTP_STATE_SHUTDOWN_SENT) &&
				    (SCTP_GET_STATE(asoc) != SCTP_STATE_SHUTDOWN_ACK_SENT)) {
					/* only send SHUTDOWN 1st time thru */
					sctp_stop_timers_for_shutdown(stcb);
					sctp_send_shutdown(stcb,
					    stcb->asoc.primary_destination);
					sctp_chunk_output(stcb->sctp_ep, stcb, SCTP_OUTPUT_FROM_T3, SCTP_SO_LOCKED);
					if ((SCTP_GET_STATE(asoc) == SCTP_STATE_OPEN) ||
					    (SCTP_GET_STATE(asoc) == SCTP_STATE_SHUTDOWN_RECEIVED)) {
						SCTP_STAT_DECR_GAUGE32(sctps_currestab);
					}
					SCTP_SET_STATE(asoc, SCTP_STATE_SHUTDOWN_SENT);
					SCTP_CLEAR_SUBSTATE(asoc, SCTP_STATE_SHUTDOWN_PENDING);
					sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWN,
					    stcb->sctp_ep, stcb,
					    asoc->primary_destination);
					sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD,
					    stcb->sctp_ep, stcb,
					    asoc->primary_destination);
				}
			} else {
				/*
				 * we still got (or just got) data to send,
				 * so set SHUTDOWN_PENDING
				 */
				/*
				 * XXX sockets draft says that SCTP_EOF
				 * should be sent with no data. currently,
				 * we will allow user data to be sent first
				 * and move to SHUTDOWN-PENDING
				 */
				asoc->state |= SCTP_STATE_SHUTDOWN_PENDING;
				sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD, stcb->sctp_ep, stcb,
				    asoc->primary_destination);
				if (asoc->locked_on_sending) {
					/* Locked to send out the data */
					struct sctp_stream_queue_pending *sp;

					sp = TAILQ_LAST(&asoc->locked_on_sending->outqueue, sctp_streamhead);
					if (sp == NULL) {
						SCTP_PRINTF("Error, sp is NULL, locked on sending is non-null strm:%d\n",
						    asoc->locked_on_sending->stream_no);
					} else {
						if ((sp->length == 0) && (sp->msg_is_complete == 0))
							asoc->state |= SCTP_STATE_PARTIAL_MSG_LEFT;
					}
				}
				if (TAILQ_EMPTY(&asoc->send_queue) &&
				    TAILQ_EMPTY(&asoc->sent_queue) &&
				    (asoc->state & SCTP_STATE_PARTIAL_MSG_LEFT)) {
					struct mbuf *op_err;

			abort_anyway:
					op_err = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + sizeof(uint32_t)),
					    0, M_DONTWAIT, 1, MT_DATA);
					if (op_err) {
						/*
						 * Fill in the user
						 * initiated abort
						 */
						struct sctp_paramhdr *ph;
						uint32_t *ippp;

						SCTP_BUF_LEN(op_err) =
						    (sizeof(struct sctp_paramhdr) + sizeof(uint32_t));
						ph = mtod(op_err,
						    struct sctp_paramhdr *);
						ph->param_type = htons(
						    SCTP_CAUSE_USER_INITIATED_ABT);
						ph->param_length = htons(SCTP_BUF_LEN(op_err));
						ippp = (uint32_t *) (ph + 1);
						*ippp = htonl(SCTP_FROM_SCTP_USRREQ + SCTP_LOC_4);
					}
#if defined(SCTP_PANIC_ON_ABORT)
					panic("disconnect does an abort");
#endif

					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_USRREQ + SCTP_LOC_4;
					sctp_send_abort_tcb(stcb, op_err, SCTP_SO_LOCKED);
					SCTP_STAT_INCR_COUNTER32(sctps_aborted);
					if ((SCTP_GET_STATE(&stcb->asoc) == SCTP_STATE_OPEN) ||
					    (SCTP_GET_STATE(&stcb->asoc) == SCTP_STATE_SHUTDOWN_RECEIVED)) {
						SCTP_STAT_DECR_GAUGE32(sctps_currestab);
					}
					SCTP_INP_RUNLOCK(inp);
					(void)sctp_free_assoc(inp, stcb, SCTP_NORMAL_PROC, SCTP_FROM_SCTP_USRREQ + SCTP_LOC_5);
					return (0);
				} else {
					sctp_chunk_output(inp, stcb, SCTP_OUTPUT_FROM_CLOSING, SCTP_SO_LOCKED);
				}
			}
			SCTP_TCB_UNLOCK(stcb);
			SCTP_INP_RUNLOCK(inp);
			return (0);
		}
		/* not reached */
	} else {
		/* UDP model does not support this */
		SCTP_INP_RUNLOCK(inp);
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EOPNOTSUPP);
		return EOPNOTSUPP;
	}
}

int
sctp_flush(struct socket *so, int how)
{
	/*
	 * We will just clear out the values and let subsequent close clear
	 * out the data, if any. Note if the user did a shutdown(SHUT_RD)
	 * they will not be able to read the data, the socket will block
	 * that from happening.
	 */
	if ((how == PRU_FLUSH_RD) || (how == PRU_FLUSH_RDWR)) {
		/*
		 * First make sure the sb will be happy, we don't use these
		 * except maybe the count
		 */
		so->so_rcv.sb_cc = 0;
		so->so_rcv.sb_mbcnt = 0;
		so->so_rcv.sb_mb = NULL;
	}
	if ((how == PRU_FLUSH_WR) || (how == PRU_FLUSH_RDWR)) {
		/*
		 * First make sure the sb will be happy, we don't use these
		 * except maybe the count
		 */
		so->so_snd.sb_cc = 0;
		so->so_snd.sb_mbcnt = 0;
		so->so_snd.sb_mb = NULL;

	}
	return (0);
}

int
sctp_shutdown(struct socket *so)
{
	struct sctp_inpcb *inp;

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0) {
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		return EINVAL;
	}
	SCTP_INP_RLOCK(inp);
	/* For UDP model this is a invalid call */
	if (inp->sctp_flags & SCTP_PCB_FLAGS_UDPTYPE) {
		/* Restore the flags that the soshutdown took away. */
		so->so_rcv.sb_state &= ~SBS_CANTRCVMORE;
		/* This proc will wakeup for read and do nothing (I hope) */
		SCTP_INP_RUNLOCK(inp);
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EOPNOTSUPP);
		return (EOPNOTSUPP);
	}
	/*
	 * Ok if we reach here its the TCP model and it is either a SHUT_WR
	 * or SHUT_RDWR. This means we put the shutdown flag against it.
	 */
	{
		struct sctp_tcb *stcb;
		struct sctp_association *asoc;

		socantsendmore(so);

		stcb = LIST_FIRST(&inp->sctp_asoc_list);
		if (stcb == NULL) {
			/*
			 * Ok we hit the case that the shutdown call was
			 * made after an abort or something. Nothing to do
			 * now.
			 */
			SCTP_INP_RUNLOCK(inp);
			return (0);
		}
		SCTP_TCB_LOCK(stcb);
		asoc = &stcb->asoc;
		if (TAILQ_EMPTY(&asoc->send_queue) &&
		    TAILQ_EMPTY(&asoc->sent_queue) &&
		    (asoc->stream_queue_cnt == 0)) {
			if (asoc->locked_on_sending) {
				goto abort_anyway;
			}
			/* there is nothing queued to send, so I'm done... */
			if (SCTP_GET_STATE(asoc) != SCTP_STATE_SHUTDOWN_SENT) {
				/* only send SHUTDOWN the first time through */
				sctp_stop_timers_for_shutdown(stcb);
				sctp_send_shutdown(stcb,
				    stcb->asoc.primary_destination);
				sctp_chunk_output(stcb->sctp_ep, stcb, SCTP_OUTPUT_FROM_T3, SCTP_SO_LOCKED);
				if ((SCTP_GET_STATE(asoc) == SCTP_STATE_OPEN) ||
				    (SCTP_GET_STATE(asoc) == SCTP_STATE_SHUTDOWN_RECEIVED)) {
					SCTP_STAT_DECR_GAUGE32(sctps_currestab);
				}
				SCTP_SET_STATE(asoc, SCTP_STATE_SHUTDOWN_SENT);
				SCTP_CLEAR_SUBSTATE(asoc, SCTP_STATE_SHUTDOWN_PENDING);
				sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWN,
				    stcb->sctp_ep, stcb,
				    asoc->primary_destination);
				sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD,
				    stcb->sctp_ep, stcb,
				    asoc->primary_destination);
			}
		} else {
			/*
			 * we still got (or just got) data to send, so set
			 * SHUTDOWN_PENDING
			 */
			asoc->state |= SCTP_STATE_SHUTDOWN_PENDING;
			sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD, stcb->sctp_ep, stcb,
			    asoc->primary_destination);

			if (asoc->locked_on_sending) {
				/* Locked to send out the data */
				struct sctp_stream_queue_pending *sp;

				sp = TAILQ_LAST(&asoc->locked_on_sending->outqueue, sctp_streamhead);
				if (sp == NULL) {
					SCTP_PRINTF("Error, sp is NULL, locked on sending is non-null strm:%d\n",
					    asoc->locked_on_sending->stream_no);
				} else {
					if ((sp->length == 0) && (sp->msg_is_complete == 0)) {
						asoc->state |= SCTP_STATE_PARTIAL_MSG_LEFT;
					}
				}
			}
			if (TAILQ_EMPTY(&asoc->send_queue) &&
			    TAILQ_EMPTY(&asoc->sent_queue) &&
			    (asoc->state & SCTP_STATE_PARTIAL_MSG_LEFT)) {
				struct mbuf *op_err;

		abort_anyway:
				op_err = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + sizeof(uint32_t)),
				    0, M_DONTWAIT, 1, MT_DATA);
				if (op_err) {
					/* Fill in the user initiated abort */
					struct sctp_paramhdr *ph;
					uint32_t *ippp;

					SCTP_BUF_LEN(op_err) =
					    sizeof(struct sctp_paramhdr) + sizeof(uint32_t);
					ph = mtod(op_err,
					    struct sctp_paramhdr *);
					ph->param_type = htons(
					    SCTP_CAUSE_USER_INITIATED_ABT);
					ph->param_length = htons(SCTP_BUF_LEN(op_err));
					ippp = (uint32_t *) (ph + 1);
					*ippp = htonl(SCTP_FROM_SCTP_USRREQ + SCTP_LOC_6);
				}
#if defined(SCTP_PANIC_ON_ABORT)
				panic("shutdown does an abort");
#endif
				stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_USRREQ + SCTP_LOC_6;
				sctp_abort_an_association(stcb->sctp_ep, stcb,
				    SCTP_RESPONSE_TO_USER_REQ,
				    op_err, SCTP_SO_LOCKED);
				goto skip_unlock;
			} else {
				sctp_chunk_output(inp, stcb, SCTP_OUTPUT_FROM_CLOSING, SCTP_SO_LOCKED);
			}
		}
		SCTP_TCB_UNLOCK(stcb);
	}
skip_unlock:
	SCTP_INP_RUNLOCK(inp);
	return 0;
}

/*
 * copies a "user" presentable address and removes embedded scope, etc.
 * returns 0 on success, 1 on error
 */
static uint32_t
sctp_fill_user_address(struct sockaddr_storage *ss, struct sockaddr *sa)
{
#ifdef INET6
	struct sockaddr_in6 lsa6;

	sa = (struct sockaddr *)sctp_recover_scope((struct sockaddr_in6 *)sa,
	    &lsa6);
#endif
	memcpy(ss, sa, sa->sa_len);
	return (0);
}



/*
 * NOTE: assumes addr lock is held
 */
static size_t
sctp_fill_up_addresses_vrf(struct sctp_inpcb *inp,
    struct sctp_tcb *stcb,
    size_t limit,
    struct sockaddr_storage *sas,
    uint32_t vrf_id)
{
	struct sctp_ifn *sctp_ifn;
	struct sctp_ifa *sctp_ifa;
	int loopback_scope, ipv4_local_scope, local_scope, site_scope;
	size_t actual;
	int ipv4_addr_legal, ipv6_addr_legal;
	struct sctp_vrf *vrf;

	actual = 0;
	if (limit <= 0)
		return (actual);

	if (stcb) {
		/* Turn on all the appropriate scope */
		loopback_scope = stcb->asoc.loopback_scope;
		ipv4_local_scope = stcb->asoc.ipv4_local_scope;
		local_scope = stcb->asoc.local_scope;
		site_scope = stcb->asoc.site_scope;
	} else {
		/* Turn on ALL scope, since we look at the EP */
		loopback_scope = ipv4_local_scope = local_scope =
		    site_scope = 1;
	}
	ipv4_addr_legal = ipv6_addr_legal = 0;
	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {
		ipv6_addr_legal = 1;
		if (SCTP_IPV6_V6ONLY(inp) == 0) {
			ipv4_addr_legal = 1;
		}
	} else {
		ipv4_addr_legal = 1;
	}
	vrf = sctp_find_vrf(vrf_id);
	if (vrf == NULL) {
		return (0);
	}
	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
		LIST_FOREACH(sctp_ifn, &vrf->ifnlist, next_ifn) {
			if ((loopback_scope == 0) &&
			    SCTP_IFN_IS_IFT_LOOP(sctp_ifn)) {
				/* Skip loopback if loopback_scope not set */
				continue;
			}
			LIST_FOREACH(sctp_ifa, &sctp_ifn->ifalist, next_ifa) {
				if (stcb) {
					/*
					 * For the BOUND-ALL case, the list
					 * associated with a TCB is Always
					 * considered a reverse list.. i.e.
					 * it lists addresses that are NOT
					 * part of the association. If this
					 * is one of those we must skip it.
					 */
					if (sctp_is_addr_restricted(stcb,
					    sctp_ifa)) {
						continue;
					}
				}
				switch (sctp_ifa->address.sa.sa_family) {
				case AF_INET:
					if (ipv4_addr_legal) {
						struct sockaddr_in *sin;

						sin = (struct sockaddr_in *)&sctp_ifa->address.sa;
						if (sin->sin_addr.s_addr == 0) {
							/*
							 * we skip
							 * unspecifed
							 * addresses
							 */
							continue;
						}
						if ((ipv4_local_scope == 0) &&
						    (IN4_ISPRIVATE_ADDRESS(&sin->sin_addr))) {
							continue;
						}
#ifdef INET6
						if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_NEEDS_MAPPED_V4)) {
							in6_sin_2_v4mapsin6(sin, (struct sockaddr_in6 *)sas);
							((struct sockaddr_in6 *)sas)->sin6_port = inp->sctp_lport;
							sas = (struct sockaddr_storage *)((caddr_t)sas + sizeof(struct sockaddr_in6));
							actual += sizeof(struct sockaddr_in6);
						} else {
#endif
							memcpy(sas, sin, sizeof(*sin));
							((struct sockaddr_in *)sas)->sin_port = inp->sctp_lport;
							sas = (struct sockaddr_storage *)((caddr_t)sas + sizeof(*sin));
							actual += sizeof(*sin);
#ifdef INET6
						}
#endif
						if (actual >= limit) {
							return (actual);
						}
					} else {
						continue;
					}
					break;
#ifdef INET6
				case AF_INET6:
					if (ipv6_addr_legal) {
						struct sockaddr_in6 *sin6;

						sin6 = (struct sockaddr_in6 *)&sctp_ifa->address.sa;
						if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
							/*
							 * we skip
							 * unspecifed
							 * addresses
							 */
							continue;
						}
						if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
							if (local_scope == 0)
								continue;
							if (sin6->sin6_scope_id == 0) {
								if (sa6_recoverscope(sin6) != 0)
									/*
									 * 
									 * bad
									 * 
									 * li
									 * nk
									 * 
									 * loc
									 * al
									 * 
									 * add
									 * re
									 * ss
									 * */
									continue;
							}
						}
						if ((site_scope == 0) &&
						    (IN6_IS_ADDR_SITELOCAL(&sin6->sin6_addr))) {
							continue;
						}
						memcpy(sas, sin6, sizeof(*sin6));
						((struct sockaddr_in6 *)sas)->sin6_port = inp->sctp_lport;
						sas = (struct sockaddr_storage *)((caddr_t)sas + sizeof(*sin6));
						actual += sizeof(*sin6);
						if (actual >= limit) {
							return (actual);
						}
					} else {
						continue;
					}
					break;
#endif
				default:
					/* TSNH */
					break;
				}
			}
		}
	} else {
		struct sctp_laddr *laddr;

		LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
			if (stcb) {
				if (sctp_is_addr_restricted(stcb, laddr->ifa)) {
					continue;
				}
			}
			if (sctp_fill_user_address(sas, &laddr->ifa->address.sa))
				continue;

			((struct sockaddr_in6 *)sas)->sin6_port = inp->sctp_lport;
			sas = (struct sockaddr_storage *)((caddr_t)sas +
			    laddr->ifa->address.sa.sa_len);
			actual += laddr->ifa->address.sa.sa_len;
			if (actual >= limit) {
				return (actual);
			}
		}
	}
	return (actual);
}

static size_t
sctp_fill_up_addresses(struct sctp_inpcb *inp,
    struct sctp_tcb *stcb,
    size_t limit,
    struct sockaddr_storage *sas)
{
	size_t size = 0;

	SCTP_IPI_ADDR_RLOCK();
	/* fill up addresses for the endpoint's default vrf */
	size = sctp_fill_up_addresses_vrf(inp, stcb, limit, sas,
	    inp->def_vrf_id);
	SCTP_IPI_ADDR_RUNLOCK();
	return (size);
}

/*
 * NOTE: assumes addr lock is held
 */
static int
sctp_count_max_addresses_vrf(struct sctp_inpcb *inp, uint32_t vrf_id)
{
	int cnt = 0;
	struct sctp_vrf *vrf = NULL;

	/*
	 * In both sub-set bound an bound_all cases we return the MAXIMUM
	 * number of addresses that you COULD get. In reality the sub-set
	 * bound may have an exclusion list for a given TCB OR in the
	 * bound-all case a TCB may NOT include the loopback or other
	 * addresses as well.
	 */
	vrf = sctp_find_vrf(vrf_id);
	if (vrf == NULL) {
		return (0);
	}
	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
		struct sctp_ifn *sctp_ifn;
		struct sctp_ifa *sctp_ifa;

		LIST_FOREACH(sctp_ifn, &vrf->ifnlist, next_ifn) {
			LIST_FOREACH(sctp_ifa, &sctp_ifn->ifalist, next_ifa) {
				/* Count them if they are the right type */
				if (sctp_ifa->address.sa.sa_family == AF_INET) {
					if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_NEEDS_MAPPED_V4))
						cnt += sizeof(struct sockaddr_in6);
					else
						cnt += sizeof(struct sockaddr_in);

				} else if (sctp_ifa->address.sa.sa_family == AF_INET6)
					cnt += sizeof(struct sockaddr_in6);
			}
		}
	} else {
		struct sctp_laddr *laddr;

		LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
			if (laddr->ifa->address.sa.sa_family == AF_INET) {
				if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_NEEDS_MAPPED_V4))
					cnt += sizeof(struct sockaddr_in6);
				else
					cnt += sizeof(struct sockaddr_in);

			} else if (laddr->ifa->address.sa.sa_family == AF_INET6)
				cnt += sizeof(struct sockaddr_in6);
		}
	}
	return (cnt);
}

static int
sctp_count_max_addresses(struct sctp_inpcb *inp)
{
	int cnt = 0;

	SCTP_IPI_ADDR_RLOCK();
	/* count addresses for the endpoint's default VRF */
	cnt = sctp_count_max_addresses_vrf(inp, inp->def_vrf_id);
	SCTP_IPI_ADDR_RUNLOCK();
	return (cnt);
}

static int
sctp_do_connect_x(struct socket *so, struct sctp_inpcb *inp, void *optval,
    size_t optsize, void *p, int delay)
{
	int error = 0;
	int creat_lock_on = 0;
	struct sctp_tcb *stcb = NULL;
	struct sockaddr *sa;
	int num_v6 = 0, num_v4 = 0, *totaddrp, totaddr;
	int added = 0;
	uint32_t vrf_id;
	int bad_addresses = 0;
	sctp_assoc_t *a_id;

	SCTPDBG(SCTP_DEBUG_PCB1, "Connectx called\n");

	if ((inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) &&
	    (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)) {
		/* We are already connected AND the TCP model */
		SCTP_LTRACE_ERR_RET(inp, stcb, NULL, SCTP_FROM_SCTP_USRREQ, EADDRINUSE);
		return (EADDRINUSE);
	}
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL) &&
	    (sctp_is_feature_off(inp, SCTP_PCB_FLAGS_PORTREUSE))) {
		SCTP_LTRACE_ERR_RET(inp, stcb, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		return (EINVAL);
	}
	if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
		SCTP_INP_RLOCK(inp);
		stcb = LIST_FIRST(&inp->sctp_asoc_list);
		SCTP_INP_RUNLOCK(inp);
	}
	if (stcb) {
		SCTP_LTRACE_ERR_RET(inp, stcb, NULL, SCTP_FROM_SCTP_USRREQ, EALREADY);
		return (EALREADY);
	}
	SCTP_INP_INCR_REF(inp);
	SCTP_ASOC_CREATE_LOCK(inp);
	creat_lock_on = 1;
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) ||
	    (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE)) {
		SCTP_LTRACE_ERR_RET(inp, stcb, NULL, SCTP_FROM_SCTP_USRREQ, EFAULT);
		error = EFAULT;
		goto out_now;
	}
	totaddrp = (int *)optval;
	totaddr = *totaddrp;
	sa = (struct sockaddr *)(totaddrp + 1);
	stcb = sctp_connectx_helper_find(inp, sa, &totaddr, &num_v4, &num_v6, &error, (optsize - sizeof(int)), &bad_addresses);
	if ((stcb != NULL) || bad_addresses) {
		/* Already have or am bring up an association */
		SCTP_ASOC_CREATE_UNLOCK(inp);
		creat_lock_on = 0;
		if (stcb)
			SCTP_TCB_UNLOCK(stcb);
		if (bad_addresses == 0) {
			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EALREADY);
			error = EALREADY;
		}
		goto out_now;
	}
#ifdef INET6
	if (((inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) == 0) &&
	    (num_v6 > 0)) {
		error = EINVAL;
		goto out_now;
	}
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) &&
	    (num_v4 > 0)) {
		struct in6pcb *inp6;

		inp6 = (struct in6pcb *)inp;
		if (SCTP_IPV6_V6ONLY(inp6)) {
			/*
			 * if IPV6_V6ONLY flag, ignore connections destined
			 * to a v4 addr or v4-mapped addr
			 */
			SCTP_LTRACE_ERR_RET(inp, stcb, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
			error = EINVAL;
			goto out_now;
		}
	}
#endif				/* INET6 */
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_UNBOUND) ==
	    SCTP_PCB_FLAGS_UNBOUND) {
		/* Bind a ephemeral port */
		error = sctp_inpcb_bind(so, NULL, NULL, p);
		if (error) {
			goto out_now;
		}
	}
	/* FIX ME: do we want to pass in a vrf on the connect call? */
	vrf_id = inp->def_vrf_id;


	/* We are GOOD to go */
	stcb = sctp_aloc_assoc(inp, sa, 1, &error, 0, vrf_id,
	    (struct thread *)p
	    );
	if (stcb == NULL) {
		/* Gak! no memory */
		goto out_now;
	}
	SCTP_SET_STATE(&stcb->asoc, SCTP_STATE_COOKIE_WAIT);
	/* move to second address */
	if (sa->sa_family == AF_INET)
		sa = (struct sockaddr *)((caddr_t)sa + sizeof(struct sockaddr_in));
	else
		sa = (struct sockaddr *)((caddr_t)sa + sizeof(struct sockaddr_in6));

	error = 0;
	added = sctp_connectx_helper_add(stcb, sa, (totaddr - 1), &error);
	/* Fill in the return id */
	if (error) {
		(void)sctp_free_assoc(inp, stcb, SCTP_PCBFREE_FORCE, SCTP_FROM_SCTP_USRREQ + SCTP_LOC_12);
		goto out_now;
	}
	a_id = (sctp_assoc_t *) optval;
	*a_id = sctp_get_associd(stcb);

	/* initialize authentication parameters for the assoc */
	sctp_initialize_auth_params(inp, stcb);

	if (delay) {
		/* doing delayed connection */
		stcb->asoc.delayed_connection = 1;
		sctp_timer_start(SCTP_TIMER_TYPE_INIT, inp, stcb, stcb->asoc.primary_destination);
	} else {
		(void)SCTP_GETTIME_TIMEVAL(&stcb->asoc.time_entered);
		sctp_send_initiate(inp, stcb, SCTP_SO_LOCKED);
	}
	SCTP_TCB_UNLOCK(stcb);
	if (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) {
		stcb->sctp_ep->sctp_flags |= SCTP_PCB_FLAGS_CONNECTED;
		/* Set the connected flag so we can queue data */
		soisconnecting(so);
	}
out_now:
	if (creat_lock_on) {
		SCTP_ASOC_CREATE_UNLOCK(inp);
	}
	SCTP_INP_DECR_REF(inp);
	return error;
}

#define SCTP_FIND_STCB(inp, stcb, assoc_id) { \
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) ||\
	    (inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL)) { \
		SCTP_INP_RLOCK(inp); \
		stcb = LIST_FIRST(&inp->sctp_asoc_list); \
		if (stcb) { \
			SCTP_TCB_LOCK(stcb); \
                } \
		SCTP_INP_RUNLOCK(inp); \
	} else if (assoc_id != 0) { \
		stcb = sctp_findassociation_ep_asocid(inp, assoc_id, 1); \
		if (stcb == NULL) { \
		        SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOENT); \
			error = ENOENT; \
			break; \
		} \
	} else { \
		stcb = NULL; \
        } \
  }


#define SCTP_CHECK_AND_CAST(destp, srcp, type, size)  {\
	if (size < sizeof(type)) { \
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL); \
		error = EINVAL; \
		break; \
	} else { \
		destp = (type *)srcp; \
	} \
      }

static int
sctp_getopt(struct socket *so, int optname, void *optval, size_t *optsize,
    void *p)
{
	struct sctp_inpcb *inp = NULL;
	int error, val = 0;
	struct sctp_tcb *stcb = NULL;

	if (optval == NULL) {
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		return (EINVAL);
	}
	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0) {
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		return EINVAL;
	}
	error = 0;

	switch (optname) {
	case SCTP_NODELAY:
	case SCTP_AUTOCLOSE:
	case SCTP_EXPLICIT_EOR:
	case SCTP_AUTO_ASCONF:
	case SCTP_DISABLE_FRAGMENTS:
	case SCTP_I_WANT_MAPPED_V4_ADDR:
	case SCTP_USE_EXT_RCVINFO:
		SCTP_INP_RLOCK(inp);
		switch (optname) {
		case SCTP_DISABLE_FRAGMENTS:
			val = sctp_is_feature_on(inp, SCTP_PCB_FLAGS_NO_FRAGMENT);
			break;
		case SCTP_I_WANT_MAPPED_V4_ADDR:
			val = sctp_is_feature_on(inp, SCTP_PCB_FLAGS_NEEDS_MAPPED_V4);
			break;
		case SCTP_AUTO_ASCONF:
			if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
				/* only valid for bound all sockets */
				val = sctp_is_feature_on(inp, SCTP_PCB_FLAGS_AUTO_ASCONF);
			} else {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
				error = EINVAL;
				goto flags_out;
			}
			break;
		case SCTP_EXPLICIT_EOR:
			val = sctp_is_feature_on(inp, SCTP_PCB_FLAGS_EXPLICIT_EOR);
			break;
		case SCTP_NODELAY:
			val = sctp_is_feature_on(inp, SCTP_PCB_FLAGS_NODELAY);
			break;
		case SCTP_USE_EXT_RCVINFO:
			val = sctp_is_feature_on(inp, SCTP_PCB_FLAGS_EXT_RCVINFO);
			break;
		case SCTP_AUTOCLOSE:
			if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_AUTOCLOSE))
				val = TICKS_TO_SEC(inp->sctp_ep.auto_close_time);
			else
				val = 0;
			break;

		default:
			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOPROTOOPT);
			error = ENOPROTOOPT;
		}		/* end switch (sopt->sopt_name) */
		if (optname != SCTP_AUTOCLOSE) {
			/* make it an "on/off" value */
			val = (val != 0);
		}
		if (*optsize < sizeof(val)) {
			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
			error = EINVAL;
		}
flags_out:
		SCTP_INP_RUNLOCK(inp);
		if (error == 0) {
			/* return the option value */
			*(int *)optval = val;
			*optsize = sizeof(val);
		}
		break;
	case SCTP_GET_PACKET_LOG:
		{
#ifdef  SCTP_PACKET_LOGGING
			uint8_t *target;
			int ret;

			SCTP_CHECK_AND_CAST(target, optval, uint8_t, *optsize);
			ret = sctp_copy_out_packet_log(target, (int)*optsize);
			*optsize = ret;
#else
			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EOPNOTSUPP);
			error = EOPNOTSUPP;
#endif
			break;
		}
	case SCTP_REUSE_PORT:
		{
			uint32_t *value;

			if ((inp->sctp_flags & SCTP_PCB_FLAGS_UDPTYPE)) {
				/* Can't do this for a 1-m socket */
				error = EINVAL;
				break;
			}
			SCTP_CHECK_AND_CAST(value, optval, uint32_t, *optsize);
			*value = sctp_is_feature_on(inp, SCTP_PCB_FLAGS_PORTREUSE);
			*optsize = sizeof(uint32_t);
		}
		break;
	case SCTP_PARTIAL_DELIVERY_POINT:
		{
			uint32_t *value;

			SCTP_CHECK_AND_CAST(value, optval, uint32_t, *optsize);
			*value = inp->partial_delivery_point;
			*optsize = sizeof(uint32_t);
		}
		break;
	case SCTP_FRAGMENT_INTERLEAVE:
		{
			uint32_t *value;

			SCTP_CHECK_AND_CAST(value, optval, uint32_t, *optsize);
			if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_FRAG_INTERLEAVE)) {
				if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_INTERLEAVE_STRMS)) {
					*value = SCTP_FRAG_LEVEL_2;
				} else {
					*value = SCTP_FRAG_LEVEL_1;
				}
			} else {
				*value = SCTP_FRAG_LEVEL_0;
			}
			*optsize = sizeof(uint32_t);
		}
		break;
	case SCTP_CMT_ON_OFF:
		{
			struct sctp_assoc_value *av;

			SCTP_CHECK_AND_CAST(av, optval, struct sctp_assoc_value, *optsize);
			if (SCTP_BASE_SYSCTL(sctp_cmt_on_off)) {
				SCTP_FIND_STCB(inp, stcb, av->assoc_id);
				if (stcb) {
					av->assoc_value = stcb->asoc.sctp_cmt_on_off;
					SCTP_TCB_UNLOCK(stcb);

				} else {
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOTCONN);
					error = ENOTCONN;
				}
			} else {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOPROTOOPT);
				error = ENOPROTOOPT;
			}
			*optsize = sizeof(*av);
		}
		break;
		/* JRS - Get socket option for pluggable congestion control */
	case SCTP_PLUGGABLE_CC:
		{
			struct sctp_assoc_value *av;

			SCTP_CHECK_AND_CAST(av, optval, struct sctp_assoc_value, *optsize);
			SCTP_FIND_STCB(inp, stcb, av->assoc_id);
			if (stcb) {
				av->assoc_value = stcb->asoc.congestion_control_module;
				SCTP_TCB_UNLOCK(stcb);
			} else {
				av->assoc_value = inp->sctp_ep.sctp_default_cc_module;
			}
			*optsize = sizeof(*av);
		}
		break;
	case SCTP_GET_ADDR_LEN:
		{
			struct sctp_assoc_value *av;

			SCTP_CHECK_AND_CAST(av, optval, struct sctp_assoc_value, *optsize);
			error = EINVAL;
#ifdef INET
			if (av->assoc_value == AF_INET) {
				av->assoc_value = sizeof(struct sockaddr_in);
				error = 0;
			}
#endif
#ifdef INET6
			if (av->assoc_value == AF_INET6) {
				av->assoc_value = sizeof(struct sockaddr_in6);
				error = 0;
			}
#endif
			if (error) {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, error);
			}
			*optsize = sizeof(*av);
		}
		break;
	case SCTP_GET_ASSOC_NUMBER:
		{
			uint32_t *value, cnt;

			SCTP_CHECK_AND_CAST(value, optval, uint32_t, *optsize);
			cnt = 0;
			SCTP_INP_RLOCK(inp);
			LIST_FOREACH(stcb, &inp->sctp_asoc_list, sctp_tcblist) {
				cnt++;
			}
			SCTP_INP_RUNLOCK(inp);
			*value = cnt;
			*optsize = sizeof(uint32_t);
		}
		break;

	case SCTP_GET_ASSOC_ID_LIST:
		{
			struct sctp_assoc_ids *ids;
			unsigned int at, limit;

			SCTP_CHECK_AND_CAST(ids, optval, struct sctp_assoc_ids, *optsize);
			at = 0;
			limit = *optsize / sizeof(sctp_assoc_t);
			SCTP_INP_RLOCK(inp);
			LIST_FOREACH(stcb, &inp->sctp_asoc_list, sctp_tcblist) {
				if (at < limit) {
					ids->gaids_assoc_id[at++] = sctp_get_associd(stcb);
				} else {
					error = EINVAL;
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, error);
					break;
				}
			}
			SCTP_INP_RUNLOCK(inp);
			*optsize = at * sizeof(sctp_assoc_t);
		}
		break;
	case SCTP_CONTEXT:
		{
			struct sctp_assoc_value *av;

			SCTP_CHECK_AND_CAST(av, optval, struct sctp_assoc_value, *optsize);
			SCTP_FIND_STCB(inp, stcb, av->assoc_id);

			if (stcb) {
				av->assoc_value = stcb->asoc.context;
				SCTP_TCB_UNLOCK(stcb);
			} else {
				SCTP_INP_RLOCK(inp);
				av->assoc_value = inp->sctp_context;
				SCTP_INP_RUNLOCK(inp);
			}
			*optsize = sizeof(*av);
		}
		break;
	case SCTP_VRF_ID:
		{
			uint32_t *default_vrfid;

			SCTP_CHECK_AND_CAST(default_vrfid, optval, uint32_t, *optsize);
			*default_vrfid = inp->def_vrf_id;
			break;
		}
	case SCTP_GET_ASOC_VRF:
		{
			struct sctp_assoc_value *id;

			SCTP_CHECK_AND_CAST(id, optval, struct sctp_assoc_value, *optsize);
			SCTP_FIND_STCB(inp, stcb, id->assoc_id);
			if (stcb == NULL) {
				error = EINVAL;
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, error);
				break;
			}
			id->assoc_value = stcb->asoc.vrf_id;
			break;
		}
	case SCTP_GET_VRF_IDS:
		{
			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EOPNOTSUPP);
			error = EOPNOTSUPP;
			break;
		}
	case SCTP_GET_NONCE_VALUES:
		{
			struct sctp_get_nonce_values *gnv;

			SCTP_CHECK_AND_CAST(gnv, optval, struct sctp_get_nonce_values, *optsize);
			SCTP_FIND_STCB(inp, stcb, gnv->gn_assoc_id);

			if (stcb) {
				gnv->gn_peers_tag = stcb->asoc.peer_vtag;
				gnv->gn_local_tag = stcb->asoc.my_vtag;
				SCTP_TCB_UNLOCK(stcb);
			} else {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOTCONN);
				error = ENOTCONN;
			}
			*optsize = sizeof(*gnv);
		}
		break;
	case SCTP_DELAYED_SACK:
		{
			struct sctp_sack_info *sack;

			SCTP_CHECK_AND_CAST(sack, optval, struct sctp_sack_info, *optsize);
			SCTP_FIND_STCB(inp, stcb, sack->sack_assoc_id);
			if (stcb) {
				sack->sack_delay = stcb->asoc.delayed_ack;
				sack->sack_freq = stcb->asoc.sack_freq;
				SCTP_TCB_UNLOCK(stcb);
			} else {
				SCTP_INP_RLOCK(inp);
				sack->sack_delay = TICKS_TO_MSEC(inp->sctp_ep.sctp_timeoutticks[SCTP_TIMER_RECV]);
				sack->sack_freq = inp->sctp_ep.sctp_sack_freq;
				SCTP_INP_RUNLOCK(inp);
			}
			*optsize = sizeof(*sack);
		}
		break;

	case SCTP_GET_SNDBUF_USE:
		{
			struct sctp_sockstat *ss;

			SCTP_CHECK_AND_CAST(ss, optval, struct sctp_sockstat, *optsize);
			SCTP_FIND_STCB(inp, stcb, ss->ss_assoc_id);

			if (stcb) {
				ss->ss_total_sndbuf = stcb->asoc.total_output_queue_size;
				ss->ss_total_recv_buf = (stcb->asoc.size_on_reasm_queue +
				    stcb->asoc.size_on_all_streams);
				SCTP_TCB_UNLOCK(stcb);
			} else {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOTCONN);
				error = ENOTCONN;
			}
			*optsize = sizeof(struct sctp_sockstat);
		}
		break;
	case SCTP_MAX_BURST:
		{
			uint8_t *value;

			SCTP_CHECK_AND_CAST(value, optval, uint8_t, *optsize);

			SCTP_INP_RLOCK(inp);
			*value = inp->sctp_ep.max_burst;
			SCTP_INP_RUNLOCK(inp);
			*optsize = sizeof(uint8_t);
		}
		break;
	case SCTP_MAXSEG:
		{
			struct sctp_assoc_value *av;
			int ovh;

			SCTP_CHECK_AND_CAST(av, optval, struct sctp_assoc_value, *optsize);
			SCTP_FIND_STCB(inp, stcb, av->assoc_id);

			if (stcb) {
				av->assoc_value = sctp_get_frag_point(stcb, &stcb->asoc);
				SCTP_TCB_UNLOCK(stcb);
			} else {
				SCTP_INP_RLOCK(inp);
				if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {
					ovh = SCTP_MED_OVERHEAD;
				} else {
					ovh = SCTP_MED_V4_OVERHEAD;
				}
				if (inp->sctp_frag_point >= SCTP_DEFAULT_MAXSEGMENT)
					av->assoc_value = 0;
				else
					av->assoc_value = inp->sctp_frag_point - ovh;
				SCTP_INP_RUNLOCK(inp);
			}
			*optsize = sizeof(struct sctp_assoc_value);
		}
		break;
	case SCTP_GET_STAT_LOG:
		error = sctp_fill_stat_log(optval, optsize);
		break;
	case SCTP_EVENTS:
		{
			struct sctp_event_subscribe *events;

			SCTP_CHECK_AND_CAST(events, optval, struct sctp_event_subscribe, *optsize);
			memset(events, 0, sizeof(*events));
			SCTP_INP_RLOCK(inp);
			if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_RECVDATAIOEVNT))
				events->sctp_data_io_event = 1;

			if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_RECVASSOCEVNT))
				events->sctp_association_event = 1;

			if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_RECVPADDREVNT))
				events->sctp_address_event = 1;

			if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_RECVSENDFAILEVNT))
				events->sctp_send_failure_event = 1;

			if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_RECVPEERERR))
				events->sctp_peer_error_event = 1;

			if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_RECVSHUTDOWNEVNT))
				events->sctp_shutdown_event = 1;

			if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_PDAPIEVNT))
				events->sctp_partial_delivery_event = 1;

			if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_ADAPTATIONEVNT))
				events->sctp_adaptation_layer_event = 1;

			if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_AUTHEVNT))
				events->sctp_authentication_event = 1;

			if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_STREAM_RESETEVNT))
				events->sctp_stream_reset_events = 1;
			SCTP_INP_RUNLOCK(inp);
			*optsize = sizeof(struct sctp_event_subscribe);
		}
		break;

	case SCTP_ADAPTATION_LAYER:
		{
			uint32_t *value;

			SCTP_CHECK_AND_CAST(value, optval, uint32_t, *optsize);

			SCTP_INP_RLOCK(inp);
			*value = inp->sctp_ep.adaptation_layer_indicator;
			SCTP_INP_RUNLOCK(inp);
			*optsize = sizeof(uint32_t);
		}
		break;
	case SCTP_SET_INITIAL_DBG_SEQ:
		{
			uint32_t *value;

			SCTP_CHECK_AND_CAST(value, optval, uint32_t, *optsize);
			SCTP_INP_RLOCK(inp);
			*value = inp->sctp_ep.initial_sequence_debug;
			SCTP_INP_RUNLOCK(inp);
			*optsize = sizeof(uint32_t);
		}
		break;
	case SCTP_GET_LOCAL_ADDR_SIZE:
		{
			uint32_t *value;

			SCTP_CHECK_AND_CAST(value, optval, uint32_t, *optsize);
			SCTP_INP_RLOCK(inp);
			*value = sctp_count_max_addresses(inp);
			SCTP_INP_RUNLOCK(inp);
			*optsize = sizeof(uint32_t);
		}
		break;
	case SCTP_GET_REMOTE_ADDR_SIZE:
		{
			uint32_t *value;
			size_t size;
			struct sctp_nets *net;

			SCTP_CHECK_AND_CAST(value, optval, uint32_t, *optsize);
			/* FIXME MT: change to sctp_assoc_value? */
			SCTP_FIND_STCB(inp, stcb, (sctp_assoc_t) * value);

			if (stcb) {
				size = 0;
				/* Count the sizes */
				TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
					if ((sctp_is_feature_on(inp, SCTP_PCB_FLAGS_NEEDS_MAPPED_V4)) ||
					    (((struct sockaddr *)&net->ro._l_addr)->sa_family == AF_INET6)) {
						size += sizeof(struct sockaddr_in6);
					} else if (((struct sockaddr *)&net->ro._l_addr)->sa_family == AF_INET) {
						size += sizeof(struct sockaddr_in);
					} else {
						/* huh */
						break;
					}
				}
				SCTP_TCB_UNLOCK(stcb);
				*value = (uint32_t) size;
			} else {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOTCONN);
				error = ENOTCONN;
			}
			*optsize = sizeof(uint32_t);
		}
		break;
	case SCTP_GET_PEER_ADDRESSES:
		/*
		 * Get the address information, an array is passed in to
		 * fill up we pack it.
		 */
		{
			size_t cpsz, left;
			struct sockaddr_storage *sas;
			struct sctp_nets *net;
			struct sctp_getaddresses *saddr;

			SCTP_CHECK_AND_CAST(saddr, optval, struct sctp_getaddresses, *optsize);
			SCTP_FIND_STCB(inp, stcb, saddr->sget_assoc_id);

			if (stcb) {
				left = (*optsize) - sizeof(struct sctp_getaddresses);
				*optsize = sizeof(struct sctp_getaddresses);
				sas = (struct sockaddr_storage *)&saddr->addr[0];

				TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
					if ((sctp_is_feature_on(inp, SCTP_PCB_FLAGS_NEEDS_MAPPED_V4)) ||
					    (((struct sockaddr *)&net->ro._l_addr)->sa_family == AF_INET6)) {
						cpsz = sizeof(struct sockaddr_in6);
					} else if (((struct sockaddr *)&net->ro._l_addr)->sa_family == AF_INET) {
						cpsz = sizeof(struct sockaddr_in);
					} else {
						/* huh */
						break;
					}
					if (left < cpsz) {
						/* not enough room. */
						break;
					}
#ifdef INET6
					if ((sctp_is_feature_on(inp, SCTP_PCB_FLAGS_NEEDS_MAPPED_V4)) &&
					    (((struct sockaddr *)&net->ro._l_addr)->sa_family == AF_INET)) {
						/* Must map the address */
						in6_sin_2_v4mapsin6((struct sockaddr_in *)&net->ro._l_addr,
						    (struct sockaddr_in6 *)sas);
					} else {
#endif
						memcpy(sas, &net->ro._l_addr, cpsz);
#ifdef INET6
					}
#endif
					((struct sockaddr_in *)sas)->sin_port = stcb->rport;

					sas = (struct sockaddr_storage *)((caddr_t)sas + cpsz);
					left -= cpsz;
					*optsize += cpsz;
				}
				SCTP_TCB_UNLOCK(stcb);
			} else {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOENT);
				error = ENOENT;
			}
		}
		break;
	case SCTP_GET_LOCAL_ADDRESSES:
		{
			size_t limit, actual;
			struct sockaddr_storage *sas;
			struct sctp_getaddresses *saddr;

			SCTP_CHECK_AND_CAST(saddr, optval, struct sctp_getaddresses, *optsize);
			SCTP_FIND_STCB(inp, stcb, saddr->sget_assoc_id);

			sas = (struct sockaddr_storage *)&saddr->addr[0];
			limit = *optsize - sizeof(sctp_assoc_t);
			actual = sctp_fill_up_addresses(inp, stcb, limit, sas);
			if (stcb) {
				SCTP_TCB_UNLOCK(stcb);
			}
			*optsize = sizeof(struct sockaddr_storage) + actual;
		}
		break;
	case SCTP_PEER_ADDR_PARAMS:
		{
			struct sctp_paddrparams *paddrp;
			struct sctp_nets *net;

			SCTP_CHECK_AND_CAST(paddrp, optval, struct sctp_paddrparams, *optsize);
			SCTP_FIND_STCB(inp, stcb, paddrp->spp_assoc_id);

			net = NULL;
			if (stcb) {
				net = sctp_findnet(stcb, (struct sockaddr *)&paddrp->spp_address);
			} else {
				/*
				 * We increment here since
				 * sctp_findassociation_ep_addr() wil do a
				 * decrement if it finds the stcb as long as
				 * the locked tcb (last argument) is NOT a
				 * TCB.. aka NULL.
				 */
				SCTP_INP_INCR_REF(inp);
				stcb = sctp_findassociation_ep_addr(&inp, (struct sockaddr *)&paddrp->spp_address, &net, NULL, NULL);
				if (stcb == NULL) {
					SCTP_INP_DECR_REF(inp);
				}
			}
			if (stcb && (net == NULL)) {
				struct sockaddr *sa;

				sa = (struct sockaddr *)&paddrp->spp_address;
				if (sa->sa_family == AF_INET) {
					struct sockaddr_in *sin;

					sin = (struct sockaddr_in *)sa;
					if (sin->sin_addr.s_addr) {
						error = EINVAL;
						SCTP_TCB_UNLOCK(stcb);
						SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, error);
						break;
					}
				} else if (sa->sa_family == AF_INET6) {
					struct sockaddr_in6 *sin6;

					sin6 = (struct sockaddr_in6 *)sa;
					if (!IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
						error = EINVAL;
						SCTP_TCB_UNLOCK(stcb);
						SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, error);
						break;
					}
				} else {
					error = EAFNOSUPPORT;
					SCTP_TCB_UNLOCK(stcb);
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, error);
					break;
				}
			}
			if (stcb) {
				/* Applys to the specific association */
				paddrp->spp_flags = 0;
				if (net) {
					int ovh;

					if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {
						ovh = SCTP_MED_OVERHEAD;
					} else {
						ovh = SCTP_MED_V4_OVERHEAD;
					}


					paddrp->spp_pathmaxrxt = net->failure_threshold;
					paddrp->spp_pathmtu = net->mtu - ovh;
					/* get flags for HB */
					if (net->dest_state & SCTP_ADDR_NOHB)
						paddrp->spp_flags |= SPP_HB_DISABLE;
					else
						paddrp->spp_flags |= SPP_HB_ENABLE;
					/* get flags for PMTU */
					if (SCTP_OS_TIMER_PENDING(&net->pmtu_timer.timer)) {
						paddrp->spp_flags |= SPP_PMTUD_ENABLE;
					} else {
						paddrp->spp_flags |= SPP_PMTUD_DISABLE;
					}
#ifdef INET
					if (net->ro._l_addr.sin.sin_family == AF_INET) {
						paddrp->spp_ipv4_tos = net->tos_flowlabel & 0x000000fc;
						paddrp->spp_flags |= SPP_IPV4_TOS;
					}
#endif
#ifdef INET6
					if (net->ro._l_addr.sin6.sin6_family == AF_INET6) {
						paddrp->spp_ipv6_flowlabel = net->tos_flowlabel;
						paddrp->spp_flags |= SPP_IPV6_FLOWLABEL;
					}
#endif
				} else {
					/*
					 * No destination so return default
					 * value
					 */
					int cnt = 0;

					paddrp->spp_pathmaxrxt = stcb->asoc.def_net_failure;
					paddrp->spp_pathmtu = sctp_get_frag_point(stcb, &stcb->asoc);
#ifdef INET
					paddrp->spp_ipv4_tos = stcb->asoc.default_tos & 0x000000fc;
					paddrp->spp_flags |= SPP_IPV4_TOS;
#endif
#ifdef INET6
					paddrp->spp_ipv6_flowlabel = stcb->asoc.default_flowlabel;
					paddrp->spp_flags |= SPP_IPV6_FLOWLABEL;
#endif
					/* default settings should be these */
					if (stcb->asoc.hb_is_disabled == 0) {
						paddrp->spp_flags |= SPP_HB_ENABLE;
					} else {
						paddrp->spp_flags |= SPP_HB_DISABLE;
					}
					TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
						if (SCTP_OS_TIMER_PENDING(&net->pmtu_timer.timer)) {
							cnt++;
						}
					}
					if (cnt) {
						paddrp->spp_flags |= SPP_PMTUD_ENABLE;
					}
				}
				paddrp->spp_hbinterval = stcb->asoc.heart_beat_delay;
				paddrp->spp_assoc_id = sctp_get_associd(stcb);
				SCTP_TCB_UNLOCK(stcb);
			} else {
				/* Use endpoint defaults */
				SCTP_INP_RLOCK(inp);
				paddrp->spp_pathmaxrxt = inp->sctp_ep.def_net_failure;
				paddrp->spp_hbinterval = TICKS_TO_MSEC(inp->sctp_ep.sctp_timeoutticks[SCTP_TIMER_HEARTBEAT]);
				paddrp->spp_assoc_id = (sctp_assoc_t) 0;
				/* get inp's default */
#ifdef INET
				paddrp->spp_ipv4_tos = inp->ip_inp.inp.inp_ip_tos;
				paddrp->spp_flags |= SPP_IPV4_TOS;
#endif
#ifdef INET6
				if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {
					paddrp->spp_ipv6_flowlabel = ((struct in6pcb *)inp)->in6p_flowinfo;
					paddrp->spp_flags |= SPP_IPV6_FLOWLABEL;
				}
#endif
				/* can't return this */
				paddrp->spp_pathmtu = 0;

				/* default behavior, no stcb */
				paddrp->spp_flags = SPP_PMTUD_ENABLE;

				if (sctp_is_feature_off(inp, SCTP_PCB_FLAGS_DONOT_HEARTBEAT)) {
					paddrp->spp_flags |= SPP_HB_ENABLE;
				} else {
					paddrp->spp_flags |= SPP_HB_DISABLE;
				}
				SCTP_INP_RUNLOCK(inp);
			}
			*optsize = sizeof(struct sctp_paddrparams);
		}
		break;
	case SCTP_GET_PEER_ADDR_INFO:
		{
			struct sctp_paddrinfo *paddri;
			struct sctp_nets *net;

			SCTP_CHECK_AND_CAST(paddri, optval, struct sctp_paddrinfo, *optsize);
			SCTP_FIND_STCB(inp, stcb, paddri->spinfo_assoc_id);

			net = NULL;
			if (stcb) {
				net = sctp_findnet(stcb, (struct sockaddr *)&paddri->spinfo_address);
			} else {
				/*
				 * We increment here since
				 * sctp_findassociation_ep_addr() wil do a
				 * decrement if it finds the stcb as long as
				 * the locked tcb (last argument) is NOT a
				 * TCB.. aka NULL.
				 */
				SCTP_INP_INCR_REF(inp);
				stcb = sctp_findassociation_ep_addr(&inp, (struct sockaddr *)&paddri->spinfo_address, &net, NULL, NULL);
				if (stcb == NULL) {
					SCTP_INP_DECR_REF(inp);
				}
			}

			if ((stcb) && (net)) {
				paddri->spinfo_state = net->dest_state & (SCTP_REACHABLE_MASK | SCTP_ADDR_NOHB);
				paddri->spinfo_cwnd = net->cwnd;
				paddri->spinfo_srtt = ((net->lastsa >> 2) + net->lastsv) >> 1;
				paddri->spinfo_rto = net->RTO;
				paddri->spinfo_assoc_id = sctp_get_associd(stcb);
				SCTP_TCB_UNLOCK(stcb);
			} else {
				if (stcb) {
					SCTP_TCB_UNLOCK(stcb);
				}
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOENT);
				error = ENOENT;
			}
			*optsize = sizeof(struct sctp_paddrinfo);
		}
		break;
	case SCTP_PCB_STATUS:
		{
			struct sctp_pcbinfo *spcb;

			SCTP_CHECK_AND_CAST(spcb, optval, struct sctp_pcbinfo, *optsize);
			sctp_fill_pcbinfo(spcb);
			*optsize = sizeof(struct sctp_pcbinfo);
		}
		break;

	case SCTP_STATUS:
		{
			struct sctp_nets *net;
			struct sctp_status *sstat;

			SCTP_CHECK_AND_CAST(sstat, optval, struct sctp_status, *optsize);
			SCTP_FIND_STCB(inp, stcb, sstat->sstat_assoc_id);

			if (stcb == NULL) {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, error);
				error = EINVAL;
				break;
			}
			/*
			 * I think passing the state is fine since
			 * sctp_constants.h will be available to the user
			 * land.
			 */
			sstat->sstat_state = stcb->asoc.state;
			sstat->sstat_assoc_id = sctp_get_associd(stcb);
			sstat->sstat_rwnd = stcb->asoc.peers_rwnd;
			sstat->sstat_unackdata = stcb->asoc.sent_queue_cnt;
			/*
			 * We can't include chunks that have been passed to
			 * the socket layer. Only things in queue.
			 */
			sstat->sstat_penddata = (stcb->asoc.cnt_on_reasm_queue +
			    stcb->asoc.cnt_on_all_streams);


			sstat->sstat_instrms = stcb->asoc.streamincnt;
			sstat->sstat_outstrms = stcb->asoc.streamoutcnt;
			sstat->sstat_fragmentation_point = sctp_get_frag_point(stcb, &stcb->asoc);
			memcpy(&sstat->sstat_primary.spinfo_address,
			    &stcb->asoc.primary_destination->ro._l_addr,
			    ((struct sockaddr *)(&stcb->asoc.primary_destination->ro._l_addr))->sa_len);
			net = stcb->asoc.primary_destination;
			((struct sockaddr_in *)&sstat->sstat_primary.spinfo_address)->sin_port = stcb->rport;
			/*
			 * Again the user can get info from sctp_constants.h
			 * for what the state of the network is.
			 */
			sstat->sstat_primary.spinfo_state = net->dest_state & SCTP_REACHABLE_MASK;
			sstat->sstat_primary.spinfo_cwnd = net->cwnd;
			sstat->sstat_primary.spinfo_srtt = net->lastsa;
			sstat->sstat_primary.spinfo_rto = net->RTO;
			sstat->sstat_primary.spinfo_mtu = net->mtu;
			sstat->sstat_primary.spinfo_assoc_id = sctp_get_associd(stcb);
			SCTP_TCB_UNLOCK(stcb);
			*optsize = sizeof(*sstat);
		}
		break;
	case SCTP_RTOINFO:
		{
			struct sctp_rtoinfo *srto;

			SCTP_CHECK_AND_CAST(srto, optval, struct sctp_rtoinfo, *optsize);
			SCTP_FIND_STCB(inp, stcb, srto->srto_assoc_id);

			if (stcb) {
				srto->srto_initial = stcb->asoc.initial_rto;
				srto->srto_max = stcb->asoc.maxrto;
				srto->srto_min = stcb->asoc.minrto;
				SCTP_TCB_UNLOCK(stcb);
			} else {
				SCTP_INP_RLOCK(inp);
				srto->srto_initial = inp->sctp_ep.initial_rto;
				srto->srto_max = inp->sctp_ep.sctp_maxrto;
				srto->srto_min = inp->sctp_ep.sctp_minrto;
				SCTP_INP_RUNLOCK(inp);
			}
			*optsize = sizeof(*srto);
		}
		break;
	case SCTP_ASSOCINFO:
		{
			struct sctp_assocparams *sasoc;
			uint32_t oldval;

			SCTP_CHECK_AND_CAST(sasoc, optval, struct sctp_assocparams, *optsize);
			SCTP_FIND_STCB(inp, stcb, sasoc->sasoc_assoc_id);

			if (stcb) {
				oldval = sasoc->sasoc_cookie_life;
				sasoc->sasoc_cookie_life = TICKS_TO_MSEC(stcb->asoc.cookie_life);
				sasoc->sasoc_asocmaxrxt = stcb->asoc.max_send_times;
				sasoc->sasoc_number_peer_destinations = stcb->asoc.numnets;
				sasoc->sasoc_peer_rwnd = stcb->asoc.peers_rwnd;
				sasoc->sasoc_local_rwnd = stcb->asoc.my_rwnd;
				SCTP_TCB_UNLOCK(stcb);
			} else {
				SCTP_INP_RLOCK(inp);
				sasoc->sasoc_cookie_life = TICKS_TO_MSEC(inp->sctp_ep.def_cookie_life);
				sasoc->sasoc_asocmaxrxt = inp->sctp_ep.max_send_times;
				sasoc->sasoc_number_peer_destinations = 0;
				sasoc->sasoc_peer_rwnd = 0;
				sasoc->sasoc_local_rwnd = sbspace(&inp->sctp_socket->so_rcv);
				SCTP_INP_RUNLOCK(inp);
			}
			*optsize = sizeof(*sasoc);
		}
		break;
	case SCTP_DEFAULT_SEND_PARAM:
		{
			struct sctp_sndrcvinfo *s_info;

			SCTP_CHECK_AND_CAST(s_info, optval, struct sctp_sndrcvinfo, *optsize);
			SCTP_FIND_STCB(inp, stcb, s_info->sinfo_assoc_id);

			if (stcb) {
				memcpy(s_info, &stcb->asoc.def_send, sizeof(stcb->asoc.def_send));
				SCTP_TCB_UNLOCK(stcb);
			} else {
				SCTP_INP_RLOCK(inp);
				memcpy(s_info, &inp->def_send, sizeof(inp->def_send));
				SCTP_INP_RUNLOCK(inp);
			}
			*optsize = sizeof(*s_info);
		}
		break;
	case SCTP_INITMSG:
		{
			struct sctp_initmsg *sinit;

			SCTP_CHECK_AND_CAST(sinit, optval, struct sctp_initmsg, *optsize);
			SCTP_INP_RLOCK(inp);
			sinit->sinit_num_ostreams = inp->sctp_ep.pre_open_stream_count;
			sinit->sinit_max_instreams = inp->sctp_ep.max_open_streams_intome;
			sinit->sinit_max_attempts = inp->sctp_ep.max_init_times;
			sinit->sinit_max_init_timeo = inp->sctp_ep.initial_init_rto_max;
			SCTP_INP_RUNLOCK(inp);
			*optsize = sizeof(*sinit);
		}
		break;
	case SCTP_PRIMARY_ADDR:
		/* we allow a "get" operation on this */
		{
			struct sctp_setprim *ssp;

			SCTP_CHECK_AND_CAST(ssp, optval, struct sctp_setprim, *optsize);
			SCTP_FIND_STCB(inp, stcb, ssp->ssp_assoc_id);

			if (stcb) {
				/* simply copy out the sockaddr_storage... */
				int len;

				len = *optsize;
				if (len > stcb->asoc.primary_destination->ro._l_addr.sa.sa_len)
					len = stcb->asoc.primary_destination->ro._l_addr.sa.sa_len;

				memcpy(&ssp->ssp_addr,
				    &stcb->asoc.primary_destination->ro._l_addr,
				    len);
				SCTP_TCB_UNLOCK(stcb);
			} else {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, error);
				error = EINVAL;
			}
			*optsize = sizeof(*ssp);
		}
		break;

	case SCTP_HMAC_IDENT:
		{
			struct sctp_hmacalgo *shmac;
			sctp_hmaclist_t *hmaclist;
			uint32_t size;
			int i;

			SCTP_CHECK_AND_CAST(shmac, optval, struct sctp_hmacalgo, *optsize);

			SCTP_INP_RLOCK(inp);
			hmaclist = inp->sctp_ep.local_hmacs;
			if (hmaclist == NULL) {
				/* no HMACs to return */
				*optsize = sizeof(*shmac);
				SCTP_INP_RUNLOCK(inp);
				break;
			}
			/* is there room for all of the hmac ids? */
			size = sizeof(*shmac) + (hmaclist->num_algo *
			    sizeof(shmac->shmac_idents[0]));
			if ((size_t)(*optsize) < size) {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, error);
				error = EINVAL;
				SCTP_INP_RUNLOCK(inp);
				break;
			}
			/* copy in the list */
			shmac->shmac_number_of_idents = hmaclist->num_algo;
			for (i = 0; i < hmaclist->num_algo; i++) {
				shmac->shmac_idents[i] = hmaclist->hmac[i];
			}
			SCTP_INP_RUNLOCK(inp);
			*optsize = size;
			break;
		}
	case SCTP_AUTH_ACTIVE_KEY:
		{
			struct sctp_authkeyid *scact;

			SCTP_CHECK_AND_CAST(scact, optval, struct sctp_authkeyid, *optsize);
			SCTP_FIND_STCB(inp, stcb, scact->scact_assoc_id);

			if (stcb) {
				/* get the active key on the assoc */
				scact->scact_keynumber = stcb->asoc.authinfo.assoc_keyid;
				SCTP_TCB_UNLOCK(stcb);
			} else {
				/* get the endpoint active key */
				SCTP_INP_RLOCK(inp);
				scact->scact_keynumber = inp->sctp_ep.default_keyid;
				SCTP_INP_RUNLOCK(inp);
			}
			*optsize = sizeof(*scact);
			break;
		}
	case SCTP_LOCAL_AUTH_CHUNKS:
		{
			struct sctp_authchunks *sac;
			sctp_auth_chklist_t *chklist = NULL;
			size_t size = 0;

			SCTP_CHECK_AND_CAST(sac, optval, struct sctp_authchunks, *optsize);
			SCTP_FIND_STCB(inp, stcb, sac->gauth_assoc_id);

			if (stcb) {
				/* get off the assoc */
				chklist = stcb->asoc.local_auth_chunks;
				/* is there enough space? */
				size = sctp_auth_get_chklist_size(chklist);
				if (*optsize < (sizeof(struct sctp_authchunks) + size)) {
					error = EINVAL;
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, error);
				} else {
					/* copy in the chunks */
					(void)sctp_serialize_auth_chunks(chklist, sac->gauth_chunks);
				}
				SCTP_TCB_UNLOCK(stcb);
			} else {
				/* get off the endpoint */
				SCTP_INP_RLOCK(inp);
				chklist = inp->sctp_ep.local_auth_chunks;
				/* is there enough space? */
				size = sctp_auth_get_chklist_size(chklist);
				if (*optsize < (sizeof(struct sctp_authchunks) + size)) {
					error = EINVAL;
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, error);
				} else {
					/* copy in the chunks */
					(void)sctp_serialize_auth_chunks(chklist, sac->gauth_chunks);
				}
				SCTP_INP_RUNLOCK(inp);
			}
			*optsize = sizeof(struct sctp_authchunks) + size;
			break;
		}
	case SCTP_PEER_AUTH_CHUNKS:
		{
			struct sctp_authchunks *sac;
			sctp_auth_chklist_t *chklist = NULL;
			size_t size = 0;

			SCTP_CHECK_AND_CAST(sac, optval, struct sctp_authchunks, *optsize);
			SCTP_FIND_STCB(inp, stcb, sac->gauth_assoc_id);

			if (stcb) {
				/* get off the assoc */
				chklist = stcb->asoc.peer_auth_chunks;
				/* is there enough space? */
				size = sctp_auth_get_chklist_size(chklist);
				if (*optsize < (sizeof(struct sctp_authchunks) + size)) {
					error = EINVAL;
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, error);
				} else {
					/* copy in the chunks */
					(void)sctp_serialize_auth_chunks(chklist, sac->gauth_chunks);
				}
				SCTP_TCB_UNLOCK(stcb);
			} else {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOENT);
				error = ENOENT;
			}
			*optsize = sizeof(struct sctp_authchunks) + size;
			break;
		}


	default:
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOPROTOOPT);
		error = ENOPROTOOPT;
		*optsize = 0;
		break;
	}			/* end switch (sopt->sopt_name) */
	return (error);
}

static int
sctp_setopt(struct socket *so, int optname, void *optval, size_t optsize,
    void *p)
{
	int error, set_opt;
	uint32_t *mopt;
	struct sctp_tcb *stcb = NULL;
	struct sctp_inpcb *inp = NULL;
	uint32_t vrf_id;

	if (optval == NULL) {
		SCTP_PRINTF("optval is NULL\n");
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		return (EINVAL);
	}
	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0) {
		SCTP_PRINTF("inp is NULL?\n");
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		return EINVAL;
	}
	vrf_id = inp->def_vrf_id;

	error = 0;
	switch (optname) {
	case SCTP_NODELAY:
	case SCTP_AUTOCLOSE:
	case SCTP_AUTO_ASCONF:
	case SCTP_EXPLICIT_EOR:
	case SCTP_DISABLE_FRAGMENTS:
	case SCTP_USE_EXT_RCVINFO:
	case SCTP_I_WANT_MAPPED_V4_ADDR:
		/* copy in the option value */
		SCTP_CHECK_AND_CAST(mopt, optval, uint32_t, optsize);
		set_opt = 0;
		if (error)
			break;
		switch (optname) {
		case SCTP_DISABLE_FRAGMENTS:
			set_opt = SCTP_PCB_FLAGS_NO_FRAGMENT;
			break;
		case SCTP_AUTO_ASCONF:
			/*
			 * NOTE: we don't really support this flag
			 */
			if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
				/* only valid for bound all sockets */
				set_opt = SCTP_PCB_FLAGS_AUTO_ASCONF;
			} else {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
				return (EINVAL);
			}
			break;
		case SCTP_EXPLICIT_EOR:
			set_opt = SCTP_PCB_FLAGS_EXPLICIT_EOR;
			break;
		case SCTP_USE_EXT_RCVINFO:
			set_opt = SCTP_PCB_FLAGS_EXT_RCVINFO;
			break;
		case SCTP_I_WANT_MAPPED_V4_ADDR:
			if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {
				set_opt = SCTP_PCB_FLAGS_NEEDS_MAPPED_V4;
			} else {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
				return (EINVAL);
			}
			break;
		case SCTP_NODELAY:
			set_opt = SCTP_PCB_FLAGS_NODELAY;
			break;
		case SCTP_AUTOCLOSE:
			if ((inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) ||
			    (inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL)) {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
				return (EINVAL);
			}
			set_opt = SCTP_PCB_FLAGS_AUTOCLOSE;
			/*
			 * The value is in ticks. Note this does not effect
			 * old associations, only new ones.
			 */
			inp->sctp_ep.auto_close_time = SEC_TO_TICKS(*mopt);
			break;
		}
		SCTP_INP_WLOCK(inp);
		if (*mopt != 0) {
			sctp_feature_on(inp, set_opt);
		} else {
			sctp_feature_off(inp, set_opt);
		}
		SCTP_INP_WUNLOCK(inp);
		break;
	case SCTP_REUSE_PORT:
		{
			SCTP_CHECK_AND_CAST(mopt, optval, uint32_t, optsize);
			if ((inp->sctp_flags & SCTP_PCB_FLAGS_UNBOUND) == 0) {
				/* Can't set it after we are bound */
				error = EINVAL;
				break;
			}
			if ((inp->sctp_flags & SCTP_PCB_FLAGS_UDPTYPE)) {
				/* Can't do this for a 1-m socket */
				error = EINVAL;
				break;
			}
			if (optval)
				sctp_feature_on(inp, SCTP_PCB_FLAGS_PORTREUSE);
			else
				sctp_feature_off(inp, SCTP_PCB_FLAGS_PORTREUSE);
		}
		break;
	case SCTP_PARTIAL_DELIVERY_POINT:
		{
			uint32_t *value;

			SCTP_CHECK_AND_CAST(value, optval, uint32_t, optsize);
			if (*value > SCTP_SB_LIMIT_RCV(so)) {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
				error = EINVAL;
				break;
			}
			inp->partial_delivery_point = *value;
		}
		break;
	case SCTP_FRAGMENT_INTERLEAVE:
		/* not yet until we re-write sctp_recvmsg() */
		{
			uint32_t *level;

			SCTP_CHECK_AND_CAST(level, optval, uint32_t, optsize);
			if (*level == SCTP_FRAG_LEVEL_2) {
				sctp_feature_on(inp, SCTP_PCB_FLAGS_FRAG_INTERLEAVE);
				sctp_feature_on(inp, SCTP_PCB_FLAGS_INTERLEAVE_STRMS);
			} else if (*level == SCTP_FRAG_LEVEL_1) {
				sctp_feature_on(inp, SCTP_PCB_FLAGS_FRAG_INTERLEAVE);
				sctp_feature_off(inp, SCTP_PCB_FLAGS_INTERLEAVE_STRMS);
			} else if (*level == SCTP_FRAG_LEVEL_0) {
				sctp_feature_off(inp, SCTP_PCB_FLAGS_FRAG_INTERLEAVE);
				sctp_feature_off(inp, SCTP_PCB_FLAGS_INTERLEAVE_STRMS);

			} else {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
				error = EINVAL;
			}
		}
		break;
	case SCTP_CMT_ON_OFF:
		{
			struct sctp_assoc_value *av;

			SCTP_CHECK_AND_CAST(av, optval, struct sctp_assoc_value, optsize);
			if (SCTP_BASE_SYSCTL(sctp_cmt_on_off)) {
				SCTP_FIND_STCB(inp, stcb, av->assoc_id);
				if (stcb) {
					stcb->asoc.sctp_cmt_on_off = (uint8_t) av->assoc_value;
					SCTP_TCB_UNLOCK(stcb);
				} else {
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOTCONN);
					error = ENOTCONN;
				}
			} else {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOPROTOOPT);
				error = ENOPROTOOPT;
			}
		}
		break;
		/* JRS - Set socket option for pluggable congestion control */
	case SCTP_PLUGGABLE_CC:
		{
			struct sctp_assoc_value *av;

			SCTP_CHECK_AND_CAST(av, optval, struct sctp_assoc_value, optsize);
			SCTP_FIND_STCB(inp, stcb, av->assoc_id);
			if (stcb) {
				switch (av->assoc_value) {
					/*
					 * JRS - Standard TCP congestion
					 * control
					 */
				case SCTP_CC_RFC2581:
					{
						stcb->asoc.congestion_control_module = SCTP_CC_RFC2581;
						stcb->asoc.cc_functions.sctp_set_initial_cc_param = &sctp_set_initial_cc_param;
						stcb->asoc.cc_functions.sctp_cwnd_update_after_sack = &sctp_cwnd_update_after_sack;
						stcb->asoc.cc_functions.sctp_cwnd_update_after_fr = &sctp_cwnd_update_after_fr;
						stcb->asoc.cc_functions.sctp_cwnd_update_after_timeout = &sctp_cwnd_update_after_timeout;
						stcb->asoc.cc_functions.sctp_cwnd_update_after_ecn_echo = &sctp_cwnd_update_after_ecn_echo;
						stcb->asoc.cc_functions.sctp_cwnd_update_after_packet_dropped = &sctp_cwnd_update_after_packet_dropped;
						stcb->asoc.cc_functions.sctp_cwnd_update_after_output = &sctp_cwnd_update_after_output;
						stcb->asoc.cc_functions.sctp_cwnd_update_after_fr_timer = &sctp_cwnd_update_after_fr_timer;
						SCTP_TCB_UNLOCK(stcb);
						break;
					}
					/*
					 * JRS - High Speed TCP congestion
					 * control (Floyd)
					 */
				case SCTP_CC_HSTCP:
					{
						stcb->asoc.congestion_control_module = SCTP_CC_HSTCP;
						stcb->asoc.cc_functions.sctp_set_initial_cc_param = &sctp_set_initial_cc_param;
						stcb->asoc.cc_functions.sctp_cwnd_update_after_sack = &sctp_hs_cwnd_update_after_sack;
						stcb->asoc.cc_functions.sctp_cwnd_update_after_fr = &sctp_hs_cwnd_update_after_fr;
						stcb->asoc.cc_functions.sctp_cwnd_update_after_timeout = &sctp_cwnd_update_after_timeout;
						stcb->asoc.cc_functions.sctp_cwnd_update_after_ecn_echo = &sctp_cwnd_update_after_ecn_echo;
						stcb->asoc.cc_functions.sctp_cwnd_update_after_packet_dropped = &sctp_cwnd_update_after_packet_dropped;
						stcb->asoc.cc_functions.sctp_cwnd_update_after_output = &sctp_cwnd_update_after_output;
						stcb->asoc.cc_functions.sctp_cwnd_update_after_fr_timer = &sctp_cwnd_update_after_fr_timer;
						SCTP_TCB_UNLOCK(stcb);
						break;
					}
					/* JRS - HTCP congestion control */
				case SCTP_CC_HTCP:
					{
						stcb->asoc.congestion_control_module = SCTP_CC_HTCP;
						stcb->asoc.cc_functions.sctp_set_initial_cc_param = &sctp_htcp_set_initial_cc_param;
						stcb->asoc.cc_functions.sctp_cwnd_update_after_sack = &sctp_htcp_cwnd_update_after_sack;
						stcb->asoc.cc_functions.sctp_cwnd_update_after_fr = &sctp_htcp_cwnd_update_after_fr;
						stcb->asoc.cc_functions.sctp_cwnd_update_after_timeout = &sctp_htcp_cwnd_update_after_timeout;
						stcb->asoc.cc_functions.sctp_cwnd_update_after_ecn_echo = &sctp_htcp_cwnd_update_after_ecn_echo;
						stcb->asoc.cc_functions.sctp_cwnd_update_after_packet_dropped = &sctp_cwnd_update_after_packet_dropped;
						stcb->asoc.cc_functions.sctp_cwnd_update_after_output = &sctp_cwnd_update_after_output;
						stcb->asoc.cc_functions.sctp_cwnd_update_after_fr_timer = &sctp_htcp_cwnd_update_after_fr_timer;
						SCTP_TCB_UNLOCK(stcb);
						break;
					}
					/*
					 * JRS - All other values are
					 * invalid
					 */
				default:
					{
						SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
						error = EINVAL;
						SCTP_TCB_UNLOCK(stcb);
						break;
					}
				}
			} else {
				switch (av->assoc_value) {
				case SCTP_CC_RFC2581:
				case SCTP_CC_HSTCP:
				case SCTP_CC_HTCP:
					inp->sctp_ep.sctp_default_cc_module = av->assoc_value;
					break;
				default:
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
					error = EINVAL;
					break;
				};
			}
		}
		break;
	case SCTP_CLR_STAT_LOG:
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EOPNOTSUPP);
		error = EOPNOTSUPP;
		break;
	case SCTP_CONTEXT:
		{
			struct sctp_assoc_value *av;

			SCTP_CHECK_AND_CAST(av, optval, struct sctp_assoc_value, optsize);
			SCTP_FIND_STCB(inp, stcb, av->assoc_id);

			if (stcb) {
				stcb->asoc.context = av->assoc_value;
				SCTP_TCB_UNLOCK(stcb);
			} else {
				SCTP_INP_WLOCK(inp);
				inp->sctp_context = av->assoc_value;
				SCTP_INP_WUNLOCK(inp);
			}
		}
		break;
	case SCTP_VRF_ID:
		{
			uint32_t *default_vrfid;

			SCTP_CHECK_AND_CAST(default_vrfid, optval, uint32_t, optsize);
			if (*default_vrfid > SCTP_MAX_VRF_ID) {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
				error = EINVAL;
				break;
			}
			inp->def_vrf_id = *default_vrfid;
			break;
		}
	case SCTP_DEL_VRF_ID:
		{
			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EOPNOTSUPP);
			error = EOPNOTSUPP;
			break;
		}
	case SCTP_ADD_VRF_ID:
		{
			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EOPNOTSUPP);
			error = EOPNOTSUPP;
			break;
		}
	case SCTP_DELAYED_SACK:
		{
			struct sctp_sack_info *sack;

			SCTP_CHECK_AND_CAST(sack, optval, struct sctp_sack_info, optsize);
			SCTP_FIND_STCB(inp, stcb, sack->sack_assoc_id);
			if (sack->sack_delay) {
				if (sack->sack_delay > SCTP_MAX_SACK_DELAY)
					sack->sack_delay = SCTP_MAX_SACK_DELAY;
			}
			if (stcb) {
				if (sack->sack_delay) {
					if (MSEC_TO_TICKS(sack->sack_delay) < 1) {
						sack->sack_delay = TICKS_TO_MSEC(1);
					}
					stcb->asoc.delayed_ack = sack->sack_delay;
				}
				if (sack->sack_freq) {
					stcb->asoc.sack_freq = sack->sack_freq;
				}
				SCTP_TCB_UNLOCK(stcb);
			} else {
				SCTP_INP_WLOCK(inp);
				if (sack->sack_delay) {
					if (MSEC_TO_TICKS(sack->sack_delay) < 1) {
						sack->sack_delay = TICKS_TO_MSEC(1);
					}
					inp->sctp_ep.sctp_timeoutticks[SCTP_TIMER_RECV] = MSEC_TO_TICKS(sack->sack_delay);
				}
				if (sack->sack_freq) {
					inp->sctp_ep.sctp_sack_freq = sack->sack_freq;
				}
				SCTP_INP_WUNLOCK(inp);
			}
			break;
		}
	case SCTP_AUTH_CHUNK:
		{
			struct sctp_authchunk *sauth;

			SCTP_CHECK_AND_CAST(sauth, optval, struct sctp_authchunk, optsize);

			SCTP_INP_WLOCK(inp);
			if (sctp_auth_add_chunk(sauth->sauth_chunk, inp->sctp_ep.local_auth_chunks)) {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
				error = EINVAL;
			}
			SCTP_INP_WUNLOCK(inp);
			break;
		}
	case SCTP_AUTH_KEY:
		{
			struct sctp_authkey *sca;
			struct sctp_keyhead *shared_keys;
			sctp_sharedkey_t *shared_key;
			sctp_key_t *key = NULL;
			size_t size;

			SCTP_CHECK_AND_CAST(sca, optval, struct sctp_authkey, optsize);
			SCTP_FIND_STCB(inp, stcb, sca->sca_assoc_id);
			size = optsize - sizeof(*sca);

			if (stcb) {
				/* set it on the assoc */
				shared_keys = &stcb->asoc.shared_keys;
				/* clear the cached keys for this key id */
				sctp_clear_cachedkeys(stcb, sca->sca_keynumber);
				/*
				 * create the new shared key and
				 * insert/replace it
				 */
				if (size > 0) {
					key = sctp_set_key(sca->sca_key, (uint32_t) size);
					if (key == NULL) {
						SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOMEM);
						error = ENOMEM;
						SCTP_TCB_UNLOCK(stcb);
						break;
					}
				}
				shared_key = sctp_alloc_sharedkey();
				if (shared_key == NULL) {
					sctp_free_key(key);
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOMEM);
					error = ENOMEM;
					SCTP_TCB_UNLOCK(stcb);
					break;
				}
				shared_key->key = key;
				shared_key->keyid = sca->sca_keynumber;
				sctp_insert_sharedkey(shared_keys, shared_key);
				SCTP_TCB_UNLOCK(stcb);
			} else {
				/* set it on the endpoint */
				SCTP_INP_WLOCK(inp);
				shared_keys = &inp->sctp_ep.shared_keys;
				/*
				 * clear the cached keys on all assocs for
				 * this key id
				 */
				sctp_clear_cachedkeys_ep(inp, sca->sca_keynumber);
				/*
				 * create the new shared key and
				 * insert/replace it
				 */
				if (size > 0) {
					key = sctp_set_key(sca->sca_key, (uint32_t) size);
					if (key == NULL) {
						SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOMEM);
						error = ENOMEM;
						SCTP_INP_WUNLOCK(inp);
						break;
					}
				}
				shared_key = sctp_alloc_sharedkey();
				if (shared_key == NULL) {
					sctp_free_key(key);
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOMEM);
					error = ENOMEM;
					SCTP_INP_WUNLOCK(inp);
					break;
				}
				shared_key->key = key;
				shared_key->keyid = sca->sca_keynumber;
				sctp_insert_sharedkey(shared_keys, shared_key);
				SCTP_INP_WUNLOCK(inp);
			}
			break;
		}
	case SCTP_HMAC_IDENT:
		{
			struct sctp_hmacalgo *shmac;
			sctp_hmaclist_t *hmaclist;
			uint16_t hmacid;
			uint32_t i;

			size_t found;

			SCTP_CHECK_AND_CAST(shmac, optval, struct sctp_hmacalgo, optsize);
			if (optsize < sizeof(struct sctp_hmacalgo) + shmac->shmac_number_of_idents * sizeof(uint16_t)) {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
				error = EINVAL;
				break;
			}
			hmaclist = sctp_alloc_hmaclist(shmac->shmac_number_of_idents);
			if (hmaclist == NULL) {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOMEM);
				error = ENOMEM;
				break;
			}
			for (i = 0; i < shmac->shmac_number_of_idents; i++) {
				hmacid = shmac->shmac_idents[i];
				if (sctp_auth_add_hmacid(hmaclist, hmacid)) {
					 /* invalid HMACs were found */ ;
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
					error = EINVAL;
					sctp_free_hmaclist(hmaclist);
					goto sctp_set_hmac_done;
				}
			}
			found = 0;
			for (i = 0; i < hmaclist->num_algo; i++) {
				if (hmaclist->hmac[i] == SCTP_AUTH_HMAC_ID_SHA1) {
					/* already in list */
					found = 1;
				}
			}
			if (!found) {
				sctp_free_hmaclist(hmaclist);
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
				error = EINVAL;
				break;
			}
			/* set it on the endpoint */
			SCTP_INP_WLOCK(inp);
			if (inp->sctp_ep.local_hmacs)
				sctp_free_hmaclist(inp->sctp_ep.local_hmacs);
			inp->sctp_ep.local_hmacs = hmaclist;
			SCTP_INP_WUNLOCK(inp);
	sctp_set_hmac_done:
			break;
		}
	case SCTP_AUTH_ACTIVE_KEY:
		{
			struct sctp_authkeyid *scact;

			SCTP_CHECK_AND_CAST(scact, optval, struct sctp_authkeyid, optsize);
			SCTP_FIND_STCB(inp, stcb, scact->scact_assoc_id);

			/* set the active key on the right place */
			if (stcb) {
				/* set the active key on the assoc */
				if (sctp_auth_setactivekey(stcb, scact->scact_keynumber)) {
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
					error = EINVAL;
				}
				SCTP_TCB_UNLOCK(stcb);
			} else {
				/* set the active key on the endpoint */
				SCTP_INP_WLOCK(inp);
				if (sctp_auth_setactivekey_ep(inp, scact->scact_keynumber)) {
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
					error = EINVAL;
				}
				SCTP_INP_WUNLOCK(inp);
			}
			break;
		}
	case SCTP_AUTH_DELETE_KEY:
		{
			struct sctp_authkeyid *scdel;

			SCTP_CHECK_AND_CAST(scdel, optval, struct sctp_authkeyid, optsize);
			SCTP_FIND_STCB(inp, stcb, scdel->scact_assoc_id);

			/* delete the key from the right place */
			if (stcb) {
				if (sctp_delete_sharedkey(stcb, scdel->scact_keynumber)) {
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
					error = EINVAL;
				}
				SCTP_TCB_UNLOCK(stcb);
			} else {
				SCTP_INP_WLOCK(inp);
				if (sctp_delete_sharedkey_ep(inp, scdel->scact_keynumber)) {
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
					error = EINVAL;
				}
				SCTP_INP_WUNLOCK(inp);
			}
			break;
		}

	case SCTP_RESET_STREAMS:
		{
			struct sctp_stream_reset *strrst;
			uint8_t send_in = 0, send_tsn = 0, send_out = 0;
			int i;

			SCTP_CHECK_AND_CAST(strrst, optval, struct sctp_stream_reset, optsize);
			SCTP_FIND_STCB(inp, stcb, strrst->strrst_assoc_id);

			if (stcb == NULL) {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOENT);
				error = ENOENT;
				break;
			}
			if (stcb->asoc.peer_supports_strreset == 0) {
				/*
				 * Peer does not support it, we return
				 * protocol not supported since this is true
				 * for this feature and this peer, not the
				 * socket request in general.
				 */
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EPROTONOSUPPORT);
				error = EPROTONOSUPPORT;
				SCTP_TCB_UNLOCK(stcb);
				break;
			}
			if (stcb->asoc.stream_reset_outstanding) {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EALREADY);
				error = EALREADY;
				SCTP_TCB_UNLOCK(stcb);
				break;
			}
			if (strrst->strrst_flags == SCTP_RESET_LOCAL_RECV) {
				send_in = 1;
			} else if (strrst->strrst_flags == SCTP_RESET_LOCAL_SEND) {
				send_out = 1;
			} else if (strrst->strrst_flags == SCTP_RESET_BOTH) {
				send_in = 1;
				send_out = 1;
			} else if (strrst->strrst_flags == SCTP_RESET_TSN) {
				send_tsn = 1;
			} else {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
				error = EINVAL;
				SCTP_TCB_UNLOCK(stcb);
				break;
			}
			for (i = 0; i < strrst->strrst_num_streams; i++) {
				if ((send_in) &&

				    (strrst->strrst_list[i] > stcb->asoc.streamincnt)) {
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
					error = EINVAL;
					goto get_out;
				}
				if ((send_out) &&
				    (strrst->strrst_list[i] > stcb->asoc.streamoutcnt)) {
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
					error = EINVAL;
					goto get_out;
				}
			}
			if (error) {
		get_out:
				SCTP_TCB_UNLOCK(stcb);
				break;
			}
			error = sctp_send_str_reset_req(stcb, strrst->strrst_num_streams,
			    strrst->strrst_list,
			    send_out, (stcb->asoc.str_reset_seq_in - 3),
			    send_in, send_tsn);

			sctp_chunk_output(inp, stcb, SCTP_OUTPUT_FROM_STRRST_REQ, SCTP_SO_LOCKED);
			SCTP_TCB_UNLOCK(stcb);
		}
		break;

	case SCTP_CONNECT_X:
		if (optsize < (sizeof(int) + sizeof(struct sockaddr_in))) {
			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
			error = EINVAL;
			break;
		}
		error = sctp_do_connect_x(so, inp, optval, optsize, p, 0);
		break;

	case SCTP_CONNECT_X_DELAYED:
		if (optsize < (sizeof(int) + sizeof(struct sockaddr_in))) {
			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
			error = EINVAL;
			break;
		}
		error = sctp_do_connect_x(so, inp, optval, optsize, p, 1);
		break;

	case SCTP_CONNECT_X_COMPLETE:
		{
			struct sockaddr *sa;
			struct sctp_nets *net;

			/* FIXME MT: check correct? */
			SCTP_CHECK_AND_CAST(sa, optval, struct sockaddr, optsize);

			/* find tcb */
			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				SCTP_INP_RLOCK(inp);
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb) {
					SCTP_TCB_LOCK(stcb);
					net = sctp_findnet(stcb, sa);
				}
				SCTP_INP_RUNLOCK(inp);
			} else {
				/*
				 * We increment here since
				 * sctp_findassociation_ep_addr() wil do a
				 * decrement if it finds the stcb as long as
				 * the locked tcb (last argument) is NOT a
				 * TCB.. aka NULL.
				 */
				SCTP_INP_INCR_REF(inp);
				stcb = sctp_findassociation_ep_addr(&inp, sa, &net, NULL, NULL);
				if (stcb == NULL) {
					SCTP_INP_DECR_REF(inp);
				}
			}

			if (stcb == NULL) {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOENT);
				error = ENOENT;
				break;
			}
			if (stcb->asoc.delayed_connection == 1) {
				stcb->asoc.delayed_connection = 0;
				(void)SCTP_GETTIME_TIMEVAL(&stcb->asoc.time_entered);
				sctp_timer_stop(SCTP_TIMER_TYPE_INIT, inp, stcb,
				    stcb->asoc.primary_destination,
				    SCTP_FROM_SCTP_USRREQ + SCTP_LOC_9);
				sctp_send_initiate(inp, stcb, SCTP_SO_LOCKED);
			} else {
				/*
				 * already expired or did not use delayed
				 * connectx
				 */
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EALREADY);
				error = EALREADY;
			}
			SCTP_TCB_UNLOCK(stcb);
		}
		break;
	case SCTP_MAX_BURST:
		{
			uint8_t *burst;

			SCTP_CHECK_AND_CAST(burst, optval, uint8_t, optsize);

			SCTP_INP_WLOCK(inp);
			if (*burst) {
				inp->sctp_ep.max_burst = *burst;
			}
			SCTP_INP_WUNLOCK(inp);
		}
		break;
	case SCTP_MAXSEG:
		{
			struct sctp_assoc_value *av;
			int ovh;

			SCTP_CHECK_AND_CAST(av, optval, struct sctp_assoc_value, optsize);
			SCTP_FIND_STCB(inp, stcb, av->assoc_id);

			if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {
				ovh = SCTP_MED_OVERHEAD;
			} else {
				ovh = SCTP_MED_V4_OVERHEAD;
			}
			if (stcb) {
				if (av->assoc_value) {
					stcb->asoc.sctp_frag_point = (av->assoc_value + ovh);
				} else {
					stcb->asoc.sctp_frag_point = SCTP_DEFAULT_MAXSEGMENT;
				}
				SCTP_TCB_UNLOCK(stcb);
			} else {
				SCTP_INP_WLOCK(inp);
				/*
				 * FIXME MT: I think this is not in tune
				 * with the API ID
				 */
				if (av->assoc_value) {
					inp->sctp_frag_point = (av->assoc_value + ovh);
				} else {
					inp->sctp_frag_point = SCTP_DEFAULT_MAXSEGMENT;
				}
				SCTP_INP_WUNLOCK(inp);
			}
		}
		break;
	case SCTP_EVENTS:
		{
			struct sctp_event_subscribe *events;

			SCTP_CHECK_AND_CAST(events, optval, struct sctp_event_subscribe, optsize);

			SCTP_INP_WLOCK(inp);
			if (events->sctp_data_io_event) {
				sctp_feature_on(inp, SCTP_PCB_FLAGS_RECVDATAIOEVNT);
			} else {
				sctp_feature_off(inp, SCTP_PCB_FLAGS_RECVDATAIOEVNT);
			}

			if (events->sctp_association_event) {
				sctp_feature_on(inp, SCTP_PCB_FLAGS_RECVASSOCEVNT);
			} else {
				sctp_feature_off(inp, SCTP_PCB_FLAGS_RECVASSOCEVNT);
			}

			if (events->sctp_address_event) {
				sctp_feature_on(inp, SCTP_PCB_FLAGS_RECVPADDREVNT);
			} else {
				sctp_feature_off(inp, SCTP_PCB_FLAGS_RECVPADDREVNT);
			}

			if (events->sctp_send_failure_event) {
				sctp_feature_on(inp, SCTP_PCB_FLAGS_RECVSENDFAILEVNT);
			} else {
				sctp_feature_off(inp, SCTP_PCB_FLAGS_RECVSENDFAILEVNT);
			}

			if (events->sctp_peer_error_event) {
				sctp_feature_on(inp, SCTP_PCB_FLAGS_RECVPEERERR);
			} else {
				sctp_feature_off(inp, SCTP_PCB_FLAGS_RECVPEERERR);
			}

			if (events->sctp_shutdown_event) {
				sctp_feature_on(inp, SCTP_PCB_FLAGS_RECVSHUTDOWNEVNT);
			} else {
				sctp_feature_off(inp, SCTP_PCB_FLAGS_RECVSHUTDOWNEVNT);
			}

			if (events->sctp_partial_delivery_event) {
				sctp_feature_on(inp, SCTP_PCB_FLAGS_PDAPIEVNT);
			} else {
				sctp_feature_off(inp, SCTP_PCB_FLAGS_PDAPIEVNT);
			}

			if (events->sctp_adaptation_layer_event) {
				sctp_feature_on(inp, SCTP_PCB_FLAGS_ADAPTATIONEVNT);
			} else {
				sctp_feature_off(inp, SCTP_PCB_FLAGS_ADAPTATIONEVNT);
			}

			if (events->sctp_authentication_event) {
				sctp_feature_on(inp, SCTP_PCB_FLAGS_AUTHEVNT);
			} else {
				sctp_feature_off(inp, SCTP_PCB_FLAGS_AUTHEVNT);
			}

			if (events->sctp_stream_reset_events) {
				sctp_feature_on(inp, SCTP_PCB_FLAGS_STREAM_RESETEVNT);
			} else {
				sctp_feature_off(inp, SCTP_PCB_FLAGS_STREAM_RESETEVNT);
			}
			SCTP_INP_WUNLOCK(inp);
		}
		break;

	case SCTP_ADAPTATION_LAYER:
		{
			struct sctp_setadaptation *adap_bits;

			SCTP_CHECK_AND_CAST(adap_bits, optval, struct sctp_setadaptation, optsize);
			SCTP_INP_WLOCK(inp);
			inp->sctp_ep.adaptation_layer_indicator = adap_bits->ssb_adaptation_ind;
			SCTP_INP_WUNLOCK(inp);
		}
		break;
#ifdef SCTP_DEBUG
	case SCTP_SET_INITIAL_DBG_SEQ:
		{
			uint32_t *vvv;

			SCTP_CHECK_AND_CAST(vvv, optval, uint32_t, optsize);
			SCTP_INP_WLOCK(inp);
			inp->sctp_ep.initial_sequence_debug = *vvv;
			SCTP_INP_WUNLOCK(inp);
		}
		break;
#endif
	case SCTP_DEFAULT_SEND_PARAM:
		{
			struct sctp_sndrcvinfo *s_info;

			SCTP_CHECK_AND_CAST(s_info, optval, struct sctp_sndrcvinfo, optsize);
			SCTP_FIND_STCB(inp, stcb, s_info->sinfo_assoc_id);

			if (stcb) {
				if (s_info->sinfo_stream <= stcb->asoc.streamoutcnt) {
					memcpy(&stcb->asoc.def_send, s_info, min(optsize, sizeof(stcb->asoc.def_send)));
				} else {
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
					error = EINVAL;
				}
				SCTP_TCB_UNLOCK(stcb);
			} else {
				SCTP_INP_WLOCK(inp);
				memcpy(&inp->def_send, s_info, min(optsize, sizeof(inp->def_send)));
				SCTP_INP_WUNLOCK(inp);
			}
		}
		break;
	case SCTP_PEER_ADDR_PARAMS:
		/* Applys to the specific association */
		{
			struct sctp_paddrparams *paddrp;
			struct sctp_nets *net;

			SCTP_CHECK_AND_CAST(paddrp, optval, struct sctp_paddrparams, optsize);
			SCTP_FIND_STCB(inp, stcb, paddrp->spp_assoc_id);
			net = NULL;
			if (stcb) {
				net = sctp_findnet(stcb, (struct sockaddr *)&paddrp->spp_address);
			} else {
				/*
				 * We increment here since
				 * sctp_findassociation_ep_addr() wil do a
				 * decrement if it finds the stcb as long as
				 * the locked tcb (last argument) is NOT a
				 * TCB.. aka NULL.
				 */
				SCTP_INP_INCR_REF(inp);
				stcb = sctp_findassociation_ep_addr(&inp,
				    (struct sockaddr *)&paddrp->spp_address,
				    &net, NULL, NULL);
				if (stcb == NULL) {
					SCTP_INP_DECR_REF(inp);
				}
			}
			if (stcb && (net == NULL)) {
				struct sockaddr *sa;

				sa = (struct sockaddr *)&paddrp->spp_address;
				if (sa->sa_family == AF_INET) {
					struct sockaddr_in *sin;

					sin = (struct sockaddr_in *)sa;
					if (sin->sin_addr.s_addr) {
						SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
						SCTP_TCB_UNLOCK(stcb);
						error = EINVAL;
						break;
					}
				} else if (sa->sa_family == AF_INET6) {
					struct sockaddr_in6 *sin6;

					sin6 = (struct sockaddr_in6 *)sa;
					if (!IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
						SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
						SCTP_TCB_UNLOCK(stcb);
						error = EINVAL;
						break;
					}
				} else {
					error = EAFNOSUPPORT;
					SCTP_TCB_UNLOCK(stcb);
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, error);
					break;
				}
			}
			/* sanity checks */
			if ((paddrp->spp_flags & SPP_HB_ENABLE) && (paddrp->spp_flags & SPP_HB_DISABLE)) {
				if (stcb)
					SCTP_TCB_UNLOCK(stcb);
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
				return (EINVAL);
			}
			if ((paddrp->spp_flags & SPP_PMTUD_ENABLE) && (paddrp->spp_flags & SPP_PMTUD_DISABLE)) {
				if (stcb)
					SCTP_TCB_UNLOCK(stcb);
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
				return (EINVAL);
			}
			if (stcb) {
				/************************TCB SPECIFIC SET ******************/
				/*
				 * do we change the timer for HB, we run
				 * only one?
				 */
				int ovh = 0;

				if (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {
					ovh = SCTP_MED_OVERHEAD;
				} else {
					ovh = SCTP_MED_V4_OVERHEAD;
				}

				if (paddrp->spp_hbinterval)
					stcb->asoc.heart_beat_delay = paddrp->spp_hbinterval;
				else if (paddrp->spp_flags & SPP_HB_TIME_IS_ZERO)
					stcb->asoc.heart_beat_delay = 0;

				/* network sets ? */
				if (net) {
					/************************NET SPECIFIC SET ******************/
					if (paddrp->spp_flags & SPP_HB_DEMAND) {
						/* on demand HB */
						if (sctp_send_hb(stcb, 1, net) < 0) {
							/* asoc destroyed */
							SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
							error = EINVAL;
							break;
						}
					}
					if (paddrp->spp_flags & SPP_HB_DISABLE) {
						net->dest_state |= SCTP_ADDR_NOHB;
					}
					if (paddrp->spp_flags & SPP_HB_ENABLE) {
						net->dest_state &= ~SCTP_ADDR_NOHB;
					}
					if ((paddrp->spp_flags & SPP_PMTUD_DISABLE) && (paddrp->spp_pathmtu >= SCTP_SMALLEST_PMTU)) {
						if (SCTP_OS_TIMER_PENDING(&net->pmtu_timer.timer)) {
							sctp_timer_stop(SCTP_TIMER_TYPE_PATHMTURAISE, inp, stcb, net,
							    SCTP_FROM_SCTP_USRREQ + SCTP_LOC_10);
						}
						if (paddrp->spp_pathmtu > SCTP_DEFAULT_MINSEGMENT) {
							net->mtu = paddrp->spp_pathmtu + ovh;
							if (net->mtu < stcb->asoc.smallest_mtu) {
#ifdef SCTP_PRINT_FOR_B_AND_M
								SCTP_PRINTF("SCTP_PMTU_DISABLE calls sctp_pathmtu_adjustment:%d\n",
								    net->mtu);
#endif
								sctp_pathmtu_adjustment(inp, stcb, net, net->mtu);
							}
						}
					}
					if (paddrp->spp_flags & SPP_PMTUD_ENABLE) {
						if (SCTP_OS_TIMER_PENDING(&net->pmtu_timer.timer)) {
							sctp_timer_start(SCTP_TIMER_TYPE_PATHMTURAISE, inp, stcb, net);
						}
					}
					if (paddrp->spp_pathmaxrxt)
						net->failure_threshold = paddrp->spp_pathmaxrxt;
#ifdef INET
					if (paddrp->spp_flags & SPP_IPV4_TOS) {
						if (net->ro._l_addr.sin.sin_family == AF_INET) {
							net->tos_flowlabel = paddrp->spp_ipv4_tos & 0x000000fc;
						}
					}
#endif
#ifdef INET6
					if (paddrp->spp_flags & SPP_IPV6_FLOWLABEL) {
						if (net->ro._l_addr.sin6.sin6_family == AF_INET6) {
							net->tos_flowlabel = paddrp->spp_ipv6_flowlabel;
						}
					}
#endif
				} else {
					/************************ASSOC ONLY -- NO NET SPECIFIC SET ******************/
					if (paddrp->spp_pathmaxrxt)
						stcb->asoc.def_net_failure = paddrp->spp_pathmaxrxt;

					if (paddrp->spp_flags & SPP_HB_ENABLE) {
						/* Turn back on the timer */
						stcb->asoc.hb_is_disabled = 0;
						sctp_timer_start(SCTP_TIMER_TYPE_HEARTBEAT, inp, stcb, net);
					}
					if ((paddrp->spp_flags & SPP_PMTUD_DISABLE) && (paddrp->spp_pathmtu >= SCTP_SMALLEST_PMTU)) {
						TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
							if (SCTP_OS_TIMER_PENDING(&net->pmtu_timer.timer)) {
								sctp_timer_stop(SCTP_TIMER_TYPE_PATHMTURAISE, inp, stcb, net,
								    SCTP_FROM_SCTP_USRREQ + SCTP_LOC_10);
							}
							if (paddrp->spp_pathmtu > SCTP_DEFAULT_MINSEGMENT) {
								net->mtu = paddrp->spp_pathmtu + ovh;
								if (net->mtu < stcb->asoc.smallest_mtu) {
#ifdef SCTP_PRINT_FOR_B_AND_M
									SCTP_PRINTF("SCTP_PMTU_DISABLE calls sctp_pathmtu_adjustment:%d\n",
									    net->mtu);
#endif
									sctp_pathmtu_adjustment(inp, stcb, net, net->mtu);
								}
							}
						}
					}
					if (paddrp->spp_flags & SPP_PMTUD_ENABLE) {
						TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
							if (SCTP_OS_TIMER_PENDING(&net->pmtu_timer.timer)) {
								sctp_timer_start(SCTP_TIMER_TYPE_PATHMTURAISE, inp, stcb, net);
							}
						}
					}
					if (paddrp->spp_flags & SPP_HB_DISABLE) {
						int cnt_of_unconf = 0;
						struct sctp_nets *lnet;

						stcb->asoc.hb_is_disabled = 1;
						TAILQ_FOREACH(lnet, &stcb->asoc.nets, sctp_next) {
							if (lnet->dest_state & SCTP_ADDR_UNCONFIRMED) {
								cnt_of_unconf++;
							}
						}
						/*
						 * stop the timer ONLY if we
						 * have no unconfirmed
						 * addresses
						 */
						if (cnt_of_unconf == 0) {
							TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
								sctp_timer_stop(SCTP_TIMER_TYPE_HEARTBEAT, inp, stcb, net,
								    SCTP_FROM_SCTP_USRREQ + SCTP_LOC_11);
							}
						}
					}
					if (paddrp->spp_flags & SPP_HB_ENABLE) {
						/* start up the timer. */
						TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
							sctp_timer_start(SCTP_TIMER_TYPE_HEARTBEAT, inp, stcb, net);
						}
					}
#ifdef INET
					if (paddrp->spp_flags & SPP_IPV4_TOS)
						stcb->asoc.default_tos = paddrp->spp_ipv4_tos & 0x000000fc;
#endif
#ifdef INET6
					if (paddrp->spp_flags & SPP_IPV6_FLOWLABEL)
						stcb->asoc.default_flowlabel = paddrp->spp_ipv6_flowlabel;
#endif

				}
				SCTP_TCB_UNLOCK(stcb);
			} else {
				/************************NO TCB, SET TO default stuff ******************/
				SCTP_INP_WLOCK(inp);
				/*
				 * For the TOS/FLOWLABEL stuff you set it
				 * with the options on the socket
				 */
				if (paddrp->spp_pathmaxrxt) {
					inp->sctp_ep.def_net_failure = paddrp->spp_pathmaxrxt;
				}
				if (paddrp->spp_flags & SPP_HB_TIME_IS_ZERO)
					inp->sctp_ep.sctp_timeoutticks[SCTP_TIMER_HEARTBEAT] = 0;
				else if (paddrp->spp_hbinterval) {
					if (paddrp->spp_hbinterval > SCTP_MAX_HB_INTERVAL)
						paddrp->spp_hbinterval = SCTP_MAX_HB_INTERVAL;
					inp->sctp_ep.sctp_timeoutticks[SCTP_TIMER_HEARTBEAT] = MSEC_TO_TICKS(paddrp->spp_hbinterval);
				}
				if (paddrp->spp_flags & SPP_HB_ENABLE) {
					sctp_feature_off(inp, SCTP_PCB_FLAGS_DONOT_HEARTBEAT);

				} else if (paddrp->spp_flags & SPP_HB_DISABLE) {
					sctp_feature_on(inp, SCTP_PCB_FLAGS_DONOT_HEARTBEAT);
				}
				SCTP_INP_WUNLOCK(inp);
			}
		}
		break;
	case SCTP_RTOINFO:
		{
			struct sctp_rtoinfo *srto;
			uint32_t new_init, new_min, new_max;

			SCTP_CHECK_AND_CAST(srto, optval, struct sctp_rtoinfo, optsize);
			SCTP_FIND_STCB(inp, stcb, srto->srto_assoc_id);

			if (stcb) {
				if (srto->srto_initial)
					new_init = srto->srto_initial;
				else
					new_init = stcb->asoc.initial_rto;
				if (srto->srto_max)
					new_max = srto->srto_max;
				else
					new_max = stcb->asoc.maxrto;
				if (srto->srto_min)
					new_min = srto->srto_min;
				else
					new_min = stcb->asoc.minrto;
				if ((new_min <= new_init) && (new_init <= new_max)) {
					stcb->asoc.initial_rto = new_init;
					stcb->asoc.maxrto = new_max;
					stcb->asoc.minrto = new_min;
				} else {
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
					error = EINVAL;
				}
				SCTP_TCB_UNLOCK(stcb);
			} else {
				SCTP_INP_WLOCK(inp);
				if (srto->srto_initial)
					new_init = srto->srto_initial;
				else
					new_init = inp->sctp_ep.initial_rto;
				if (srto->srto_max)
					new_max = srto->srto_max;
				else
					new_max = inp->sctp_ep.sctp_maxrto;
				if (srto->srto_min)
					new_min = srto->srto_min;
				else
					new_min = inp->sctp_ep.sctp_minrto;
				if ((new_min <= new_init) && (new_init <= new_max)) {
					inp->sctp_ep.initial_rto = new_init;
					inp->sctp_ep.sctp_maxrto = new_max;
					inp->sctp_ep.sctp_minrto = new_min;
				} else {
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
					error = EINVAL;
				}
				SCTP_INP_WUNLOCK(inp);
			}
		}
		break;
	case SCTP_ASSOCINFO:
		{
			struct sctp_assocparams *sasoc;

			SCTP_CHECK_AND_CAST(sasoc, optval, struct sctp_assocparams, optsize);
			SCTP_FIND_STCB(inp, stcb, sasoc->sasoc_assoc_id);
			if (sasoc->sasoc_cookie_life) {
				/* boundary check the cookie life */
				if (sasoc->sasoc_cookie_life < 1000)
					sasoc->sasoc_cookie_life = 1000;
				if (sasoc->sasoc_cookie_life > SCTP_MAX_COOKIE_LIFE) {
					sasoc->sasoc_cookie_life = SCTP_MAX_COOKIE_LIFE;
				}
			}
			if (stcb) {
				if (sasoc->sasoc_asocmaxrxt)
					stcb->asoc.max_send_times = sasoc->sasoc_asocmaxrxt;
				sasoc->sasoc_number_peer_destinations = stcb->asoc.numnets;
				sasoc->sasoc_peer_rwnd = 0;
				sasoc->sasoc_local_rwnd = 0;
				if (sasoc->sasoc_cookie_life) {
					stcb->asoc.cookie_life = MSEC_TO_TICKS(sasoc->sasoc_cookie_life);
				}
				SCTP_TCB_UNLOCK(stcb);
			} else {
				SCTP_INP_WLOCK(inp);
				if (sasoc->sasoc_asocmaxrxt)
					inp->sctp_ep.max_send_times = sasoc->sasoc_asocmaxrxt;
				sasoc->sasoc_number_peer_destinations = 0;
				sasoc->sasoc_peer_rwnd = 0;
				sasoc->sasoc_local_rwnd = 0;
				if (sasoc->sasoc_cookie_life) {
					inp->sctp_ep.def_cookie_life = MSEC_TO_TICKS(sasoc->sasoc_cookie_life);
				}
				SCTP_INP_WUNLOCK(inp);
			}
		}
		break;
	case SCTP_INITMSG:
		{
			struct sctp_initmsg *sinit;

			SCTP_CHECK_AND_CAST(sinit, optval, struct sctp_initmsg, optsize);
			SCTP_INP_WLOCK(inp);
			if (sinit->sinit_num_ostreams)
				inp->sctp_ep.pre_open_stream_count = sinit->sinit_num_ostreams;

			if (sinit->sinit_max_instreams)
				inp->sctp_ep.max_open_streams_intome = sinit->sinit_max_instreams;

			if (sinit->sinit_max_attempts)
				inp->sctp_ep.max_init_times = sinit->sinit_max_attempts;

			if (sinit->sinit_max_init_timeo)
				inp->sctp_ep.initial_init_rto_max = sinit->sinit_max_init_timeo;
			SCTP_INP_WUNLOCK(inp);
		}
		break;
	case SCTP_PRIMARY_ADDR:
		{
			struct sctp_setprim *spa;
			struct sctp_nets *net, *lnet;

			SCTP_CHECK_AND_CAST(spa, optval, struct sctp_setprim, optsize);
			SCTP_FIND_STCB(inp, stcb, spa->ssp_assoc_id);

			net = NULL;
			if (stcb) {
				net = sctp_findnet(stcb, (struct sockaddr *)&spa->ssp_addr);
			} else {
				/*
				 * We increment here since
				 * sctp_findassociation_ep_addr() wil do a
				 * decrement if it finds the stcb as long as
				 * the locked tcb (last argument) is NOT a
				 * TCB.. aka NULL.
				 */
				SCTP_INP_INCR_REF(inp);
				stcb = sctp_findassociation_ep_addr(&inp,
				    (struct sockaddr *)&spa->ssp_addr,
				    &net, NULL, NULL);
				if (stcb == NULL) {
					SCTP_INP_DECR_REF(inp);
				}
			}

			if ((stcb) && (net)) {
				if ((net != stcb->asoc.primary_destination) &&
				    (!(net->dest_state & SCTP_ADDR_UNCONFIRMED))) {
					/* Ok we need to set it */
					lnet = stcb->asoc.primary_destination;
					if (sctp_set_primary_addr(stcb, (struct sockaddr *)NULL, net) == 0) {
						if (net->dest_state & SCTP_ADDR_SWITCH_PRIMARY) {
							net->dest_state |= SCTP_ADDR_DOUBLE_SWITCH;
						}
						net->dest_state |= SCTP_ADDR_SWITCH_PRIMARY;
					}
				}
			} else {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
				error = EINVAL;
			}
			if (stcb) {
				SCTP_TCB_UNLOCK(stcb);
			}
		}
		break;
	case SCTP_SET_DYNAMIC_PRIMARY:
		{
			union sctp_sockstore *ss;

			error = priv_check(curthread,
			    PRIV_NETINET_RESERVEDPORT);
			if (error)
				break;

			SCTP_CHECK_AND_CAST(ss, optval, union sctp_sockstore, optsize);
			/* SUPER USER CHECK? */
			error = sctp_dynamic_set_primary(&ss->sa, vrf_id);
		}
		break;
	case SCTP_SET_PEER_PRIMARY_ADDR:
		{
			struct sctp_setpeerprim *sspp;

			SCTP_CHECK_AND_CAST(sspp, optval, struct sctp_setpeerprim, optsize);
			SCTP_FIND_STCB(inp, stcb, sspp->sspp_assoc_id);
			if (stcb != NULL) {
				struct sctp_ifa *ifa;

				ifa = sctp_find_ifa_by_addr((struct sockaddr *)&sspp->sspp_addr,
				    stcb->asoc.vrf_id, SCTP_ADDR_NOT_LOCKED);
				if (ifa == NULL) {
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
					error = EINVAL;
					goto out_of_it;
				}
				if ((inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) == 0) {
					/*
					 * Must validate the ifa found is in
					 * our ep
					 */
					struct sctp_laddr *laddr;
					int found = 0;

					LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
						if (laddr->ifa == NULL) {
							SCTPDBG(SCTP_DEBUG_OUTPUT1, "%s: NULL ifa\n",
							    __FUNCTION__);
							continue;
						}
						if (laddr->ifa == ifa) {
							found = 1;
							break;
						}
					}
					if (!found) {
						SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
						error = EINVAL;
						goto out_of_it;
					}
				}
				if (sctp_set_primary_ip_address_sa(stcb,
				    (struct sockaddr *)&sspp->sspp_addr) != 0) {
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
					error = EINVAL;
				}
		out_of_it:
				SCTP_TCB_UNLOCK(stcb);
			} else {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
				error = EINVAL;
			}

		}
		break;
	case SCTP_BINDX_ADD_ADDR:
		{
			struct sctp_getaddresses *addrs;
			size_t sz;
			struct thread *td;
			int prison = 0;

			td = (struct thread *)p;
			if (jailed(td->td_ucred)) {
				prison = 1;
			}
			SCTP_CHECK_AND_CAST(addrs, optval, struct sctp_getaddresses,
			    optsize);
			if (addrs->addr->sa_family == AF_INET) {
				sz = sizeof(struct sctp_getaddresses) - sizeof(struct sockaddr) + sizeof(struct sockaddr_in);
				if (optsize < sz) {
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
					error = EINVAL;
					break;
				}
				if (prison && prison_ip(td->td_ucred, 0, &(((struct sockaddr_in *)(addrs->addr))->sin_addr.s_addr))) {
					SCTP_LTRACE_ERR_RET(inp, stcb, NULL, SCTP_FROM_SCTP_USRREQ, EADDRNOTAVAIL);
					error = EADDRNOTAVAIL;
				}
			} else if (addrs->addr->sa_family == AF_INET6) {
				sz = sizeof(struct sctp_getaddresses) - sizeof(struct sockaddr) + sizeof(struct sockaddr_in6);
				if (optsize < sz) {
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
					error = EINVAL;
					break;
				}
				/* JAIL XXXX Add else here for V6 */
			}
			sctp_bindx_add_address(so, inp, addrs->addr,
			    addrs->sget_assoc_id, vrf_id,
			    &error, p);
		}
		break;
	case SCTP_BINDX_REM_ADDR:
		{
			struct sctp_getaddresses *addrs;
			size_t sz;
			struct thread *td;
			int prison = 0;

			td = (struct thread *)p;
			if (jailed(td->td_ucred)) {
				prison = 1;
			}
			SCTP_CHECK_AND_CAST(addrs, optval, struct sctp_getaddresses, optsize);
			if (addrs->addr->sa_family == AF_INET) {
				sz = sizeof(struct sctp_getaddresses) - sizeof(struct sockaddr) + sizeof(struct sockaddr_in);
				if (optsize < sz) {
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
					error = EINVAL;
					break;
				}
				if (prison && prison_ip(td->td_ucred, 0, &(((struct sockaddr_in *)(addrs->addr))->sin_addr.s_addr))) {
					SCTP_LTRACE_ERR_RET(inp, stcb, NULL, SCTP_FROM_SCTP_USRREQ, EADDRNOTAVAIL);
					error = EADDRNOTAVAIL;
				}
			} else if (addrs->addr->sa_family == AF_INET6) {
				sz = sizeof(struct sctp_getaddresses) - sizeof(struct sockaddr) + sizeof(struct sockaddr_in6);
				if (optsize < sz) {
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
					error = EINVAL;
					break;
				}
				/* JAIL XXXX Add else here for V6 */
			}
			sctp_bindx_delete_address(so, inp, addrs->addr,
			    addrs->sget_assoc_id, vrf_id,
			    &error);
		}
		break;
	default:
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOPROTOOPT);
		error = ENOPROTOOPT;
		break;
	}			/* end switch (opt) */
	return (error);
}

int
sctp_ctloutput(struct socket *so, struct sockopt *sopt)
{
	void *optval = NULL;
	size_t optsize = 0;
	struct sctp_inpcb *inp;
	void *p;
	int error = 0;

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0) {
		/* I made the same as TCP since we are not setup? */
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		return (ECONNRESET);
	}
	if (sopt->sopt_level != IPPROTO_SCTP) {
		/* wrong proto level... send back up to IP */
#ifdef INET6
		if (INP_CHECK_SOCKAF(so, AF_INET6))
			error = ip6_ctloutput(so, sopt);
		else
#endif				/* INET6 */
			error = ip_ctloutput(so, sopt);
		return (error);
	}
	optsize = sopt->sopt_valsize;
	if (optsize) {
		SCTP_MALLOC(optval, void *, optsize, SCTP_M_SOCKOPT);
		if (optval == NULL) {
			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOBUFS);
			return (ENOBUFS);
		}
		error = sooptcopyin(sopt, optval, optsize, optsize);
		if (error) {
			SCTP_FREE(optval, SCTP_M_SOCKOPT);
			goto out;
		}
	}
	p = (void *)sopt->sopt_td;
	if (sopt->sopt_dir == SOPT_SET) {
		error = sctp_setopt(so, sopt->sopt_name, optval, optsize, p);
	} else if (sopt->sopt_dir == SOPT_GET) {
		error = sctp_getopt(so, sopt->sopt_name, optval, &optsize, p);
	} else {
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		error = EINVAL;
	}
	if ((error == 0) && (optval != NULL)) {
		error = sooptcopyout(sopt, optval, optsize);
		SCTP_FREE(optval, SCTP_M_SOCKOPT);
	} else if (optval != NULL) {
		SCTP_FREE(optval, SCTP_M_SOCKOPT);
	}
out:
	return (error);
}


static int
sctp_connect(struct socket *so, struct sockaddr *addr, struct thread *p)
{
	int error = 0;
	int create_lock_on = 0;
	uint32_t vrf_id;
	struct sctp_inpcb *inp;
	struct sctp_tcb *stcb = NULL;

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0) {
		/* I made the same as TCP since we are not setup? */
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		return (ECONNRESET);
	}
	if (addr == NULL) {
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		return EINVAL;
	}
	if ((addr->sa_family == AF_INET6) && (addr->sa_len != sizeof(struct sockaddr_in6))) {
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		return (EINVAL);
	}
	if ((addr->sa_family == AF_INET) && (addr->sa_len != sizeof(struct sockaddr_in))) {
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		return (EINVAL);
	}
	SCTP_INP_INCR_REF(inp);
	SCTP_ASOC_CREATE_LOCK(inp);
	create_lock_on = 1;

	if ((inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) ||
	    (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE)) {
		/* Should I really unlock ? */
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EFAULT);
		error = EFAULT;
		goto out_now;
	}
#ifdef INET6
	if (((inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) == 0) &&
	    (addr->sa_family == AF_INET6)) {
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		error = EINVAL;
		goto out_now;
	}
#endif				/* INET6 */
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_UNBOUND) ==
	    SCTP_PCB_FLAGS_UNBOUND) {
		/* Bind a ephemeral port */
		error = sctp_inpcb_bind(so, NULL, NULL, p);
		if (error) {
			goto out_now;
		}
	}
	/* Now do we connect? */
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL) &&
	    (sctp_is_feature_off(inp, SCTP_PCB_FLAGS_PORTREUSE))) {
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		error = EINVAL;
		goto out_now;
	}
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) &&
	    (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)) {
		/* We are already connected AND the TCP model */
		SCTP_LTRACE_ERR_RET(inp, stcb, NULL, SCTP_FROM_SCTP_USRREQ, EADDRINUSE);
		error = EADDRINUSE;
		goto out_now;
	}
	if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
		SCTP_INP_RLOCK(inp);
		stcb = LIST_FIRST(&inp->sctp_asoc_list);
		SCTP_INP_RUNLOCK(inp);
	} else {
		/*
		 * We increment here since sctp_findassociation_ep_addr()
		 * will do a decrement if it finds the stcb as long as the
		 * locked tcb (last argument) is NOT a TCB.. aka NULL.
		 */
		SCTP_INP_INCR_REF(inp);
		stcb = sctp_findassociation_ep_addr(&inp, addr, NULL, NULL, NULL);
		if (stcb == NULL) {
			SCTP_INP_DECR_REF(inp);
		} else {
			SCTP_TCB_UNLOCK(stcb);
		}
	}
	if (stcb != NULL) {
		/* Already have or am bring up an association */
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EALREADY);
		error = EALREADY;
		goto out_now;
	}
	vrf_id = inp->def_vrf_id;
	/* We are GOOD to go */
	stcb = sctp_aloc_assoc(inp, addr, 1, &error, 0, vrf_id, p);
	if (stcb == NULL) {
		/* Gak! no memory */
		goto out_now;
	}
	if (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) {
		stcb->sctp_ep->sctp_flags |= SCTP_PCB_FLAGS_CONNECTED;
		/* Set the connected flag so we can queue data */
		soisconnecting(so);
	}
	SCTP_SET_STATE(&stcb->asoc, SCTP_STATE_COOKIE_WAIT);
	(void)SCTP_GETTIME_TIMEVAL(&stcb->asoc.time_entered);

	/* initialize authentication parameters for the assoc */
	sctp_initialize_auth_params(inp, stcb);

	sctp_send_initiate(inp, stcb, SCTP_SO_LOCKED);
	SCTP_TCB_UNLOCK(stcb);
out_now:
	if (create_lock_on) {
		SCTP_ASOC_CREATE_UNLOCK(inp);
	}
	SCTP_INP_DECR_REF(inp);
	return error;
}

int
sctp_listen(struct socket *so, int backlog, struct thread *p)
{
	/*
	 * Note this module depends on the protocol processing being called
	 * AFTER any socket level flags and backlog are applied to the
	 * socket. The traditional way that the socket flags are applied is
	 * AFTER protocol processing. We have made a change to the
	 * sys/kern/uipc_socket.c module to reverse this but this MUST be in
	 * place if the socket API for SCTP is to work properly.
	 */

	int error = 0;
	struct sctp_inpcb *inp;

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0) {
		/* I made the same as TCP since we are not setup? */
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		return (ECONNRESET);
	}
	if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_PORTREUSE)) {
		/* See if we have a listener */
		struct sctp_inpcb *tinp;
		union sctp_sockstore store, *sp;

		sp = &store;
		if ((inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) == 0) {
			/* not bound all */
			struct sctp_laddr *laddr;

			LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
				memcpy(&store, &laddr->ifa->address, sizeof(store));
				sp->sin.sin_port = inp->sctp_lport;
				tinp = sctp_pcb_findep(&sp->sa, 0, 0, inp->def_vrf_id);
				if (tinp && (tinp != inp) &&
				    ((tinp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) == 0) &&
				    ((tinp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) == 0) &&
				    (tinp->sctp_socket->so_qlimit)) {
					/*
					 * we have a listener already and
					 * its not this inp.
					 */
					SCTP_INP_DECR_REF(tinp);
					return (EADDRINUSE);
				} else if (tinp) {
					SCTP_INP_DECR_REF(tinp);
				}
			}
		} else {
			/* Setup a local addr bound all */
			memset(&store, 0, sizeof(store));
			store.sin.sin_port = inp->sctp_lport;
#ifdef INET6
			if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {
				store.sa.sa_family = AF_INET6;
				store.sa.sa_len = sizeof(struct sockaddr_in6);
			}
#endif
			if ((inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) == 0) {
				store.sa.sa_family = AF_INET;
				store.sa.sa_len = sizeof(struct sockaddr_in);
			}
			tinp = sctp_pcb_findep(&sp->sa, 0, 0, inp->def_vrf_id);
			if (tinp && (tinp != inp) &&
			    ((tinp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) == 0) &&
			    ((tinp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) == 0) &&
			    (tinp->sctp_socket->so_qlimit)) {
				/*
				 * we have a listener already and its not
				 * this inp.
				 */
				SCTP_INP_DECR_REF(tinp);
				return (EADDRINUSE);
			} else if (tinp) {
				SCTP_INP_DECR_REF(inp);
			}
		}
	}
	SCTP_INP_RLOCK(inp);
#ifdef SCTP_LOCK_LOGGING
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_LOCK_LOGGING_ENABLE) {
		sctp_log_lock(inp, (struct sctp_tcb *)NULL, SCTP_LOG_LOCK_SOCK);
	}
#endif
	SOCK_LOCK(so);
	error = solisten_proto_check(so);
	if (error) {
		SOCK_UNLOCK(so);
		SCTP_INP_RUNLOCK(inp);
		return (error);
	}
	if ((sctp_is_feature_on(inp, SCTP_PCB_FLAGS_PORTREUSE)) &&
	    (inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL)) {
		/*
		 * The unlucky case - We are in the tcp pool with this guy.
		 * - Someone else is in the main inp slot. - We must move
		 * this guy (the listener) to the main slot - We must then
		 * move the guy that was listener to the TCP Pool.
		 */
		if (sctp_swap_inpcb_for_listen(inp)) {
			goto in_use;
		}
	}
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) &&
	    (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)) {
		/* We are already connected AND the TCP model */
in_use:
		SCTP_INP_RUNLOCK(inp);
		SOCK_UNLOCK(so);
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EADDRINUSE);
		return (EADDRINUSE);
	}
	SCTP_INP_RUNLOCK(inp);
	if (inp->sctp_flags & SCTP_PCB_FLAGS_UNBOUND) {
		/* We must do a bind. */
		SOCK_UNLOCK(so);
		if ((error = sctp_inpcb_bind(so, NULL, NULL, p))) {
			/* bind error, probably perm */
			return (error);
		}
		SOCK_LOCK(so);
	}
	/* It appears for 7.0 and on, we must always call this. */
	solisten_proto(so, backlog);
	if (inp->sctp_flags & SCTP_PCB_FLAGS_UDPTYPE) {
		/* remove the ACCEPTCONN flag for one-to-many sockets */
		so->so_options &= ~SO_ACCEPTCONN;
	}
	if (backlog == 0) {
		/* turning off listen */
		so->so_options &= ~SO_ACCEPTCONN;
	}
	SOCK_UNLOCK(so);
	return (error);
}

static int sctp_defered_wakeup_cnt = 0;

int
sctp_accept(struct socket *so, struct sockaddr **addr)
{
	struct sctp_tcb *stcb;
	struct sctp_inpcb *inp;
	union sctp_sockstore store;

#ifdef INET6
	int error;

#endif
	inp = (struct sctp_inpcb *)so->so_pcb;

	if (inp == 0) {
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		return (ECONNRESET);
	}
	SCTP_INP_RLOCK(inp);
	if (inp->sctp_flags & SCTP_PCB_FLAGS_UDPTYPE) {
		SCTP_INP_RUNLOCK(inp);
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EOPNOTSUPP);
		return (EOPNOTSUPP);
	}
	if (so->so_state & SS_ISDISCONNECTED) {
		SCTP_INP_RUNLOCK(inp);
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ECONNABORTED);
		return (ECONNABORTED);
	}
	stcb = LIST_FIRST(&inp->sctp_asoc_list);
	if (stcb == NULL) {
		SCTP_INP_RUNLOCK(inp);
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		return (ECONNRESET);
	}
	SCTP_TCB_LOCK(stcb);
	SCTP_INP_RUNLOCK(inp);
	store = stcb->asoc.primary_destination->ro._l_addr;
	SCTP_TCB_UNLOCK(stcb);
	switch (store.sa.sa_family) {
	case AF_INET:
		{
			struct sockaddr_in *sin;

			SCTP_MALLOC_SONAME(sin, struct sockaddr_in *, sizeof *sin);
			sin->sin_family = AF_INET;
			sin->sin_len = sizeof(*sin);
			sin->sin_port = ((struct sockaddr_in *)&store)->sin_port;
			sin->sin_addr = ((struct sockaddr_in *)&store)->sin_addr;
			*addr = (struct sockaddr *)sin;
			break;
		}
#ifdef INET6
	case AF_INET6:
		{
			struct sockaddr_in6 *sin6;

			SCTP_MALLOC_SONAME(sin6, struct sockaddr_in6 *, sizeof *sin6);
			sin6->sin6_family = AF_INET6;
			sin6->sin6_len = sizeof(*sin6);
			sin6->sin6_port = ((struct sockaddr_in6 *)&store)->sin6_port;

			sin6->sin6_addr = ((struct sockaddr_in6 *)&store)->sin6_addr;
			if ((error = sa6_recoverscope(sin6)) != 0) {
				SCTP_FREE_SONAME(sin6);
				return (error);
			}
			*addr = (struct sockaddr *)sin6;
			break;
		}
#endif
	default:
		/* TSNH */
		break;
	}
	/* Wake any delayed sleep action */
	if (inp->sctp_flags & SCTP_PCB_FLAGS_DONT_WAKE) {
		SCTP_INP_WLOCK(inp);
		inp->sctp_flags &= ~SCTP_PCB_FLAGS_DONT_WAKE;
		if (inp->sctp_flags & SCTP_PCB_FLAGS_WAKEOUTPUT) {
			inp->sctp_flags &= ~SCTP_PCB_FLAGS_WAKEOUTPUT;
			SCTP_INP_WUNLOCK(inp);
			SOCKBUF_LOCK(&inp->sctp_socket->so_snd);
			if (sowriteable(inp->sctp_socket)) {
				sowwakeup_locked(inp->sctp_socket);
			} else {
				SOCKBUF_UNLOCK(&inp->sctp_socket->so_snd);
			}
			SCTP_INP_WLOCK(inp);
		}
		if (inp->sctp_flags & SCTP_PCB_FLAGS_WAKEINPUT) {
			inp->sctp_flags &= ~SCTP_PCB_FLAGS_WAKEINPUT;
			SCTP_INP_WUNLOCK(inp);
			SOCKBUF_LOCK(&inp->sctp_socket->so_rcv);
			if (soreadable(inp->sctp_socket)) {
				sctp_defered_wakeup_cnt++;
				sorwakeup_locked(inp->sctp_socket);
			} else {
				SOCKBUF_UNLOCK(&inp->sctp_socket->so_rcv);
			}
			SCTP_INP_WLOCK(inp);
		}
		SCTP_INP_WUNLOCK(inp);
	}
	return (0);
}

int
sctp_ingetaddr(struct socket *so, struct sockaddr **addr)
{
	struct sockaddr_in *sin;
	uint32_t vrf_id;
	struct sctp_inpcb *inp;
	struct sctp_ifa *sctp_ifa;

	/*
	 * Do the malloc first in case it blocks.
	 */
	SCTP_MALLOC_SONAME(sin, struct sockaddr_in *, sizeof *sin);
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);
	inp = (struct sctp_inpcb *)so->so_pcb;
	if (!inp) {
		SCTP_FREE_SONAME(sin);
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		return ECONNRESET;
	}
	SCTP_INP_RLOCK(inp);
	sin->sin_port = inp->sctp_lport;
	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
		if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
			struct sctp_tcb *stcb;
			struct sockaddr_in *sin_a;
			struct sctp_nets *net;
			int fnd;

			stcb = LIST_FIRST(&inp->sctp_asoc_list);
			if (stcb == NULL) {
				goto notConn;
			}
			fnd = 0;
			sin_a = NULL;
			SCTP_TCB_LOCK(stcb);
			TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
				sin_a = (struct sockaddr_in *)&net->ro._l_addr;
				if (sin_a == NULL)
					/* this will make coverity happy */
					continue;

				if (sin_a->sin_family == AF_INET) {
					fnd = 1;
					break;
				}
			}
			if ((!fnd) || (sin_a == NULL)) {
				/* punt */
				SCTP_TCB_UNLOCK(stcb);
				goto notConn;
			}
			vrf_id = inp->def_vrf_id;
			sctp_ifa = sctp_source_address_selection(inp,
			    stcb,
			    (sctp_route_t *) & net->ro,
			    net, 0, vrf_id);
			if (sctp_ifa) {
				sin->sin_addr = sctp_ifa->address.sin.sin_addr;
				sctp_free_ifa(sctp_ifa);
			}
			SCTP_TCB_UNLOCK(stcb);
		} else {
			/* For the bound all case you get back 0 */
	notConn:
			sin->sin_addr.s_addr = 0;
		}

	} else {
		/* Take the first IPv4 address in the list */
		struct sctp_laddr *laddr;
		int fnd = 0;

		LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
			if (laddr->ifa->address.sa.sa_family == AF_INET) {
				struct sockaddr_in *sin_a;

				sin_a = (struct sockaddr_in *)&laddr->ifa->address.sa;
				sin->sin_addr = sin_a->sin_addr;
				fnd = 1;
				break;
			}
		}
		if (!fnd) {
			SCTP_FREE_SONAME(sin);
			SCTP_INP_RUNLOCK(inp);
			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOENT);
			return ENOENT;
		}
	}
	SCTP_INP_RUNLOCK(inp);
	(*addr) = (struct sockaddr *)sin;
	return (0);
}

int
sctp_peeraddr(struct socket *so, struct sockaddr **addr)
{
	struct sockaddr_in *sin = (struct sockaddr_in *)*addr;
	int fnd;
	struct sockaddr_in *sin_a;
	struct sctp_inpcb *inp;
	struct sctp_tcb *stcb;
	struct sctp_nets *net;

	/* Do the malloc first in case it blocks. */
	inp = (struct sctp_inpcb *)so->so_pcb;
	if ((inp == NULL) ||
	    ((inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) == 0)) {
		/* UDP type and listeners will drop out here */
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOTCONN);
		return (ENOTCONN);
	}
	SCTP_MALLOC_SONAME(sin, struct sockaddr_in *, sizeof *sin);
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);

	/* We must recapture incase we blocked */
	inp = (struct sctp_inpcb *)so->so_pcb;
	if (!inp) {
		SCTP_FREE_SONAME(sin);
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		return ECONNRESET;
	}
	SCTP_INP_RLOCK(inp);
	stcb = LIST_FIRST(&inp->sctp_asoc_list);
	if (stcb) {
		SCTP_TCB_LOCK(stcb);
	}
	SCTP_INP_RUNLOCK(inp);
	if (stcb == NULL) {
		SCTP_FREE_SONAME(sin);
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, EINVAL);
		return ECONNRESET;
	}
	fnd = 0;
	TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
		sin_a = (struct sockaddr_in *)&net->ro._l_addr;
		if (sin_a->sin_family == AF_INET) {
			fnd = 1;
			sin->sin_port = stcb->rport;
			sin->sin_addr = sin_a->sin_addr;
			break;
		}
	}
	SCTP_TCB_UNLOCK(stcb);
	if (!fnd) {
		/* No IPv4 address */
		SCTP_FREE_SONAME(sin);
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_USRREQ, ENOENT);
		return ENOENT;
	}
	(*addr) = (struct sockaddr *)sin;
	return (0);
}

struct pr_usrreqs sctp_usrreqs = {
	.pru_abort = sctp_abort,
	.pru_accept = sctp_accept,
	.pru_attach = sctp_attach,
	.pru_bind = sctp_bind,
	.pru_connect = sctp_connect,
	.pru_control = in_control,
	.pru_close = sctp_close,
	.pru_detach = sctp_close,
	.pru_sopoll = sopoll_generic,
	.pru_flush = sctp_flush,
	.pru_disconnect = sctp_disconnect,
	.pru_listen = sctp_listen,
	.pru_peeraddr = sctp_peeraddr,
	.pru_send = sctp_sendm,
	.pru_shutdown = sctp_shutdown,
	.pru_sockaddr = sctp_ingetaddr,
	.pru_sosend = sctp_sosend,
	.pru_soreceive = sctp_soreceive
};
