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
 * PVC-only Signalling Manager
 * ---------------------------
 *
 * Protocol control blocks
 *
 */

#ifndef _SIGPVC_SIGPVC_VAR_H
#define _SIGPVC_SIGPVC_VAR_H

#ifdef ATM_KERNEL
/*
 * Structure containing state information for each SigPVC protocol instance.
 * There will be one instance for each ATM device interface using the SigPVC
 * signalling manager.
 */
struct	sigpvc {
	struct siginst	pv_inst;	/* Common header */
};
#define	pv_next		pv_inst.si_next
#define	pv_pif		pv_inst.si_pif
#define	pv_addr		pv_inst.si_addr
#define	pv_vccq		pv_inst.si_vccq
#define	pv_state	pv_inst.si_state
#endif	/* ATM_KERNEL */

/*
 * SigPVC Protocol States
 */
#define	SIGPVC_ACTIVE	1		/* Active */
#define	SIGPVC_DETACH	2		/* Detach in progress */


#ifdef ATM_KERNEL
/*
 * SigPVC Virtual Channel Connection control block.  All information regarding
 * the state of a SigPVC controlled VCC will be recorded here.  There will be
 * one SigPVC VCC control block for each SigPVC-controlled VCC.
 */
struct sigpvc_vccb {
	struct vccb	vcp_hdr;	/* Generic vccb */
};
#endif	/* ATM_KERNEL */

/*
 * SigPVC VCC Signalling Protocol States
 */
#define	VCCS_NULL	0		/* No state */
#define	VCCS_ACTIVE	1		/* Active */
#define	VCCS_FREE	2		/* Waiting for user to free resources */


#ifdef ATM_KERNEL
/*
 * Global function declarations
 */
	/* sigpvc_if.c */

	/* sigpvc_subr.c */
int		sigpvc_create_pvc __P((struct sigpvc *, Atm_connvc *, int *));
void		sigpvc_close_vcc __P((struct vccb *));

#endif	/* ATM_KERNEL */

#endif	/* _SIGPVC_SIGPVC_VAR_H */
