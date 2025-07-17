/*
 * Copyright (c) 2010, Oracle America, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the "Oracle America, Inc." nor the names of
 *       its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * xdr_sizeof.c
 *
 * General purpose routine to see how much space something will use
 * when serialized using XDR.
 */

#include <gssrpc/types.h>
#include <gssrpc/xdr.h>
#include <sys/types.h>

/* ARGSUSED */
static bool_t
x_putlong(xdrs, longp)
	XDR *xdrs;
	long *longp;
{
	xdrs->x_handy += BYTES_PER_XDR_UNIT;
	return (TRUE);
}

/* ARGSUSED */
static bool_t
x_putbytes(xdrs, bp, len)
	XDR *xdrs;
	char  *bp;
	int len;
{
	xdrs->x_handy += len;

	return (TRUE);
}

static u_int
x_getpostn(xdrs)
	XDR *xdrs;
{
	return (xdrs->x_handy);
}

/* ARGSUSED */
static bool_t
x_setpostn(xdrs, pos)
	XDR *xdrs;
	u_int pos;
{
	/* This is not allowed */
	return (FALSE);
}

static rpc_inline_t *
x_inline(xdrs, len)
	XDR *xdrs;
	int len;
{
	if (len == 0) {
		return (NULL);
	}
	if (xdrs->x_op != XDR_ENCODE) {
		return (NULL);
	}
	if (len < (int) ((caddr_t) xdrs->x_private - xdrs->x_base)) {
		/* x_private was already allocated */
		xdrs->x_handy += len;
		return ((rpc_inline_t *) xdrs->x_private);
	} else {
		/* Free the earlier space and allocate new area */
		if (xdrs->x_base)
			free(xdrs->x_base);
		if ((xdrs->x_base = (caddr_t) malloc(len)) == NULL) {
			xdrs->x_private = NULL;
			return (NULL);
		}
		xdrs->x_private = xdrs->x_base + len;
		xdrs->x_handy += len;
		return ((rpc_inline_t *) (void *) xdrs->x_base);
	}
}

static int
harmless()
{
	/* Always return FALSE/NULL, as the case may be */
	return (0);
}

static void
x_destroy(xdrs)
	XDR *xdrs;
{
	xdrs->x_handy = 0;
	xdrs->x_private = NULL;
	if (xdrs->x_base) {
		free(xdrs->x_base);
		xdrs->x_base = NULL;
	}
	return;
}

unsigned long
xdr_sizeof(func, data)
	xdrproc_t func;
	void *data;
{
	XDR x;
	struct xdr_ops ops;
	bool_t stat;
	/* to stop ANSI-C compiler from complaining */
	typedef  bool_t (* dummyfunc1)(XDR *, long *);
	typedef  bool_t (* dummyfunc2)(XDR *, caddr_t, u_int);

	ops.x_putlong = x_putlong;
	ops.x_putbytes = x_putbytes;
	ops.x_inline = x_inline;
	ops.x_getpostn = x_getpostn;
	ops.x_setpostn = x_setpostn;
	ops.x_destroy = x_destroy;

	/* the other harmless ones */
	ops.x_getlong =  (dummyfunc1) harmless;
	ops.x_getbytes = (dummyfunc2) harmless;

	x.x_op = XDR_ENCODE;
	x.x_ops = &ops;
	x.x_handy = 0;
	x.x_private = (caddr_t) NULL;
	x.x_base = (caddr_t) 0;

	stat = func(&x, data);
	if (x.x_base)
		free(x.x_base);
	return (stat == TRUE ? (unsigned) x.x_handy: 0);
}
