/*
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
 */

/*
 * ATM Forum UNI 3.0/3.1 Signalling Manager
 * ----------------------------------------
 *
 * Message buffer handling routines
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/if.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_vc.h>
#include <netatm/atm_sigmgr.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#include <netatm/uni/unisig_var.h>
#include <netatm/uni/unisig_mbuf.h>
#include <netatm/uni/unisig_msg.h>

/*
 * Initialize a unisig formatting structure
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	usp	pointer to a unisig protocol instance
 *	buf	pointer to a buffer chain (decode only)
 *	op	operation code (encode or decode)
 *	headroom headroom to leave in first buffer
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
usf_init(usf, usp, buf, op, headroom)
	struct usfmt	*usf;
	struct unisig	*usp;
	KBuffer		*buf;
	int		op;
	int		headroom;
{
	KBuffer		*m;

	ATM_DEBUG3("usf_init: usf=%p, buf=%p, op=%d\n",
			usf, buf, op);

	/*
	 * Check parameters
	 */
	if (!usf)
		return(EINVAL);

	switch(op) {

	case USF_ENCODE:
		/*
		 * Get a buffer
		 */
		KB_ALLOCPKT(m, USF_MIN_ALLOC, KB_F_NOWAIT, KB_T_DATA);
		if (m == NULL)
			return(ENOMEM);
		KB_LEN(m) = 0;
		if (headroom < KB_BFRLEN(m)) {
			KB_HEADSET(m, headroom);
		}
		break;

	case USF_DECODE:
		/*
		 * Verify buffer address
		 */
		if (!buf)
			return(EINVAL);
		m = buf;
		break;

	default:
		return(EINVAL);
	}

	/*
	 * Save parameters in formatting structure
	 */
	usf->usf_m_addr = m;
	usf->usf_m_base = m;
	usf->usf_loc = 0;
	usf->usf_op = op;
	usf->usf_sig = usp;

	return(0);
}


/*
 * Get or put the next byte of a signalling message
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	c	pointer to the byte to send from or receive into
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
usf_byte(usf, c)
	struct usfmt	*usf;
	u_char		*c;
{
	u_char		*mp;
	KBuffer		*m = usf->usf_m_addr, *m1;
	int		space;

	switch (usf->usf_op) {

	case USF_DECODE:
		/*
		 * Make sure we're not past the end of the buffer
		 * (allowing for zero-length buffers)
		 */
		while (usf->usf_loc >= KB_LEN(m)) {
			if (KB_NEXT(usf->usf_m_addr)) {
				usf->usf_m_addr = m = KB_NEXT(usf->usf_m_addr);
				usf->usf_loc = 0;
			} else {
				return(EMSGSIZE);
			}
		}

		/*
		 * Get the data from the buffer
		 */
		KB_DATASTART(m, mp, u_char *);
		*c = mp[usf->usf_loc];
		usf->usf_loc++;
		break;

	case USF_ENCODE:
		/*
		 * If the current buffer is full, get another
		 */
		KB_TAILROOM(m, space);
		if (space == 0) {
			KB_ALLOC(m1, USF_MIN_ALLOC, KB_F_NOWAIT, KB_T_DATA);
			if (m1 == NULL)
				return(ENOMEM);
			KB_LEN(m1) = 0;
			KB_LINK(m1, m);
			usf->usf_m_addr = m = m1;
			usf->usf_loc = 0;
		}

		/*
		 * Put the data into the buffer
		 */
		KB_DATASTART(m, mp, u_char *);
		mp[usf->usf_loc] = *c;
		KB_TAILADJ(m, 1);
		usf->usf_loc++;
		break;

	default:
		/*
		 * Invalid operation code
		 */
		return(EINVAL);
	}

	return(0);

}

/*
 * Get or put a short integer
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	s	pointer to a short to send from or receive into
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
usf_short(usf, s)
	struct usfmt	*usf;
	u_short		*s;

{
	int	rc;
	union {
		u_short	value;
		u_char	b[sizeof(u_short)];
	} tval;

	tval.value = 0;
	if (usf->usf_op == USF_ENCODE)
		tval.value = htons(*s);

	if ((rc = usf_byte(usf, &tval.b[0])) != 0)
		return(rc);
	if ((rc = usf_byte(usf, &tval.b[1])) != 0)
		return(rc);

	if (usf->usf_op == USF_DECODE)
		*s = ntohs(tval.value);

	return(0);
}


/*
 * Get or put a 3-byte integer
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	i	pointer to an integer to send from or receive into
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
usf_int3(usf, i)
	struct usfmt	*usf;
	u_int		*i;

{
	int	j, rc;
	union {
		u_int	value;
		u_char	b[sizeof(u_int)];
	} tval;

	tval.value = 0;

	if (usf->usf_op == USF_ENCODE)
		tval.value = htonl(*i);

	for (j=0; j<3; j++) {
		rc = usf_byte(usf, &tval.b[j+sizeof(u_int)-3]);
		if (rc)
			return(rc);
	}

	if (usf->usf_op == USF_DECODE)
		*i = ntohl(tval.value);

	return(rc);
}


/*
 * Get or put an integer
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	i	pointer to an integer to send from or receive into
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
usf_int(usf, i)
	struct usfmt	*usf;
	u_int		*i;

{
	int	j, rc;
	union {
		u_int	value;
		u_char	b[sizeof(u_int)];
	} tval;

	if (usf->usf_op == USF_ENCODE)
		tval.value = htonl(*i);

	for (j=0; j<4; j++) {
		rc = usf_byte(usf, &tval.b[j+sizeof(u_int)-4]);
		if (rc)
			return(rc);
	}

	if (usf->usf_op == USF_DECODE)
		*i = ntohl(tval.value);

	return(rc);
}


/*
 * Get or put an extented field
 *
 * An extented field consists of a string of bytes.  All but the last
 * byte of the field has the high-order bit set to zero.  When decoding,
 * this routine will read bytes until either the input is exhausted or
 * a byte with a high-order one is found.  Whe encoding, it will take an
 * unsigned integer and write until the highest-order one bit has been
 * written.
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	i	pointer to an integer to send from or receive into
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
usf_ext(usf, i)
	struct usfmt	*usf;
	u_int		*i;

{
	int	j, rc;
	u_char	c, buff[sizeof(u_int)+1];
	u_int	val;
	union {
		u_int	value;
		u_char	b[sizeof(u_int)];
	} tval;

	switch(usf->usf_op) {

	case USF_ENCODE:
		val = *i;
		j = 0;
		while (val) {
			tval.value = htonl(val);
			buff[j] = tval.b[sizeof(u_int)-1] & UNI_IE_EXT_MASK;
			val >>= 7;
			j++;
		}
		j--;
		buff[0] |= UNI_IE_EXT_BIT;
		for (; j>=0; j--) {
			rc = usf_byte(usf, &buff[j]);
			if (rc)
				return(rc);
		}
		break;

	case USF_DECODE:
		c = 0;
		val = 0;
		while (!(c & UNI_IE_EXT_BIT)) {
			rc = usf_byte(usf, &c);
			if (rc)
				return(rc);
			val = (val << 7) + (c & UNI_IE_EXT_MASK);
		}
		*i = val;
		break;

	default:
		return(EINVAL);
	}

	return(0);
}


/*
 * Count the bytes remaining to be decoded
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *
 * Returns:
 *	int	the number of bytes in the buffer chain remaining to
 *		be decoded
 *
 */
int
usf_count(usf)
	struct usfmt	*usf;
{
	int		count;
	KBuffer		*m = usf->usf_m_addr;

	/*
	 * Return zero if we're not decoding
	 */
	if (usf->usf_op != USF_DECODE)
		return (0);

	/*
	 * Calculate the length of data remaining in the current buffer
	 */
	count = KB_LEN(m) - usf->usf_loc;

	/*
	 * Loop through any remaining buffers, adding in their lengths
	 */
	while (KB_NEXT(m)) {
		m = KB_NEXT(m);
		count += KB_LEN(m);
	}

	return(count);

}


/*
 * Get or put the next byte of a signalling message and return
 * the byte's buffer address 
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	c	pointer to the byte to send from or receive into
 *	bp	address to store the byte's buffer address
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
usf_byte_mark(usf, c, bp)
	struct usfmt	*usf;
	u_char		*c;
	u_char		**bp;
{
	u_char		*mp;
	int		rc;

	/*
	 * First, get/put the data byte
	 */
	rc = usf_byte(usf, c);
	if (rc) {

		/*
		 * Error encountered
		 */
		*bp = NULL;
		return (rc);
	}

	/*
	 * Now return the buffer address of that byte
	 */
	KB_DATASTART(usf->usf_m_addr, mp, u_char *);
	*bp = &mp[usf->usf_loc - 1];

	return (0);
}

