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
 *
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/* Copyright (c) 1983, 1984, 1985, 1986, 1987, 1988, 1989 AT&T */
/* All Rights Reserved */
/*
 * Portions of this source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 */

/*
 * xdr.h, External Data Representation Serialization Routines.
 *
 */

#ifndef _RPC_XDR_H
#define	_RPC_XDR_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/byteorder.h>	/* For all ntoh* and hton*() kind of macros */
#include <rpc/types.h>	/* For all ntoh* and hton*() kind of macros */
#ifndef _KERNEL
#include <stdio.h> /* defines FILE *, used in ANSI C function prototypes */
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * XDR provides a conventional way for converting between C data
 * types and an external bit-string representation.  Library supplied
 * routines provide for the conversion on built-in C data types.  These
 * routines and utility routines defined here are used to help implement
 * a type encode/decode routine for each user-defined type.
 *
 * Each data type provides a single procedure which takes two arguments:
 *
 *	bool_t
 *	xdrproc(xdrs, argresp)
 *		XDR *xdrs;
 *		<type> *argresp;
 *
 * xdrs is an instance of a XDR handle, to which or from which the data
 * type is to be converted.  argresp is a pointer to the structure to be
 * converted.  The XDR handle contains an operation field which indicates
 * which of the operations (ENCODE, DECODE * or FREE) is to be performed.
 *
 * XDR_DECODE may allocate space if the pointer argresp is null.  This
 * data can be freed with the XDR_FREE operation.
 *
 * We write only one procedure per data type to make it easy
 * to keep the encode and decode procedures for a data type consistent.
 * In many cases the same code performs all operations on a user defined type,
 * because all the hard work is done in the component type routines.
 * decode as a series of calls on the nested data types.
 */

/*
 * Xdr operations.  XDR_ENCODE causes the type to be encoded into the
 * stream.  XDR_DECODE causes the type to be extracted from the stream.
 * XDR_FREE can be used to release the space allocated by an XDR_DECODE
 * request.
 */
enum xdr_op {
	XDR_ENCODE = 0,
	XDR_DECODE = 1,
	XDR_FREE = 2
};

/*
 * This is the number of bytes per unit of external data.
 */
#define	BYTES_PER_XDR_UNIT	(4)
#define	RNDUP(x)  ((((x) + BYTES_PER_XDR_UNIT - 1) / BYTES_PER_XDR_UNIT) \
		    * BYTES_PER_XDR_UNIT)

/*
 * The XDR handle.
 * Contains operation which is being applied to the stream,
 * an operations vector for the paticular implementation (e.g. see xdr_mem.c),
 * and two private fields for the use of the particular impelementation.
 *
 * PSARC 2003/523 Contract Private Interface
 * XDR
 * Changes must be reviewed by Solaris File Sharing
 * Changes must be communicated to contract-2003-523@sun.com
 */
typedef struct XDR {
	enum xdr_op	x_op;	/* operation; fast additional param */
	struct xdr_ops *x_ops;
	caddr_t 	x_public; /* users' data */
	caddr_t		x_private; /* pointer to private data */
	caddr_t 	x_base;	/* private used for position info */
	int		x_handy; /* extra private word */
} XDR;

/*
 * PSARC 2003/523 Contract Private Interface
 * xdr_ops
 * Changes must be reviewed by Solaris File Sharing
 * Changes must be communicated to contract-2003-523@sun.com
 */
struct xdr_ops {
#ifdef __STDC__
#if !defined(_KERNEL)
		bool_t	(*x_getlong)(struct XDR *, long *);
		/* get a long from underlying stream */
		bool_t	(*x_putlong)(struct XDR *, long *);
		/* put a long to " */
#endif /* KERNEL */
		bool_t	(*x_getbytes)(struct XDR *, caddr_t, int);
		/* get some bytes from " */
		bool_t	(*x_putbytes)(struct XDR *, caddr_t, int);
		/* put some bytes to " */
		uint_t	(*x_getpostn)(struct XDR *);
		/* returns bytes off from beginning */
		bool_t  (*x_setpostn)(struct XDR *, uint_t);
		/* lets you reposition the stream */
		rpc_inline_t *(*x_inline)(struct XDR *, int);
		/* buf quick ptr to buffered data */
		void	(*x_destroy)(struct XDR *);
		/* free privates of this xdr_stream */
		bool_t	(*x_control)(struct XDR *, int, void *);
#if defined(_LP64) || defined(_KERNEL)
		bool_t	(*x_getint32)(struct XDR *, int32_t *);
		/* get a int from underlying stream */
		bool_t	(*x_putint32)(struct XDR *, int32_t *);
		/* put an int to " */
#endif /* _LP64 || _KERNEL */
#else
#if !defined(_KERNEL)
		bool_t	(*x_getlong)();	/* get a long from underlying stream */
		bool_t	(*x_putlong)();	/* put a long to " */
#endif /* KERNEL */
		bool_t	(*x_getbytes)(); /* get some bytes from " */
		bool_t	(*x_putbytes)(); /* put some bytes to " */
		uint_t	(*x_getpostn)(); /* returns bytes off from beginning */
		bool_t  (*x_setpostn)(); /* lets you reposition the stream */
		rpc_inline_t *(*x_inline)();
				/* buf quick ptr to buffered data */
		void	(*x_destroy)();	/* free privates of this xdr_stream */
		bool_t	(*x_control)();
#if defined(_LP64) || defined(_KERNEL)
		bool_t	(*x_getint32)();
		bool_t	(*x_putint32)();
#endif /* _LP64 || defined(_KERNEL) */
#endif
};

/*
 * Operations defined on a XDR handle
 *
 * XDR		*xdrs;
 * long		*longp;
 * caddr_t	 addr;
 * uint_t	 len;
 * uint_t	 pos;
 */
#if !defined(_KERNEL)
#define	XDR_GETLONG(xdrs, longp)			\
	(*(xdrs)->x_ops->x_getlong)(xdrs, longp)
#define	xdr_getlong(xdrs, longp)			\
	(*(xdrs)->x_ops->x_getlong)(xdrs, longp)

#define	XDR_PUTLONG(xdrs, longp)			\
	(*(xdrs)->x_ops->x_putlong)(xdrs, longp)
#define	xdr_putlong(xdrs, longp)			\
	(*(xdrs)->x_ops->x_putlong)(xdrs, longp)
#endif /* KERNEL */


#if !defined(_LP64) && !defined(_KERNEL)

/*
 * For binary compatability on ILP32 we do not change the shape
 * of the XDR structure and the GET/PUTINT32 functions just use
 * the get/putlong vectors which operate on identically-sized
 * units of data.
 */

#define	XDR_GETINT32(xdrs, int32p)			\
	(*(xdrs)->x_ops->x_getlong)(xdrs, (long *)int32p)
#define	xdr_getint32(xdrs, int32p)			\
	(*(xdrs)->x_ops->x_getlong)(xdrs, (long *)int32p)

#define	XDR_PUTINT32(xdrs, int32p)			\
	(*(xdrs)->x_ops->x_putlong)(xdrs, (long *)int32p)
#define	xdr_putint32(xdrs, int32p)			\
	(*(xdrs)->x_ops->x_putlong)(xdrs, (long *)int32p)

#else /* !_LP64 && !_KERNEL */

#define	XDR_GETINT32(xdrs, int32p)			\
	(*(xdrs)->x_ops->x_getint32)(xdrs, int32p)
#define	xdr_getint32(xdrs, int32p)			\
	(*(xdrs)->x_ops->x_getint32)(xdrs, int32p)

#define	XDR_PUTINT32(xdrs, int32p)			\
	(*(xdrs)->x_ops->x_putint32)(xdrs, int32p)
#define	xdr_putint32(xdrs, int32p)			\
	(*(xdrs)->x_ops->x_putint32)(xdrs, int32p)

#endif /* !_LP64 && !_KERNEL */

#define	XDR_GETBYTES(xdrs, addr, len)			\
	(*(xdrs)->x_ops->x_getbytes)(xdrs, addr, len)
#define	xdr_getbytes(xdrs, addr, len)			\
	(*(xdrs)->x_ops->x_getbytes)(xdrs, addr, len)

#define	XDR_PUTBYTES(xdrs, addr, len)			\
	(*(xdrs)->x_ops->x_putbytes)(xdrs, addr, len)
#define	xdr_putbytes(xdrs, addr, len)			\
	(*(xdrs)->x_ops->x_putbytes)(xdrs, addr, len)

#define	XDR_GETPOS(xdrs)				\
	(*(xdrs)->x_ops->x_getpostn)(xdrs)
#define	xdr_getpos(xdrs)				\
	(*(xdrs)->x_ops->x_getpostn)(xdrs)

#define	XDR_SETPOS(xdrs, pos)				\
	(*(xdrs)->x_ops->x_setpostn)(xdrs, pos)
#define	xdr_setpos(xdrs, pos)				\
	(*(xdrs)->x_ops->x_setpostn)(xdrs, pos)

#define	XDR_INLINE(xdrs, len)				\
	(*(xdrs)->x_ops->x_inline)(xdrs, len)
#define	xdr_inline(xdrs, len)				\
	(*(xdrs)->x_ops->x_inline)(xdrs, len)

#define	XDR_DESTROY(xdrs)				\
	(*(xdrs)->x_ops->x_destroy)(xdrs)
#define	xdr_destroy(xdrs)				\
	(*(xdrs)->x_ops->x_destroy)(xdrs)

#define	XDR_CONTROL(xdrs, req, op)			\
	(*(xdrs)->x_ops->x_control)(xdrs, req, op)
#define	xdr_control(xdrs, req, op)			\
	(*(xdrs)->x_ops->x_control)(xdrs, req, op)

/*
 * Support struct for discriminated unions.
 * You create an array of xdrdiscrim structures, terminated with
 * a entry with a null procedure pointer.  The xdr_union routine gets
 * the discriminant value and then searches the array of structures
 * for a matching value.  If a match is found the associated xdr routine
 * is called to handle that part of the union.  If there is
 * no match, then a default routine may be called.
 * If there is no match and no default routine it is an error.
 */


/*
 * A xdrproc_t exists for each data type which is to be encoded or decoded.
 *
 * The second argument to the xdrproc_t is a pointer to an opaque pointer.
 * The opaque pointer generally points to a structure of the data type
 * to be decoded.  If this pointer is 0, then the type routines should
 * allocate dynamic storage of the appropriate size and return it.
 * bool_t	(*xdrproc_t)(XDR *, void *);
 */
#ifdef __cplusplus
typedef bool_t (*xdrproc_t)(XDR *, void *);
#else
#ifdef __STDC__
typedef bool_t (*xdrproc_t)(); /* For Backward compatibility */
#else
typedef	bool_t (*xdrproc_t)();
#endif
#endif

#define	NULL_xdrproc_t ((xdrproc_t)0)

#if defined(_LP64) || defined(_I32LPx)
#define	xdr_rpcvers(xdrs, versp)	xdr_u_int(xdrs, versp)
#define	xdr_rpcprog(xdrs, progp)	xdr_u_int(xdrs, progp)
#define	xdr_rpcproc(xdrs, procp)	xdr_u_int(xdrs, procp)
#define	xdr_rpcprot(xdrs, protp)	xdr_u_int(xdrs, protp)
#define	xdr_rpcport(xdrs, portp)	xdr_u_int(xdrs, portp)
#else
#define	xdr_rpcvers(xdrs, versp)	xdr_u_long(xdrs, versp)
#define	xdr_rpcprog(xdrs, progp)	xdr_u_long(xdrs, progp)
#define	xdr_rpcproc(xdrs, procp)	xdr_u_long(xdrs, procp)
#define	xdr_rpcprot(xdrs, protp)	xdr_u_long(xdrs, protp)
#define	xdr_rpcport(xdrs, portp)	xdr_u_long(xdrs, portp)
#endif

struct xdr_discrim {
	int	value;
	xdrproc_t proc;
};

/*
 * In-line routines for fast encode/decode of primitve data types.
 * Caveat emptor: these use single memory cycles to get the
 * data from the underlying buffer, and will fail to operate
 * properly if the data is not aligned.  The standard way to use these
 * is to say:
 *	if ((buf = XDR_INLINE(xdrs, count)) == NULL)
 *		return (FALSE);
 *	<<< macro calls >>>
 * where ``count'' is the number of bytes of data occupied
 * by the primitive data types.
 *
 * N.B. and frozen for all time: each data type here uses 4 bytes
 * of external representation.
 */

#define	IXDR_GET_INT32(buf)		((int32_t)ntohl((uint32_t)*(buf)++))
#define	IXDR_PUT_INT32(buf, v)		(*(buf)++ = (int32_t)htonl((uint32_t)v))
#define	IXDR_GET_U_INT32(buf)		((uint32_t)IXDR_GET_INT32(buf))
#define	IXDR_PUT_U_INT32(buf, v)	IXDR_PUT_INT32((buf), ((int32_t)(v)))

#if !defined(_KERNEL) && !defined(_LP64)

#define	IXDR_GET_LONG(buf)		((long)ntohl((ulong_t)*(buf)++))
#define	IXDR_PUT_LONG(buf, v)		(*(buf)++ = (long)htonl((ulong_t)v))
#define	IXDR_GET_U_LONG(buf)		((ulong_t)IXDR_GET_LONG(buf))
#define	IXDR_PUT_U_LONG(buf, v)		IXDR_PUT_LONG((buf), ((long)(v)))

#define	IXDR_GET_BOOL(buf)		((bool_t)IXDR_GET_LONG(buf))
#define	IXDR_GET_ENUM(buf, t)		((t)IXDR_GET_LONG(buf))
#define	IXDR_GET_SHORT(buf)		((short)IXDR_GET_LONG(buf))
#define	IXDR_GET_U_SHORT(buf)		((ushort_t)IXDR_GET_LONG(buf))

#define	IXDR_PUT_BOOL(buf, v)		IXDR_PUT_LONG((buf), ((long)(v)))
#define	IXDR_PUT_ENUM(buf, v)		IXDR_PUT_LONG((buf), ((long)(v)))
#define	IXDR_PUT_SHORT(buf, v)		IXDR_PUT_LONG((buf), ((long)(v)))
#define	IXDR_PUT_U_SHORT(buf, v)	IXDR_PUT_LONG((buf), ((long)(v)))

#else

#define	IXDR_GET_BOOL(buf)		((bool_t)IXDR_GET_INT32(buf))
#define	IXDR_GET_ENUM(buf, t)		((t)IXDR_GET_INT32(buf))
#define	IXDR_GET_SHORT(buf)		((short)IXDR_GET_INT32(buf))
#define	IXDR_GET_U_SHORT(buf)		((ushort_t)IXDR_GET_INT32(buf))

#define	IXDR_PUT_BOOL(buf, v)		IXDR_PUT_INT32((buf), ((int)(v)))
#define	IXDR_PUT_ENUM(buf, v)		IXDR_PUT_INT32((buf), ((int)(v)))
#define	IXDR_PUT_SHORT(buf, v)		IXDR_PUT_INT32((buf), ((int)(v)))
#define	IXDR_PUT_U_SHORT(buf, v)	IXDR_PUT_INT32((buf), ((int)(v)))

#endif

#ifndef _LITTLE_ENDIAN
#define	IXDR_GET_HYPER(buf, v)	{ \
			*((int32_t *)(&v)) = ntohl(*(uint32_t *)buf++); \
			*((int32_t *)(((char *)&v) + BYTES_PER_XDR_UNIT)) \
			= ntohl(*(uint32_t *)buf++); \
			}
#define	IXDR_PUT_HYPER(buf, v)	{ \
			*(buf)++ = (int32_t)htonl(*(uint32_t *) \
				((char *)&v)); \
			*(buf)++ = \
				(int32_t)htonl(*(uint32_t *)(((char *)&v) \
				+ BYTES_PER_XDR_UNIT)); \
			}
#else

#define	IXDR_GET_HYPER(buf, v)	{ \
			*((int32_t *)(((char *)&v) + \
				BYTES_PER_XDR_UNIT)) \
				= ntohl(*(uint32_t *)buf++); \
			*((int32_t *)(&v)) = \
				ntohl(*(uint32_t *)buf++); \
			}

#define	IXDR_PUT_HYPER(buf, v)	{ \
			*(buf)++ = \
				(int32_t)htonl(*(uint32_t *)(((char *)&v) + \
				BYTES_PER_XDR_UNIT)); \
			*(buf)++ = \
				(int32_t)htonl(*(uint32_t *)((char *)&v)); \
			}
#endif
#define	IXDR_GET_U_HYPER(buf, v)	IXDR_GET_HYPER(buf, v)
#define	IXDR_PUT_U_HYPER(buf, v)	IXDR_PUT_HYPER(buf, v)


/*
 * These are the "generic" xdr routines.
 */
#ifdef __STDC__
extern bool_t	xdr_void(void);
extern bool_t	xdr_int(XDR *, int *);
extern bool_t	xdr_u_int(XDR *, uint_t *);
extern bool_t	xdr_long(XDR *, long *);
extern bool_t	xdr_u_long(XDR *, ulong_t *);
extern bool_t	xdr_short(XDR *, short *);
extern bool_t	xdr_u_short(XDR *, ushort_t *);
extern bool_t	xdr_bool(XDR *, bool_t *);
extern bool_t	xdr_enum(XDR *, enum_t *);
extern bool_t	xdr_array(XDR *, caddr_t *, uint_t *, const uint_t,
		    const uint_t, const xdrproc_t);
extern bool_t	xdr_bytes(XDR *, char **, uint_t *, const uint_t);
extern bool_t	xdr_opaque(XDR *, caddr_t, const uint_t);
extern bool_t	xdr_string(XDR *, char **, const uint_t);
extern bool_t	xdr_union(XDR *, enum_t *, char *,
		    const struct xdr_discrim *, const xdrproc_t);
extern unsigned int  xdr_sizeof(xdrproc_t, void *);

extern bool_t   xdr_hyper(XDR *, longlong_t *);
extern bool_t   xdr_longlong_t(XDR *, longlong_t *);
extern bool_t   xdr_u_hyper(XDR *, u_longlong_t *);
extern bool_t   xdr_u_longlong_t(XDR *, u_longlong_t *);

extern bool_t	xdr_char(XDR *, char *);
extern bool_t	xdr_wrapstring(XDR *, char **);
extern bool_t	xdr_reference(XDR *, caddr_t *, uint_t, const xdrproc_t);
extern bool_t	xdr_pointer(XDR *, char **, uint_t, const xdrproc_t);
extern void	xdr_free(xdrproc_t, char *);
extern bool_t	xdr_time_t(XDR *, time_t *);

extern bool_t	xdr_int8_t(XDR *, int8_t *);
extern bool_t	xdr_uint8_t(XDR *, uint8_t *);
extern bool_t	xdr_int16_t(XDR *, int16_t *);
extern bool_t	xdr_uint16_t(XDR *, uint16_t *);
extern bool_t	xdr_int32_t(XDR *, int32_t *);
extern bool_t	xdr_uint32_t(XDR *, uint32_t *);
#if defined(_INT64_TYPE)
extern bool_t	xdr_int64_t(XDR *, int64_t *);
extern bool_t	xdr_uint64_t(XDR *, uint64_t *);
#endif

#ifndef _KERNEL
extern bool_t	xdr_u_char(XDR *, uchar_t *);
extern bool_t	xdr_vector(XDR *, char *, const uint_t, const uint_t, const
xdrproc_t);
extern bool_t	xdr_float(XDR *, float *);
extern bool_t	xdr_double(XDR *, double *);
extern bool_t	xdr_quadruple(XDR *, long double *);
#endif /* !_KERNEL */
#else
extern bool_t	xdr_void();
extern bool_t	xdr_int();
extern bool_t	xdr_u_int();
extern bool_t	xdr_long();
extern bool_t	xdr_u_long();
extern bool_t	xdr_short();
extern bool_t	xdr_u_short();
extern bool_t	xdr_bool();
extern bool_t	xdr_enum();
extern bool_t	xdr_array();
extern bool_t	xdr_bytes();
extern bool_t	xdr_opaque();
extern bool_t	xdr_string();
extern bool_t	xdr_union();

extern bool_t   xdr_hyper();
extern bool_t   xdr_longlong_t();
extern bool_t   xdr_u_hyper();
extern bool_t   xdr_u_longlong_t();
extern bool_t	xdr_char();
extern bool_t	xdr_reference();
extern bool_t	xdr_pointer();
extern void	xdr_free();
extern bool_t	xdr_wrapstring();
extern bool_t	xdr_time_t();

extern bool_t	xdr_int8_t();
extern bool_t	xdr_uint8_t();
extern bool_t	xdr_int16_t();
extern bool_t	xdr_uint16_t();
extern bool_t	xdr_int32_t();
extern bool_t	xdr_uint32_t();
#if defined(_INT64_TYPE)
extern bool_t	xdr_int64_t();
extern bool_t	xdr_uint64_t();
#endif

#ifndef _KERNEL
extern bool_t	xdr_u_char();
extern bool_t	xdr_vector();
extern bool_t	xdr_float();
extern bool_t	xdr_double();
extern bool_t   xdr_quadruple();
#endif /* !_KERNEL */
#endif

/*
 * Common opaque bytes objects used by many rpc protocols;
 * declared here due to commonality.
 */
#define	MAX_NETOBJ_SZ 1024
struct netobj {
	uint_t	n_len;
	char	*n_bytes;
};
typedef struct netobj netobj;

#ifdef __STDC__
extern bool_t   xdr_netobj(XDR *, netobj *);
#else
extern bool_t   xdr_netobj();
#endif

/*
 * These are XDR control operators
 */

#define	XDR_GET_BYTES_AVAIL 1

struct xdr_bytesrec {
	bool_t xc_is_last_record;
	size_t xc_num_avail;
};

typedef struct xdr_bytesrec xdr_bytesrec;

/*
 * These are the request arguments to XDR_CONTROL.
 *
 * XDR_PEEK - returns the contents of the next XDR unit on the XDR stream.
 * XDR_SKIPBYTES - skips the next N bytes in the XDR stream.
 * XDR_RDMAGET - for xdr implementation over RDMA, gets private flags from
 *		 the XDR stream being moved over RDMA
 * XDR_RDMANOCHUNK - for xdr implementaion over RDMA, sets private flags in
 *                   the XDR stream moving over RDMA.
 */
#ifdef _KERNEL
#define	XDR_PEEK		2
#define	XDR_SKIPBYTES		3
#define	XDR_RDMAGET		4
#define	XDR_RDMASET		5
#endif

/*
 * These are the public routines for the various implementations of
 * xdr streams.
 */
#ifndef _KERNEL
#ifdef __STDC__
extern void   xdrmem_create(XDR *, const caddr_t, const uint_t, const enum
xdr_op);
	/* XDR using memory buffers */
extern void   xdrrec_create(XDR *, const uint_t, const uint_t, const caddr_t,
int (*) (void *, caddr_t, int), int (*) (void *, caddr_t, int));
/* XDR pseudo records for tcp */
extern bool_t xdrrec_endofrecord(XDR *, bool_t);
/* make end of xdr record */
extern bool_t xdrrec_skiprecord(XDR *);
/* move to beginning of next record */
extern bool_t xdrrec_eof(XDR *);
extern uint_t xdrrec_readbytes(XDR *, caddr_t, uint_t);
/* true if no more input */
#else
extern void   xdrmem_create();
extern void   xdrstdio_create();
extern void   xdrrec_create();
extern bool_t xdrrec_endofrecord();
extern bool_t xdrrec_skiprecord();
extern bool_t xdrrec_eof();
extern uint_t xdrrec_readbytes();
#endif
#else

extern void	xdrmem_create(XDR *, caddr_t, uint_t, enum xdr_op);

extern struct xdr_ops xdrmblk_ops;

struct rpc_msg;
extern bool_t	xdr_callmsg(XDR *, struct rpc_msg *);
extern bool_t	xdr_replymsg_body(XDR *, struct rpc_msg *);
extern bool_t	xdr_replymsg_hdr(XDR *, struct rpc_msg *);

#include <sys/malloc.h>
#ifdef	mem_alloc
#undef	mem_alloc
#define	mem_alloc(size)		malloc((size), M_TEMP, M_WAITOK | M_ZERO)
#endif
#ifdef	mem_free
#undef	mem_free
#define	mem_free(ptr, size)	free((ptr), M_TEMP)
#endif

#endif /* !_KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* !_RPC_XDR_H */
