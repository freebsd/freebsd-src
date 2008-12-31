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
 * $FreeBSD: src/sys/i386/acpica/acpi_wakecode.S,v 1.13.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#define LOCORE

#include <machine/asmacros.h>
#include <machine/specialreg.h>

#include "assym.s"

/*
 * Resume entry point.  The BIOS enters here in real mode after POST with
 * CS set to the page where we stored this code.  It should configure the
 * segment registers with a flat 4 GB address space and EFLAGS.IF = 0.
 * Depending on the previous sleep state, we may need to initialize more
 * of the system (i.e., S3 suspend-to-RAM vs. S4 suspend-to-disk).
 */
	.align 4
	.code16
wakeup_16:
	nop
	cli
	cld

	/*
	 * Set up segment registers for real mode, a small stack for
	 * any calls we make, and clear any flags.
	 */
	movw	%cs,%ax
	movw	%ax,%ds
	movw	%ax,%ss
	movw	$PAGE_SIZE,%sp
	pushl	$0
	popfl

	/* To debug resume hangs, beep the speaker if the user requested. */
	cmpl	$1,resume_beep
	jne	nobeep
	movb	$0xc0,%al
	outb	%al,$0x42
	movb	$0x04,%al
	outb	%al,$0x42
	inb	$0x61,%al
	orb	$0x3,%al
	outb	%al,$0x61
nobeep:

	/* Re-initialize video BIOS if the reset_video tunable is set. */
	cmpl	$1,reset_video
	jne	nobiosreset
	lcall	$0xc000,$3

	/*
	 * Set up segment registers for real mode again in case the
	 * previous BIOS call clobbers them.
	 */
	movw	%cs,%ax
	movw	%ax,%ds
	movw	%ax,%ss
nobiosreset:

	/* Load GDT for real mode.  Use 32 bit prefix for addresses >16 MB. */
	lgdtl	physical_gdt

	/* Restore CR2, CR3 and CR4 */
	movl	previous_cr2,%eax
	movl	%eax,%cr2
	movl	previous_cr3,%eax
	movl	%eax,%cr3
	movl	previous_cr4,%eax
	movl	%eax,%cr4

	/* Transfer some values to protected mode with an inline stack */
#define NVALUES	9
#define TRANSFER_STACK32(val, idx)	\
	movl	val,%eax;		\
	movl	%eax,wakeup_32stack+(idx+1)+(idx*4)

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
	movl	%cr0,%eax
	orl	$(CR0_PE),%eax
	movl	%eax,%cr0

wakeup_sw32:
	/* Switch to protected mode by intersegmental jump */
	ljmpl	$KCSEL,$0x12345678	/* Code location, to be replaced */

	/*
	 * Now switched to protected mode without paging enabled.
	 *	%esi: KERNEL stack pointer (physical address)
	 */
	.code32
wakeup_32:
	nop

	/* Set up segment registers for protected mode */
	movw	$KDSEL,%ax		/* KDSEL to segment registers */
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%gs
	movw	%ax,%ss
	movw	$KPSEL,%ax		/* KPSEL to %fs */
	movw	%ax,%fs
	movl	%esi,%esp		/* physical address stack pointer */

wakeup_32stack:
	/* Operands are overwritten in 16 bit code by TRANSFER_STACK32 macro */
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
	lgdtl	PREVIOUS_GDT
	lidtl	PREVIOUS_IDT

	/* Pack values from the GDT to be loaded into segment registers. */
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
	 * Now we are in kernel virtual memory addressing with the following
	 * original register values:
	 *	%ebx: ds + es
	 *	%ecx: fs + gs
	 *	%edx: ss + dummy
	 *	%esi: LDTR + TR
	 *	%edi: recover address
	 * We'll load these back into the segment registers now.
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
resume_beep:		.long 0
reset_video:		.long 0

/*
 * Transfer from real mode to protected mode.  The order of these variables
 * is very important, DO NOT INSERT OR CHANGE unless you know why.
 */
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
