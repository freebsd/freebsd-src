/*-
 * Copyright (c) 2001 Takanori Watanabe <takawata@jp.freebsd.org>
 * Copyright (c) 2001 Mitsuru IWASAKI <iwasaki@jp.freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#define LOCORE

#include <machine/asmacros.h>
#include <machine/param.h>
#include <machine/specialreg.h>

	.align 4
	.code16
wakeup_16:
	nop
	cli

	/*
	 * Set up segment registers for real mode and a small stack for
	 * any calls we make.
	 */
	movw	%cs,%ax
	movw	%ax,%ds
	movw	%ax,%ss
	movw	$PAGE_SIZE,%sp

	/* Re-initialize video BIOS if the reset_video tunable is set. */
	cmp	$0,reset_video
	je	wakeup_16_gdt
	lcall	$0xc000,$3

	/*
	 * Set up segment registers for real mode again in case the
	 * previous BIOS call clobbers them.
	 */
	movw	%cs,%ax
	movw	%ax,%ds
	movw	%ax,%ss

wakeup_16_gdt:
	/* Load GDT for real mode */
	lgdt	physical_gdt

	/* Restore CR2, CR3 and CR4 */
	mov	previous_cr2,%eax
	mov	%eax,%cr2
	mov	previous_cr3,%eax
	mov	%eax,%cr3
	mov	previous_cr4,%eax
	mov	%eax,%cr4

	/* Transfer some values to protected mode */
#define NVALUES	9
#define TRANSFER_STACK32(val, idx)	\
	mov	val,%eax;		\
	mov	%eax,wakeup_32stack+(idx+1)+(idx*4);

	TRANSFER_STACK32(previous_ss,		(NVALUES - 9))
	TRANSFER_STACK32(previous_fs,		(NVALUES - 8))
	TRANSFER_STACK32(previous_ds,		(NVALUES - 7))
	TRANSFER_STACK32(physical_gdt+2,	(NVALUES - 6))
	TRANSFER_STACK32(where_to_recover,	(NVALUES - 5))
	TRANSFER_STACK32(previous_idt+2,	(NVALUES - 4))
	TRANSFER_STACK32(previous_ldt,		(NVALUES - 3))
	TRANSFER_STACK32(previous_gdt+2,	(NVALUES - 2))
	TRANSFER_STACK32(previous_tr,		(NVALUES - 1))
	TRANSFER_STACK32(previous_cr0,		(NVALUES - 0))

	mov	physical_esp,%esi	/* to be used in 32bit code */

	/* Enable protected mode */
	mov	%cr0,%eax
	orl	$(CR0_PE),%eax
	mov	%eax,%cr0

wakeup_sw32:
	/* Switch to protected mode by intersegmental jump */
	ljmpl	$0x8,$0x12345678	/* Code location, to be replaced */

	.code32
wakeup_32:
	/*
	 * Switched to protected mode w/o paging
	 *	%esi:	KERNEL stack pointer (physical address)
	 */

	nop

	/* Set up segment registers for protected mode */
	movw	$0x10,%ax		/* KDSEL to segment registers */
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%gs
	movw	%ax,%ss
	movw	$0x18,%ax		/* KPSEL to %fs */
	movw	%ax,%fs
	movl	%esi,%esp		/* physical address stack pointer */

wakeup_32stack:
	/* Operands are overwritten in 16bit code */
	pushl	$0xabcdef09		/* ss + dummy */
	pushl	$0xabcdef08		/* fs + gs */
	pushl	$0xabcdef07		/* ds + es */
	pushl	$0xabcdef06		/* gdt:base (physical address) */
	pushl	$0xabcdef05		/* recover address */ 
	pushl	$0xabcdef04		/* idt:base */ 
	pushl	$0xabcdef03		/* ldt + idt:limit */
	pushl	$0xabcdef02		/* gdt:base */
	pushl	$0xabcdef01		/* TR + gdt:limit */
	pushl	$0xabcdef00		/* CR0 */

	movl	%esp,%ebp
#define CR0_REGISTER		0(%ebp)
#define TASK_REGISTER		4(%ebp)
#define PREVIOUS_GDT		6(%ebp)
#define PREVIOUS_LDT		12(%ebp)
#define PREVIOUS_IDT		14(%ebp)
#define RECOVER_ADDR		20(%ebp)
#define PHYSICAL_GDT_BASE	24(%ebp)
#define PREVIOUS_DS		28(%ebp)
#define PREVIOUS_ES		30(%ebp)
#define PREVIOUS_FS		32(%ebp)
#define PREVIOUS_GS		34(%ebp)
#define PREVIOUS_SS		36(%ebp)

	/* Fixup TSS type field */
#define TSS_TYPEFIX_MASK	0xf9
	xorl	%esi,%esi
	movl	PHYSICAL_GDT_BASE,%ebx
	movw	TASK_REGISTER,%si
	leal	(%ebx,%esi),%eax	/* get TSS segment descriptor */
	andb	$TSS_TYPEFIX_MASK,5(%eax)

	/* Prepare to return to sleep/wakeup code point */
	lgdt	PREVIOUS_GDT
	lidt	PREVIOUS_IDT

	xorl	%eax,%eax
	movl	%eax,%ebx
	movl	%eax,%ecx
	movl	%eax,%edx
	movl	%eax,%esi
	movl	%eax,%edi
	movl	PREVIOUS_DS,%ebx
	movl	PREVIOUS_FS,%ecx
	movl	PREVIOUS_SS,%edx
	movw	TASK_REGISTER,%si
	shll	$16,%esi
	movw	PREVIOUS_LDT,%si
	movl	RECOVER_ADDR,%edi

	/* Enable paging and etc. */
	movl	CR0_REGISTER,%eax
	movl	%eax,%cr0

	/* Flush the prefetch queue */
	jmp	1f
1:	jmp	1f
1:
	/*
	 * Now that we are in kernel virtual memory addressing
	 *	%ebx:	ds + es
	 *	%ecx:	fs + gs
	 *	%edx:	ss + dummy
	 *	%esi:	LDTR + TR
	 *	%edi:	recover address
	 */

	nop

	movl	%esi,%eax		/* LDTR + TR */
	lldt	%ax			/* load LDT register */
	shrl	$16,%eax
	ltr	%ax			/* load task register */

	/* Restore segment registers */
	movl	%ebx,%eax		/* ds + es */
	movw	%ax,%ds
	shrl	$16,%eax
	movw	%ax,%es
	movl	%ecx,%eax		/* fs + gs */
	movw	%ax,%fs
	shrl	$16,%eax
	movw	%ax,%gs
	movl	%edx,%eax		/* ss */
	movw	%ax,%ss

	/* Jump to acpi_restorecpu() */
	jmp	*%edi

/* used in real mode */
physical_gdt:		.word 0
			.long 0
physical_esp:		.long 0
previous_cr2:		.long 0
previous_cr3:		.long 0
previous_cr4:		.long 0
reset_video:		.long 0

/* transfer from real mode to protected mode */
previous_cr0:		.long 0
previous_tr:		.word 0
previous_gdt:		.word 0
			.long 0
previous_ldt:		.word 0
previous_idt:		.word 0
			.long 0
where_to_recover:	.long 0
previous_ds:		.word 0
previous_es:		.word 0
previous_fs:		.word 0
previous_gs:		.word 0
previous_ss:		.word 0
dummy:			.word 0
