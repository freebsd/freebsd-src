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
 *	$Id: asm.S,v 1.5 1994/10/06 09:41:01 rgrimes Exp $
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

	.file "asm.s"

#include "asm.h"


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

	/* load the gdtr */
	addr32
	data32
	lgdt	EXT(Gdtr)

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
	ljmp	$0x18, $xprot

xprot:
	/*
	 * we are in USE32 mode now
	 * set up the protected mode segment registers : DS, SS, ES
	 */
	mov	$0x20, %eax
	movw	%ax, %ds
	movw	%ax, %ss
	movw	%ax, %es

#ifdef BDE_DEBUGGER
	/* load idtr so we can debug */
	lidt	EXT(Idtr_prot)
#endif

	ret

/*
 *
 * prot_to_real()
 * 	transfer from protected mode to real mode
 * 
 */

ENTRY(prot_to_real)

	/* set up a dummy stack frame for the second seg change. */
	movw 	$BOOTSEG, %ax
	pushw	%ax
	movw	$xreal, %ax	/* gas botches pushw $xreal, extra bytes 0, 0 */
	pushw	%ax		/* decode to add %al, (%eax) (%al usually 0) */

	/* Change to use16 mode. */
	ljmp	$0x28, $x16

x16:
	/* clear the PE bit of CR0 */
	mov	%cr0, %eax
	data32
	and 	$CR0_PE_OFF, %eax
	mov	%eax, %cr0

	/*
	 * make intersegment jmp to flush the processor pipeline
	 * using the fake stack frame set up earlier
	 * and reload CS register
	 */
	lret

xreal:
	/*
	 * we are in real mode now
	 * set up the real mode segment registers : DS, SS, ES
	 */
	movw	%cs, %ax
	movw	%ax, %ds
	movw	%ax, %ss
	movw	%ax, %es

#ifdef BDE_DEBUGGER
	/* load idtr so we can debug */
	addr32
	data32
	lidt	EXT(Idtr_real)
#endif

	data32
	ret

/*
 * startprog(phyaddr)
 *	start the program on protected mode where phyaddr is the entry point
 *
 * XXX This whole mess should go away and we should run the boot code in
 * flat 32 bit mode with it linked -T BOOTSEG.  See the netboot code for
 * how this is done.
 */

ENTRY(startprog)
	push	%ebp
	mov	%esp, %ebp
	movl	%esp, %eax		/* Use eax as the old stack pointer */

	/* convert the current stack to a 32 bit flat model */
	mov	$0x10, %ebx
	movw	%bx, %ss
	addl	$(BOOTSEG<<4),%esp
	
	/* copy the arguments from the old stack to the new stack */
	pushl	0x14(%eax)		/* &bootinfo */
	pushl	$0			/* was &nfsdiskless */
	pushl	$0			/* was esym */
	pushl	$0			/* was cyloffset */
	pushl	0x10(%eax)		/* bootdev */
	pushl	0x0C(%eax)		/* howto */
	movl	$(ourreturn),%ebx
	addl	$(BOOTSEG<<4),%ebx	/* Fix it up for flat segments */
	pushl	%ebx			/* our return address */
	
	/* push on our entry address */
	pushl	$0x08			/* segment selector */
	pushl	0x08(%eax)		/* kernel entry address */

	/* convert over the other data segs */
	mov	$0x10, %ebx
	movw	%bx, %ds
	movw	%bx, %es

	/* convert the PC (and code seg) */
	lret
ourreturn:
	/* For now there is not much we can do, just lock in a loop */
	jmp	ourreturn

/*
 *
 * pbzero( dst, cnt)
 *	where src is a virtual address and dst is a physical address
 */

ENTRY(pbzero)
	push	%ebp
	mov	%esp, %ebp
	push	%es
	push	%esi
	push	%edi
	push	%ecx

	cld

	/* set %es to point at the flat segment */
	mov	$0x10, %eax
	movw	%ax, %es

	mov	0x8(%ebp), %edi		/* destination */
	mov	0xc(%ebp), %ecx		/* count */
	mov	$0x0, %eax		/* value */

	rep
	stosb

	pop	%ecx
	pop	%edi
	pop	%esi
	pop	%es
	pop	%ebp

	ret
/*
 * pcpy(src, dst, cnt)
 *	where src is a virtual address and dst is a physical address
 */

ENTRY(pcpy)
	push	%ebp
	mov	%esp, %ebp
	push	%es
	push	%esi
	push	%edi
	push	%ecx

	cld

	/* set %es to point at the flat segment */
	mov	$0x10, %eax
	movw	%ax, %es

	mov	0x8(%ebp), %esi		/* source */
	mov	0xc(%ebp), %edi		/* destination */
	mov	0x10(%ebp), %ecx	/* count */

	rep
	movsb

	pop	%ecx
	pop	%edi
	pop	%esi
	pop	%es
	pop	%ebp

	ret

