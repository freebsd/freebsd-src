/* $FreeBSD: src/libexec/bootpd/bptypes.h,v 1.2.58.1 2010/02/10 00:26:20 kensmith Exp $
 */

#ifndef	BPTYPES_H
#define	BPTYPES_H

#include <sys/types.h>

/*
 * 32 bit integers are different types on various architectures
 */

#define	int32	int32_t
#define	u_int32	u_int32_t

/*
 * Nice typedefs. . .
 */

typedef int boolean;
typedef unsigned char byte;

#endif	/* BPTYPES_H */
