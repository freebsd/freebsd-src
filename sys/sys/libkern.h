/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)libkern.h	8.1 (Berkeley) 6/10/93
 * $FreeBSD$
 */

#ifndef _SYS_LIBKERN_H_
#define	_SYS_LIBKERN_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#ifdef _KERNEL
#include <sys/systm.h>
#endif

/* BCD conversions. */
extern u_char const	bcd2bin_data[];
extern u_char const	bin2bcd_data[];
extern char const	hex2ascii_data[];

#define	bcd2bin(bcd)	(bcd2bin_data[bcd])
#define	bin2bcd(bin)	(bin2bcd_data[bin])
#define	hex2ascii(hex)	(hex2ascii_data[hex])

static __inline int imax(int a, int b) { return (a > b ? a : b); }
static __inline int imin(int a, int b) { return (a < b ? a : b); }
static __inline long lmax(long a, long b) { return (a > b ? a : b); }
static __inline long lmin(long a, long b) { return (a < b ? a : b); }
static __inline u_int max(u_int a, u_int b) { return (a > b ? a : b); }
static __inline u_int min(u_int a, u_int b) { return (a < b ? a : b); }
static __inline quad_t qmax(quad_t a, quad_t b) { return (a > b ? a : b); }
static __inline quad_t qmin(quad_t a, quad_t b) { return (a < b ? a : b); }
static __inline u_long ulmax(u_long a, u_long b) { return (a > b ? a : b); }
static __inline u_long ulmin(u_long a, u_long b) { return (a < b ? a : b); }

/* Prototypes for non-quad routines. */
u_int32_t arc4random __P((void));
int	 bcmp __P((const void *, const void *, size_t));
#ifndef HAVE_INLINE_FFS
int	 ffs __P((int));
#endif
#ifndef	HAVE_INLINE_FLS
int	 fls __P((int));
#endif
int	 locc __P((int, char *, u_int));
void	 qsort __P((void *base, size_t nmemb, size_t size,
		    int (*compar)(const void *, const void *)));
u_long	 random __P((void));
char	*index __P((const char *, int));
char	*rindex __P((const char *, int));
int	 scanc __P((u_int, const u_char *, const u_char *, int));
int	 skpc __P((int, int, char *));
void	 srandom __P((u_long));
char	*strcat __P((char *, const char *));
int	 strcmp __P((const char *, const char *));
char	*strcpy __P((char *, const char *));
size_t	 strlen __P((const char *));
int	 strncmp __P((const char *, const char *, size_t));
char	*strncpy __P((char *, const char *, size_t));


static __inline int
memcmp(const void *b1, const void *b2, size_t len)
{
	return (bcmp(b1, b2, len));
}

static __inline void *
memset(void *b, int c, size_t len)
{
	char *bb;

	if (c == 0)
		bzero(b, len);
	else
		for (bb = (char *)b; len--; )
			*bb++ = c;
	return (b);
}

#endif /* !_SYS_LIBKERN_H_ */
