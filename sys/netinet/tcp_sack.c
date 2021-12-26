/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1994, 1995
 *	The Regents of the University of California.
 * All rights reserved.
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
 *	@(#)tcp_sack.c	8.12 (Berkeley) 5/24/95
 */

/*-
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_tcpdebug.h"

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
#include <net/if_var.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_var.h>
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
#include <netinet/cc/cc.h>
#ifdef TCPDEBUG
#include <netinet/tcp_debug.h>
#endif /* TCPDEBUG */

#include <machine/in_cksum.h>

VNET_DECLARE(struct uma_zone *, sack_hole_zone);
#define	V_sack_hole_zone		VNET(sack_hole_zone)

SYSCTL_NODE(_net_inet_tcp, OID_AUTO, sack, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "TCP SACK");

VNET_DEFINE(int, tcp_do_sack) = 1;
SYSCTL_INT(_net_inet_tcp_sack, OID_AUTO, enable, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(tcp_do_sack), 0,
    "Enable/Disable TCP SACK support");

VNET_DEFINE(int, tcp_do_newsack) = 1;
SYSCTL_INT(_net_inet_tcp_sack, OID_AUTO, revised, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(tcp_do_newsack), 0,
    "Use revised SACK loss recovery per RFC 6675");

VNET_DEFINE(int, tcp_sack_maxholes) = 128;
SYSCTL_INT(_net_inet_tcp_sack, OID_AUTO, maxholes, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(tcp_sack_maxholes), 0,
    "Maximum number of TCP SACK holes allowed per connection");

VNET_DEFINE(int, tcp_sack_globalmaxholes) = 65536;
SYSCTL_INT(_net_inet_tcp_sack, OID_AUTO, globalmaxholes, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(tcp_sack_globalmaxholes), 0,
    "Global maximum number of TCP SACK holes");

VNET_DEFINE(int, tcp_sack_globalholes) = 0;
SYSCTL_INT(_net_inet_tcp_sack, OID_AUTO, globalholes, CTLFLAG_VNET | CTLFLAG_RD,
    &VNET_NAME(tcp_sack_globalholes), 0,
    "Global number of TCP SACK holes currently allocated");

int
tcp_dsack_block_exists(struct tcpcb *tp)
{
	/* Return true if a DSACK block exists */
	if (tp->rcv_numsacks == 0)
		return (0);
	if (SEQ_LEQ(tp->sackblks[0].end, tp->rcv_nxt))
		return(1);
	return (0);
}

/*
 * This function will find overlaps with the currently stored sackblocks
 * and add any overlap as a dsack block upfront
 */
void
tcp_update_dsack_list(struct tcpcb *tp, tcp_seq rcv_start, tcp_seq rcv_end)
{
	struct sackblk head_blk,mid_blk,saved_blks[MAX_SACK_BLKS];
	int i, j, n, identical;
	tcp_seq start, end;

	INP_WLOCK_ASSERT(tp->t_inpcb);

	KASSERT(SEQ_LT(rcv_start, rcv_end), ("rcv_start < rcv_end"));

	if (SEQ_LT(rcv_end, tp->rcv_nxt) ||
	    ((rcv_end == tp->rcv_nxt) &&
	     (tp->rcv_numsacks > 0 ) &&
	     (tp->sackblks[0].end == tp->rcv_nxt))) {
		saved_blks[0].start = rcv_start;
		saved_blks[0].end = rcv_end;
	} else {
		saved_blks[0].start = saved_blks[0].end = 0;
	}

	head_blk.start = head_blk.end = 0;
	mid_blk.start = rcv_start;
	mid_blk.end = rcv_end;
	identical = 0;

	for (i = 0; i < tp->rcv_numsacks; i++) {
		start = tp->sackblks[i].start;
		end = tp->sackblks[i].end;
		if (SEQ_LT(rcv_end, start)) {
			/* pkt left to sack blk */
			continue;
		}
		if (SEQ_GT(rcv_start, end)) {
			/* pkt right to sack blk */
			continue;
		}
		if (SEQ_GT(tp->rcv_nxt, end)) {
			if ((SEQ_MAX(rcv_start, start) != SEQ_MIN(rcv_end, end)) &&
			    (SEQ_GT(head_blk.start, SEQ_MAX(rcv_start, start)) ||
			    (head_blk.start == head_blk.end))) {
				head_blk.start = SEQ_MAX(rcv_start, start);
				head_blk.end = SEQ_MIN(rcv_end, end);
			}
			continue;
		}
		if (((head_blk.start == head_blk.end) ||
		     SEQ_LT(start, head_blk.start)) &&
		     (SEQ_GT(end, rcv_start) &&
		      SEQ_LEQ(start, rcv_end))) {
			head_blk.start = start;
			head_blk.end = end;
		}
		mid_blk.start = SEQ_MIN(mid_blk.start, start);
		mid_blk.end = SEQ_MAX(mid_blk.end, end);
		if ((mid_blk.start == start) &&
		    (mid_blk.end == end))
			identical = 1;
	}
	if (SEQ_LT(head_blk.start, head_blk.end)) {
		/* store overlapping range */
		saved_blks[0].start = SEQ_MAX(rcv_start, head_blk.start);
		saved_blks[0].end   = SEQ_MIN(rcv_end, head_blk.end);
	}
	n = 1;
	/*
	 * Second, if not ACKed, store the SACK block that
	 * overlaps with the DSACK block unless it is identical
	 */
	if ((SEQ_LT(tp->rcv_nxt, mid_blk.end) &&
	    !((mid_blk.start == saved_blks[0].start) &&
	    (mid_blk.end == saved_blks[0].end))) ||
	    identical == 1) {
		saved_blks[n].start = mid_blk.start;
		saved_blks[n++].end = mid_blk.end;
	}
	for (j = 0; (j < tp->rcv_numsacks) && (n < MAX_SACK_BLKS); j++) {
		if (((SEQ_LT(tp->sackblks[j].end, mid_blk.start) ||
		      SEQ_GT(tp->sackblks[j].start, mid_blk.end)) &&
		    (SEQ_GT(tp->sackblks[j].start, tp->rcv_nxt))))
		saved_blks[n++] = tp->sackblks[j];
	}
	j = 0;
	for (i = 0; i < n; i++) {
		/* we can end up with a stale initial entry */
		if (SEQ_LT(saved_blks[i].start, saved_blks[i].end)) {
			tp->sackblks[j++] = saved_blks[i];
		}
	}
	tp->rcv_numsacks = j;
}

/*
 * This function is called upon receipt of new valid data (while not in
 * header prediction mode), and it updates the ordered list of sacks.
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

	INP_WLOCK_ASSERT(tp->t_inpcb);

	/* Check arguments. */
	KASSERT(SEQ_LEQ(rcv_start, rcv_end), ("rcv_start <= rcv_end"));

	if ((rcv_start == rcv_end) &&
	    (tp->rcv_numsacks >= 1) &&
	    (rcv_end == tp->sackblks[0].end)) {
		/* retaining DSACK block below rcv_nxt (todrop) */
		head_blk = tp->sackblks[0];
	} else {
		/* SACK block for the received segment. */
		head_blk.start = rcv_start;
		head_blk.end = rcv_end;
	}

	/*
	 * Merge updated SACK blocks into head_blk, and save unchanged SACK
	 * blocks into saved_blks[].  num_saved will have the number of the
	 * saved SACK blocks.
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
			 * Merge this SACK block into head_blk.  This SACK
			 * block itself will be discarded.
			 */
			/*
			 * |-|
			 *   |---|  merge
			 *
			 *     |-|
			 * |---|    merge
			 *
			 * |-----|
			 *   |-|    DSACK smaller
			 *
			 *   |-|
			 * |-----|  DSACK smaller
			 */
			if (head_blk.start == end)
				head_blk.start = start;
			else if (head_blk.end == start)
				head_blk.end = end;
			else {
				if (SEQ_LT(head_blk.start, start)) {
					tcp_seq temp = start;
					start = head_blk.start;
					head_blk.start = temp;
				}
				if (SEQ_GT(head_blk.end, end)) {
					tcp_seq temp = end;
					end = head_blk.end;
					head_blk.end = temp;
				}
				if ((head_blk.start != start) ||
				    (head_blk.end != end)) {
					if ((num_saved >= 1) &&
					   SEQ_GEQ(saved_blks[num_saved-1].start, start) &&
					   SEQ_LEQ(saved_blks[num_saved-1].end, end))
						num_saved--;
					saved_blks[num_saved].start = start;
					saved_blks[num_saved].end = end;
					num_saved++;
				}
			}
		} else {
			/*
			 * This block supercedes the prior block
			 */
			if ((num_saved >= 1) &&
			   SEQ_GEQ(saved_blks[num_saved-1].start, start) &&
			   SEQ_LEQ(saved_blks[num_saved-1].end, end))
				num_saved--;
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
	if (SEQ_LT(rcv_start, rcv_end)) {
		/*
		 * The received data segment is an out-of-order segment.  Put
		 * head_blk at the top of SACK list.
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
	if ((rcv_start == rcv_end) &&
	    (rcv_start == tp->sackblks[0].end)) {
		num_head = 1;
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

void
tcp_clean_dsack_blocks(struct tcpcb *tp)
{
	struct sackblk saved_blks[MAX_SACK_BLKS];
	int num_saved, i;

	INP_WLOCK_ASSERT(tp->t_inpcb);
	/*
	 * Clean up any DSACK blocks that
	 * are in our queue of sack blocks.
	 *
	 */
	num_saved = 0;
	for (i = 0; i < tp->rcv_numsacks; i++) {
		tcp_seq start = tp->sackblks[i].start;
		tcp_seq end = tp->sackblks[i].end;
		if (SEQ_GEQ(start, end) || SEQ_LEQ(start, tp->rcv_nxt)) {
			/*
			 * Discard this D-SACK block.
			 */
			continue;
		}
		/*
		 * Save this SACK block.
		 */
		saved_blks[num_saved].start = start;
		saved_blks[num_saved].end = end;
		num_saved++;
	}
	if (num_saved > 0) {
		/*
		 * Copy the saved SACK blocks back.
		 */
		bcopy(saved_blks, &tp->sackblks[0],
		      sizeof(struct sackblk) * num_saved);
	}
	tp->rcv_numsacks = num_saved;
}

/*
 * Delete all receiver-side SACK information.
 */
void
tcp_clean_sackreport(struct tcpcb *tp)
{
	int i;

	INP_WLOCK_ASSERT(tp->t_inpcb);
	tp->rcv_numsacks = 0;
	for (i = 0; i < MAX_SACK_BLKS; i++)
		tp->sackblks[i].start = tp->sackblks[i].end=0;
}

/*
 * Allocate struct sackhole.
 */
static struct sackhole *
tcp_sackhole_alloc(struct tcpcb *tp, tcp_seq start, tcp_seq end)
{
	struct sackhole *hole;

	if (tp->snd_numholes >= V_tcp_sack_maxholes ||
	    V_tcp_sack_globalholes >= V_tcp_sack_globalmaxholes) {
		TCPSTAT_INC(tcps_sack_sboverflow);
		return NULL;
	}

	hole = (struct sackhole *)uma_zalloc(V_sack_hole_zone, M_NOWAIT);
	if (hole == NULL)
		return NULL;

	hole->start = start;
	hole->end = end;
	hole->rxmit = start;

	tp->snd_numholes++;
	atomic_add_int(&V_tcp_sack_globalholes, 1);

	return hole;
}

/*
 * Free struct sackhole.
 */
static void
tcp_sackhole_free(struct tcpcb *tp, struct sackhole *hole)
{

	uma_zfree(V_sack_hole_zone, hole);

	tp->snd_numholes--;
	atomic_subtract_int(&V_tcp_sack_globalholes, 1);

	KASSERT(tp->snd_numholes >= 0, ("tp->snd_numholes >= 0"));
	KASSERT(V_tcp_sack_globalholes >= 0, ("tcp_sack_globalholes >= 0"));
}

/*
 * Insert new SACK hole into scoreboard.
 */
static struct sackhole *
tcp_sackhole_insert(struct tcpcb *tp, tcp_seq start, tcp_seq end,
    struct sackhole *after)
{
	struct sackhole *hole;

	/* Allocate a new SACK hole. */
	hole = tcp_sackhole_alloc(tp, start, end);
	if (hole == NULL)
		return NULL;

	/* Insert the new SACK hole into scoreboard. */
	if (after != NULL)
		TAILQ_INSERT_AFTER(&tp->snd_holes, after, hole, scblink);
	else
		TAILQ_INSERT_TAIL(&tp->snd_holes, hole, scblink);

	/* Update SACK hint. */
	if (tp->sackhint.nexthole == NULL)
		tp->sackhint.nexthole = hole;

	return hole;
}

/*
 * Remove SACK hole from scoreboard.
 */
static void
tcp_sackhole_remove(struct tcpcb *tp, struct sackhole *hole)
{

	/* Update SACK hint. */
	if (tp->sackhint.nexthole == hole)
		tp->sackhint.nexthole = TAILQ_NEXT(hole, scblink);

	/* Remove this SACK hole. */
	TAILQ_REMOVE(&tp->snd_holes, hole, scblink);

	/* Free this SACK hole. */
	tcp_sackhole_free(tp, hole);
}

/*
 * Process cumulative ACK and the TCP SACK option to update the scoreboard.
 * tp->snd_holes is an ordered list of holes (oldest to newest, in terms of
 * the sequence space).
 * Returns 1 if incoming ACK has previously unknown SACK information,
 * 0 otherwise.
 */
int
tcp_sack_doack(struct tcpcb *tp, struct tcpopt *to, tcp_seq th_ack)
{
	struct sackhole *cur, *temp;
	struct sackblk sack, sack_blocks[TCP_MAX_SACK + 1], *sblkp;
	int i, j, num_sack_blks, sack_changed;
	int delivered_data, left_edge_delta;

	INP_WLOCK_ASSERT(tp->t_inpcb);

	num_sack_blks = 0;
	sack_changed = 0;
	delivered_data = 0;
	left_edge_delta = 0;
	/*
	 * If SND.UNA will be advanced by SEG.ACK, and if SACK holes exist,
	 * treat [SND.UNA, SEG.ACK) as if it is a SACK block.
	 * Account changes to SND.UNA always in delivered data.
	 */
	if (SEQ_LT(tp->snd_una, th_ack) && !TAILQ_EMPTY(&tp->snd_holes)) {
		left_edge_delta = th_ack - tp->snd_una;
		sack_blocks[num_sack_blks].start = tp->snd_una;
		sack_blocks[num_sack_blks++].end = th_ack;
		/*
		 * Pulling snd_fack forward if we got here
		 * due to DSACK blocks
		 */
		if (SEQ_LT(tp->snd_fack, th_ack)) {
			delivered_data += th_ack - tp->snd_una;
			tp->snd_fack = th_ack;
			sack_changed = 1;
		}
	}
	/*
	 * Append received valid SACK blocks to sack_blocks[], but only if we
	 * received new blocks from the other side.
	 */
	if (to->to_flags & TOF_SACK) {
		for (i = 0; i < to->to_nsacks; i++) {
			bcopy((to->to_sacks + i * TCPOLEN_SACK),
			    &sack, sizeof(sack));
			sack.start = ntohl(sack.start);
			sack.end = ntohl(sack.end);
			if (SEQ_GT(sack.end, sack.start) &&
			    SEQ_GT(sack.start, tp->snd_una) &&
			    SEQ_GT(sack.start, th_ack) &&
			    SEQ_LT(sack.start, tp->snd_max) &&
			    SEQ_GT(sack.end, tp->snd_una) &&
			    SEQ_LEQ(sack.end, tp->snd_max)) {
				sack_blocks[num_sack_blks++] = sack;
			} else if (SEQ_LEQ(sack.start, th_ack) &&
			    SEQ_LEQ(sack.end, th_ack)) {
				/*
				 * Its a D-SACK block.
				 */
				tcp_record_dsack(tp, sack.start, sack.end, 0);
			}
		}
	}
	/*
	 * Return if SND.UNA is not advanced and no valid SACK block is
	 * received.
	 */
	if (num_sack_blks == 0)
		return (sack_changed);

	/*
	 * Sort the SACK blocks so we can update the scoreboard with just one
	 * pass. The overhead of sorting up to 4+1 elements is less than
	 * making up to 4+1 passes over the scoreboard.
	 */
	for (i = 0; i < num_sack_blks; i++) {
		for (j = i + 1; j < num_sack_blks; j++) {
			if (SEQ_GT(sack_blocks[i].end, sack_blocks[j].end)) {
				sack = sack_blocks[i];
				sack_blocks[i] = sack_blocks[j];
				sack_blocks[j] = sack;
			}
		}
	}
	if (TAILQ_EMPTY(&tp->snd_holes)) {
		/*
		 * Empty scoreboard. Need to initialize snd_fack (it may be
		 * uninitialized or have a bogus value). Scoreboard holes
		 * (from the sack blocks received) are created later below
		 * (in the logic that adds holes to the tail of the
		 * scoreboard).
		 */
		tp->snd_fack = SEQ_MAX(tp->snd_una, th_ack);
		tp->sackhint.sacked_bytes = 0;	/* reset */
	}
	/*
	 * In the while-loop below, incoming SACK blocks (sack_blocks[]) and
	 * SACK holes (snd_holes) are traversed from their tails with just
	 * one pass in order to reduce the number of compares especially when
	 * the bandwidth-delay product is large.
	 *
	 * Note: Typically, in the first RTT of SACK recovery, the highest
	 * three or four SACK blocks with the same ack number are received.
	 * In the second RTT, if retransmitted data segments are not lost,
	 * the highest three or four SACK blocks with ack number advancing
	 * are received.
	 */
	sblkp = &sack_blocks[num_sack_blks - 1];	/* Last SACK block */
	tp->sackhint.last_sack_ack = sblkp->end;
	if (SEQ_LT(tp->snd_fack, sblkp->start)) {
		/*
		 * The highest SACK block is beyond fack.  First,
		 * check if there was a successful Rescue Retransmission,
		 * and move this hole left. With normal holes, snd_fack
		 * is always to the right of the end.
		 */
		if (((temp = TAILQ_LAST(&tp->snd_holes, sackhole_head)) != NULL) &&
		    SEQ_LEQ(tp->snd_fack,temp->end)) {
			temp->start = SEQ_MAX(tp->snd_fack, SEQ_MAX(tp->snd_una, th_ack));
			temp->end = sblkp->start;
			temp->rxmit = temp->start;
			delivered_data += sblkp->end - sblkp->start;
			tp->snd_fack = sblkp->end;
			sblkp--;
			sack_changed = 1;
		} else {
			/*
			 * Append a new SACK hole at the tail.  If the
			 * second or later highest SACK blocks are also
			 * beyond the current fack, they will be inserted
			 * by way of hole splitting in the while-loop below.
			 */
			temp = tcp_sackhole_insert(tp, tp->snd_fack,sblkp->start,NULL);
			if (temp != NULL) {
				delivered_data += sblkp->end - sblkp->start;
				tp->snd_fack = sblkp->end;
				/* Go to the previous sack block. */
				sblkp--;
				sack_changed = 1;
			} else {
				/*
				 * We failed to add a new hole based on the current
				 * sack block.  Skip over all the sack blocks that
				 * fall completely to the right of snd_fack and
				 * proceed to trim the scoreboard based on the
				 * remaining sack blocks.  This also trims the
				 * scoreboard for th_ack (which is sack_blocks[0]).
				 */
				while (sblkp >= sack_blocks &&
				       SEQ_LT(tp->snd_fack, sblkp->start))
					sblkp--;
				if (sblkp >= sack_blocks &&
				    SEQ_LT(tp->snd_fack, sblkp->end)) {
					delivered_data += sblkp->end - tp->snd_fack;
					tp->snd_fack = sblkp->end;
					sack_changed = 1;
				}
			}
		}
	} else if (SEQ_LT(tp->snd_fack, sblkp->end)) {
		/* fack is advanced. */
		delivered_data += sblkp->end - tp->snd_fack;
		tp->snd_fack = sblkp->end;
		sack_changed = 1;
	}
	cur = TAILQ_LAST(&tp->snd_holes, sackhole_head); /* Last SACK hole. */
	/*
	 * Since the incoming sack blocks are sorted, we can process them
	 * making one sweep of the scoreboard.
	 */
	while (sblkp >= sack_blocks  && cur != NULL) {
		if (SEQ_GEQ(sblkp->start, cur->end)) {
			/*
			 * SACKs data beyond the current hole.  Go to the
			 * previous sack block.
			 */
			sblkp--;
			continue;
		}
		if (SEQ_LEQ(sblkp->end, cur->start)) {
			/*
			 * SACKs data before the current hole.  Go to the
			 * previous hole.
			 */
			cur = TAILQ_PREV(cur, sackhole_head, scblink);
			continue;
		}
		tp->sackhint.sack_bytes_rexmit -=
		    (SEQ_MIN(cur->rxmit, cur->end) - cur->start);
		KASSERT(tp->sackhint.sack_bytes_rexmit >= 0,
		    ("sackhint bytes rtx >= 0"));
		sack_changed = 1;
		if (SEQ_LEQ(sblkp->start, cur->start)) {
			/* Data acks at least the beginning of hole. */
			if (SEQ_GEQ(sblkp->end, cur->end)) {
				/* Acks entire hole, so delete hole. */
				delivered_data += (cur->end - cur->start);
				temp = cur;
				cur = TAILQ_PREV(cur, sackhole_head, scblink);
				tcp_sackhole_remove(tp, temp);
				/*
				 * The sack block may ack all or part of the
				 * next hole too, so continue onto the next
				 * hole.
				 */
				continue;
			} else {
				/* Move start of hole forward. */
				delivered_data += (sblkp->end - cur->start);
				cur->start = sblkp->end;
				cur->rxmit = SEQ_MAX(cur->rxmit, cur->start);
			}
		} else {
			/* Data acks at least the end of hole. */
			if (SEQ_GEQ(sblkp->end, cur->end)) {
				/* Move end of hole backward. */
				delivered_data += (cur->end - sblkp->start);
				cur->end = sblkp->start;
				cur->rxmit = SEQ_MIN(cur->rxmit, cur->end);
				if ((tp->t_flags & TF_LRD) && SEQ_GEQ(cur->rxmit, cur->end))
					cur->rxmit = tp->snd_recover;
			} else {
				/*
				 * ACKs some data in middle of a hole; need
				 * to split current hole
				 */
				temp = tcp_sackhole_insert(tp, sblkp->end,
				    cur->end, cur);
				if (temp != NULL) {
					if (SEQ_GT(cur->rxmit, temp->rxmit)) {
						temp->rxmit = cur->rxmit;
						tp->sackhint.sack_bytes_rexmit +=
						    (SEQ_MIN(temp->rxmit,
						    temp->end) - temp->start);
					}
					cur->end = sblkp->start;
					cur->rxmit = SEQ_MIN(cur->rxmit,
					    cur->end);
					if ((tp->t_flags & TF_LRD) && SEQ_GEQ(cur->rxmit, cur->end))
						cur->rxmit = tp->snd_recover;
					delivered_data += (sblkp->end - sblkp->start);
				}
			}
		}
		tp->sackhint.sack_bytes_rexmit +=
		    (SEQ_MIN(cur->rxmit, cur->end) - cur->start);
		/*
		 * Testing sblkp->start against cur->start tells us whether
		 * we're done with the sack block or the sack hole.
		 * Accordingly, we advance one or the other.
		 */
		if (SEQ_LEQ(sblkp->start, cur->start))
			cur = TAILQ_PREV(cur, sackhole_head, scblink);
		else
			sblkp--;
	}
	if (!(to->to_flags & TOF_SACK))
		/*
		 * If this ACK did not contain any
		 * SACK blocks, any only moved the
		 * left edge right, it is a pure
		 * cumulative ACK. Do not count
		 * DupAck for this. Also required
		 * for RFC6675 rescue retransmission.
		 */
		sack_changed = 0;
	tp->sackhint.delivered_data = delivered_data;
	tp->sackhint.sacked_bytes += delivered_data - left_edge_delta;
	KASSERT((delivered_data >= 0), ("delivered_data < 0"));
	KASSERT((tp->sackhint.sacked_bytes >= 0), ("sacked_bytes < 0"));
	return (sack_changed);
}

/*
 * Free all SACK holes to clear the scoreboard.
 */
void
tcp_free_sackholes(struct tcpcb *tp)
{
	struct sackhole *q;

	INP_WLOCK_ASSERT(tp->t_inpcb);
	while ((q = TAILQ_FIRST(&tp->snd_holes)) != NULL)
		tcp_sackhole_remove(tp, q);
	tp->sackhint.sack_bytes_rexmit = 0;

	KASSERT(tp->snd_numholes == 0, ("tp->snd_numholes == 0"));
	KASSERT(tp->sackhint.nexthole == NULL,
		("tp->sackhint.nexthole == NULL"));
}

/*
 * Partial ack handling within a sack recovery episode.  Keeping this very
 * simple for now.  When a partial ack is received, force snd_cwnd to a value
 * that will allow the sender to transmit no more than 2 segments.  If
 * necessary, a better scheme can be adopted at a later point, but for now,
 * the goal is to prevent the sender from bursting a large amount of data in
 * the midst of sack recovery.
 */
void
tcp_sack_partialack(struct tcpcb *tp, struct tcphdr *th)
{
	int num_segs = 1;
	u_int maxseg = tcp_maxseg(tp);

	INP_WLOCK_ASSERT(tp->t_inpcb);
	tcp_timer_activate(tp, TT_REXMT, 0);
	tp->t_rtttime = 0;
	/* Send one or 2 segments based on how much new data was acked. */
	if ((BYTES_THIS_ACK(tp, th) / maxseg) >= 2)
		num_segs = 2;
	tp->snd_cwnd = (tp->sackhint.sack_bytes_rexmit +
	    (tp->snd_nxt - tp->snd_recover) + num_segs * maxseg);
	if (tp->snd_cwnd > tp->snd_ssthresh)
		tp->snd_cwnd = tp->snd_ssthresh;
	tp->t_flags |= TF_ACKNOW;
	/*
	 * RFC6675 rescue retransmission
	 * Add a hole between th_ack (snd_una is not yet set) and snd_max,
	 * if this was a pure cumulative ACK and no data was send beyond
	 * recovery point. Since the data in the socket has not been freed
	 * at this point, we check if the scoreboard is empty, and the ACK
	 * delivered some new data, indicating a full ACK. Also, if the
	 * recovery point is still at snd_max, we are probably application
	 * limited. However, this inference might not always be true. The
	 * rescue retransmission may rarely be slightly premature
	 * compared to RFC6675.
	 * The corresponding ACK+SACK will cause any further outstanding
	 * segments to be retransmitted. This addresses a corner case, when
	 * the trailing packets of a window are lost and no further data
	 * is available for sending.
	 */
	if ((V_tcp_do_newsack) &&
	    SEQ_LT(th->th_ack, tp->snd_recover) &&
	    (tp->snd_recover == tp->snd_max) &&
	    TAILQ_EMPTY(&tp->snd_holes) &&
	    (tp->sackhint.delivered_data > 0)) {
		/*
		 * Exclude FIN sequence space in
		 * the hole for the rescue retransmission,
		 * and also don't create a hole, if only
		 * the ACK for a FIN is outstanding.
		 */
		tcp_seq highdata = tp->snd_max;
		if (tp->t_flags & TF_SENTFIN)
			highdata--;
		if (th->th_ack != highdata) {
			tp->snd_fack = th->th_ack;
			(void)tcp_sackhole_insert(tp, SEQ_MAX(th->th_ack,
			    highdata - maxseg), highdata, NULL);
		}
	}
	(void) tcp_output(tp);
}

#if 0
/*
 * Debug version of tcp_sack_output() that walks the scoreboard.  Used for
 * now to sanity check the hint.
 */
static struct sackhole *
tcp_sack_output_debug(struct tcpcb *tp, int *sack_bytes_rexmt)
{
	struct sackhole *p;

	INP_WLOCK_ASSERT(tp->t_inpcb);
	*sack_bytes_rexmt = 0;
	TAILQ_FOREACH(p, &tp->snd_holes, scblink) {
		if (SEQ_LT(p->rxmit, p->end)) {
			if (SEQ_LT(p->rxmit, tp->snd_una)) {/* old SACK hole */
				continue;
			}
			*sack_bytes_rexmt += (p->rxmit - p->start);
			break;
		}
		*sack_bytes_rexmt += (SEQ_MIN(p->rxmit, p->end) - p->start);
	}
	return (p);
}
#endif

/*
 * Returns the next hole to retransmit and the number of retransmitted bytes
 * from the scoreboard.  We store both the next hole and the number of
 * retransmitted bytes as hints (and recompute these on the fly upon SACK/ACK
 * reception).  This avoids scoreboard traversals completely.
 *
 * The loop here will traverse *at most* one link.  Here's the argument.  For
 * the loop to traverse more than 1 link before finding the next hole to
 * retransmit, we would need to have at least 1 node following the current
 * hint with (rxmit == end).  But, for all holes following the current hint,
 * (start == rxmit), since we have not yet retransmitted from them.
 * Therefore, in order to traverse more 1 link in the loop below, we need to
 * have at least one node following the current hint with (start == rxmit ==
 * end).  But that can't happen, (start == end) means that all the data in
 * that hole has been sacked, in which case, the hole would have been removed
 * from the scoreboard.
 */
struct sackhole *
tcp_sack_output(struct tcpcb *tp, int *sack_bytes_rexmt)
{
	struct sackhole *hole = NULL;

	INP_WLOCK_ASSERT(tp->t_inpcb);
	*sack_bytes_rexmt = tp->sackhint.sack_bytes_rexmit;
	hole = tp->sackhint.nexthole;
	if (hole == NULL || SEQ_LT(hole->rxmit, hole->end))
		goto out;
	while ((hole = TAILQ_NEXT(hole, scblink)) != NULL) {
		if (SEQ_LT(hole->rxmit, hole->end)) {
			tp->sackhint.nexthole = hole;
			break;
		}
	}
out:
	return (hole);
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

	INP_WLOCK_ASSERT(tp->t_inpcb);
	if (cur == NULL)
		return; /* No holes */
	if (SEQ_GEQ(tp->snd_nxt, tp->snd_fack))
		return; /* We're already beyond any SACKed blocks */
	/*-
	 * Two cases for which we want to advance snd_nxt:
	 * i) snd_nxt lies between end of one hole and beginning of another
	 * ii) snd_nxt lies between end of last hole and snd_fack
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
	tp->snd_nxt = tp->snd_fack;
}

/*
 * Lost Retransmission Detection
 * Check is FACK is beyond the rexmit of the leftmost hole.
 * If yes, we restart sending from still existing holes,
 * and adjust cwnd via the congestion control module.
 */
void
tcp_sack_lost_retransmission(struct tcpcb *tp, struct tcphdr *th)
{
	struct sackhole *temp;
	uint32_t prev_cwnd;
	if (IN_RECOVERY(tp->t_flags) &&
	    SEQ_GT(tp->snd_fack, tp->snd_recover) &&
	    ((temp = TAILQ_FIRST(&tp->snd_holes)) != NULL) &&
	    SEQ_GEQ(temp->rxmit, temp->end) &&
	    SEQ_GEQ(tp->snd_fack, temp->rxmit)) {
		TCPSTAT_INC(tcps_sack_lostrexmt);
		/*
		 * Start retransmissions from the first hole, and
		 * subsequently all other remaining holes, including
		 * those, which had been sent completely before.
		 */
		tp->sackhint.nexthole = temp;
		TAILQ_FOREACH(temp, &tp->snd_holes, scblink) {
			if (SEQ_GEQ(tp->snd_fack, temp->rxmit) &&
			    SEQ_GEQ(temp->rxmit, temp->end))
				temp->rxmit = temp->start;
		}
		/*
		 * Remember the old ssthresh, to deduct the beta factor used
		 * by the CC module. Finally, set cwnd to ssthresh just
		 * prior to invoking another cwnd reduction by the CC
		 * module, to not shrink it excessively.
		 */
		prev_cwnd = tp->snd_cwnd;
		tp->snd_cwnd = tp->snd_ssthresh;
		/*
		 * Formally exit recovery, and let the CC module adjust
		 * ssthresh as intended.
		 */
		EXIT_RECOVERY(tp->t_flags);
		cc_cong_signal(tp, th, CC_NDUPACK);
		/*
		 * For PRR, adjust recover_fs as if this new reduction
		 * initialized this variable.
		 * cwnd will be adjusted by SACK or PRR processing
		 * subsequently, only set it to a safe value here.
		 */
		tp->snd_cwnd = tcp_maxseg(tp);
		tp->sackhint.recover_fs = (tp->snd_max - tp->snd_una) -
					    tp->sackhint.recover_fs;
	}
}
