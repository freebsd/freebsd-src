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
#error NAME not defined
#endif

/*
 * Byte-swap a 4-byte quantity.  (Convert 0x01234567 to 0x67452301.)
 *
 * Argument is an unsigned 4-byte integer (u_int32_t).
 */
ENTRY(NAME, 1)
	mux1	r16=in0,@rev
	;;
	extr.u	r8=r16,32,32
	br.ret.sptk.few rp
END(NAME)
