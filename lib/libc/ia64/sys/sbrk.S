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

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

#include "SYS.h"

	.globl	_end

	.data
EXPORT(curbrk)
	.quad	_end

	.text
ENTRY(sbrk, 1)
	add	r14 = @ltoff(curbrk), gp
	;;
	ld8	r14 = [r14]
	cmp.eq	p6, p0 = r32, r0
	;;
	ld8	ret0 = [r14]
(p6)	br.ret.sptk.few	rp
	;;
	add	r32 = ret0, r32
	;;
	st8	[sp] = r32
	CALLSYS_ERROR(break)
	ld8	r15 = [sp]
	add	r14 = @ltoff(curbrk), gp
	;;
	ld8	r14 = [r14]
	;;
	ld8	ret0 = [r14]
	st8	[r14] = r15
	br.ret.sptk.few	rp
END(sbrk)
