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
 * Efficient ENI Adapter Support
 * -----------------------------
 *
 * Driver initialization support
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>

#include <dev/hea/eni_stats.h>
#include <dev/hea/eni.h>
#include <dev/hea/eni_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif

/*
 * Initialize adapter for PDU processing
 *
 * Enable interrupts, set master control, initialize TX buffer,
 * set initial pointers, etc.
 *
 * Arguments:
 *	eup		pointer to device unit structure
 *
 * Returns:
 *	0		successful
 *	error		error condition
 */
int
eni_init ( eup )
	Eni_unit *eup;
{
	u_long	words, order;

	/*
	 * Allocate one large TX buffer. Currently we use only one
	 * channel with full cell rate which all VCs will use.
	 * This will (probably) have to change (alot) when we
	 * implement QoS.
	 */
	/*
	 * Server cards, which have more then 512KB of RAM, will
	 * allocate a 128KB TX buffer, while client cards, with
	 * 512KB or less will allocate a 32KB TX buffer.
	 */
	words = ( eup->eu_ramsize > MAX_CLIENT_RAM * ENI_BUF_PGSZ ?
		TX_LARGE_BSIZE : TX_SMALL_BSIZE ) * ENI_BUF_PGSZ;
	if ( ( eup->eu_txbuf = eni_allocate_buffer ( eup, &words ) ) ==
	    (caddr_t)NULL ) {
		return ENOMEM;
	} 
	eup->eu_txsize = words >> 2;		/* Bytes to words */
	words >>= ENI_LOC_PREDIV;		/* Predivide by 256 words */
	for ( order = -1; words; order++ )
		words >>= 1;
	eup->eu_midway[MIDWAY_TXPLACE] =
	    (order << TXSIZE_SHIFT) | ((int)eup->eu_txbuf >> ENI_LOC_PREDIV);
	eup->eu_txpos = eup->eu_midway[MIDWAY_DESCR] & 0x7FFF;
	/*
	 * Set first word of unack'ed data to start
	 */
	eup->eu_txfirst = eup->eu_txpos;
	
	/*
	 * Set initial values of local DMA pointer used to prevent wraps
	 */
	eup->eu_txdmawr = 0;
	eup->eu_rxdmawr = 0;

	/*
	 * Initialize queue's for receive/transmit pdus
	 */
	eup->eu_txqueue.ifq_maxlen = ENI_IFQ_MAXLEN;
	eup->eu_rxqueue.ifq_maxlen = ENI_IFQ_MAXLEN;

	/*
	 * Acknowledge any interrupts
	 */
	(void) eup->eu_midway[MIDWAY_ISA];

	/*
	 * "Zero" Sonet error counters
	 */
	eni_zero_stats ( eup );

	/*
	 * Set master control register
	 *
	 * IntSel1 | LOCK_MODE | DMA_ENABLE | TX_ENABLE | RX_ENABLE
	 *
	 */
	eup->eu_midway[MIDWAY_MASTER] = 1 << ENI_ISEL_SHIFT |
	    ENI_M_DMAENABLE | ENI_M_TXENABLE | ENI_M_RXENABLE;

	/*
	 * Enable interrupts
	 */
	eup->eu_midway[MIDWAY_IE] = ENI_INT_SERVICE | ENI_INT_RX_DMA |
		ENI_INT_TX_DMA | ENI_INT_DMA_ERR | ENI_INT_DMA_LERR |
			ENI_INT_IDEN | ENI_INT_DMA_OVFL;

	/*
	 * Last thing to do is to indicate that we've finished initializing
	 * this unit.
	 */
	eup->eu_flags |= CUF_INITED;

	return 0;
}

