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

#include <rune.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

char *_PathLocale;

extern int		_none_init __P((_RuneLocale *));
#ifdef XPG4
extern int		_UTF2_init __P((_RuneLocale *));
extern int		_EUC_init __P((_RuneLocale *));
#endif
extern _RuneLocale      *_Read_RuneMagi __P((FILE *));

#ifdef XPG4
int
setrunelocale(encoding)
	char *encoding;
{
	return _xpg4_setrunelocale(encoding);
}
#endif

int
#ifndef XPG4
setrunelocale(encoding)
#else
_xpg4_setrunelocale(encoding)
#endif
	char *encoding;
{
	FILE *fp;
	char name[PATH_MAX];
	_RuneLocale *rl;

	if (!encoding)
	    return(EFAULT);

	/*
	 * The "C" and "POSIX" locale are always here.
	 */
	if (!strcmp(encoding, "C") || !strcmp(encoding, "POSIX")) {
		_CurrentRuneLocale = &_DefaultRuneLocale;
		return(0);
	}

	if (!_PathLocale && !(_PathLocale = getenv("PATH_LOCALE")))
		_PathLocale = _PATH_LOCALE;

	(void) strcpy(name, _PathLocale);
	(void) strcat(name, "/");
	(void) strcat(name, encoding);
	(void) strcat(name, "/LC_CTYPE");

	if ((fp = fopen(name, "r")) == NULL)
		return(ENOENT);

	if ((rl = _Read_RuneMagi(fp)) == 0) {
		fclose(fp);
		return(EFTYPE);
	}
	fclose(fp);

#ifdef XPG4
	if (!rl->encoding[0] || !strcmp(rl->encoding, "UTF2")) {
		return(_UTF2_init(rl));
#else
	if (!rl->encoding[0]) {
		return(EINVAL);
#endif
	} else if (!strcmp(rl->encoding, "NONE")) {
		return(_none_init(rl));
#ifdef XPG4
	} else if (!strcmp(rl->encoding, "EUC")) {
		return(_EUC_init(rl));
#endif
	} else
		return(EINVAL);
}

