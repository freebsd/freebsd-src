/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 1996, 1998, 1999 by Ralf Baechle
 * Copyright (c) 1999 Silicon Graphics, Inc.
 */
#include <asm/asm.h>
#include <asm/offset.h>
#include <asm/regdef.h>
#include <asm/sgidefs.h>

#define EX(insn,reg,addr,handler)			\
9:	insn	reg, addr;				\
	.section __ex_table,"a";			\
	PTR	9b, handler;				\
	.previous

/*
 * Return the size of a string (including the ending 0)
 *
 * Return 0 for error, len on string but at max a1 otherwise
 */
LEAF(__strnlen_user_asm)
	ld	v0, THREAD_CURDS($28)	# pointer ok?
	and	v0, a0
	bnez	v0, fault

FEXPORT(__strnlen_user_nocheck_asm)
	move	v0, a0
	daddu	a1, a0			# stop pointer
1:	beq	v0, a1, 1f		# limit reached?
	EX(lb, ta0, (v0), fault)
	daddiu	v0, 1
	bnez	ta0, 1b
1:	dsubu	v0, a0
	jr	ra
	END(__strnlen_user_asm)

fault:	move	v0, zero
	jr	ra
