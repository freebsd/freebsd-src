/* $FreeBSD$ */
/*	$NetBSD: byte_swap_4.S,v 1.2 1996/10/17 03:08:09 cgd Exp $	*/

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
#define	NAME byte_swap_4
#endif

/*
 * Byte-swap a 4-byte quantity.  (Convert 0x01234567 to 0x67452301.)
 *
 * Argument is an unsigned 4-byte integer (u_int32_t).
 */
ENTRY(NAME, 1)				/* in0  = 0x01234567 */
	extr.u	r16=in0,0,8		/* r16  = 0x      67 */
	extr.u	r17=in0,8,8		/* r17  = 0x      45 */
	extr.u	r18=in0,16,8		/* r18  = 0x      23 */
	extr.u	ret0=in0,24,8		/* ret0 = 0x      01 */
	;;
	dep	ret0=ret0,r16,24,8	/* ret0 = 0x67    01 */
	;;
	dep	ret0=ret0,r17,16,8	/* ret0 = 0x6745  01 */
	;; 
	dep	ret0=ret0,r16,8,8	/* ret0 = 0x67452301 */
	br.ret.sptk.few rp
END(NAME)
