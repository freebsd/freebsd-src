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
 * Core ATM Services
 * -----------------
 *
 * ATM Virtual Channel definitions 
 *
 */

#ifndef _NETATM_ATM_VC_H
#define _NETATM_ATM_VC_H


#ifdef _KERNEL
/*
 * ATM Virtual Channel Connection control block.  All vccb's are created
 * and controlled by an ATM signalling manager.  Each ATM signalling 
 * protocol will also have its own protocol-specific vccb format.  Each 
 * of these protocol vccb's must have this common block at the beginning.
 */
struct vccb {
	u_char		vc_type;	/* VCC type (see below) */
	u_char		vc_proto;	/* Signalling protocol */
	u_char		vc_sstate;	/* Signalling state (sigmgr specific) */
	u_char		vc_ustate;	/* User interface state (see below) */
	struct atm_pif	*vc_pif;	/* Physical interface */
	struct atm_nif	*vc_nif;	/* Network interface */
	Qelem_t		vc_sigelem;	/* Signalling instance vccb queue */
	struct atm_time	vc_time;	/* Timer controls */
	u_short		vc_vpi;		/* Virtual Path Identifier */
	u_short		vc_vci;		/* Virtual Channel Identifier */
	Atm_connvc	*vc_connvc;	/* CM connection VCC instance */
	long		vc_ipdus;	/* PDUs received from VCC */
	long		vc_opdus;	/* PDUs sent to VCC */
	long		vc_ibytes;	/* Bytes received from VCC */
	long		vc_obytes;	/* Bytes sent to VCC */
	long		vc_ierrors;	/* Errors receiving from VCC */
	long		vc_oerrors;	/* Errors sending to VCC */
	time_t		vc_tstamp;	/* State transition timestamp */
};
#endif	/* _KERNEL */

/*
 * VCC Types
 */
#define	VCC_PVC		0x01		/* PVC (Permanent Virtual Channel) */
#define	VCC_SVC		0x02		/* SVC (Switched Virtual Channel) */
#define	VCC_IN		0x04		/* Inbound VCC */
#define	VCC_OUT		0x08		/* Outbound VCC */

/*
 * VCC Signalling-to-User Interface States
 */
#define	VCCU_NULL	0		/* No state */
#define	VCCU_POPEN	1		/* Pending open completion */
#define	VCCU_OPEN	2		/* Connection is open */
#define	VCCU_CLOSED	3		/* Connection has been terminated */
#define	VCCU_ABORT	4		/* Connection being aborted */


#endif	/* _NETATM_ATM_VC_H */
