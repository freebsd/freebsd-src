/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1985, 1987, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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

#ifndef lint
static char const copyright[] =
"@(#) Copyright (c) 1985, 1987, 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#if 0
#ifndef lint
static char sccsid[] = "@(#)date.c	8.2 (Berkeley) 4/28/95";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <utmpx.h>

#include "vary.h"

#ifndef	TM_YEAR_BASE
#define	TM_YEAR_BASE	1900
#endif

static void badformat(void);
static void iso8601_usage(const char *) __dead2;
static void multipleformats(void);
static void printdate(const char *);
static void printisodate(struct tm *, long);
static void setthetime(const char *, const char *, int, struct timespec *);
static size_t strftime_ns(char * __restrict, size_t, const char * __restrict,
    const struct tm * __restrict, long);
static void usage(void) __dead2;

static const struct iso8601_fmt {
	const char *refname;
	const char *format_string;
} iso8601_fmts[] = {
	{ "date", "%Y-%m-%d" },
	{ "hours", "T%H" },
	{ "minutes", ":%M" },
	{ "seconds", ":%S" },
	{ "ns", ",%N" },
};
static const struct iso8601_fmt *iso8601_selected;

static const char *rfc2822_format = "%a, %d %b %Y %T %z";

int
main(int argc, char *argv[])
{
	struct timespec ts;
	int ch, rflag;
	bool Iflag, jflag, Rflag;
	const char *format;
	char buf[1024];
	char *fmt, *outzone = NULL;
	char *tmp;
	struct vary *v;
	const struct vary *badv;
	struct tm *lt;
	struct stat sb;
	size_t i;

	v = NULL;
	fmt = NULL;
	(void) setlocale(LC_TIME, "");
	rflag = 0;
	Iflag = jflag = Rflag = 0;
	ts.tv_sec = 0;
	ts.tv_nsec = 0;
	while ((ch = getopt(argc, argv, "f:I::jnRr:uv:z:")) != -1)
		switch((char)ch) {
		case 'f':
			fmt = optarg;
			break;
		case 'I':
			if (Rflag)
				multipleformats();
			Iflag = 1;
			if (optarg == NULL) {
				iso8601_selected = iso8601_fmts;
				break;
			}
			for (i = 0; i < nitems(iso8601_fmts); i++)
				if (strcmp(optarg, iso8601_fmts[i].refname) == 0)
					break;
			if (i == nitems(iso8601_fmts))
				iso8601_usage(optarg);

			iso8601_selected = &iso8601_fmts[i];
			break;
		case 'j':
			jflag = 1;	/* don't set time */
			break;
		case 'n':
			break;
		case 'R':		/* RFC 2822 datetime format */
			if (Iflag)
				multipleformats();
			Rflag = 1;
			break;
		case 'r':		/* user specified seconds */
			rflag = 1;
			ts.tv_sec = strtoq(optarg, &tmp, 0);
			if (*tmp != 0) {
				if (stat(optarg, &sb) == 0) {
					ts.tv_sec = sb.st_mtim.tv_sec;
					ts.tv_nsec = sb.st_mtim.tv_nsec;
				} else
					usage();
			}
			break;
		case 'u':		/* do everything in UTC */
			(void)setenv("TZ", "UTC0", 1);
			break;
		case 'z':
			outzone = optarg;
			break;
		case 'v':
			v = vary_append(v, optarg);
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!rflag && clock_gettime(CLOCK_REALTIME, &ts) == -1)
		err(1, "clock_gettime");

	format = "%+";

	if (Rflag)
		format = rfc2822_format;

	/* allow the operands in any order */
	if (*argv && **argv == '+') {
		if (Iflag)
			multipleformats();
		format = *argv + 1;
		++argv;
	}

	if (*argv) {
		setthetime(fmt, *argv, jflag, &ts);
		++argv;
	} else if (fmt != NULL)
		usage();

	if (*argv && **argv == '+') {
		if (Iflag)
			multipleformats();
		format = *argv + 1;
	}

	if (outzone != NULL && setenv("TZ", outzone, 1) != 0)
		err(1, "setenv(TZ)");
	lt = localtime(&ts.tv_sec);
	if (lt == NULL)
		errx(1, "invalid time");
	badv = vary_apply(v, lt);
	if (badv) {
		fprintf(stderr, "%s: Cannot apply date adjustment\n",
			badv->arg);
		vary_destroy(v);
		usage();
	}
	vary_destroy(v);

	if (Iflag)
		printisodate(lt, ts.tv_nsec);

	if (format == rfc2822_format)
		/*
		 * When using RFC 2822 datetime format, don't honor the
		 * locale.
		 */
		setlocale(LC_TIME, "C");


	(void)strftime_ns(buf, sizeof(buf), format, lt, ts.tv_nsec);
	printdate(buf);
}

static void
printdate(const char *buf)
{
	(void)printf("%s\n", buf);
	if (fflush(stdout))
		err(1, "stdout");
	exit(EXIT_SUCCESS);
}

static void
printisodate(struct tm *lt, long nsec)
{
	const struct iso8601_fmt *it;
	char fmtbuf[64], buf[64], tzbuf[8];

	fmtbuf[0] = 0;
	for (it = iso8601_fmts; it <= iso8601_selected; it++)
		strlcat(fmtbuf, it->format_string, sizeof(fmtbuf));

	(void)strftime_ns(buf, sizeof(buf), fmtbuf, lt, nsec);

	if (iso8601_selected > iso8601_fmts) {
		(void)strftime_ns(tzbuf, sizeof(tzbuf), "%z", lt, nsec);
		memmove(&tzbuf[4], &tzbuf[3], 3);
		tzbuf[3] = ':';
		strlcat(buf, tzbuf, sizeof(buf));
	}

	printdate(buf);
}

#define	ATOI2(s)	((s) += 2, ((s)[-2] - '0') * 10 + ((s)[-1] - '0'))

static void
setthetime(const char *fmt, const char *p, int jflag, struct timespec *ts)
{
	struct utmpx utx;
	struct tm *lt;
	const char *dot, *t;
	int century;

	lt = localtime(&ts->tv_sec);
	if (lt == NULL)
		errx(1, "invalid time");
	lt->tm_isdst = -1;		/* divine correct DST */

	if (fmt != NULL) {
		t = strptime(p, fmt, lt);
		if (t == NULL) {
			fprintf(stderr, "Failed conversion of ``%s''"
				" using format ``%s''\n", p, fmt);
			badformat();
		} else if (*t != '\0')
			fprintf(stderr, "Warning: Ignoring %ld extraneous"
				" characters in date string (%s)\n",
				(long) strlen(t), t);
	} else {
		for (t = p, dot = NULL; *t; ++t) {
			if (isdigit(*t))
				continue;
			if (*t == '.' && dot == NULL) {
				dot = t;
				continue;
			}
			badformat();
		}

		if (dot != NULL) {			/* .ss */
			dot++; /* *dot++ = '\0'; */
			if (strlen(dot) != 2)
				badformat();
			lt->tm_sec = ATOI2(dot);
			if (lt->tm_sec > 61)
				badformat();
		} else
			lt->tm_sec = 0;

		century = 0;
		/* if p has a ".ss" field then let's pretend it's not there */
		switch (strlen(p) - ((dot != NULL) ? 3 : 0)) {
		case 12:				/* cc */
			lt->tm_year = ATOI2(p) * 100 - TM_YEAR_BASE;
			century = 1;
			/* FALLTHROUGH */
		case 10:				/* yy */
			if (century)
				lt->tm_year += ATOI2(p);
			else {
				lt->tm_year = ATOI2(p);
				if (lt->tm_year < 69)	/* hack for 2000 ;-} */
					lt->tm_year += 2000 - TM_YEAR_BASE;
				else
					lt->tm_year += 1900 - TM_YEAR_BASE;
			}
			/* FALLTHROUGH */
		case 8:					/* mm */
			lt->tm_mon = ATOI2(p);
			if (lt->tm_mon > 12)
				badformat();
			--lt->tm_mon;		/* time struct is 0 - 11 */
			/* FALLTHROUGH */
		case 6:					/* dd */
			lt->tm_mday = ATOI2(p);
			if (lt->tm_mday > 31)
				badformat();
			/* FALLTHROUGH */
		case 4:					/* HH */
			lt->tm_hour = ATOI2(p);
			if (lt->tm_hour > 23)
				badformat();
			/* FALLTHROUGH */
		case 2:					/* MM */
			lt->tm_min = ATOI2(p);
			if (lt->tm_min > 59)
				badformat();
			break;
		default:
			badformat();
		}
	}

	/* convert broken-down time to GMT clock time */
	lt->tm_yday = -1;
	ts->tv_sec = mktime(lt);
	if (lt->tm_yday == -1)
		errx(1, "nonexistent time");
	ts->tv_nsec = 0;

	if (!jflag) {
		utx.ut_type = OLD_TIME;
		memset(utx.ut_id, 0, sizeof(utx.ut_id));
		(void)gettimeofday(&utx.ut_tv, NULL);
		pututxline(&utx);
		if (clock_settime(CLOCK_REALTIME, ts) != 0)
			err(1, "clock_settime");
		utx.ut_type = NEW_TIME;
		(void)gettimeofday(&utx.ut_tv, NULL);
		pututxline(&utx);

		if ((p = getlogin()) == NULL)
			p = "???";
		syslog(LOG_AUTH | LOG_NOTICE, "date set by %s", p);
	}
}

/*
 * The strftime_ns function is a wrapper around strftime(3), which adds support
 * for features absent from strftime(3). Currently, the only extra feature is
 * support for %N, the nanosecond conversion specification.
 *
 * The functions scans the format string for the non-standard conversion
 * specifications and replaces them with the date and time values before
 * passing the format string to strftime(3). The handling of the non-standard
 * conversion specifications happens before the call to strftime(3) to handle
 * cases like "%%N" correctly ("%%N" should yield "%N" instead of nanoseconds).
 */
static size_t
strftime_ns(char * __restrict s, size_t maxsize, const char * __restrict format,
    const struct tm * __restrict t, long nsec)
{
	size_t prefixlen;
	size_t ret;
	char *newformat;
	char *oldformat;
	const char *prefix;
	const char *suffix;
	const char *tok;
	bool seen_percent;

	seen_percent = false;
	if ((newformat = strdup(format)) == NULL)
		err(1, "strdup");
	tok = newformat;
	for (tok = newformat; *tok != '\0'; tok++) {
		switch (*tok) {
		case '%':
			/*
			 * If the previous token was a percent sign,
			 * then there are two percent tokens in a row.
			 */
			if (seen_percent)
				seen_percent = false;
			else
				seen_percent = true;
			break;
		case 'N':
			if (seen_percent) {
				oldformat = newformat;
				prefix = oldformat;
				prefixlen = tok - oldformat - 1;
				suffix = tok + 1;
				/*
				 * Construct a new format string from the
				 * prefix (i.e., the part of the old format
				 * from its beginning to the currently handled
				 * "%N" conversion specification), the
				 * nanoseconds, and the suffix (i.e., the part
				 * of the old format from the next token to the
				 * end).
				 */
				if (asprintf(&newformat, "%.*s%.9ld%s",
				    (int)prefixlen, prefix, nsec,
				    suffix) < 0) {
					err(1, "asprintf");
				}
				free(oldformat);
				tok = newformat + prefixlen + 9;
			}
			seen_percent = false;
			break;
		default:
			seen_percent = false;
			break;
		}
	}

	ret = strftime(s, maxsize, newformat, t);
	free(newformat);
	return (ret);
}

static void
badformat(void)
{
	warnx("illegal time format");
	usage();
}

static void
iso8601_usage(const char *badarg)
{
	errx(1, "invalid argument '%s' for -I", badarg);
}

static void
multipleformats(void)
{
	errx(1, "multiple output formats specified");
}

static void
usage(void)
{
	(void)fprintf(stderr, "%s\n%s\n%s\n",
	    "usage: date [-jnRu] [-I[date|hours|minutes|seconds|ns]] [-f input_fmt]",
	    "            "
	    "[ -z output_zone ] [-r filename|seconds] [-v[+|-]val[y|m|w|d|H|M|S]]",
	    "            "
	    "[[[[[[cc]yy]mm]dd]HH]MM[.SS] | new_date] [+output_fmt]"
	    );
	exit(1);
}
