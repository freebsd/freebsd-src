/*
 *
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
 *
 *	@(#) $FreeBSD$
 *
 */

/*
 * ATM Forum UNI Support
 * ---------------------
 *
 * SSCOP - PDU subroutines
 *
 */

#include <netatm/kern_include.h>

#include <netatm/uni/sscop.h>
#include <netatm/uni/sscop_misc.h>
#include <netatm/uni/sscop_pdu.h>
#include <netatm/uni/sscop_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif

/*
 * Local functions
 */
static KBuffer *	sscop_stat_init __P((struct sscop *));
static KBuffer *	sscop_stat_add __P((sscop_seq, KBuffer *));
static int		sscop_stat_end __P((struct sscop *, sscop_seq,
				KBuffer *, KBuffer *));
static int		sscop_recv_locate __P((struct sscop *, sscop_seq,
				struct pdu_hdr **));


/*
 * Build and send BGN PDU
 * 
 * A BGN PDU will be constructed and passed down the protocol stack.
 * The SSCOP-UU/N(UU) field is not supported.
 *
 * Arguments:
 *	sop	pointer to sscop connection control block
 *	source	originator of BGN PDU (Q.SAAL1 only)
 *
 * Returns:
 *	0	PDU successfully built and passed down the stack
 *	else	unable to build or send pdu
 *
 */
int
sscop_send_bgn(sop, source)
	struct sscop	*sop;
	int		source;
{
	KBuffer		*m;
	struct bgn_pdu	*bp;
	int		err;


	/*
	 * Get buffer for PDU
	 */
	KB_ALLOCPKT(m, sizeof(struct bgn_pdu), KB_F_NOWAIT, KB_T_HEADER);
	if (m == NULL)
		return (1);

	/*
	 * Setup buffer controls
	 */
	KB_HEADSET(m, MIN(sop->so_headout,
		KB_BFRLEN(m) - sizeof(struct bgn_pdu)));
	KB_LEN(m) = sizeof(struct bgn_pdu);

	/*
	 * Build PDU
	 */
	KB_DATASTART(m, bp, struct bgn_pdu *);
	*(int *)&bp->bgn_rsvd[0] = 0;
	if (sop->so_vers != SSCOP_VERS_QSAAL)
		bp->bgn_nsq = sop->so_sendconn;
	bp->bgn_nmr =
		htonl((PT_BGN << PT_TYPE_SHIFT) | SEQ_VAL(sop->so_rcvmax));
	if ((sop->so_vers == SSCOP_VERS_QSAAL) &&
	    (source == SSCOP_SOURCE_SSCOP))
		bp->bgn_type |= PT_SOURCE_SSCOP;

	/*
	 * Send PDU towards peer
	 */
	STACK_CALL(CPCS_UNITDATA_INV, sop->so_lower, sop->so_tokl,
		sop->so_connvc, (int)m, 0, err);

	if (err)
		KB_FREEALL(m);

	return (err);
}


/*
 * Build and send BGAK PDU
 * 
 * A BGAK PDU will be constructed and passed down the protocol stack.
 * The SSCOP-UU/N(UU) field is not supported.
 *
 * Arguments:
 *	sop	pointer to sscop connection control block
 *
 * Returns:
 *	0	PDU successfully built and passed down the stack
 *	else	unable to build or send pdu
 *
 */
int
sscop_send_bgak(sop)
	struct sscop	*sop;
{
	KBuffer		*m;
	struct bgak_pdu	*bp;
	int		err;


	/*
	 * Get buffer for PDU
	 */
	KB_ALLOCPKT(m, sizeof(struct bgak_pdu), KB_F_NOWAIT, KB_T_HEADER);
	if (m == NULL)
		return (1);

	/*
	 * Setup buffer controls
	 */
	KB_HEADSET(m, MIN(sop->so_headout,
		KB_BFRLEN(m) - sizeof(struct bgak_pdu)));
	KB_LEN(m) = sizeof(struct bgak_pdu);

	/*
	 * Build PDU
	 */
	KB_DATASTART(m, bp, struct bgak_pdu *);
	bp->bgak_rsvd = 0;
	bp->bgak_nmr =
		htonl((PT_BGAK << PT_TYPE_SHIFT) | SEQ_VAL(sop->so_rcvmax));

	/*
	 * Send PDU towards peer
	 */
	STACK_CALL(CPCS_UNITDATA_INV, sop->so_lower, sop->so_tokl,
		sop->so_connvc, (int)m, 0, err);

	if (err)
		KB_FREEALL(m);

	return (err);
}


/*
 * Build and send BGREJ PDU
 * 
 * A BGREJ PDU will be constructed and passed down the protocol stack.
 * The SSCOP-UU/N(UU) field is not supported.
 *
 * Arguments:
 *	sop	pointer to sscop connection control block
 *
 * Returns:
 *	0	PDU successfully built and passed down the stack
 *	else	unable to build or send pdu
 *
 */
int
sscop_send_bgrej(sop)
	struct sscop	*sop;
{
	KBuffer		*m;
	struct bgrej_pdu	*bp;
	int		err;


	/*
	 * Get buffer for PDU
	 */
	KB_ALLOCPKT(m, sizeof(struct bgrej_pdu), KB_F_NOWAIT, KB_T_HEADER);
	if (m == NULL)
		return (1);

	/*
	 * Setup buffer controls
	 */
	KB_HEADSET(m, MIN(sop->so_headout,
		KB_BFRLEN(m) - sizeof(struct bgrej_pdu)));
	KB_LEN(m) = sizeof(struct bgrej_pdu);

	/*
	 * Build PDU
	 */
	KB_DATASTART(m, bp, struct bgrej_pdu *);
	bp->bgrej_rsvd2 = 0;
	*(u_int *)&bp->bgrej_type = htonl((PT_BGREJ << PT_TYPE_SHIFT) | 0);

	/*
	 * Send PDU towards peer
	 */
	STACK_CALL(CPCS_UNITDATA_INV, sop->so_lower, sop->so_tokl,
		sop->so_connvc, (int)m, 0, err);

	if (err)
		KB_FREEALL(m);

	return (err);
}


/*
 * Build and send END PDU
 * 
 * An END PDU will be constructed and passed down the protocol stack.
 * The SSCOP-UU/N(UU) field is not supported.
 *
 * Arguments:
 *	sop	pointer to sscop connection control block
 *	source	originator of END PDU
 *
 * Returns:
 *	0	PDU successfully built and passed down the stack
 *	else	unable to build or send pdu
 *
 */
int
sscop_send_end(sop, source)
	struct sscop	*sop;
	int		source;
{
	KBuffer		*m;
	struct end_pdu	*ep;
	int		err;


	/*
	 * Get buffer for PDU
	 */
	KB_ALLOCPKT(m, sizeof(struct end_pdu), KB_F_NOWAIT, KB_T_HEADER);
	if (m == NULL)
		return (1);

	/*
	 * Setup buffer controls
	 */
	KB_HEADSET(m, MIN(sop->so_headout,
		KB_BFRLEN(m) - sizeof(struct end_pdu)));
	KB_LEN(m) = sizeof(struct end_pdu);

	/*
	 * Build PDU
	 */
	KB_DATASTART(m, ep, struct end_pdu *);
	ep->end_rsvd2 = 0;
	*(u_int *)&ep->end_type = htonl((PT_END << PT_TYPE_SHIFT) | 0);
	if (source == SSCOP_SOURCE_SSCOP) {
		ep->end_type |= PT_SOURCE_SSCOP;
		sop->so_flags |= SOF_ENDSSCOP;
	} else if (source == SSCOP_SOURCE_USER)
		sop->so_flags &= ~SOF_ENDSSCOP;
	else if ((source == SSCOP_SOURCE_LAST) &&
		 (sop->so_flags & SOF_ENDSSCOP))
		ep->end_type |= PT_SOURCE_SSCOP;

	/*
	 * Send PDU towards peer
	 */
	STACK_CALL(CPCS_UNITDATA_INV, sop->so_lower, sop->so_tokl,
		sop->so_connvc, (int)m, 0, err);

	if (err)
		KB_FREEALL(m);

	return (err);
}


/*
 * Build and send ENDAK PDU
 * 
 * An ENDAK PDU will be constructed and passed down the protocol stack.
 *
 * Arguments:
 *	sop	pointer to sscop connection control block
 *
 * Returns:
 *	0	PDU successfully built and passed down the stack
 *	else	unable to build or send pdu
 *
 */
int
sscop_send_endak(sop)
	struct sscop	*sop;
{
	KBuffer		*m;
	struct endak_q2110_pdu	*e2p;
	struct endak_qsaal_pdu	*esp;
	int		err, size;


	/*
	 * Get size of PDU
	 */
	if (sop->so_vers == SSCOP_VERS_QSAAL)
		size = sizeof(struct endak_qsaal_pdu);
	else
		size = sizeof(struct endak_q2110_pdu);

	/*
	 * Get buffer for PDU
	 */
	KB_ALLOCPKT(m, size, KB_F_NOWAIT, KB_T_HEADER);
	if (m == NULL)
		return (1);

	/*
	 * Setup buffer controls
	 */
	KB_HEADSET(m, MIN(sop->so_headout, KB_BFRLEN(m) - size));
	KB_LEN(m) = size;

	/*
	 * Build PDU
	 */
	if (sop->so_vers == SSCOP_VERS_QSAAL) {
		KB_DATASTART(m, esp, struct endak_qsaal_pdu *);
		*(u_int *)&esp->endak_type =
			htonl((PT_ENDAK << PT_TYPE_SHIFT) | 0);
	} else {
		KB_DATASTART(m, e2p, struct endak_q2110_pdu *);
		e2p->endak_rsvd2 = 0;
		*(u_int *)&e2p->endak_type =
			htonl((PT_ENDAK << PT_TYPE_SHIFT) | 0);
	}

	/*
	 * Send PDU towards peer
	 */
	STACK_CALL(CPCS_UNITDATA_INV, sop->so_lower, sop->so_tokl,
		sop->so_connvc, (int)m, 0, err);

	if (err)
		KB_FREEALL(m);

	return (err);
}


/*
 * Build and send RS PDU
 * 
 * A RS PDU will be constructed and passed down the protocol stack.
 * The SSCOP-UU/N(UU) field is not supported.
 *
 * Arguments:
 *	sop	pointer to sscop connection control block
 *
 * Returns:
 *	0	PDU successfully built and passed down the stack
 *	else	unable to build or send pdu
 *
 */
int
sscop_send_rs(sop)
	struct sscop	*sop;
{
	KBuffer		*m;
	struct rs_pdu	*rp;
	int		err;


	/*
	 * Get buffer for PDU
	 */
	KB_ALLOCPKT(m, sizeof(struct rs_pdu), KB_F_NOWAIT, KB_T_HEADER);
	if (m == NULL)
		return (1);

	/*
	 * Setup buffer controls
	 */
	KB_HEADSET(m, MIN(sop->so_headout,
		KB_BFRLEN(m) - sizeof(struct rs_pdu)));
	KB_LEN(m) = sizeof(struct rs_pdu);

	/*
	 * Build PDU
	 */
	KB_DATASTART(m, rp, struct rs_pdu *);
	*(int *)&rp->rs_rsvd[0] = 0;
	if (sop->so_vers != SSCOP_VERS_QSAAL) {
		rp->rs_nsq = sop->so_sendconn;
		rp->rs_nmr = htonl((PT_RS << PT_TYPE_SHIFT) |
				SEQ_VAL(sop->so_rcvmax));
	} else {
		rp->rs_nmr = htonl((PT_RS << PT_TYPE_SHIFT) | 0);
	}

	/*
	 * Send PDU towards peer
	 */
	STACK_CALL(CPCS_UNITDATA_INV, sop->so_lower, sop->so_tokl,
		sop->so_connvc, (int)m, 0, err);

	if (err)
		KB_FREEALL(m);

	return (err);
}


/*
 * Build and send RSAK PDU
 * 
 * An RSAK PDU will be constructed and passed down the protocol stack.
 *
 * Arguments:
 *	sop	pointer to sscop connection control block
 *
 * Returns:
 *	0	PDU successfully built and passed down the stack
 *	else	unable to build or send pdu
 *
 */
int
sscop_send_rsak(sop)
	struct sscop	*sop;
{
	KBuffer		*m;
	struct rsak_q2110_pdu	*r2p;
	struct rsak_qsaal_pdu	*rsp;
	int		err, size;


	/*
	 * Get size of PDU
	 */
	if (sop->so_vers == SSCOP_VERS_QSAAL)
		size = sizeof(struct rsak_qsaal_pdu);
	else
		size = sizeof(struct rsak_q2110_pdu);

	/*
	 * Get buffer for PDU
	 */
	KB_ALLOCPKT(m, size, KB_F_NOWAIT, KB_T_HEADER);
	if (m == NULL)
		return (1);

	/*
	 * Setup buffer controls
	 */
	KB_HEADSET(m, MIN(sop->so_headout, KB_BFRLEN(m) - size));
	KB_LEN(m) = size;

	/*
	 * Build PDU
	 */
	if (sop->so_vers == SSCOP_VERS_QSAAL) {
		KB_DATASTART(m, rsp, struct rsak_qsaal_pdu *);
		*(u_int *)&rsp->rsaks_type =
			htonl((PT_RSAK << PT_TYPE_SHIFT) | 0);
	} else {
		KB_DATASTART(m, r2p, struct rsak_q2110_pdu *);
		r2p->rsak_rsvd = 0;
		r2p->rsak_nmr = htonl((PT_RSAK << PT_TYPE_SHIFT) |
					SEQ_VAL(sop->so_rcvmax));
	}

	/*
	 * Send PDU towards peer
	 */
	STACK_CALL(CPCS_UNITDATA_INV, sop->so_lower, sop->so_tokl,
		sop->so_connvc, (int)m, 0, err);

	if (err)
		KB_FREEALL(m);

	return (err);
}


/*
 * Build and send ER PDU
 * 
 * An ER PDU will be constructed and passed down the protocol stack.
 *
 * Arguments:
 *	sop	pointer to sscop connection control block
 *
 * Returns:
 *	0	PDU successfully built and passed down the stack
 *	else	unable to build or send pdu
 *
 */
int
sscop_send_er(sop)
	struct sscop	*sop;
{
	KBuffer		*m;
	struct er_pdu	*ep;
	int		err;


	/*
	 * Get buffer for PDU
	 */
	KB_ALLOCPKT(m, sizeof(struct er_pdu), KB_F_NOWAIT, KB_T_HEADER);
	if (m == NULL)
		return (1);

	/*
	 * Setup buffer controls
	 */
	KB_HEADSET(m, MIN(sop->so_headout,
		KB_BFRLEN(m) - sizeof(struct er_pdu)));
	KB_LEN(m) = sizeof(struct er_pdu);

	/*
	 * Build PDU
	 */
	KB_DATASTART(m, ep, struct er_pdu *);
	*(int *)&ep->er_rsvd[0] = 0;
	ep->er_nsq = sop->so_sendconn;
	ep->er_nmr = htonl((PT_ER << PT_TYPE_SHIFT) | SEQ_VAL(sop->so_rcvmax));

	/*
	 * Send PDU towards peer
	 */
	STACK_CALL(CPCS_UNITDATA_INV, sop->so_lower, sop->so_tokl,
		sop->so_connvc, (int)m, 0, err);

	if (err)
		KB_FREEALL(m);

	return (err);
}


/*
 * Build and send ERAK PDU
 * 
 * An ERAK PDU will be constructed and passed down the protocol stack.
 *
 * Arguments:
 *	sop	pointer to sscop connection control block
 *
 * Returns:
 *	0	PDU successfully built and passed down the stack
 *	else	unable to build or send pdu
 *
 */
int
sscop_send_erak(sop)
	struct sscop	*sop;
{
	KBuffer		*m;
	struct erak_pdu	*ep;
	int		err;


	/*
	 * Get buffer for PDU
	 */
	KB_ALLOCPKT(m, sizeof(struct erak_pdu), KB_F_NOWAIT, KB_T_HEADER);
	if (m == NULL)
		return (1);

	/*
	 * Setup buffer controls
	 */
	KB_HEADSET(m, MIN(sop->so_headout,
		KB_BFRLEN(m) - sizeof(struct erak_pdu)));
	KB_LEN(m) = sizeof(struct erak_pdu);

	/*
	 * Build PDU
	 */
	KB_DATASTART(m, ep, struct erak_pdu *);
	ep->erak_rsvd = 0;
	ep->erak_nmr = htonl((PT_ERAK << PT_TYPE_SHIFT) |
				SEQ_VAL(sop->so_rcvmax));

	/*
	 * Send PDU towards peer
	 */
	STACK_CALL(CPCS_UNITDATA_INV, sop->so_lower, sop->so_tokl,
		sop->so_connvc, (int)m, 0, err);

	if (err)
		KB_FREEALL(m);

	return (err);
}


/*
 * Build and send POLL PDU
 * 
 * A POLL PDU will be constructed and passed down the protocol stack.
 *
 * Arguments:
 *	sop	pointer to sscop connection control block
 *
 * Returns:
 *	0	PDU successfully built and passed down the stack
 *	else	unable to build or send pdu
 *
 */
int
sscop_send_poll(sop)
	struct sscop	*sop;
{
	KBuffer		*m;
	struct poll_pdu	*pp;
	int		err;


	/*
	 * Get buffer for PDU
	 */
	KB_ALLOCPKT(m, sizeof(struct poll_pdu), KB_F_NOWAIT, KB_T_HEADER);
	if (m == NULL)
		return (1);

	/*
	 * Setup buffer controls
	 */
	KB_HEADSET(m, MIN(sop->so_headout,
		KB_BFRLEN(m) - sizeof(struct poll_pdu)));
	KB_LEN(m) = sizeof(struct poll_pdu);

	/*
	 * Build PDU
	 */
	KB_DATASTART(m, pp, struct poll_pdu *);
	pp->poll_nps = htonl(SEQ_VAL(sop->so_pollsend));
	pp->poll_ns = htonl((PT_POLL << PT_TYPE_SHIFT) | SEQ_VAL(sop->so_send));

	/*
	 * Send PDU towards peer
	 */
	STACK_CALL(CPCS_UNITDATA_INV, sop->so_lower, sop->so_tokl,
		sop->so_connvc, (int)m, 0, err);

	if (err)
		KB_FREEALL(m);

	return (err);
}


/*
 * STAT PDU Construction - Initialize for new PDU
 * 
 * Arguments:
 *	sop	pointer to sscop connection control block
 *
 * Returns:
 *	addr	pointer to initialized buffer
 *	0	unable to allocate buffer
 *
 */
static KBuffer *
sscop_stat_init(sop)
	struct sscop	*sop;
{
	KBuffer		*m;

#define	STAT_INIT_SIZE	(sizeof(struct stat_pdu) + 2 * sizeof(sscop_seq))

	/*
	 * Get buffer for PDU
	 */
	KB_ALLOCPKT(m, STAT_INIT_SIZE, KB_F_NOWAIT, KB_T_HEADER);
	if (m == NULL)
		return (0);

	/*
	 * Setup buffer controls
	 */
	KB_HEADSET(m, sop->so_headout < (KB_BFRLEN(m) - STAT_INIT_SIZE) ?
		sop->so_headout : 0);
	KB_LEN(m) = 0;

	return (m);
#undef	STAT_INIT_SIZE
}


/*
 * STAT PDU Construction - Add List Element
 * 
 * Arguments:
 *	elem	sequence number to add to list
 *	m	pointer to current buffer
 *
 * Returns:
 *	addr	pointer to current buffer (updated)
 *	0	buffer allocation failure
 *
 */
static KBuffer *
sscop_stat_add(elem, m)
	sscop_seq	elem;
	KBuffer		*m;
{
	KBuffer		*n;
	sscop_seq	*sp;
	int		space;

	/*
	 * See if new element will fit in current buffer
	 */
	KB_TAILROOM(m, space);
	if (space < sizeof(elem)) {

		/*
		 * Nope, so get another buffer
		 */
		KB_ALLOC(n, sizeof(elem), KB_F_NOWAIT, KB_T_DATA);
		if (n == NULL)
			return (0);

		/*
		 * Link in new buffer
		 */
		KB_LINK(n, m);
		KB_LEN(n) = 0;
		m = n;
	}

	/*
	 * Add new element
	 */
	KB_DATAEND(m, sp, sscop_seq *);
	*sp = htonl(elem);
	KB_LEN(m) += sizeof (elem);
	return (m);
}


/*
 * STAT PDU Construction - Add Trailer and Send
 * 
 * Arguments:
 *	sop	pointer to sscop connection control block
 *	nps	received poll sequence number (POLL.N(PS))
 *	head	pointer to head of buffer chain
 *	m	pointer to current (last) buffer
 *
 * Returns:
 *	0	STAT successfully sent
 *	else	unable to send STAT or truncated STAT was sent - buffer freed
 *
 */
static int
sscop_stat_end(sop, nps, head, m)
	struct sscop	*sop;
	sscop_seq	nps;
	KBuffer		*head;
	KBuffer		*m;
{
	struct stat_pdu	*sp;
	KBuffer		*n;
	int		err, space, trunc = 0;

	/*
	 * See if PDU trailer will fit in current buffer
	 */
	KB_TAILROOM(m, space);
	if (space < sizeof(struct stat_pdu)) {

		/*
		 * Doesn't fit, so get another buffer
		 */
		KB_ALLOC(n, sizeof(struct stat_pdu), KB_F_NOWAIT, KB_T_DATA);
		if (n == NULL) {
			/*
			 * Out of buffers - truncate elements and send
			 * what we can, but tell caller that we can't
			 * send any more segments.
			 */
			trunc = 1;
			do {
				KB_LEN(m) -= sizeof(sscop_seq);
				space += sizeof(sscop_seq);
			} while (space < sizeof(struct stat_pdu));
		} else {
			/*
			 * Link in new buffer
			 */
			KB_LINK(n, m);
			KB_LEN(n) = 0;
			m = n;
		}
	}

	/*
	 * Build PDU trailer
	 */
	KB_DATAEND(m, sp, struct stat_pdu *);
	sp->stat_nps = htonl(nps);
	sp->stat_nmr = htonl(sop->so_rcvmax);
	sp->stat_nr = htonl(sop->so_rcvnext);
	sp->stat_type = PT_STAT;
	KB_LEN(m) += sizeof(struct stat_pdu);

	/*
	 * Finally, send the STAT
	 */
	STACK_CALL(CPCS_UNITDATA_INV, sop->so_lower, sop->so_tokl,
		sop->so_connvc, (int)head, 0, err);

	if (err) {
		/*
		 * We lie about the STACK_CALL failing...
		 */
		KB_FREEALL(head);
	}

	if (trunc)
		return (1);
	else
		return (0);
}


/*
 * Check for PDU in Receive Queue
 * 
 * A receive queue will be searched for an SD PDU matching the requested
 * sequence number.  The caller must supply a pointer to the address of the
 * PDU in the particular receive queue at which to begin the search.  This
 * function will update that pointer as it traverses the queue.
 *
 * Arguments:
 *	sop	pointer to sscop connection control block
 *	seq	sequence number of PDU to locate
 *	currp	address of pointer to PDU in receive queue to start search
 *
 * Returns:
 *	0	reqeusted PDU not in receive queue
 *	1	requested PDU located in receive queue
 *
 */
static int
sscop_recv_locate(sop, seq, currp)
	struct sscop	*sop;
	sscop_seq	seq;
	struct pdu_hdr	**currp;
{
	sscop_seq	cs;

	/*
	 * Search queue until we know the answer
	 */
	while (1) {
		/*
		 * If we're at the end of the queue, the PDU isn't there
		 */
		if (*currp == NULL)
			return (0);

		/*
		 * Get the current PDU sequence number
		 */
		cs = (*currp)->ph_ns;

		/*
		 * See if we're at the requested PDU
		 */
		if (seq == cs)
			return (1);

		/*
		 * If we're past the requested seq number, 
		 * the PDU isn't there
		 */
		if (SEQ_LT(seq, cs, sop->so_rcvnext))
			return (0);

		/*
		 * Go to next PDU and keep looking
		 */
		*currp = (*currp)->ph_recv_lk;
	}
}


/*
 * Build and send STAT PDU
 * 
 * A STAT PDU will be constructed and passed down the protocol stack.
 *
 * Arguments:
 *	sop	pointer to sscop connection control block
 *	nps	received poll sequence number (POLL.N(PS))
 *
 * Returns:
 *	0	PDU successfully built and passed down the stack
 *	else	unable to build or send complete pdu
 *
 */
int
sscop_send_stat(sop, nps)
	struct sscop	*sop;
	sscop_seq	nps;
{
	KBuffer		*head, *curr, *n;
	struct pdu_hdr	*rq = sop->so_recv_hd;
	sscop_seq	i;
	sscop_seq	vrh = sop->so_rcvhigh;
	sscop_seq	vrr = sop->so_rcvnext;
	int		len = 0;

	/*
	 * Initialize for start of STAT PDU
	 */
	head = sscop_stat_init(sop);
	if (head == NULL)
		return (1);
	curr = head;

	/*
	 * Start with first PDU not yet received
	 */
	i = vrr;

	/*
	 * Keep looping until we get to last sent PDU
	 */
	while (i != vrh) {

		/*
		 * Find next missing PDU
		 */
		while (SEQ_LT(i, vrh, vrr) && sscop_recv_locate(sop, i, &rq)) {
			SEQ_INCR(i, 1);
		}

		/*
		 * Add odd (start of missing gap) STAT element 
		 */
		n = sscop_stat_add(i, curr);
		if (n == NULL) {
			goto nobufs;
		}
		curr = n;
		len++;

		/*
		 * Have we reached the last sent PDU sequence number??
		 */
		if (i == vrh) {
			/*
			 * Yes, then we're done, send STAT
			 */
			break;
		}

		/*
		 * Have we reached the max STAT size yet??
		 */
		if (len >= PDU_MAX_ELEM) {
			/*
			 * Yes, send this STAT segment
			 */
			if (sscop_stat_end(sop, nps, head, curr)) {
				return (1);
			}

			/*
			 * Start a new segment
			 */
			head = sscop_stat_init(sop);
			if (head == NULL)
				return (1);
			curr = head;

			/*
			 * Restart missing gap
			 */
			curr = sscop_stat_add(i, curr);
			if (curr == NULL) {
				KB_FREEALL(head);
				return (1);
			}
			len = 1;
		}

		/*
		 * Now find the end of the missing gap
		 */
		do {
			SEQ_INCR(i, 1);
		} while (SEQ_LT(i, vrh, vrr) && 
			 (sscop_recv_locate(sop, i, &rq) == 0));

		/*
		 * Add even (start of received gap) STAT element 
		 */
		n = sscop_stat_add(i, curr);
		if (n == NULL) {
			goto nobufs;
		}
		curr = n;
		len++;
	}

	/*
	 * Finally, send the STAT PDU (or last STAT segment)
	 */
	if (sscop_stat_end(sop, nps, head, curr)) {
		return (1);
	}

	return (0);

nobufs:
	/*
	 * Send a truncated STAT PDU
	 */
	sscop_stat_end(sop, nps, head, curr);

	return (1);
}


/*
 * Build and send USTAT PDU
 * 
 * A USTAT PDU will be constructed and passed down the protocol stack.
 *
 * Arguments:
 *	sop	pointer to sscop connection control block
 *	ns	sequence number for second list element 
 *
 * Returns:
 *	0	PDU successfully built and passed down the stack
 *	else	unable to build or send pdu
 *
 */
int
sscop_send_ustat(sop, ns)
	struct sscop	*sop;
	sscop_seq	ns;
{
	KBuffer		*m;
	struct ustat_pdu	*up;
	int		err;


	/*
	 * Get buffer for PDU
	 */
	KB_ALLOCPKT(m, sizeof(struct ustat_pdu), KB_F_NOWAIT, KB_T_HEADER);
	if (m == NULL)
		return (1);

	/*
	 * Setup buffer controls
	 */
	KB_HEADSET(m, MIN(sop->so_headout,
		KB_BFRLEN(m) - sizeof(struct ustat_pdu)));
	KB_LEN(m) = sizeof(struct ustat_pdu);

	/*
	 * Build PDU
	 */
	KB_DATASTART(m, up, struct ustat_pdu *);
	up->ustat_le1 = htonl(SEQ_VAL(sop->so_rcvhigh));
	up->ustat_le2 = htonl(SEQ_VAL(ns));
	up->ustat_nmr = htonl(SEQ_VAL(sop->so_rcvmax));
	up->ustat_nr =
		htonl((PT_USTAT << PT_TYPE_SHIFT) | SEQ_VAL(sop->so_rcvnext));

	/*
	 * Send PDU towards peer
	 */
	STACK_CALL(CPCS_UNITDATA_INV, sop->so_lower, sop->so_tokl,
		sop->so_connvc, (int)m, 0, err);

	if (err)
		KB_FREEALL(m);

	return (err);
}


/*
 * Build and send UD PDU
 * 
 * A UD PDU will be constructed and passed down the protocol stack.
 *
 * Arguments:
 *	sop	pointer to sscop connection control block
 *	m	pointer to user data buffer chain
 *
 * Returns:
 *	0	PDU successfully built and passed down the stack
 *	else	unable to build or send pdu (buffer released)
 *
 */
int
sscop_send_ud(sop, m)
	struct sscop	*sop;
	KBuffer		*m;
{
	KBuffer		*ml, *n;
	int		len = 0, err;
	int		pad, trlen, space;
	u_char		*cp;

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
		KB_FREEALL(m);
		sscop_abort(sop, "sscop: maximum unitdata size exceeded\n");
		return (1);
	}

	/*
	 * Figure out how much padding we'll need
	 */
	pad = ((len + (PDU_PAD_ALIGN - 1)) & ~(PDU_PAD_ALIGN - 1)) - len;
	trlen = pad + sizeof(struct ud_pdu);

	/*
	 * Get space for PDU trailer and padding
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
	 * have to move this a byte at a time.
	 */
	KB_DATAEND(ml, cp, u_char *);
	cp += pad;
	*cp++ = (pad << PT_PAD_SHIFT) | PT_UD;
	KM_ZERO(cp, 3);
	KB_LEN(ml) += trlen;

	/*
	 * Now pass PDU down the stack
	 */
	STACK_CALL(CPCS_UNITDATA_INV, sop->so_lower, sop->so_tokl, 
		sop->so_connvc, (int)m, 0, err);
	if (err) {
		KB_FREEALL(m);
		return (1);
	}

	return (0);
}


/*
 * Print an SSCOP PDU
 * 
 * Arguments:
 *	sop	pointer to sscop connection control block
 *	m	pointer to pdu buffer chain
 *	msg	pointer to message string
 *
 * Returns:
 *	none
 *
 */
void
sscop_pdu_print(sop, m, msg)
	struct sscop	*sop;
	KBuffer		*m;
	char		*msg;
{
	char		buf[128];
	struct vccb	*vcp;

	vcp = sop->so_connvc->cvc_vcc;
	snprintf(buf, sizeof(buf),
	    "sscop %s: vcc=(%d,%d)\n", msg, vcp->vc_vpi, vcp->vc_vci);
	atm_pdu_print(m, buf);
}

