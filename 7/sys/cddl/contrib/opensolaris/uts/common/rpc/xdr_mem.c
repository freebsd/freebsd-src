/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1983, 1984, 1985, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * Portions of this source code were derived from Berkeley 4.3 BSD
 * under license from the Regents of the University of California.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * xdr_mem.c, XDR implementation using memory buffers.
 *
 * If you have some data to be interpreted as external data representation
 * or to be converted to external data representation in a memory buffer,
 * then this is the package for you.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>

#include <rpc/types.h>
#include <rpc/xdr.h>

static struct xdr_ops *xdrmem_ops(void);

/*
 * The procedure xdrmem_create initializes a stream descriptor for a
 * memory buffer.
 */
void
xdrmem_create(XDR *xdrs, caddr_t addr, uint_t size, enum xdr_op op)
{
	xdrs->x_op = op;
	xdrs->x_ops = xdrmem_ops();
	xdrs->x_private = xdrs->x_base = addr;
	xdrs->x_handy = size;
	xdrs->x_public = NULL;
}

/* ARGSUSED */
static void
xdrmem_destroy(XDR *xdrs)
{
}

static bool_t
xdrmem_getint32(XDR *xdrs, int32_t *int32p)
{
	if ((xdrs->x_handy -= (int)sizeof (int32_t)) < 0)
		return (FALSE);
	/* LINTED pointer alignment */
	*int32p = (int32_t)ntohl((uint32_t)(*((int32_t *)(xdrs->x_private))));
	xdrs->x_private += sizeof (int32_t);
	return (TRUE);
}

static bool_t
xdrmem_putint32(XDR *xdrs, int32_t *int32p)
{
	if ((xdrs->x_handy -= (int)sizeof (int32_t)) < 0)
		return (FALSE);
	/* LINTED pointer alignment */
	*(int32_t *)xdrs->x_private = (int32_t)htonl((uint32_t)(*int32p));
	xdrs->x_private += sizeof (int32_t);
	return (TRUE);
}

static bool_t
xdrmem_getbytes(XDR *xdrs, caddr_t addr, int len)
{
	if ((xdrs->x_handy -= len) < 0)
		return (FALSE);
	bcopy(xdrs->x_private, addr, len);
	xdrs->x_private += len;
	return (TRUE);
}

static bool_t
xdrmem_putbytes(XDR *xdrs, caddr_t addr, int len)
{
	if ((xdrs->x_handy -= len) < 0)
		return (FALSE);
	bcopy(addr, xdrs->x_private, len);
	xdrs->x_private += len;
	return (TRUE);
}

static uint_t
xdrmem_getpos(XDR *xdrs)
{
	return ((uint_t)((uintptr_t)xdrs->x_private - (uintptr_t)xdrs->x_base));
}

static bool_t
xdrmem_setpos(XDR *xdrs, uint_t pos)
{
	caddr_t newaddr = xdrs->x_base + pos;
	caddr_t lastaddr = xdrs->x_private + xdrs->x_handy;
	ptrdiff_t diff;

	if (newaddr > lastaddr)
		return (FALSE);
	xdrs->x_private = newaddr;
	diff = lastaddr - newaddr;
	xdrs->x_handy = (int)diff;
	return (TRUE);
}

static rpc_inline_t *
xdrmem_inline(XDR *xdrs, int len)
{
	rpc_inline_t *buf = NULL;

	if (xdrs->x_handy >= len) {
		xdrs->x_handy -= len;
		/* LINTED pointer alignment */
		buf = (rpc_inline_t *)xdrs->x_private;
		xdrs->x_private += len;
	}
	return (buf);
}

static bool_t
xdrmem_control(XDR *xdrs, int request, void *info)
{
	xdr_bytesrec *xptr;
	int32_t *int32p;
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
		int32p = (int32_t *)info;
		*int32p = (int32_t)ntohl((uint32_t)
		    (*((int32_t *)(xdrs->x_private))));
		return (TRUE);

	case XDR_SKIPBYTES:
		/*
		 * Skip the next N bytes in the XDR stream.
		 */
		int32p = (int32_t *)info;
		len = RNDUP((int)(*int32p));
		if ((xdrs->x_handy -= len) < 0)
			return (FALSE);
		xdrs->x_private += len;
		return (TRUE);

	}
	return (FALSE);
}

static struct xdr_ops *
xdrmem_ops(void)
{
	static struct xdr_ops ops;

	if (ops.x_getint32 == NULL) {
		ops.x_getbytes = xdrmem_getbytes;
		ops.x_putbytes = xdrmem_putbytes;
		ops.x_getpostn = xdrmem_getpos;
		ops.x_setpostn = xdrmem_setpos;
		ops.x_inline = xdrmem_inline;
		ops.x_destroy = xdrmem_destroy;
		ops.x_control = xdrmem_control;
		ops.x_getint32 = xdrmem_getint32;
		ops.x_putint32 = xdrmem_putint32;
	}
	return (&ops);
}
