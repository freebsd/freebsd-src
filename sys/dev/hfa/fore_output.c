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
 * FORE Systems 200-Series Adapter Support
 * ---------------------------------------
 *
 * PDU output processing
 *
 */

#include <dev/hfa/fore_include.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Local functions
 */
static KBuffer *	fore_xmit_segment __P((Fore_unit *, KBuffer *,
				H_xmit_queue *, int *, int *));
static void		fore_seg_dma_free __P((H_xmit_queue *, KBuffer *, int));


/*
 * Output a PDU
 * 
 * This function is called via the common driver code after receiving a
 * stack *_DATA* command.  The common code has already validated most of
 * the request so we just need to check a few more Fore-specific details.
 * Then we just build a transmit descriptor request for the PDU and issue 
 * the command to the CP.  
 *
 * Arguments:
 *	cup	pointer to device common unit
 *	cvp	pointer to common VCC entry
 *	m	pointer to output PDU buffer chain head
 *
 * Returns:
 *	none
 *
 */
void
fore_output(cup, cvp, m)
	Cmn_unit	*cup;
	Cmn_vcc		*cvp;
	KBuffer		*m;
{
	Fore_unit	*fup = (Fore_unit *)cup;
	Fore_vcc	*fvp = (Fore_vcc *)cvp;
	struct vccb	*vcp;
	H_xmit_queue	*hxp;
	Xmit_queue	*cqp;
	Xmit_descr	*xdp;
	int		retry, nsegs, pdulen;
	int		s;

#ifdef DIAGNOSTIC
	if (atm_dev_print)
		atm_dev_pdu_print(cup, cvp, m, "fore_output");
#endif

	vcp = fvp->fv_connvc->cvc_vcc;

	/*
	 * If we're still waiting for activation to finish, delay for
	 * a little while before we toss the PDU
	 */
	if (fvp->fv_state == CVS_INITED) {
		retry = 3;
		while (retry-- && (fvp->fv_state == CVS_INITED))
			DELAY(1000);
		if (fvp->fv_state != CVS_ACTIVE) {
			/*
			 * Activation still hasn't finished, oh well....
			 */
			fup->fu_stats->st_drv.drv_xm_notact++;
			vcp->vc_oerrors++;
			if (vcp->vc_nif)
				vcp->vc_nif->nif_if.if_oerrors++;
			KB_FREEALL(m);
			return;
		}
	}

	/*
	 * Queue PDU at end of transmit queue
	 *
	 * If queue is full we'll delay a bit before tossing the PDU
	 */
	s = splnet();
	hxp = fup->fu_xmit_tail;
	if (!((*hxp->hxq_status) & QSTAT_FREE)) {

		fup->fu_stats->st_drv.drv_xm_full++;
		retry = 3;
		do {
			DELAY(1000);

			DEVICE_LOCK((Cmn_unit *)fup);
			fore_xmit_drain(fup);
			DEVICE_UNLOCK((Cmn_unit *)fup);

		} while (--retry && (!((*hxp->hxq_status) & QSTAT_FREE)));

		if (!((*hxp->hxq_status) & QSTAT_FREE)) {
			/*
			 * Queue is still full, bye-bye PDU
			 */
			fup->fu_pif.pif_oerrors++;
			vcp->vc_oerrors++;
			if (vcp->vc_nif)
				vcp->vc_nif->nif_if.if_oerrors++;
			KB_FREEALL(m);
			(void) splx(s);
			return;
		}
	}

	/*
	 * We've got a free transmit queue entry
	 */

	/*
	 * Now build the transmit segment descriptors for this PDU
	 */
	m = fore_xmit_segment(fup, m, hxp, &nsegs, &pdulen);
	if (m == NULL) {
		/*
		 * The build failed, buffer chain has been freed
		 */
		vcp->vc_oerrors++;
		if (vcp->vc_nif)
			vcp->vc_nif->nif_if.if_oerrors++;
		(void) splx(s);
		return;
	}

	/*
	 * Set up the descriptor header
	 */
	xdp = hxp->hxq_descr;
	xdp->xd_cell_hdr = ATM_HDR_SET(vcp->vc_vpi, vcp->vc_vci, 0, 0);
	xdp->xd_spec = XDS_SET_SPEC(0, fvp->fv_aal, nsegs, pdulen);
	xdp->xd_rate = fvp->rate;

	/*
	 * Everything is ready to go, so officially claim the host queue
	 * entry and setup the CP-resident queue entry.  The CP will grab
	 * the PDU when the descriptor pointer is set.
	 */
	fup->fu_xmit_tail = hxp->hxq_next;
	hxp->hxq_buf = m;
	hxp->hxq_vcc = fvp;
	(*hxp->hxq_status) = QSTAT_PENDING;
	cqp = hxp->hxq_cpelem;
	cqp->cq_descr = (CP_dma)
		CP_WRITE((u_long)hxp->hxq_descr_dma | XMIT_SEGS_TO_BLKS(nsegs));

	(void) splx(s);

	/*
	 * See if there are any completed queue entries
	 */
	DEVICE_LOCK((Cmn_unit *)fup);
	fore_xmit_drain(fup);
	DEVICE_UNLOCK((Cmn_unit *)fup);

	return;
}


/*
 * Build Transmit Segment Descriptors
 * 
 * This function will take a supplied buffer chain of data to be transmitted
 * and build the transmit segment descriptors for the data.  This will include 
 * the dreaded operation of ensuring that the data for each transmit segment
 * is full-word aligned and (except for the last segment) is an integral number
 * of words in length.  If the data isn't already aligned and sized as
 * required, then the data must be shifted (copied) into place - a sure
 * performance killer.  Note that we rely on the fact that all buffer data
 * areas are allocated with (at least) full-word alignments/lengths.
 *
 * If any errors are encountered, the buffer chain will be freed.
 * 
 * Arguments:
 *	fup	pointer to device unit
 *	m	pointer to output PDU buffer chain head
 *	hxp	pointer to host transmit queue entry
 *	segp	pointer to return the number of transmit segments
 *	lenp	pointer to return the pdu length
 *
 * Returns:
 *	m	build successful, pointer to (possibly new) head of 
 *		output PDU buffer chain
 *	NULL	build failed, buffer chain freed
 *
 */
static KBuffer *
fore_xmit_segment(fup, m, hxp, segp, lenp)
	Fore_unit	*fup;
	KBuffer		*m;
	H_xmit_queue	*hxp;
	int		*segp;
	int		*lenp;
{
	Xmit_descr	*xdp = hxp->hxq_descr;
	Xmit_seg_descr	*xsp;
	H_dma		*sdmap;
	KBuffer		*m0, *m1, *mprev;
	caddr_t		cp, bfr;
	void		*dma;
	int		pdulen, nsegs, len, align;
	int		compressed = 0;

	m0 = m;

retry:
	xsp = xdp->xd_seg;
	sdmap = hxp->hxq_dma;
	mprev = NULL;
	pdulen = 0;
	nsegs = 0;

	/*
	 * Loop thru each buffer in the chain, performing the necessary
	 * data positioning and then building a segment descriptor for
	 * that data.
	 */
	while (m) {
		/*
		 * Get rid of any zero-length buffers
		 */
		if (KB_LEN(m) == 0) {
			if (mprev) {
				KB_UNLINK(m, mprev, m1);
			} else {
				KB_UNLINKHEAD(m, m1);
				m0 = m1;
			}
			m = m1;
			continue;
		}

		/*
		 * Make sure we don't try to use too many segments
		 */
		if (nsegs >= XMIT_MAX_SEGS) {
			/*
			 * First, free already allocated DMA addresses
			 */
			fore_seg_dma_free(hxp, m0, nsegs);

			/*
			 * Try to compress buffer chain (but only once)
			 */
			if (compressed) {
				KB_FREEALL(m0);
				return (NULL);
			}

			fup->fu_stats->st_drv.drv_xm_maxpdu++;

			m = atm_dev_compress(m0);
			if (m == NULL) {
				return (NULL);
			}

			/*
			 * Build segment descriptors for compressed chain
			 */
			m0 = m;
			compressed = 1;
			goto retry;
		}

		/*
		 * Get start of data onto full-word alignment
		 */
		KB_DATASTART(m, cp, caddr_t);
		if ((align = ((u_int)cp) & (XMIT_SEG_ALIGN - 1)) != 0) {
			/*
			 * Gotta slide the data up
			 */
			fup->fu_stats->st_drv.drv_xm_segnoal++;
			bfr = cp - align;
			KM_COPY(cp, bfr, KB_LEN(m));
			KB_HEADMOVE(m, -align);
		} else {
			/*
			 * Data already aligned
			 */
			bfr = cp;
		}

		/*
		 * Now work on getting the data length correct
		 */
		len = KB_LEN(m);
		while ((align = (len & (XMIT_SEG_ALIGN - 1))) &&
		       (m1 = KB_NEXT(m))) {

			/*
			 * Have to move some data from following buffer(s)
			 * to word-fill this buffer
			 */
			int	ncopy = MIN(XMIT_SEG_ALIGN - align, KB_LEN(m1));

			if (ncopy) {
				/*
				 * Move data to current buffer
				 */
				caddr_t		dest;

				fup->fu_stats->st_drv.drv_xm_seglen++;
				KB_DATASTART(m1, cp, caddr_t);
				dest = bfr + len;
				KB_HEADADJ(m1, -ncopy);
				KB_TAILADJ(m, ncopy);
				len += ncopy;
				while (ncopy--) {
					*dest++ = *cp++;
				}
			}

			/*
			 * If we've drained the buffer, free it
			 */
			if (KB_LEN(m1) == 0) {
				KBuffer		*m2;

				KB_UNLINK(m1, m, m2);
			}
		}

		/*
		 * Finally, build the segment descriptor
		 */

		/*
		 * Round last segment to fullword length (if needed)
		 */
		if (len & (XMIT_SEG_ALIGN - 1))
			xsp->xsd_len = KB_LEN(m) =
				(len + XMIT_SEG_ALIGN) & ~(XMIT_SEG_ALIGN - 1);
		else
			xsp->xsd_len = KB_LEN(m) = len;

		/*
		 * Get a DMA address for the data
		 */
		dma = DMA_GET_ADDR(bfr, xsp->xsd_len, XMIT_SEG_ALIGN, 0);
		if (dma == NULL) {
			fup->fu_stats->st_drv.drv_xm_segdma++;
			fore_seg_dma_free(hxp, m0, nsegs);
			KB_FREEALL(m0);
			return (NULL);
		}

		/*
		 * Now we're really ready to call it a segment
		 */
		*sdmap++ = xsp->xsd_buffer = (H_dma) dma;

		/*
		 * Bump counters and get ready for next buffer
		 */
		pdulen += len;
		nsegs++;
		xsp++;
		mprev = m;
		m = KB_NEXT(m);
	}

	/*
	 * Validate PDU length
	 */
	if (pdulen > XMIT_MAX_PDULEN) {
		fup->fu_stats->st_drv.drv_xm_maxpdu++;
		fore_seg_dma_free(hxp, m0, nsegs);
		KB_FREEALL(m0);
		return (NULL);
	}

	/*
	 * Return the good news to the caller
	 */
	*segp = nsegs;
	*lenp = pdulen;

	return (m0);
}


/*
 * Free Transmit Segment Queue DMA addresses
 * 
 * Arguments:
 *	hxp	pointer to host transmit queue entry
 *	m0	pointer to output PDU buffer chain head
 *	nsegs	number of processed transmit segments
 *
 * Returns:
 *	none
 *
 */
static void
fore_seg_dma_free(hxp, m0, nsegs)
	H_xmit_queue	*hxp;
	KBuffer		*m0;
	int		nsegs;
{
	KBuffer		*m = m0;
	H_dma		*sdmap = hxp->hxq_dma;
	caddr_t         cp;
	int		i;

	for (i = 0; i < nsegs; i++) {
		KB_DATASTART(m, cp, caddr_t);
		DMA_FREE_ADDR(cp, *sdmap, KB_LEN(m), 0);
		m = KB_NEXT(m);
		sdmap++;
	}
}

