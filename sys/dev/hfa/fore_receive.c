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
 *	@(#) $FreeBSD: src/sys/dev/hfa/fore_receive.c,v 1.5 1999/08/28 00:41:51 peter Exp $
 *
 */

/*
 * FORE Systems 200-Series Adapter Support
 * ---------------------------------------
 *
 * Receive queue management
 *
 */

#include <dev/hfa/fore_include.h>

#ifndef lint
__RCSID("@(#) $FreeBSD: src/sys/dev/hfa/fore_receive.c,v 1.5 1999/08/28 00:41:51 peter Exp $");
#endif


/*
 * Local functions
 */
static void	fore_recv_stack __P((void *, KBuffer *));


/*
 * Allocate Receive Queue Data Structures
 *
 * Arguments:
 *	fup		pointer to device unit structure
 *
 * Returns:
 *	0		allocations successful
 *	else		allocation failed
 */
int
fore_recv_allocate(fup)
	Fore_unit	*fup;
{
	caddr_t		memp;

	/*
	 * Allocate non-cacheable memory for receive status words
	 */
	memp = atm_dev_alloc(sizeof(Q_status) * RECV_QUELEN,
			QSTAT_ALIGN, ATM_DEV_NONCACHE);
	if (memp == NULL) {
		return (1);
	}
	fup->fu_recv_stat = (Q_status *) memp;

	memp = DMA_GET_ADDR(fup->fu_recv_stat, sizeof(Q_status) * RECV_QUELEN,
			QSTAT_ALIGN, ATM_DEV_NONCACHE);
	if (memp == NULL) {
		return (1);
	}
	fup->fu_recv_statd = (Q_status *) memp;

	/*
	 * Allocate memory for receive descriptors
	 */
	memp = atm_dev_alloc(sizeof(Recv_descr) * RECV_QUELEN,
			RECV_DESCR_ALIGN, 0);
	if (memp == NULL) {
		return (1);
	}
	fup->fu_recv_desc = (Recv_descr *) memp;

	memp = DMA_GET_ADDR(fup->fu_recv_desc,
			sizeof(Recv_descr) * RECV_QUELEN, RECV_DESCR_ALIGN, 0);
	if (memp == NULL) {
		return (1);
	}
	fup->fu_recv_descd = (Recv_descr *) memp;

	return (0);
}


/*
 * Receive Queue Initialization
 *
 * Allocate and initialize the host-resident receive queue structures
 * and then initialize the CP-resident queue structures.
 * 
 * Called at interrupt level.
 *
 * Arguments:
 *	fup		pointer to device unit structure
 *
 * Returns:
 *	none
 */
void
fore_recv_initialize(fup)
	Fore_unit	*fup;
{
	Aali		*aap = fup->fu_aali;
	Recv_queue	*cqp;
	H_recv_queue	*hrp;
	Recv_descr	*rdp;
	Recv_descr	*rdp_dma;
	Q_status	*qsp;
	Q_status	*qsp_dma;
	int		i;

	/*
	 * Point to CP-resident receive queue
	 */
	cqp = (Recv_queue *)(fup->fu_ram + CP_READ(aap->aali_recv_q));

	/*
	 * Point to host-resident receive queue structures
	 */
	hrp = fup->fu_recv_q;
	qsp = fup->fu_recv_stat;
	qsp_dma = fup->fu_recv_statd;
	rdp = fup->fu_recv_desc;
	rdp_dma = fup->fu_recv_descd;

	/*
	 * Loop thru all queue entries and do whatever needs doing
	 */
	for (i = 0; i < RECV_QUELEN; i++) {

		/*
		 * Set queue status word to free
		 */
		*qsp = QSTAT_FREE;

		/*
		 * Set up host queue entry and link into ring
		 */
		hrp->hrq_cpelem = cqp;
		hrp->hrq_status = qsp;
		hrp->hrq_descr = rdp;
		hrp->hrq_descr_dma = rdp_dma;
		if (i == (RECV_QUELEN - 1))
			hrp->hrq_next = fup->fu_recv_q;
		else
			hrp->hrq_next = hrp + 1;

		/*
		 * Now let the CP into the game
		 */
		cqp->cq_descr = (CP_dma) CP_WRITE(rdp_dma);
		cqp->cq_status = (CP_dma) CP_WRITE(qsp_dma);

		/*
		 * Bump all queue pointers
		 */
		hrp++;
		qsp++;
		qsp_dma++;
		rdp++;
		rdp_dma++;
		cqp++;
	}

	/*
	 * Initialize queue pointers
	 */
	fup->fu_recv_head = fup->fu_recv_q;

	return;
}


/*
 * Drain Receive Queue
 *
 * This function will process all completed entries at the head of the
 * receive queue.  The received segments will be linked into a received
 * PDU buffer chain and it will then be passed up the PDU's VCC stack for 
 * processing by the next higher protocol layer.
 *
 * May be called in interrupt state.
 * Must be called with interrupts locked out.
 *
 * Arguments:
 *	fup		pointer to device unit structure
 *
 * Returns:
 *	none
 */
void
fore_recv_drain(fup)
	Fore_unit	*fup;
{
	H_recv_queue	*hrp = NULL;
	Recv_descr	*rdp;
	Recv_seg_descr	*rsp;
	Buf_handle	*bhp;
	Fore_vcc	*fvp;
	struct vccb	*vcp;
	KBuffer		*m, *mhead, *mtail;
	caddr_t		cp;
	u_long		hdr, nsegs;
	u_int		seglen, type0;
	int		i, pdulen, retries = 0, error;

	/* Silence the compiler */
	mtail = NULL;
	type0 = 0;

	/*
	 * Process each completed entry
	 */
retry:
	while (*fup->fu_recv_head->hrq_status & QSTAT_COMPLETED) {

		/*
		 * Get completed entry's receive descriptor
		 */
		hrp = fup->fu_recv_head;
		rdp = hrp->hrq_descr;

#ifdef VAC
		/*
		 * Cache flush receive descriptor 
		 */
		if (vac) {
			vac_flush((addr_t)rdp, sizeof(Recv_descr));
		}
#endif

		hdr = rdp->rd_cell_hdr;
		nsegs = rdp->rd_nsegs;

		pdulen = 0;
		error = 0;
		mhead = NULL;

		/*
		 * Locate incoming VCC for this PDU
		 */
		fvp = (Fore_vcc *) atm_dev_vcc_find((Cmn_unit *)fup,
			ATM_HDR_GET_VPI(hdr), ATM_HDR_GET_VCI(hdr), VCC_IN);

		/*
		 * Check for a receive error
		 *
		 * Apparently the receive descriptor itself contains valid 
		 * information, but the received pdu data is probably bogus.
		 * We'll arrange for the receive buffer segments to be tossed.
		 */
		if (*hrp->hrq_status & QSTAT_ERROR) {

			fup->fu_pif.pif_ierrors++;
			if (fvp) {
				vcp = fvp->fv_connvc->cvc_vcc;
				vcp->vc_ierrors++;
				if (vcp->vc_nif)
					vcp->vc_nif->nif_if.if_ierrors++;
			}
			ATM_DEBUG1("fore receive error: hdr=0x%lx\n", hdr);
			error = 1;
		}

		/*
		 * Build PDU buffer chain from receive segments
		 */
		for (i = 0, rsp = rdp->rd_seg; i < nsegs; i++, rsp++) {

			bhp = rsp->rsd_handle;
			seglen = rsp->rsd_len;

			/*
			 * Remove buffer from our supplied queue and get
			 * to the underlying buffer
			 */
			switch (bhp->bh_type) {

			case BHT_S1_SMALL:
				DEQUEUE(bhp, Buf_handle, bh_qelem,
					fup->fu_buf1s_bq);
				fup->fu_buf1s_cnt--;
				m = (KBuffer *) ((caddr_t)bhp - BUF1_SM_HOFF);
				KB_DATASTART(m, cp, caddr_t);
				DMA_FREE_ADDR(cp, bhp->bh_dma, BUF1_SM_SIZE, 0);
				break;

			case BHT_S1_LARGE:
				DEQUEUE(bhp, Buf_handle, bh_qelem,
					fup->fu_buf1l_bq);
				fup->fu_buf1l_cnt--;
				m = (KBuffer *) ((caddr_t)bhp - BUF1_LG_HOFF);
				KB_DATASTART(m, cp, caddr_t);
				DMA_FREE_ADDR(cp, bhp->bh_dma, BUF1_LG_SIZE, 0);
				break;

			default:
				log(LOG_ERR,
					"fore_recv_drain: bhp=%p type=0x%x\n",
					bhp, bhp->bh_type);
				panic("fore_recv_drain: bad buffer type");
			}

			/*
			 * Toss any zero-length or receive error buffers 
			 */
			if ((seglen == 0) || error) {
				KB_FREEALL(m);
				continue;
			}

			/*
			 * Link buffer into chain
			 */
			if (mhead == NULL) {
				type0 = bhp->bh_type;
				KB_LINKHEAD(m, mhead);
				mhead = m;
			} else {
				KB_LINK(m, mtail);
			}
			KB_LEN(m) = seglen;
			pdulen += seglen;
			mtail = m;

			/*
			 * Flush received buffer data
			 */
#ifdef VAC
			if (vac) {
				addr_t	dp;

				KB_DATASTART(m, dp, addr_t);
				vac_pageflush(dp);
			}
#endif
		}

		/*
		 * Make sure we've got a non-null PDU
		 */
		if (mhead == NULL) {
			goto free_ent;
		}

		/*
		 * We only support user data PDUs (for now)
		 */
		if (hdr & ATM_HDR_SET_PT(ATM_PT_NONUSER)) {
			KB_FREEALL(mhead);
			goto free_ent;
		}

		/*
		 * Toss the data if there's no VCC
		 */
		if (fvp == NULL) {
			fup->fu_stats->st_drv.drv_rv_novcc++;
			KB_FREEALL(mhead);
			goto free_ent;
		}

#ifdef DIAGNOSTIC
		if (atm_dev_print)
			atm_dev_pdu_print((Cmn_unit *)fup, (Cmn_vcc *)fvp, 
				mhead, "fore_recv");
#endif

		/*
		 * Make sure we have our queueing headroom at the front
		 * of the buffer chain
		 */
		if (type0 != BHT_S1_SMALL) {

			/*
			 * Small buffers already have headroom built-in, but
			 * if CP had to use a large buffer for the first 
			 * buffer, then we have to allocate a buffer here to
			 * contain the headroom.
			 */
			fup->fu_stats->st_drv.drv_rv_nosbf++;

			KB_ALLOCPKT(m, BUF1_SM_SIZE, KB_F_NOWAIT, KB_T_DATA);
			if (m == NULL) {
				fup->fu_stats->st_drv.drv_rv_nomb++;
				KB_FREEALL(mhead);
				goto free_ent;
			}

			/*
			 * Put new buffer at head of PDU chain
			 */
			KB_LINKHEAD(m, mhead);
			KB_LEN(m) = 0;
			KB_HEADSET(m, BUF1_SM_DOFF);
			mhead = m;
		}

		/*
		 * It looks like we've got a valid PDU - count it quick!!
		 */
		KB_PLENSET(mhead, pdulen);
		fup->fu_pif.pif_ipdus++;
		fup->fu_pif.pif_ibytes += pdulen;
		vcp = fvp->fv_connvc->cvc_vcc;
		vcp->vc_ipdus++;
		vcp->vc_ibytes += pdulen;
		if (vcp->vc_nif) {
			vcp->vc_nif->nif_ibytes += pdulen;
			vcp->vc_nif->nif_if.if_ipackets++;
#if (defined(BSD) && (BSD >= 199103))
			vcp->vc_nif->nif_if.if_ibytes += pdulen;
#endif
		}

		/*
		 * The STACK_CALL needs to happen at splnet() in order
		 * for the stack sequence processing to work.  Schedule an
		 * interrupt queue callback at splnet() since we are 
		 * currently at device level.
		 */

		/*
		 * Prepend callback function pointer and token value to buffer.
		 * We have already guaranteed that the space is available
		 * in the first buffer.
		 */
		KB_HEADADJ(mhead, sizeof(atm_intr_func_t) + sizeof(int));
		KB_DATASTART(mhead, cp, caddr_t);
		*((atm_intr_func_t *)cp) = fore_recv_stack;
		cp += sizeof(atm_intr_func_t);
		*((void **)cp) = (void *)fvp;

		/*
		 * Schedule callback
		 */
		if (!IF_QFULL(&atm_intrq)) {
			IF_ENQUEUE(&atm_intrq, mhead);
			SCHED_ATM;
		} else {
			fup->fu_stats->st_drv.drv_rv_ifull++;
			KB_FREEALL(mhead);
			goto free_ent;
		}

free_ent:
		/*
		 * Mark this entry free for use and bump head pointer
		 * to the next entry in the queue
		 */
		*hrp->hrq_status = QSTAT_FREE;
		hrp->hrq_cpelem->cq_descr = 
			(CP_dma) CP_WRITE((u_long)hrp->hrq_descr_dma);
		fup->fu_recv_head = hrp->hrq_next;
	}

	/*
	 * Nearly all of the interrupts generated by the CP will be due
	 * to PDU reception.  However, we may receive an interrupt before
	 * the CP has completed the status word DMA to host memory.  Thus,
	 * if we haven't processed any PDUs during this interrupt, we will
	 * wait a bit for completed work on the receive queue, rather than 
	 * having to field an extra interrupt very soon.
	 */
	if (hrp == NULL) {
		if (++retries <= FORE_RECV_RETRY) {
			DELAY(FORE_RECV_DELAY);
			goto retry;
		}
	}

	return;
}


/*
 * Pass Incoming PDU up Stack
 *
 * This function is called via the core ATM interrupt queue callback 
 * set in fore_recv_drain().  It will pass the supplied incoming 
 * PDU up the incoming VCC's stack.
 *
 * Called at splnet.
 *
 * Arguments:
 *	tok		token to identify stack instantiation
 *	m		pointer to incoming PDU buffer chain
 *
 * Returns:
 *	none
 */
static void
fore_recv_stack(tok, m)
	void		*tok;
	KBuffer		*m;
{
	Fore_vcc	*fvp = (Fore_vcc *)tok;
	int		err;

	/*
	 * Send the data up the stack
	 */
	STACK_CALL(CPCS_UNITDATA_SIG, fvp->fv_upper,
		fvp->fv_toku, fvp->fv_connvc, (int)m, 0, err);
	if (err)
		KB_FREEALL(m);

	return;
}


/*
 * Free Receive Queue Data Structures
 *
 * Arguments:
 *	fup		pointer to device unit structure
 *
 * Returns:
 *	none
 */
void
fore_recv_free(fup)
	Fore_unit	*fup;
{
	/*
	 * We'll just let fore_buf_free() take care of freeing any
	 * buffers sitting on the receive queue (which are also still
	 * on the fu_*_bq queue).
	 */
	if (fup->fu_flags & CUF_INITED) {
	}

	/*
	 * Free the status words
	 */
	if (fup->fu_recv_stat) {
		if (fup->fu_recv_statd) {
			DMA_FREE_ADDR(fup->fu_recv_stat, fup->fu_recv_statd,
				sizeof(Q_status) * RECV_QUELEN,
				ATM_DEV_NONCACHE);
		}
		atm_dev_free((volatile void *)fup->fu_recv_stat);
		fup->fu_recv_stat = NULL;
		fup->fu_recv_statd = NULL;
	}

	/*
	 * Free the receive descriptors
	 */
	if (fup->fu_recv_desc) {
		if (fup->fu_recv_descd) {
			DMA_FREE_ADDR(fup->fu_recv_desc, fup->fu_recv_descd,
				sizeof(Recv_descr) * RECV_QUELEN, 0);
		}
		atm_dev_free(fup->fu_recv_desc);
		fup->fu_recv_desc = NULL;
		fup->fu_recv_descd = NULL;
	}

	return;
}

