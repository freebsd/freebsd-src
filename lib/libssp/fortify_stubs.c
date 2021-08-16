/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
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
__FBSDID("$FreeBSD$");

#include <sys/types.h>

#include <stdarg.h>
#include <stdlib.h>

/* Signatures grabbed from LSB Core Specification 4.1 */
void	*__memcpy_chk(void * __restrict dst, const void * __restrict src, size_t len,
    size_t dstlen);
void	*__memset_chk(void *dst, int c, size_t len, size_t dstlen);
int	__snprintf_chk(char * __restrict str, size_t maxlen, int flag, size_t strlen,
    const char * __restrict fmt, ...);
int	__sprintf_chk(char *str, int flag, size_t strlen, const char * __restrict fmt, ...);
char	*__stpcpy_chk(char * __restrict dst, const char * __restrict src, size_t dstlen);
char	*__strcat_chk(char * __restrict dst, const char * __restrict src, size_t dstlen);
char	*__strcpy_chk(char * __restrict dst, const char * __restrict src, size_t dstlen);
char	*__strncat_chk(char * __restrict dst, const char * __restrict src, size_t len, size_t dstlen);
char	*__strncpy_chk(char * __restrict dst, const char * __restrict src, size_t len, size_t dstlen);
int	__vsnprintf_chk(char * __restrict str, size_t size, int flags, size_t len,
    const char * __restrict format, va_list ap);
int	__vsprintf_chk(char * __restrict str, int flag, size_t slen, const char * __restrict format,
    va_list ap);

#define	ABORT()	abort2("_FORTIFY_SOURCE not supported", 0, NULL)

void *
__memcpy_chk(void * __restrict dst, const void * __restrict src, size_t len,
    size_t dstlen)
{

	ABORT();
}

void *
__memset_chk(void * __restrict dst, int c, size_t len, size_t dstlen)
{

	ABORT();
}

int
__snprintf_chk(char * __restrict str, size_t maxlen, int flag, size_t strlen,
    const char * __restrict fmt, ...)
{

	ABORT();
}

int
__sprintf_chk(char * __restrict str, int flag, size_t strlen, const char * __restrict fmt, ...)
{

	ABORT();
}

char *
__stpcpy_chk(char * __restrict dst, const char * __restrict src, size_t dstlen)
{

	ABORT();
}

char *
__strcat_chk(char * __restrict dst, const char * __restrict src, size_t dstlen)
{

	ABORT();
}

char *
__strcpy_chk(char * __restrict dst, const char * __restrict src, size_t dstlen)
{

	ABORT();
}

char *
__strncat_chk(char * __restrict dst, const char * __restrict src, size_t len, size_t dstlen)
{

	ABORT();
}

char *
__strncpy_chk(char * __restrict dst, const char * __restrict src, size_t len, size_t dstlen)
{

	ABORT();
}

int
__vsnprintf_chk(char * __restrict str, size_t size, int flags, size_t len,
    const char * __restrict format, va_list ap)
{

	ABORT();
}

int
__vsprintf_chk(char * __restrict str, int flag, size_t slen, const char * __restrict format,
    va_list ap)
{

	ABORT();
}
