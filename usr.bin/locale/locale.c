/*-
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
 *
 * $FreeBSD$
 */

/*
 * XXX: implement missing int_* (LC_MONETARY) and era_* (LC_CTYIME) keywords
 *      (require libc modification)
 *
 * XXX: correctly handle reserved 'charmap' keyword and '-m' option (require
 *      localedef(1) implementation).  Currently it's handled via
 *	nl_langinfo(CODESET).
 *
 * XXX: implement '-k list' to show all available keywords.  Add descriptions
 *      for all of keywords (and mention FreeBSD only there)
 *
 */

#include <sys/types.h>
#include <dirent.h>
#include <err.h>
#include <locale.h>
#include <langinfo.h>
#include <rune.h>		/* for _PATH_LOCALE */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stringlist.h>
#include <unistd.h>

/* Local prototypes */
void	usage(void);
void	init_locales_list(void);
void	list_locales(void);
void	showdetails(char *);
void	showlocale(void);

/* Global variables */
static StringList *locales = NULL;

int	all_locales = 0;
int	all_charmaps = 0;
int	prt_categories = 0;
int	prt_keywords = 0;
int	more_params = 0;

struct _lcinfo {
	char	*name;
	int	id;
} lcinfo [] = {
	{ "LC_CTYPE",		LC_CTYPE }, 
	{ "LC_COLLATE",		LC_COLLATE }, 
	{ "LC_TIME",		LC_TIME },
	{ "LC_NUMERIC",		LC_NUMERIC },
	{ "LC_MONETARY",	LC_MONETARY },
	{ "LC_MESSAGES",	LC_MESSAGES }
};
#define NLCINFO (sizeof(lcinfo)/sizeof(lcinfo[0]))

/* ids for values not referenced by nl_langinfo() */
#define	KW_ZERO			10000
#define	KW_GROUPING		(KW_ZERO+1)
#define KW_INT_CURR_SYMBOL 	(KW_ZERO+2)
#define KW_CURRENCY_SYMBOL 	(KW_ZERO+3)
#define KW_MON_DECIMAL_POINT 	(KW_ZERO+4)
#define KW_MON_THOUSANDS_SEP 	(KW_ZERO+5)
#define KW_MON_GROUPING 	(KW_ZERO+6)
#define KW_POSITIVE_SIGN 	(KW_ZERO+7)
#define KW_NEGATIVE_SIGN 	(KW_ZERO+8)
#define KW_INT_FRAC_DIGITS 	(KW_ZERO+9)
#define KW_FRAC_DIGITS 		(KW_ZERO+10)
#define KW_P_CS_PRECEDES 	(KW_ZERO+11)
#define KW_P_SEP_BY_SPACE 	(KW_ZERO+12)
#define KW_N_CS_PRECEDES 	(KW_ZERO+13)
#define KW_N_SEP_BY_SPACE 	(KW_ZERO+14)
#define KW_P_SIGN_POSN 		(KW_ZERO+15)
#define KW_N_SIGN_POSN 		(KW_ZERO+16)

struct _kwinfo {
	char	*name;
	int	isstr;		/* true - string, false - number */
	int	catid;		/* LC_* */
	int	value_ref;
} kwinfo [] = {
	{ "charmap",		1, LC_CTYPE,	CODESET },	/* hack */

	{ "decimal_point",	1, LC_NUMERIC,	RADIXCHAR },
	{ "thousands_sep",	1, LC_NUMERIC,	THOUSEP },
	{ "grouping",		1, LC_NUMERIC,	KW_GROUPING },
	{ "radixchar",		1, LC_NUMERIC,	RADIXCHAR },	/* compat */
	{ "thousep",		1, LC_NUMERIC,	THOUSEP},	/* compat */

	{ "currency_symbol",	1, LC_MONETARY, KW_CURRENCY_SYMBOL }, /*compat*/
	{ "int_curr_symbol",	1, LC_MONETARY,	KW_INT_CURR_SYMBOL },
	{ "currency_symbol",	1, LC_MONETARY,	KW_CURRENCY_SYMBOL },
	{ "mon_decimal_point",	1, LC_MONETARY,	KW_MON_DECIMAL_POINT },
	{ "mon_thousands_sep",	1, LC_MONETARY,	KW_MON_THOUSANDS_SEP },
	{ "mon_grouping",	1, LC_MONETARY,	KW_MON_GROUPING },
	{ "positive_sign",	1, LC_MONETARY,	KW_POSITIVE_SIGN },
	{ "negative_sign",	1, LC_MONETARY,	KW_NEGATIVE_SIGN },

	{ "int_frac_digits",	0, LC_MONETARY,	KW_INT_FRAC_DIGITS },
	{ "frac_digits",	0, LC_MONETARY,	KW_FRAC_DIGITS },
	{ "p_cs_precedes",	0, LC_MONETARY,	KW_P_CS_PRECEDES },
	{ "p_sep_by_space",	0, LC_MONETARY,	KW_P_SEP_BY_SPACE },
	{ "n_cs_precedes",	0, LC_MONETARY,	KW_N_CS_PRECEDES },
	{ "n_sep_by_space",	0, LC_MONETARY,	KW_N_SEP_BY_SPACE },
	{ "p_sign_posn",	0, LC_MONETARY,	KW_P_SIGN_POSN },
	{ "n_sign_posn",	0, LC_MONETARY,	KW_N_SIGN_POSN },

	{ "d_t_fmt",		1, LC_TIME,	D_T_FMT },
	{ "d_fmt",		1, LC_TIME,	D_FMT },
	{ "t_fmt",		1, LC_TIME,	T_FMT },
	{ "am_str",		1, LC_TIME,	AM_STR },
	{ "pm_str",		1, LC_TIME,	PM_STR },
	{ "t_fmt_ampm",		1, LC_TIME,	T_FMT_AMPM },
	{ "day_1",		1, LC_TIME,	DAY_1 },
	{ "day_2",		1, LC_TIME,	DAY_2 },
	{ "day_3",		1, LC_TIME,	DAY_3 },
	{ "day_4",		1, LC_TIME,	DAY_4 },
	{ "day_5",		1, LC_TIME,	DAY_5 },
	{ "day_6",		1, LC_TIME,	DAY_6 },
	{ "day_7",		1, LC_TIME,	DAY_7 },
	{ "abday_1",		1, LC_TIME,	ABDAY_1 },
	{ "abday_2",		1, LC_TIME,	ABDAY_2 },
	{ "abday_3",		1, LC_TIME,	ABDAY_3 },
	{ "abday_4",		1, LC_TIME,	ABDAY_4 },
	{ "abday_5",		1, LC_TIME,	ABDAY_5 },
	{ "abday_6",		1, LC_TIME,	ABDAY_6 },
	{ "abday_7",		1, LC_TIME,	ABDAY_7 },
	{ "mon_1",		1, LC_TIME,	MON_1 },
	{ "mon_2",		1, LC_TIME,	MON_2 },
	{ "mon_3",		1, LC_TIME,	MON_3 },
	{ "mon_4",		1, LC_TIME,	MON_4 },
	{ "mon_5",		1, LC_TIME,	MON_5 },
	{ "mon_6",		1, LC_TIME,	MON_6 },
	{ "mon_7",		1, LC_TIME,	MON_7 },
	{ "mon_8",		1, LC_TIME,	MON_8 },
	{ "mon_9",		1, LC_TIME,	MON_9 },
	{ "mon_10",		1, LC_TIME,	MON_10 },
	{ "mon_11",		1, LC_TIME,	MON_11 },
	{ "mon_12",		1, LC_TIME,	MON_12 },
	{ "abmon_1",		1, LC_TIME,	ABMON_1 },
	{ "abmon_2",		1, LC_TIME,	ABMON_2 },
	{ "abmon_3",		1, LC_TIME,	ABMON_3 },
	{ "abmon_4",		1, LC_TIME,	ABMON_4 },
	{ "abmon_5",		1, LC_TIME,	ABMON_5 },
	{ "abmon_6",		1, LC_TIME,	ABMON_6 },
	{ "abmon_7",		1, LC_TIME,	ABMON_7 },
	{ "abmon_8",		1, LC_TIME,	ABMON_8 },
	{ "abmon_9",		1, LC_TIME,	ABMON_9 },
	{ "abmon_10",		1, LC_TIME,	ABMON_10 },
	{ "abmon_11",		1, LC_TIME,	ABMON_11 },
	{ "abmon_12",		1, LC_TIME,	ABMON_12 },
	{ "era",		1, LC_TIME,	ERA },
	{ "era_d_fmt",		1, LC_TIME,	ERA_D_FMT },
	{ "era_d_t_fmt",	1, LC_TIME,	ERA_D_T_FMT },
	{ "era_t_fmt",		1, LC_TIME,	ERA_T_FMT },
	{ "alt_digits",		1, LC_TIME,	ALT_DIGITS },
	{ "d_md_order",		1, LC_TIME,	D_MD_ORDER },	/* local */

	{ "yesexpr",		1, LC_MESSAGES, YESEXPR },
	{ "noexpr",		1, LC_MESSAGES, NOEXPR },
	{ "yesstr",		1, LC_MESSAGES, YESSTR },	/* local */
	{ "nostr",		1, LC_MESSAGES, NOSTR }		/* local */

};
#define NKWINFO (sizeof(kwinfo)/sizeof(kwinfo[0]))

int
main(int argc, char *argv[])
{
	char	ch;

	while ((ch = getopt(argc, argv, "ackm")) != -1) {
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
	if ((prt_categories || prt_keywords) && argc <= 0)
		usage();

	/* process '-a' */
	if (all_locales) {
		list_locales();
		exit(0);
	}

	/* process '-m' */
	if (all_charmaps) {
		/*
		 * XXX: charmaps are not supported by FreeBSD now.  It
		 * need to be implemented as soon as localedef(1) implemented.
		 */
		exit(1);
	}

	/* process '-c' and/or '-k' */
	if (prt_categories || prt_keywords || argc > 0) {
		setlocale(LC_ALL, "");
		while (argc > 0) {
			showdetails(*argv);
			argv++;
			argc--;
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
               "       locale [ -ck ] name ...\n");
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
	int i;

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
	return strcmp(*(const char **)s1, *(const char **)s2);
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
	char *dirname;

	/* why call this function twice ? */
	if (locales != NULL)
		return;

	/* initialize StringList */
	locales = sl_init();
	if (locales == NULL)
		err(1, "could not allocate memory");

	/* get actual locales directory name */
	dirname = getenv("PATH_LOCALE");
	if (dirname == NULL)
		dirname = _PATH_LOCALE;

	/* open locales directory */
	dirp = opendir(dirname);
	if (dirp == NULL)
		err(1, "could not open directory '%s'", dirname);

	/* scan directory and store its contents except "." and ".." */
	while ((dp = readdir(dirp)) != NULL) {
		if (*(dp->d_name) == '.')
			continue;		/* exclude "." and ".." */
		sl_add(locales, strdup(dp->d_name));
	}
	closedir(dirp);

        /* make sure that 'POSIX' and 'C' locales are present in the list.
	 * POSIX 1003.1-2001 requires presence of 'POSIX' name only here, but
         * we also list 'C' for constistency
         */
	if (sl_find(locales, "POSIX") == NULL)
		sl_add(locales, "POSIX");

	if (sl_find(locales, "C") == NULL)
		sl_add(locales, "C");

	/* make output nicer, sort the list */
	qsort(locales->sl_str, locales->sl_cur, sizeof(char *), scmp);
}

/*
 * Show current locale status, depending on environment variables
 */
void
showlocale(void)
{
	int	i;
	char	*lang, *vval, *eval;

	setlocale(LC_ALL, "");

	lang = getenv("LANG");
	if (lang == NULL || *lang == '\0') {
		lang = "";
	}
	printf("LANG=%s\n", lang);

	for (i = 0; i < NLCINFO; i++) {
		vval = setlocale(lcinfo[i].id, NULL);
		eval = getenv(lcinfo[i].name);
		if (eval != NULL && !strcmp(eval, vval)
				&& strcmp(lang, vval)) {
			/*
			 * Appropriate environment variable set, its value
			 * is valid and not overriden by LC_ALL
			 *
			 * XXX: possible side effect: if both LANG and
			 * overriden environment variable are set into same
			 * value, then it'll be assumed as 'implied'
			 */
			printf("%s=%s\n", lcinfo[i].name, vval);
		} else {
			printf("%s=\"%s\"\n", lcinfo[i].name, vval);
		}
	}

	vval = getenv("LC_ALL");
	if (vval == NULL || *vval == '\0') {
		vval = "";
	}
	printf("LC_ALL=%s\n", vval);
}

/*
 * keyword value lookup helper (via localeconv())
 */
char *
kwval_lconv(int id)
{
	struct lconv *lc = localeconv();
	char *rval = NULL;

	switch (id) {
		case KW_GROUPING:
			rval = lc->grouping;
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
			rval = lc->mon_grouping;
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
		default:
			break;
	}
	return (rval);
}

/*
 * keyword value and properties lookup
 */
int
kwval_lookup(char *kwname, char **kwval, int *cat, int *isstr)
{
	int rval = 0;
	int i;

	for (i = 0; i < NKWINFO; i++) {
		if (strcasecmp(kwname, kwinfo[i].name) == 0) {
			rval = 1;
			*cat = kwinfo[i].catid;
			*isstr = kwinfo[i].isstr;
			if (kwinfo[i].value_ref < KW_ZERO) {
				*kwval = nl_langinfo(kwinfo[i].value_ref);
			} else {
				*kwval = kwval_lconv(kwinfo[i].value_ref);
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
void
showdetails(char *kw)
{
	char	*kwval, *tmps;
	int	isstr, cat, tmp;

	if (kwval_lookup(kw, &kwval, &cat, &isstr) == 0) {
		/*
		 * invalid keyword specified.
		 * XXX: any actions?
		 */
		return;
	}

	if (prt_categories) {
		tmps = NULL;
		for (tmp = 0; tmp < NLCINFO; tmp++) {
			if (lcinfo[tmp].id == cat)
				tmps = lcinfo[tmp].name;
		}
		if (tmps == NULL)
			tmps = "UNKNOWN";
		printf("%s\n", tmps);
	}

	if (prt_keywords) {
		if (isstr) {
			printf("%s=\"%s\"\n", kw, kwval);
		} else {
			tmp = (char) *kwval;
			printf("%s=%d\n", kw, tmp);
		}
	}

	if (!prt_categories && !prt_keywords) {
		if (isstr) {
			printf("%s\n", kwval);
		} else {
			tmp = (char) *kwval;
			printf("%d\n", tmp);
		}
	}
}
