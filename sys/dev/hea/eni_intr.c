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
 *	@(#) $FreeBSD: src/sys/dev/hea/eni_intr.c,v 1.4 1999/08/28 00:41:44 peter Exp $
 *
 */

/*
 * Efficient ENI Adapter Support
 * -----------------------------
 *
 * Interrupt processing
 *
 */

#include <netatm/kern_include.h>

#include <dev/hea/eni_stats.h>
#include <dev/hea/eni.h>
#include <dev/hea/eni_suni.h>
#include <dev/hea/eni_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD: src/sys/dev/hea/eni_intr.c,v 1.4 1999/08/28 00:41:44 peter Exp $");
#endif

static void	eni_suni_intr __P((Eni_unit *));

/*
 * SUNI Interrupt processing
 *
 * Currently, we don't do anything more then clear the interrupt
 * for the SUNI chip.
 *
 * Arguments:
 *	eup		pointer to device unit structure
 *
 * Returns:
 *	none
 *
 */
static void
eni_suni_intr ( eup )
	Eni_unit *eup;
{
	int	SuniInt;
	int	val;

	SuniInt = eup->eu_suni[SUNI_IS_REG];
	
	/* RSOPI */
	if ( SuniInt & SUNI_RSOPI )
		val = eup->eu_suni[SUNI_RSOP_REG];

	/* RLOPI */
	if ( SuniInt & SUNI_RLOPI )
		val = eup->eu_suni[SUNI_RLOP_REG];

	/* RPOPI */
	if ( SuniInt & SUNI_RPOPI )
		val = eup->eu_suni[SUNI_RPOP_IS_REG];

	/* RACPI */
	if ( SuniInt & SUNI_RACPI )
		val = eup->eu_suni[SUNI_RACP_REG];

	/* TACPI */
	if ( SuniInt & SUNI_TACPI )
		val = eup->eu_suni[SUNI_TACP_REG];

	/* TROOLI */
	if ( SuniInt & SUNI_TROOLI )
		val = eup->eu_suni[SUNI_CLOCK_REG];

	/* LCDI */
		/* Cleared when reading Master Interrupt Status Reg */

	/* RDOOLI */
	if ( SuniInt & SUNI_RDOOLI )
		val = eup->eu_suni[SUNI_CLOCK_REG];

	return;
}

/*
 * Device interrupt routine
 *
 * Service an interrupt from this device
 *
 * Arguments:
 *	eup		pointer to device unit structure
 *
 * Returns:
 *	none
 *
 */
#if defined(BSD) && BSD < 199506
int
#else
void
#endif
eni_intr ( arg )
	void *arg;
{
	Eni_unit	*eup = (Eni_unit *)arg;
#if defined(BSD) && BSD < 199506
	int		serviced = 1;
#endif	/* BSD < 199506 */

	/*
	 * Read and acknowledge any interrupts
	 */
	u_long	mask = eup->eu_midway[MIDWAY_ISA];
	/*
	 * Read the error statistics counter
	 */
	u_long	sval = eup->eu_midway[MIDWAY_STAT];

	/*
	 * Update statistics from adapter
	 */
	eup->eu_trash += ( sval >> 16 );
	eup->eu_ovfl += ( sval & 0xffff );

	/*
	 * We handle any DMA completes first so
	 * that we can free resources for use
	 * during transmit and especially receive
	 */
	/*
	 * Handle RX DMA Complete
	 */
	if ( mask & ENI_INT_RX_DMA ) {
		eni_recv_drain ( eup );
	}

	/*
	 * Handle TX DMA Complete
	 */
	if ( mask & ENI_INT_TX_DMA ) {
		eni_xmit_drain ( eup );
	}

	/*
	 * Look for any PDUs in service list
	 */
	if ( mask & ENI_INT_SERVICE ) {
		eni_do_service ( eup );
	}

	/*
	 * Handle miscelaneous interrupts
	 */
	if ( mask & ENI_INT_STAT ) {			/* STAT_OVFL */
		log ( LOG_INFO, "eni_intr: stat_ovfl: 0x%lx\n", sval );
	}
	if ( mask & ENI_INT_SUNI ) {			/* SUNI_INTR */
		eni_suni_intr ( eup );
	}
	if ( mask & ENI_INT_DMA_ERR ) {			/* DMA Error */
		log ( LOG_ERR,
			"eni_intr: DMA Error\n" );
		/*
		 * We don't know how to recover from DMA errors
		 * yet. The adapter has disabled any further
		 * processing and we're going to leave it like
		 * that.
		 */
#if defined(BSD) && BSD < 199506
		return serviced;			/* Leave now */
#else
		return;					/* Leave now */
#endif
	}
	if ( mask & ENI_INT_IDEN ) {
		log ( LOG_ERR,
			"eni_intr: TX DMA Ident mismatch\n" );
		/*
		 * Something in the TX buffer has really gotten messed
		 * up. Since this is most likely a driver bug, and
		 * the adapter has shut everything down, leave it
		 * like that.
		 */
#if BSD < 199506
		return 0;				/* Leave now */
#else
		return;					/* Leave now */
#endif
	}
	if ( mask & ENI_INT_DMA_OVFL )
		eup->eu_stats.eni_st_drv.drv_xm_dmaovfl++;
	if ( mask & ENI_INT_DMA_LERR ) {
		log ( LOG_ERR,
			"eni_intr: DMA LERR\n" );
#if BSD < 199506
		return 0;
#else
		return;
#endif
	}

#if BSD < 199506
	return 0;
#else
	return;
#endif
}

