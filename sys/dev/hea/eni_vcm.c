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
 *	@(#) $FreeBSD: src/sys/dev/hea/eni_vcm.c,v 1.3 1999/08/28 00:41:47 peter Exp $
 *
 */

/*
 * Efficient ENI Adapter Support
 * -----------------------------
 *
 * Virtual Channel Managment
 *
 */


#include <netatm/kern_include.h>

#include <dev/hea/eni_stats.h>
#include <dev/hea/eni.h>
#include <dev/hea/eni_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD: src/sys/dev/hea/eni_vcm.c,v 1.3 1999/08/28 00:41:47 peter Exp $");
#endif


/*
 * VCC Stack Instantiation
 * 
 * This function is called via the common driver code during a device VCC
 * stack instantiation.  The common code has already validated some of
 * the request so we just need to check a few more ENI-specific details.
 *
 * Called at splnet.
 *
 * Arguments:
 *	cup	pointer to device common unit
 *	cvp	pointer to common VCC entry
 *
 * Returns:
 *	0	instantiation successful
 *	err 	instantiation failed - reason indicated
 *
 */
int
eni_instvcc(cup, cvp)
	Cmn_unit	*cup;
	Cmn_vcc		*cvp;
{
	Eni_unit	*eup = (Eni_unit *)cup;
	Eni_vcc		*evp = (Eni_vcc *)cvp;
	Atm_attributes	*ap = &evp->ev_connvc->cvc_attr;

	/*
	 * Validate requested AAL
	 */
	switch (ap->aal.type) {

	case ATM_AAL0:
		break;

	case ATM_AAL5:
		if ((ap->aal.v.aal5.forward_max_SDU_size > ENI_IFF_MTU) ||
		    (ap->aal.v.aal5.backward_max_SDU_size > ENI_IFF_MTU)) {
			eup->eu_stats.eni_st_drv.drv_vc_maxpdu++;
			return (EINVAL);
		}
		break;

	default:
		return (EINVAL);
	}

	return (0);
}


/*
 * Open a VCC
 *
 * This function is called via the common driver code after receiving a
 * stack *_INIT* command. The common code has already validated most of
 * the request so we just need to check a few more ENI-specific details.
 *
 * Called at splimp.
 *
 * Arguments:
 *	cup	pointer to device common unit
 *	cvp	pointer to common VCC entry
 *
 * Returns:
 *	0	open sucessful
 *	err	open failed
 *
 */
int
eni_openvcc ( cup, cvp )
	Cmn_unit	*cup;
	Cmn_vcc		*cvp;
{
	Eni_unit	*eup = (Eni_unit *)cup;
	Eni_vcc		*evp = (Eni_vcc *)cvp;
	struct vccb	*vcp = evp->ev_connvc->cvc_vcc;
	int		err = 0;

	VCI_Table	*vct;
	int		size;
	int		mode;
	int		nsize;

	/*
	 * Validate the VPI and VCI values
	 */
	if ( (vcp->vc_vpi > eup->eu_pif.pif_maxvpi) ||
	     (vcp->vc_vci > eup->eu_pif.pif_maxvci) ) {
		eup->eu_stats.eni_st_drv.drv_vc_badrng++;
		return ( EFAULT );
	}

	/*
	 * Check if this VCI is already active
	 */
	vct = &eup->eu_vcitbl[ vcp->vc_vci ];
	if ( vct->vci_control >> VCI_MODE_SHIFT != VCI_MODE_TRASH ) {
		return ( EEXIST );
	}

	/*
	 * Allocate some permanent adapter memory for the reassembly
	 * buffer. Special case the signalling channel(s) buffer size.
	 * Otherwise, the buffer size will be based on whether this is
	 * a server or client card.
	 */
	if ( vcp->vc_vci == UNI_SIG_VCI )	/* HACK */
		size = RX_SIG_BSIZE;
	else
		size = (eup->eu_ramsize > MAX_CLIENT_RAM * ENI_BUF_PGSZ) ?
			RX_SERVER_BSIZE * ENI_BUF_PGSZ :
			RX_CLIENT_BSIZE * ENI_BUF_PGSZ;

	if ( ( evp->ev_rxbuf = eni_allocate_buffer ( eup, (u_long *)&size ) )
	    == (caddr_t)NULL ) {
		return ( ENOMEM );
	}
	evp->ev_rxpos = 0;

	/*
	 * We only need to open incoming VCI's so outbound VCI's
	 * just get set to CVS_ACTIVE state.
	 */
	if ( ( vcp->vc_type & VCC_IN ) == 0 ) {
		/*
		 * Set the state and return - nothing else needs to be done.
		 */
		evp->ev_state = CVS_ACTIVE;
		return ( 0 );
	}

	/*
	 * Set the VCI Table entry to start receiving
	 */
	mode = ( evp->ev_connvc->cvc_attr.aal.type == ATM_AAL5
		? VCI_MODE_AAL5 : VCI_MODE_AAL0 );
	size >>= ENI_LOC_PREDIV;	/* Predivide by 256 WORDS */
	for ( nsize = -1; size; nsize++ )
		size >>= 1;

	vct->vci_control = mode << VCI_MODE_SHIFT |
	    PTI_MODE_TRASH << VCI_PTI_SHIFT |
	        ( (u_int)(evp->ev_rxbuf) >> ENI_LOC_PREDIV ) << VCI_LOC_SHIFT |
		nsize << VCI_SIZE_SHIFT;
	vct->vci_descr = 0;		/* Descr = Rdptr = 0 */
	vct->vci_write = 0;		/* WritePtr = CellCount = 0 */

	/*
	 * Indicate VC active
	 */
	evp->ev_state = CVS_ACTIVE;

	return ( err );
}

/*
 * Close a VCC
 *
 * This function is called via the common driver code after receiving a
 * stack *_TERM* command. The common code has already validated most of
 * the request so we just need to check a few more ENI-specific details.
 *
 * Called at splimp.
 *
 * Arguments:
 *	cup	pointer to device common unit
 *	cvp	pointer to common VCC entry
 *
 * Returns:
 *	0	close sucessful
 *	err	close failed
 *
 */
int
eni_closevcc ( cup, cvp )
	Cmn_unit	*cup;
	Cmn_vcc		*cvp;
{
	Eni_unit	*eup = (Eni_unit *)cup;
	Eni_vcc		*evp = (Eni_vcc *)cvp;
	struct vccb	*vcp = evp->ev_connvc->cvc_vcc;
	int		err = 0;

	VCI_Table	*vct;

	/*
	 * Clear any references to this VCC in our transmit queue
	 */
	/*
	 * We'll simply allow any existing TX requests to be
	 * sent as that's easier then pulling them out of
	 * everywhere. Besides, they should be ignored at the
	 * receiver whenever the other end shuts down.
	 */

	/*
	 * Free the adapter receive buffer
	 */
	(void) eni_free_buffer ( eup, (caddr_t)evp->ev_rxbuf );

	/*
	 * If this is an outbound only VCI, then we can close
	 * immediately.
	 */
	if ( ( vcp->vc_type & VCC_IN ) == 0 ) {
		/*
		 * The state will be set to TERM when we return
		 * to the *_TERM caller.
		 */
		return ( 0 );
	}

	/*
	 * Find VCI entry in VCI Table
	 */
	vct = &eup->eu_vcitbl[ vcp->vc_vci ];

	/*
	 * Reset the VCI state
	 */
	vct->vci_control = ( vct->vci_control & VCI_MODE_MASK )
		/* | VCI_MODE_TRASH */;
	DELAY ( MIDWAY_DELAY );			/* Give the adapter time to */
					/* make the transition */

	/*
	 * Reset everything
	 */
	KM_ZERO ( (caddr_t)vct, sizeof(VCI_Table) );

	return ( err );
}

