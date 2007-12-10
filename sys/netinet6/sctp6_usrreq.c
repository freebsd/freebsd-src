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
/*	$KAME: sctp6_usrreq.c,v 1.38 2005/08/24 08:08:56 suz Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <netinet/sctp_os.h>
#include <sys/proc.h>
#include <netinet/sctp_pcb.h>
#include <netinet/sctp_header.h>
#include <netinet/sctp_var.h>
#if defined(INET6)
#include <netinet6/sctp6_var.h>
#endif
#include <netinet/sctp_sysctl.h>
#include <netinet/sctp_output.h>
#include <netinet/sctp_uio.h>
#include <netinet/sctp_asconf.h>
#include <netinet/sctputil.h>
#include <netinet/sctp_indata.h>
#include <netinet/sctp_timer.h>
#include <netinet/sctp_auth.h>
#include <netinet/sctp_input.h>
#include <netinet/sctp_output.h>
#include <netinet/sctp_bsd_addr.h>

#ifdef IPSEC
#include <netipsec/ipsec.h>
#if defined(INET6)
#include <netipsec/ipsec6.h>
#endif				/* INET6 */
#endif				/* IPSEC */

extern struct protosw inetsw[];

int
sctp6_input(struct mbuf **i_pak, int *offp, int proto)
{
	struct mbuf *m;
	struct ip6_hdr *ip6;
	struct sctphdr *sh;
	struct sctp_inpcb *in6p = NULL;
	struct sctp_nets *net;
	int refcount_up = 0;
	uint32_t check, calc_check;
	uint32_t vrf_id = 0;
	struct inpcb *in6p_ip;
	struct sctp_chunkhdr *ch;
	int length, mlen, offset, iphlen;
	uint8_t ecn_bits;
	struct sctp_tcb *stcb = NULL;
	int pkt_len = 0;
	int off = *offp;

	/* get the VRF and table id's */
	if (SCTP_GET_PKT_VRFID(*i_pak, vrf_id)) {
		SCTP_RELEASE_PKT(*i_pak);
		return (-1);
	}
	m = SCTP_HEADER_TO_CHAIN(*i_pak);
	pkt_len = SCTP_HEADER_LEN((*i_pak));

#ifdef  SCTP_PACKET_LOGGING
	sctp_packet_log(m, pkt_len);
#endif
	ip6 = mtod(m, struct ip6_hdr *);
	/* Ensure that (sctphdr + sctp_chunkhdr) in a row. */
	IP6_EXTHDR_GET(sh, struct sctphdr *, m, off,
	    (int)(sizeof(*sh) + sizeof(*ch)));
	if (sh == NULL) {
		SCTP_STAT_INCR(sctps_hdrops);
		return IPPROTO_DONE;
	}
	ch = (struct sctp_chunkhdr *)((caddr_t)sh + sizeof(struct sctphdr));
	iphlen = off;
	offset = iphlen + sizeof(*sh) + sizeof(*ch);
	SCTPDBG(SCTP_DEBUG_INPUT1,
	    "sctp6_input() length:%d iphlen:%d\n", pkt_len, iphlen);


#if defined(NFAITH) && NFAITH > 0

	if (faithprefix_p != NULL && (*faithprefix_p) (&ip6->ip6_dst)) {
		/* XXX send icmp6 host/port unreach? */
		goto bad;
	}
#endif				/* NFAITH defined and > 0 */
	SCTP_STAT_INCR(sctps_recvpackets);
	SCTP_STAT_INCR_COUNTER64(sctps_inpackets);
	SCTPDBG(SCTP_DEBUG_INPUT1, "V6 input gets a packet iphlen:%d pktlen:%d\n",
	    iphlen, pkt_len);
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst)) {
		/* No multi-cast support in SCTP */
		goto bad;
	}
	/* destination port of 0 is illegal, based on RFC2960. */
	if (sh->dest_port == 0)
		goto bad;
	check = sh->checksum;	/* save incoming checksum */
	if ((check == 0) && (sctp_no_csum_on_loopback) &&
	    (IN6_ARE_ADDR_EQUAL(&ip6->ip6_src, &ip6->ip6_dst))) {
		goto sctp_skip_csum;
	}
	sh->checksum = 0;	/* prepare for calc */
	calc_check = sctp_calculate_sum(m, &mlen, iphlen);
	if (calc_check != check) {
		SCTPDBG(SCTP_DEBUG_INPUT1, "Bad CSUM on SCTP packet calc_check:%x check:%x  m:%p mlen:%d iphlen:%d\n",
		    calc_check, check, m, mlen, iphlen);
		stcb = sctp_findassociation_addr(m, iphlen, offset - sizeof(*ch),
		    sh, ch, &in6p, &net, vrf_id);
		/* in6p's ref-count increased && stcb locked */
		if ((in6p) && (stcb)) {
			sctp_send_packet_dropped(stcb, net, m, iphlen, 1);
			sctp_chunk_output((struct sctp_inpcb *)in6p, stcb, SCTP_OUTPUT_FROM_INPUT_ERROR, SCTP_SO_NOT_LOCKED);
		} else if ((in6p != NULL) && (stcb == NULL)) {
			refcount_up = 1;
		}
		SCTP_STAT_INCR(sctps_badsum);
		SCTP_STAT_INCR_COUNTER32(sctps_checksumerrors);
		goto bad;
	}
	sh->checksum = calc_check;

sctp_skip_csum:
	net = NULL;
	/*
	 * Locate pcb and tcb for datagram sctp_findassociation_addr() wants
	 * IP/SCTP/first chunk header...
	 */
	stcb = sctp_findassociation_addr(m, iphlen, offset - sizeof(*ch),
	    sh, ch, &in6p, &net, vrf_id);
	/* in6p's ref-count increased */
	if (in6p == NULL) {
		struct sctp_init_chunk *init_chk, chunk_buf;

		SCTP_STAT_INCR(sctps_noport);
		if (ch->chunk_type == SCTP_INITIATION) {
			/*
			 * we do a trick here to get the INIT tag, dig in
			 * and get the tag from the INIT and put it in the
			 * common header.
			 */
			init_chk = (struct sctp_init_chunk *)sctp_m_getptr(m,
			    iphlen + sizeof(*sh), sizeof(*init_chk),
			    (uint8_t *) & chunk_buf);
			if (init_chk)
				sh->v_tag = init_chk->init.initiate_tag;
			else
				sh->v_tag = 0;
		}
		if (ch->chunk_type == SCTP_SHUTDOWN_ACK) {
			sctp_send_shutdown_complete2(m, iphlen, sh, vrf_id);
			goto bad;
		}
		if (ch->chunk_type == SCTP_SHUTDOWN_COMPLETE) {
			goto bad;
		}
		if (ch->chunk_type != SCTP_ABORT_ASSOCIATION)
			sctp_send_abort(m, iphlen, sh, 0, NULL, vrf_id);
		goto bad;
	} else if (stcb == NULL) {
		refcount_up = 1;
	}
	in6p_ip = (struct inpcb *)in6p;
#ifdef IPSEC
	/*
	 * Check AH/ESP integrity.
	 */
	if (in6p_ip && (ipsec6_in_reject(m, in6p_ip))) {
/* XXX */
		ipsec6stat.in_polvio++;
		goto bad;
	}
#endif				/* IPSEC */

	/*
	 * CONTROL chunk processing
	 */
	offset -= sizeof(*ch);
	ecn_bits = ((ntohl(ip6->ip6_flow) >> 20) & 0x000000ff);

	/* Length now holds the total packet length payload + iphlen */
	length = ntohs(ip6->ip6_plen) + iphlen;

	/* sa_ignore NO_NULL_CHK */
	sctp_common_input_processing(&m, iphlen, offset, length, sh, ch,
	    in6p, stcb, net, ecn_bits, vrf_id);
	/* inp's ref-count reduced && stcb unlocked */
	/* XXX this stuff below gets moved to appropriate parts later... */
	if (m)
		sctp_m_freem(m);
	if ((in6p) && refcount_up) {
		/* reduce ref-count */
		SCTP_INP_WLOCK(in6p);
		SCTP_INP_DECR_REF(in6p);
		SCTP_INP_WUNLOCK(in6p);
	}
	return IPPROTO_DONE;

bad:
	if (stcb) {
		SCTP_TCB_UNLOCK(stcb);
	}
	if ((in6p) && refcount_up) {
		/* reduce ref-count */
		SCTP_INP_WLOCK(in6p);
		SCTP_INP_DECR_REF(in6p);
		SCTP_INP_WUNLOCK(in6p);
	}
	if (m)
		sctp_m_freem(m);
	return IPPROTO_DONE;
}


static void
sctp6_notify_mbuf(struct sctp_inpcb *inp, struct icmp6_hdr *icmp6,
    struct sctphdr *sh, struct sctp_tcb *stcb, struct sctp_nets *net)
{
	uint32_t nxtsz;

	if ((inp == NULL) || (stcb == NULL) || (net == NULL) ||
	    (icmp6 == NULL) || (sh == NULL)) {
		goto out;
	}
	/* First do we even look at it? */
	if (ntohl(sh->v_tag) != (stcb->asoc.peer_vtag))
		goto out;

	if (icmp6->icmp6_type != ICMP6_PACKET_TOO_BIG) {
		/* not PACKET TO BIG */
		goto out;
	}
	/*
	 * ok we need to look closely. We could even get smarter and look at
	 * anyone that we sent to in case we get a different ICMP that tells
	 * us there is no way to reach a host, but for this impl, all we
	 * care about is MTU discovery.
	 */
	nxtsz = ntohl(icmp6->icmp6_mtu);
	/* Stop any PMTU timer */
	sctp_timer_stop(SCTP_TIMER_TYPE_PATHMTURAISE, inp, stcb, NULL, SCTP_FROM_SCTP6_USRREQ + SCTP_LOC_1);

	/* Adjust destination size limit */
	if (net->mtu > nxtsz) {
		net->mtu = nxtsz;
	}
	/* now what about the ep? */
	if (stcb->asoc.smallest_mtu > nxtsz) {
		struct sctp_tmit_chunk *chk;

		/* Adjust that too */
		stcb->asoc.smallest_mtu = nxtsz;
		/* now off to subtract IP_DF flag if needed */

		TAILQ_FOREACH(chk, &stcb->asoc.send_queue, sctp_next) {
			if ((uint32_t) (chk->send_size + IP_HDR_SIZE) > nxtsz) {
				chk->flags |= CHUNK_FLAGS_FRAGMENT_OK;
			}
		}
		TAILQ_FOREACH(chk, &stcb->asoc.sent_queue, sctp_next) {
			if ((uint32_t) (chk->send_size + IP_HDR_SIZE) > nxtsz) {
				/*
				 * For this guy we also mark for immediate
				 * resend since we sent to big of chunk
				 */
				chk->flags |= CHUNK_FLAGS_FRAGMENT_OK;
				if (chk->sent != SCTP_DATAGRAM_RESEND)
					stcb->asoc.sent_queue_retran_cnt++;
				chk->sent = SCTP_DATAGRAM_RESEND;
				chk->rec.data.doing_fast_retransmit = 0;

				chk->sent = SCTP_DATAGRAM_RESEND;
				/* Clear any time so NO RTT is being done */
				chk->sent_rcv_time.tv_sec = 0;
				chk->sent_rcv_time.tv_usec = 0;
				stcb->asoc.total_flight -= chk->send_size;
				net->flight_size -= chk->send_size;
			}
		}
	}
	sctp_timer_start(SCTP_TIMER_TYPE_PATHMTURAISE, inp, stcb, NULL);
out:
	if (stcb) {
		SCTP_TCB_UNLOCK(stcb);
	}
}


void
sctp6_notify(struct sctp_inpcb *inp,
    struct icmp6_hdr *icmph,
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
	if (icmph->icmp6_type != ICMP_UNREACH) {
		/* We only care about unreachable */
		SCTP_TCB_UNLOCK(stcb);
		return;
	}
	if ((icmph->icmp6_code == ICMP_UNREACH_NET) ||
	    (icmph->icmp6_code == ICMP_UNREACH_HOST) ||
	    (icmph->icmp6_code == ICMP_UNREACH_NET_UNKNOWN) ||
	    (icmph->icmp6_code == ICMP_UNREACH_HOST_UNKNOWN) ||
	    (icmph->icmp6_code == ICMP_UNREACH_ISOLATED) ||
	    (icmph->icmp6_code == ICMP_UNREACH_NET_PROHIB) ||
	    (icmph->icmp6_code == ICMP_UNREACH_HOST_PROHIB) ||
	    (icmph->icmp6_code == ICMP_UNREACH_FILTER_PROHIB)) {

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
			if (sctp_cmt_on_off && sctp_cmt_pf) {
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
	} else if ((icmph->icmp6_code == ICMP_UNREACH_PROTOCOL) ||
	    (icmph->icmp6_code == ICMP_UNREACH_PORT)) {
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
sctp6_ctlinput(int cmd, struct sockaddr *pktdst, void *d)
{
	struct sctphdr sh;
	struct ip6ctlparam *ip6cp = NULL;
	uint32_t vrf_id;

	vrf_id = SCTP_DEFAULT_VRFID;

	if (pktdst->sa_family != AF_INET6 ||
	    pktdst->sa_len != sizeof(struct sockaddr_in6))
		return;

	if ((unsigned)cmd >= PRC_NCMDS)
		return;
	if (PRC_IS_REDIRECT(cmd)) {
		d = NULL;
	} else if (inet6ctlerrmap[cmd] == 0) {
		return;
	}
	/* if the parameter is from icmp6, decode it. */
	if (d != NULL) {
		ip6cp = (struct ip6ctlparam *)d;
	} else {
		ip6cp = (struct ip6ctlparam *)NULL;
	}

	if (ip6cp) {
		/*
		 * XXX: We assume that when IPV6 is non NULL, M and OFF are
		 * valid.
		 */
		/* check if we can safely examine src and dst ports */
		struct sctp_inpcb *inp = NULL;
		struct sctp_tcb *stcb = NULL;
		struct sctp_nets *net = NULL;
		struct sockaddr_in6 final;

		if (ip6cp->ip6c_m == NULL)
			return;

		bzero(&sh, sizeof(sh));
		bzero(&final, sizeof(final));
		inp = NULL;
		net = NULL;
		m_copydata(ip6cp->ip6c_m, ip6cp->ip6c_off, sizeof(sh),
		    (caddr_t)&sh);
		ip6cp->ip6c_src->sin6_port = sh.src_port;
		final.sin6_len = sizeof(final);
		final.sin6_family = AF_INET6;
		final.sin6_addr = ((struct sockaddr_in6 *)pktdst)->sin6_addr;
		final.sin6_port = sh.dest_port;
		stcb = sctp_findassociation_addr_sa((struct sockaddr *)ip6cp->ip6c_src,
		    (struct sockaddr *)&final,
		    &inp, &net, 1, vrf_id);
		/* inp's ref-count increased && stcb locked */
		if (stcb != NULL && inp && (inp->sctp_socket != NULL)) {
			if (cmd == PRC_MSGSIZE) {
				sctp6_notify_mbuf(inp,
				    ip6cp->ip6c_icmp6,
				    &sh,
				    stcb,
				    net);
				/* inp's ref-count reduced && stcb unlocked */
			} else {
				sctp6_notify(inp, ip6cp->ip6c_icmp6, &sh,
				    (struct sockaddr *)&final,
				    stcb, net);
				/* inp's ref-count reduced && stcb unlocked */
			}
		} else {
			if (PRC_IS_REDIRECT(cmd) && inp) {
				in6_rtchange((struct in6pcb *)inp,
				    inet6ctlerrmap[cmd]);
			}
			if (inp) {
				/* reduce inp's ref-count */
				SCTP_INP_WLOCK(inp);
				SCTP_INP_DECR_REF(inp);
				SCTP_INP_WUNLOCK(inp);
			}
			if (stcb)
				SCTP_TCB_UNLOCK(stcb);
		}
	}
}

/*
 * this routine can probably be collasped into the one in sctp_userreq.c
 * since they do the same thing and now we lookup with a sockaddr
 */
static int
sctp6_getcred(SYSCTL_HANDLER_ARGS)
{
	struct xucred xuc;
	struct sockaddr_in6 addrs[2];
	struct sctp_inpcb *inp;
	struct sctp_nets *net;
	struct sctp_tcb *stcb;
	int error;
	uint32_t vrf_id;

	vrf_id = SCTP_DEFAULT_VRFID;

	error = priv_check(req->td, PRIV_NETINET_GETCRED);
	if (error)
		return (error);

	if (req->newlen != sizeof(addrs)) {
		SCTP_LTRACE_ERR_RET(NULL, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, EINVAL);
		return (EINVAL);
	}
	if (req->oldlen != sizeof(struct ucred)) {
		SCTP_LTRACE_ERR_RET(NULL, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, EINVAL);
		return (EINVAL);
	}
	error = SYSCTL_IN(req, addrs, sizeof(addrs));
	if (error)
		return (error);

	stcb = sctp_findassociation_addr_sa(sin6tosa(&addrs[0]),
	    sin6tosa(&addrs[1]),
	    &inp, &net, 1, vrf_id);
	if (stcb == NULL || inp == NULL || inp->sctp_socket == NULL) {
		if ((inp != NULL) && (stcb == NULL)) {
			/* reduce ref-count */
			SCTP_INP_WLOCK(inp);
			SCTP_INP_DECR_REF(inp);
			goto cred_can_cont;
		}
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, ENOENT);
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

SYSCTL_PROC(_net_inet6_sctp6, OID_AUTO, getcred, CTLTYPE_OPAQUE | CTLFLAG_RW,
    0, 0,
    sctp6_getcred, "S,ucred", "Get the ucred of a SCTP6 connection");


/* This is the same as the sctp_abort() could be made common */
static void
sctp6_abort(struct socket *so)
{
	struct sctp_inpcb *inp;
	uint32_t flags;

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0) {
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, EINVAL);
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
sctp6_attach(struct socket *so, int proto, struct thread *p)
{
	struct in6pcb *inp6;
	int error;
	struct sctp_inpcb *inp;
	uint32_t vrf_id = SCTP_DEFAULT_VRFID;

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp != NULL) {
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, EINVAL);
		return EINVAL;
	}
	if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
		error = SCTP_SORESERVE(so, sctp_sendspace, sctp_recvspace);
		if (error)
			return error;
	}
	error = sctp_inpcb_alloc(so, vrf_id);
	if (error)
		return error;
	inp = (struct sctp_inpcb *)so->so_pcb;
	SCTP_INP_WLOCK(inp);
	inp->sctp_flags |= SCTP_PCB_FLAGS_BOUND_V6;	/* I'm v6! */
	inp6 = (struct in6pcb *)inp;

	inp6->inp_vflag |= INP_IPV6;
	inp6->in6p_hops = -1;	/* use kernel default */
	inp6->in6p_cksum = -1;	/* just to be sure */
#ifdef INET
	/*
	 * XXX: ugly!! IPv4 TTL initialization is necessary for an IPv6
	 * socket as well, because the socket may be bound to an IPv6
	 * wildcard address, which may match an IPv4-mapped IPv6 address.
	 */
	inp6->inp_ip_ttl = ip_defttl;
#endif
	/*
	 * Hmm what about the IPSEC stuff that is missing here but in
	 * sctp_attach()?
	 */
	SCTP_INP_WUNLOCK(inp);
	return 0;
}

static int
sctp6_bind(struct socket *so, struct sockaddr *addr, struct thread *p)
{
	struct sctp_inpcb *inp;
	struct in6pcb *inp6;
	int error;

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0) {
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, EINVAL);
		return EINVAL;
	}
	if (addr) {
		if ((addr->sa_family == AF_INET6) &&
		    (addr->sa_len != sizeof(struct sockaddr_in6))) {
			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, EINVAL);
			return EINVAL;
		}
		if ((addr->sa_family == AF_INET) &&
		    (addr->sa_len != sizeof(struct sockaddr_in))) {
			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, EINVAL);
			return EINVAL;
		}
	}
	inp6 = (struct in6pcb *)inp;
	inp6->inp_vflag &= ~INP_IPV4;
	inp6->inp_vflag |= INP_IPV6;
	if ((addr != NULL) && (SCTP_IPV6_V6ONLY(inp6) == 0)) {
		if (addr->sa_family == AF_INET) {
			/* binding v4 addr to v6 socket, so reset flags */
			inp6->inp_vflag |= INP_IPV4;
			inp6->inp_vflag &= ~INP_IPV6;
		} else {
			struct sockaddr_in6 *sin6_p;

			sin6_p = (struct sockaddr_in6 *)addr;

			if (IN6_IS_ADDR_UNSPECIFIED(&sin6_p->sin6_addr)) {
				inp6->inp_vflag |= INP_IPV4;
			} else if (IN6_IS_ADDR_V4MAPPED(&sin6_p->sin6_addr)) {
				struct sockaddr_in sin;

				in6_sin6_2_sin(&sin, sin6_p);
				inp6->inp_vflag |= INP_IPV4;
				inp6->inp_vflag &= ~INP_IPV6;
				error = sctp_inpcb_bind(so, (struct sockaddr *)&sin, NULL, p);
				return error;
			}
		}
	} else if (addr != NULL) {
		/* IPV6_V6ONLY socket */
		if (addr->sa_family == AF_INET) {
			/* can't bind v4 addr to v6 only socket! */
			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, EINVAL);
			return EINVAL;
		} else {
			struct sockaddr_in6 *sin6_p;

			sin6_p = (struct sockaddr_in6 *)addr;

			if (IN6_IS_ADDR_V4MAPPED(&sin6_p->sin6_addr)) {
				/* can't bind v4-mapped addrs either! */
				/* NOTE: we don't support SIIT */
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, EINVAL);
				return EINVAL;
			}
		}
	}
	error = sctp_inpcb_bind(so, addr, NULL, p);
	return error;
}


static void
sctp6_close(struct socket *so)
{
	sctp_close(so);
}

/* This could be made common with sctp_detach() since they are identical */

static
int
sctp6_disconnect(struct socket *so)
{
	return (sctp_disconnect(so));
}


int
sctp_sendm(struct socket *so, int flags, struct mbuf *m, struct sockaddr *addr,
    struct mbuf *control, struct thread *p);


static int
sctp6_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *addr,
    struct mbuf *control, struct thread *p)
{
	struct sctp_inpcb *inp;
	struct inpcb *in_inp;
	struct in6pcb *inp6;

#ifdef INET
	struct sockaddr_in6 *sin6;

#endif				/* INET */
	/* No SPL needed since sctp_output does this */

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == NULL) {
		if (control) {
			SCTP_RELEASE_PKT(control);
			control = NULL;
		}
		SCTP_RELEASE_PKT(m);
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, EINVAL);
		return EINVAL;
	}
	in_inp = (struct inpcb *)inp;
	inp6 = (struct in6pcb *)inp;
	/*
	 * For the TCP model we may get a NULL addr, if we are a connected
	 * socket thats ok.
	 */
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) &&
	    (addr == NULL)) {
		goto connected_type;
	}
	if (addr == NULL) {
		SCTP_RELEASE_PKT(m);
		if (control) {
			SCTP_RELEASE_PKT(control);
			control = NULL;
		}
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, EDESTADDRREQ);
		return (EDESTADDRREQ);
	}
#ifdef INET
	sin6 = (struct sockaddr_in6 *)addr;
	if (SCTP_IPV6_V6ONLY(inp6)) {
		/*
		 * if IPV6_V6ONLY flag, we discard datagrams destined to a
		 * v4 addr or v4-mapped addr
		 */
		if (addr->sa_family == AF_INET) {
			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, EINVAL);
			return EINVAL;
		}
		if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, EINVAL);
			return EINVAL;
		}
	}
	if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
		if (!ip6_v6only) {
			struct sockaddr_in sin;

			/* convert v4-mapped into v4 addr and send */
			in6_sin6_2_sin(&sin, sin6);
			return sctp_sendm(so, flags, m, (struct sockaddr *)&sin,
			    control, p);
		} else {
			/* mapped addresses aren't enabled */
			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, EINVAL);
			return EINVAL;
		}
	}
#endif				/* INET */
connected_type:
	/* now what about control */
	if (control) {
		if (inp->control) {
			SCTP_PRINTF("huh? control set?\n");
			SCTP_RELEASE_PKT(inp->control);
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
	/* FreeBSD and MacOSX uses a flag passed */
	    ((flags & PRUS_MORETOCOME) == 0)
	    ) {
		/*
		 * note with the current version this code will only be used
		 * by OpenBSD, NetBSD and FreeBSD have methods for
		 * re-defining sosend() to use sctp_sosend().  One can
		 * optionaly switch back to this code (by changing back the
		 * defininitions but this is not advisable.
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

static int
sctp6_connect(struct socket *so, struct sockaddr *addr, struct thread *p)
{
	uint32_t vrf_id;
	int error = 0;
	struct sctp_inpcb *inp;
	struct in6pcb *inp6;
	struct sctp_tcb *stcb;

#ifdef INET
	struct sockaddr_in6 *sin6;
	struct sockaddr_storage ss;

#endif				/* INET */

	inp6 = (struct in6pcb *)so->so_pcb;
	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0) {
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, ECONNRESET);
		return (ECONNRESET);	/* I made the same as TCP since we are
					 * not setup? */
	}
	if (addr == NULL) {
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, EINVAL);
		return (EINVAL);
	}
	if ((addr->sa_family == AF_INET6) && (addr->sa_len != sizeof(struct sockaddr_in6))) {
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, EINVAL);
		return (EINVAL);
	}
	if ((addr->sa_family == AF_INET) && (addr->sa_len != sizeof(struct sockaddr_in))) {
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, EINVAL);
		return (EINVAL);
	}
	vrf_id = inp->def_vrf_id;
	SCTP_ASOC_CREATE_LOCK(inp);
	SCTP_INP_RLOCK(inp);
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_UNBOUND) ==
	    SCTP_PCB_FLAGS_UNBOUND) {
		/* Bind a ephemeral port */
		SCTP_INP_RUNLOCK(inp);
		error = sctp6_bind(so, NULL, p);
		if (error) {
			SCTP_ASOC_CREATE_UNLOCK(inp);

			return (error);
		}
		SCTP_INP_RLOCK(inp);
	}
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) &&
	    (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)) {
		/* We are already connected AND the TCP model */
		SCTP_INP_RUNLOCK(inp);
		SCTP_ASOC_CREATE_UNLOCK(inp);
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, EADDRINUSE);
		return (EADDRINUSE);
	}
#ifdef INET
	sin6 = (struct sockaddr_in6 *)addr;
	if (SCTP_IPV6_V6ONLY(inp6)) {
		/*
		 * if IPV6_V6ONLY flag, ignore connections destined to a v4
		 * addr or v4-mapped addr
		 */
		if (addr->sa_family == AF_INET) {
			SCTP_INP_RUNLOCK(inp);
			SCTP_ASOC_CREATE_UNLOCK(inp);
			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, EINVAL);
			return EINVAL;
		}
		if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
			SCTP_INP_RUNLOCK(inp);
			SCTP_ASOC_CREATE_UNLOCK(inp);
			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, EINVAL);
			return EINVAL;
		}
	}
	if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
		if (!ip6_v6only) {
			/* convert v4-mapped into v4 addr */
			in6_sin6_2_sin((struct sockaddr_in *)&ss, sin6);
			addr = (struct sockaddr *)&ss;
		} else {
			/* mapped addresses aren't enabled */
			SCTP_INP_RUNLOCK(inp);
			SCTP_ASOC_CREATE_UNLOCK(inp);
			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, EINVAL);
			return EINVAL;
		}
	} else
#endif				/* INET */
		addr = addr;	/* for true v6 address case */

	/* Now do we connect? */
	if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
		stcb = LIST_FIRST(&inp->sctp_asoc_list);
		if (stcb) {
			SCTP_TCB_UNLOCK(stcb);
		}
		SCTP_INP_RUNLOCK(inp);
	} else {
		SCTP_INP_RUNLOCK(inp);
		SCTP_INP_WLOCK(inp);
		SCTP_INP_INCR_REF(inp);
		SCTP_INP_WUNLOCK(inp);
		stcb = sctp_findassociation_ep_addr(&inp, addr, NULL, NULL, NULL);
		if (stcb == NULL) {
			SCTP_INP_WLOCK(inp);
			SCTP_INP_DECR_REF(inp);
			SCTP_INP_WUNLOCK(inp);
		}
	}

	if (stcb != NULL) {
		/* Already have or am bring up an association */
		SCTP_ASOC_CREATE_UNLOCK(inp);
		SCTP_TCB_UNLOCK(stcb);
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, EALREADY);
		return (EALREADY);
	}
	/* We are GOOD to go */
	stcb = sctp_aloc_assoc(inp, addr, 1, &error, 0, vrf_id, p);
	SCTP_ASOC_CREATE_UNLOCK(inp);
	if (stcb == NULL) {
		/* Gak! no memory */
		return (error);
	}
	if (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) {
		stcb->sctp_ep->sctp_flags |= SCTP_PCB_FLAGS_CONNECTED;
		/* Set the connected flag so we can queue data */
		soisconnecting(so);
	}
	stcb->asoc.state = SCTP_STATE_COOKIE_WAIT;
	(void)SCTP_GETTIME_TIMEVAL(&stcb->asoc.time_entered);

	/* initialize authentication parameters for the assoc */
	sctp_initialize_auth_params(inp, stcb);

	sctp_send_initiate(inp, stcb, SCTP_SO_LOCKED);
	SCTP_TCB_UNLOCK(stcb);
	return error;
}

static int
sctp6_getaddr(struct socket *so, struct sockaddr **addr)
{
	struct sockaddr_in6 *sin6;
	struct sctp_inpcb *inp;
	uint32_t vrf_id;
	struct sctp_ifa *sctp_ifa;

	int error;

	/*
	 * Do the malloc first in case it blocks.
	 */
	SCTP_MALLOC_SONAME(sin6, struct sockaddr_in6 *, sizeof *sin6);
	sin6->sin6_family = AF_INET6;
	sin6->sin6_len = sizeof(*sin6);

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == NULL) {
		SCTP_FREE_SONAME(sin6);
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, ECONNRESET);
		return ECONNRESET;
	}
	SCTP_INP_RLOCK(inp);
	sin6->sin6_port = inp->sctp_lport;
	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
		/* For the bound all case you get back 0 */
		if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
			struct sctp_tcb *stcb;
			struct sockaddr_in6 *sin_a6;
			struct sctp_nets *net;
			int fnd;

			stcb = LIST_FIRST(&inp->sctp_asoc_list);
			if (stcb == NULL) {
				goto notConn6;
			}
			fnd = 0;
			sin_a6 = NULL;
			TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
				sin_a6 = (struct sockaddr_in6 *)&net->ro._l_addr;
				if (sin_a6 == NULL)
					/* this will make coverity happy */
					continue;

				if (sin_a6->sin6_family == AF_INET6) {
					fnd = 1;
					break;
				}
			}
			if ((!fnd) || (sin_a6 == NULL)) {
				/* punt */
				goto notConn6;
			}
			vrf_id = inp->def_vrf_id;
			sctp_ifa = sctp_source_address_selection(inp, stcb, (sctp_route_t *) & net->ro, net, 0, vrf_id);
			if (sctp_ifa) {
				sin6->sin6_addr = sctp_ifa->address.sin6.sin6_addr;
			}
		} else {
			/* For the bound all case you get back 0 */
	notConn6:
			memset(&sin6->sin6_addr, 0, sizeof(sin6->sin6_addr));
		}
	} else {
		/* Take the first IPv6 address in the list */
		struct sctp_laddr *laddr;
		int fnd = 0;

		LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
			if (laddr->ifa->address.sa.sa_family == AF_INET6) {
				struct sockaddr_in6 *sin_a;

				sin_a = (struct sockaddr_in6 *)&laddr->ifa->address.sin6;
				sin6->sin6_addr = sin_a->sin6_addr;
				fnd = 1;
				break;
			}
		}
		if (!fnd) {
			SCTP_FREE_SONAME(sin6);
			SCTP_INP_RUNLOCK(inp);
			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, ENOENT);
			return ENOENT;
		}
	}
	SCTP_INP_RUNLOCK(inp);
	/* Scoping things for v6 */
	if ((error = sa6_recoverscope(sin6)) != 0) {
		SCTP_FREE_SONAME(sin6);
		return (error);
	}
	(*addr) = (struct sockaddr *)sin6;
	return (0);
}

static int
sctp6_peeraddr(struct socket *so, struct sockaddr **addr)
{
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)*addr;
	int fnd;
	struct sockaddr_in6 *sin_a6;
	struct sctp_inpcb *inp;
	struct sctp_tcb *stcb;
	struct sctp_nets *net;

	int error;

	/*
	 * Do the malloc first in case it blocks.
	 */
	inp = (struct sctp_inpcb *)so->so_pcb;
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) == 0) {
		/* UDP type and listeners will drop out here */
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, ENOTCONN);
		return (ENOTCONN);
	}
	SCTP_MALLOC_SONAME(sin6, struct sockaddr_in6 *, sizeof *sin6);
	sin6->sin6_family = AF_INET6;
	sin6->sin6_len = sizeof(*sin6);

	/* We must recapture incase we blocked */
	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == NULL) {
		SCTP_FREE_SONAME(sin6);
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, ECONNRESET);
		return ECONNRESET;
	}
	SCTP_INP_RLOCK(inp);
	stcb = LIST_FIRST(&inp->sctp_asoc_list);
	if (stcb) {
		SCTP_TCB_LOCK(stcb);
	}
	SCTP_INP_RUNLOCK(inp);
	if (stcb == NULL) {
		SCTP_FREE_SONAME(sin6);
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, ECONNRESET);
		return ECONNRESET;
	}
	fnd = 0;
	TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
		sin_a6 = (struct sockaddr_in6 *)&net->ro._l_addr;
		if (sin_a6->sin6_family == AF_INET6) {
			fnd = 1;
			sin6->sin6_port = stcb->rport;
			sin6->sin6_addr = sin_a6->sin6_addr;
			break;
		}
	}
	SCTP_TCB_UNLOCK(stcb);
	if (!fnd) {
		/* No IPv4 address */
		SCTP_FREE_SONAME(sin6);
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, ENOENT);
		return ENOENT;
	}
	if ((error = sa6_recoverscope(sin6)) != 0)
		return (error);
	*addr = (struct sockaddr *)sin6;
	return (0);
}

static int
sctp6_in6getaddr(struct socket *so, struct sockaddr **nam)
{
	struct sockaddr *addr;
	struct in6pcb *inp6 = sotoin6pcb(so);
	int error;

	if (inp6 == NULL) {
		SCTP_LTRACE_ERR_RET(NULL, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, EINVAL);
		return EINVAL;
	}
	/* allow v6 addresses precedence */
	error = sctp6_getaddr(so, nam);
	if (error) {
		/* try v4 next if v6 failed */
		error = sctp_ingetaddr(so, nam);
		if (error) {
			return (error);
		}
		addr = *nam;
		/* if I'm V6ONLY, convert it to v4-mapped */
		if (SCTP_IPV6_V6ONLY(inp6)) {
			struct sockaddr_in6 sin6;

			in6_sin_2_v4mapsin6((struct sockaddr_in *)addr, &sin6);
			memcpy(addr, &sin6, sizeof(struct sockaddr_in6));

		}
	}
	return (error);
}


static int
sctp6_getpeeraddr(struct socket *so, struct sockaddr **nam)
{
	struct sockaddr *addr = *nam;
	struct in6pcb *inp6 = sotoin6pcb(so);
	int error;

	if (inp6 == NULL) {
		SCTP_LTRACE_ERR_RET(NULL, NULL, NULL, SCTP_FROM_SCTP6_USRREQ, EINVAL);
		return EINVAL;
	}
	/* allow v6 addresses precedence */
	error = sctp6_peeraddr(so, nam);
	if (error) {
		/* try v4 next if v6 failed */
		error = sctp_peeraddr(so, nam);
		if (error) {
			return (error);
		}
		/* if I'm V6ONLY, convert it to v4-mapped */
		if (SCTP_IPV6_V6ONLY(inp6)) {
			struct sockaddr_in6 sin6;

			in6_sin_2_v4mapsin6((struct sockaddr_in *)addr, &sin6);
			memcpy(addr, &sin6, sizeof(struct sockaddr_in6));
		}
	}
	return error;
}

struct pr_usrreqs sctp6_usrreqs = {
	.pru_abort = sctp6_abort,
	.pru_accept = sctp_accept,
	.pru_attach = sctp6_attach,
	.pru_bind = sctp6_bind,
	.pru_connect = sctp6_connect,
	.pru_control = in6_control,
	.pru_close = sctp6_close,
	.pru_detach = sctp6_close,
	.pru_sopoll = sopoll_generic,
	.pru_disconnect = sctp6_disconnect,
	.pru_listen = sctp_listen,
	.pru_peeraddr = sctp6_getpeeraddr,
	.pru_send = sctp6_send,
	.pru_shutdown = sctp_shutdown,
	.pru_sockaddr = sctp6_in6getaddr,
	.pru_sosend = sctp_sosend,
	.pru_soreceive = sctp_soreceive
};
