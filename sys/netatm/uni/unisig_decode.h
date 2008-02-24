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
 *	@(#) $FreeBSD: src/sys/netatm/uni/unisig_decode.h,v 1.5 2007/06/23 00:02:20 mjacob Exp $
 *
 */

/*
 * ATM Forum UNI 3.0/3.1 Signalling Manager
 * ----------------------------------------
 *
 * Message formats
 *
 */

#ifndef _UNI_SIG_DECODE_H
#define _UNI_SIG_DECODE_H


/*
 * Values specifying which IEs are required in messages
 */
#define	IE_NA	0
#define	IE_MAND	1
#define	IE_OPT	2

/*
 * Structure for information element decoding information
 */
struct ie_ent {
	u_char		ident;		/* IE identifier */
	int		min_len;	/* Min. length */
	int		max_len;	/* Max. length */
	int		p_idx;		/* IE pointer index in msg */
	int		(*decode)	/* Decoding function */
				(struct usfmt *, struct ie_generic *);
};

#define IE_OFF_SIZE(f)   \
	offsetof(struct ie_generic, f), (sizeof(((struct ie_generic *) 0)->f))


/*
 * Structure to define a field-driven decoding table (for AAL
 * parameters and ATM user cell rate IEs)
 */
struct ie_decode_tbl {
	u_char	ident;
	int	len;
	int	f_offs;
	int	f_size;
};

#endif /* _UNI_SIG_DECODE_H */
