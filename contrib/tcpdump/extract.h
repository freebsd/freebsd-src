/*
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @(#) $Header: /tcpdump/master/tcpdump/extract.h,v 1.19 2002/12/11 07:13:51 guy Exp $ (LBL)
 */

/* Network to host order macros */

#ifdef LBL_ALIGN
/*
 * The processor doesn't natively handle unaligned loads.
 */
#ifdef HAVE___ATTRIBUTE__
/*
 * We have __attribute__; we assume that means we have __attribute__((packed)).
 * Declare packed structures containing a u_int16_t and a u_int32_t,
 * cast the pointer to point to one of those, and fetch through it;
 * the GCC manual doesn't appear to explicitly say that
 * __attribute__((packed)) causes the compiler to generate unaligned-safe
 * code, but it apppears to do so.
 *
 * We do this in case the compiler can generate, for this instruction set,
 * better code to do an unaligned load and pass stuff to "ntohs()" or
 * "ntohl()" than the code to fetch the bytes one at a time and
 * assemble them.  (That might not be the case on a little-endian platform,
 * where "ntohs()" and "ntohl()" might not be done inline.)
 */
typedef struct {
	u_int16_t	val;
} __attribute__((packed)) unaligned_u_int16_t;

typedef struct {
	u_int32_t	val;
} __attribute__((packed)) unaligned_u_int32_t;

#define EXTRACT_16BITS(p) \
	((u_int16_t)ntohs(((const unaligned_u_int16_t *)(p))->val))
#define EXTRACT_32BITS(p) \
	((u_int32_t)ntohl(((const unaligned_u_int32_t *)(p))->val))
#else /* HAVE___ATTRIBUTE__ */
/*
 * We don't have __attribute__, so do unaligned loads of big-endian
 * quantities the hard way - fetch the bytes one at a time and
 * assemble them.
 */
#define EXTRACT_16BITS(p) \
	((u_int16_t)((u_int16_t)*((const u_int8_t *)(p) + 0) << 8 | \
		     (u_int16_t)*((const u_int8_t *)(p) + 1)))
#define EXTRACT_32BITS(p) \
	((u_int32_t)((u_int32_t)*((const u_int8_t *)(p) + 0) << 24 | \
		     (u_int32_t)*((const u_int8_t *)(p) + 1) << 16 | \
		     (u_int32_t)*((const u_int8_t *)(p) + 2) << 8 | \
		     (u_int32_t)*((const u_int8_t *)(p) + 3)))
#endif /* HAVE___ATTRIBUTE__ */
#else /* LBL_ALIGN */
/*
 * The processor natively handles unaligned loads, so we can just
 * cast the pointer and fetch through it.
 */
#define EXTRACT_16BITS(p) \
	((u_int16_t)ntohs(*(const u_int16_t *)(p)))
#define EXTRACT_32BITS(p) \
	((u_int32_t)ntohl(*(const u_int32_t *)(p)))
#endif /* LBL_ALIGN */

#define EXTRACT_24BITS(p) \
	((u_int32_t)((u_int32_t)*((const u_int8_t *)(p) + 0) << 16 | \
		     (u_int32_t)*((const u_int8_t *)(p) + 1) << 8 | \
		     (u_int32_t)*((const u_int8_t *)(p) + 2)))

/* Little endian protocol host order macros */

#define EXTRACT_LE_8BITS(p) (*(p))
#define EXTRACT_LE_16BITS(p) \
	((u_int16_t)((u_int16_t)*((const u_int8_t *)(p) + 1) << 8 | \
		     (u_int16_t)*((const u_int8_t *)(p) + 0)))
#define EXTRACT_LE_32BITS(p) \
	((u_int32_t)((u_int32_t)*((const u_int8_t *)(p) + 3) << 24 | \
		     (u_int32_t)*((const u_int8_t *)(p) + 2) << 16 | \
		     (u_int32_t)*((const u_int8_t *)(p) + 1) << 8 | \
		     (u_int32_t)*((const u_int8_t *)(p) + 0)))
