/*
 * Copyright (c) 1990,1991 Regents of The University of Michigan.
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation, and that the name of The University
 * of Michigan not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. This software is supplied as is without expressed or
 * implied warranties of any kind.
 *
 *	Research Systems Unix Group
 *	The University of Michigan
 *	c/o Mike Clark
 *	535 W. William Street
 *	Ann Arbor, Michigan
 *	+1-313-763-0525
 *	netatalk@itd.umich.edu
 */

#  ifndef _ATALK_ENDIAN_H_
#define _ATALK_ENDIAN_H_

#ifdef _IBMR2
#include <sys/machine.h>
#endif _IBMR2

#ifdef linux
#include <bytesex.h>
#define BYTE_ORDER	__BYTE_ORDER
#endif linux

#ifdef __FreeBSD__
#include <machine/endian.h>
#endif __FreeBSD__

# ifndef BYTE_ORDER
#define LITTLE_ENDIAN	1234
#define BIG_ENDIAN	4321
#define PDP_ENDIAN	3412

#ifdef sun
#ifdef i386
#define BYTE_ORDER	LITTLE_ENDIAN
#else i386
#define BYTE_ORDER	BIG_ENDIAN
#endif i386
#else
#ifdef MIPSEB
#define BYTE_ORDER	BIG_ENDIAN
#else
#ifdef MIPSEL
#define BYTE_ORDER	LITTLE_ENDIAN
#else
Like, what is your byte order, man?
#endif MIPSEL
#endif MIPSEB
#endif sun
# endif BYTE_ORDER

# ifndef ntohl
# if defined( sun ) || defined( ultrix ) || defined( _IBMR2 )
#if BYTE_ORDER == BIG_ENDIAN
#define ntohl(x)	(x)
#define ntohs(x)	(x)
#define htonl(x)	(x)
#define htons(x)	(x)

#else
#if defined( mips ) && defined( KERNEL )
#define	ntohl(x)	nuxi_l(x)
#define	ntohs(x)	nuxi_s(x)
#define	htonl(x)	nuxi_l(x)
#define	htons(x)	nuxi_s(x)

#else mips KERNEL
unsigned short	ntohs(), htons();
unsigned long	ntohl(), htonl();

#endif mips KERNEL
#endif BYTE_ORDER
# endif sun ultrix _IBMR2
# endif ntohl
#  endif _ATALK_ENDIAN_H_
