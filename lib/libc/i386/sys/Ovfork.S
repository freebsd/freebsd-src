/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *
 *	$Id: Ovfork.S,v 1.2 1995/01/23 01:29:37 davidg Exp $
 */

#if defined(SYSLIBC_RCS) && !defined(lint)
	.text
	.asciz "$Id: Ovfork.S,v 1.2 1995/01/23 01:29:37 davidg Exp $"
#endif /* SYSLIBC_RCS and not lint */

#include "DEFS.h"
#include "SYS.h"

/*
 * pid = vfork();
 *
 * %edx == 0 in parent process, %edx == 1 in child process.
 * %eax == pid of child in parent, %eax == pid of parent in child.
 *
 */

ENTRY(vfork)
	popl	%ecx		/* my rta into ecx */
	lea	SYS_vfork,%eax
	KERNCALL
	jb	verror
vforkok:
	cmpl	$0,%edx		/* child process? */
	jne	child		/* yes */
	jmp 	parent 
#ifdef	_THREAD_SAFE
	/*
	 * Threaded version using __error().
	 */
	.globl	CNAME(__error)
	.type	CNAME(__error),@function
verror:
	pushl	%eax
#ifdef PIC
	call	PIC_PLT(CNAME(__error))
#else
	call	CNAME(__error)
#endif
	popl	%ecx
	movl	%ecx,(%eax)
	movl	$-1,%eax
	movl	$-1,%edx
#else	/* !_THREAD_SAFE */
	/*
	 * Non-threaded version using global errno.
	 */
	.globl	CNAME(errno)
verror:
#ifdef PIC
	PIC_PROLOGUE
	movl	PIC_GOT(CNAME(errno)), %edx
	movl	%eax,(%edx)
	PIC_EPILOGUE
#else
	movl	%eax,CNAME(errno)
#endif
	movl	$-1,%eax
#endif	/* !_THREAD_SAFE */
	jmp	%ecx
child:
	movl	$0,%eax
parent:
	jmp	%ecx
