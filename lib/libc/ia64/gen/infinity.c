/*	$NetBSD: infinity.c,v 1.1 1995/02/10 17:50:23 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/endian.h>
#include <math.h>

/* bytes for +Infinity on an ia64 (IEEE double format) */
#if _BYTE_ORDER == _LITTLE_ENDIAN
const union __infinity_un __infinity = { { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f } };
#else /* _BIG_ENDIAN */
const union __infinity_un __infinity = { { 0x7f, 0xf0, 0, 0, 0, 0, 0, 0 } };
#endif

/* bytes for NaN */
#if _BYTE_ORDER == _LITTLE_ENDIAN
const union __nan_un __nan = { { 0, 0, 0xc0, 0xff } };
#else /* _BIG_ENDIAN */
const union __nan_un __nan = { { 0xff, 0xc0, 0, 0 } };
#endif
