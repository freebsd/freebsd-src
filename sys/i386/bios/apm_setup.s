/*
 * LP (Laptop Package)
 *
 * Copyright (C) 1994 by HOSOKAWA, Tatsumi <hosokawa@mt.cs.keio.ac.jp>
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
 *	$Id$
 */

#ifdef APM

#define ASM

#include <machine/asmacros.h>
#include "assym.s"
#include <machine/apm_bios.h>

	.file	"apm_setup.s"

	.text

	/* void call_apm(union real_regs *); */
_call_apm:
	.globl	_call_apm
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%esi
	pushl	%edi
	pushl	%ebx
	pushl	%ecx

	movl	8(%ebp), %eax
	movl	%eax, struct_regs
	movw	2(%eax), %bx
	movw	4(%eax), %cx
	movw	6(%eax), %dx
	movw	8(%eax), %si
	movw	10(%eax), %di
	movw	0(%eax), %ax

	lcall	_apm_addr		/* intersegment call */

	setc	cf_result
	movb	%ah, _apm_errno
	push	%eax
	movl	struct_regs, %eax
	movw	%bx, 2(%eax)
	movw	%cx, 4(%eax)
	movw	%dx, 6(%eax)
	movw	%si, 8(%eax)
	movw	%di, 10(%eax)
	movb	cf_result, %bl
	xorb	%bh, %bh
	movw	%bx, 12(%eax)
	popl	%ebx
	movl	%ebx, 0(%eax)

	popl	%ecx
	popl	%ebx
	popl	%edi
	popl	%esi
	leave
	ret

	.data
struct_regs:
	.long	0

cf_result:
	.byte	0

_apm_errno:
	.globl	_apm_errno
	.byte	0

	.data
_apm_init_image:
	.globl	_apm_init_image

1:
#include "apm_init/apm_init.inc"
2:

_apm_init_image_size:
	.globl	_apm_init_image_size
	.long	2b - 1b

_apm_version:
	.globl	_apm_version
	.long	0

_apm_cs_entry:
	.globl	_apm_cs_entry
	.long	0

_apm_cs16_base:
	.globl	_apm_cs16_base
	.word	0

_apm_cs32_base:
	.globl	_apm_cs32_base
	.word	0

_apm_ds_base:
	.globl	_apm_ds_base
	.word	0

_apm_cs_limit:
	.globl	_apm_cs_limit
	.word	0

_apm_ds_limit:
	.globl	_apm_ds_limit
	.word	0

_apm_flags:
	.globl	_apm_flags
	.word	0

#endif	/* APM */
