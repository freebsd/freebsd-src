/* $FreeBSD$ */
/*	$NetBSD: brk.S,v 1.4 1996/10/17 03:08:15 cgd Exp $	*/

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

#include "SYS.h"

	.globl	_end
IMPORT(curbrk, 8)

	.data
EXPORT(minbrk)
	.quad	_end

	.text
ENTRY(brk, 1)
	add	r14=@gprel(minbrk),gp ;;
	ld8	r14=[r14] ;;
	cmp.ltu	p6,p0=in0,r14 ;;
(p6)	mov	in0=r14
	CALLSYS_ERROR(break)
	add	r14=@gprel(curbrk),gp ;;
	st8	[r14]=in0
	mov	ret0=0
	br.ret.sptk.few rp
END(brk)
