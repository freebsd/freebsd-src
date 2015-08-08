/*
 * Copyright 2011 Nexenta Systems, Inc.  All rights reserved.
 * Copyright 2012 Garrett D'Amore <garrett@damore.org>  All rights reserved.
 * Copyright 2015 John Marino <draco@marino.st>
 *
 * This source code is derived from the illumos localedef command, and
 * provided under BSD-style license terms by Nexenta Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * LC_CTYPE database generation routines for localedef.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/tree.h>

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <wchar.h>
#include <ctype.h>
#include <wctype.h>
#include <unistd.h>
#include "localedef.h"
#include "parser.h"
#include "runefile.h"


#define _ISUPPER	_CTYPE_U
#define _ISLOWER	_CTYPE_L
#define	_ISDIGIT	_CTYPE_D
#define	_ISXDIGIT	_CTYPE_X
#define	_ISSPACE	_CTYPE_S
#define	_ISBLANK	_CTYPE_B
#define	_ISALPHA	_CTYPE_A
#define	_ISPUNCT	_CTYPE_P
#define	_ISGRAPH	_CTYPE_G
#define	_ISPRINT	_CTYPE_R
#define	_ISCNTRL	_CTYPE_C
#define	_E1		_CTYPE_Q
#define	_E2		_CTYPE_I
#define	_E3		0
#define	_E4		0
#define	_E5		_CTYPE_T

static wchar_t		last_ctype;
static int ctype_compare(const void *n1, const void *n2);

typedef struct ctype_node {
	wchar_t wc;
	int32_t	ctype;
	int32_t	toupper;
	int32_t	tolower;
	RB_ENTRY(ctype_node) entry;
} ctype_node_t;

RB_HEAD(ctypes, ctype_node) ctypes;
RB_GENERATE_STATIC(ctypes, ctype_node, entry, ctype_compare);

static int
ctype_compare(const void *n1, const void *n2)
{
	const ctype_node_t *c1 = n1;
	const ctype_node_t *c2 = n2;

	return (c1->wc < c2->wc ? -1 : c1->wc > c2->wc ? 1 : 0);
}

void
init_ctype(void)
{
	RB_INIT(&ctypes);
}


static void
add_ctype_impl(ctype_node_t *ctn)
{
	switch (last_kw) {
	case T_ISUPPER:
		ctn->ctype |= (_ISUPPER | _ISALPHA | _ISGRAPH | _ISPRINT);
		break;
	case T_ISLOWER:
		ctn->ctype |= (_ISLOWER | _ISALPHA | _ISGRAPH | _ISPRINT);
		break;
	case T_ISALPHA:
		ctn->ctype |= (_ISALPHA | _ISGRAPH | _ISPRINT);
		break;
	case T_ISDIGIT:
		ctn->ctype |= (_ISDIGIT | _ISGRAPH | _ISPRINT | _ISXDIGIT);
		break;
	case T_ISSPACE:
		ctn->ctype |= _ISSPACE;
		break;
	case T_ISCNTRL:
		ctn->ctype |= _ISCNTRL;
		break;
	case T_ISGRAPH:
		ctn->ctype |= (_ISGRAPH | _ISPRINT);
		break;
	case T_ISPRINT:
		ctn->ctype |= _ISPRINT;
		break;
	case T_ISPUNCT:
		ctn->ctype |= (_ISPUNCT | _ISGRAPH | _ISPRINT);
		break;
	case T_ISXDIGIT:
		ctn->ctype |= (_ISXDIGIT | _ISPRINT);
		break;
	case T_ISBLANK:
		ctn->ctype |= (_ISBLANK | _ISSPACE);
		break;
	case T_ISPHONOGRAM:
		ctn->ctype |= (_E1 | _ISPRINT | _ISGRAPH);
		break;
	case T_ISIDEOGRAM:
		ctn->ctype |= (_E2 | _ISPRINT | _ISGRAPH);
		break;
	case T_ISENGLISH:
		ctn->ctype |= (_E3 | _ISPRINT | _ISGRAPH);
		break;
	case T_ISNUMBER:
		ctn->ctype |= (_E4 | _ISPRINT | _ISGRAPH);
		break;
	case T_ISSPECIAL:
		ctn->ctype |= (_E5 | _ISPRINT | _ISGRAPH);
		break;
	case T_ISALNUM:
		/*
		 * We can't do anything with this.  The character
		 * should already be specified as a digit or alpha.
		 */
		break;
	default:
		errf("not a valid character class");
	}
}

static ctype_node_t *
get_ctype(wchar_t wc)
{
	ctype_node_t	srch;
	ctype_node_t	*ctn;

	srch.wc = wc;
	if ((ctn = RB_FIND(ctypes, &ctypes, &srch)) == NULL) {
		if ((ctn = calloc(1, sizeof (*ctn))) == NULL) {
			errf("out of memory");
			return (NULL);
		}
		ctn->wc = wc;

		RB_INSERT(ctypes, &ctypes, ctn);
	}
	return (ctn);
}

void
add_ctype(int val)
{
	ctype_node_t	*ctn;

	if ((ctn = get_ctype(val)) == NULL) {
		INTERR;
		return;
	}
	add_ctype_impl(ctn);
	last_ctype = ctn->wc;
}

void
add_ctype_range(int end)
{
	ctype_node_t	*ctn;
	wchar_t		cur;

	if (end < last_ctype) {
		errf("malformed character range (%u ... %u))",
		    last_ctype, end);
		return;
	}
	for (cur = last_ctype + 1; cur <= end; cur++) {
		if ((ctn = get_ctype(cur)) == NULL) {
			INTERR;
			return;
		}
		add_ctype_impl(ctn);
	}
	last_ctype = end;

}

/*
 * A word about widths: if the width mask is specified, then libc
 * unconditionally honors it.  Otherwise, it assumes printable
 * characters have width 1, and non-printable characters have width
 * -1 (except for NULL which is special with with 0).  Hence, we have
 * no need to inject defaults here -- the "default" unset value of 0
 * indicates that libc should use its own logic in wcwidth as described.
 */
void
add_width(int wc, int width)
{
	ctype_node_t	*ctn;

	if ((ctn = get_ctype(wc)) == NULL) {
		INTERR;
		return;
	}
	ctn->ctype &= ~(_CTYPE_SWM);
	switch (width) {
	case 0:
		ctn->ctype |= _CTYPE_SW0;
		break;
	case 1:
		ctn->ctype |= _CTYPE_SW1;
		break;
	case 2:
		ctn->ctype |= _CTYPE_SW2;
		break;
	case 3:
		ctn->ctype |= _CTYPE_SW3;
		break;
	}
}

void
add_width_range(int start, int end, int width)
{
	for (; start <= end; start++) {
		add_width(start, width);
	}
}

void
add_caseconv(int val, int wc)
{
	ctype_node_t	*ctn;

	ctn = get_ctype(val);
	if (ctn == NULL) {
		INTERR;
		return;
	}

	switch (last_kw) {
	case T_TOUPPER:
		ctn->toupper = wc;
		break;
	case T_TOLOWER:
		ctn->tolower = wc;
		break;
	default:
		INTERR;
		break;
	}
}

void
dump_ctype(void)
{
	FILE		*f;
	_FileRuneLocale	rl;
	ctype_node_t	*ctn, *last_ct, *last_lo, *last_up;
	_FileRuneEntry	*ct = NULL;
	_FileRuneEntry	*lo = NULL;
	_FileRuneEntry	*up = NULL;
	wchar_t		wc;

	(void) memset(&rl, 0, sizeof (rl));
	last_ct = NULL;
	last_lo = NULL;
	last_up = NULL;

	if ((f = open_category()) == NULL)
		return;

	(void) memcpy(rl.magic, _FILE_RUNE_MAGIC_1, 8);
	(void) strncpy(rl.encoding, get_wide_encoding(), sizeof (rl.encoding));

	/*
	 * Initialize the identity map.
	 */
	for (wc = 0; (unsigned)wc < _CACHED_RUNES; wc++) {
		rl.maplower[wc] = wc;
		rl.mapupper[wc] = wc;
	}

	RB_FOREACH(ctn, ctypes, &ctypes) {
		int conflict = 0;

		wc = ctn->wc;

		/*
		 * POSIX requires certain portable characters have
		 * certain types.  Add them if they are missing.
		 */
		if ((wc >= 1) && (wc <= 127)) {
			if ((wc >= 'A') && (wc <= 'Z'))
				ctn->ctype |= _ISUPPER;
			if ((wc >= 'a') && (wc <= 'z'))
				ctn->ctype |= _ISLOWER;
			if ((wc >= '0') && (wc <= '9'))
				ctn->ctype |= _ISDIGIT;
			if (strchr(" \f\n\r\t\v", (char)wc) != NULL)
				ctn->ctype |= _ISSPACE;
			if (strchr("0123456789ABCDEFabcdef", (char)wc) != NULL)
				ctn->ctype |= _ISXDIGIT;
			if (strchr(" \t", (char)wc))
				ctn->ctype |= _ISBLANK;

			/*
			 * Technically these settings are only
			 * required for the C locale.  However, it
			 * turns out that because of the historical
			 * version of isprint(), we need them for all
			 * locales as well.  Note that these are not
			 * necessarily valid punctation characters in
			 * the current language, but ispunct() needs
			 * to return TRUE for them.
			 */
			if (strchr("!\"'#$%&()*+,-./:;<=>?@[\\]^_`{|}~",
			    (char)wc))
				ctn->ctype |= _ISPUNCT;
		}

		/*
		 * POSIX also requires that certain types imply
		 * others.  Add any inferred types here.
		 */
		if (ctn->ctype & (_ISUPPER |_ISLOWER))
			ctn->ctype |= _ISALPHA;
		if (ctn->ctype & _ISDIGIT)
			ctn->ctype |= _ISXDIGIT;
		if (ctn->ctype & _ISBLANK)
			ctn->ctype |= _ISSPACE;
		if (ctn->ctype & (_ISALPHA|_ISDIGIT|_ISXDIGIT))
			ctn->ctype |= _ISGRAPH;
		if (ctn->ctype & _ISGRAPH)
			ctn->ctype |= _ISPRINT;

		/*
		 * Finally, POSIX requires that certain combinations
		 * are invalid.  We don't flag this as a fatal error,
		 * but we will warn about.
		 */
		if ((ctn->ctype & _ISALPHA) &&
		    (ctn->ctype & (_ISPUNCT|_ISDIGIT)))
			conflict++;
		if ((ctn->ctype & _ISPUNCT) &
		    (ctn->ctype & (_ISDIGIT|_ISALPHA|_ISXDIGIT)))
			conflict++;
		if ((ctn->ctype & _ISSPACE) && (ctn->ctype & _ISGRAPH))
			conflict++;
		if ((ctn->ctype & _ISCNTRL) & _ISPRINT)
			conflict++;
		if ((wc == ' ') && (ctn->ctype & (_ISPUNCT|_ISGRAPH)))
			conflict++;

		if (conflict) {
			warn("conflicting classes for character 0x%x (%x)",
			    wc, ctn->ctype);
		}
		/*
		 * Handle the lower 256 characters using the simple
		 * optimization.  Note that if we have not defined the
		 * upper/lower case, then we identity map it.
		 */
		if ((unsigned)wc < _CACHED_RUNES) {
			rl.runetype[wc] = ctn->ctype;
			if (ctn->tolower)
				rl.maplower[wc] = ctn->tolower;
			if (ctn->toupper)
				rl.mapupper[wc] = ctn->toupper;
			continue;
		}

		if ((last_ct != NULL) && (last_ct->ctype == ctn->ctype)) {
			ct[rl.runetype_ext_nranges-1].max = wc;
			last_ct = ctn;
		} else {
			rl.runetype_ext_nranges++;
			ct = realloc(ct,
			    sizeof (*ct) * rl.runetype_ext_nranges);
			ct[rl.runetype_ext_nranges - 1].min = wc;
			ct[rl.runetype_ext_nranges - 1].max = wc;
			ct[rl.runetype_ext_nranges - 1].map = ctn->ctype;
			last_ct = ctn;
		}
		if (ctn->tolower == 0) {
			last_lo = NULL;
		} else if ((last_lo != NULL) &&
		    (last_lo->tolower + 1 == ctn->tolower)) {
			lo[rl.maplower_ext_nranges-1].max = wc;
			last_lo = ctn;
		} else {
			rl.maplower_ext_nranges++;
			lo = realloc(lo,
			    sizeof (*lo) * rl.maplower_ext_nranges);
			lo[rl.maplower_ext_nranges - 1].min = wc;
			lo[rl.maplower_ext_nranges - 1].max = wc;
			lo[rl.maplower_ext_nranges - 1].map = ctn->tolower;
			last_lo = ctn;
		}

		if (ctn->toupper == 0) {
			last_up = NULL;
		} else if ((last_up != NULL) &&
		    (last_up->toupper + 1 == ctn->toupper)) {
			up[rl.mapupper_ext_nranges-1].max = wc;
			last_up = ctn;
		} else {
			rl.mapupper_ext_nranges++;
			up = realloc(up,
			    sizeof (*up) * rl.mapupper_ext_nranges);
			up[rl.mapupper_ext_nranges - 1].min = wc;
			up[rl.mapupper_ext_nranges - 1].max = wc;
			up[rl.mapupper_ext_nranges - 1].map = ctn->toupper;
			last_up = ctn;
		}
	}

	if ((wr_category(&rl, sizeof (rl), f) < 0) ||
	    (wr_category(ct, sizeof (*ct) * rl.runetype_ext_nranges, f) < 0) ||
	    (wr_category(lo, sizeof (*lo) * rl.maplower_ext_nranges, f) < 0) ||
	    (wr_category(up, sizeof (*up) * rl.mapupper_ext_nranges, f) < 0)) {
		return;
	}

	close_category(f);
}
