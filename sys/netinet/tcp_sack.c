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

u_char tcp_saveipgen[40]; /* the size must be of max ip header, now IPv6 */
struct tcphdr tcp_savetcp;
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

/*
 * This function is called upon receipt of new valid data (while not in header
 * prediction mode), and it updates the ordered list of sacks.
 */
void
tcp_update_sack_list(tp, rcv_laststart, rcv_lastend)
	struct tcpcb *tp;
	tcp_seq rcv_laststart, rcv_lastend;
{
	/*
	 * First reported block MUST be the most recent one.  Subsequent
	 * blocks SHOULD be in the order in which they arrived at the
	 * receiver.  These two conditions make the implementation fully
	 * compliant with RFC 2018.
	 */
	int i, j = 0, count = 0, lastpos = -1;
	struct sackblk sack, firstsack, temp[MAX_SACK_BLKS];

	INP_LOCK_ASSERT(tp->t_inpcb);
	/* First clean up current list of sacks */
	for (i = 0; i < tp->rcv_numsacks; i++) {
		sack = tp->sackblks[i];
		if (sack.start == 0 && sack.end == 0) {
			count++; /* count = number of blocks to be discarded */
			continue;
		}
		if (SEQ_LEQ(sack.end, tp->rcv_nxt)) {
			tp->sackblks[i].start = tp->sackblks[i].end = 0;
			count++;
		} else {
			temp[j].start = tp->sackblks[i].start;
			temp[j++].end = tp->sackblks[i].end;
		}
	}
	tp->rcv_numsacks -= count;
	if (tp->rcv_numsacks == 0) { /* no sack blocks currently (fast path) */
		tcp_clean_sackreport(tp);
		if (SEQ_LT(tp->rcv_nxt, rcv_laststart)) {
			/* ==> need first sack block */
			tp->sackblks[0].start = rcv_laststart;
			tp->sackblks[0].end = rcv_lastend;
			tp->rcv_numsacks = 1;
		}
		return;
	}
	/* Otherwise, sack blocks are already present. */
	for (i = 0; i < tp->rcv_numsacks; i++)
		tp->sackblks[i] = temp[i]; /* first copy back sack list */
	if (SEQ_GEQ(tp->rcv_nxt, rcv_lastend))
		return;     /* sack list remains unchanged */
	/*
	 * From here, segment just received should be (part of) the 1st sack.
	 * Go through list, possibly coalescing sack block entries.
	 */
	firstsack.start = rcv_laststart;
	firstsack.end = rcv_lastend;
	for (i = 0; i < tp->rcv_numsacks; i++) {
		sack = tp->sackblks[i];
		if (SEQ_LT(sack.end, firstsack.start) ||
		    SEQ_GT(sack.start, firstsack.end))
			continue; /* no overlap */
		if (sack.start == firstsack.start && sack.end == firstsack.end){
			/*
			 * identical block; delete it here since we will
			 * move it to the front of the list.
			 */
			tp->sackblks[i].start = tp->sackblks[i].end = 0;
			lastpos = i;    /* last posn with a zero entry */
			continue;
		}
		if (SEQ_LEQ(sack.start, firstsack.start))
			firstsack.start = sack.start; /* merge blocks */
		if (SEQ_GEQ(sack.end, firstsack.end))
			firstsack.end = sack.end;     /* merge blocks */
		tp->sackblks[i].start = tp->sackblks[i].end = 0;
		lastpos = i;    /* last posn with a zero entry */
	}
	if (lastpos != -1) {    /* at least one merge */
		for (i = 0, j = 1; i < tp->rcv_numsacks; i++) {
			sack = tp->sackblks[i];
			if (sack.start == 0 && sack.end == 0)
				continue;
			temp[j++] = sack;
		}
		tp->rcv_numsacks = j; /* including first blk (added later) */
		for (i = 1; i < tp->rcv_numsacks; i++) /* now copy back */
			tp->sackblks[i] = temp[i];
	} else {        /* no merges -- shift sacks by 1 */
		if (tp->rcv_numsacks < MAX_SACK_BLKS)
			tp->rcv_numsacks++;
		for (i = tp->rcv_numsacks-1; i > 0; i--)
			tp->sackblks[i] = tp->sackblks[i-1];
	}
	tp->sackblks[0] = firstsack;
	return;
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
	struct sackhole *cur, *p, *temp;

	INP_LOCK_ASSERT(tp->t_inpcb);
	if (!tp->sack_enable)
		return (1);

	/* Note: TCPOLEN_SACK must be 2*sizeof(tcp_seq) */
	if (optlen <= 2 || (optlen - 2) % TCPOLEN_SACK != 0)
		return (1);
	/* If ack is outside window, ignore the SACK options */
	if (SEQ_LT(th->th_ack, tp->snd_una) || SEQ_GT(th->th_ack, tp->snd_max))
		return (1);
	tmp_cp = cp + 2;
	tmp_olen = optlen - 2;
	tcpstat.tcps_sack_rcv_blocks++;
	if (tp->snd_numholes < 0)
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
		if (tp->snd_holes == NULL) { /* first hole */
			tp->snd_holes = (struct sackhole *)
				uma_zalloc(sack_hole_zone,M_NOWAIT);
			if (tp->snd_holes == NULL) {
				/* ENOBUFS, so ignore SACKed block for now*/
				continue;
			}
			cur = tp->snd_holes;
			cur->start = th->th_ack;
			cur->end = sack.start;
			cur->rxmit = cur->start;
			cur->next = NULL;
			tp->snd_numholes = 1;
			tp->rcv_lastsack = sack.end;
			continue; /* with next sack block */
		}
		/* Go thru list of holes:  p = previous,  cur = current */
		p = cur = tp->snd_holes;
		while (cur) {
			if (SEQ_LEQ(sack.end, cur->start))
				/* SACKs data before the current hole */
				break; /* no use going through more holes */
			if (SEQ_GEQ(sack.start, cur->end)) {
				/* SACKs data beyond the current hole */
				p = cur;
				cur = cur->next;
				continue;
			}
			if (SEQ_LEQ(sack.start, cur->start)) {
				/* Data acks at least the beginning of hole */
				if (SEQ_GEQ(sack.end, cur->end)) {
					/* Acks entire hole, so delete hole */
					if (p != cur) {
						p->next = cur->next;
						uma_zfree(sack_hole_zone, cur);
						cur = p->next;
					} else {
						cur = cur->next;
						uma_zfree(sack_hole_zone, p);
						p = cur;
						tp->snd_holes = p;
					}
					tp->snd_numholes--;
					continue;
				}
				/* otherwise, move start of hole forward */
				cur->start = sack.end;
				cur->rxmit = SEQ_MAX(cur->rxmit, cur->start);
				p = cur;
				cur = cur->next;
				continue;
			}
			/* move end of hole backward */
			if (SEQ_GEQ(sack.end, cur->end)) {
				cur->end = sack.start;
				cur->rxmit = SEQ_MIN(cur->rxmit, cur->end);
				p = cur;
				cur = cur->next;
				continue;
			}
			if (SEQ_LT(cur->start, sack.start) &&
			    SEQ_GT(cur->end, sack.end)) {
				/*
				 * ACKs some data in middle of a hole; need to
				 * split current hole
				 */
				temp = (struct sackhole *)
					uma_zalloc(sack_hole_zone,M_NOWAIT);
				if (temp == NULL)
					continue; /* ENOBUFS */
				temp->next = cur->next;
				temp->start = sack.end;
				temp->end = cur->end;
				temp->rxmit = SEQ_MAX(cur->rxmit, temp->start);
				cur->end = sack.start;
				cur->rxmit = SEQ_MIN(cur->rxmit, cur->end);
				cur->next = temp;
				p = temp;
				cur = p->next;
				tp->snd_numholes++;
			}
		}
		/* At this point, p points to the last hole on the list */
		if (SEQ_LT(tp->rcv_lastsack, sack.start)) {
			/*
			 * Need to append new hole at end.
			 * Last hole is p (and it's not NULL).
			 */
			temp = (struct sackhole *)
				uma_zalloc(sack_hole_zone,M_NOWAIT);
			if (temp == NULL)
				continue; /* ENOBUFS */
			temp->start = tp->rcv_lastsack;
			temp->end = sack.start;
			temp->rxmit = temp->start;
			temp->next = 0;
			p->next = temp;
			tp->rcv_lastsack = sack.end;
			tp->snd_numholes++;
		}
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
		struct sackhole *cur = tp->snd_holes;
		struct sackhole *prev;
		while (cur)
			if (SEQ_LEQ(cur->end, lastack)) {
				prev = cur;
				cur = cur->next;
				uma_zfree(sack_hole_zone, prev);
				tp->snd_numholes--;
			} else if (SEQ_LT(cur->start, lastack)) {
				cur->start = lastack;
				if (SEQ_LT(cur->rxmit, cur->start))
					cur->rxmit = cur->start;
				break;
			} else
				break;
		tp->snd_holes = cur;
	}
}

void
tcp_free_sackholes(struct tcpcb *tp)
{
	struct sackhole *p, *q;

	INP_LOCK_ASSERT(tp->t_inpcb);
	q = tp->snd_holes;
	while (q != NULL) {
		p = q;
		q = q->next;
		uma_zfree(sack_hole_zone, p);
	}
	tp->snd_holes = 0;
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
	INP_LOCK_ASSERT(tp->t_inpcb);
	int num_segs = 1;
	int sack_bytes_rxmt = 0;

	callout_stop(tp->tt_rexmt);
	tp->t_rtttime = 0;
	/* send one or 2 segments based on how much new data was acked */
	if (((th->th_ack - tp->snd_una) / tp->t_maxseg) > 2)
		num_segs = 2;
	(void)tcp_sack_output(tp, &sack_bytes_rxmt);
	tp->snd_cwnd = sack_bytes_rxmt + (tp->snd_nxt - tp->sack_newdata) +
		num_segs * tp->t_maxseg;
	tp->t_flags |= TF_ACKNOW;
	(void) tcp_output(tp);
}

#ifdef TCP_SACK_DEBUG
void
tcp_print_holes(struct tcpcb *tp)
{
	struct sackhole *p = tp->snd_holes;
	if (p == 0)
		return;
	printf("Hole report: start--end dups rxmit\n");
	while (p) {
		printf("%x--%x r %x\n", p->start, p->end, p->rxmit);
		p = p->next;
	}
	printf("\n");
}
#endif /* TCP_SACK_DEBUG */

/*
 * Returns pointer to a sackhole if there are any pending retransmissions;
 * NULL otherwise.
 */
struct sackhole *
tcp_sack_output(struct tcpcb *tp, int *sack_bytes_rexmt)
{
	struct sackhole *p = NULL;

	INP_LOCK_ASSERT(tp->t_inpcb);
	if (!tp->sack_enable)
		return (NULL);
	*sack_bytes_rexmt = 0;
	for (p = tp->snd_holes; p ; p = p->next) {
		if (SEQ_LT(p->rxmit, p->end)) {
			if (SEQ_LT(p->rxmit, tp->snd_una)) {/* old SACK hole */
				continue;
			}
#ifdef TCP_SACK_DEBUG
			if (p)
				tcp_print_holes(tp);
#endif
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
	INP_LOCK_ASSERT(tp->t_inpcb);
	struct sackhole *cur = tp->snd_holes;
	if (cur == NULL)
		return; /* No holes */
	if (SEQ_GEQ(tp->snd_nxt, tp->rcv_lastsack))
		return; /* We're already beyond any SACKed blocks */
	/*
	 * Two cases for which we want to advance snd_nxt:
	 * i) snd_nxt lies between end of one hole and beginning of another
	 * ii) snd_nxt lies between end of last hole and rcv_lastsack
	 */
	while (cur->next) {
		if (SEQ_LT(tp->snd_nxt, cur->end))
			return;
		if (SEQ_GEQ(tp->snd_nxt, cur->next->start))
			cur = cur->next;
		else {
			tp->snd_nxt = cur->next->start;
			return;
		}
	}
	if (SEQ_LT(tp->snd_nxt, cur->end))
		return;
	tp->snd_nxt = tp->rcv_lastsack;
	return;
}
