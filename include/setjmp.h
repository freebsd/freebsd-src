/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)setjmp.h	8.2 (Berkeley) 1/21/94
 */

#ifndef _SETJMP_H_
#define _SETJMP_H_

/* The size of the jmp_buf is machine dependent: */
#include <machine/setjmp.h>

/*
 * jmp_buf and sigjmp_buf are encapsulated in different structs to force
 * compile-time diagnostics for mismatches.  The structs are the same
 * internally to avoid some run-time errors for mismatches.
 */
#ifndef _ANSI_SOURCE
typedef struct { long _sjb[_JBLEN + 1]; } sigjmp_buf[1];
#endif /* not ANSI */

typedef struct { long _jb[_JBLEN + 1]; } jmp_buf[1];

#include <sys/cdefs.h>

__BEGIN_DECLS
int	setjmp __P((jmp_buf));
void	longjmp __P((jmp_buf, int)) __dead2;

#ifndef _ANSI_SOURCE
int	sigsetjmp __P((sigjmp_buf, int));
void	siglongjmp __P((sigjmp_buf, int)) __dead2;
#endif /* not ANSI */

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
int	_setjmp __P((jmp_buf));
void	_longjmp __P((jmp_buf, int)) __dead2;
void	longjmperror __P((void));
#endif /* neither ANSI nor POSIX */
__END_DECLS

#endif /* !_SETJMP_H_ */
