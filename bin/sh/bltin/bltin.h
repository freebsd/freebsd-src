/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
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
 *	@(#)bltin.h	8.2 (Berkeley) 5/4/95
 * $FreeBSD$
 */

/*
 * This file is included by programs which are optionally built into the
 * shell.  If SHELL is defined, we try to map the standard UNIX library
 * routines to ash routines using defines.
 */

#include "../shell.h"
#include "../mystring.h"
#ifdef SHELL
#include "../output.h"
#undef stdout
#define stdout out1
#undef stderr
#define stderr out2
#define printf out1fmt
#undef putc
#define putc(c, file)	outc(c, file)
#undef putchar
#define putchar(c)	out1c(c)
#define fprintf outfmt
#define fputs outstr
#define fflush flushout
#define INITARGS(argv)
#define warnx1(a, b, c) {				\
	char buf[64];					\
	(void)snprintf(buf, sizeof(buf), a);		\
	error("%s", buf);				\
}
#define warnx2(a, b, c) {				\
	char buf[64];					\
	(void)snprintf(buf, sizeof(buf), a, b);		\
	error("%s", buf);				\
}
#define warnx3(a, b, c) {				\
	char buf[64];					\
	(void)snprintf(buf, sizeof(buf), a, b, c);	\
	error("%s", buf);				\
}

#else
#undef NULL
#include <stdio.h>
#undef main
#define INITARGS(argv)	if ((commandname = argv[0]) == NULL) {fputs("Argc is zero\n", stderr); exit(2);} else
#endif

pointer stalloc(int);
void error(const char *, ...) __printf0like(1, 2);


extern char *commandname;
