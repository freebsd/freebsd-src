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
 *	$Id: apm_setup.s,v 1.5 1995/02/17 02:22:23 phk Exp $
 */

#include "apm.h"
  
#if NAPM > 0

#define ASSEMBLER
#include "assym.s"                    /* system definitions */
#include <machine/asmacros.h>         /* miscellaneous asm macros */
#include <machine/apm_bios.h>
#include <machine/apm_segments.h>
#define PADDR(addr)        addr-KERNBASE
  
	.file	"apm_setup.s"

	.data
_apm_init_image:
	.globl	_apm_init_image

1:
#include "i386/apm/apm_init/apm_init.inc"
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
	.globl  _apm_current_gdt_pdesc          /* current GDT pseudo desc. */
_apm_current_gdt_pdesc:
	.word   0, 0, 0

	.globl  _bootstrap_gdt
_bootstrap_gdt:
	.space  SIZEOF_GDT*BOOTSTRAP_GDT_NUM 

	.text
_apm_setup:
	.globl	_apm_setup

	/*
	 * Setup APM BIOS:
	 *
	 * APM BIOS initialization should be done from real mode or V86 mode.
	 *
	 * (by HOSOKAWA, Tatsumi <hosokawa@mt.cs.keio.ac.jp>)
	 */

	/*
	 * Don't trust the value of %fs and %gs (some AT-compatible BIOS 
	 * implementations leave junk values in these segment registers
	 * on bootstrap)
	 */
	xorl	%eax, %eax	/* null selector */
	movw	%ax, %fs
	movw	%ax, %gs

	/* 
         * Copy APM initializer under 1MB boundary:
	 *
	 * APM initializer program must switch the CPU to real mode.
	 * But FreeBSD kernel runs above 1MB boundary. So we must 
	 * copy the initializer code to conventional memory.
	 */
	movl	PADDR(_apm_init_image_size), %ecx	/* size */
	lea	PADDR(_apm_init_image), %esi		/* source */
	movl	$ APM_OURADDR, %edi			/* destination */
	cld
	rep
	movsb

	/* get GDT base */
	sgdt	PADDR(_apm_current_gdt_pdesc)

	/* copy GDT to _bootstrap_gdt */
	xorl	%ecx, %ecx
	movw	PADDR(_apm_current_gdt_pdesc), %cx
	movl	PADDR(_apm_current_gdt_pdesc + 2), %esi
	lea	PADDR(_bootstrap_gdt), %edi
	cld
	rep
	movsb

	/* setup GDT pseudo descriptor */
	movw	$(SIZEOF_GDT*BOOTSTRAP_GDT_NUM), %ax
	movw	%ax, PADDR(_apm_current_gdt_pdesc)
	leal	PADDR(_bootstrap_gdt), %eax
	movl	%eax, PADDR(_apm_current_gdt_pdesc + 2)

	/* load new GDTR */
	lgdt	PADDR(_apm_current_gdt_pdesc)

	/* setup GDT for APM initializer */
	lea	PADDR(_bootstrap_gdt), %ecx
	movl	$(APM_OURADDR), %eax	/* use %ax for 15..0 */
	movl	%eax, %ebx
	shrl	$16, %ebx		/* use %bl for 23..16 */
					/* use %bh for 31..24 */
#define APM_SETUP_GDT(index, attrib) \
	movl	$(index), %si ; \
	lea	0(%ecx,%esi,8), %edx ; \
	movw	$0xffff, (%edx) ; \
	movw	%ax, 2(%edx) ; \
	movb	%bl, 4(%edx) ; \
	movw	$(attrib), 5(%edx) ; \
	movb	%bh, 7(%edx)

	APM_SETUP_GDT(APM_INIT_CS_INDEX  , CS32_ATTRIB)
	APM_SETUP_GDT(APM_INIT_DS_INDEX  , DS32_ATTRIB)
	APM_SETUP_GDT(APM_INIT_CS16_INDEX, CS16_ATTRIB)
	APM_SETUP_GDT(APM_INIT_DS16_INDEX, DS16_ATTRIB)

	/*
	 * Call the initializer:
	 *
	 * direct intersegment call to conventional memory code
	 */
	.byte	0x9a		/* actually, lcall $APM_INIT_CS_SEL, $0 */
	.long	0
	.word	APM_INIT_CS_SEL

	movl	%eax, PADDR(_apm_version)
	movl	%ebx, PADDR(_apm_cs_entry)
	movw	%cx, PADDR(_apm_cs32_base)
	shrl	$16, %ecx
	movw	%cx, PADDR(_apm_cs16_base)
	movw	%dx, PADDR(_apm_ds_base)
	movw	%si, PADDR(_apm_cs_limit)
	shrl	$16, %esi
	movw	%si, PADDR(_apm_ds_limit)
	movw	%di, PADDR(_apm_flags)

	ret
#endif NAPM > 0
