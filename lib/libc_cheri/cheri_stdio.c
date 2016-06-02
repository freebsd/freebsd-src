/*-
 * Copyright (c) 2013 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#include <sys/types.h>

#include <cheri/cheri.h>
#include <cheri/cheric.h>
#include <cheri/cheri_system.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static FILE __sF[3];
FILE *__stdinp = &__sF[0];
FILE *__stdoutp = &__sF[1];
FILE *__stderrp = &__sF[2];

int
__swbuf(int c, FILE *stream)
{

	if (stream != stdout)
		return(EOF);

	return(cheri_system_putchar(c));
}

int
fputs(const char *str, FILE *stream)
{

	if (stream != stdout) {
		errno = ECAPMODE;
		return (EOF);
	}

	return (cheri_system_puts(cheri_ptr((void *)str, strlen(str) + 1)));
}

size_t
fwrite(const void * restrict ptr, size_t size,
    size_t nitems, FILE* restrict stream)
{
	size_t i;

	if (stream != stdout)
		return (0);

	for (i = 0; i < size * nitems; i++)
		(void)cheri_system_putchar(((const char *)ptr)[i]);

	return (nitems);
}

int
fputc(int c, FILE *stream)
{

	if (stream == stdout)
		return(cheri_system_putchar(c));
	else
		return (EOF);
}

int
fprintf(FILE * restrict stream, const char * restrict format, ...)
{
	int ret;
	va_list ap;

	va_start(ap, format);
	ret = vfprintf(stream, format, ap);
	va_end(ap);
	return (ret);
}

int
fprintf_l(FILE * restrict stream, locale_t loc __unused,
     const char * restrict format, ...)
{
	int ret;
	va_list ap;

	va_start(ap, format);
	ret = vfprintf(stream, format, ap);
	va_end(ap);
	return (ret);
}


int
vfprintf(FILE * restrict stream, const char * restrict format, va_list ap)
{

	if (stream != stdout)
		return (0); /* XXX correct? */

	return(vprintf(format, ap));
}

int
fclose(FILE *stream __unused)
{

	return (EOF);
}

FILE *
fopen(const char * restrict path __unused, const char * restrict mode __unused)
{

	return (NULL);
}

size_t
fread(void * restrict ptr __unused, size_t size __unused, size_t nmemb __unused,
    FILE * restrict stream __unused)
{

	return (0);
}

void
rewind(FILE *stream __unused)
{

}

int
fflush(FILE *stream)
{

	if (stream == stdout)
		return (0);	/* Always flushed immediately */
	return (EOF);
}

char *
fgets(char * restrict str __unused, int size __unused,
    FILE * restrict stream __unused)
{

	return (NULL);
}

#undef fileno
int
fileno(FILE *stream)
{

	return (-1);
}

#undef putc
int
putc(int c, FILE *stream)
{

	if (stream == stdout)
		return(cheri_system_putchar(c));
	else
		return (EOF);
}
