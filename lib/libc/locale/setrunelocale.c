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

/* setrunelocale() is obsolete in FreeBSD 6 -- use ANSI functions instead. */
#define	OBSOLETE_IN_6

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <rune.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wchar.h>
#include "ldpart.h"
#include "setlocale.h"

extern int		_none_init(_RuneLocale *);
extern int		_UTF2_init(_RuneLocale *);
extern int		_UTF8_init(_RuneLocale *);
extern int		_EUC_init(_RuneLocale *);
extern int		_GB18030_init(_RuneLocale *);
extern int		_GBK_init(_RuneLocale *);
extern int		_BIG5_init(_RuneLocale *);
extern int		_MSKanji_init(_RuneLocale *);
extern _RuneLocale	*_Read_RuneMagi(FILE *);

extern size_t (*__mbrtowc)(wchar_t * __restrict, const char * __restrict,
    size_t, mbstate_t * __restrict);
extern size_t (*__wcrtomb)(char * __restrict, wchar_t, mbstate_t * __restrict);
extern size_t __emulated_mbrtowc(wchar_t * __restrict, const char * __restrict,
    size_t, mbstate_t * __restrict ps);
extern size_t __emulated_wcrtomb(char * __restrict, wchar_t,
    mbstate_t * __restrict ps);
extern rune_t __emulated_sgetrune(const char *, size_t, const char **);
extern int __emulated_sputrune(rune_t, char *, size_t, char **);

static int		__setrunelocale(const char *);

__warn_references(setrunelocale, "warning: setrunelocale() is deprecated. See setrunelocale(3).");
int
setrunelocale(char *encoding)
{
	int ret;

	if (!encoding || !*encoding || strlen(encoding) > ENCODING_LEN ||
	    (encoding[0] == '.' &&
	     (encoding[1] == '\0' ||
	      (encoding[1] == '.' && encoding[2] == '\0'))) ||
	    strchr(encoding, '/') != NULL)
		return (EINVAL);

	ret = __detect_path_locale();
	if (ret != 0)
		return (ret);

	return (__setrunelocale((const char *)encoding));
}

static int
__setrunelocale(const char *encoding)
{
	FILE *fp;
	char name[PATH_MAX];
	_RuneLocale *rl;
	int saverr, ret;
	static char ctype_encoding[ENCODING_LEN + 1];
	static _RuneLocale *CachedRuneLocale;
	static int Cached__mb_cur_max;

	/*
	 * The "C" and "POSIX" locale are always here.
	 */
	if (strcmp(encoding, "C") == 0 || strcmp(encoding, "POSIX") == 0) {
		_CurrentRuneLocale = &_DefaultRuneLocale;
		__mb_cur_max = 1;
		return (0);
	}

	/*
	 * If the locale name is the same as our cache, use the cache.
	 */
	if (CachedRuneLocale != NULL &&
	    strcmp(encoding, ctype_encoding) == 0) {
		_CurrentRuneLocale = CachedRuneLocale;
		__mb_cur_max = Cached__mb_cur_max;
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

	__mbrtowc = __emulated_mbrtowc;
	__wcrtomb = __emulated_wcrtomb;
	rl->sputrune = __emulated_sputrune;
	rl->sgetrune = __emulated_sgetrune;
	if (strcmp(rl->encoding, "NONE") == 0)
		ret = _none_init(rl);
	else if (strcmp(rl->encoding, "UTF2") == 0)
		ret = _UTF2_init(rl);
	else if (strcmp(rl->encoding, "UTF-8") == 0)
		ret = _UTF8_init(rl);
	else if (strcmp(rl->encoding, "EUC") == 0)
		ret = _EUC_init(rl);
 	else if (strcmp(rl->encoding, "GB18030") == 0)
 		ret = _GB18030_init(rl);
	else if (strcmp(rl->encoding, "GBK") == 0)
		ret = _GBK_init(rl);
	else if (strcmp(rl->encoding, "BIG5") == 0)
		ret = _BIG5_init(rl);
	else if (strcmp(rl->encoding, "MSKanji") == 0)
		ret = _MSKanji_init(rl);
	else
		ret = EFTYPE;
	if (ret == 0) {
		if (CachedRuneLocale != NULL) {
			/* See euc.c */
			if (strcmp(CachedRuneLocale->encoding, "EUC") == 0)
				free(CachedRuneLocale->variable);
			free(CachedRuneLocale);
		}
		CachedRuneLocale = _CurrentRuneLocale;
		Cached__mb_cur_max = __mb_cur_max;
		(void)strcpy(ctype_encoding, encoding);
	} else
		free(rl);

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

