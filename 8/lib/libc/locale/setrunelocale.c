/*-
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <runetype.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wchar.h>
#include "ldpart.h"
#include "mblocal.h"
#include "setlocale.h"

extern int __mb_sb_limit;

extern _RuneLocale	*_Read_RuneMagi(FILE *);

static int		__setrunelocale(const char *);

static int
__setrunelocale(const char *encoding)
{
	FILE *fp;
	char name[PATH_MAX];
	_RuneLocale *rl;
	int saverr, ret;
	size_t (*old__mbrtowc)(wchar_t * __restrict,
	    const char * __restrict, size_t, mbstate_t * __restrict);
	size_t (*old__wcrtomb)(char * __restrict, wchar_t,
	    mbstate_t * __restrict);
	int (*old__mbsinit)(const mbstate_t *);
	size_t (*old__mbsnrtowcs)(wchar_t * __restrict,
	    const char ** __restrict, size_t, size_t, mbstate_t * __restrict);
	size_t (*old__wcsnrtombs)(char * __restrict,
	    const wchar_t ** __restrict, size_t, size_t,
	    mbstate_t * __restrict);
	static char ctype_encoding[ENCODING_LEN + 1];
	static _RuneLocale *CachedRuneLocale;
	static int Cached__mb_cur_max;
	static int Cached__mb_sb_limit;
	static size_t (*Cached__mbrtowc)(wchar_t * __restrict,
	    const char * __restrict, size_t, mbstate_t * __restrict);
	static size_t (*Cached__wcrtomb)(char * __restrict, wchar_t,
	    mbstate_t * __restrict);
	static int (*Cached__mbsinit)(const mbstate_t *);
	static size_t (*Cached__mbsnrtowcs)(wchar_t * __restrict,
	    const char ** __restrict, size_t, size_t, mbstate_t * __restrict);
	static size_t (*Cached__wcsnrtombs)(char * __restrict,
	    const wchar_t ** __restrict, size_t, size_t,
	    mbstate_t * __restrict);

	/*
	 * The "C" and "POSIX" locale are always here.
	 */
	if (strcmp(encoding, "C") == 0 || strcmp(encoding, "POSIX") == 0) {
		(void) _none_init(&_DefaultRuneLocale);
		return (0);
	}

	/*
	 * If the locale name is the same as our cache, use the cache.
	 */
	if (CachedRuneLocale != NULL &&
	    strcmp(encoding, ctype_encoding) == 0) {
		_CurrentRuneLocale = CachedRuneLocale;
		__mb_cur_max = Cached__mb_cur_max;
		__mb_sb_limit = Cached__mb_sb_limit;
		__mbrtowc = Cached__mbrtowc;
		__mbsinit = Cached__mbsinit;
		__mbsnrtowcs = Cached__mbsnrtowcs;
		__wcrtomb = Cached__wcrtomb;
		__wcsnrtombs = Cached__wcsnrtombs;
		return (0);
	}

	/*
	 * Slurp the locale file into the cache.
	 */

	/* Range checking not needed, encoding length already checked before */
	(void) strcpy(name, _PathLocale);
	(void) strcat(name, "/");
	(void) strcat(name, encoding);
	(void) strcat(name, "/LC_CTYPE");

	if ((fp = fopen(name, "r")) == NULL)
		return (errno == 0 ? ENOENT : errno);

	if ((rl = _Read_RuneMagi(fp)) == NULL) {
		saverr = (errno == 0 ? EFTYPE : errno);
		(void)fclose(fp);
		return (saverr);
	}
	(void)fclose(fp);

	old__mbrtowc = __mbrtowc;
	old__mbsinit = __mbsinit;
	old__mbsnrtowcs = __mbsnrtowcs;
	old__wcrtomb = __wcrtomb;
	old__wcsnrtombs = __wcsnrtombs;

	__mbrtowc = NULL;
	__mbsinit = NULL;
	__mbsnrtowcs = __mbsnrtowcs_std;
	__wcrtomb = NULL;
	__wcsnrtombs = __wcsnrtombs_std;

	rl->__sputrune = NULL;
	rl->__sgetrune = NULL;
	if (strcmp(rl->__encoding, "NONE") == 0)
		ret = _none_init(rl);
	else if (strcmp(rl->__encoding, "ASCII") == 0)
		ret = _ascii_init(rl);
	else if (strcmp(rl->__encoding, "UTF-8") == 0)
		ret = _UTF8_init(rl);
	else if (strcmp(rl->__encoding, "EUC") == 0)
		ret = _EUC_init(rl);
	else if (strcmp(rl->__encoding, "GB18030") == 0)
 		ret = _GB18030_init(rl);
	else if (strcmp(rl->__encoding, "GB2312") == 0)
		ret = _GB2312_init(rl);
	else if (strcmp(rl->__encoding, "GBK") == 0)
		ret = _GBK_init(rl);
	else if (strcmp(rl->__encoding, "BIG5") == 0)
		ret = _BIG5_init(rl);
	else if (strcmp(rl->__encoding, "MSKanji") == 0)
		ret = _MSKanji_init(rl);
	else
		ret = EFTYPE;

	if (ret == 0) {
		if (CachedRuneLocale != NULL) {
			/* See euc.c */
			if (strcmp(CachedRuneLocale->__encoding, "EUC") == 0)
				free(CachedRuneLocale->__variable);
			free(CachedRuneLocale);
		}
		CachedRuneLocale = _CurrentRuneLocale;
		Cached__mb_cur_max = __mb_cur_max;
		Cached__mb_sb_limit = __mb_sb_limit;
		Cached__mbrtowc = __mbrtowc;
		Cached__mbsinit = __mbsinit;
		Cached__mbsnrtowcs = __mbsnrtowcs;
		Cached__wcrtomb = __wcrtomb;
		Cached__wcsnrtombs = __wcsnrtombs;
		(void)strcpy(ctype_encoding, encoding);
	} else {
		__mbrtowc = old__mbrtowc;
		__mbsinit = old__mbsinit;
		__mbsnrtowcs = old__mbsnrtowcs;
		__wcrtomb = old__wcrtomb;
		__wcsnrtombs = old__wcsnrtombs;
		free(rl);
	}

	return (ret);
}

int
__wrap_setrunelocale(const char *locale)
{
	int ret = __setrunelocale(locale);

	if (ret != 0) {
		errno = ret;
		return (_LDP_ERROR);
	}
	return (_LDP_LOADED);
}

