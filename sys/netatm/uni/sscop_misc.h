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
 * ATM Forum UNI Support
 * ---------------------
 *
 * SSCOP miscellaneous definitions
 *
 */

#ifndef _UNI_SSCOP_MISC_H
#define _UNI_SSCOP_MISC_H

/*
 * SSCOP command definitions
 */
#define	SSCOP_CMD_MIN	SSCOP_INIT	/* Minimum SSCOP CMD value */
#define	SSCOP_CMD_MAX	SSCOP_RETRIEVECMP_IND	/* Maximum SSCOP CMD value */
#define	SSCOP_CMD_SIZE	36		/* Size of command lookup table */


/*
 * Management Errors
 */
#define	MAA_ERROR_MIN	'A'
#define	MAA_ERROR_MAX	'X'
#define	MAA_ERROR_INVAL	(MAA_ERROR_MAX + 1)
#define	MAA_ERROR_COUNT	(MAA_ERROR_MAX - MAA_ERROR_MIN + 2)


/*
 * SSCOP Sequence Numbers
 *
 * SSCOP sequence numbers are 24 bit integers using modulo arithmetic.
 * The macros below must be used to modify and compare such numbers.
 * Comparison of sequence numbers is always relative to some base number (b).
 */
typedef u_int     sscop_seq;

#define	SEQ_MOD		0xffffff
#define	SEQ_VAL(v)	((v) & SEQ_MOD)
#define	SEQ_SET(s,v)	((s) = SEQ_VAL(v))
#define	SEQ_ADD(s,v)	(SEQ_VAL((s) + (v)))
#define	SEQ_SUB(s,v)	(SEQ_VAL((s) - (v)))
#define	SEQ_INCR(s,v)	((s) = SEQ_VAL((s) + (v)))
#define	SEQ_DECR(s,v)	((s) = SEQ_VAL((s) - (v)))
#define	SEQ_EQ(x,y)	(SEQ_VAL(x) == SEQ_VAL(y))
#define	SEQ_NEQ(x,y)	(SEQ_VAL(x) != SEQ_VAL(y))
#define	SEQ_LT(x,y,b)	(SEQ_VAL((x) - (b)) < SEQ_VAL((y) - (b)))
#define	SEQ_LEQ(x,y,b)	(SEQ_VAL((x) - (b)) <= SEQ_VAL((y) - (b)))
#define	SEQ_GT(x,y,b)	(SEQ_VAL((x) - (b)) > SEQ_VAL((y) - (b)))
#define	SEQ_GEQ(x,y,b)	(SEQ_VAL((x) - (b)) >= SEQ_VAL((y) - (b)))


/*
 * SSCOP Timers
 *
 * All of the SSCOP timer fields are maintained in terms of clock ticks.
 * The timers tick 2 times per second.
 */
#define	SSCOP_HZ	2		/* SSCOP ticks per second */

#define	SSCOP_T_NUM	4		/* Number of timers per connection */

#define	SSCOP_T_POLL	0		/* Timer_POLL / Timer_KEEP-ALIVE */
#define	SSCOP_T_NORESP	1		/* Timer_NO-RESPONSE */
#define	SSCOP_T_CC	2		/* Timer_CC */
#define	SSCOP_T_IDLE	3		/* Timer_IDLE */

#endif	/* _UNI_SSCOP_MISC_H */
