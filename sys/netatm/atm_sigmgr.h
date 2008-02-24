/*-
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
 *	@(#) $FreeBSD: src/sys/netatm/atm_sigmgr.h,v 1.5 2005/01/07 01:45:36 imp Exp $
 *
 */

/*
 * Core ATM Services
 * -----------------
 *
 * ATM Signalling Manager definitions 
 *
 */

#ifndef _NETATM_ATM_SIGMGR_H
#define _NETATM_ATM_SIGMGR_H

#ifdef _KERNEL
/*
 * Structure common to all ATM Signalling Managers.  Each Signalling 
 * Manager must create one of these and use it to register itself 
 * with the system.
 */
struct	sigmgr {
	struct sigmgr	*sm_next;	/* Next registered sigmgr */
	u_char		sm_proto;	/* Signalling protocol (see below) */
	struct siginst	*sm_prinst;	/* List of protocol instances */
/* Exported functions */
	int		(*sm_attach)	/* Attach interface */
				(struct sigmgr *, struct atm_pif *);
	int		(*sm_detach)	/* Detach interface */
				(struct atm_pif *);
	int		(*sm_setup)	/* Connection setup */
				(Atm_connvc *, int *);
	int		(*sm_accept)	/* Call accepted */
				(struct vccb *, int *);
	int		(*sm_reject)	/* Call rejected */
				(struct vccb *, int *);
	int		(*sm_release)	/* Connection release */
				(struct vccb *, int *);
	int		(*sm_free)	/* Free connection resources */
				(struct vccb *);
	int		(*sm_ioctl)	/* Ioctl handler */
				(int, caddr_t, caddr_t);
};
#endif	/* _KERNEL */

/* 
 * ATM Signalling Protocols
 */
#define	ATM_SIG_PVC	1		/* PVC-only */
#define	ATM_SIG_SPANS	2		/* Fore Systems SPANS */
#define	ATM_SIG_UNI30	3		/* ATM Forum UNI 3.0 */
#define	ATM_SIG_UNI31	4		/* ATM Forum UNI 3.1 */
#define	ATM_SIG_UNI40	5		/* ATM Forum UNI 4.0 */


#ifdef _KERNEL
/*
 * Signalling Protocol Instance control block header.  Common header for
 * every signalling protocol instance control block.
 */
struct	siginst {
	struct siginst	*si_next;	/* Next sigmgr protocol instance */
	struct atm_pif	*si_pif;	/* Device interface */
	Atm_addr	si_addr;	/* Interface ATM address */
	Atm_addr	si_subaddr;	/* Interface ATM subaddress */
	Queue_t		si_vccq;	/* VCCB queue */
	u_short		si_state;	/* Protocol state (sigmgr specific) */

/* Exported protocol services */
	struct ip_serv	*si_ipserv;	/* IP/ATM services */
};


/*
 * Sigmgr function return codes
 */
#define	CALL_PROCEEDING	1		/* Connection request is in progress */
#define	CALL_FAILED	2		/* Connection request failed */
#define	CALL_CONNECTED	3		/* Connection setup successful */
#define	CALL_CLEARED	4		/* Connection has been terminated */

#endif	/* _KERNEL */

#endif	/* _NETATM_ATM_SIGMGR_H */
