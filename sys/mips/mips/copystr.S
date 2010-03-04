/*      $NetBSD: copy.S,v 1.5 2007/10/17 19:55:37 garbled Exp $ */

/*-
 * Copyright (c) 1992, 1993
 *      The Regents of the University of California.  All rights reserved.
 * 
 * This code is derived from software contributed to Berkeley by
 * Digital Equipment Corporation and Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (C) 1989 Digital Equipment Corporation.
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies.
 * Digital Equipment Corporation makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * 
 * from: Header: /sprite/src/kernel/mach/ds3100.md/RCS/loMem.s,
 *      v 1.1 89/07/11 17:55:04 nelson Exp  SPRITE (DECWRL)
 * from: Header: /sprite/src/kernel/mach/ds3100.md/RCS/machAsm.s,
 *      v 9.2 90/01/29 18:00:39 shirriff Exp  SPRITE (DECWRL)
 * from: Header: /sprite/src/kernel/vm/ds3100.md/vmPmaxAsm.s,
 *      v 1.1 89/07/10 14:27:41 nelson Exp  SPRITE (DECWRL)
 * 
 *      @(#)locore.s    8.5 (Berkeley) 1/4/94
 */
	
#include "assym.s"
#include <machine/asm.h>
#include <machine/asmacros.h>
__FBSDID("$FreeBSD$");

#include <sys/errno.h>

/*
 * copystr(9)
 * <v0>int copystr(<a0>const void *src, <a1>void *dst, <a2>size_t len,
 *                 <a3>size_t *done)
 */
ENTRY(copystr)
	.set noreorder
	.set noat
	move	v0, zero
	beqz	a2, 2f
	move	t1, zero
1:	subu	a2, 1			/*XXX mips64 unsafe -- long */
	lbu	t0, 0(a0)
	PTR_ADDU a0, 1
	sb	t0, 0(a1)
	PTR_ADDU a1, 1
	beqz	t0, 3f /* NULL  - end of string*/
	addu	t1, 1			/*XXX mips64 unsafe -- long */
	bnez	a2, 1b
	nop
2:	/* ENAMETOOLONG */
	li	v0, ENAMETOOLONG
3:	/* done != NULL -> how many bytes were copied */
	beqz	a3, 4f
	nop
	sw	t1, 0(a3)		/*XXX mips64 unsafe -- long */
4:	jr	ra
	nop
	.set reorder
	.set at
END(copystr)

/*
 * int copyinstr(void *uaddr, void *kaddr, size_t maxlen, size_t *lencopied)
 * Copy a NIL-terminated string, at most maxlen characters long, from the
 * user's address space.  Return the number of characters copied (including
 * the NIL) in *lencopied.  If the string is too long, return ENAMETOOLONG;
 * else return 0 or EFAULT.
 */
LEAF(copyinstr)
	.set noreorder
	.set noat
	lw	t2, pcpup
	lw	v1, PC_CURPCB(t2)
	PTR_LA	v0, _C_LABEL(copystrerr)
	blt	a0, zero, _C_LABEL(copystrerr)
	sw	v0, PCB_ONFAULT(v1)
	move	t0, a2
	beq	a2, zero, 4f
1:
	lbu	v0, 0(a0)
	subu	a2, a2, 1		/*xxx mips64 unsafe -- long */
	beq	v0, zero, 2f
	sb	v0, 0(a1)
	PTR_ADDU a0, a0, 1
	bne	a2, zero, 1b
	PTR_ADDU a1, a1, 1
4:
	li	v0, ENAMETOOLONG
2:
	beq	a3, zero, 3f
	subu	a2, t0, a2		/*xxx mips64 unsafe -- long */
	sw	a2, 0(a3)		/*xxx mips64 unsafe -- long */
3:
	j	ra				# v0 is 0 or ENAMETOOLONG
	sw	zero, PCB_ONFAULT(v1)
	.set reorder
	.set at
END(copyinstr)

/*
 * int copyoutstr(void *uaddr, void *kaddr, size_t maxlen, size_t *lencopied);
 * Copy a NIL-terminated string, at most maxlen characters long, into the
 * user's address space.  Return the number of characters copied (including
 * the NIL) in *lencopied.  If the string is too long, return ENAMETOOLONG;
 * else return 0 or EFAULT.
 */
LEAF(copyoutstr)
	.set noreorder
	.set noat
	lw	t2, pcpup
	lw	v1, PC_CURPCB(t2)
	PTR_LA	v0, _C_LABEL(copystrerr)
	blt	a1, zero, _C_LABEL(copystrerr)
	sw	v0, PCB_ONFAULT(v1)
	move	t0, a2
	beq	a2, zero, 4f
1:
	lbu	v0, 0(a0)
	subu	a2, a2, 1		/*xxx mips64 unsafe -- long */
	beq	v0, zero, 2f
	sb	v0, 0(a1)
	PTR_ADDU a0, a0, 1
	bne	a2, zero, 1b
	PTR_ADDU a1, a1, 1
4:
	li	v0, ENAMETOOLONG
2:
	beq	a3, zero, 3f
	subu	a2, t0, a2		/*xxx mips64 unsafe -- long */
	sw	a2, 0(a3)		/*xxx mips64 unsafe -- long */
3:
	j	ra				# v0 is 0 or ENAMETOOLONG
	sw	zero, PCB_ONFAULT(v1)
	.set reorder
	.set at
END(copyoutstr)

LEAF(copystrerr)
	sw	zero, PCB_ONFAULT(v1)
	j	ra
	li	v0, EFAULT			# return EFAULT
END(copystrerr)
