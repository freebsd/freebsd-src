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
 *	$Id: apm_setup.s,v 1.2 1994/10/01 05:12:24 davidg Exp $
 */

#ifdef APM

#define ASM

#include <machine/asmacros.h>
#include "assym.s"
#include <machine/apm_bios.h>

	.file	"apm_setup.s"

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
