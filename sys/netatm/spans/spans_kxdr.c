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
 *	@(#) $FreeBSD: src/sys/netatm/spans/spans_kxdr.c,v 1.3 1999/08/28 00:48:50 peter Exp $
 *
 */

/*
 * SPANS Signalling Manager
 * ---------------------------
 *
 * Kernel XDR (External Data Representation) routines
 *
 */

#include <netatm/kern_include.h>

#ifndef lint
__RCSID("@(#) $FreeBSD: src/sys/netatm/spans/spans_kxdr.c,v 1.3 1999/08/28 00:48:50 peter Exp $");
#endif

/*
 * This file contains code that has been copied and/or modified from
 * the following FreeBSD files:
 *
 *	/usr/src/lib/libc/xdr/xdr.c
 *	/usr/src/lib/libc/xdr/xdr_mem.c
 *
 * which are covered by the copyright notice below.
 */

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#if !defined(sun)

#if defined(LIBC_SCCS) && !defined(lint)
/*static char *sccsid = "from: @(#)xdr.c 1.35 87/08/12";*/
/*static char *sccsid = "from: @(#)xdr.c	2.1 88/07/29 4.0 RPCSRC";*/
/*static char *rcsid = "Id: xdr.c,v 1.2.4.2 1996/06/05 02:52:02 jkh Exp";*/
#endif

/*
 * xdr.c, Generic XDR routines implementation.
 *
 * Copyright (C) 1986, Sun Microsystems, Inc.
 *
 * These are the "generic" xdr routines used to serialize and de-serialize
 * most common data items.  See xdr.h for more info on the interface to
 * xdr.
 */

#include <rpc/types.h>
#include <rpc/xdr.h>

/*
 * constants specific to the xdr "protocol"
 */
#define XDR_FALSE	((long) 0)
#define XDR_TRUE	((long) 1)
#define LASTUNSIGNED	((u_int) 0-1)

/*
 * for unit alignment
 */
static char xdr_zero[BYTES_PER_XDR_UNIT] = { 0, 0, 0, 0 };

/*
 * XDR integers
 */
bool_t
xdr_int(xdrs, ip)
	XDR *xdrs;
	int *ip;
{

#ifdef lint
	(void) (xdr_short(xdrs, (short *)ip));
	return (xdr_long(xdrs, (long *)ip));
#else
	if (sizeof (int) == sizeof (long)) {
		return (xdr_long(xdrs, (long *)ip));
	} else {
		return (xdr_short(xdrs, (short *)ip));
	}
#endif
}

/*
 * XDR unsigned integers
 */
bool_t
xdr_u_int(xdrs, up)
	XDR *xdrs;
	u_int *up;
{

#ifdef lint
	(void) (xdr_short(xdrs, (short *)up));
	return (xdr_u_long(xdrs, (u_long *)up));
#else
	if (sizeof (u_int) == sizeof (u_long)) {
		return (xdr_u_long(xdrs, (u_long *)up));
	} else {
		return (xdr_short(xdrs, (short *)up));
	}
#endif
}

/*
 * XDR long integers
 * same as xdr_u_long - open coded to save a proc call!
 */
bool_t
xdr_long(xdrs, lp)
	register XDR *xdrs;
	long *lp;
{

	if (xdrs->x_op == XDR_ENCODE)
		return (XDR_PUTLONG(xdrs, lp));

	if (xdrs->x_op == XDR_DECODE)
		return (XDR_GETLONG(xdrs, lp));

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

	return (FALSE);
}

/*
 * XDR unsigned long integers
 * same as xdr_long - open coded to save a proc call!
 */
bool_t
xdr_u_long(xdrs, ulp)
	register XDR *xdrs;
	u_long *ulp;
{

	if (xdrs->x_op == XDR_DECODE)
		return (XDR_GETLONG(xdrs, (long *)ulp));
	if (xdrs->x_op == XDR_ENCODE)
		return (XDR_PUTLONG(xdrs, (long *)ulp));
	if (xdrs->x_op == XDR_FREE)
		return (TRUE);
	return (FALSE);
}

/*
 * XDR short integers
 */
bool_t
xdr_short(xdrs, sp)
	register XDR *xdrs;
	short *sp;
{
	long l;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		l = (long) *sp;
		return (XDR_PUTLONG(xdrs, &l));

	case XDR_DECODE:
		if (!XDR_GETLONG(xdrs, &l)) {
			return (FALSE);
		}
		*sp = (short) l;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}

/*
 * XDR unsigned short integers
 */
bool_t
xdr_u_short(xdrs, usp)
	register XDR *xdrs;
	u_short *usp;
{
	u_long l;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		l = (u_long) *usp;
		return (XDR_PUTLONG(xdrs, &l));

	case XDR_DECODE:
		if (!XDR_GETLONG(xdrs, &l)) {
			return (FALSE);
		}
		*usp = (u_short) l;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}


/*
 * XDR a char
 */
bool_t
xdr_char(xdrs, cp)
	XDR *xdrs;
	char *cp;
{
	int i;

	i = (*cp);
	if (!xdr_int(xdrs, &i)) {
		return (FALSE);
	}
	*cp = i;
	return (TRUE);
}

/*
 * XDR an unsigned char
 */
bool_t
xdr_u_char(xdrs, cp)
	XDR *xdrs;
	u_char *cp;
{
	u_int u;

	u = (*cp);
	if (!xdr_u_int(xdrs, &u)) {
		return (FALSE);
	}
	*cp = u;
	return (TRUE);
}

/*
 * XDR booleans
 */
bool_t
xdr_bool(xdrs, bp)
	register XDR *xdrs;
	bool_t *bp;
{
	long lb;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		lb = *bp ? XDR_TRUE : XDR_FALSE;
		return (XDR_PUTLONG(xdrs, &lb));

	case XDR_DECODE:
		if (!XDR_GETLONG(xdrs, &lb)) {
			return (FALSE);
		}
		*bp = (lb == XDR_FALSE) ? FALSE : TRUE;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}

/*
 * XDR enumerations
 */
bool_t
xdr_enum(xdrs, ep)
	XDR *xdrs;
	enum_t *ep;
{
#ifndef lint
	enum sizecheck { SIZEVAL };	/* used to find the size of an enum */

	/*
	 * enums are treated as ints
	 */
	if (sizeof (enum sizecheck) == sizeof (long)) {
		return (xdr_long(xdrs, (long *)ep));
	} else if (sizeof (enum sizecheck) == sizeof (short)) {
		return (xdr_short(xdrs, (short *)ep));
	} else {
		return (FALSE);
	}
#else
	(void) (xdr_short(xdrs, (short *)ep));
	return (xdr_long(xdrs, (long *)ep));
#endif
}

/*
 * XDR opaque data
 * Allows the specification of a fixed size sequence of opaque bytes.
 * cp points to the opaque object and cnt gives the byte length.
 */
bool_t
xdr_opaque(xdrs, cp, cnt)
	register XDR *xdrs;
	caddr_t cp;
	register u_int cnt;
{
	register u_int rndup;
	static char crud[BYTES_PER_XDR_UNIT];

	/*
	 * if no data we are done
	 */
	if (cnt == 0)
		return (TRUE);

	/*
	 * round byte count to full xdr units
	 */
	rndup = cnt % BYTES_PER_XDR_UNIT;
	if (rndup > 0)
		rndup = BYTES_PER_XDR_UNIT - rndup;

	if (xdrs->x_op == XDR_DECODE) {
		if (!XDR_GETBYTES(xdrs, cp, cnt)) {
			return (FALSE);
		}
		if (rndup == 0)
			return (TRUE);
		return (XDR_GETBYTES(xdrs, crud, rndup));
	}

	if (xdrs->x_op == XDR_ENCODE) {
		if (!XDR_PUTBYTES(xdrs, cp, cnt)) {
			return (FALSE);
		}
		if (rndup == 0)
			return (TRUE);
		return (XDR_PUTBYTES(xdrs, xdr_zero, rndup));
	}

	if (xdrs->x_op == XDR_FREE) {
		return (TRUE);
	}

	return (FALSE);
}


/*
 * XDR implementation using kernel buffers
 */

#if defined(LIBC_SCCS) && !defined(lint)
/*static char *sccsid = "from: @(#)xdr_mem.c 1.19 87/08/11 Copyr 1984 Sun Micro";*/
/*static char *sccsid = "from: @(#)xdr_mem.c	2.1 88/07/29 4.0 RPCSRC";*/
/*static char *rcsid = "Id: xdr_mem.c,v 1.2.4.2 1996/06/05 02:52:04 jkh Exp";*/
#endif

/*
 * xdr_mem.h, XDR implementation using memory buffers.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 * If you have some data to be interpreted as external data representation
 * or to be converted to external data representation in a memory buffer,
 * then this is the package for you.
 *
 */


void		xdrmbuf_init __P((XDR *, KBuffer *, enum xdr_op));
static bool_t	xdrmbuf_getlong __P((XDR *, long *));
static bool_t	xdrmbuf_putlong __P((XDR *, long *));
static bool_t	xdrmbuf_getbytes __P((XDR *, caddr_t, u_int));
static bool_t	xdrmbuf_putbytes __P((XDR *, caddr_t, u_int));
static u_int	xdrmbuf_getpos __P((XDR *));

static struct	xdr_ops xdrmbuf_ops = {
	xdrmbuf_getlong,
	xdrmbuf_putlong,
	xdrmbuf_getbytes,
	xdrmbuf_putbytes,
	xdrmbuf_getpos,
	NULL,
	NULL,
	NULL
};

/*
 * The procedure xdrmbuf_init initializes a stream descriptor for a
 * kernel buffer.
 */
void
xdrmbuf_init(xdrs, m, op)
	register XDR *xdrs;
	KBuffer	*m;
	enum xdr_op op;
{

	xdrs->x_op = op;
	xdrs->x_ops = &xdrmbuf_ops;
	xdrs->x_base = (caddr_t)m;
	KB_DATASTART(m, xdrs->x_private, caddr_t);
	xdrs->x_handy = KB_LEN(m);
}

static bool_t
xdrmbuf_getlong(xdrs, lp)
	register XDR *xdrs;
	long *lp;
{

	/*
	 * See if long is contained in this buffer
	 */
	if ((xdrs->x_handy -= sizeof(long)) < 0) {
		register KBuffer	*m;

		/*
		 * We (currently) don't allow a long to span a buffer
		 */
		if (xdrs->x_handy != -sizeof(long)) {
			printf("xdrmbuf_getlong: data spans buffers\n");
			return (FALSE);
		}

		/*
		 * Try to move to a chained buffer
		 */
		if ((m = (KBuffer *)(xdrs->x_base)) != NULL) {
			m = KB_NEXT(m);
			xdrs->x_base = (caddr_t)m;
		}
		if (m) {
			/*
			 * Setup new buffer's info
			 */
			KB_DATASTART(m, xdrs->x_private, caddr_t);
			if ((xdrs->x_handy = KB_LEN(m) - sizeof(long)) < 0) {
				printf("xdrmbuf_getlong: short buffer\n");
				return (FALSE);
			}
		} else {
			/*
			 * No more buffers
			 */
			return (FALSE);
		}
	}

	/*
	 * Return the long value
	 */
	*lp = (long)ntohl((u_long)(*((long *)(xdrs->x_private))));

	/*
	 * Advance the data stream
	 */
	xdrs->x_private += sizeof(long);
	return (TRUE);
}

static bool_t
xdrmbuf_putlong(xdrs, lp)
	register XDR *xdrs;
	long *lp;
{

	/*
	 * See if long will fit in this buffer
	 */
	if ((xdrs->x_handy -= sizeof(long)) < 0) {
		register KBuffer	*m;

		/*
		 * We (currently) don't allow a long to span a buffer
		 */
		if (xdrs->x_handy != -sizeof(long)) {
			printf("xdrmbuf_putlong: data spans buffers\n");
			return (FALSE);
		}

		/*
		 * Try to move to a chained buffer
		 */
		if ((m = (KBuffer *)(xdrs->x_base)) != NULL) {
			m = KB_NEXT(m);
			xdrs->x_base = (caddr_t)m;
		}
		if (m) {
			/*
			 * Setup new buffer's info
			 */
			KB_DATASTART(m, xdrs->x_private, caddr_t);
			if ((xdrs->x_handy = KB_LEN(m) - sizeof(long)) < 0) {
				printf("xdrmbuf_putlong: short buffer\n");
				return (FALSE);
			}
		} else {
			/*
			 * No more buffers
			 */
			return (FALSE);
		}
	}

	/*
	 * Store the long value into our buffer
	 */
	*(long *)xdrs->x_private = (long)htonl((u_long)(*lp));

	/*
	 * Advance the data stream
	 */
	xdrs->x_private += sizeof(long);
	return (TRUE);
}

static bool_t
xdrmbuf_getbytes(xdrs, addr, len)
	register XDR *xdrs;
	caddr_t addr;
	register u_int len;
{

	while (len > 0) {
		u_int	copy;

		if (xdrs->x_handy <= 0) {
			register KBuffer	*m;

			/*
			 * No data in current buffer, move to a chained buffer
			 */
			if ((m = (KBuffer *)(xdrs->x_base)) != NULL) {
				m = KB_NEXT(m);
				xdrs->x_base = (caddr_t)m;
			}
			if (m) {
				/*
				 * Setup new buffer's info
				 */
				KB_DATASTART(m, xdrs->x_private, caddr_t);
				xdrs->x_handy = KB_LEN(m);
			} else {
				/*
				 * No more buffers
				 */
				return (FALSE);
			}
		}

		/*
		 * Copy from buffer to user's space
		 */
		copy = MIN(len, xdrs->x_handy);
		KM_COPY(xdrs->x_private, addr, copy);

		/*
		 * Update data stream controls
		 */
		xdrs->x_private += copy;
		xdrs->x_handy -= copy;
		addr += copy;
		len -= copy;
	}
	return (TRUE);
}

static bool_t
xdrmbuf_putbytes(xdrs, addr, len)
	register XDR *xdrs;
	caddr_t addr;
	register u_int len;
{

	while (len > 0) {
		u_int	copy;

		if (xdrs->x_handy <= 0) {
			register KBuffer	*m;

			/*
			 * No data in current buffer, move to a chained buffer
			 */
			if ((m = (KBuffer *)(xdrs->x_base)) != NULL) {
				m = KB_NEXT(m);
				xdrs->x_base = (caddr_t)m;
			}
			if (m) {
				/*
				 * Setup new buffer's info
				 */
				KB_DATASTART(m, xdrs->x_private, caddr_t);
				xdrs->x_handy = KB_LEN(m);
			} else {
				/*
				 * No more buffers
				 */
				return (FALSE);
			}
		}

		/*
		 * Copy from user's space into buffer
		 */
		copy = MIN(len, xdrs->x_handy);
		KM_COPY(addr, xdrs->x_private, copy);

		/*
		 * Update data stream controls
		 */
		xdrs->x_private += copy;
		xdrs->x_handy -= copy;
		addr += copy;
		len -= copy;
	}
	return (TRUE);
}

static u_int
xdrmbuf_getpos(xdrs)
	register XDR *xdrs;
{

	return ((u_int)xdrs->x_private - (u_int)xdrs->x_base);
}

#endif	/* !defined(sun) */

