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
 * ATM socket protocol definitions
 *
 */

#ifndef _NETATM_ATM_PCB_H
#define _NETATM_ATM_PCB_H


#ifdef ATM_KERNEL
/*
 * ATM Socket PCB
 *
 * Common structure for all ATM protocol sockets.  This control block
 * will be used for all ATM socket types.
 */
struct atm_pcb {
	struct socket	*atp_socket;	/* Socket */
	Atm_connection	*atp_conn;	/* Connection manager token */
	u_char		atp_type;	/* Protocol type (see below) */
	u_char		atp_flags;	/* Protocol flags (see below) */
	Atm_attributes	atp_attr;	/* Socket's call attributes */
	char		atp_name[T_ATM_APP_NAME_LEN];	/* Owner's name */
};
typedef struct atm_pcb	Atm_pcb;

/*
 * Protocol Types
 */
#define	ATPT_AAL5	0		/* AAL type 5 protocol */
#define	ATPT_SSCOP	1		/* SSCOP protocol */
#define	ATPT_NUM	2		/* Number of protocols */

/*
 * PCB Flags
 */


/*
 * Handy macros
 */
#define sotoatmpcb(so)   ((Atm_pcb *)(so)->so_pcb)


/*
 * ATM Socket Statistics
 */
struct atm_sock_stat {
	u_long	as_connreq[ATPT_NUM];	/* Connection requests */
	u_long	as_inconn[ATPT_NUM];	/* Incoming connection requests */
	u_long	as_conncomp[ATPT_NUM];	/* Connections completed */
	u_long	as_connfail[ATPT_NUM];	/* Connections failed */
	u_long	as_connrel[ATPT_NUM];	/* Connections released */
	u_long	as_connclr[ATPT_NUM];	/* Connections cleared */
	u_long	as_indrop[ATPT_NUM];	/* Input packets dropped */
	u_long	as_outdrop[ATPT_NUM];	/* Output packets dropped */
};
#endif	/* ATM_KERNEL */

#endif	/* _NETATM_ATM_PCB_H */
