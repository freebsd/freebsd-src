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
 * ATM Forum UNI 3.0/3.1 Signalling Manager
 * ----------------------------------------
 *
 * Message buffer formats
 *
 */

#ifndef _UNI_SIG_MBUF_H
#define _UNI_SIG_MBUF_H


/*
 * Structure for message encoding/decoding information.
 */
struct usfmt {
	KBuffer		*usf_m_addr;	/* Current buffer */
	KBuffer		*usf_m_base;	/* First buffer in chain */
	int		usf_loc;	/* Offset in current buffer */
	int		usf_op;		/* Operation (see below) */
	struct unisig	*usf_sig;	/* UNI signalling instance */
};

#define	USF_ENCODE	1
#define	USF_DECODE	2

#define	USF_MIN_ALLOC	MHLEN		/* Minimum encoding buffer size */

#endif	/* _UNI_SIG_MBUF_H */
