/* $FreeBSD$	*/
/*	From: NetBSD: rtld_start.S,v 1.1 1996/12/16 20:38:09 cgd Exp 	*/

/*
 * Copyright 1996 Matt Thomas <matt@3am-software.com>
 * Copyright 2000 John D. Polstra
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/asm.h>
#include <machine/pal.h>

.extern	_GLOBAL_OFFSET_TABLE_

LEAF(_rtld_start, 0)		/* XXX */
	.set	noreorder
	br	pv, $33
$33:	LDGP(pv)

	/* save away the stack pointer */

	lda	s0, 0(sp)	/* get argc from stack */
	lda	sp, -16(sp)	/* space for arguments */

	/* save ps_strings pointer */
	mov	a3, s1

	/* Step 1 -- Figure out the displacement */

	br	t2, $34		/* get our PC */
$34:	ldiq	t3, $34		/* get where the linker thought we were */
	subq	t2, t3, t8	/* calculate the displacement */


	/* Step 2 -- Find bounds of global offset table */

	lda	t5, _GLOBAL_OFFSET_TABLE_
	addq	t8, t5, t9	/* add the displacement */
	lda	t4, _DYNAMIC
	addq	t8, t4, t10	/* add the displacement */

	/*
	 * Step 3 -- Every entry in the global offset table needs to
	 * modified for the displacement before any code will work.
	 */

$35:	ldq	t1, 0(t9)	/* load the value */
	addq	t8, t1, t1	/* add the displacement */
	stq	t1, 0(t9)	/* save the new value */
	lda	t9, 8(t9)	/* point to next entry */
	cmpult	t9, t10, t1	/* are we done? */
	bne	t1, $35		/* no, do more */

	/*
	 *  Ya!  Things are far enough so we can do some dynamic linking!
	 */

	lda	a0, 0(s0)	/* initial sp */
	lda	a1, -16(s0)	/* address for exit proc */
	lda	a2, -8(s0)	/* address for obj_main */
	CALL(_rtld)		/* v0 = _rtld(sp, &exit_proc, &obj_main); */

	ldq	a1, -16(s0)	/* our atexit function */
	ldq	a2, -8(s0)	/* obj_main entry */
	lda	sp, 16(sp)	/* readjust our stack */
	mov	s0, a0		/* stack pointer */
	mov	s1, a3		/* ps_strings pointer */
	mov	v0, t12
	jsr	ra, (v0), 0	/* (*_start)(sp, cleanup, obj); */
	ldgp	gp, 0(ra)

	CALL(exit)
	halt
END(_rtld_start)

	.set	noat
	.globl	_rtld_bind_start
	.ent	_rtld_bind_start
_rtld_bind_start:

	lda     sp, -168(sp)
	.frame  sp, 168, $26
	/* Preserve all registers that C normally doesn't.  */
	stq     $26, 0(sp)
	stq     $0, 8(sp)
	stq     $1, 16(sp)
	stq     $2, 24(sp)
	stq     $3, 32(sp)
	stq     $4, 40(sp)
	stq     $5, 48(sp)
	stq     $6, 56(sp)
	stq     $7, 64(sp)
	stq     $8, 72(sp)
	stq     $16, 80(sp)
	stq     $17, 88(sp)
	stq     $18, 96(sp)
	stq     $19, 104(sp)
	stq     $20, 112(sp)
	stq     $21, 120(sp)
	stq     $22, 128(sp)
	stq     $23, 136(sp)
	stq     $24, 144(sp)
	stq     $25, 152(sp)
	stq     $29, 160(sp)
	.mask   0x27ff01ff, -168
	/* Set up our $gp */
	br      gp, $100
$100:	ldgp    gp, 0(gp)
	.prologue 1
	/* Set up the arguments for _rtld_bind. */
	subq	at_reg, t12, a1		/* calculate reloc offset */
	ldq     a0, 8(t12)		/* object structure */
	subq	a1, 20, a1		/* = (at - t11 - 20) / 12 * 24 */
	addq	a1, a1, a1
	CALL(_rtld_bind)

	/* Move the destination address into position.  */
	mov     $0, $27
	/* Restore program registers.  */
	ldq     $26, 0(sp)
	ldq     $0, 8(sp)
	ldq     $1, 16(sp)
	ldq     $2, 24(sp)
	ldq     $3, 32(sp)
	ldq     $4, 40(sp)
	ldq     $5, 48(sp)
	ldq     $6, 56(sp)
	ldq     $7, 64(sp)
	ldq     $8, 72(sp)
	ldq     $16, 80(sp)
	ldq     $17, 88(sp)
	ldq     $18, 96(sp)
	ldq     $19, 104(sp)
	ldq     $20, 112(sp)
	ldq     $21, 120(sp)
	ldq     $22, 128(sp)
	ldq     $23, 136(sp)
	ldq     $24, 144(sp)
	ldq     $25, 152(sp)
	ldq     $29, 160(sp)
	/* Flush the Icache after having modified the .plt code.  */
	imb
	/* Clean up and turn control to the destination */
	lda     sp, 168(sp)
	jmp     $31, ($27)
	.end _rtld_bind_start

/*
 * int cmp0_and_store_int(volatile int *p, int newval);
 *
 * If an int holds 0, store newval into it; else do nothing.  Returns
 * the previous value.
 */
LEAF(cmp0_and_store_int, 2)
1:	mov	a1, t0
	ldl_l	v0, 0(a0)
	bne	v0, 3f
	stl_c	t0, 0(a0)
	beq	t0, 2f
	mb
	RET
2:	br	1b
3:	RET
END(cmp0_and_store_int)

LEAF(atomic_add_int, 2)
0:	ldl_l	t0, 0(a0)
	addq	t0, a1, t0
	stl_c	t0, 0(a0)
	beq	t0, 1f
	mb
	RET
1:	br	0b
END(atomic_add_int)
 
/* Atomically increment an int. */
LEAF(atomic_incr_int, 1)
0:	ldl_l	t0, 0(a0)
	addq	t0, 1, t0
	stl_c	t0, 0(a0)
	beq	t0, 1f
	mb
	RET
1:	br	0b
END(atomic_incr_int)

/* Atomically decrement an int. */
LEAF(atomic_decr_int, 1)
0:	ldl_l	t0, 0(a0)
	subq	t0, 1, t0
	stl_c	t0, 0(a0)
	beq	t0, 1f
	mb
	RET
1:	br	0b
END(atomic_decr_int)
