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
 * Return 0 for error, len of string but at max a1 otherwise
 *
 * Note: for performance reasons we deliberately accept that a user may
 *       make strlen_user and strnlen_user access the first few KSEG0
 *       bytes.  There's nothing secret there ...
 */
LEAF(__strnlen_user_asm)
	lw	v0, THREAD_CURDS($28)	# pointer ok?
	and	v0, a0
	bltz	v0, fault

FEXPORT(__strnlen_user_nocheck_asm)
	.type	__strnlen_user_nocheck_asm,@function
	move	v0, a0
	addu	a1, a0			# stop pointer
	.set	noreorder
1:	beq	v0, a1, 1f		# limit reached?
	 addiu	v0, 1
	.set	reorder
	EX(lb, t0, -1(v0), fault)
	bnez	t0, 1b
1:	subu	v0, a0
	jr	ra
	END(__strnlen_user_asm)

fault:	move	v0, zero
	jr	ra
