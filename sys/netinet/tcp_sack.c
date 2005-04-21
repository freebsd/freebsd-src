/*-
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1994, 1995
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
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)tcp_sack.c	8.12 (Berkeley) 5/24/95
 * $FreeBSD$
 */

/*-
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1994
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
 *
 *	@@(#)COPYRIGHT	1.1 (NRL) 17 January 1995
 *
 * NRL grants permission for redistribution and use in source and binary
 * forms, with or without modification, of the software and documentation
 * created at NRL provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgements:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 *	This product includes software developed at the Information
 *	Technology Division, US Naval Research Laboratory.
 * 4. Neither the name of the NRL nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THE SOFTWARE PROVIDED BY NRL IS PROVIDED BY NRL AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL NRL OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the US Naval
 * Research Laboratory (NRL).
 */
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_tcpdebug.h"
#include "opt_tcp_input.h"
#include "opt_tcp_sack.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>		/* for proc0 declaration */
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#include <machine/cpu.h>	/* before tcp_seq.h, for tcp_random18() */

#include <vm/uma.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>	/* for ICMP_BANDLIM		*/
#include <netinet/in_var.h>
#include <netinet/icmp_var.h>	/* for ICMP_BANDLIM		*/
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_pcb.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet6/tcp6_var.h>
#include <netinet/tcpip.h>
#ifdef TCPDEBUG
#include <netinet/tcp_debug.h>
#endif /* TCPDEBUG */

#ifdef FAST_IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/ipsec6.h>
#endif

#ifdef IPSEC
#include <netinet6/ipsec.h>
#include <netinet6/ipsec6.h>
#include <netkey/key.h>
#endif /*IPSEC*/
#include <machine/in_cksum.h>

extern struct uma_zone *sack_hole_zone;

SYSCTL_NODE(_net_inet_tcp, OID_AUTO, sack, CTLFLAG_RW, 0, "TCP SACK");
int tcp_do_sack = 1;
SYSCTL_INT(_net_inet_tcp_sack, OID_AUTO, enable, CTLFLAG_RW,
	&tcp_do_sack, 0, "Enable/Disable TCP SACK support");
TUNABLE_INT("net.inet.tcp.sack.enable", &tcp_do_sack);

static int tcp_sack_maxholes = 128;
SYSCTL_INT(_net_inet_tcp_sack, OID_AUTO, maxholes, CTLFLAG_RW,
	&tcp_sack_maxholes, 0, 
    "Maximum number of TCP SACK holes allowed per connection");

static int tcp_sack_globalmaxholes = 65536;
SYSCTL_INT(_net_inet_tcp_sack, OID_AUTO, globalmaxholes, CTLFLAG_RW,
	&tcp_sack_globalmaxholes, 0, 
    "Global maximum number of TCP SACK holes");

static int tcp_sack_globalholes = 0;
SYSCTL_INT(_net_inet_tcp_sack, OID_AUTO, globalholes, CTLFLAG_RD,
    &tcp_sack_globalholes, 0,
    "Global number of TCP SACK holes currently allocated");

/*
 * This function is called upon receipt of new valid data (while not in header
 * prediction mode), and it updates the ordered list of sacks.
 */
void
tcp_update_sack_list(struct tcpcb *tp, tcp_seq rcv_start, tcp_seq rcv_end)
{
	/*
	 * First reported block MUST be the most recent one.  Subsequent
	 * blocks SHOULD be in the order in which they arrived at the
	 * receiver.  These two conditions make the implementation fully
	 * compliant with RFC 2018.
	 */
	struct sackblk head_blk, saved_blks[MAX_SACK_BLKS];
	int num_head, num_saved, i;

	INP_LOCK_ASSERT(tp->t_inpcb);

	/* Check arguments */
	KASSERT(SEQ_LT(rcv_start, rcv_end), ("rcv_start < rcv_end"));

	/* SACK block for the received segment. */
	head_blk.start = rcv_start;
	head_blk.end = rcv_end;

	/*
	 * Merge updated SACK blocks into head_blk, and
	 * save unchanged SACK blocks into saved_blks[].
	 * num_saved will have the number of the saved SACK blocks.
	 */
	num_saved = 0;
	for (i = 0; i < tp->rcv_numsacks; i++) {
		tcp_seq start = tp->sackblks[i].start;
		tcp_seq end = tp->sackblks[i].end;
		if (SEQ_GEQ(start, end) || SEQ_LEQ(start, tp->rcv_nxt)) {
			/*
			 * Discard this SACK block.
			 */
		} else if (SEQ_LEQ(head_blk.start, end) &&
			   SEQ_GEQ(head_blk.end, start)) {
			/*
			 * Merge this SACK block into head_blk.
			 * This SACK block itself will be discarded.
			 */
			if (SEQ_GT(head_blk.start, start))
				head_blk.start = start;
			if (SEQ_LT(head_blk.end, end))
				head_blk.end = end;
		} else {
			/*
			 * Save this SACK block.
			 */
			saved_blks[num_saved].start = start;
			saved_blks[num_saved].end = end;
			num_saved++;
		}
	}

	/*
	 * Update SACK list in tp->sackblks[].
	 */
	num_head = 0;
	if (SEQ_GT(head_blk.start, tp->rcv_nxt)) {
		/*
		 * The received data segment is an out-of-order segment.
		 * Put head_blk at the top of SACK list.
		 */
		tp->sackblks[0] = head_blk;
		num_head = 1;
		/*
		 * If the number of saved SACK blocks exceeds its limit,
		 * discard the last SACK block.
		 */
		if (num_saved >= MAX_SACK_BLKS)
			num_saved--;
	}
	if (num_saved > 0) {
		/*
		 * Copy the saved SACK blocks back.
		 */
		bcopy(saved_blks, &tp->sackblks[num_head],
		      sizeof(struct sackblk) * num_saved);
	}

	/* Save the number of SACK blocks. */
	tp->rcv_numsacks = num_head + num_saved;
}

/*
 * Delete all receiver-side SACK information.
 */
void
tcp_clean_sackreport(tp)
	struct tcpcb *tp;
{
	int i;

	INP_LOCK_ASSERT(tp->t_inpcb);
	tp->rcv_numsacks = 0;
	for (i = 0; i < MAX_SACK_BLKS; i++)
		tp->sackblks[i].start = tp->sackblks[i].end=0;
}

/*
 * Process the TCP SACK option.  Returns 1 if tcp_dooptions() should continue,
 * and 0 otherwise, if the option was fine.  tp->snd_holes is an ordered list
 * of holes (oldest to newest, in terms of the sequence space).
 */
int
tcp_sack_option(struct tcpcb *tp, struct tcphdr *th, u_char *cp, int optlen)
{
	int tmp_olen;
	u_char *tmp_cp;
	struct sackhole *cur, *temp;

	INP_LOCK_ASSERT(tp->t_inpcb);
	if (!tp->sack_enable)
		return (1);
	if ((th->th_flags & TH_ACK) == 0)
		return (1);
	/* Note: TCPOLEN_SACK must be 2*sizeof(tcp_seq) */
	if (optlen <= 2 || (optlen - 2) % TCPOLEN_SACK != 0)
		return (1);
	/* If ack is outside [snd_una, snd_max], ignore the SACK options */
	if (SEQ_LT(th->th_ack, tp->snd_una) || SEQ_GT(th->th_ack, tp->snd_max))
		return (1);
	tmp_cp = cp + 2;
	tmp_olen = optlen - 2;
	tcpstat.tcps_sack_rcv_blocks++;
	if (tp->snd_numholes < 0) /* XXX panic? */
		tp->snd_numholes = 0;
	if (tp->t_maxseg == 0)
		panic("tcp_sack_option"); /* Should never happen */
	while (tmp_olen > 0) {
		struct sackblk sack;

		bcopy(tmp_cp, (char *) &(sack.start), sizeof(tcp_seq));
		sack.start = ntohl(sack.start);
		bcopy(tmp_cp + sizeof(tcp_seq),
		    (char *) &(sack.end), sizeof(tcp_seq));
		sack.end = ntohl(sack.end);
		tmp_olen -= TCPOLEN_SACK;
		tmp_cp += TCPOLEN_SACK;
		if (SEQ_LEQ(sack.end, sack.start))
			continue; /* bad SACK fields */
		if (SEQ_LEQ(sack.end, tp->snd_una))
			continue; /* old block */
		if (SEQ_GT(th->th_ack, tp->snd_una)) {
			if (SEQ_LT(sack.start, th->th_ack))
				continue;
		}
		if (SEQ_GT(sack.end, tp->snd_max))
			continue;
		if (TAILQ_EMPTY(&tp->snd_holes)) { /* first hole */
			if (tcp_sack_globalholes >= tcp_sack_globalmaxholes ||
			    tcp_sack_maxholes == 0) {
				tcpstat.tcps_sack_sboverflow++;
				continue;
			}
			cur = (struct sackhole *)
				uma_zalloc(sack_hole_zone,M_NOWAIT);
			if (cur == NULL) {
				/* ENOBUFS, so ignore SACKed block for now*/
				continue;
			}
			cur->start = th->th_ack;
			cur->end = sack.start;
			cur->rxmit = cur->start;
			tp->snd_numholes = 1;
			tcp_sack_globalholes++;
			tp->rcv_lastsack = sack.end;
			TAILQ_INSERT_HEAD(&tp->snd_holes, cur, scblink);
			continue; /* with next sack block */
		}
		/* Go thru list of holes. */
		cur = TAILQ_FIRST(&tp->snd_holes);
		while (cur) {
			if (SEQ_LEQ(sack.end, cur->start))
				/* SACKs data before the current hole */
				break; /* no use going through more holes */
			if (SEQ_GEQ(sack.start, cur->end)) {
				/* SACKs data beyond the current hole */
				cur = TAILQ_NEXT(cur, scblink);
				continue;
			}
			if (SEQ_LEQ(sack.start, cur->start)) {
				/* Data acks at least the beginning of hole */
				if (SEQ_GEQ(sack.end, cur->end)) {
					/* Acks entire hole, so delete hole */
					temp = cur;
					cur = TAILQ_NEXT(cur, scblink);
					TAILQ_REMOVE(&tp->snd_holes,
						temp, scblink);
					uma_zfree(sack_hole_zone, temp);
					tp->snd_numholes--;
					tcp_sack_globalholes--;
					continue;
				}
				/* otherwise, move start of hole forward */
				cur->start = sack.end;
				cur->rxmit = SEQ_MAX(cur->rxmit, cur->start);
				cur = TAILQ_NEXT(cur, scblink);
				continue;
			}
			/* move end of hole backward */
			if (SEQ_GEQ(sack.end, cur->end)) {
				cur->end = sack.start;
				cur->rxmit = SEQ_MIN(cur->rxmit, cur->end);
				cur = TAILQ_NEXT(cur, scblink);
				continue;
			}
			if (SEQ_LT(cur->start, sack.start) &&
			    SEQ_GT(cur->end, sack.end)) {
				/*
				 * ACKs some data in middle of a hole; need to
				 * split current hole
				 */
				if (tp->snd_numholes >= tcp_sack_maxholes ||
					tcp_sack_globalholes >= 
					 tcp_sack_globalmaxholes) {
					tcpstat.tcps_sack_sboverflow++;
					continue;
				}
				temp = (struct sackhole *)
					uma_zalloc(sack_hole_zone,M_NOWAIT);
				if (temp == NULL)
					continue; /* ENOBUFS */
				temp->start = sack.end;
				temp->end = cur->end;
				temp->rxmit = SEQ_MAX(cur->rxmit, temp->start);
				cur->end = sack.start;
				cur->rxmit = SEQ_MIN(cur->rxmit, cur->end);
				TAILQ_INSERT_AFTER(&tp->snd_holes,
					cur, temp, scblink);
				cur = TAILQ_NEXT(temp, scblink);
				tp->snd_numholes++;
				tcp_sack_globalholes++;
			}
		}
		/* At this point, we are at the tail of the scoreboard. */
		if (SEQ_LT(tp->rcv_lastsack, sack.start)) {
			/* Need to append new hole at end. */
			if (tp->snd_numholes >= tcp_sack_maxholes ||
			    tcp_sack_globalholes >= tcp_sack_globalmaxholes) {
				tcpstat.tcps_sack_sboverflow++;
				continue;
			}
			temp = (struct sackhole *)
				uma_zalloc(sack_hole_zone,M_NOWAIT);
			if (temp == NULL)
				continue; /* ENOBUFS */
			temp->start = tp->rcv_lastsack;
			temp->end = sack.start;
			temp->rxmit = temp->start;
			tp->rcv_lastsack = sack.end;
			tp->snd_numholes++;
			tcp_sack_globalholes++;
			TAILQ_INSERT_TAIL(&tp->snd_holes, temp, scblink);
		}
		if (SEQ_LT(tp->rcv_lastsack, sack.end))
			tp->rcv_lastsack = sack.end;
	}
	return (0);
}

/*
 * Delete stale (i.e, cumulatively ack'd) holes.  Hole is deleted only if
 * it is completely acked; otherwise, tcp_sack_option(), called from
 * tcp_dooptions(), will fix up the hole.
 */
void
tcp_del_sackholes(tp, th)
	struct tcpcb *tp;
	struct tcphdr *th;
{
	INP_LOCK_ASSERT(tp->t_inpcb);
	if (tp->sack_enable && tp->t_state != TCPS_LISTEN) {
		/* max because this could be an older ack just arrived */
		tcp_seq lastack = SEQ_GT(th->th_ack, tp->snd_una) ?
			th->th_ack : tp->snd_una;
		struct sackhole *cur = TAILQ_FIRST(&tp->snd_holes);
		struct sackhole *prev;
		while (cur)
			if (SEQ_LEQ(cur->end, lastack)) {
				prev = cur;
				cur = TAILQ_NEXT(cur, scblink);
				TAILQ_REMOVE(&tp->snd_holes, prev, scblink);
				uma_zfree(sack_hole_zone, prev);
				tp->snd_numholes--;
				tcp_sack_globalholes--;
			} else if (SEQ_LT(cur->start, lastack)) {
				cur->start = lastack;
				if (SEQ_LT(cur->rxmit, cur->start))
					cur->rxmit = cur->start;
				break;
			} else
				break;
	}
}

void
tcp_free_sackholes(struct tcpcb *tp)
{
	struct sackhole *q;

	INP_LOCK_ASSERT(tp->t_inpcb);
	while ((q = TAILQ_FIRST(&tp->snd_holes)) != NULL) {
		TAILQ_REMOVE(&tp->snd_holes, q, scblink);
		uma_zfree(sack_hole_zone, q);
		tcp_sack_globalholes--;
	}
	tp->snd_numholes = 0;
}

/*
 * Partial ack handling within a sack recovery episode. 
 * Keeping this very simple for now. When a partial ack
 * is received, force snd_cwnd to a value that will allow
 * the sender to transmit no more than 2 segments.
 * If necessary, a better scheme can be adopted at a 
 * later point, but for now, the goal is to prevent the
 * sender from bursting a large amount of data in the midst
 * of sack recovery.
 */
void
tcp_sack_partialack(tp, th)
	struct tcpcb *tp;
	struct tcphdr *th;
{
	int num_segs = 1;
	int sack_bytes_rxmt = 0;

	INP_LOCK_ASSERT(tp->t_inpcb);
	callout_stop(tp->tt_rexmt);
	tp->t_rtttime = 0;
	/* send one or 2 segments based on how much new data was acked */
	if (((th->th_ack - tp->snd_una) / tp->t_maxseg) > 2)
		num_segs = 2;
	(void)tcp_sack_output(tp, &sack_bytes_rxmt);
	tp->snd_cwnd = sack_bytes_rxmt + (tp->snd_nxt - tp->sack_newdata) +
		num_segs * tp->t_maxseg;
	if (tp->snd_cwnd > tp->snd_ssthresh)
		tp->snd_cwnd = tp->snd_ssthresh;
	tp->t_flags |= TF_ACKNOW;
	(void) tcp_output(tp);
}

/*
 * Returns pointer to a sackhole if there are any pending retransmissions;
 * NULL otherwise.
 */
struct sackhole *
tcp_sack_output(struct tcpcb *tp, int *sack_bytes_rexmt)
{
	struct sackhole *p;

	INP_LOCK_ASSERT(tp->t_inpcb);
	if (!tp->sack_enable)
		return (NULL);
	*sack_bytes_rexmt = 0;
	TAILQ_FOREACH(p, &tp->snd_holes, scblink) {
		if (SEQ_LT(p->rxmit, p->end)) {
			if (SEQ_LT(p->rxmit, tp->snd_una)) {/* old SACK hole */
				continue;
			}
			*sack_bytes_rexmt += (p->rxmit - p->start);
			break;
		}
		*sack_bytes_rexmt += (p->rxmit - p->start);
	}
	return (p);
}

/*
 * After a timeout, the SACK list may be rebuilt.  This SACK information
 * should be used to avoid retransmitting SACKed data.  This function
 * traverses the SACK list to see if snd_nxt should be moved forward.
 */
void
tcp_sack_adjust(struct tcpcb *tp)
{
	struct sackhole *p, *cur = TAILQ_FIRST(&tp->snd_holes);

	INP_LOCK_ASSERT(tp->t_inpcb);
	if (cur == NULL)
		return; /* No holes */
	if (SEQ_GEQ(tp->snd_nxt, tp->rcv_lastsack))
		return; /* We're already beyond any SACKed blocks */
	/*
	 * Two cases for which we want to advance snd_nxt:
	 * i) snd_nxt lies between end of one hole and beginning of another
	 * ii) snd_nxt lies between end of last hole and rcv_lastsack
	 */
	while ((p = TAILQ_NEXT(cur, scblink)) != NULL) {
		if (SEQ_LT(tp->snd_nxt, cur->end))
			return;
		if (SEQ_GEQ(tp->snd_nxt, p->start))
			cur = p;
		else {
			tp->snd_nxt = p->start;
			return;
		}
	}
	if (SEQ_LT(tp->snd_nxt, cur->end))
		return;
	tp->snd_nxt = tp->rcv_lastsack;
	return;
}

/*
 * Calculate the number of SACKed bytes in the scoreboard by
 * subtracting the amount of data accounted for in sackholes
 * from the total span of the scoreboard. Also returns the 
 * amount of data that is "lost" and has not yet been retransmitted.
 */
int
tcp_sacked_bytes(struct tcpcb *tp, int *lost_not_rexmitted)
{
	INP_LOCK_ASSERT(tp->t_inpcb);
	struct sackhole *cur = TAILQ_FIRST(&tp->snd_holes);
	int sacked = 0;
	int lost = 0;

	if (cur == NULL) /* Scoreboard empty. */
		goto out;
	if (SEQ_GEQ(tp->snd_una, tp->rcv_lastsack)) /* Scoreboard is stale. */
		goto out;
	sacked = tp->rcv_lastsack - cur->start;
	while (cur) {
		lost += (cur->end - cur->rxmit);
		sacked -= (cur->end - cur->start);
		cur = TAILQ_NEXT(cur, scblink);
	}
out:
	if (lost_not_rexmitted)
		*lost_not_rexmitted = lost;
	return (sacked);
}
