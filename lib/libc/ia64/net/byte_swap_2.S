/* $FreeBSD$ */
/*	$NetBSD: byte_swap_2.S,v 1.2 1996/10/17 03:08:08 cgd Exp $	*/

/*
 * Copyright (c) 1996 Carnegie-Mellon University.
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

#include <machine/asm.h>

#ifndef NAME
#define	NAME byte_swap_2
#endif

/*
 * Byte-swap a 2-byte quantity.  (Convert 0x0123 to 0x2301.)
 *
 * Argument is an unsigned 2-byte integer (u_int16_t).
 */
ENTRY(NAME, 1)				/* in0  = 0x0123 */
	extr.u	r2=in0,8,8		/* r2   = 0x  01 */
	dep	r3=in0,r0,8,8		/* r3   = 0x23   */
	;; 
	or	ret0=r2,r3		/* ret0 = 0x2301 */
	br.ret.sptk.few rp
END(NAME)
