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
 *	@(#) $Id: sigpvc.h,v 1.2 1997/05/06 22:15:43 mks Exp $
 *
 */

/*
 * PVC-only Signalling Manager
 * ---------------------------
 *
 * Protocol definitions
 *
 */

#ifndef _SIGPVC_SIGPVC_H
#define _SIGPVC_SIGPVC_H

/*
 * Protocol Variables
 */
#define	SIGPVC_DOWN_DELAY	(15 * ATM_HZ)	/* Delay til i/f marked down */
#define	SIGPVC_UP_DELAY		(5 * ATM_HZ)	/* Delay til i/f marked up */

#endif	/* _SIGPVC_SIGPVC_H */
