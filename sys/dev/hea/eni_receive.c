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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Efficient ENI Adapter Support
 * -----------------------------
 *
 * Receive management
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/if.h>
#include <net/netisr.h>
#include <netinet/in.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_vc.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#include <dev/hea/eni_stats.h>
#include <dev/hea/eni.h>
#include <dev/hea/eni_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif

static void	eni_recv_stack(void *, KBuffer *);

#ifdef	DIAGNOSTIC
extern int	eni_pdu_print;
#endif

/*
 * Procedure to remove VCs from the Service List and generate DMA
 * requests to move the associated PDUs into host memory. As PDUs
 * are completed in adapter memory, the adapter examines the IN_SERVICE
 * bit for the VC in the VC table. If this bit is not set, the adapter
 * will place the VC number at the end of the service list queue, set
 * the IN_SERVICE bit in the VC table, and interrupt the host. The host
 * will remove VCs from the service list, clear the IN_SERVICE bit in
 * the VC table, and create a DMA list to move the PDU into host buffers.
 *
 * Arguments:
 *	eup		pointer to per unit structure
 *
 * Returns:
 *	none
 *
 */
void
eni_do_service ( eup )
	Eni_unit *eup;
{
	int		vcc;
	Eni_vcc		*evp;
	u_long		servwrite;
	VCI_Table	*vct;
	u_long		rdptr;
	u_long		*rxp;
	KBuffer		*m;
	u_long		dma[TEMP_DMA_SIZE];
	u_long		i, j;
	u_long		dma_rd, dma_wr;
	u_long		dma_avail;
	int		pdulen;
	int		mask;
	u_long		*upp;

	/*
	 * Where is the adapter currently inserting entries?
	 */
	servwrite = eup->eu_midway[MIDWAY_SVCWR] & SVC_SIZE_MASK;
	/*
	 * As long as we're not caught up with the adapter, keep
	 * removing VCs from the service list.
	 */
	while ( servwrite != eup->eu_servread ) {
		int	vci_hdr;
		u_long	descr;

		/*
		 * Get VC number and find VC table entry.
		 */
		vcc = eup->eu_svclist[eup->eu_servread];
		vct = &eup->eu_vcitbl[vcc];
		vci_hdr = vct->vci_control;	/* Current status */

		/*
		 * Check that this VCC still needs servicing. We
		 * might have closed this VCC down in between
		 * the adapter setting the flag and our checking
		 * the flag. Also check that we haven't placed the
		 * VCC into TRASH mode.
		 */
		if ( ( vci_hdr & VCI_IN_SERVICE ) == 0 ||
		    ( (vci_hdr & ~VCI_MODE_MASK) ==
			(VCI_MODE_TRASH << VCI_MODE_SHIFT) ) )
			    goto next_vcc;

		/*
		 * Find the size of this VCs buffer
		 */
		mask = (vci_hdr >> VCI_SIZE_SHIFT) & VCI_SIZE_MASK;
		mask = 1 << (ENI_LOC_PREDIV + mask);
		/* Turn byte count into word count */
		mask >>= 2;
		/*
		 * Find the start of the adapter buffer for this VC.
		 */
		rxp = (u_long *)
		    ((intptr_t)(((vci_hdr >> VCI_LOC_SHIFT ) & VCI_LOC_MASK)
			<< ENI_LOC_PREDIV) + (intptr_t)eup->eu_ram);
		/*
		 * Locate incoming VCC for this PDU and find where we
		 * should next read from.
		 */
		evp = (Eni_vcc *) atm_dev_vcc_find ( (Cmn_unit *)eup,
		    0, vcc, VCC_IN );
		if ( evp == (Eni_vcc *)NULL )
			goto next_vcc;		/* VCI no longer active */
		rdptr = evp->ev_rxpos;
		/*
		 * Find out where the adapter is currently reassembling.
		 * The PDU which starts at descr is not yet complete so we
		 * must stop there.
		 */
		descr = ( vct->vci_descr >> 16 ) & 0x7FFF;
		/*
		 * As long as we haven't processed all the completed PDUs on
		 * this VC, keep going...
		 */
		while ( rdptr != descr )
		{
		    int		n_cells;
		    int		pdu_descr;
		    int		aal5;

		    /*
		     * Ensure that the following are reset for every new
		     * PDU.
		     */
		    upp = NULL;
		    m = NULL;

		    /*
		     * Fisrt build a DMA with JK to skip the descriptor word.
		     * We must always skip the descriptor even if it turns out
		     * that there isn't any PDU here.
		     */
		    j = 0;
		    dma[j++] = (((rdptr + 1) & (mask-1)) << DMA_COUNT_SHIFT ) |
		    	( vcc << DMA_VCC_SHIFT ) | DMA_JK;
		    dma[j++] = 0;

		    /*
		     * We'll use some of the values below for skipping
		     * bad PDUs or counting statistics so compute them
		     * now.
		     */

		    /*
		     * Grab a copy of the descriptor word
		     */
		    pdu_descr = rxp[rdptr];

		    /*
		     * Strip out cell count from descriptor word.
		     * At this point, we still don't know if there
		     * is any real data until after we check for
		     * TRASH mode.
		     */
		    n_cells = pdu_descr & DESCR_CELL_COUNT;

		    /*
		     * Is this an AAL5 PDU? Check MODE in vci_hdr.
		     */
		    aal5 = ( ( vci_hdr & ~VCI_MODE_MASK ) ==
			VCI_MODE_AAL5 << VCI_MODE_SHIFT );

		    /*
		     * Now check to see if we're trashing on this vcc.
		     * If so, there is no data with this VC and the
		     * next word after the current descriptor is the
		     * descriptor for the next PDU.
		     */
		    if ( ( pdu_descr & DESCR_TRASH_BIT ) != 0 ) {
			if ( aal5 )
				/*
				 * Count as number of AAL5 cells dropped
				 */
				eup->eu_stats.eni_st_aal5.aal5_drops += n_cells;
			else
				/*
				 * Count as number of AAL0 cells dropped
				 */
				eup->eu_stats.eni_st_aal0.aal0_drops += n_cells;
			eup->eu_pif.pif_ierrors++;
			/*
			 * When cells have been trashed, all we have in the
			 * buffer is a descriptor word. There are no data
			 * words. Set the number of cells to zero so that
			 * we correctly skip to the next word which will
			 * be the descriptor for the next PDU.
			 */
			n_cells = 0;
			/*
			 * Go issue the DMA to skip this descriptor word.
			 */
			goto send_dma;
		    }

		    /*
		     * Data length: number of cells * cell size
		     */
		    pdulen = n_cells * BYTES_PER_CELL;

		    /*
		     * If this is an AAL5 PDU, then we need to check
		     * for the presence of any CRC errors. If there
		     * is one or more CRC errors, then we are going to
		     * drop this PDU.
		     */
		    if ( aal5 && ( pdu_descr & DESCR_CRC_ERR ) ) {
			/*
			 * Count the stat
			 */
			eup->eu_pif.pif_ierrors++;
			eup->eu_stats.eni_st_aal5.aal5_pdu_crc++;
			if ( evp->ev_connvc->cvc_vcc )
				evp->ev_connvc->cvc_vcc->vc_ierrors++;
			/*
			 * Build a DMA entry to skip the rest of this
			 * PDU.
			 */
			dma[j++] =
			    (((rdptr + n_cells*WORDS_PER_CELL + 1)
				& (mask-1)) << DMA_COUNT_SHIFT ) |
				    (vcc << DMA_VCC_SHIFT ) | DMA_JK;
			dma[j++] = 0;
			/*
			 * All done with this PDU. Get a buffer to save some
			 * data for reclamation services.
			 */
			KB_ALLOCPKT ( m, ENI_SMALL_BSIZE, KB_F_NOWAIT,
			    KB_T_DATA );
			if ( m ) {
				u_long	*up;

				KB_DATASTART ( m, up, u_long * );
				/*
				 * Indicate no PDU
				 */
				KB_PLENSET ( m, 0 );
				/*
				 * Set buffer length - only driver overhead
				 */
				KB_LEN ( m ) = 3 * sizeof ( u_long );
				/*
				 * Insert vcc, space for DMA pointers,
				 * and pdulen
				 */
				*up++ = vcc;
				upp = up;	/* Remember location */
				up++;		/* And skip it */
						/* - to be filled later */
				*up = pdulen;	/* Actual PDU length if it */
						/* were valid */
			} else {
				/*
				 * We've a real problem here as now we can't
				 * reclaim/advance resources/safety pointers.
				 */
				eup->eu_stats.eni_st_drv.drv_rv_norsc++;
#ifdef	DO_LOG
				log ( LOG_ERR,
    "eni_do_service: No drain buffers available. Receiver about to lock.\n" );
#endif
			}
			goto send_dma;
		    }

		    /*
		     * Do we need to strip the AAL layer? Yes if this
		     * is an AAL5 PDU.
		     */
		    if ( aal5 ) {
			/*
			 * Grab the CS-PDU length. Find the address of the
			 * last word, back up one word to skip CRC, and
			 * then mask the whole thing to handle circular wraps.
			 */
			pdulen = rxp[(rdptr + n_cells*WORDS_PER_CELL - 1)
			    & (mask-1)]
				& 0xFFFF;
		    }

		    /*
		     * We now have a valid PDU of some length. Build
		     * the necessary DMA list to move it into host
		     * memory.
		     */

		    /*
		     * Get an initial buffer.
		     */
		    KB_ALLOCPKT ( m, ENI_SMALL_BSIZE, KB_F_NOWAIT, KB_T_DATA );
		    /*
		     * Do we have a valid buffer?
		     */
		    if ( m != (KBuffer *)NULL )
		    {
			int	len;
			u_long	*up;
			KBuffer *m0;
	
			KB_DATASTART ( m, up, u_long * );
			/*
			 * Fill in pdulen in PKTHDR structure (for IP).
			 */
			KB_PLENSET ( m, pdulen );
			/*
			 * We're going to save the VCI nuber, the start
			 * and stop DMA pointers, and the PDU length at
			 * the head of the buffer. We'll pull this out
			 * later after the DMA has completed.
			 *
			 * Insert VCI number as first word in first buffer,
			 * remeber where we want to store the start/stop
			 * pointers, and store the PDU length.
			 */
			*up++ = vcc;	/* PDU's VCC */
			upp = up;	/* Remember where we are */
			up++;		/* To stuff start/stop pointers in */
			*up++ = pdulen;	/* PDU's length */
			/*
			 * Leave some extra room in case a higher protocol
			 * (IP) wants to do a pullup. Maybe we can keep
			 * someone from having to allocate another buffer
			 * a do a larger memory copy.
			 */
			len = MIN ( ENI_SMALL_BSIZE, pdulen );
			(void) eni_set_dma ( eup, 1, dma, TEMP_DMA_SIZE, &j,
				vcc, (u_long)up, len );
			/*
			 * Adjust length of remaining data in PDU
			 */
			pdulen -= len;
			/*
			 * Set buffer length, including our overhead
			 */
			KB_LEN ( m ) = len + 3 * sizeof ( u_long );
			/*
			 * Finish by moving anything which won't fit in
			 * first buffer
			 */
			m0 = m;
			while ( pdulen ) {
				KBuffer *m1;
				u_long	data_addr;
	
				/*
				 * Get another buffer
				 */
				KB_ALLOCEXT ( m1, ENI_LARGE_BSIZE, KB_F_NOWAIT,
					KB_T_DATA );
	
				/*
				 * If we succeeded...
				 */
				if ( m1 ) {
				    /*
				     * Figure out how much we can move into
				     * this buffer.
				     */
				    len = MIN ( ENI_LARGE_BSIZE, pdulen );
				    /*
				     * Setup DMA list for this buffer
				     */
				    KB_DATASTART ( m1, data_addr, u_long );
				    (void) eni_set_dma
					( eup, 1, dma, TEMP_DMA_SIZE, &j, vcc,
					    data_addr, len );
				    /*
				     * Adjust remaining length
				     */
				    pdulen -= len;
				    /*
				     * Set buffer length
				     */
				    KB_LEN ( m1 ) = len;
				    /*
				     * Link new buffer onto end and advance
				     * pointer
				     */
				    KB_NEXT ( m0 ) = m1;
				    m0 = m1;
				} else {
				    /*
				     * Either we were unable to grab another
				     * buffer or there are no large buffers
				     * available. We know that the first
				     * buffer is valid, so drop everything
				     * else, build a JK DMA to skip/drop this
				     * PDU, set the pointers to reclaim
				     * resources/advance pointers, and
				     * finish this PDU now.
				     */
				    if ( KB_NEXT ( m ) )
				    	KB_FREEALL ( KB_NEXT ( m ) );
				    eup->eu_pif.pif_ierrors++;
				    j = 2;
				    dma[j++] =
				        (((rdptr + n_cells*WORDS_PER_CELL + 1)
					    & (mask-1)) << DMA_COUNT_SHIFT ) |
					        (vcc << DMA_VCC_SHIFT ) |
						    DMA_JK;
				    dma[j++] = 0;
				    /*
				     * Reset PDU length to zero
				     */
				    KB_PLENSET ( m, 0 );
				    /*
				     * Count some statistics
				     */
				    /*
				     * Count this as dropped cells
				     */
				    if ( aal5 ) {
					eup->eu_stats.eni_st_aal5.aal5_drops +=
					    n_cells;
					eup->eu_stats.eni_st_aal5.aal5_pdu_drops++;
				    } else
					eup->eu_stats.eni_st_aal0.aal0_drops +=
					    n_cells;
				    /*
				     * Drop it
				     */
				    goto send_dma;
				}
			}
			/*
			 * If necessary, skip AAL layer
			 */
			if ( aal5 ) {
				dma[j++] =
				  (((rdptr + n_cells*WORDS_PER_CELL + 1)
					& (mask-1)) << DMA_COUNT_SHIFT)
				            | (vcc << DMA_VCC_SHIFT) | DMA_JK;
				dma[j++] = 0;
			}
		    } else {
			/*
			 * We failed to get an initial buffer. Since we
			 * haven't changed anything for this PDU yet and the
			 * PDU is still valid, exit now and try to service it
			 * next time around. We're not very likely to get
			 * another buffer right now anyways.
			 */
			eup->eu_stats.eni_st_drv.drv_rv_nobufs++;
#ifdef	DO_LOG
			log ( LOG_ERR,
"eni_do_service: No buffers available. Exiting without servicing service list.\n" );
#endif
			/*
			 * Clear the IN_SERVICE indicator for this VCC
			 */
			vct->vci_control &= ~VCI_IN_SERVICE;
			return;
		    }

send_dma:
		    /*
		     * Set the end bit on the last DMA for this PDU
		     */
		    dma[j-2] |= DMA_END_BIT;

		    /*
		     * Where are the current DMA pointers
		     */
		    dma_rd = eup->eu_midway[MIDWAY_RX_RD];
		    dma_wr = eup->eu_midway[MIDWAY_RX_WR];

		    /*
		     * Check how much space is available
		     */
		    if ( dma_rd == dma_wr )
			dma_avail = DMA_LIST_SIZE;
		    else
			dma_avail = ( dma_rd + DMA_LIST_SIZE - dma_wr )
			    & (DMA_LIST_SIZE-1);

		    /*
		     * Check for queue full or wrap past write okay pointer
		     */
		    if ( dma_avail < j  ||
		        ( dma_wr + j > eup->eu_rxdmawr + DMA_LIST_SIZE ) ) {
			/*
			 * There's no room in the DMA list to insert
			 * this request. Since we haven't changed anything
			 * yet and the PDU is good, exit now and service
			 * it next time around. What we really need to do
			 * is wait for the RX list to drain and that won't
			 * happen if we keep trying to process PDUs here.
			 */
			eup->eu_stats.eni_st_drv.drv_rv_nodma++;
#ifdef	DO_LOG
			log ( LOG_ERR,
"eni_do_service: No room in receive DMA list. Postponing service request.\n" );
#endif
			/*
			 * Free the local buffer chain
			 */
			KB_FREEALL ( m );
			/*
			 * Clear the IN_SERVICE indicator for this VCC.
			 */
			vct->vci_control &= ~VCI_IN_SERVICE;
			return;	
		    }

		    /*
		     * If we have a buffer chain, save the starting
		     * dma_list location.
		     */
		    if ( upp ) {
			*upp = dma_wr << 16;
		    }

		    /*
		     * Stuff the DMA list
		     */
		    j >>= 1;
		    for ( i = 0; i < j; i++ ) {
			eup->eu_rxdma[dma_wr*2] = dma[i*2];
			eup->eu_rxdma[dma_wr*2+1] = dma[i*2+1];
			dma_wr = (dma_wr+1) & (DMA_LIST_SIZE-1);
		    }
		    /*
		     * If we have a buffer chain, save the location of
		     * the ending dma_list location and queue the chain
		     * so that we can recover the resources later.
		     */
		    if ( upp ) {
			*upp |= dma_wr;
		        /*
		         * Place buffer on receive queue waiting for RX_DMA
		         */
		        if ( _IF_QFULL ( &eup->eu_rxqueue ) ) {
			    /*
			     * We haven't done anything we can't back out
			     * of. Drop request and service it next time.
			     * We've inserted the DMA list but it's not
			     * valid until we advance the RX_WR pointer,
			     * thus it's okay to bail here...
			     */
			    eup->eu_stats.eni_st_drv.drv_rv_rxq++;
#ifdef	DO_LOG
			    log ( LOG_ERR,
	"eni_do_service: RX drain queue full. Postponing servicing.\n" );
#endif
			    KB_FREEALL ( m );
			    /*
			     * Clear the IN_SERVICE indicator for this VCC.
			     */
			    vct->vci_control &= ~VCI_IN_SERVICE;
			    return;
		        } else { 
		            _IF_ENQUEUE ( &eup->eu_rxqueue, m );
			    /*
			     * Advance the RX_WR pointer to cause
			     * the adapter to work on this DMA list.
			     */
		            eup->eu_midway[MIDWAY_RX_WR] = dma_wr;
		        }
		    }
		    /*
		     * Advance our notion of where the next PDU
		     * should start.
		     */
		    rdptr = (rdptr + n_cells*WORDS_PER_CELL + 1)
			& (mask-1);
		    evp->ev_rxpos = rdptr;

		    /*
		     * Increment cells/pdu received stats.
		     */
		    eup->eu_stats.eni_st_atm.atm_rcvd += n_cells;
		    if ( aal5 ) {
			eup->eu_stats.eni_st_aal5.aal5_rcvd += n_cells;
			eup->eu_stats.eni_st_aal5.aal5_pdu_rcvd++;
		    } else {
			eup->eu_stats.eni_st_aal0.aal0_rcvd += n_cells;
		    }

		    /*
		     * Continue processing PDUs on this same VCI
		     */
		}

next_vcc:
		/*
		 * Advance to next entry in the service_list.
		 */
		eup->eu_servread = (eup->eu_servread + 1) & SVC_SIZE_MASK;

		/*
		 * And clear the IN_SERVICE indicator for this VCC.
		 */
		vct->vci_control &= ~VCI_IN_SERVICE;
	}
	return;
}

/*
 * Drain Receive queue
 *
 * As we build DMA lists to move PDUs from adapter buffers into host
 * buffers, we place the request on a private ifqueue so that we can
 * free any resources AFTER we know they've been successfully DMAed.
 * As part of the service processing, we record the PDUs start and stop
 * entries in the DMA list, and prevent wrapping. When we pull the top
 * entry off, we simply check that the current DMA location is outside
 * this PDU and if so, it's okay to free things.
 *
 * Arguments:
 *	eup		pointer to device unit structure
 *
 * Returns:
 *	none
 *
 */
void
eni_recv_drain ( eup )
	Eni_unit *eup;
{
	KBuffer 	*m;
	Eni_vcc		*evp;
	struct vccb	*vcp;
	u_long		vcc;
	u_long		DMA_Rdptr;
	u_long		dma_wrp;
	u_long		start, stop;
	int		s;

	s = splimp();
	/* Pop first buffer */
	_IF_DEQUEUE ( &eup->eu_rxqueue, m );
	while ( m ) {
		u_long	*up;
		u_long	pdulen;

		KB_DATASTART ( m, up, u_long * );

		/*
		 * Grab the VCI number
		 */
		vcc = *up++;

		/*
		 * Check to see if we can process this buffer yet.
		 */
		/* Get current DMA_Rdptr */
		DMA_Rdptr = eup->eu_midway[MIDWAY_RX_RD];
		/* Boundaries for first buffer */
		dma_wrp = *up++;
		start = dma_wrp >> 16;
		stop = dma_wrp & 0xffff;
		/*
		 * Start should not equal stop because that would
		 * mean we tried inserting a NULL DMA list.
		 */
		if ( start > stop ) {		/* We wrapped */
			if ( !(DMA_Rdptr >= stop && DMA_Rdptr < start) ) {
				_IF_PREPEND ( &eup->eu_rxqueue, m );
				goto finish;
			}
		} else {
			if ( DMA_Rdptr < stop && DMA_Rdptr >= start ) {
				_IF_PREPEND ( &eup->eu_rxqueue, m );
				goto finish;
			}
		}
		/*
		 * Adapter is finished with this buffer, we can
		 * continue processing it now.
		 */

		/*
		 * Locate incoming VCC for this PDU
		 */
		evp = (Eni_vcc *) atm_dev_vcc_find ( (Cmn_unit *)eup,
		    0, vcc, VCC_IN );

		if ( evp == NULL ) {
			eup->eu_stats.eni_st_drv.drv_rv_novcc++;
			KB_FREEALL ( m );
			goto next_buffer;
		}

#ifdef	DIAGNOSTIC
		if ( eni_pdu_print )
		    atm_dev_pdu_print ( (Cmn_unit *)eup, (Cmn_vcc *)evp, m,
		        "eni_stack_drain" );
#endif

		/*
		 * Grab theoretical PDU length
		 */
		pdulen = *up++;

		/*
		 * Quick, count the PDU
		 */
		eup->eu_pif.pif_ipdus++;
		eup->eu_pif.pif_ibytes += pdulen;
		if ( evp ) {
		    vcp = evp->ev_connvc->cvc_vcc;
		    if ( vcp ) {
			vcp->vc_ipdus++;
			vcp->vc_ibytes += pdulen;
			if ( vcp->vc_nif ) {
			    vcp->vc_nif->nif_ibytes += pdulen;
			    vcp->vc_nif->nif_if.if_ipackets++;
#if (defined(BSD) && (BSD >= 199103))
			    vcp->vc_nif->nif_if.if_ibytes += pdulen;
#endif
			}
		    }
		}

		/*
		 * Advance DMA write allowable pointer
		 */
		eup->eu_rxdmawr = stop;

		/*
		 * Get packet PDU length
		 */
		KB_PLENGET ( m, pdulen );

		/*
		 * Only try queueing this if there is data
		 * to be handed up to the next layer. Errors
		 * such as CRC and VC trashing will get us this
		 * far to advance pointers, etc., but the PDU
		 * length will be zero.
		 */
		if ( pdulen ) {
			/*
			 * We saved three words back in eni_do_service()
			 * to use for callback. Since the core only
			 * expects two words, skip over the first one.
			 * Then, reset up pointer to start of buffer data
			 * area and write the callback info.
			 */
			KB_HEADADJ ( m, -sizeof(u_long) );
			KB_DATASTART ( m, up, u_long * );
			*((int *)up) = (int)eni_recv_stack;
			up++;
			*((int *)up) = (int)(intptr_t)evp;
			/*
			 * Schedule callback
			 */
			if (! netisr_queue(NETISR_ATM, m)) {
				eup->eu_stats.eni_st_drv.drv_rv_intrq++;
				eup->eu_pif.pif_ierrors++;
#ifdef	DO_LOG
				log ( LOG_ERR,
"eni_receive_drain: ATM_INTRQ is full. Unable to pass up stack.\n" );
#endif
			}
		} else {
			/*
			 * Free zero-length buffer
			 */
			KB_FREEALL(m);
		}

next_buffer:
		/*
		 * Look for next buffer
		 */
		_IF_DEQUEUE ( &eup->eu_rxqueue, m );
	}
finish:
	(void) splx(s);
	return;
}

/*
 * Pass incoming PDU up Stack
 *
 * This function is called via the core ATM interrupt queue callback
 * set in eni_recv_drain(). It will pass the supplied incoming
 * PDU up the incoming VCC's stack.
 *
 * Arguments:
 *	tok		token to identify stack instantiation
 *	m		pointer to incoming PDU buffer chain
 *
 * Returns:
 *	none
 */
static void
eni_recv_stack ( tok, m )
	void		*tok;
	KBuffer		*m;
{
	Eni_vcc		*evp = (Eni_vcc *)tok;
	int		err;

	/*
	 * This should never happen now but if it does and we don't stop it,
	 * we end up panic'ing in netatm when trying to pull a function
	 * pointer and token value out of a buffer with address zero.
	 */
	if ( !m ) {
#ifdef	DO_LOG
		log ( LOG_ERR,
			"eni_recv_stack: NULL buffer, tok = %p\n", tok );
#endif
		return;
	}

	/*
	 * Send the data up the stack
	 */
	STACK_CALL ( CPCS_UNITDATA_SIG, evp->ev_upper,
		(void *)evp->ev_toku, evp->ev_connvc, (intptr_t)m, 0, err );
	if ( err ) {
		KB_FREEALL ( m );
	}

	return;
}

