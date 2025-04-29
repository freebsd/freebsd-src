/*	$NetBSD: xdr_mem.c,v 1.15 2000/01/22 22:19:18 mycroft Exp $	*/

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

#include <sys/cdefs.h>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>

#include <rpc/types.h>
#include <rpc/xdr.h>

static void xdrmem_destroy(XDR *);
static bool_t xdrmem_getlong_aligned(XDR *, long *);
static bool_t xdrmem_putlong_aligned(XDR *, const long *);
static bool_t xdrmem_getlong_unaligned(XDR *, long *);
static bool_t xdrmem_putlong_unaligned(XDR *, const long *);
static bool_t xdrmem_getbytes(XDR *, char *, u_int);
static bool_t xdrmem_putbytes(XDR *, const char *, u_int);
static bool_t xdrmem_putmbuf(XDR *, struct mbuf *);
/* XXX: w/64-bit pointers, u_int not enough! */
static u_int xdrmem_getpos(XDR *);
static bool_t xdrmem_setpos(XDR *, u_int);
static int32_t *xdrmem_inline_aligned(XDR *, u_int);
static int32_t *xdrmem_inline_unaligned(XDR *, u_int);
static bool_t xdrmem_control(XDR *xdrs, int request, void *info);

static const struct	xdr_ops xdrmem_ops_aligned = {
	.x_getlong =	xdrmem_getlong_aligned,
	.x_putlong =	xdrmem_putlong_aligned,
	.x_getbytes =	xdrmem_getbytes,
	.x_putbytes =	xdrmem_putbytes,
	.x_putmbuf =	xdrmem_putmbuf,
	.x_getpostn =	xdrmem_getpos,
	.x_setpostn =	xdrmem_setpos,
	.x_inline =	xdrmem_inline_aligned,
	.x_destroy = 	xdrmem_destroy,
	.x_control =	xdrmem_control,
};

static const struct	xdr_ops xdrmem_ops_unaligned = {
	.x_getlong =	xdrmem_getlong_unaligned,
	.x_putlong =	xdrmem_putlong_unaligned,
	.x_getbytes =	xdrmem_getbytes,
	.x_putbytes =	xdrmem_putbytes,
	.x_putmbuf =	xdrmem_putmbuf,
	.x_getpostn =	xdrmem_getpos,
	.x_setpostn =	xdrmem_setpos,
	.x_inline =	xdrmem_inline_unaligned,
	.x_destroy =	xdrmem_destroy,
	.x_control =	xdrmem_control
};

/*
 * The procedure xdrmem_create initializes a stream descriptor for a
 * memory buffer.
 */
void
xdrmem_create(XDR *xdrs, char *addr, u_int size, enum xdr_op op)
{

	xdrs->x_op = op;
	xdrs->x_ops = ((unsigned long)addr & (sizeof(int32_t) - 1))
	    ? &xdrmem_ops_unaligned : &xdrmem_ops_aligned;
	xdrs->x_private = xdrs->x_base = addr;
	xdrs->x_handy = size;
}

/*ARGSUSED*/
static void
xdrmem_destroy(XDR *xdrs)
{

}

static bool_t
xdrmem_getlong_aligned(XDR *xdrs, long *lp)
{

	if (xdrs->x_handy < sizeof(int32_t))
		return (FALSE);
	xdrs->x_handy -= sizeof(int32_t);
	*lp = ntohl(*(uint32_t *)xdrs->x_private);
	xdrs->x_private = (char *)xdrs->x_private + sizeof(int32_t);
	return (TRUE);
}

static bool_t
xdrmem_putlong_aligned(XDR *xdrs, const long *lp)
{

	if (xdrs->x_handy < sizeof(int32_t))
		return (FALSE);
	xdrs->x_handy -= sizeof(int32_t);
	*(uint32_t *)xdrs->x_private = htonl((uint32_t)*lp);
	xdrs->x_private = (char *)xdrs->x_private + sizeof(int32_t);
	return (TRUE);
}

static bool_t
xdrmem_getlong_unaligned(XDR *xdrs, long *lp)
{
	uint32_t l;

	if (xdrs->x_handy < sizeof(int32_t))
		return (FALSE);
	xdrs->x_handy -= sizeof(int32_t);
	memmove(&l, xdrs->x_private, sizeof(int32_t));
	*lp = ntohl(l);
	xdrs->x_private = (char *)xdrs->x_private + sizeof(int32_t);
	return (TRUE);
}

static bool_t
xdrmem_putlong_unaligned(XDR *xdrs, const long *lp)
{
	uint32_t l;

	if (xdrs->x_handy < sizeof(int32_t))
		return (FALSE);
	xdrs->x_handy -= sizeof(int32_t);
	l = htonl((uint32_t)*lp);
	memmove(xdrs->x_private, &l, sizeof(int32_t));
	xdrs->x_private = (char *)xdrs->x_private + sizeof(int32_t);
	return (TRUE);
}

static bool_t
xdrmem_getbytes(XDR *xdrs, char *addr, u_int len)
{

	if (xdrs->x_handy < len)
		return (FALSE);
	xdrs->x_handy -= len;
	memmove(addr, xdrs->x_private, len);
	xdrs->x_private = (char *)xdrs->x_private + len;
	return (TRUE);
}

static bool_t
xdrmem_putbytes(XDR *xdrs, const char *addr, u_int len)
{

	if (xdrs->x_handy < len)
		return (FALSE);
	xdrs->x_handy -= len;
	memmove(xdrs->x_private, addr, len);
	xdrs->x_private = (char *)xdrs->x_private + len;
	return (TRUE);
}

/*
 * Append mbuf.  May fail if not enough space.  Caller owns the mbuf.
 */
static bool_t
xdrmem_putmbuf(XDR *xdrs, struct mbuf *m)
{
	u_int len;

	if (__predict_false(m == NULL))
		return (TRUE);

	len = m_length(m, NULL);

	if (__predict_false(xdrs->x_handy < len))
		return (FALSE);
	xdrs->x_handy -= len;
	m_copydata(m, 0, len, xdrs->x_private);
	xdrs->x_private = (char *)xdrs->x_private + len;
	return (TRUE);
}

static u_int
xdrmem_getpos(XDR *xdrs)
{

	/* XXX w/64-bit pointers, u_int not enough! */
	return (u_int)((u_long)xdrs->x_private - (u_long)xdrs->x_base);
}

static bool_t
xdrmem_setpos(XDR *xdrs, u_int pos)
{
	char *newaddr = xdrs->x_base + pos;
	char *lastaddr = (char *)xdrs->x_private + xdrs->x_handy;

	if (newaddr > lastaddr)
		return (FALSE);
	xdrs->x_private = newaddr;
	xdrs->x_handy = (u_int)(lastaddr - newaddr); /* XXX sizeof(u_int) <? sizeof(ptrdiff_t) */
	return (TRUE);
}

static int32_t *
xdrmem_inline_aligned(XDR *xdrs, u_int len)
{
	int32_t *buf = NULL;

	if (xdrs->x_handy >= len) {
		xdrs->x_handy -= len;
		buf = (int32_t *)xdrs->x_private;
		xdrs->x_private = (char *)xdrs->x_private + len;
	}
	return (buf);
}

/* ARGSUSED */
static int32_t *
xdrmem_inline_unaligned(XDR *xdrs, u_int len)
{

	return (0);
}

static bool_t
xdrmem_control(XDR *xdrs, int request, void *info)
{
	xdr_bytesrec *xptr;
	int32_t *l;
	int len;

	switch (request) {
	case XDR_GET_BYTES_AVAIL:
		xptr = (xdr_bytesrec *)info;
		xptr->xc_is_last_record = TRUE;
		xptr->xc_num_avail = xdrs->x_handy;
		return (TRUE);

	case XDR_PEEK:
		/*
		 * Return the next 4 byte unit in the XDR stream.
		 */
		if (xdrs->x_handy < sizeof (int32_t))
			return (FALSE);
		l = (int32_t *)info;
		*l = (int32_t)ntohl((uint32_t)
		    (*((int32_t *)(xdrs->x_private))));
		return (TRUE);

	case XDR_SKIPBYTES:
		/*
		 * Skip the next N bytes in the XDR stream.
		 */
		l = (int32_t *)info;
		len = RNDUP((int)(*l));
		if (xdrs->x_handy < len)
			return (FALSE);
		xdrs->x_handy -= len;
		xdrs->x_private = (char *)xdrs->x_private + len;
		return (TRUE);
	}
	return (FALSE);
}
