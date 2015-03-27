/*-
 * Copyright (c) 1985, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "lstd.h"

int
abs(int v)
{
	return v < 0 ? -v : v;
}

double
floor(double v)
{
	long long int a = (long long int)v;

	return ((double)a);
}

/*
 * Find the first occurrence in s1 of a character in s2 (excluding NUL).
 */
char *
strpbrk(const char *s1, const char *s2)
{
	const char *scanp;
	int c, sc;

	while ((c = *s1++) != 0) {
		for (scanp = s2; (sc = *scanp++) != '\0';)
			if (sc == c)
				return ((char *)(s1 - 1));
	}
	return (NULL);
}

double
ldexp (double x, int exp)
{
	if (exp >= 0)
		return x * ((long long)1 << exp);
	else
		return x / ((long long)1 << (-exp));
}


double
pow(double a, double b)
{
	printf("pow not implemented!\n");
	return 1.;
}

double
strtod(const char *string, char **endPtr)
{
	int sign = 0;
	int exp_sign = 0;
	int has_num = 0;
	int has_frac = 0;
	int has_exp = 0;
	unsigned long long num = 0;
	unsigned long long exp = 0;

	double frac = 0;
	double fm = 0.1;
	double exp_m = 1;
	double ret = 0;

	const char *ptr = string;

	while (isspace(*ptr)) ++ptr;

	if (*ptr == '-')
	{
		sign = 1;
		++ptr;
	} else if (*ptr == '+')
		++ptr;

	while (isdigit(*ptr))
	{
		num *= 10;
		num += *ptr - '0';
		++ptr;
		++has_num;
	}

	if (*ptr == '.')
	{
		++ptr;
		while (isdigit(*ptr))
		{
			frac += (double)(*ptr - '0') * fm;
			fm *= 0.1;
			++ptr;
			++has_frac;
		}
	}

	if (has_frac == 0 && has_num == 0)
	{
		if (endPtr)
			*endPtr = (char*)string;
		return 0.;
	}

	ret = (double)num;
	ret += frac;

	if (*ptr == 'e' || *ptr == 'E')
	{
		if (endPtr)
			*endPtr = (char*)ptr;
		++ptr;
		if (*ptr == '-')
		{
			exp_sign = 1;
			++ptr;
		} else if (*ptr == '+')
			++ptr;

		while (isdigit(*ptr))
		{
			exp *= 10;
			exp += *ptr - '0';
			++ptr;
			++has_exp;
		}
		if (has_exp == 0)
			return ret;
	}

	if (endPtr)
		*endPtr = (char*)ptr;

	if (has_exp)
	{
		while (exp--)
			exp_m *= 10;
		if (exp_sign)
			exp_m = 1./exp_m;

	}
	if (sign)
		ret = -ret;

	return ret * exp_m;
}

int
dtostr(double v, char *str)
{
	int	exp = 0;
	int	i;
	long long n;
	double	e = 1;
	char	*ptr;
	char	tmp[20];
	char	*buf = str;

	if (v == 0)
	{
		str[0] = '0';
		str[1] = 0;
		return 1;
	}

	if (v < 0)
	{
		*buf++ = '-';
		v = -v;
	}

	if (v <= e)
	{
		while (v < e)
		{
			--exp;
			e *= 0.1;
		}
	} else {
		while (v > e)
		{
			++exp;
			e *= 10;
		}
		--exp;
		e /= 10;
	}
	if (exp > 9 || exp < -9)
	{
		v /= e;
	} else {
		exp = 0;
	}

	n = (long long)v;
	v -= n;
	ptr = &tmp[19];
	*ptr = 0;

	do
	{
		i = n % 10;
		n /= 10;
		*(--ptr) = i + '0';
	} while (n > 0);

	while (*ptr != 0) *buf++ = *ptr++;

	if (v != 0)
	{
		ptr = buf;
		*buf++ = '.';

		for (i = 0; i < 17; ++i)
		{
			v *= 10;
			n = (long long)v;
			*buf++ = '0' + n;
			ptr = n > 0 ? buf : ptr;
			v -= n;
		}
		buf = ptr;
	}

	if (exp != 0)
	{
		*buf++ = 'e';
		if (exp < 0)
		{
			*buf++ = '-';
			exp = -exp;
		}
		ptr = &tmp[19];
		*ptr = 0;
		while (exp > 0)
		{
			i = exp % 10;
			exp /= 10;
			*(--ptr) = '0' + i;
		}
		while (*ptr != 0) *buf++ = *ptr++;
	}
	*buf = 0;
	return buf - str;
}

FILE *
fopen(const char *filename, const char *mode)
{
	struct stat	st;
	int		fd, r;
	FILE		*f;

	if (mode == NULL || mode[0] != 'r') return NULL;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
	{
		return NULL;
	}

	f = malloc(sizeof(FILE));
	if (f == NULL)
	{
		close(fd);
		return NULL;
	}

	r = fstat(fd, &st);
	if (r == 0)
	{
		f->fd = fd;
		f->offset = 0;
		f->size = st.st_size;
	} else {
		free(f);
		close(fd);
		f = NULL;
	}
	return f;
}


FILE *
freopen(const char *filename, const char *mode, FILE *stream)
{
	fclose(stream);
	return fopen(filename, mode);
}

size_t
fread(void *ptr, size_t size, size_t count, FILE *stream)
{
	size_t r;
	if (stream == NULL) return 0;
	r = (size_t)read(stream->fd, ptr, size * count);
	stream->offset += r;
	return r;
}

int
fclose(FILE *stream)
{
	if (stream == NULL) return EOF;
	close(stream->fd);
	free(stream);
	return 0;
}

int
ferror(FILE *stream)
{
	return (stream == NULL) || (stream->fd < 0);
}

int
feof(FILE *stream)
{
	if (stream == NULL) return 1;
	return stream->offset >= stream->size;
}

int
getc(FILE *stream)
{
	char	ch;
	size_t	r;

	if (stream == NULL) return EOF;
	r = read(stream->fd, &ch, 1);
	if (r == 1) return ch;
	return EOF;
}

/*
 * Find the first occurrence of find in s.
 */
char *
strstr(const char *s, const char *find)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != '\0') {
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == '\0')
					return (NULL);
			} while (sc != c);
		} while (strncmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}

void
luai_writestring(const char *s, int i)
{
	while (i-- > 0)
		putchar(*s++);
}

int
iscntrl(int c)
{
	return (c >= 0x00 && c <= 0x1F) || c == 0x7F;
}

int
isgraph(int c)
{
	return (c >= 0x21 && c <= 0x7E);
}

int
ispunct(int c)
{
	return (c >= 0x21 && c <= 0x2F) || (c >= 0x3A && c <= 0x40) ||
	    (c >= 0x5B && c <= 0x60) || (c >= 0x7B && c <= 0x7E);
}

void *
memchr(const void *s, int c, size_t n)
{
	if (n != 0) {
		const unsigned char *p = s;

		do {
			if (*p++ == (unsigned char)c)
				return ((void *)(p - 1));
		} while (--n != 0);
	}
	return (NULL);
}

void
abort(void)
{
	printf("abort called!\n");
	for (;;)
		;
}
