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
 *	from: Mach, Revision 2.2  92/04/04  11:35:26  rpd
 *	$Id: boot2.S,v 1.2 1993/10/16 19:11:33 rgrimes Exp $
 */

#include	"asm.h"

/* Conventional GDT indexes. */
#define BOOT_CS_INDEX		3
#define BOOT_CS16_INDEX		5
#define BOOT_DS_INDEX		4
#define DB_CS_INDEX		14
#define DB_CS16_INDEX		15
#define DB_DS_INDEX		16
#define GDT_INDEX		17

/* Vector numbers. */
#define BREAKPOINT_VECTOR	3
#define DEBUG_VECTOR		1

/*
 * boot2() -- second stage boot
 */

.globl EXT(ouraddr)

ENTRY(boot2)
	data32
	subl	%eax, %eax
	mov	%cs, %ax
	mov	%ax, %ds
	mov	%ax, %es
	data32
	shll	$4, %eax
	addr32
	data32
	movl	%eax, EXT(ouraddr)

	/* fix up GDT entries for bootstrap */
#define FIXUP(gdt_index) \
	addr32; \
	movl	%eax, EXT(Gdt)+(8*gdt_index)+2;	/* actually movw %ax */ \
	addr32; \
	movb	%bl, EXT(Gdt)+(8*gdt_index)+4

	data32
	shld	$16, %eax, %ebx
	FIXUP(BOOT_CS_INDEX)
	FIXUP(BOOT_CS16_INDEX)
	FIXUP(BOOT_DS_INDEX)

	/* fix up GDT entry for GDT, and GDT and IDT pointers */
	data32
	movl	%eax, %ecx
	data32
	addl	$ EXT(Gdt), %eax
	data32
	shld	$16, %eax, %ebx
	FIXUP(GDT_INDEX)
	addr32
	data32
	movl	%eax, EXT(Gdtr)+2
	data32
	addl	$ EXT(Idt), %ecx
	addr32
	data32
	movl	%ecx, EXT(Idtr_prot)+2

	/* %es = vector table segment for a while */
	push	%es
	data32
	subl	%eax, %eax
	mov	%ax, %es

	/* fix up GDT entries for bdb */
	data32
	movl	$4*DEBUG_VECTOR, %esi
	addr32
	movl	%es: 2(%esi), %eax	/* actually movw to %ax */
	data32
	shll	$4, %eax
	data32
	shld	$16, %eax, %ebx
	FIXUP(DB_CS_INDEX)
	FIXUP(DB_CS16_INDEX)
	FIXUP(DB_DS_INDEX)

	/* Fetch entry points of bdb's protected mode trap handlers.  These
	 * are stored at 2 before the corresponding entry points for real mode.
	 */
	data32
	subl	%ebx, %ebx
	addr32
	movl	%es: (%esi), %ebx	/* actually movw to %bx */
	data32
	subl	%ecx, %ecx
	addr32
	movl	%es: 4*(BREAKPOINT_VECTOR-DEBUG_VECTOR)(%esi), %ecx
					/* actually movw to %cx */

	/* %es = bdb segment for a while */
	data32
	shrl	$4, %eax
	mov	%ax, %es

	/* fix up IDT entries for bdb */
	subl	$2, %ebx
	movl	%es: (%ebx), %eax	/* actually movw to %ax */
	addr32
	movl	%eax, EXT(Idt)+8*DEBUG_VECTOR	/* actually movw %ax */
	subl	$2, %ecx
	movl	%es: (%ecx), %eax	/* actually movw to %ax */
	addr32
	movl	%eax, EXT(Idt)+8*BREAKPOINT_VECTOR	/* actually movw %ax */

	/* finished with groping in real mode segments */
	pop	%es

	/* change to protected mode */
	data32
	call	EXT(real_to_prot)

	/* clear the bss */
	movl	$ EXT(edata), %edi	/* no EXT(_edata) - krufty ld */
	movl	$ EXT(end), %ecx	/* or EXT(_end) */
	subl	%edi, %ecx
	subb	%al, %al
	rep
	stosb

	movzbl	%dl, %edx	/* discard head (%dh) and random high bits */
	pushl	%edx
	call	EXT(boot)
oops:
	hlt
	jmp	oops

	.data
        .align 2
#if 0 /* XXX this would give losing "_ouraddr :".  Better declared in C */
EXT(ouraddr):
#else
_ouraddr:
#endif
        .long 0
