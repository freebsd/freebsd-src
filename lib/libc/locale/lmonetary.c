/*
 * Copyright (c) 2000, 2001 Alexey Zelkin <phantom@FreeBSD.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <limits.h>
#include <stdlib.h>
#include "lmonetary.h"
#include "ldpart.h"

extern int __mlocale_changed;
extern const char * __fix_locale_grouping_str(const char *);

#define LCMONETARY_SIZE (sizeof(struct lc_monetary_T) / sizeof(char *))

static char	empty[] = "";
static char	numempty[] = { CHAR_MAX, '\0'};

static const struct lc_monetary_T _C_monetary_locale = {
	empty,		/* int_curr_symbol */
	empty,		/* currency_symbol */
	empty,		/* mon_decimal_point */
	empty,		/* mon_thousands_sep */
	numempty,	/* mon_grouping */
	empty,		/* positive_sign */
	empty,		/* negative_sign */
	numempty,	/* int_frac_digits */
	numempty,	/* frac_digits */
	numempty,	/* p_cs_precedes */
	numempty,	/* p_sep_by_space */
	numempty,	/* n_cs_precedes */
	numempty,	/* n_sep_by_space */
	numempty,	/* p_sign_posn */
	numempty	/* n_sign_posn */
};

static struct lc_monetary_T _monetary_locale;
static int	_monetary_using_locale;
static char	*_monetary_locale_buf;

static char
cnv(const char *str) {
	int i = strtol(str, NULL, 10);
	if (i == -1)
		i = CHAR_MAX;
	return (char)i;
}

int
__monetary_load_locale(const char *name) {

	int ret;
	__mlocale_changed = 1;
	ret = __part_load_locale(name, &_monetary_using_locale,
		_monetary_locale_buf, "LC_MONETARY",
		LCMONETARY_SIZE, LCMONETARY_SIZE,
		(const char **)&_monetary_locale);
	if (ret == 0 && _monetary_using_locale) {
		_monetary_locale.mon_grouping =
		     __fix_locale_grouping_str(_monetary_locale.mon_grouping);

#define M_ASSIGN_CHAR(NAME) (((char *)_monetary_locale.NAME)[0] = \
			     cnv(_monetary_locale.NAME))

		M_ASSIGN_CHAR(int_frac_digits);
		M_ASSIGN_CHAR(frac_digits);
		M_ASSIGN_CHAR(p_cs_precedes);
		M_ASSIGN_CHAR(p_sep_by_space);
		M_ASSIGN_CHAR(n_cs_precedes);
		M_ASSIGN_CHAR(n_sep_by_space);
		M_ASSIGN_CHAR(p_sign_posn);
		M_ASSIGN_CHAR(n_sign_posn);
	}
	return ret;
}

struct lc_monetary_T *
__get_current_monetary_locale(void) {

	return (_monetary_using_locale
		? &_monetary_locale
		: (struct lc_monetary_T *)&_C_monetary_locale);
}

#ifdef LOCALE_DEBUG
void
monetdebug() {
printf(	"int_curr_symbol = %s\n"
	"currency_symbol = %s\n"
	"mon_decimal_point = %s\n"
	"mon_thousands_sep = %s\n"
	"mon_grouping = %s\n"
	"positive_sign = %s\n"
	"negative_sign = %s\n"
	"int_frac_digits = %d\n"
	"frac_digits = %d\n"
	"p_cs_precedes = %d\n"
	"p_sep_by_space = %d\n"
	"n_cs_precedes = %d\n"
	"n_sep_by_space = %d\n"
	"p_sign_posn = %d\n"
	"n_sign_posn = %d\n",
	_monetary_locale.int_curr_symbol,
	_monetary_locale.currency_symbol,
	_monetary_locale.mon_decimal_point,
	_monetary_locale.mon_thousands_sep,
	_monetary_locale.mon_grouping,
	_monetary_locale.positive_sign,
	_monetary_locale.negative_sign,
	_monetary_locale.int_frac_digits[0],
	_monetary_locale.frac_digits[0],
	_monetary_locale.p_cs_precedes[0],
	_monetary_locale.p_sep_by_space[0],
	_monetary_locale.n_cs_precedes[0],
	_monetary_locale.n_sep_by_space[0],
	_monetary_locale.p_sign_posn[0],
	_monetary_locale.n_sign_posn[0]
);
}
#endif /* LOCALE_DEBUG */
