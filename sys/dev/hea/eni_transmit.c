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
 *	@(#) $FreeBSD: src/sys/dev/hea/eni_transmit.c,v 1.6 1999/12/21 08:24:35 eivind Exp $
 *
 */

/*
 * Efficient ENI Adapter Support
 * -----------------------------
 * 
 * Transmit queue management and PDU output processing
 *
 */


#include <netatm/kern_include.h>

#include <dev/hea/eni_stats.h>
#include <dev/hea/eni.h>
#include <dev/hea/eni_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD: src/sys/dev/hea/eni_transmit.c,v 1.6 1999/12/21 08:24:35 eivind Exp $");
#endif

/*
 * Make a variable which controls printing of PDUs
 * as they travel through the driver.
 */
#ifdef	DIAGNOSTIC
int	eni_pdu_print = 0;
#endif

/*
 * Some PCI chipsets do not handle one or more of the 8WORD or
 * 4WORD DMA transfer sizes. Default to using only 1WORD transfer
 * sizes unless the user wishes to experiment.
 *
 * Make sure that these have to be changed here in this module.
 */
#define	DMA_USE_8WORD
#define	DMA_USE_4WORD

/*
 * Create a DMA list entry
 *
 * DMA entries consist of a control word and a physical address.
 * Control words are comprised of a DMA type, a count of type transfers
 * to occur, and a variable which for TX requests is the TX channel
 * number and for RX requests is the VCC number.
 *
 * Arguments:
 *	eup		pointer to unit structure
 *	rx		set if receiving
 *	dma_list	pointer to DMA list structure
 *	list_size	length of DMA list structure
 *	idx		pointer to current list entry
 *	val		TX channel or RX vcc
 *	addr		virtual DMA address of data buffer
 *	size		size in bytes of DMA request to be built
 *
 * Returns:
 *	dma_list	updated with new entries
 *	idx		points to next list entry
 *	-1		no room in DMA list structure or DMA_GET_ADDR failed
 */
int
eni_set_dma ( eup, rx, dma_list, list_size, idx, val, addr, size )
Eni_unit *eup;
u_long	*dma_list;
int	list_size;
long	*idx;
int	val;
u_long	addr;
int	size;
{
	int	dsize;		/* Size of current DMA request */

	/*
	 * Round up to multiple of word and convert to number
	 * of words rather then number of bytes.
	 */
	size = ( size + 3 ) >> 2;

#ifdef	DMA_USE_8WORD
	/*
	 * Check for room in DMA list - we need two entires
	 */
	if ( *idx + 2 >= list_size )
		return ( -1 );

	/*
	 * Here is the big win. Move as much data possible with
	 * n 8WORD DMAs.
	 */
	/*
	 * Check if we can do one or more 8WORD DMAs
	 */
	dsize = size & ~7;
	if ( dsize ) {
		dma_list[(*idx)++] = ( dsize >> 3 ) << DMA_COUNT_SHIFT |
			val << DMA_VCC_SHIFT | DMA_8WORD;
		dma_list[*idx] = (u_long)DMA_GET_ADDR ( addr, dsize, 0, 0 );
		if ( dma_list[*idx] == 0 ) {
			if ( rx )
				eup->eu_stats.eni_st_drv.drv_rv_segdma++;
			else
				eup->eu_stats.eni_st_drv.drv_xm_segdma++;
			return ( -1 );		/* DMA_GET_ADDR failed */
		} else
			(*idx)++;		/* increment index */
		/*
		 * Adjust addr and size
		 */
		addr += dsize << 2;
		size &= 7;
	}
#endif	/* DMA_USE_8WORD */

#ifdef	DMA_USE_4WORD
	/*
	 * Check for room in DMA list - we need two entries
	 */
	if ( *idx + 2 >= list_size )
		return ( -1 );

	/*
	 * Kindof a tossup from this point on. Since we hacked as many 
	 * 8WORD DMAs off as possible, we are left with 0-7 words
	 * of remaining data. We could do upto one 4WORD with 0-3
	 * words left, or upto three 2WORDS with 0-1 words left,
	 * or upto seven WORDS with nothing left. Someday we should
	 * experiment with performance and see if any particular
	 * combination is a better win then some other...
	 */
	/*
	 * Check if we can do one or more 4WORD DMAs
	 */
	dsize = size & ~3;
	if ( dsize ) {
		dma_list[(*idx)++] = ( dsize >> 2 ) << DMA_COUNT_SHIFT |
			val << DMA_VCC_SHIFT | DMA_4WORD;
		dma_list[*idx] = (u_long)DMA_GET_ADDR ( addr, dsize, 0, 0 );
		if ( dma_list[*idx] == 0 ) {
			if ( rx )
				eup->eu_stats.eni_st_drv.drv_rv_segdma++;
			else
				eup->eu_stats.eni_st_drv.drv_xm_segdma++;
			return ( -1 );		/* DMA_GET_ADDR failed */
		} else
			(*idx)++;		/* increment index */
		/*
		 * Adjust addr and size
		 */
		addr += dsize << 2;
		size &= 3;
	}
#endif	/* DMA_USE_4WORD */

	/*
	 * Check for room in DMA list - we need two entries
	 */
	if ( *idx + 2 >= list_size )
		return ( -1 );

	/*
	 * Hard to know if one 2WORD and 0/1 WORD DMA would be better
	 * then 2/3 WORD DMAs. For now, skip 2WORD DMAs in favor of
	 * WORD DMAs.
	 */

	/*
	 * Finish remaining size a 1WORD DMAs
	 */
	if ( size ) {
		dma_list[(*idx)++] = ( size ) << DMA_COUNT_SHIFT |
			val << DMA_VCC_SHIFT | DMA_WORD;
		dma_list[*idx] = (u_long)DMA_GET_ADDR ( addr, size, 0, 0 );
		if ( dma_list[*idx] == 0 ) {
			if ( rx )
				eup->eu_stats.eni_st_drv.drv_rv_segdma++;
			else
				eup->eu_stats.eni_st_drv.drv_xm_segdma++;
			return ( -1 );		/* DMA_GET_ADDR failed */
		} else
			(*idx)++;		/* increment index */
	}

	/*
	 * Inserted descriptor okay
	 */
	return 0;
}

/*
 * Drain Transmit queue
 *
 * As PDUs are given to the adapter to be transmitted, we
 * place them into a private ifqueue so that we can free
 * any resources AFTER we know they've been successfully DMAed.
 * As part of the output processing, we record the PDUs start
 * and stop entries in the DMA list, and prevent wrapping. When
 * we pull the top element off, we simply check that the current
 * DMA location is outside this PDU and if so, it's okay to free
 * things.
 *
 * PDUs are always in ascending order in the queue.
 *
 * Arguments:
 *	eup		pointer to device unit structure
 *
 * Returns:
 *	none
 *
 */
void
eni_xmit_drain ( eup )
	Eni_unit *eup;
{
	KBuffer 	*m;
	Eni_vcc		*evp;
	struct vccb	*vcp;
	u_long		pdulen;
	u_long		start, stop;
	u_long		dmap;
	int		s = splimp();

	/*
	 * Pull the top element (PDU) off
	 */
	IF_DEQUEUE ( &eup->eu_txqueue, m );
	/*
	 * As long as there are valid elements
	 */
	while ( m ) {
		u_long *up;

		/*
		 * Find start of buffer
		 */
		KB_DATASTART ( m, up, u_long * );

		/*
		 * First word is the VCC for this PDU
		 */
		/*
		 * NOTE: There is a potential problem here in that
		 * if the VCC is closed after this buffer was transmitted
		 * but before we get here, that while evp is non-null,
		 * it will not reference a valid vccb. We need to either
		 * delay closing the VCC until all references are removed
		 * from the drain stacks, actually go through the drain
		 * stacks and remove any references, or find someway of
		 * indicating that this vccb is nolonger usable.
		 */
		evp = (Eni_vcc *)*up++;
		/*
		 * Second word is the start and stop DMA pointers
		 */
		start = *up >> 16;
		stop = *up++ & 0xffff;
		/*
		 * Find out where the TX engine is at
		 */
		dmap = eup->eu_midway[MIDWAY_TX_RD];
		/*
		 * Check to see if TX engine has processed this
		 * PDU yet. Remember that everything is circular
		 * and that stop might be less than start numerically.
		 */
		if ( start > stop ) {
		    if ( !(dmap >= stop && dmap < start) ) {
			/*
			 * Haven't finished this PDU yet - replace
			 * it as the head of list.
			 */
			IF_PREPEND ( &eup->eu_txqueue, m );
			/*
			 * If this one isn't done, none of the others
			 * are either.
			 */
			(void) splx(s);
			return;
		    }
		} else {
		    if ( dmap < stop && dmap >= start ) {
			/*
			 * Haven't finished this PDU yet - replace
			 * it as the head of list.
			 */
			IF_PREPEND ( &eup->eu_txqueue, m );
			/*
			 * If this one isn't done, none of the others
			 * are either.
			 */
			(void) splx(s);
			return;
		    }
		}

		/*
		 * Count the PDU stats for this interface
		 */
		eup->eu_pif.pif_opdus++;
		/*
		 * Third word is PDU length from eni_output().
		 */
		pdulen = *up++;
		eup->eu_txfirst = (eup->eu_txfirst + *up) &
			(eup->eu_txsize - 1);
		eup->eu_pif.pif_obytes += pdulen;

		/*
		 * Now lookup the VCC entry and counts the stats for
		 * this VC.
		 */
		if ( evp ) {
		    vcp = evp->ev_connvc->cvc_vcc;
		    if ( vcp ) {
			vcp->vc_opdus++;
			vcp->vc_obytes += pdulen;
			/*
			 * If we also have a network interface, count the PDU
			 * there also.
			 */
			if ( vcp->vc_nif ) {
				vcp->vc_nif->nif_obytes += pdulen;
				vcp->vc_nif->nif_if.if_opackets++;
#if (defined(BSD) && (BSD >= 199103))
				vcp->vc_nif->nif_if.if_obytes += pdulen;
#endif
			}
		    }
		}
		/*
		 * Free the buffer chain
		 */
		KB_FREEALL ( m );

		/*
		 * Advance DMA write okay pointer
		 */
		eup->eu_txdmawr = stop;

		/*
		 * Look for next completed transmit PDU
		 */
		IF_DEQUEUE ( &eup->eu_txqueue, m );
	}
	/*
	 * We've drained the queue...
	 */
	(void) splx(s);
	return;
}

/*
 * Output a PDU
 *
 * This function is called via the common driver code after receiving a
 * stack *_DATA* command. The common code has already validated most of
 * the request so we just need to check a few more ENI-specific details.
 * Then we just build a segmentation structure for the PDU and place the
 * address into the DMA_Transmit_queue.
 *
 * Arguments:
 *	cup		pointer to device common unit
 *	cvp		pointer to common VCC entry
 *	m		pointer to output PDU buffer chain head
 *
 * Returns:
 *	none
 *
 */
void
eni_output ( cup, cvp, m )
	Cmn_unit	*cup;
	Cmn_vcc		*cvp;
	KBuffer		*m;
{
	Eni_unit	*eup = (Eni_unit *)cup;
	Eni_vcc		*evp = (Eni_vcc *)cvp;
	int		s, s2;
	int		pdulen = 0;
	u_long		size;
	u_long		buf_avail;
	u_long		dma_rd, dma_wr;
	u_long		dma[TEMP_DMA_SIZE];
	int		aal5, i;
	long		j;
	u_long		dma_avail;
	u_long		dma_start;
	Eni_mem		tx_send;
	u_long		*up;
	KBuffer		*m0 = m, *m1, *mprev = NULL;
	caddr_t		cp, bfr;
	u_int		len, align;
	int		compressed = 0;

#ifdef	DIAGNOSTIC
	if ( eni_pdu_print )
		atm_dev_pdu_print ( cup, cvp, m, "eni output" );
#endif

	/*
	 * Re-entry point for after buffer compression (if needed)
	 */
retry:

	/*
	 * We can avoid traversing the buffer list twice by building
	 * the middle (minus header and trailer) dma list at the
	 * same time we massage address and size alignments. Since
	 * this list remains local until we determine we've enough
	 * room, we're not going to trash anything by not checking
	 * sizes, etc. yet. Skip first entry to be used later to skip
	 * descriptor word.
	 */
	j = 2;
	/*
	 * Do data positioning for address and length alignment
	 */
	while ( m ) {
		u_long	buf_addr;	/* For passing addr to eni_set_dma() */

		/*
		 * Get rid of any zero length buffers
		 */
		if ( KB_LEN ( m ) == 0 ) {
			if ( mprev ) {
				KB_UNLINK ( m, mprev, m1 );
			} else {
				KB_UNLINKHEAD ( m, m1 );
				m0 = m1;
			}
			m = m1;
			continue;
		}
		/*
		 * Get start of data onto full-word alignment
		 */
		KB_DATASTART ( m, cp, caddr_t );
		if ((align = ((u_int)cp) & (sizeof(u_long)-1)) != 0) {
			/*
			 * Gotta slide the data up
			 */
			eup->eu_stats.eni_st_drv.drv_xm_segnoal++;
			bfr = cp - align;
			KM_COPY ( cp, bfr, KB_LEN ( m ) );
			KB_HEADMOVE ( m, -align );
		} else {
			/*
			 * Data already aligned
			 */
			bfr = cp;
		}
		/*
		 * Now work on getting the data length correct
		 */
		len = KB_LEN ( m );
		while ( ( align = ( len & (sizeof(u_long)-1))) &&
			(m1 = KB_NEXT ( m ) ) ) {

			/*
			 * Have to move some data from following buffer(s)
			 * to word-fill this buffer
			 */
			u_int ncopy = MIN ( sizeof(u_long) - align,
				KB_LEN ( m1 ) );

			if ( ncopy ) {
				/*
				 * Move data to current buffer
				 */
				caddr_t	dest;

				eup->eu_stats.eni_st_drv.drv_xm_seglen++;
				KB_DATASTART ( m1, cp, caddr_t );
				dest = bfr + len;
				KB_HEADADJ ( m1, -ncopy );
				KB_TAILADJ ( m, ncopy );
				len += ncopy;
				while ( ncopy-- ) {
					*dest++ = *cp++;
				}
			}

			/*
			 * If we've drained the buffer, free it
			 */
			if ( KB_LEN ( m1 ) == 0 ) {
				KBuffer *m2;

				KB_UNLINK ( m1, m, m2 );
			}
		}

		/*
		 * Address and size are now aligned. Build dma list
		 * using TX channel 0. Also, round length up to a word
		 * size which should only effect the last buffer in the
		 * chain. This works because the PDU length is maintained
		 * seperately and we're not really adjusting the buffer's
		 * idea of its length.
		 */
		KB_DATASTART ( m, buf_addr, u_long );
		if ( eni_set_dma ( eup, 0, dma, TEMP_DMA_SIZE, &j, 0,
		    buf_addr, KB_LEN ( m ) ) < 0 ) {
			/*
			 * Failed to build DMA list. First, we'll try to
			 * compress the buffer chain into a smaller number
			 * of buffers. After compressing, we'll try to send
			 * the new buffer chain. If we still fail, then
			 * we'll drop the pdu.
			 */
			if ( compressed ) {
#ifdef	DO_LOG
				log ( LOG_ERR,
					"eni_output: eni_set_dma failed\n" );
#endif
				eup->eu_pif.pif_oerrors++;
				KB_FREEALL ( m0 );
				return;
			}

			eup->eu_stats.eni_st_drv.drv_xm_maxpdu++;

			m = atm_dev_compress ( m0 );
			if ( m == NULL ) {
#ifdef	DO_LOG
				log ( LOG_ERR,
				    "eni_output: atm_dev_compress() failed\n" );
#endif
				eup->eu_pif.pif_oerrors++;
				return;
			}

			/*
			 * Reset to new head of buffer chain
			 */
			m0 = m;

			/*
			 * Indicate we've been through here
			 */
			compressed = 1;

			/*
			 * Retry to build the DMA descriptors for the newly
			 *  compressed buffer chain
			 */
			goto retry;

		}

		/*
		 * Now count the length
		 */
		pdulen += KB_LEN ( m );

		/*
		 * Bump counters and get ready for next buffer
		 */
		mprev = m;
		m = KB_NEXT ( m );
	}

	/*
	 * Get a buffer to use in a private queue so that we can
	 * reclaim resources after the DMA has finished.
	 */
	KB_ALLOC ( m, ENI_SMALL_BSIZE, KB_F_NOWAIT, KB_T_DATA );
	if ( m ) {
		/*
		 * Link the PDU onto our new head
		 */
		KB_NEXT ( m ) = m0;
	} else {
		/*
		 * Drop this PDU and let the sender try again.
		 */
		eup->eu_stats.eni_st_drv.drv_xm_norsc++;
#ifdef	DO_LOG
		log(LOG_ERR, "eni_output: Unable to allocate drain buffer.\n");
#endif
		eup->eu_pif.pif_oerrors++;
		KB_FREEALL ( m0 );
		return;
	}

	s = splnet();

	/*
	 * Calculate size of buffer necessary to store PDU. If this
	 * is an AAL5 PDU, we'll need to know where to stuff the length
	 * value in the trailer.
	 */
	/*
	 * AAL5 PDUs need an extra two words for control/length and
	 * CRC. Check for AAL5 and add requirements here.
	 */
	if ((aal5 = (evp->ev_connvc->cvc_attr.aal.type == ATM_AAL5)) != 0)
		size = pdulen + 2 * sizeof(long);
	else
		size = pdulen;
	/*
	 * Pad to next complete cell boundary
	 */
	size += (BYTES_PER_CELL - 1);
	size -= size % BYTES_PER_CELL;
	/*
	 * Convert size to words and add 2 words overhead for every
	 * PDU (descriptor and cell header).
	 */
	size = (size >> 2) + 2;

	/*
	 * First, check to see if there's enough buffer space to
	 * store the PDU. We do this by checking to see if the size
	 * required crosses the eu_txfirst pointer.  However, we don't
	 * want to exactly fill the buffer, because we won't be able to
	 * distinguish between a full and empty buffer.
	 */
	if ( eup->eu_txpos == eup->eu_txfirst )
		buf_avail = eup->eu_txsize;
	else
	    if ( eup->eu_txpos > eup->eu_txfirst )
		buf_avail = eup->eu_txsize - ( eup->eu_txpos - eup->eu_txfirst );
	    else
		buf_avail = eup->eu_txfirst - eup->eu_txpos;

	if ( size >= buf_avail )
	{
		/*
		 * No buffer space in the adapter to store this PDU.
		 * Drop PDU and return.
		 */
		eup->eu_stats.eni_st_drv.drv_xm_nobuf++;
#ifdef	DO_LOG
		log ( LOG_ERR,
			"eni_output: not enough room in buffer\n" );
#endif
		eup->eu_pif.pif_oerrors++;
	 	KB_FREEALL ( m );
		(void) splx(s);
		return;
	}

	/*
	 * Find out where current DMA pointers are at
	 */
	dma_start = dma_wr = eup->eu_midway[MIDWAY_TX_WR];
	dma_rd = eup->eu_midway[MIDWAY_TX_RD];

	/*
	 * Figure out how much DMA room we have available
	 */
	if ( dma_rd == dma_wr )	{		/* Queue is empty */
		dma_avail = DMA_LIST_SIZE;
	} else {
		dma_avail = ( dma_rd + DMA_LIST_SIZE - dma_wr )
		    & ( DMA_LIST_SIZE - 1 );
	}
	/*
	 * Check to see if we can describe this PDU or if we're:
	 * out of room, will wrap past recovered resources.
	 */
	if ( dma_avail < (j / 2 + 4) ||
	    ( dma_wr + (j / 2 + 4) > eup->eu_txdmawr + DMA_LIST_SIZE ) ) {
		/*
		 * No space to insert DMA list into queue. Drop this PDU.
		 */
		eup->eu_stats.eni_st_drv.drv_xm_nodma++;
#ifdef	DO_LOG
		log ( LOG_ERR,
			"eni_output: not enough room in DMA queue\n" );
#endif
		eup->eu_pif.pif_oerrors++;
		KB_FREEALL( m );
		(void) splx(s);
		return;
	}

	/*
	 * Create DMA descriptor for header. There is a descriptor word
	 * and also a cell header word which we'll set manually.
	 */
	dma[0] = (((int)(eup->eu_txpos + 2) & (eup->eu_txsize-1)) <<
	    DMA_COUNT_SHIFT) | DMA_JK;
	dma[1] = 0;

	/*
	 * JK for AAL5 trailer. Set END bit as well.
	 */
	if ( aal5 ) {
	    dma[j++] = (((int)(eup->eu_txpos+size) & (eup->eu_txsize-1)) <<
		DMA_COUNT_SHIFT) | DMA_END_BIT | DMA_JK;
	    dma[j++] = 0;
	} else {
		dma[j-2] |= DMA_END_BIT;	/* Backup and set END bit */
	}

	/*
	 * Find out where in adapter memory this TX buffer starts.
	 */
	tx_send = (Eni_mem)
	    ((((int)eup->eu_midway[MIDWAY_TXPLACE] & 0x7ff) << ENI_LOC_PREDIV) +
		    (int)eup->eu_ram);

	/*
	 * Set descriptor word
	 */
	tx_send[eup->eu_txpos] =
		(MIDWAY_UNQ_ID << 28) | (aal5 ? 1 << 27 : 0)
			| (size / WORDS_PER_CELL);
	/*
	 * Set cell header
	 */
	tx_send[(eup->eu_txpos+1)&(eup->eu_txsize-1)] = 
		evp->ev_connvc->cvc_vcc->vc_vci << 4;

	/*
	 * We've got all our resources, count the stats
	 */
	if ( aal5 ) {
		/*
		 * If this is an AAL5 PDU, we need to set the length
		 */
		tx_send[(eup->eu_txpos+size-2) &
			(eup->eu_txsize-1)] = pdulen;
		/*
		 * Increment AAL5 stats
		 */
		eup->eu_stats.eni_st_aal5.aal5_pdu_xmit++;
		eup->eu_stats.eni_st_aal5.aal5_xmit += (size - 2) / WORDS_PER_CELL;
	} else {
		/*
		 * Increment AAL0 stats
		 */
		eup->eu_stats.eni_st_aal0.aal0_xmit += (size - 2) / WORDS_PER_CELL;
	}
	/*
	 * Increment ATM stats
	 */
	eup->eu_stats.eni_st_atm.atm_xmit += (size - 2) / WORDS_PER_CELL;

	/*
	 * Store the DMA list
	 */
	j = j >> 1;
	for ( i = 0; i < j; i++ ) {
		eup->eu_txdma[dma_wr*2] = dma[i*2];
		eup->eu_txdma[dma_wr*2+1] = dma[i*2+1];
		dma_wr = (dma_wr+1) & (DMA_LIST_SIZE-1);
	}

	/*
	 * Build drain buffer
	 *
	 * We toss four words in to help keep track of this
	 * PDU. The first is a pointer to the VC control block
	 * so we can find which VCI this went out on, the second
	 * is the start and stop pointers for the DMA list which
	 * describes this PDU, the third is the PDU length
	 * since we'll want to know that for stats gathering,
	 * and the fourth is the number of DMA words.
	 */
	KB_DATASTART ( m, up, u_long * );
	*up++ = (u_long)cvp;
	*up++ = dma_start << 16 | dma_wr;
	*up++ = pdulen;
	*up = size;

	/*
	 * Set length of our buffer
	 */
	KB_LEN ( m ) = 4 * sizeof ( long );

	/*
	 * Place buffers onto transmit queue for draining
	 */
	s2 = splimp();
	IF_ENQUEUE ( &eup->eu_txqueue, m );
	(void) splx(s2);

	/*
	 * Update next word to be stored
	 */
	eup->eu_txpos = ((eup->eu_txpos + size) & (eup->eu_txsize - 1));

	/*
	 * Update MIDWAY_TX_WR pointer
	 */
	eup->eu_midway[MIDWAY_TX_WR] = dma_wr;
	
	(void) splx ( s );

	return;
}

