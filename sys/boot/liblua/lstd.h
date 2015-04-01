/*-
 * Copyright (c) 2014 Pedro Souza <pedrosouza@freebsd.org>
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
 *
 * $FreeBSD$
 */

#ifndef LSTD_H
#define LSTD_H

#include <stand.h>
#include <sys/types.h>
#include <sys/stdint.h>
#include <limits.h>
#include <string.h>
#include <machine/stdarg.h>


typedef struct FILE
{
	int fd;
	size_t offset;
	size_t size;
} FILE;

FILE * fopen(const char *filename, const char *mode);

FILE * freopen( const char *filename, const char *mode, FILE *stream);

size_t fread(void *ptr, size_t size, size_t count, FILE *stream);

int fclose(FILE *stream);

int ferror(FILE *stream);

int feof(FILE *stream);

int getc(FILE * stream);

#ifndef EOF
#define EOF (-1)
#endif

#define stdin ((FILE*)NULL)

#ifndef BUFSIZ
#define BUFSIZ 512
#endif

#define getlocaledecpoint() ('.')

#define strcoll strcmp

int abs(int v);

double floor(double v);

char * strpbrk (const char *str1, const char *str2);

double ldexp (double x, int exp);

double pow(double a, double b);

double strtod(const char *string, char **endPtr);

int dtostr(double v, char *str);

char * strstr(const char *str1, const char *str2);

int iscntrl(int c);

int isgraph(int c);

int ispunct(int c);

void * memchr(const void *ptr, int value, size_t num);

void abort(void) __dead2;

static inline char
_l_getlocaledecpoint(void)
{
	return ('.');
}

#ifndef l_getlocaledecpoint
#define l_getlocaledecpoint _l_getlocaledecpoint
#endif

#ifndef lua_writestringerror
#define lua_writestringerror(s,p) \
	(printf((s), (p)))
#endif

void luai_writestring(const char *, int);

#ifndef lua_writestring
#define lua_writestring(s,l) luai_writestring(s,l)
#endif

#define stdout 1
#define fflush	/* */
#define fgets(b,l,s) fgetstr((b), (l), 0)

static inline double
frexp(double value, int *exp)
{
	return 0; /* XXX */
}

static inline double
fmod(double x, double y)
{
	return 0; /* XXX */
}

#endif /* LSTD_H */
