/*
 * Copyright (c) 1998 Mark Newton
 * Copyright (c) 1994 Christos Zoulas
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
 *    derived from this software without specific prior written permission
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
 * 
 * $FreeBSD$
 */

#ifndef	_SVR4_EXEC_H_
#define	_SVR4_EXEC_H_

#ifdef SVR4_COMPAT_SOLARIS2
# define SVR4_AUX_ARGSIZ (sizeof(AuxInfo) * 12 / sizeof(char *))
#else
# define SVR4_AUX_ARGSIZ (sizeof(AuxInfo) * 8 / sizeof(char *))
#endif

#if 0
/* Don't think we need all this NetBSD stuff */
/*
 * The following is horrible; there must be a better way. I need to
 * play with brk(2) a bit more.
 */
#ifdef i386
/*
 * I cannot load the interpreter after the data segment because brk(2)
 * breaks. I have to load it somewhere before. Programs start at
 * 0x08000000 so I load the interpreter far before.
 */
#define SVR4_INTERP_ADDR	0x01000000
#endif

#ifdef sparc
/*
 * Here programs load at 0x00010000, so I load the interpreter far after
 * the end of the data segment.
 */
#define SVR4_INTERP_ADDR	0x10000000
#endif

#ifndef SVR4_INTERP_ADDR
# define SVR4_INTERP_ADDR	0
#endif
#endif

/*void svr4_setregs __P((struct proc *, struct exec_package *, u_long));*/

#endif /* !_SVR4_EXEC_H_ */
