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
 *	boot2.S,v 1.6 1995/01/25 21:37:40 bde Exp
 */
/*
 * Ported to PC-9801 by Yoshio Kimura
 */

#include	"asm.h"

/* Conventional GDT indexes. */
#define BOOT_CS_INDEX		3
#define BOOT_CS16_INDEX		5
#define BOOT_DS_INDEX		4

#ifdef BDE_DEBUGGER
#define DB_CS_INDEX		14
#define DB_CS16_INDEX		15
#define DB_DS_INDEX		16
#define GDT_INDEX		17
#endif

/* Vector numbers. */
#define BREAKPOINT_VECTOR	3
#define DEBUG_VECTOR		1

/*
 * boot2() -- second stage boot
 * SP points to default string if found
 */

ENTRY(boot2)
	data32
	subl	%eax, %eax
	mov	%cs, %ax
	mov	%ax, %ds
	mov	%ax, %es
	data32
	shll	$4, %eax
#ifdef NAMEBLOCK
	addr32
	data32
	movl	%esp, EXT(dflt_name)
#endif

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

	/* fix up GDT pointer */
	data32
	movl	%eax, %ecx
	data32
	addl	$ EXT(Gdt), %eax
	addr32
	data32
	movl	%eax, EXT(Gdtr)+2

#ifdef BDE_DEBUGGER
	/* fix up GDT entry for GDT */
	data32
	shld	$16, %eax, %ebx
	FIXUP(GDT_INDEX)

	/* fix up IDT pointer */
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
	data32
	subl	$2, %ebx		/* calculate EA to check it */
	jb	1f			/* give up if it would trap */
	addr32
	movl	%es: (%ebx), %eax	/* actually movw to %ax */
	addr32
	movl	%eax, EXT(Idt)+8*DEBUG_VECTOR	/* actually movw %ax */
1:
	data32
	subl	$2, %ecx
	jb	1f
	addr32
	movl	%es: (%ecx), %eax	/* actually movw to %ax */
	addr32
	movl	%eax, EXT(Idt)+8*BREAKPOINT_VECTOR	/* actually movw %ax */
1:

	/* finished with groping in real mode segments */
	pop	%es
#endif /* BDE_DEBUGGER */

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
