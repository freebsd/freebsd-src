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
.extern	_GOT_END_

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
	lda	t4, _GOT_END_
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
	mov	v0, pv
	jsr	ra, (v0), 0	/* (*_start)(sp, cleanup, obj); */
	ldgp	gp, 0(ra)

	CALL(exit)
	halt
END(_rtld_start)

#define	RTLD_BIND_START_PROLOGUE					\
	/* at_reg already used by PLT code. */				\
	.set	noat						;	\
									\
	/*								\
	 * Allocate stack frame and preserve all registers that the	\
	 * caller would have normally saved themselves.			\
	 */								\
	lda	sp, -168(sp)					;	\
	stq	ra, 0(sp)					;	\
	stq	v0, 8(sp)					;	\
	stq	t0, 16(sp)					;	\
	stq	t1, 24(sp)					;	\
	stq	t2, 32(sp)					;	\
	stq	t3, 40(sp)					;	\
	stq	t4, 48(sp)					;	\
	stq	t5, 56(sp)					;	\
	stq	t6, 64(sp)					;	\
	stq	t7, 72(sp)					;	\
	stq	a0, 80(sp)					;	\
	stq	a1, 88(sp)					;	\
	stq	a2, 96(sp)					;	\
	stq	a3, 104(sp)					;	\
	stq	a4, 112(sp)					;	\
	stq	a5, 120(sp)					;	\
	stq	t8, 128(sp)					;	\
	stq	t9, 136(sp)					;	\
	stq	t10, 144(sp)					;	\
	stq	t11, 152(sp)					;	\
	stq	gp, 160(sp)					;	\
									\
	/*								\
	 * Load our global pointer.  Note, can't use pv, since it is	\
	 * already used by the PLT code.				\
	 */								\
	br	t0, 1f						;	\
1:	LDGP(t0)

#define	RTLD_BIND_START_EPILOGUE					\
	/* Move the destination address into position. */		\
	mov	v0, pv						;	\
									\
	/* Restore program registers. */				\
	ldq	ra, 0(sp)					;	\
	ldq	v0, 8(sp)					;	\
	ldq	t0, 16(sp)					;	\
	ldq	t1, 24(sp)					;	\
	ldq	t2, 32(sp)					;	\
	ldq	t3, 40(sp)					;	\
	ldq	t4, 48(sp)					;	\
	ldq	t5, 56(sp)					;	\
	ldq	t6, 64(sp)					;	\
	ldq	t7, 72(sp)					;	\
	ldq	a0, 80(sp)					;	\
	ldq	a1, 88(sp)					;	\
	ldq	a2, 96(sp)					;	\
	ldq	a3, 104(sp)					;	\
	ldq	a4, 112(sp)					;	\
	ldq	a5, 120(sp)					;	\
	ldq	t8, 128(sp)					;	\
	ldq	t9, 136(sp)					;	\
	ldq	t10, 144(sp)					;	\
	ldq	t11, 152(sp)					;	\
	ldq	gp, 160(sp)					;	\
	/* XXX LDGP? */							\
									\
	/*								\
	 * We've patched the PLT; sync the I-stream.			\
	 */								\
	imb							;	\
									\
	/* Pop the stack frame and turn control to the destination. */	\
	lda     sp, 168(sp)					;	\
	jmp	zero, (pv)


/*
 * Lazy binding entry point, called via PLT.
 */
NESTED_NOPROFILE(_rtld_bind_start, 0, 168, ra, 0, 0)

	RTLD_BIND_START_PROLOGUE

	/* Set up the arguments for _rtld_bind. */
	subq	at_reg, pv, a1		/* calculate reloc offset */
	ldq     a0, 8(pv)		/* object structure */
	subq	a1, 20, a1		/* = (at - t11 - 20) / 12 * 24 */
	addq	a1, a1, a1

	CALL(_rtld_bind)

	RTLD_BIND_START_EPILOGUE

END(_rtld_bind_start)

/*
 * Lazy binding entry point, called via PLT.  This version is for the
 * old PLT entry format.
 */
NESTED_NOPROFILE(_rtld_bind_start_old, 0, 168, ra, 0, 0)

	RTLD_BIND_START_PROLOGUE

	/* Set up the arguments for _rtld_bind. */
	ldq     a0, 8(pv)		/* object structure */
	mov	at_reg, a1		/* offset of reloc entry */

	CALL(_rtld_bind)

	RTLD_BIND_START_EPILOGUE

END(_rtld_bind_start_old)

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
