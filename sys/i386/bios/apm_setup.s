/*
 * Copyright (C) 1994 by HOSOKAWA, Tatsumi <hosokawa@jp.FreeBSD.org>
 * Copyright (C) 1997 by Poul-Henning Kamp <phk@FreeBSD.org>
 *
 * This software may be used, modified, copied, distributed, and sold,
 * in both source and binary form provided that the above copyright and
 * these terms are retained. Under no circumstances is the author 
 * responsible for the proper functioning of this software, nor does 
 * the author assume any responsibility for damages incurred with its 
 * use.
 *
 * Sep., 1994	Implemented on FreeBSD 1.1.5.1R (Toshiba AVS001WD)
 *
 *	$Id: apm_setup.s,v 1.13 1998/06/03 01:59:34 msmith Exp $
 *
 * This file now contains no setup code.
 */

#define ASSEMBLER
#include "assym.s"                    /* system definitions */
#include <machine/asmacros.h>         /* miscellaneous asm macros */
#include <machine/apm_bios.h>
#include <machine/apm_segments.h>
#define PADDR(addr)        addr-KERNBASE
  
	.file	"apm_setup.s"

	.text
	.align 2
	.globl	_apm_bios_call
_apm_bios_call:
	pushl	%ebp
	movl	8(%esp),%ebp
	pushl	%esi
	pushl	%edi
	pushl	%ebx
	movl	20(%ebp),%edi
	movl	16(%ebp),%esi
	movl	12(%ebp),%edx
	movl	8(%ebp),%ecx
	movl	4(%ebp),%ebx
	movl	0(%ebp),%eax
	pushl	%ebp
	lcall	_apm_addr
	popl	%ebp
	movl	%eax,0(%ebp)
	jc	1f
	xorl	%eax,%eax
	jz	2f
1:	movl	$1, %eax
2:	movl	%ebx,4(%ebp)
	movl	%ecx,8(%ebp)
	movl	%edx,12(%ebp)
	movl	%esi,16(%ebp)
	movl	%edi,20(%ebp)
	popl	%ebx
	popl	%edi
	popl	%esi
	popl	%ebp
	ret
