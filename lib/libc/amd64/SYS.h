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
 *	from: @(#)SYS.h	5.5 (Berkeley) 5/7/91
 *
 *	$FreeBSD$
 */

#include <sys/syscall.h>
#include "DEFS.h"

#ifdef PIC
#define PIC_PROLOGUE    \
        pushl   %ebx;   \
        call    1f;     \
1:                      \
        popl    %ebx;   \
        addl    $_GLOBAL_OFFSET_TABLE_+[.-1b], %ebx
#define PIC_EPILOGUE    \
        popl    %ebx
#define PIC_PLT(x)      x@PLT
#define PIC_GOT(x)      x@GOT(%ebx)
#define PIC_GOTOFF(x)   x@GOTOFF(%ebx)
#else
#define PIC_PROLOGUE
#define PIC_EPILOGUE
#define PIC_PLT(x)      x
#define PIC_GOT(x)      x
#define PIC_GOTOFF(x)   x
#endif

#define	SYSCALL(x)	2: PIC_PROLOGUE; jmp PIC_PLT(HIDENAME(cerror)); ENTRY(x); lea __CONCAT(SYS_,x),%eax; KERNCALL; jb 2b
#define	RSYSCALL(x)	SYSCALL(x); ret

#define	PSEUDO(x,y)	ENTRY(x); lea __CONCAT(SYS_,y), %eax; KERNCALL; ret
#define	CALL(x,y)	call CNAME(y); addl $4*x,%esp
/* gas messes up offset -- although we don't currently need it, do for BCS */
#define	LCALL(x,y)	.byte 0x9a ; .long y; .word x

/*
 * Design note:
 *
 * The macros PSYSCALL() and PRSYSCALL() are intended for use where a
 * syscall needs to be renamed in the threaded library. When building
 * a normal library, they default to the traditional SYSCALL() and
 * RSYSCALL(). This avoids the need to #ifdef _THREAD_SAFE everywhere
 * that the renamed function needs to be called.
 */
#ifdef _THREAD_SAFE	/* in case */
/*
 * For the thread_safe versions, we prepend _thread_sys_ to the function
 * name so that the 'C' wrapper can go around the real name.
 */
#define	PSYSCALL(x)	2: PIC_PROLOGUE; jmp PIC_PLT(HIDENAME(cerror)); ENTRY(__CONCAT(_thread_sys_,x)); lea __CONCAT(SYS_,x),%eax; KERNCALL; jb 2b
#define	PRSYSCALL(x)	PSYSCALL(x); ret
#define	PPSEUDO(x,y)	ENTRY(__CONCAT(_thread_sys_,x)); lea __CONCAT(SYS_,y), %eax; KERNCALL; ret
#else
/*
 * The non-threaded library defaults to traditional syscalls where
 * the function name matches the syscall name.
 */
#define	PSYSCALL(x)	SYSCALL(x)
#define	PRSYSCALL(x)	RSYSCALL(x)
#define	PPSEUDO(x,y)	PSEUDO(x,y)
#endif

#ifdef __ELF__
#define KERNCALL	int $0x80	/* Faster */
#else
#define KERNCALL	LCALL(7,0)	/* The old way */
#endif

#define	ASMSTR		.asciz
