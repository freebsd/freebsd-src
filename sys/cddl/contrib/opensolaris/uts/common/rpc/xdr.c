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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
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
 * xdr.c, generic XDR routines implementation.
 * These are the "generic" xdr routines used to serialize and de-serialize
 * most common data items.  See xdr.h for more info on the interface to
 * xdr.
 */

#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/types.h>
#include <sys/systm.h>

#include <rpc/types.h>
#include <rpc/xdr.h>

#pragma weak xdr_int32_t = xdr_int
#pragma weak xdr_uint32_t = xdr_u_int
#pragma weak xdr_int64_t = xdr_longlong_t
#pragma weak xdr_uint64_t = xdr_u_longlong_t

#if defined(sun)
#if !defined(_BIG_ENDIAN) && !defined(_LITTLE_ENDIAN)
#error "Exactly one of _BIG_ENDIAN or _LITTLE_ENDIAN must be defined"
#elif defined(_BIG_ENDIAN) && defined(_LITTLE_ENDIAN)
#error "Only one of _BIG_ENDIAN or _LITTLE_ENDIAN may be defined"
#endif
#endif

/*
 * constants specific to the xdr "protocol"
 */
#define	XDR_FALSE	((int32_t)0)
#define	XDR_TRUE	((int32_t)1)
#define	LASTUNSIGNED	((uint_t)0-1)

/*
 * for unit alignment
 */
static char xdr_zero[BYTES_PER_XDR_UNIT] = { 0, 0, 0, 0 };

/*
 * Free a data structure using XDR
 * Not a filter, but a convenient utility nonetheless
 */
void
xdr_free(xdrproc_t proc, char *objp)
{
	XDR x;

	x.x_op = XDR_FREE;
	(*proc)(&x, objp);
}

/*
 * XDR nothing
 */
bool_t
xdr_void(void)
{
	return (TRUE);
}

/*
 * XDR integers
 *
 * PSARC 2003/523 Contract Private Interface
 * xdr_int
 * Changes must be reviewed by Solaris File Sharing
 * Changes must be communicated to contract-2003-523@sun.com
 */
bool_t
xdr_int(XDR *xdrs, int *ip)
{
	if (xdrs->x_op == XDR_ENCODE)
		return (XDR_PUTINT32(xdrs, ip));

	if (xdrs->x_op == XDR_DECODE)
		return (XDR_GETINT32(xdrs, ip));

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

#ifdef DEBUG
	printf("xdr_int: FAILED\n");
#endif
	return (FALSE);
}

/*
 * XDR unsigned integers
 *
 * PSARC 2003/523 Contract Private Interface
 * xdr_u_int
 * Changes must be reviewed by Solaris File Sharing
 * Changes must be communicated to contract-2003-523@sun.com
 */
bool_t
xdr_u_int(XDR *xdrs, uint_t *up)
{
	if (xdrs->x_op == XDR_ENCODE)
		return (XDR_PUTINT32(xdrs, (int32_t *)up));

	if (xdrs->x_op == XDR_DECODE)
		return (XDR_GETINT32(xdrs, (int32_t *)up));

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

#ifdef DEBUG
	printf("xdr_int: FAILED\n");
#endif
	return (FALSE);
}


#if defined(_ILP32)
/*
 * xdr_long and xdr_u_long for binary compatability on ILP32 kernels.
 *
 * No prototypes since new code should not be using these interfaces.
 */
bool_t
xdr_long(XDR *xdrs, long *ip)
{
	return (xdr_int(xdrs, (int *)ip));
}

bool_t
xdr_u_long(XDR *xdrs, unsigned long *up)
{
	return (xdr_u_int(xdrs, (uint_t *)up));
}
#endif /* _ILP32 */


/*
 * XDR long long integers
 */
bool_t
xdr_longlong_t(XDR *xdrs, longlong_t *hp)
{
	if (xdrs->x_op == XDR_ENCODE) {
#if BYTE_ORDER == _LITTLE_ENDIAN
		if (XDR_PUTINT32(xdrs, (int32_t *)((char *)hp +
		    BYTES_PER_XDR_UNIT)) == TRUE) {
			return (XDR_PUTINT32(xdrs, (int32_t *)hp));
		}
#else
		if (XDR_PUTINT32(xdrs, (int32_t *)hp) == TRUE) {
			return (XDR_PUTINT32(xdrs, (int32_t *)((char *)hp +
			    BYTES_PER_XDR_UNIT)));
		}
#endif
		return (FALSE);

	}
	if (xdrs->x_op == XDR_DECODE) {
#if BYTE_ORDER == _LITTLE_ENDIAN
		if (XDR_GETINT32(xdrs, (int32_t *)((char *)hp +
		    BYTES_PER_XDR_UNIT)) == TRUE) {
			return (XDR_GETINT32(xdrs, (int32_t *)hp));
		}
#else
		if (XDR_GETINT32(xdrs, (int32_t *)hp) == TRUE) {
			return (XDR_GETINT32(xdrs, (int32_t *)((char *)hp +
			    BYTES_PER_XDR_UNIT)));
		}
#endif
		return (FALSE);
	}
	return (TRUE);
}

/*
 * XDR unsigned long long integers
 */
bool_t
xdr_u_longlong_t(XDR *xdrs, u_longlong_t *hp)
{

	if (xdrs->x_op == XDR_ENCODE) {
#if BYTE_ORDER == _LITTLE_ENDIAN
		if (XDR_PUTINT32(xdrs, (int32_t *)((char *)hp +
		    BYTES_PER_XDR_UNIT)) == TRUE) {
			return (XDR_PUTINT32(xdrs, (int32_t *)hp));
		}
#else
		if (XDR_PUTINT32(xdrs, (int32_t *)hp) == TRUE) {
			return (XDR_PUTINT32(xdrs, (int32_t *)((char *)hp +
			    BYTES_PER_XDR_UNIT)));
		}
#endif
		return (FALSE);

	}
	if (xdrs->x_op == XDR_DECODE) {
#if BYTE_ORDER == _LITTLE_ENDIAN
		if (XDR_GETINT32(xdrs, (int32_t *)((char *)hp +
		    BYTES_PER_XDR_UNIT)) == TRUE) {
			return (XDR_GETINT32(xdrs, (int32_t *)hp));
		}
#else
		if (XDR_GETINT32(xdrs, (int32_t *)hp) == TRUE) {
			return (XDR_GETINT32(xdrs, (int32_t *)((char *)hp +
			    BYTES_PER_XDR_UNIT)));
		}
#endif
		return (FALSE);
	}
	return (TRUE);
}

/*
 * XDR short integers
 */
bool_t
xdr_short(XDR *xdrs, short *sp)
{
	int32_t l;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		l = (int32_t)*sp;
		return (XDR_PUTINT32(xdrs, &l));

	case XDR_DECODE:
		if (!XDR_GETINT32(xdrs, &l))
			return (FALSE);
		*sp = (short)l;
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
xdr_u_short(XDR *xdrs, ushort_t *usp)
{
	uint32_t l;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		l = (uint32_t)*usp;
		return (XDR_PUTINT32(xdrs, (int32_t *)&l));

	case XDR_DECODE:
		if (!XDR_GETINT32(xdrs, (int32_t *)&l)) {
#ifdef DEBUG
			printf("xdr_u_short: decode FAILED\n");
#endif
			return (FALSE);
		}
		*usp = (ushort_t)l;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
#ifdef DEBUG
	printf("xdr_u_short: bad op FAILED\n");
#endif
	return (FALSE);
}


/*
 * XDR a char
 */
bool_t
xdr_char(XDR *xdrs, char *cp)
{
	int i;

	i = (*cp);
	if (!xdr_int(xdrs, &i)) {
		return (FALSE);
	}
	*cp = (char)i;
	return (TRUE);
}

/*
 * XDR booleans
 *
 * PSARC 2003/523 Contract Private Interface
 * xdr_bool
 * Changes must be reviewed by Solaris File Sharing
 * Changes must be communicated to contract-2003-523@sun.com
 */
bool_t
xdr_bool(XDR *xdrs, bool_t *bp)
{
	int32_t i32b;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		i32b = *bp ? XDR_TRUE : XDR_FALSE;
		return (XDR_PUTINT32(xdrs, &i32b));

	case XDR_DECODE:
		if (!XDR_GETINT32(xdrs, &i32b)) {
#ifdef DEBUG
			printf("xdr_bool: decode FAILED\n");
#endif
			return (FALSE);
		}
		*bp = (i32b == XDR_FALSE) ? FALSE : TRUE;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
#ifdef DEBUG
	printf("xdr_bool: bad op FAILED\n");
#endif
	return (FALSE);
}

/*
 * XDR enumerations
 *
 * PSARC 2003/523 Contract Private Interface
 * xdr_enum
 * Changes must be reviewed by Solaris File Sharing
 * Changes must be communicated to contract-2003-523@sun.com
 */
#ifndef lint
enum sizecheck { SIZEVAL } sizecheckvar;	/* used to find the size of */
						/* an enum */
#endif
bool_t
xdr_enum(XDR *xdrs, enum_t *ep)
{
#ifndef lint
	/*
	 * enums are treated as ints
	 */
	if (sizeof (sizecheckvar) == sizeof (int32_t)) {
		return (xdr_int(xdrs, (int32_t *)ep));
	} else if (sizeof (sizecheckvar) == sizeof (short)) {
		return (xdr_short(xdrs, (short *)ep));
	} else {
		return (FALSE);
	}
#else
	(void) (xdr_short(xdrs, (short *)ep));
	return (xdr_int(xdrs, (int32_t *)ep));
#endif
}

/*
 * XDR opaque data
 * Allows the specification of a fixed size sequence of opaque bytes.
 * cp points to the opaque object and cnt gives the byte length.
 *
 * PSARC 2003/523 Contract Private Interface
 * xdr_opaque
 * Changes must be reviewed by Solaris File Sharing
 * Changes must be communicated to contract-2003-523@sun.com
 */
bool_t
xdr_opaque(XDR *xdrs, caddr_t cp, const uint_t cnt)
{
	uint_t rndup;
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
	if (rndup != 0)
		rndup = BYTES_PER_XDR_UNIT - rndup;

	if (xdrs->x_op == XDR_DECODE) {
		if (!XDR_GETBYTES(xdrs, cp, cnt)) {
#ifdef DEBUG
			printf("xdr_opaque: decode FAILED\n");
#endif
			return (FALSE);
		}
		if (rndup == 0)
			return (TRUE);
		return (XDR_GETBYTES(xdrs, (caddr_t)crud, rndup));
	}

	if (xdrs->x_op == XDR_ENCODE) {
		if (!XDR_PUTBYTES(xdrs, cp, cnt)) {
#ifdef DEBUG
			printf("xdr_opaque: encode FAILED\n");
#endif
			return (FALSE);
		}
		if (rndup == 0)
			return (TRUE);
		return (XDR_PUTBYTES(xdrs, xdr_zero, rndup));
	}

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

#ifdef DEBUG
	printf("xdr_opaque: bad op FAILED\n");
#endif
	return (FALSE);
}

/*
 * XDR counted bytes
 * *cpp is a pointer to the bytes, *sizep is the count.
 * If *cpp is NULL maxsize bytes are allocated
 *
 * PSARC 2003/523 Contract Private Interface
 * xdr_bytes
 * Changes must be reviewed by Solaris File Sharing
 * Changes must be communicated to contract-2003-523@sun.com
 */
bool_t
xdr_bytes(XDR *xdrs, char **cpp, uint_t *sizep, const uint_t maxsize)
{
	char *sp = *cpp;  /* sp is the actual string pointer */
	uint_t nodesize;

	/*
	 * first deal with the length since xdr bytes are counted
	 */
	if (!xdr_u_int(xdrs, sizep)) {
#ifdef DEBUG
		printf("xdr_bytes: size FAILED\n");
#endif
		return (FALSE);
	}
	nodesize = *sizep;
	if ((nodesize > maxsize) && (xdrs->x_op != XDR_FREE)) {
#ifdef DEBUG
		printf("xdr_bytes: bad size (%d) FAILED (%d max)\n",
		    nodesize, maxsize);
#endif
		return (FALSE);
	}

	/*
	 * now deal with the actual bytes
	 */
	switch (xdrs->x_op) {
	case XDR_DECODE:
		if (nodesize == 0)
			return (TRUE);
		if (sp == NULL)
			*cpp = sp = (char *)mem_alloc(nodesize);
		/* FALLTHROUGH */

	case XDR_ENCODE:
		return (xdr_opaque(xdrs, sp, nodesize));

	case XDR_FREE:
		if (sp != NULL) {
			mem_free(sp, nodesize);
			*cpp = NULL;
		}
		return (TRUE);
	}
#ifdef DEBUG
	printf("xdr_bytes: bad op FAILED\n");
#endif
	return (FALSE);
}

/*
 * Implemented here due to commonality of the object.
 */
bool_t
xdr_netobj(XDR *xdrs, struct netobj *np)
{
	return (xdr_bytes(xdrs, &np->n_bytes, &np->n_len, MAX_NETOBJ_SZ));
}

/*
 * XDR a descriminated union
 * Support routine for discriminated unions.
 * You create an array of xdrdiscrim structures, terminated with
 * an entry with a null procedure pointer.  The routine gets
 * the discriminant value and then searches the array of xdrdiscrims
 * looking for that value.  It calls the procedure given in the xdrdiscrim
 * to handle the discriminant.  If there is no specific routine a default
 * routine may be called.
 * If there is no specific or default routine an error is returned.
 */
bool_t
xdr_union(XDR *xdrs, enum_t *dscmp, char *unp,
	const struct xdr_discrim *choices, const xdrproc_t dfault)
{
	enum_t dscm;

	/*
	 * we deal with the discriminator;  it's an enum
	 */
	if (!xdr_enum(xdrs, dscmp)) {
#ifdef DEBUG
		printf("xdr_enum: dscmp FAILED\n");
#endif
		return (FALSE);
	}
	dscm = *dscmp;

	/*
	 * search choices for a value that matches the discriminator.
	 * if we find one, execute the xdr routine for that value.
	 */
	for (; choices->proc != NULL_xdrproc_t; choices++) {
		if (choices->value == dscm)
			return ((*(choices->proc))(xdrs, unp, LASTUNSIGNED));
	}

	/*
	 * no match - execute the default xdr routine if there is one
	 */
	return ((dfault == NULL_xdrproc_t) ? FALSE :
	    (*dfault)(xdrs, unp, LASTUNSIGNED));
}


/*
 * Non-portable xdr primitives.
 * Care should be taken when moving these routines to new architectures.
 */


/*
 * XDR null terminated ASCII strings
 * xdr_string deals with "C strings" - arrays of bytes that are
 * terminated by a NULL character.  The parameter cpp references a
 * pointer to storage; If the pointer is null, then the necessary
 * storage is allocated.  The last parameter is the max allowed length
 * of the string as specified by a protocol.
 */
bool_t
xdr_string(XDR *xdrs, char **cpp, const uint_t maxsize)
{
	char *sp = *cpp;  /* sp is the actual string pointer */
	uint_t size;
	uint_t nodesize;

	/*
	 * first deal with the length since xdr strings are counted-strings
	 */
	switch (xdrs->x_op) {
	case XDR_FREE:
		if (sp == NULL)
			return (TRUE);	/* already free */
		/* FALLTHROUGH */
	case XDR_ENCODE:
		size = (sp != NULL) ? (uint_t)strlen(sp) : 0;
		break;
	case XDR_DECODE:
		break;
	}
	if (!xdr_u_int(xdrs, &size)) {
#ifdef DEBUG
		printf("xdr_string: size FAILED\n");
#endif
		return (FALSE);
	}
	if (size > maxsize) {
#ifdef DEBUG
		printf("xdr_string: bad size FAILED\n");
#endif
		return (FALSE);
	}
	nodesize = size + 1;

	/*
	 * now deal with the actual bytes
	 */
	switch (xdrs->x_op) {
	case XDR_DECODE:
		if (nodesize == 0)
			return (TRUE);
		if (sp == NULL)
			sp = (char *)mem_alloc(nodesize);
		sp[size] = 0;
		if (!xdr_opaque(xdrs, sp, size)) {
			/*
			 * free up memory if allocated here
			 */
			if (*cpp == NULL) {
				mem_free(sp, nodesize);
			}
			return (FALSE);
		}
		if (strlen(sp) != size) {
			if (*cpp == NULL) {
				mem_free(sp, nodesize);
			}
			return (FALSE);
		}
		*cpp = sp;
		return (TRUE);

	case XDR_ENCODE:
		return (xdr_opaque(xdrs, sp, size));

	case XDR_FREE:
		mem_free(sp, nodesize);
		*cpp = NULL;
		return (TRUE);
	}
#ifdef DEBUG
	printf("xdr_string: bad op FAILED\n");
#endif
	return (FALSE);
}

/*
 * Wrapper for xdr_string that can be called directly from
 * routines like clnt_call
 */
bool_t
xdr_wrapstring(XDR *xdrs, char **cpp)
{
	if (xdr_string(xdrs, cpp, LASTUNSIGNED))
		return (TRUE);
	return (FALSE);
}
