/*-
 * Copyright (c) 2003 Tim J. Robbins.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/stdio/unlocked.c,v 1.1 2003/01/10 04:35:08 tjr Exp $");

#include <stdio.h>

#undef getchar_unlocked
int
getchar_unlocked(void)
{

	return (__sgetc(stdin));
}

#undef getc_unlocked
int
getc_unlocked(FILE *fp)
{

	return (__sgetc(fp));
}

#undef putchar_unlocked
int
putchar_unlocked(int ch)
{

	return (__sputc(ch, stdout));
}

#undef putc_unlocked
int
putc_unlocked(int ch, FILE *fp)
{

	return (__sputc(ch, fp));
}

#undef feof_unlocked
int
feof_unlocked(FILE *fp)
{

	return (__sfeof(fp));
}

#undef ferror_unlocked
int
ferror_unlocked(FILE *fp)
{

	return (__sferror(fp));
}

#undef clearerr_unlocked
void
clearerr_unlocked(FILE *fp)
{

	__sclearerr(fp);
}

#undef fileno_unlocked
int
fileno_unlocked(FILE *fp)
{

	return (__sfileno(fp));
}
