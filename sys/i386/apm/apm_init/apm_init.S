/*
 * APM (Advanced Power Management) BIOS Device Driver
 *
 * Copyright (c) 1994-1995 by HOSOKAWA, Tatsumi <hosokawa@mt.cs.keio.ac.jp>
 *
 * This software may be used, modified, copied, and distributed, in
 * both source and binary form provided that the above copyright and
 * these terms are retained. Under no circumstances is the author 
 * responsible for the proper functioning of this software, nor does 
 * the author assume any responsibility for damages incurred with its 
 * use.
 *
 * Sep., 1994	Implemented on FreeBSD 1.1.5.1R (Toshiba AVS001WD)
 *
 *	$Id: apm_init.S,v 1.4.4.2 1996/03/12 21:28:18 nate Exp $
 */

/*
 * If you want to know the specification of APM BIOS, see the following
 * documentations,
 *
 * [1] Intel Corporation and Microsoft Corporation, "Advanced Power 
 *     Management, The Next Generation, Version 1.0", Feb.,1992.
 *
 * [2] Intel Corporation and Microsoft Corporation, "Advanced Power
 *     Management (APM) BIOS Interface Specification Revision 1.1",
 *     Sep.,1993, Intel Order Number: 241704-001, Microsoft Part
 *     Number: 781-110-X01
 *
 * or contact
 *
 * APM Support Desk (Intel Corporation, US)
 *   TEL: (800)628-8686 
 *   FAX: (916)356-6100.
 */

	.file	"apm_init.S"

#define ASM

#include "real_prot.h"
#include "apm_bios.h"
#include "apm_segments.h"

/*
 * APM BIOS initializer
 *
 * Return value:
 *	%eax	0xffffffff	Can't find APM BIOS
 *		0xfffffffe	Don't support 32bit connection
 *		0xfffffffd	Connection error
 *		otherwise	APM version (16bit BCD format)
 *	%ebx			APM cs entry offset (32bit)
 *	%ecx	lower 16bit	APM 16bit cs base (real mode segment)
 *		upper 16bit	APM 32bit cs base (real mode segment)
 *	%edx	lower 16bit	APM ds limit (real mode segment)
 *		upper 16bit	[Reserved]
 *	%esi	lower 16bit	APM cs limit (APM 1.1 or later)
 *		upper 16bit	APM ds limit (APM 1.1 or later)
 *	%edi	bit0 = 1	16bit protected mode interface supported
 *		bit1 = 1	32bit protected mode interface supported
 *		bit2 = 1	"CPU idle" call slows processor clock speed
 *		bit3 = 1	APM BIOS Power Management disabled
 *		bit4 = 1	APM BIOS Power Management disengaged
 */

	.text
ENTRY(apm_init)
	cli				/* disable interrupt */
	pushl	%ebp			/* save original base pointer */
					/* ebp is used as a register variable */
	/* 
	 * save old data segments: We assume that %ds == %es && %ds == %ss
	 */
	pushl	%fs
	movw	%ds, %ax
	movw	%ax, %fs
	movw	$(APM_INIT_DS_SEL), %ax	/* initializer data segment */
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss
	movl	%esp, old_esp		/* save original stack pointer */
	movl	$0x10000, %esp		/* setup temporary stack */
					/* (note that it isn't 0x00000000) */

	sidt	EXT(Idtr_prot)		/* save current IDT */
	call	EXT(prot_to_real)	/* return to real mode */

	/*
	 * APM installation check
	 */
	movb	$(APM_BIOS), %ah
	movb	$(APM_INSTCHECK), %al
	data32
	movl	$(PMDV_APMBIOS), %ebx
	sti
	int	$(SYSTEM_BIOS)		/* call system BIOS */
	cli

	jnc	1f			/* if found, goto 1f */
	
	data32
	call	EXT(real_to_prot)	/* come back again to protected mode */
	movl	$(APMINI_CANTFIND), apm_version
					/* can't find APM BIOS */
	jmp	finish

1:
	movl	%eax, %edx		/* actually, movw %ax, %dx */
					/* save the value of %ax */
	data32
	call	EXT(real_to_prot)	/* come back again to protected mode */
	cmpw	$0x504d, %bx		/* "PM" signature? */
	jz	1f

	movl	$(APMINI_CANTFIND), apm_version
					/* can't find APM BIOS */
	jmp	finish

1:
	testl	$(APM_32BIT_SUPPORT), %ecx
					/* supports 32bit connection? */
	jnz	1f

	movl	$(APMINI_NOT32BIT), apm_version
					/* don't support 32bit connection */
	jmp	finish
1:
	andl	$0x0000ffff, %edx
	movl	%edx, apm_version
	andl	$0x0000ffff, %ecx
	movl	%ecx, apm_flags

	/*
	 * APM Protected Mode 32-bit Interface Connect
	 */
	call	EXT(prot_to_real)	/* return to real mode */

	/* Disconnect, just in case */
	movb	$(APM_BIOS), %ah
	movb	$(APM_DISCONNECT), %al
	data32
	movl	$(PMDV_APMBIOS), %ebx
	sti
	int	$(SYSTEM_BIOS)
	cli

	/* Ignore return code, but we can now connect safely */
	movb	$(APM_BIOS), %ah
	movb	$(APM_PROT32CONNECT), %al
	data32
	movl	$(PMDV_APMBIOS), %ebx
	sti
	int	$(SYSTEM_BIOS)
	cli
	jnc	1f			/* if successed, go to 1f */
	data32
	call	EXT(real_to_prot)
	movl	$(APMINI_CONNECTERR), apm_version
					/* connection error */
	jmp	finish
1:	
	/* save PM 32bit code segment into %bp */
	movl	%eax, %ebp		/* actually, movw %ax, %bp */
	data32
	call	EXT(real_to_prot)
	movl	$0x0000ffff, %eax
	andl	%eax, %ebp		/* 32bit cs base */
	andl	%eax, %ecx		/* 16bit cs base */
	andl	%eax, %edx		/* ds base */
	andl	%eax, %esi		/* cs length (APM 1.1 or later) */
	andl	%eax, %edi		/* ds length (APM 1.1 or later) */
					/* %ebx is code offset */
	/* pack 32bit cs and 16bit cs into %ecx */
	shll	$16, %ebp
	orl	%ebp, %ecx
	/* pack cs length and ds length into %esi */
	shll	$16, %edi
	orl	%edi, %esi
finish:
	cli
	lidt	EXT(Idtr_prot)		/* restore old IDTR */
	movl	old_esp, %esp		/* restore old stack pointer */
	movl	apm_version, %ebp	/* stored to %eax later */
	movl	apm_flags, %edi
	movw	%fs, %ax
	movw	%ax, %ss
	movw	%ax, %es
	movw	%ax, %ds
	movl	%ebp, %eax
	popl	%fs
	popl	%ebp			/* restore old base pointer */
	lret				/* restore old code segment */

	.data

	.globl	EXT(ouraddr)
LEXT(ouraddr)
	.long	APM_OURADDR

old_esp:
	.long	0
apm_version:
	.long	0
apm_flags:
	.long	0
old_ds:
	.word	0
old_es:
	.word	0
old_ss:
	.word	0
