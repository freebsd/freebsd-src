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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)rune.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <rune.h>
#include <stdio.h>
#include <stdlib.h>

extern int		_none_init __P((_RuneLocale *));
extern int		_UTF2_init __P((_RuneLocale *));
extern int		_EUC_init __P((_RuneLocale *));
static _RuneLocale	*_Read_RuneMagi __P((FILE *));

static char *PathLocale = 0;

int
setrunelocale(encoding)
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

	if (!PathLocale && !(PathLocale = getenv("PATH_LOCALE")))
		PathLocale = _PATH_LOCALE;

	sprintf(name, "%s/%s/LC_CTYPE", PathLocale, encoding);

	if ((fp = fopen(name, "r")) == NULL)
		return(ENOENT);

	if ((rl = _Read_RuneMagi(fp)) == 0) {
		fclose(fp);
		return(EFTYPE);
	}

	if (!rl->encoding[0] || !strcmp(rl->encoding, "UTF2")) {
		return(_UTF2_init(rl));
	} else if (!strcmp(rl->encoding, "NONE")) {
		return(_none_init(rl));
	} else if (!strcmp(rl->encoding, "EUC")) {
		return(_EUC_init(rl));
	} else
		return(EINVAL);
}

void
setinvalidrune(ir)
	rune_t ir;
{
	_INVALID_RUNE = ir;
}

static _RuneLocale *
_Read_RuneMagi(fp)
	FILE *fp;
{
	char *data;
	void *np;
	void *lastp;
	_RuneLocale *rl;
	_RuneEntry *rr;
	struct stat sb;
	int x;

	if (fstat(fileno(fp), &sb) < 0)
		return(0);

	if (sb.st_size < sizeof(_RuneLocale))
		return(0);

	if ((data = malloc(sb.st_size)) == NULL)
		return(0);

	rewind(fp); /* Someone might have read the magic number once already */

	if (fread(data, sb.st_size, 1, fp) != 1) {
		free(data);
		return(0);
	}

	rl = (_RuneLocale *)data;
	lastp = data + sb.st_size;

	rl->variable = rl + 1;

	if (memcmp(rl->magic, _RUNE_MAGIC_1, sizeof(rl->magic))) {
		free(data);
		return(0);
	}

	rl->invalid_rune = ntohl(rl->invalid_rune);
	rl->variable_len = ntohl(rl->variable_len);
	rl->runetype_ext.nranges = ntohl(rl->runetype_ext.nranges);
	rl->maplower_ext.nranges = ntohl(rl->maplower_ext.nranges);
	rl->mapupper_ext.nranges = ntohl(rl->mapupper_ext.nranges);

	for (x = 0; x < _CACHED_RUNES; ++x) {
		rl->runetype[x] = ntohl(rl->runetype[x]);
		rl->maplower[x] = ntohl(rl->maplower[x]);
		rl->mapupper[x] = ntohl(rl->mapupper[x]);
	}

	rl->runetype_ext.ranges = (_RuneEntry *)rl->variable;
	rl->variable = rl->runetype_ext.ranges + rl->runetype_ext.nranges;
	if (rl->variable > lastp) {
		free(data);
		return(0);
	}

	rl->maplower_ext.ranges = (_RuneEntry *)rl->variable;
	rl->variable = rl->maplower_ext.ranges + rl->maplower_ext.nranges;
	if (rl->variable > lastp) {
		free(data);
		return(0);
	}

	rl->mapupper_ext.ranges = (_RuneEntry *)rl->variable;
	rl->variable = rl->mapupper_ext.ranges + rl->mapupper_ext.nranges;
	if (rl->variable > lastp) {
		free(data);
		return(0);
	}

	for (x = 0; x < rl->runetype_ext.nranges; ++x) {
		rr = rl->runetype_ext.ranges;

		rr[x].min = ntohl(rr[x].min);
		rr[x].max = ntohl(rr[x].max);
		if ((rr[x].map = ntohl(rr[x].map)) == 0) {
			int len = rr[x].max - rr[x].min + 1;
			rr[x].types = rl->variable;
			rl->variable = rr[x].types + len;
			if (rl->variable > lastp) {
				free(data);
				return(0);
			}
			while (len-- > 0)
				rr[x].types[len] = ntohl(rr[x].types[len]);
		} else
			rr[x].types = 0;
	}

	for (x = 0; x < rl->maplower_ext.nranges; ++x) {
		rr = rl->maplower_ext.ranges;

		rr[x].min = ntohl(rr[x].min);
		rr[x].max = ntohl(rr[x].max);
		rr[x].map = ntohl(rr[x].map);
	}

	for (x = 0; x < rl->mapupper_ext.nranges; ++x) {
		rr = rl->mapupper_ext.ranges;

		rr[x].min = ntohl(rr[x].min);
		rr[x].max = ntohl(rr[x].max);
		rr[x].map = ntohl(rr[x].map);
	}
	if (((char *)rl->variable) + rl->variable_len > (char *)lastp) {
		free(data);
		return(0);
	}

	/*
	 * Go out and zero pointers that should be zero.
	 */
	if (!rl->variable_len)
		rl->variable = 0;

	if (!rl->runetype_ext.nranges)
		rl->runetype_ext.ranges = 0;

	if (!rl->maplower_ext.nranges)
		rl->maplower_ext.ranges = 0;

	if (!rl->mapupper_ext.nranges)
		rl->mapupper_ext.ranges = 0;
	    
	return(rl);
}

unsigned long
___runetype(c)
	_BSD_RUNE_T_ c;
{
	int x;
	_RuneRange *rr = &_CurrentRuneLocale->runetype_ext;
	_RuneEntry *re = rr->ranges;

	if (c == EOF)
		return(0);
	for (x = 0; x < rr->nranges; ++x, ++re) {
		if (c < re->min)
			return(0L);
		if (c <= re->max) {
			if (re->types)
			    return(re->types[c - re->min]);
			else
			    return(re->map);
		}
	}
	return(0L);
}

_BSD_RUNE_T_
___toupper(c)
	_BSD_RUNE_T_ c;
{
	int x;
	_RuneRange *rr = &_CurrentRuneLocale->mapupper_ext;
	_RuneEntry *re = rr->ranges;

	if (c == EOF)
		return(EOF);
	for (x = 0; x < rr->nranges; ++x, ++re) {
		if (c < re->min)
			return(c);
		if (c <= re->max)
			return(re->map + c - re->min);
	}
	return(c);
}

_BSD_RUNE_T_
___tolower(c)
	_BSD_RUNE_T_ c;
{
	int x;
	_RuneRange *rr = &_CurrentRuneLocale->maplower_ext;
	_RuneEntry *re = rr->ranges;

	if (c == EOF)
		return(EOF);
	for (x = 0; x < rr->nranges; ++x, ++re) {
		if (c < re->min)
			return(c);
		if (c <= re->max)
			return(re->map + c - re->min);
	}
	return(c);
}


#if !defined(_USE_CTYPE_INLINE_) && !defined(_USE_CTYPE_MACROS_)
/*
 * See comments in <machine/ansi.h>
 */
int
__istype(c, f)
	_BSD_RUNE_T_ c;
	unsigned long f;
{
	return ((((c & _CRMASK) ? ___runetype(c)
           : _CurrentRuneLocale->runetype[c]) & f) ? 1 : 0);
}

int
__isctype(_BSD_RUNE_T_ c, unsigned long f)
	_BSD_RUNE_T_ c;
	unsigned long f;
{
	return ((((c & _CRMASK) ? 0
           : _DefaultRuneLocale.runetype[c]) & f) ? 1 : 0);
}

_BSD_RUNE_T_
toupper(c)
	_BSD_RUNE_T_ c;
{
	return ((c & _CRMASK) ?
	    ___toupper(c) : _CurrentRuneLocale->mapupper[c]);
}

_BSD_RUNE_T_
tolower(c)
	_BSD_RUNE_T_ c;
{
	return ((c & _CRMASK) ?
	    ___tolower(c) : _CurrentRuneLocale->maplower[c]);
}
#endif
