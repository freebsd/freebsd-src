/*-
 * Copyright (c) 2002, 2003 Tim J. Robbins. All rights reserved.
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Borman at Krystal Technologies.
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
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)euc.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>

#include <errno.h>
#include <runetype.h>
#include <stdlib.h>
#include <wchar.h>

extern size_t (*__mbrtowc)(wchar_t * __restrict, const char * __restrict,
    size_t, mbstate_t * __restrict);
extern size_t (*__wcrtomb)(char * __restrict, wchar_t, mbstate_t * __restrict);

int	_EUC_init(_RuneLocale *);
size_t	_EUC_mbrtowc(wchar_t * __restrict, const char * __restrict, size_t,
	    mbstate_t * __restrict);
size_t	_EUC_wcrtomb(char * __restrict, wchar_t, mbstate_t * __restrict);

typedef struct {
	int	count[4];
	wchar_t	bits[4];
	wchar_t	mask;
} _EucInfo;

int
_EUC_init(_RuneLocale *rl)
{
	_EucInfo *ei;
	int x, new__mb_cur_max;
	char *v, *e;

	if (rl->variable == NULL)
		return (EFTYPE);

	v = (char *)rl->variable;

	while (*v == ' ' || *v == '\t')
		++v;

	if ((ei = malloc(sizeof(_EucInfo))) == NULL)
		return (errno == 0 ? ENOMEM : errno);

	new__mb_cur_max = 0;
	for (x = 0; x < 4; ++x) {
		ei->count[x] = (int)strtol(v, &e, 0);
		if (v == e || !(v = e)) {
			free(ei);
			return (EFTYPE);
		}
		if (new__mb_cur_max < ei->count[x])
			new__mb_cur_max = ei->count[x];
		while (*v == ' ' || *v == '\t')
			++v;
		ei->bits[x] = (int)strtol(v, &e, 0);
		if (v == e || !(v = e)) {
			free(ei);
			return (EFTYPE);
		}
		while (*v == ' ' || *v == '\t')
			++v;
	}
	ei->mask = (int)strtol(v, &e, 0);
	if (v == e || !(v = e)) {
		free(ei);
		return (EFTYPE);
	}
	rl->variable = ei;
	rl->variable_len = sizeof(_EucInfo);
	_CurrentRuneLocale = rl;
	__mb_cur_max = new__mb_cur_max;
	__mbrtowc = _EUC_mbrtowc;
	__wcrtomb = _EUC_wcrtomb;
	return (0);
}

#define	CEI	((_EucInfo *)(_CurrentRuneLocale->variable))

#define	_SS2	0x008e
#define	_SS3	0x008f

#define	GR_BITS	0x80808080 /* XXX: to be fixed */

static __inline int
_euc_set(u_int c)
{
	c &= 0xff;
	return ((c & 0x80) ? c == _SS3 ? 3 : c == _SS2 ? 2 : 1 : 0);
}

size_t
_EUC_mbrtowc(wchar_t * __restrict pwc, const char * __restrict s, size_t n,
    mbstate_t * __restrict ps __unused)
{
	int len, remain, set;
	wchar_t wc;

	if (s == NULL)
		/* Reset to initial shift state (no-op) */
		return (0);
	if (n == 0 || (size_t)(len = CEI->count[set = _euc_set(*s)]) > n)
		/* Incomplete multibyte sequence */
		return ((size_t)-2);
	wc = 0;
	remain = len;
	switch (set) {
	case 3:
	case 2:
		--remain;
		++s;
		/* FALLTHROUGH */
	case 1:
	case 0:
		while (remain-- > 0)
			wc = (wc << 8) | (unsigned char)*s++;
		break;
	}
	wc = (wc & ~CEI->mask) | CEI->bits[set];
	if (pwc != NULL)
		*pwc = wc;
	return (wc == L'\0' ? 0 : len);
}

size_t
_EUC_wcrtomb(char * __restrict s, wchar_t wc,
    mbstate_t * __restrict ps __unused)
{
	wchar_t m, nm;
	int i, len;

	if (s == NULL)
		/* Reset to initial shift state (no-op) */
		return (1);

	m = wc & CEI->mask;
	nm = wc & ~m;

	if (m == CEI->bits[1]) {
CodeSet1:
		/* Codeset 1: The first byte must have 0x80 in it. */
		i = len = CEI->count[1];
		while (i-- > 0)
			*s++ = (nm >> (i << 3)) | 0x80;
	} else {
		if (m == CEI->bits[0])
			i = len = CEI->count[0];
		else if (m == CEI->bits[2]) {
			i = len = CEI->count[2];
			*s++ = _SS2;
			--i;
			/* SS2 designates G2 into GR */
			nm |= GR_BITS;
		} else if (m == CEI->bits[3]) {
			i = len = CEI->count[3];
			*s++ = _SS3;
			--i;
			/* SS3 designates G3 into GR */
			nm |= GR_BITS;
		} else
			goto CodeSet1;	/* Bletch */
		while (i-- > 0)
			*s++ = (nm >> (i << 3)) & 0xff;
	}
	return (len);
}
