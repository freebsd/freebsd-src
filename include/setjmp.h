/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 *	@(#)setjmp.h	5.5 (Berkeley) 6/8/91
 */

#ifndef _SETJMP_H_
#define _SETJMP_H_

#if defined(hp300) || defined(__hp300__)
#define _JBLEN	17
#endif

#if defined(i386) || defined(__i386__)
#define _JBLEN	10
#endif

#if defined(tahoe) || defined(__tahoe__)
#define _JBLEN	10
#endif

#if defined(vax) || defined(__vax__)
#define _JBLEN	10
#endif

typedef int jmp_buf[_JBLEN];

#ifndef _ANSI_SOURCE
typedef int sigjmp_buf[_JBLEN + 1];
#endif /* not ANSI */

#include <sys/cdefs.h>

__BEGIN_DECLS
int	setjmp __P((jmp_buf));
void	longjmp __P((jmp_buf, int));

#ifndef _ANSI_SOURCE
int	sigsetjmp __P((sigjmp_buf, int));
void	siglongjmp __P((sigjmp_buf, int));
#endif /* not ANSI */

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
int	_setjmp __P((jmp_buf));
void	_longjmp __P((jmp_buf, int));
void	longjmperror __P((void));
#endif /* neither ANSI nor POSIX */
__END_DECLS

#endif /* !_SETJMP_H_ */
