/*
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 */

/*
 * ATM Forum UNI Support
 * ---------------------
 *
 * SSCOP - Subroutines
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <net/if.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#include <netatm/uni/sscop.h>
#include <netatm/uni/sscop_misc.h>
#include <netatm/uni/sscop_pdu.h>
#include <netatm/uni/sscop_var.h>

/*
 * Local functions
 */
static int sscop_proc_xmit(struct sscop *);


/*
 * Get Next Element from STAT PDU
 *
 * Arguments:
 *	m	pointer to current buffer in STAT PDU
 *	pelem	pointer to location to store element value
 *
 * Returns:
 *	addr	pointer to updated current buffer in STAT PDU
 *
 */
KBuffer *
sscop_stat_getelem(m, pelem)
	KBuffer		*m;
	sscop_seq	*pelem;
{
	caddr_t		cp;

	/*
	 * Get to start of element
	 *
	 * Note that we always ensure that the current buffer has
	 * at least one byte of the next element.
	 */
	KB_DATASTART(m, cp, caddr_t);

	/*
	 * See how much of element is in this buffer
	 */
	if (KB_LEN(m) >= sizeof(sscop_seq)) {
		/*
		 * Get element from this buffer
		 */
		if ((intptr_t)cp & (sizeof(sscop_seq) - 1))
			bcopy(cp, (caddr_t)pelem, sizeof(sscop_seq));
		else
			*pelem = *(sscop_seq *)cp;

		/*
		 * Update buffer controls
		 */
		KB_HEADADJ(m, -sizeof(sscop_seq));
	} else {
		/*
		 * Get element split between two buffers
		 */
		int	i, j;

		/*
		 * Copy what's in this buffer
		 */
		i = KB_LEN(m);
		bcopy(cp, (caddr_t)pelem, i);
		KB_LEN(m) = 0;

		/*
		 * Now get to next buffer
		 */
		while (m && (KB_LEN(m) == 0))
			m = KB_NEXT(m);

		/*
		 * And copy remainder of element
		 */
		j = sizeof(sscop_seq) - i;
		KB_DATASTART(m, cp, caddr_t);
		bcopy(cp, (caddr_t)pelem + i, j);

		/*
		 * Update buffer controls
		 */
		KB_HEADADJ(m, -j);
	}

	/*
	 * Put element (sequence number) into host order
	 */
	*pelem = ntohl(*pelem);

	/*
	 * Get pointers set for next call
	 */
	while (m && (KB_LEN(m) == 0))
		m = KB_NEXT(m);

	return (m);
}


/*
 * Locate SD PDU on Pending Ack Queue
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	seq	sequence number of PDU to locate
 *
 * Returns:
 *	addr	pointer to located PDU header
 *	0	SD PDU sequence number not found
 *
 */
struct pdu_hdr *
sscop_pack_locate(sop, seq)
	struct sscop	*sop;
	sscop_seq	seq;
{
	struct pdu_hdr	*php;

	/*
	 * Loop thru queue until we either find the PDU or the queue's
	 * sequence numbers are greater than the PDU's sequence number,
	 * indicating that the PDU is not on the queue.
	 */
	for (php = sop->so_pack_hd; php; php = php->ph_pack_lk) {
		if (php->ph_ns == seq)
			break;

		if (SEQ_GT(php->ph_ns, seq, sop->so_ack)) {
			php = NULL;
			break;
		}
	}

	return (php);
}


/*
 * Free Acknowledged SD PDU
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	seq	sequence number of PDU to free
 *
 * Returns:
 *	none
 *
 */
void
sscop_pack_free(sop, seq)
	struct sscop	*sop;
	sscop_seq	seq;
{
	struct pdu_hdr	*php, *prev;

	/*
	 * Unlink PDU from pending ack queue
	 *
	 * First, check for an empty queue
	 */
	php = sop->so_pack_hd;
	if (php == NULL)
		return;

	/*
	 * Now check for PDU at head of queue
	 */
	if (php->ph_ns == seq) {
		if ((sop->so_pack_hd = php->ph_pack_lk) == NULL)
			sop->so_pack_tl = NULL;
		goto found;
	}

	/*
	 * Otherwise, loop thru queue until we either find the PDU or
	 * the queue's sequence numbers are greater than the PDU's
	 * sequence number, indicating that the PDU is not on the queue.
	 */
	prev = php;
	php = php->ph_pack_lk;
	while (php) {
		if (php->ph_ns == seq) {
			if ((prev->ph_pack_lk = php->ph_pack_lk) == NULL)
				sop->so_pack_tl = prev;
			goto found;
		}

		if (SEQ_GT(php->ph_ns, seq, sop->so_ack))
			return;

		prev = php;
		php = php->ph_pack_lk;
	}

	return;

found:
	/*
	 * We've got the ack'ed PDU - unlink it from retransmit queue
	 */
	sscop_rexmit_unlink(sop, php);

	/*
	 * Free PDU buffers
	 */
	KB_FREEALL(php->ph_buf);

	return;
}


/*
 * Insert SD PDU into Retransmit Queue
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	php	pointer to SD PDU header
 *
 * Returns:
 *	none
 *
 */
void
sscop_rexmit_insert(sop, php)
	struct sscop	*sop;
	struct pdu_hdr	*php;
{
	struct pdu_hdr	*curr, *next;
	sscop_seq	seq = php->ph_ns;

	/*
	 * Check for an empty queue
	 */
	if ((curr = sop->so_rexmit_hd) == NULL) {
		php->ph_rexmit_lk = NULL;
		sop->so_rexmit_hd = php;
		sop->so_rexmit_tl = php;
		return;
	}

	/*
	 * Now see if PDU belongs at head of queue
	 */
	if (SEQ_LT(seq, curr->ph_ns, sop->so_ack)) {
		php->ph_rexmit_lk = curr;
		sop->so_rexmit_hd = php;
		return;
	}

	/*
	 * Otherwise, loop thru the queue until we find the
	 * proper insertion point for the PDU
	 */
	while ((next = curr->ph_rexmit_lk) != NULL) {
		if (SEQ_LT(seq, next->ph_ns, sop->so_ack)) {
			php->ph_rexmit_lk = next;
			curr->ph_rexmit_lk = php;
			return;
		}
		curr = next;
	}

	/*
	 * Insert PDU at end of queue
	 */
	php->ph_rexmit_lk = NULL;
	curr->ph_rexmit_lk = php;
	sop->so_rexmit_tl = php;

	return;
}


/*
 * Unlink SD PDU from Retransmit Queue
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	php	pointer to PDU header to unlink
 *
 * Returns:
 *	none
 *
 */
void
sscop_rexmit_unlink(sop, php)
	struct sscop	*sop;
	struct pdu_hdr	*php;
{
	struct pdu_hdr	*curr;

	/*
	 * See if PDU is on retransmit queue
	 */
	if ((php->ph_rexmit_lk == NULL) && (sop->so_rexmit_tl != php))
		return;

	/*
	 * It's here somewhere, so first check for the PDU at the
	 * head of the queue
	 */
	if (php == sop->so_rexmit_hd) {
		if ((sop->so_rexmit_hd = php->ph_rexmit_lk) == NULL)
			sop->so_rexmit_tl = NULL;
		php->ph_rexmit_lk = NULL;
		return;
	}

	/*
	 * Otherwise, loop thru the queue until we find the PDU
	 */
	for (curr = sop->so_rexmit_hd; curr; curr = curr->ph_rexmit_lk) {
		if (curr->ph_rexmit_lk == php)
			break;
	}
	if (curr) {
		if ((curr->ph_rexmit_lk = php->ph_rexmit_lk) == NULL)
			sop->so_rexmit_tl = curr;
	} else {
		log(LOG_ERR,
			"sscop_rexmit_unlink: Not found - sop=%p, php=%p\n",
			sop, php);
#ifdef DIAGNOSTIC
		panic("sscop_rexmit_unlink: Not found");
#endif
	}
	php->ph_rexmit_lk = NULL;

	return;
}


/*
 * Drain Transmission Queues
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *
 * Returns:
 *	none
 *
 */
void
sscop_xmit_drain(sop)
	struct sscop	*sop;
{
	KBuffer		*m;
	struct pdu_hdr	*php;

	/*
	 * Free transmission queue buffers
	 */
	while ((m = sop->so_xmit_hd) != NULL) {
		sop->so_xmit_hd = KB_QNEXT(m);
		KB_FREEALL(m);
	}
	sop->so_xmit_tl = NULL;

	/*
	 * Free retransmission queue
	 *
	 * All retranmission buffers are also on the pending ack
	 * queue (but not the converse), so we just clear the queue
	 * pointers here and do all the real work below.
	 */
	sop->so_rexmit_hd = NULL;
	sop->so_rexmit_tl = NULL;

	/*
	 * Free pending ack queue buffers
	 */
	while ((php = sop->so_pack_hd) != NULL) {
		sop->so_pack_hd = php->ph_pack_lk;
		KB_FREEALL(php->ph_buf);
	}
	sop->so_pack_tl = NULL;

	/*
	 * Clear service required flag
	 */
	sop->so_flags &= ~SOF_XMITSRVC;

	return;
}


/*
 * Insert SD PDU into Receive Queue
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	php	pointer to SD PDU header
 *
 * Returns:
 *	0	PDU successfully inserted into queue
 *	1	duplicate sequence number PDU on queue, PDU not inserted
 *
 */
int
sscop_recv_insert(sop, php)
	struct sscop	*sop;
	struct pdu_hdr	*php;
{
	struct pdu_hdr	*curr, *next;
	sscop_seq	seq = php->ph_ns;

	/*
	 * Check for an empty queue
	 */
	if ((curr = sop->so_recv_hd) == NULL) {
		php->ph_recv_lk = NULL;
		sop->so_recv_hd = php;
		sop->so_recv_tl = php;
		return (0);
	}

	/*
	 * Now see if PDU belongs at head of queue
	 */
	if (SEQ_LT(seq, curr->ph_ns, sop->so_rcvnext)) {
		php->ph_recv_lk = curr;
		sop->so_recv_hd = php;
		return (0);
	}

	/*
	 * Otherwise, loop thru the queue until we find the
	 * proper insertion point for the PDU.  We also check
	 * to make sure there isn't a PDU already on the queue
	 * with a matching sequence number.
	 */
	while ((next = curr->ph_recv_lk) != NULL) {
		if (SEQ_LT(seq, next->ph_ns, sop->so_rcvnext)) {
			if (seq == curr->ph_ns)
				return (1);
			php->ph_recv_lk = next;
			curr->ph_recv_lk = php;
			return (0);
		}
		curr = next;
	}

	/*
	 * Insert PDU at end of queue
	 */
	if (seq == curr->ph_ns)
		return (1);
	php->ph_recv_lk = NULL;
	curr->ph_recv_lk = php;
	sop->so_recv_tl = php;

	return (0);
}


/*
 * Drain Receiver Queues
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *
 * Returns:
 *	none
 *
 */
void
sscop_rcvr_drain(sop)
	struct sscop	*sop;
{
	struct pdu_hdr	*php;

	/*
	 * Free receive queue buffers
	 */
	while ((php = sop->so_recv_hd) != NULL) {
		sop->so_recv_hd = php->ph_recv_lk;
		KB_FREEALL(php->ph_buf);
	}
	sop->so_recv_tl = NULL;

	return;
}


/*
 * Service connection's transmit queues
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *
 * Returns:
 *	none
 *
 */
void
sscop_service_xmit(sop)
	struct sscop	*sop;
{
	KBuffer		*m, *n;
	struct pdu_hdr	*php;
	int		err = 0, pollsent = 0;

	/*
	 * Initially assume we need service
	 */
	sop->so_flags |= SOF_XMITSRVC;

	/*
	 * Loop until done with queues
	 *
	 * (Congestion control will be added later)
	 */
	while (1) {
		if ((php = sop->so_rexmit_hd) != NULL) {

			/*
			 * Send SD PDU from retransmit queue
			 *
			 * First, get a copy of the PDU to send
			 */
			m = php->ph_buf;
			if (KB_LEN(m) == 0)
				m = KB_NEXT(m);
			KB_COPY(m, 0, KB_COPYALL, n, KB_F_NOWAIT);
			if (n == NULL) {
				err = 1;
				break;
			}

			/*
			 * Now pass it down the stack
			 */
			STACK_CALL(CPCS_UNITDATA_INV, sop->so_lower,
				sop->so_tokl, sop->so_connvc, (intptr_t)n, 0,
				err);
			if (err) {
				KB_FREEALL(n);
				break;
			}

			/*
			 * PDU is on its way, so remove it from
			 * the retransmit queue
			 */
			if (sop->so_rexmit_tl == php) {
				sop->so_rexmit_hd = NULL;
				sop->so_rexmit_tl = NULL;
			} else {
				sop->so_rexmit_hd = php->ph_rexmit_lk;
			}
			php->ph_rexmit_lk = NULL;

			/*
			 * Update PDU's poll sequence
			 */
			php->ph_nps = sop->so_pollsend;

		} else if (sop->so_xmit_hd) {

			/*
			 * Newly arrived data waiting to be sent.
			 * See if transmit window allows us to send it.
			 */
			if (SEQ_LT(sop->so_send, sop->so_sendmax, sop->so_ack)){
				/*
				 * OK, send SD PDU from transmission queue
				 */
				err = sscop_proc_xmit(sop);
				if (err)
					break;
			} else {
				/*
				 * Can't send now, so leave idle phase.
				 */
				if (sop->so_timer[SSCOP_T_IDLE] != 0) {
					sop->so_timer[SSCOP_T_IDLE] = 0;
					sop->so_timer[SSCOP_T_NORESP] =
						sop->so_parm.sp_timeresp;
					err = 1;
				}
				break;
			}

		} else {

			/*
			 * We're finished, so clear service required flag
			 */
			sop->so_flags &= ~SOF_XMITSRVC;
			break;
		}

		/*
		 * We've sent another SD PDU
		 */
		sop->so_polldata++;

		/*
		 * Transition into active (polling) phase
		 */
		if (sop->so_timer[SSCOP_T_POLL] != 0) {
			if (sop->so_flags & SOF_KEEPALIVE) {
				/*
				 * Leaving transient phase
				 */
				sop->so_flags &= ~SOF_KEEPALIVE;
				sop->so_timer[SSCOP_T_POLL] =
						sop->so_parm.sp_timepoll;
			}
		} else {
			/*
			 * Leaving idle phase
			 */
			sop->so_timer[SSCOP_T_IDLE] = 0;
			sop->so_timer[SSCOP_T_NORESP] =
						sop->so_parm.sp_timeresp;
			sop->so_timer[SSCOP_T_POLL] = sop->so_parm.sp_timepoll;
		}

		/*
		 * Let's see if we need to send a POLL yet
		 */
		if (sop->so_polldata < sop->so_parm.sp_maxpd)
			continue;

		/*
		 * Yup, send another poll out
		 */
		SEQ_INCR(sop->so_pollsend, 1);
		(void) sscop_send_poll(sop);
		pollsent++;

		/*
		 * Reset data counter for this poll cycle
		 */
		sop->so_polldata = 0;

		/*
		 * Restart polling timer in active phase
		 */
		sop->so_timer[SSCOP_T_POLL] = sop->so_parm.sp_timepoll;
	}

	/*
	 * If we need/want to send a poll, but haven't sent any yet
	 * on this servicing, send one now
	 */
	if (err && (pollsent == 0)) {
		/*
		 * Send poll
		 */
		SEQ_INCR(sop->so_pollsend, 1);
		(void) sscop_send_poll(sop);

		/*
		 * Reset data counter for this poll cycle
		 */
		sop->so_polldata = 0;

		/*
		 * Restart polling timer in active phase
		 */
		sop->so_timer[SSCOP_T_POLL] = sop->so_parm.sp_timepoll;
		sop->so_flags &= ~SOF_KEEPALIVE;
	}

	return;
}


/*
 * Process Transmission Queue PDU
 *
 * For the first entry on the transmission queue: add a PDU header and
 * trailer, send a copy of the PDU down the stack and move the PDU from
 * the transmission queue to the pending ack queue.
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *
 * Returns:
 *	0	head of transmission queue successfully processed
 *	else	processing error, tranmission queue unchanged
 *
 */
static int
sscop_proc_xmit(sop)
	struct sscop	*sop;
{
	KBuffer		*m, *ml, *n;
	struct pdu_hdr	*php;
	sscop_seq	seq;
	int		len = 0, err;
	int		pad, trlen, space;
	u_char		*cp;

	/*
	 * Get first buffer chain on queue
	 */
	if ((m = sop->so_xmit_hd) == NULL)
		return (0);

	/*
	 * Count data and get to last buffer in chain
	 */
	for (ml = m; ; ml = KB_NEXT(ml)) {
		len += KB_LEN(ml);
		if (KB_NEXT(ml) == NULL)
			break;
	}

	/*
	 * Verify data length
	 */
	if (len > sop->so_parm.sp_maxinfo) {
		sscop_abort(sop, "sscop: maximum data size exceeded\n");
		return (1);
	}

	/*
	 * Get space for PDU header
	 */
	KB_HEADROOM(m, space);
	if (space < sizeof(struct pdu_hdr)) {
		/*
		 * Allocate & link buffer for header
		 */
		KB_ALLOC(n, sizeof(struct pdu_hdr), KB_F_NOWAIT, KB_T_HEADER);
		if (n == NULL)
			return (1);

		KB_LEN(n) = 0;
		KB_HEADSET(n, sizeof(struct pdu_hdr));
		KB_LINKHEAD(n, m);
		KB_QNEXT(n) = KB_QNEXT(m);
		KB_QNEXT(m) = NULL;
		sop->so_xmit_hd = n;
		if (sop->so_xmit_tl == m)
			sop->so_xmit_tl = n;
		m = n;
	}

	/*
	 * Figure out how much padding we'll need
	 */
	pad = ((len + (PDU_PAD_ALIGN - 1)) & ~(PDU_PAD_ALIGN - 1)) - len;
	trlen = pad + sizeof(struct sd_pdu);

	/*
	 * Now get space for PDU trailer and padding
	 */
	KB_TAILROOM(ml, space);
	if (space < trlen) {
		/*
		 * Allocate & link buffer for pad and trailer
		 */
		KB_ALLOC(n, trlen, KB_F_NOWAIT, KB_T_HEADER);
		if (n == NULL)
			return (1);

		KB_LEN(n) = 0;
		KB_LINK(n, ml);
		ml = n;
	}

	/*
	 * Build the PDU trailer
	 *
	 * Since we can't be sure of alignment in the buffers, we
	 * have to move this a byte at a time and we have to be
	 * careful with host byte order issues.
	 */
	KB_DATASTART(ml, cp, u_char *);
	cp += KB_LEN(ml) + pad;
	*cp++ = (pad << PT_PAD_SHIFT) | PT_SD;
	seq = sop->so_send;
	*(cp + 2) = (u_char)(seq & 0xff);
	seq >>= 8;
	*(cp + 1) = (u_char)(seq & 0xff);
	seq >>= 8;
	*(cp) = (u_char)(seq & 0xff);
	KB_LEN(ml) += trlen;

	/*
	 * Get a copy of the SD PDU to send
	 */
	if (KB_LEN(m) == 0)
		n = KB_NEXT(m);
	else
		n = m;
	KB_COPY(n, 0, KB_COPYALL, n, KB_F_NOWAIT);
	if (n == NULL) {
		KB_LEN(ml) -= trlen;
		return (1);
	}

	/*
	 * Now pass copy down the stack
	 */
	STACK_CALL(CPCS_UNITDATA_INV, sop->so_lower, sop->so_tokl,
		sop->so_connvc, (intptr_t)n, 0, err);
	if (err) {
		KB_FREEALL(n);
		KB_LEN(ml) -= trlen;
		return (1);
	}

	/*
	 * PDU is on its way, so remove buffer from
	 * the transmission queue
	 */
	if (sop->so_xmit_tl == m) {
		sop->so_xmit_hd = NULL;
		sop->so_xmit_tl = NULL;
	} else {
		sop->so_xmit_hd = KB_QNEXT(m);
	}
	KB_QNEXT(m) = NULL;

	/*
	 * Build PDU header
	 *
	 * We can at least assume/require that the start of
	 * the user data is aligned.  Also note that we don't
	 * include this header in the buffer len/offset fields.
	 */
	KB_DATASTART(m, php, struct pdu_hdr *);
	php--;
	php->ph_ns = sop->so_send;
	php->ph_nps = sop->so_pollsend;
	php->ph_buf = m;
	php->ph_rexmit_lk = NULL;
	php->ph_pack_lk = NULL;

	/*
	 * Put PDU onto the pending ack queue
	 */
	if (sop->so_pack_hd == NULL)
		sop->so_pack_hd = php;
	else
		sop->so_pack_tl->ph_pack_lk = php;
	sop->so_pack_tl = php;

	/*
	 * Finally, bump send sequence number
	 */
	SEQ_INCR(sop->so_send, 1);

	return (0);
}


/*
 * Detect Retransmitted PDUs
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	nsq	connection sequence value (N(SQ)) from received PDU
 *
 * Returns:
 *	0	received PDU was NOT retransmitted
 *	1	received PDU was retransmitted
 *
 */
int
sscop_is_rexmit(sop, nsq)
	struct sscop	*sop;
	u_char		nsq;
{

	/*
	 * For Q.SAAL1, N(SQ) doesn't exist
	 */
	if (sop->so_vers == SSCOP_VERS_QSAAL)
		return (0);

	/*
	 * If we've already received the N(SQ) value,
	 * then this PDU has been retransmitted
	 */
	if (nsq == sop->so_rcvconn)
		return (1);

	/*
	 * New PDU, save its N(SQ)
	 */
	sop->so_rcvconn = nsq;

	return (0);
}


/*
 * Start connection poll timer
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *
 * Returns:
 *	none
 *
 */
void
sscop_set_poll(sop)
	struct sscop	*sop;
{

	/*
	 * Decide which polling timer value to set
	 */
	if ((sop->so_xmit_hd != NULL) || SEQ_NEQ(sop->so_send, sop->so_ack)) {
		/*
		 * Data outstanding, poll frequently
		 */
		sop->so_timer[SSCOP_T_POLL] = sop->so_parm.sp_timepoll;
		sop->so_flags &= ~SOF_KEEPALIVE;
	} else {
		/*
		 * No data outstanding, just poll occassionally
		 */
		sop->so_timer[SSCOP_T_POLL] = sop->so_parm.sp_timekeep;
		sop->so_flags |= SOF_KEEPALIVE;
	}

	return;
}

