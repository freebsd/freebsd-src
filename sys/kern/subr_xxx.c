/*
 * Copyright (c) 1982, 1986, 1991 Regents of the University of California.
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
 *	from: @(#)subr_xxx.c	7.10 (Berkeley) 4/20/91
 *	$Id: subr_xxx.c,v 1.3 1993/11/25 01:33:19 wollman Exp $
 */

/*
 * Miscellaneous trivial functions, including many
 * that are often inline-expanded or done in assembler.
 */
#include "param.h"
#include "systm.h"
#include "machine/cpu.h"

/*
 * Unsupported device function (e.g. writing to read-only device).
 */
int
enodev()
{

	return (ENODEV);
}

/*
 * Unconfigured device function; driver not configured.
 */
int
enxio()
{

	return (ENXIO);
}

/*
 * Unsupported ioctl function.
 */
int
enoioctl()
{

	return (ENOTTY);
}

/*
 * Unsupported system function.
 * This is used for an otherwise-reasonable operation
 * that is not supported by the current system binary.
 */
int
enosys()
{

	return (ENOSYS);
}

/*
 * Return error for operation not supported
 * on a specific object or file type.
 */
int
eopnotsupp()
{

	return (EOPNOTSUPP);
}

/*
 * Generic null operation, always returns success.
 */
int
nullop()
{

	return (0);
}

/*
 * Definitions of various trivial functions;
 * usually expanded inline rather than being defined here.
 */
#ifdef NEED_MINMAX
imin(a, b)
	int a, b;
{

	return (a < b ? a : b);
}

imax(a, b)
	int a, b;
{

	return (a > b ? a : b);
}

unsigned int
min(a, b)
	unsigned int a, b;
{

	return (a < b ? a : b);
}

unsigned int
max(a, b)
	unsigned int a, b;
{

	return (a > b ? a : b);
}

long
lmin(a, b)
	long a, b;
{

	return (a < b ? a : b);
}

long
lmax(a, b)
	long a, b;
{

	return (a > b ? a : b);
}

unsigned long
ulmin(a, b)
	unsigned long a, b;
{

	return (a < b ? a : b);
}

unsigned long
ulmax(a, b)
	unsigned long a, b;
{

	return (a > b ? a : b);
}
#endif /* NEED_MINMAX */

#ifdef NEED_FFS
ffs(mask)
	register long mask;
{
	register int bit;

	if (!mask)
		return(0);
	for (bit = 1;; ++bit) {
		if (mask&0x01)
			return(bit);
		mask >>= 1;
	}
}
#endif /* NEED_FFS */

#ifdef NEED_BCMP
bcmp(v1, v2, len)
	void *v1, *v2;
	register unsigned len;
{
	register u_char *s1 = v1, *s2 = v2;

	while (len--)
		if (*s1++ != *s2++)
			return (1);
	return (0);
}
#endif /* NEED_BCMP */

#ifdef NEED_STRLEN
size_t
strlen(s1)
	register const char *s1;
{
	register size_t len;

	for (len = 0; *s1++ != '\0'; len++)
		;
	return (len);
}
#endif /* NEED_STRLEN */
