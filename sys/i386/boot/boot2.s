/*
 * Ported to boot 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/*
 * HISTORY
 * $Log:	boot2.s,v $
 * Revision 2.2  92/04/04  11:35:26  rpd
 * 	From 2.5
 * 	[92/03/30            rvb]
 * 
 * Revision 2.2  91/04/02  14:39:21  mbj
 * 	Put into rcs tree
 * 	[90/02/09            rvb]
 * 
 */

#include	"asm.h"
#define LOADMSG 1
/*
 * boot2() -- second stage boot
 */

.globl _ouraddr

ENTRY(boot2)
	movl	%cs, %ax
	movl	%ax, %ds
	movl	%ax, %es
	data32
	sall	$4, %eax
	data32
	movl	%eax, _ouraddr
	/* save the drive type and ID */ 
	data32
	pushl	%edx
	/* change to protected mode */
	data32
	call	_real_to_prot

	call	_boot
	ret

	.data
        .align 2
_ouraddr:
        .long 0


