/*
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
 *
 *	from: Mach, Revision 2.2  92/04/04  11:34:13  rpd
 *	$Id: real_prot.S,v 1.3 1996/03/11 21:41:32 nate Exp $
 */


/*
  Copyright 1988, 1989, 1990, 1991, 1992 
   by Intel Corporation, Santa Clara, California.

                All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/*
 * Copyright (C) 1994 by HOSOKAWA, Tatsumi <hosokawa@jp.FreeBSD.org>
 *
 * This software may be used, modified, copied, and distributed, in
 * both source and binary form provided that the above copyright and
 * these terms are retained. Under no circumstances is the author 
 * responsible for the proper functioning of this software, nor does 
 * the author assume any responsibility for damages incurred with its 
 * use.
 *
 * Sep., 1994	Implemented on FreeBSD 1.1.5.1R (Toshiba AVS001WD)
 */

/*
 * Modified for APM BIOS initializer by HOSOKAWA Tatsumi
 *
 * See also locore.s.
 * (This file is based directly on /sys/i386/boot/biosboot/asm.S)
 */

	.file "real_prot.S"

#include "real_prot.h"
#include "apm_segments.h"
#include "apm_bios.h"

CR0_PE_ON	=	0x1
CR0_PE_OFF	=	0xfffffffe

	.text

/*
 *
 * real_to_prot()
 * 	transfer from real mode to protected mode.
 */

ENTRY(real_to_prot)
	/* guarantee that interrupt is disabled when in prot mode */
	cli

	/* 
	 * deleted for APM initializer by HOSOKAWA Tatsumi 
	 *                                <hosoakwa@mt.cs.keio.ac.jp>
	 */
#if 0
	/* load the gdtr */
	addr32
	data32
	lgdt	EXT(Gdtr)
#endif

	/* set the PE bit of CR0 */
	mov	%cr0, %eax

	data32
	or	$CR0_PE_ON, %eax
	mov	%eax, %cr0 

	/*
	 * make intrasegment jump to flush the processor pipeline and
	 * reload CS register
	 */
	data32
	ljmp	$(APM_INIT_CS_SEL), $xprot
xprot:

	/*
	 * we are in USE32 mode now
	 * set up the protected mode segment registers : DS, SS, ES
	 */
	movw	$(APM_INIT_DS_SEL), %ax		/* data segment */
	mov	%ax, %ds	/* gas would waste a prefix byte for movw */
	mov	%ax, %ss
	mov	%ax, %es

	/* load idtr so that we can enable interrupts */
	lidt	EXT(Idtr_prot)

	ret

/*
 *
 * prot_to_real()
 *	transfer from protected mode to real mode
 * 
 */

ENTRY(prot_to_real)

	/* Prepare %ax while we're still in a mode that gas understands. */
	movw	$(APM_INIT_DS16_SEL), %ax

	/* Change to use16 mode. */
	ljmp	$(APM_INIT_CS16_SEL), $x16
x16:

	mov	%ax, %ds
	mov	%ax, %ss
	mov	%ax, %es

	/* clear the PE bit of CR0 */
	mov	%cr0, %eax
	data32
	and 	$CR0_PE_OFF, %eax
	mov	%eax, %cr0

	/*
	 * make intersegment jmp to flush the processor pipeline
	 * and reload CS register
	 */
	data32
	ljmp	$(APM_OURADDR>>4), $xreal
xreal:

	/*
	 * we are in real mode now
	 * set up the real mode segment registers : DS, SS, ES
	 */
	mov	%cs, %ax
	mov	%ax, %ds
	mov	%ax, %ss
	mov	%ax, %es

	/* load idtr so that we can enable interrupts */
	addr32
	data32
	lidt	EXT(Idtr_real)

	data32
	ret
