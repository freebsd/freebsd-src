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
 *	@(#) $FreeBSD: src/sys/netatm/atm_sap.h,v 1.2 1999/08/28 00:48:37 peter Exp $
 *
 */

/*
 * Core ATM Services
 * -----------------
 *
 * ATM Services definitions
 *
 */

#ifndef _NETATM_ATM_SAP_H
#define _NETATM_ATM_SAP_H

/*
 * Service Access Point (SAP)
 *
 * A SAP specifies the definition of an interface used between two adjacent
 * layers.  The SAP is named for the services the lower layer provides to 
 * the upper layer.
 *
 * The types of SAPs used are:
 * 	Stack - defines the interfaces between stack service entities.
 *		These are further divided into:
 *
 *		Stack class SAP - which identifies the type of interface
 *			used.  All SAPs of a particular class will provide
 *			the same interface services to the higher layer.
 *			All stack command codes are constructed using class
 *			SAP values.
 *
 *		Stack instance SAP - which identifies the specific identity
 *			of the layer providing the class interface.
 */
typedef u_short	Sap_t;

#define	SAP_TYPE_MASK	0xc000
#define	SAP_TYPE_STACK	0x8000
#define	SAP_CLASS_MASK	0xff80

#define	SAP_STACK(c, i)		(SAP_TYPE_STACK | ((c) << 7) | (i))

/* Stack SAPs */
#define	SAP_ATM			SAP_STACK(1, 0)		/* ATM cell */
#define	SAP_SAR			SAP_STACK(2, 0)		/* AAL SAR */
#define	SAP_SAR_AAL3_4		SAP_STACK(2, 3)		/* AAL3/4 SAR */
#define	SAP_SAR_AAL5		SAP_STACK(2, 5)		/* AAL5 SAR */
#define	SAP_CPCS		SAP_STACK(3, 0)		/* AAL CPCS */
#define	SAP_CPCS_AAL3_4		SAP_STACK(3, 3)		/* AAL3/4 CPCS */
#define	SAP_CPCS_AAL5		SAP_STACK(3, 5)		/* AAL5 CPCS */
#define	SAP_SSCOP		SAP_STACK(4, 0)		/* ITU Q.2110 */
#define	SAP_SSCF_UNI		SAP_STACK(5, 0)		/* ITU Q.2130 */
#define	SAP_SSCF_NNI		SAP_STACK(6, 0)		/* ITU Q.2140 */

#endif	/* _NETATM_ATM_SAP_H */
