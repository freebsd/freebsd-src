/* $FreeBSD$ */
/*	$NetBSD: sbrk.S,v 1.4 1996/10/17 03:08:20 cgd Exp $	*/

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

	.data
EXPORT(curbrk)
	.quad	_end

	.text
ENTRY(sbrk, 1)
	alloc	r14=ar.pfs,1,2,0,0

	add	loc0=@gprel(curbrk),gp ;;
	ld8	loc1=[loc0]
	cmp.eq	p6,p0=in0,r0
(p6)	br.dpnt.few 1f
	add	in0=r14,in0
	CALLSYS_ERROR(break)
	st8	[loc0]=in0
1:	mov	ret0=loc1
	br.ret.sptk.few rp
END(sbrk)
