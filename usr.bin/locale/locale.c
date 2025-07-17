/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2002, 2003 Alexey Zelkin <phantom@FreeBSD.org>
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
 */

/*
 * XXX: implement missing era_* (LC_TIME) keywords (require libc &
 *	nl_langinfo(3) extensions)
 *
 * XXX: correctly handle reserved 'charmap' keyword and '-m' option (require
 *	localedef(1) implementation).  Currently it's handled via
 *	nl_langinfo(CODESET).
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sbuf.h>

#include <dirent.h>
#include <err.h>
#include <limits.h>
#include <locale.h>
#include <langinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stringlist.h>
#include <unistd.h>
#include "setlocale.h"

/* Local prototypes */
char	*format_grouping(char *);
void	init_locales_list(void);
void	list_charmaps(void);
void	list_locales(void);
const char *lookup_localecat(int);
char	*kwval_lconv(int);
int	kwval_lookup(const char *, char **, int *, int *, int *);
int	showdetails(const char *);
void	showkeywordslist(char *substring);
void	showlocale(void);
void	usage(void);

/* Global variables */
static StringList *locales = NULL;

static int	all_locales = 0;
static int	all_charmaps = 0;
static int	prt_categories = 0;
static int	prt_keywords = 0;

static const struct _lcinfo {
	const char	*name;
	int		id;
} lcinfo [] = {
	{ "LC_CTYPE",		LC_CTYPE },
	{ "LC_COLLATE",		LC_COLLATE },
	{ "LC_TIME",		LC_TIME },
	{ "LC_NUMERIC",		LC_NUMERIC },
	{ "LC_MONETARY",	LC_MONETARY },
	{ "LC_MESSAGES",	LC_MESSAGES }
};
#define	NLCINFO nitems(lcinfo)

/* ids for values not referenced by nl_langinfo() */
enum {
	KW_GROUPING,
	KW_INT_CURR_SYMBOL,
	KW_CURRENCY_SYMBOL,
	KW_MON_DECIMAL_POINT,
	KW_MON_THOUSANDS_SEP,
	KW_MON_GROUPING,
	KW_POSITIVE_SIGN,
	KW_NEGATIVE_SIGN,
	KW_INT_FRAC_DIGITS,
	KW_FRAC_DIGITS,
	KW_P_CS_PRECEDES,
	KW_P_SEP_BY_SPACE,
	KW_N_CS_PRECEDES,
	KW_N_SEP_BY_SPACE,
	KW_P_SIGN_POSN,
	KW_N_SIGN_POSN,
	KW_INT_P_CS_PRECEDES,
	KW_INT_P_SEP_BY_SPACE,
	KW_INT_N_CS_PRECEDES,
	KW_INT_N_SEP_BY_SPACE,
	KW_INT_P_SIGN_POSN,
	KW_INT_N_SIGN_POSN,
	KW_TIME_DAY,
	KW_TIME_ABDAY,
	KW_TIME_MON,
	KW_TIME_ABMON,
	KW_TIME_AM_PM
};

enum {
	TYPE_NUM,
	TYPE_STR,
	TYPE_UNQ
};

enum {
	SRC_LINFO,
	SRC_LCONV,
	SRC_LTIME
};

static const struct _kwinfo {
	const char	*name;
	int		type;
	int		catid;		/* LC_* */
	int		source;
	int		value_ref;
	const char	*comment;
} kwinfo [] = {
	{ "charmap",		TYPE_STR, LC_CTYPE,	SRC_LINFO,
	  CODESET, "" },					/* hack */

	/* LC_MONETARY - POSIX */
	{ "int_curr_symbol",	TYPE_STR, LC_MONETARY,	SRC_LCONV,
	  KW_INT_CURR_SYMBOL, "" },
	{ "currency_symbol",	TYPE_STR, LC_MONETARY,	SRC_LCONV,
	  KW_CURRENCY_SYMBOL, "" },
	{ "mon_decimal_point",	TYPE_STR, LC_MONETARY,	SRC_LCONV,
	  KW_MON_DECIMAL_POINT, "" },
	{ "mon_thousands_sep",	TYPE_STR, LC_MONETARY,	SRC_LCONV,
	  KW_MON_THOUSANDS_SEP, "" },
	{ "mon_grouping",	TYPE_UNQ, LC_MONETARY,	SRC_LCONV,
	  KW_MON_GROUPING, "" },
	{ "positive_sign",	TYPE_STR, LC_MONETARY,	SRC_LCONV,
	  KW_POSITIVE_SIGN, "" },
	{ "negative_sign",	TYPE_STR, LC_MONETARY,	SRC_LCONV,
	  KW_NEGATIVE_SIGN, "" },
	{ "int_frac_digits",	TYPE_NUM, LC_MONETARY,	SRC_LCONV,
	  KW_INT_FRAC_DIGITS, "" },
	{ "frac_digits",	TYPE_NUM, LC_MONETARY,	SRC_LCONV,
	  KW_FRAC_DIGITS, "" },
	{ "p_cs_precedes",	TYPE_NUM, LC_MONETARY,	SRC_LCONV,
	  KW_P_CS_PRECEDES, "" },
	{ "p_sep_by_space",	TYPE_NUM, LC_MONETARY,	SRC_LCONV,
	  KW_P_SEP_BY_SPACE, "" },
	{ "n_cs_precedes",	TYPE_NUM, LC_MONETARY,	SRC_LCONV,
	  KW_N_CS_PRECEDES, "" },
	{ "n_sep_by_space",	TYPE_NUM, LC_MONETARY,	SRC_LCONV,
	  KW_N_SEP_BY_SPACE, "" },
	{ "p_sign_posn",	TYPE_NUM, LC_MONETARY,	SRC_LCONV,
	  KW_P_SIGN_POSN, "" },
	{ "n_sign_posn",	TYPE_NUM, LC_MONETARY,	SRC_LCONV,
	  KW_N_SIGN_POSN, "" },
	{ "int_p_cs_precedes",	TYPE_NUM, LC_MONETARY,	SRC_LCONV,
	  KW_INT_P_CS_PRECEDES, "" },
	{ "int_p_sep_by_space",	TYPE_NUM, LC_MONETARY,	SRC_LCONV,
	  KW_INT_P_SEP_BY_SPACE, "" },
	{ "int_n_cs_precedes",	TYPE_NUM, LC_MONETARY,	SRC_LCONV,
	  KW_INT_N_CS_PRECEDES, "" },
	{ "int_n_sep_by_space",	TYPE_NUM, LC_MONETARY,	SRC_LCONV,
	  KW_INT_N_SEP_BY_SPACE, "" },
	{ "int_p_sign_posn",	TYPE_NUM, LC_MONETARY,	SRC_LCONV,
	  KW_INT_P_SIGN_POSN, "" },
	{ "int_n_sign_posn",	TYPE_NUM, LC_MONETARY,	SRC_LCONV,
	  KW_INT_N_SIGN_POSN, "" },

	/* LC_NUMERIC - POSIX */
	{ "decimal_point",	TYPE_STR, LC_NUMERIC,	SRC_LINFO,
	  RADIXCHAR, "" },
	{ "thousands_sep",	TYPE_STR, LC_NUMERIC,	SRC_LINFO,
	  THOUSEP, "" },
	{ "grouping",		TYPE_UNQ, LC_NUMERIC,	SRC_LCONV,
	  KW_GROUPING, "" },
	/* LC_NUMERIC - local additions */
	{ "radixchar",		TYPE_STR, LC_NUMERIC,	SRC_LINFO,
	  RADIXCHAR, "Same as decimal_point (FreeBSD only)" },	/* compat */
	{ "thousep",		TYPE_STR, LC_NUMERIC,	SRC_LINFO,
	  THOUSEP, "Same as thousands_sep (FreeBSD only)" },	/* compat */

	/* LC_TIME - POSIX */
	{ "abday",		TYPE_STR, LC_TIME,	SRC_LTIME,
	  KW_TIME_ABDAY, "" },
	{ "day",		TYPE_STR, LC_TIME,	SRC_LTIME,
	  KW_TIME_DAY, "" },
	{ "abmon",		TYPE_STR, LC_TIME,	SRC_LTIME,
	  KW_TIME_ABMON, "" },
	{ "mon",		TYPE_STR, LC_TIME,	SRC_LTIME,
	  KW_TIME_MON, "" },
	{ "d_t_fmt",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  D_T_FMT, "" },
	{ "d_fmt",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  D_FMT, "" },
	{ "t_fmt",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  T_FMT, "" },
	{ "am_pm",		TYPE_STR, LC_TIME,	SRC_LTIME,
	  KW_TIME_AM_PM, "" },
	{ "t_fmt_ampm",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  T_FMT_AMPM, "" },
	{ "era",		TYPE_UNQ, LC_TIME,	SRC_LINFO,
	  ERA, "(unavailable)" },
	{ "era_d_fmt",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ERA_D_FMT, "(unavailable)" },
	{ "era_d_t_fmt",	TYPE_STR, LC_TIME,	SRC_LINFO,
	  ERA_D_T_FMT, "(unavailable)" },
	{ "era_t_fmt",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ERA_T_FMT, "(unavailable)" },
	{ "alt_digits",		TYPE_UNQ, LC_TIME,	SRC_LINFO,
	  ALT_DIGITS, "" },
	/* LC_TIME - local additions */
	{ "abday_1",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ABDAY_1, "(FreeBSD only)" },
	{ "abday_2",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ABDAY_2, "(FreeBSD only)" },
	{ "abday_3",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ABDAY_3, "(FreeBSD only)" },
	{ "abday_4",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ABDAY_4, "(FreeBSD only)" },
	{ "abday_5",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ABDAY_5, "(FreeBSD only)" },
	{ "abday_6",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ABDAY_6, "(FreeBSD only)" },
	{ "abday_7",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ABDAY_7, "(FreeBSD only)" },
	{ "day_1",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  DAY_1, "(FreeBSD only)" },
	{ "day_2",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  DAY_2, "(FreeBSD only)" },
	{ "day_3",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  DAY_3, "(FreeBSD only)" },
	{ "day_4",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  DAY_4, "(FreeBSD only)" },
	{ "day_5",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  DAY_5, "(FreeBSD only)" },
	{ "day_6",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  DAY_6, "(FreeBSD only)" },
	{ "day_7",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  DAY_7, "(FreeBSD only)" },
	{ "abmon_1",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ABMON_1, "(FreeBSD only)" },
	{ "abmon_2",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ABMON_2, "(FreeBSD only)" },
	{ "abmon_3",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ABMON_3, "(FreeBSD only)" },
	{ "abmon_4",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ABMON_4, "(FreeBSD only)" },
	{ "abmon_5",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ABMON_5, "(FreeBSD only)" },
	{ "abmon_6",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ABMON_6, "(FreeBSD only)" },
	{ "abmon_7",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ABMON_7, "(FreeBSD only)" },
	{ "abmon_8",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ABMON_8, "(FreeBSD only)" },
	{ "abmon_9",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ABMON_9, "(FreeBSD only)" },
	{ "abmon_10",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ABMON_10, "(FreeBSD only)" },
	{ "abmon_11",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ABMON_11, "(FreeBSD only)" },
	{ "abmon_12",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ABMON_12, "(FreeBSD only)" },
	{ "mon_1",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  MON_1, "(FreeBSD only)" },
	{ "mon_2",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  MON_2, "(FreeBSD only)" },
	{ "mon_3",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  MON_3, "(FreeBSD only)" },
	{ "mon_4",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  MON_4, "(FreeBSD only)" },
	{ "mon_5",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  MON_5, "(FreeBSD only)" },
	{ "mon_6",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  MON_6, "(FreeBSD only)" },
	{ "mon_7",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  MON_7, "(FreeBSD only)" },
	{ "mon_8",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  MON_8, "(FreeBSD only)" },
	{ "mon_9",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  MON_9, "(FreeBSD only)" },
	{ "mon_10",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  MON_10, "(FreeBSD only)" },
	{ "mon_11",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  MON_11, "(FreeBSD only)" },
	{ "mon_12",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  MON_12, "(FreeBSD only)" },
	{ "altmon_1",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ALTMON_1, "(FreeBSD only)" },
	{ "altmon_2",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ALTMON_2, "(FreeBSD only)" },
	{ "altmon_3",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ALTMON_3, "(FreeBSD only)" },
	{ "altmon_4",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ALTMON_4, "(FreeBSD only)" },
	{ "altmon_5",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ALTMON_5, "(FreeBSD only)" },
	{ "altmon_6",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ALTMON_6, "(FreeBSD only)" },
	{ "altmon_7",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ALTMON_7, "(FreeBSD only)" },
	{ "altmon_8",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ALTMON_8, "(FreeBSD only)" },
	{ "altmon_9",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ALTMON_9, "(FreeBSD only)" },
	{ "altmon_10",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ALTMON_10, "(FreeBSD only)" },
	{ "altmon_11",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ALTMON_11, "(FreeBSD only)" },
	{ "altmon_12",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  ALTMON_12, "(FreeBSD only)" },
	{ "am_str",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  AM_STR, "(FreeBSD only)" },
	{ "pm_str",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  PM_STR, "(FreeBSD only)" },
	{ "d_md_order",		TYPE_STR, LC_TIME,	SRC_LINFO,
	  D_MD_ORDER, "(FreeBSD only)" },			/* local */

	/* LC_MESSAGES - POSIX */
	{ "yesexpr",		TYPE_STR, LC_MESSAGES, SRC_LINFO,
	  YESEXPR, "" },
	{ "noexpr",		TYPE_STR, LC_MESSAGES, SRC_LINFO,
	  NOEXPR, "" },
	/* LC_MESSAGES - local additions */
	{ "yesstr",		TYPE_STR, LC_MESSAGES, SRC_LINFO,
	  YESSTR, "(POSIX legacy)" },				/* compat */
	{ "nostr",		TYPE_STR, LC_MESSAGES, SRC_LINFO,
	  NOSTR, "(POSIX legacy)" }				/* compat */

};
#define	NKWINFO (nitems(kwinfo))

static const char *boguslocales[] = { "UTF-8" };
#define	NBOGUS	(nitems(boguslocales))

int
main(int argc, char *argv[])
{
	int	ch;
	int	tmp;

	while ((ch = getopt(argc, argv, "ackms:")) != -1) {
		switch (ch) {
		case 'a':
			all_locales = 1;
			break;
		case 'c':
			prt_categories = 1;
			break;
		case 'k':
			prt_keywords = 1;
			break;
		case 'm':
			all_charmaps = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* validate arguments */
	if (all_locales && all_charmaps)
		usage();
	if ((all_locales || all_charmaps) && argc > 0)
		usage();
	if ((all_locales || all_charmaps) && (prt_categories || prt_keywords))
		usage();

	/* process '-a' */
	if (all_locales) {
		list_locales();
		exit(0);
	}

	/* process '-m' */
	if (all_charmaps) {
		list_charmaps();
		exit(0);
	}

	/* check for special case '-k list' */
	tmp = 0;
	if (prt_keywords && argc > 0)
		while (tmp < argc)
			if (strcasecmp(argv[tmp++], "list") == 0) {
				showkeywordslist(argv[tmp]);
				exit(0);
			}

	/* process '-c', '-k', or command line arguments. */
	if (prt_categories || prt_keywords || argc > 0) {
		if (prt_keywords || argc > 0)
			setlocale(LC_ALL, "");
		if (argc > 0) {
			while (argc > 0) {
				if (showdetails(*argv) != 0)
					exit(EXIT_FAILURE);
				argv++;
				argc--;
			}
		} else {
			uint i;
			for (i = 0; i < nitems(kwinfo); i++)
				showdetails(kwinfo[i].name);
		}
		exit(0);
	}

	/* no arguments, show current locale state */
	showlocale();

	return (0);
}

void
usage(void)
{
	printf("Usage: locale [ -a | -m ]\n"
	       "       locale -k list [prefix]\n"
	       "       locale [ -ck ] [keyword ...]\n");
	exit(1);
}

/*
 * Output information about all available locales
 *
 * XXX actually output of this function does not guarantee that locale
 *     is really available to application, since it can be broken or
 *     inconsistent thus setlocale() will fail.  Maybe add '-V' function to
 *     also validate these locales?
 */
void
list_locales(void)
{
	size_t i;

	init_locales_list();
	for (i = 0; i < locales->sl_cur; i++) {
		printf("%s\n", locales->sl_str[i]);
	}
}

/*
 * qsort() helper function
 */
static int
scmp(const void *s1, const void *s2)
{
	return strcmp(*(const char * const *)s1, *(const char * const *)s2);
}

/*
 * Output information about all available charmaps
 *
 * XXX this function is doing a task in hackish way, i.e. by scaning
 *     list of locales, spliting their codeset part and building list of
 *     them.
 */
void
list_charmaps(void)
{
	size_t i;
	char *s, *cs;
	StringList *charmaps;

	/* initialize StringList */
	charmaps = sl_init();
	if (charmaps == NULL)
		err(1, "could not allocate memory");

	/* fetch locales list */
	init_locales_list();

	/* split codesets and build their list */
	for (i = 0; i < locales->sl_cur; i++) {
		s = locales->sl_str[i];
		if ((cs = strchr(s, '.')) != NULL) {
			cs++;
			if (sl_find(charmaps, cs) == NULL)
				sl_add(charmaps, cs);
		}
	}

	/* add US-ASCII, if not yet added */
	if (sl_find(charmaps, "US-ASCII") == NULL)
		sl_add(charmaps, strdup("US-ASCII"));

	/* sort the list */
	qsort(charmaps->sl_str, charmaps->sl_cur, sizeof(char *), scmp);

	/* print results */
	for (i = 0; i < charmaps->sl_cur; i++) {
		printf("%s\n", charmaps->sl_str[i]);
	}
}

/*
 * Retrieve sorted list of system locales (or user locales, if PATH_LOCALE
 * environment variable is set)
 */
void
init_locales_list(void)
{
	DIR *dirp;
	struct dirent *dp;
	size_t i;
	int bogus;

	/* why call this function twice ? */
	if (locales != NULL)
		return;

	/* initialize StringList */
	locales = sl_init();
	if (locales == NULL)
		err(1, "could not allocate memory");

	/* get actual locales directory name */
	if (__detect_path_locale() != 0)
		err(1, "unable to find locales storage");

	/* open locales directory */
	dirp = opendir(_PathLocale);
	if (dirp == NULL)
		err(1, "could not open directory '%s'", _PathLocale);

	/* scan directory and store its contents except "." and ".." */
	while ((dp = readdir(dirp)) != NULL) {
		if (*(dp->d_name) == '.')
			continue;		/* exclude "." and ".." */
		for (bogus = i = 0; i < NBOGUS; i++)
			if (strncmp(dp->d_name, boguslocales[i],
			    strlen(boguslocales[i])) == 0)
				bogus = 1;
		if (!bogus)
			sl_add(locales, strdup(dp->d_name));
	}
	closedir(dirp);

	/* make sure that 'POSIX' and 'C' locales are present in the list.
	 * POSIX 1003.1-2001 requires presence of 'POSIX' name only here, but
	 * we also list 'C' for constistency
	 */
	if (sl_find(locales, "POSIX") == NULL)
		sl_add(locales, strdup("POSIX"));

	if (sl_find(locales, "C") == NULL)
		sl_add(locales, strdup("C"));

	/* make output nicer, sort the list */
	qsort(locales->sl_str, locales->sl_cur, sizeof(char *), scmp);
}

/*
 * Show current locale status, depending on environment variables
 */
void
showlocale(void)
{
	size_t	i;
	const char *lang, *vval, *eval;

	setlocale(LC_ALL, "");

	lang = getenv("LANG");
	if (lang == NULL) {
		lang = "";
	}
	printf("LANG=%s\n", lang);
	/* XXX: if LANG is null, then set it to "C" to get implied values? */

	for (i = 0; i < NLCINFO; i++) {
		vval = setlocale(lcinfo[i].id, NULL);
		eval = getenv(lcinfo[i].name);
		if (eval != NULL && !strcmp(eval, vval)
				&& strcmp(lang, vval)) {
			/*
			 * Appropriate environment variable set, its value
			 * is valid and not overridden by LC_ALL
			 *
			 * XXX: possible side effect: if both LANG and
			 * overridden environment variable are set into same
			 * value, then it'll be assumed as 'implied'
			 */
			printf("%s=%s\n", lcinfo[i].name, vval);
		} else {
			printf("%s=\"%s\"\n", lcinfo[i].name, vval);
		}
	}

	vval = getenv("LC_ALL");
	if (vval == NULL) {
		vval = "";
	}
	printf("LC_ALL=%s\n", vval);
}

char *
format_grouping(char *binary)
{
	static char rval[64];
	const char *cp;
	size_t roff;
	int len;

	/*
	 * XXX This check will need to be modified if/when localeconv() is
	 * fixed (PR172215).
	 */
	if (*binary == CHAR_MAX)
		return (binary);

	rval[0] = '\0';
	roff = 0;
	for (cp = binary; *cp != '\0'; ++cp) {
#if CHAR_MIN != 0
		if (*cp < 0)
			break;		/* garbage input */
#endif
		len = snprintf(&rval[roff], sizeof(rval) - roff, "%u;", *cp);
		if (len < 0 || (unsigned)len >= sizeof(rval) - roff)
			break;		/* insufficient space for output */
		roff += len;
		if (*cp == CHAR_MAX)
			break;		/* special termination */
	}

	/* Truncate at the last successfully snprintf()ed semicolon. */
	if (roff != 0)
		rval[roff - 1] = '\0';

	return (&rval[0]);
}

/*
 * keyword value lookup helper for values accessible via localeconv()
 */
char *
kwval_lconv(int id)
{
	struct lconv *lc;
	char *rval;

	rval = NULL;
	lc = localeconv();
	switch (id) {
		case KW_GROUPING:
			rval = format_grouping(lc->grouping);
			break;
		case KW_INT_CURR_SYMBOL:
			rval = lc->int_curr_symbol;
			break;
		case KW_CURRENCY_SYMBOL:
			rval = lc->currency_symbol;
			break;
		case KW_MON_DECIMAL_POINT:
			rval = lc->mon_decimal_point;
			break;
		case KW_MON_THOUSANDS_SEP:
			rval = lc->mon_thousands_sep;
			break;
		case KW_MON_GROUPING:
			rval = format_grouping(lc->mon_grouping);
			break;
		case KW_POSITIVE_SIGN:
			rval = lc->positive_sign;
			break;
		case KW_NEGATIVE_SIGN:
			rval = lc->negative_sign;
			break;
		case KW_INT_FRAC_DIGITS:
			rval = &(lc->int_frac_digits);
			break;
		case KW_FRAC_DIGITS:
			rval = &(lc->frac_digits);
			break;
		case KW_P_CS_PRECEDES:
			rval = &(lc->p_cs_precedes);
			break;
		case KW_P_SEP_BY_SPACE:
			rval = &(lc->p_sep_by_space);
			break;
		case KW_N_CS_PRECEDES:
			rval = &(lc->n_cs_precedes);
			break;
		case KW_N_SEP_BY_SPACE:
			rval = &(lc->n_sep_by_space);
			break;
		case KW_P_SIGN_POSN:
			rval = &(lc->p_sign_posn);
			break;
		case KW_N_SIGN_POSN:
			rval = &(lc->n_sign_posn);
			break;
		case KW_INT_P_CS_PRECEDES:
			rval = &(lc->int_p_cs_precedes);
			break;
		case KW_INT_P_SEP_BY_SPACE:
			rval = &(lc->int_p_sep_by_space);
			break;
		case KW_INT_N_CS_PRECEDES:
			rval = &(lc->int_n_cs_precedes);
			break;
		case KW_INT_N_SEP_BY_SPACE:
			rval = &(lc->int_n_sep_by_space);
			break;
		case KW_INT_P_SIGN_POSN:
			rval = &(lc->int_p_sign_posn);
			break;
		case KW_INT_N_SIGN_POSN:
			rval = &(lc->int_n_sign_posn);
			break;
		default:
			break;
	}
	return (rval);
}

/*
 * keyword value lookup helper for LC_TIME keywords not accessible
 * via nl_langinfo() or localeconv()
 */
static char *
kwval_ltime(int id)
{
	char *rval;
	struct sbuf *kwsbuf;
	nl_item i, s_item = 0, e_item = 0;

	switch (id) {
	case KW_TIME_DAY:
		s_item = DAY_1;
		e_item = DAY_7;
		break;
	case KW_TIME_ABDAY:
		s_item = ABDAY_1;
		e_item = ABDAY_7;
		break;
	case KW_TIME_MON:
		s_item = MON_1;
		e_item = MON_12;
		break;
	case KW_TIME_ABMON:
		s_item = ABMON_1;
		e_item = ABMON_12;
		break;
	case KW_TIME_AM_PM:
		if (asprintf(&rval, "%s;%s",
		    nl_langinfo(AM_STR),
		    nl_langinfo(PM_STR)) == -1)
			err(1, "asprintf");
		return (rval);
	}

	kwsbuf = sbuf_new_auto();
	if (kwsbuf == NULL)
		err(1, "sbuf");
	for (i = s_item; i <= e_item; i++) {
		(void) sbuf_cat(kwsbuf, nl_langinfo(i));
		if (i != e_item)
			(void) sbuf_cat(kwsbuf, ";");
	}
	(void) sbuf_finish(kwsbuf);
	rval = strdup(sbuf_data(kwsbuf));
	if (rval == NULL)
		err(1, "strdup");
	sbuf_delete(kwsbuf);
	return (rval);
}

/*
 * keyword value and properties lookup
 */
int
kwval_lookup(const char *kwname, char **kwval, int *cat, int *type, int *alloc)
{
	int	rval;
	size_t	i;
	static char nastr[3] = "-1";

	rval = 0;
	*alloc = 0;
	for (i = 0; i < NKWINFO; i++) {
		if (strcasecmp(kwname, kwinfo[i].name) == 0) {
			rval = 1;
			*cat = kwinfo[i].catid;
			*type = kwinfo[i].type;
			switch (kwinfo[i].source) {
			case SRC_LINFO:
				*kwval = nl_langinfo(kwinfo[i].value_ref);
				break;
			case SRC_LCONV:
				*kwval = kwval_lconv(kwinfo[i].value_ref);
				/*
				 * XXX This check will need to be modified
				 * if/when localeconv() is fixed (PR172215).
				 */
				if (**kwval == CHAR_MAX) {
					if (*type == TYPE_NUM)
						*type = TYPE_UNQ;
					*kwval = nastr;
				}
				break;
			case SRC_LTIME:
				*kwval = kwval_ltime(kwinfo[i].value_ref);
				*alloc = 1;
				break;
			}
			break;
		}
	}

	return (rval);
}

/*
 * Show details about requested keyword according to '-k' and/or '-c'
 * command line options specified.
 */
int
showdetails(const char *kw)
{
	int	type, cat, tmpval, alloc;
	char	*kwval;

	if (kwval_lookup(kw, &kwval, &cat, &type, &alloc) == 0) {
		/* Invalid keyword specified */
		fprintf(stderr, "Unknown keyword: `%s'\n", kw);
		return (1);
	}

	if (prt_categories) {
		if (prt_keywords)
			printf("%-20s ", lookup_localecat(cat));
		else
			printf("%-20s\t%s\n", kw, lookup_localecat(cat));
	}

	if (prt_keywords) {
		switch (type) {
		case TYPE_NUM:
			tmpval = (char)*kwval;
			printf("%s=%d\n", kw, tmpval);
			break;
		case TYPE_STR:
			printf("%s=\"%s\"\n", kw, kwval);
			break;
		case TYPE_UNQ:
			printf("%s=%s\n", kw, kwval);
			break;
		}
	}

	if (!prt_categories && !prt_keywords) {
		switch (type) {
		case TYPE_NUM:
			tmpval = (char)*kwval;
			printf("%d\n", tmpval);
			break;
		case TYPE_STR:
		case TYPE_UNQ:
			printf("%s\n", kwval);
			break;
		}
	}

	if (alloc)
		free(kwval);

	return (0);
}

/*
 * Convert locale category id into string
 */
const char *
lookup_localecat(int cat)
{
	size_t	i;

	for (i = 0; i < NLCINFO; i++)
		if (lcinfo[i].id == cat) {
			return (lcinfo[i].name);
		}
	return ("UNKNOWN");
}

/*
 * Show list of keywords
 */
void
showkeywordslist(char *substring)
{
	size_t	i;

#define	FMT "%-20s %-12s %-7s %-20s\n"

	if (substring == NULL)
		printf("List of available keywords\n\n");
	else
		printf("List of available keywords starting with '%s'\n\n",
		    substring);
	printf(FMT, "Keyword", "Category", "Type", "Comment");
	printf("-------------------- ------------ ------- --------------------\n");
	for (i = 0; i < NKWINFO; i++) {
		if (substring != NULL) {
			if (strncmp(kwinfo[i].name, substring,
			    strlen(substring)) != 0)
				continue;
		}
		printf(FMT,
			kwinfo[i].name,
			lookup_localecat(kwinfo[i].catid),
			(kwinfo[i].type == TYPE_NUM) ? "number" : "string",
			kwinfo[i].comment);
	}
}
