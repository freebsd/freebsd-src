/*-
 * Copyright (c) 2011 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by David Chisnall under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions * are met:
 * 1.  Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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
#ifndef _XLOCALE_H_
#error This header should only be included by <xlocale.h>, never directly.
#endif

#ifndef _XLOCALE_CTYPE_H_
__BEGIN_DECLS
unsigned long	___runetype_l(__ct_rune_t, locale_t) __pure;
__ct_rune_t	___tolower_l(__ct_rune_t, locale_t) __pure;
__ct_rune_t	___toupper_l(__ct_rune_t, locale_t) __pure;
_RuneLocale	*__runes_for_locale(locale_t, int*);
__END_DECLS
#endif

#ifndef _XLOCALE_INLINE
#if __GNUC__ && !__GNUC_STDC_INLINE__
#define _XLOCALE_INLINE extern inline
#else
#define _XLOCALE_INLINE inline
#endif
#endif

#ifdef XLOCALE_WCTYPES
static __inline int
__maskrune_l(__ct_rune_t _c, unsigned long _f, locale_t locale)
{
	int mb_sb_limit;
	_RuneLocale *runes = __runes_for_locale(locale, &mb_sb_limit);
	return (_c < 0 || _c >= _CACHED_RUNES) ? ___runetype_l(_c, locale) :
	       runes->__runetype[_c] & _f;
}

static __inline int
__istype_l(__ct_rune_t _c, unsigned long _f, locale_t locale)
{
	return (!!__maskrune_l(_c, _f, locale));
}

#define XLOCALE_ISCTYPE(fname, cat) \
		_XLOCALE_INLINE int isw##fname##_l(int c, locale_t l)\
		{ return __istype_l(c, cat, l); }
#else
static __inline int
__sbmaskrune_l(__ct_rune_t _c, unsigned long _f, locale_t locale)
{
	int mb_sb_limit;
	_RuneLocale *runes = __runes_for_locale(locale, &mb_sb_limit);
	return (_c < 0 || _c >= mb_sb_limit) ? 0 :
	       runes->__runetype[_c] & _f;
}

static __inline int
__sbistype_l(__ct_rune_t _c, unsigned long _f, locale_t locale)
{
	return (!!__sbmaskrune_l(_c, _f, locale));
}

#define XLOCALE_ISCTYPE(fname, cat) \
		_XLOCALE_INLINE int is##fname##_l(int c, locale_t l)\
		{ return __sbistype_l(c, cat, l); }
#endif

XLOCALE_ISCTYPE(alnum, _CTYPE_A|_CTYPE_D)
XLOCALE_ISCTYPE(alpha, _CTYPE_A)
XLOCALE_ISCTYPE(blank, _CTYPE_B)
XLOCALE_ISCTYPE(cntrl, _CTYPE_C)
XLOCALE_ISCTYPE(digit, _CTYPE_D)
XLOCALE_ISCTYPE(graph, _CTYPE_G)
XLOCALE_ISCTYPE(hexnumber, _CTYPE_X)
XLOCALE_ISCTYPE(ideogram, _CTYPE_I)
XLOCALE_ISCTYPE(lower, _CTYPE_L)
XLOCALE_ISCTYPE(number, _CTYPE_D)
XLOCALE_ISCTYPE(phonogram, _CTYPE_Q)
XLOCALE_ISCTYPE(print, _CTYPE_R)
XLOCALE_ISCTYPE(punct, _CTYPE_P)
XLOCALE_ISCTYPE(rune, 0xFFFFFF00L)
XLOCALE_ISCTYPE(space, _CTYPE_S)
XLOCALE_ISCTYPE(special, _CTYPE_T)
XLOCALE_ISCTYPE(upper, _CTYPE_U)
XLOCALE_ISCTYPE(xdigit, _CTYPE_X)
#undef XLOCALE_ISCTYPE

#ifdef XLOCALE_WCTYPES
_XLOCALE_INLINE int towlower_l(int c, locale_t locale)
{
	int mb_sb_limit;
	_RuneLocale *runes = __runes_for_locale(locale, &mb_sb_limit);
	return (c < 0 || c >= _CACHED_RUNES) ? ___tolower_l(c, locale) :
	       runes->__maplower[c];
}
_XLOCALE_INLINE int towupper_l(int c, locale_t locale)
{
	int mb_sb_limit;
	_RuneLocale *runes = __runes_for_locale(locale, &mb_sb_limit);
	return (c < 0 || c >= _CACHED_RUNES) ? ___toupper_l(c, locale) :
	       runes->__mapupper[c];
}
_XLOCALE_INLINE int
__wcwidth_l(__ct_rune_t _c, locale_t locale)
{
	unsigned int _x;

	if (_c == 0)
		return (0);
	_x = (unsigned int)__maskrune_l(_c, _CTYPE_SWM|_CTYPE_R, locale);
	if ((_x & _CTYPE_SWM) != 0)
		return ((_x & _CTYPE_SWM) >> _CTYPE_SWS);
	return ((_x & _CTYPE_R) != 0 ? 1 : -1);
}
int iswctype_l(wint_t wc, wctype_t charclass, locale_t locale);
wctype_t wctype_l(const char *property, locale_t locale);
wint_t towctrans_l(wint_t wc, wctrans_t desc, locale_t locale);
wint_t nextwctype_l(wint_t wc, wctype_t wct, locale_t locale);
wctrans_t wctrans_l(const char *charclass, locale_t locale);
#undef XLOCALE_WCTYPES
#else
_XLOCALE_INLINE int digittoint_l(int c, locale_t locale)
{ return __sbmaskrune_l((c), 0xFF, locale); }

_XLOCALE_INLINE int tolower_l(int c, locale_t locale)
{
	int mb_sb_limit;
	_RuneLocale *runes = __runes_for_locale(locale, &mb_sb_limit);
	return (c < 0 || c >= mb_sb_limit) ? c :
	       runes->__maplower[c];
}
_XLOCALE_INLINE int toupper_l(int c, locale_t locale)
{
	int mb_sb_limit;
	_RuneLocale *runes = __runes_for_locale(locale, &mb_sb_limit);
	return (c < 0 || c >= mb_sb_limit) ? c :
	       runes->__mapupper[c];
}
#endif
