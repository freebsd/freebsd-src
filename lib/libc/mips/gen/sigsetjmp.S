/*-
 * Copyright (c) 1991, 1993, 1995,
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Havard Eidnes.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/syscall.h>
#include <machine/regnum.h>
#include <machine/asm.h>
#include <machine/setjmp.h>

#if defined(LIBC_SCCS) 
	.text
	.asciz "$OpenBSD$"
#endif /* LIBC_SCCS */

/*
 * C library -- sigsetjmp, siglongjmp
 *
 *	siglongjmp(a,v)
 * will generate a "return(v)" from
 * the last call to
 *	sigsetjmp(a, savemask)
 * by restoring registers from the stack,
 * and dependent on savemask restores the
 * signal mask.
 */

LEAF(sigsetjmp)
	.set	reorder
	sw	a1, (_JBLEN*4)(a0)		# save "savemask"
	bne	a1, 0x0, 1f			# do saving of signal mask?
	la	t9, _setjmp
	jr t9

1:	la	t9, setjmp
	jr t9
END(sigsetjmp)

LEAF(siglongjmp)
	.set	reorder
	lw	t0, (_JBLEN * 4)(a0)		# get "savemask"
	bne	t0, 0x0, 1f			# restore signal mask?
	la	t9, _longjmp
	jr	t9
1:	la	t9, longjmp
	jr	t9
END(siglongjmp)
