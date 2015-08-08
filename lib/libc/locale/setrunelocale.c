/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Borman at Krystal Technologies.
 *
 * Copyright (c) 2011 The FreeBSD Foundation
 * All rights reserved.
 * Portions of this software were developed by David Chisnall
 * under sponsorship from the FreeBSD Foundation.
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

#define __RUNETYPE_INTERNAL 1

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

#undef _CurrentRuneLocale
extern _RuneLocale const *_CurrentRuneLocale;
#ifndef __NO_TLS
/*
 * A cached version of the runes for this thread.  Used by ctype.h
 */
_Thread_local const _RuneLocale *_ThreadRuneLocale;
#endif

extern int __mb_sb_limit;

extern _RuneLocale	*_Read_RuneMagi(const char *);

static int		__setrunelocale(struct xlocale_ctype *l, const char *);

static void
destruct_ctype(void *v)
{
	struct xlocale_ctype *l = v;

	if (&_DefaultRuneLocale != l->runes) 
		free(l->runes);
	free(l);
}

const _RuneLocale *
__getCurrentRuneLocale(void)
{

	return XLOCALE_CTYPE(__get_locale())->runes;
}

static void
free_runes(_RuneLocale *rl)
{
	if ((rl != &_DefaultRuneLocale) && (rl)) {
		free(rl);
	}
}

static int
__setrunelocale(struct xlocale_ctype *l, const char *encoding)
{
	_RuneLocale *rl;
	int ret;
	char path[PATH_MAX];
	struct xlocale_ctype saved = *l;

	/*
	 * The "C" and "POSIX" locale are always here.
	 */
	if (strcmp(encoding, "C") == 0 || strcmp(encoding, "POSIX") == 0) {
		free_runes(saved.runes);
		(void) _none_init(l, (_RuneLocale*)&_DefaultRuneLocale);
		return (0);
	}

	/* Range checking not needed, encoding length already checked before */
	(void) snprintf(path, sizeof (path), "%s/%s/LC_CTYPE",
	    _PathLocale, encoding);

	if ((rl = _Read_RuneMagi(path)) == NULL) {
		errno = EINVAL;
		return (errno);
	}

	l->__mbrtowc = NULL;
	l->__mbsinit = NULL;
	l->__mbsnrtowcs = NULL;
	l->__wcrtomb = NULL;
	l->__wcsnrtombs = NULL;

	rl->__sputrune = NULL;
	rl->__sgetrune = NULL;
	if (strcmp(rl->__encoding, "NONE") == 0)
		ret = _none_init(l, rl);
	else if (strcmp(rl->__encoding, "UTF-8") == 0)
		ret = _UTF8_init(l, rl);
	else if (strcmp(rl->__encoding, "EUC-CN") == 0)
		ret = _EUC_CN_init(l, rl);
	else if (strcmp(rl->__encoding, "EUC-JP") == 0)
		ret = _EUC_JP_init(l, rl);
	else if (strcmp(rl->__encoding, "EUC-KR") == 0)
		ret = _EUC_KR_init(l, rl);
	else if (strcmp(rl->__encoding, "EUC-TW") == 0)
		ret = _EUC_TW_init(l, rl);
	else if (strcmp(rl->__encoding, "GB18030") == 0)
 		ret = _GB18030_init(l, rl);
	else if (strcmp(rl->__encoding, "GB2312") == 0)
		ret = _GB2312_init(l, rl);
	else if (strcmp(rl->__encoding, "GBK") == 0)
		ret = _GBK_init(l, rl);
	else if (strcmp(rl->__encoding, "BIG5") == 0)
		ret = _BIG5_init(l, rl);
	else if (strcmp(rl->__encoding, "MSKanji") == 0)
		ret = _MSKanji_init(l, rl);
	else
		ret = EFTYPE;

	if (ret == 0) {
		/* Free the old runes if it exists. */
		free_runes(saved.runes);
	} else {
		/* Restore the saved version if this failed. */
		memcpy(l, &saved, sizeof(struct xlocale_ctype));
		free(rl);
	}

	return (ret);
}

int
__wrap_setrunelocale(const char *locale)
{
	int ret = __setrunelocale(&__xlocale_global_ctype, locale);

	if (ret != 0) {
		errno = ret;
		return (_LDP_ERROR);
	}
	__mb_cur_max = __xlocale_global_ctype.__mb_cur_max;
	__mb_sb_limit = __xlocale_global_ctype.__mb_sb_limit;
	_CurrentRuneLocale = __xlocale_global_ctype.runes;
	return (_LDP_LOADED);
}

#ifndef __NO_TLS
void
__set_thread_rune_locale(locale_t loc)
{

	if (loc == NULL) {
		_ThreadRuneLocale = &_DefaultRuneLocale;
	} else if (loc == LC_GLOBAL_LOCALE) {
		_ThreadRuneLocale = 0;
	} else {
		_ThreadRuneLocale = XLOCALE_CTYPE(loc)->runes;
	}
}
#endif

void *
__ctype_load(const char *locale, locale_t unused __unused)
{
	struct xlocale_ctype *l = calloc(sizeof(struct xlocale_ctype), 1);

	l->header.header.destructor = destruct_ctype;
	if (__setrunelocale(l, locale))
	{
		free(l);
		return NULL;
	}
	return l;
}
