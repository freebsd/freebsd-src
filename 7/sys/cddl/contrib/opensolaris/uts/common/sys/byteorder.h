/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
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
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1983, 1984, 1985, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * University Copyright- Copyright (c) 1982, 1986, 1988
 * The Regents of the University of California
 * All Rights Reserved
 *
 * University Acknowledgment- Portions of this document are derived from
 * software developed by the University of California, Berkeley, and its
 * contributors.
 */

#ifndef _SYS_BYTEORDER_H
#define	_SYS_BYTEORDER_H

#include <sys/isa_defs.h>
#include <sys/int_types.h>

#if defined(__GNUC__) && defined(_ASM_INLINES) && \
	(defined(__i386) || defined(__amd64))
#include <asm/byteorder.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * macros for conversion between host and (internet) network byte order
 */

#if BYTE_ORDER == _BIG_ENDIAN && !defined(ntohl) && !defined(__lint)
/* big-endian */
#define	ntohl(x)	(x)
#define	ntohs(x)	(x)
#define	htonl(x)	(x)
#define	htons(x)	(x)
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	ntohll(x)	(x)
#define	htonll(x)	(x)
#endif	/* !_XPG4_2 || __EXTENSIONS__ */

#elif !defined(ntohl) /* little-endian */

#ifndef	_IN_PORT_T
#define	_IN_PORT_T
typedef uint16_t in_port_t;
#endif

#ifndef	_IN_ADDR_T
#define	_IN_ADDR_T
typedef uint32_t in_addr_t;
#endif

#if !defined(_XPG4_2) || defined(__EXTENSIONS__) || defined(_XPG5)
extern	uint32_t htonl(uint32_t);
extern	uint16_t htons(uint16_t);
extern	uint32_t ntohl(uint32_t);
extern	uint16_t ntohs(uint16_t);
#else
extern	in_addr_t htonl(in_addr_t);
extern	in_port_t htons(in_port_t);
extern	in_addr_t ntohl(in_addr_t);
extern	in_port_t ntohs(in_port_t);
#endif	/* !_XPG4_2 || __EXTENSIONS__ || _XPG5 */

#if defined(_LP64) || defined(_LONGLONG_TYPE)
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
extern	uint64_t htonll(uint64_t);
extern	uint64_t ntohll(uint64_t);
#endif	/* !_XPG4_2 || __EXTENSIONS__ */
#endif	/* _LP64 || _LONGLONG_TYPE  */
#endif

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)

/*
 * Macros to reverse byte order
 */
#define	BSWAP_8(x)	((x) & 0xff)
#if !defined(__i386) && !defined(__amd64)
#define	BSWAP_16(x)	((BSWAP_8(x) << 8) | BSWAP_8((x) >> 8))
#define	BSWAP_32(x)	(((uint32_t)(x) << 24) | \
			(((uint32_t)(x) << 8) & 0xff0000) | \
			(((uint32_t)(x) >> 8) & 0xff00) | \
			((uint32_t)(x)  >> 24))
#else /* x86 */
#define	BSWAP_16(x)	htons(x)
#define	BSWAP_32(x)	htonl(x)
#endif	/* !__i386 && !__amd64 */

#if defined(_LP64) || defined(_LONGLONG_TYPE)
#if (!defined(__i386) && !defined(__amd64))
#define	BSWAP_64(x)	(((uint64_t)(x) << 56) | \
			(((uint64_t)(x) << 40) & 0xff000000000000ULL) | \
			(((uint64_t)(x) << 24) & 0xff0000000000ULL) | \
			(((uint64_t)(x) << 8)  & 0xff00000000ULL) | \
			(((uint64_t)(x) >> 8)  & 0xff000000ULL) | \
			(((uint64_t)(x) >> 24) & 0xff0000ULL) | \
			(((uint64_t)(x) >> 40) & 0xff00ULL) | \
			((uint64_t)(x)  >> 56))
#else /* x86 */
#define	BSWAP_64(x)	htonll(x)
#endif	/* !__i386 && !__amd64 */
#else /* no uint64_t */
#define	BSWAP_64(x)	((BSWAP_32(x) << 32) | BSWAP_32((x) >> 32))
#endif	/* _LP64 || _LONGLONG_TYPE  */

#define	BMASK_8(x)	((x) & 0xff)
#define	BMASK_16(x)	((x) & 0xffff)
#define	BMASK_32(x)	((x) & 0xffffffff)
#define	BMASK_64(x)	(x)

/*
 * Macros to convert from a specific byte order to/from native byte order
 */
#if BYTE_ORDER == _BIG_ENDIAN
#define	BE_8(x)		BMASK_8(x)
#define	BE_16(x)	BMASK_16(x)
#define	BE_32(x)	BMASK_32(x)
#define	BE_64(x)	BMASK_64(x)
#define	LE_8(x)		BSWAP_8(x)
#define	LE_16(x)	BSWAP_16(x)
#define	LE_32(x)	BSWAP_32(x)
#define	LE_64(x)	BSWAP_64(x)
#else
#define	LE_8(x)		BMASK_8(x)
#define	LE_16(x)	BMASK_16(x)
#define	LE_32(x)	BMASK_32(x)
#define	LE_64(x)	BMASK_64(x)
#define	BE_8(x)		BSWAP_8(x)
#define	BE_16(x)	BSWAP_16(x)
#define	BE_32(x)	BSWAP_32(x)
#define	BE_64(x)	BSWAP_64(x)
#endif

#endif	/* !_XPG4_2 || __EXTENSIONS__ */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_BYTEORDER_H */
