/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kim Letkeman.
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)cal.c	8.4 (Berkeley) 4/2/94";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define	THURSDAY		4		/* for reformation */
#define	SATURDAY 		6		/* 1 Jan 1 was a Saturday */

#define	FIRST_MISSING_DAY 	639787		/* 3 Sep 1752 */
#define	NUMBER_MISSING_DAYS 	11		/* 11 day correction */

#define	MAXDAYS			42		/* max slots in a month array */
#define	SPACE			-1		/* used in day array */

static int days_in_month[2][13] = {
	{0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
	{0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
};

int sep1752[MAXDAYS] = {
	SPACE,	SPACE,	1,	2,	14,	15,	16,
	17,	18,	19,	20,	21,	22,	23,
	24,	25,	26,	27,	28,	29,	30,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
}, j_sep1752[MAXDAYS] = {
	SPACE,	SPACE,	245,	246,	258,	259,	260,
	261,	262,	263,	264,	265,	266,	267,
	268,	269,	270,	271,	272,	273,	274,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
}, empty[MAXDAYS] = {
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,	SPACE,
};

char *month_names[12];

char day_headings[] = "                    ";
char j_day_headings[] = "                           ";

/* leap year -- account for gregorian reformation in 1752 */
#define	leap_year(yr) \
	((yr) <= 1752 ? !((yr) % 4) : \
	!((yr) % 4) && ((yr) % 100) || !((yr) % 400))

/* number of centuries since 1700, not inclusive */
#define	centuries_since_1700(yr) \
	((yr) > 1700 ? (yr) / 100 - 17 : 0)

/* number of centuries since 1700 whose modulo of 400 is 0 */
#define	quad_centuries_since_1700(yr) \
	((yr) > 1600 ? ((yr) - 1600) / 400 : 0)

/* number of leap years between year 1 and this year, not inclusive */
#define	leap_years_since_year_1(yr) \
	((yr) / 4 - centuries_since_1700(yr) + quad_centuries_since_1700(yr))

int julian;

void	ascii_day __P((char *, int));
void	center __P((char *, int, int));
void	day_array __P((int, int, int *));
int	day_in_week __P((int, int, int));
int	day_in_year __P((int, int, int));
void	j_yearly __P((int));
void	monthly __P((int, int));
void	trim_trailing_spaces __P((char *));
void	usage __P((void));
void	yearly __P((int));

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct tm *local_time;
	static struct tm zero_tm;
	time_t now;
	int ch, month, year, yflag, i;
	char buf[40];

	(void) setlocale(LC_TIME, "");

	yflag = 0;
	while ((ch = getopt(argc, argv, "jy")) !=  -1)
		switch(ch) {
		case 'j':
			julian = 1;
			break;
		case 'y':
			yflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	month = 0;
	switch(argc) {
	case 2:
		if ((month = atoi(*argv++)) < 1 || month > 12)
			errx(1, "illegal month value: use 1-12");
		/* FALLTHROUGH */
	case 1:
		if ((year = atoi(*argv)) < 1 || year > 9999)
			errx(1, "illegal year value: use 1-9999");
		break;
	case 0:
		(void)time(&now);
		local_time = localtime(&now);
		year = local_time->tm_year + 1900;
		if (!yflag)
			month = local_time->tm_mon + 1;
		break;
	default:
		usage();
	}

	for (i = 0; i < 12; i++) {
		zero_tm.tm_mon = i;
		strftime(buf, sizeof(buf), "%B", &zero_tm);
		month_names[i] = strdup(buf);
	}
	for (i = 0; i < 7; i++) {
		zero_tm.tm_wday = i;
		strftime(buf, sizeof(buf), "%a", &zero_tm);
		strncpy(day_headings + i * 3, buf, 2);
		strncpy(j_day_headings + i * 4 + 1, buf, 2);
	}

	if (month)
		monthly(month, year);
	else if (julian)
		j_yearly(year);
	else
		yearly(year);
	exit(0);
}

#define	DAY_LEN		3		/* 3 spaces per day */
#define	J_DAY_LEN	4		/* 4 spaces per day */
#define	WEEK_LEN	20		/* 7 * 3 - one space at the end */
#define	J_WEEK_LEN	27		/* 7 * 4 - one space at the end */
#define	HEAD_SEP	2		/* spaces between day headings */
#define	J_HEAD_SEP	2

void
monthly(month, year)
	int month, year;
{
	int col, row, len, days[MAXDAYS];
	char *p, lineout[30];

	day_array(month, year, days);
	len = sprintf(lineout, "%s %d", month_names[month - 1], year);
	(void)printf("%*s%s\n%s\n",
	    ((julian ? J_WEEK_LEN : WEEK_LEN) - len) / 2, "",
	    lineout, julian ? j_day_headings : day_headings);
	for (row = 0; row < 6; row++) {
		for (col = 0, p = lineout; col < 7; col++,
		    p += julian ? J_DAY_LEN : DAY_LEN)
			ascii_day(p, days[row * 7 + col]);
		*p = '\0';
		trim_trailing_spaces(lineout);
		(void)printf("%s\n", lineout);
	}
}

void
j_yearly(year)
	int year;
{
	int col, *dp, i, month, row, which_cal;
	int days[12][MAXDAYS];
	char *p, lineout[80];

	(void)sprintf(lineout, "%d", year);
	center(lineout, J_WEEK_LEN * 2 + J_HEAD_SEP, 0);
	(void)printf("\n\n");
	for (i = 0; i < 12; i++)
		day_array(i + 1, year, days[i]);
	(void)memset(lineout, ' ', sizeof(lineout) - 1);
	lineout[sizeof(lineout) - 1] = '\0';
	for (month = 0; month < 12; month += 2) {
		center(month_names[month], J_WEEK_LEN, J_HEAD_SEP);
		center(month_names[month + 1], J_WEEK_LEN, 0);
		(void)printf("\n%s%*s%s\n", j_day_headings, J_HEAD_SEP, "",
		    j_day_headings);
		for (row = 0; row < 6; row++) {
			for (which_cal = 0; which_cal < 2; which_cal++) {
				p = lineout + which_cal * (J_WEEK_LEN + 2);
				dp = &days[month + which_cal][row * 7];
				for (col = 0; col < 7; col++, p += J_DAY_LEN)
					ascii_day(p, *dp++);
			}
			*p = '\0';
			trim_trailing_spaces(lineout);
			(void)printf("%s\n", lineout);
		}
	}
	(void)printf("\n");
}

void
yearly(year)
	int year;
{
	int col, *dp, i, month, row, which_cal;
	int days[12][MAXDAYS];
	char *p, lineout[80];

	(void)sprintf(lineout, "%d", year);
	center(lineout, WEEK_LEN * 3 + HEAD_SEP * 2, 0);
	(void)printf("\n\n");
	for (i = 0; i < 12; i++)
		day_array(i + 1, year, days[i]);
	(void)memset(lineout, ' ', sizeof(lineout) - 1);
	lineout[sizeof(lineout) - 1] = '\0';
	for (month = 0; month < 12; month += 3) {
		center(month_names[month], WEEK_LEN, HEAD_SEP);
		center(month_names[month + 1], WEEK_LEN, HEAD_SEP);
		center(month_names[month + 2], WEEK_LEN, 0);
		(void)printf("\n%s%*s%s%*s%s\n", day_headings, HEAD_SEP,
		    "", day_headings, HEAD_SEP, "", day_headings);
		for (row = 0; row < 6; row++) {
			for (which_cal = 0; which_cal < 3; which_cal++) {
				p = lineout + which_cal * (WEEK_LEN + 2);
				dp = &days[month + which_cal][row * 7];
				for (col = 0; col < 7; col++, p += DAY_LEN)
					ascii_day(p, *dp++);
			}
			*p = '\0';
			trim_trailing_spaces(lineout);
			(void)printf("%s\n", lineout);
		}
	}
	(void)printf("\n");
}

/*
 * day_array --
 *	Fill in an array of 42 integers with a calendar.  Assume for a moment
 *	that you took the (maximum) 6 rows in a calendar and stretched them
 *	out end to end.  You would have 42 numbers or spaces.  This routine
 *	builds that array for any month from Jan. 1 through Dec. 9999.
 */
void
day_array(month, year, days)
	int month, year;
	int *days;
{
	int day, dw, dm;

	if (month == 9 && year == 1752) {
		memmove(days,
			julian ? j_sep1752 : sep1752, MAXDAYS * sizeof(int));
		return;
	}
	memmove(days, empty, MAXDAYS * sizeof(int));
	dm = days_in_month[leap_year(year)][month];
	dw = day_in_week(1, month, year);
	day = julian ? day_in_year(1, month, year) : 1;
	while (dm--)
		days[dw++] = day++;
}

/*
 * day_in_year --
 *	return the 1 based day number within the year
 */
int
day_in_year(day, month, year)
	int day, month, year;
{
	int i, leap;

	leap = leap_year(year);
	for (i = 1; i < month; i++)
		day += days_in_month[leap][i];
	return (day);
}

/*
 * day_in_week
 *	return the 0 based day number for any date from 1 Jan. 1 to
 *	31 Dec. 9999.  Assumes the Gregorian reformation eliminates
 *	3 Sep. 1752 through 13 Sep. 1752.  Returns Thursday for all
 *	missing days.
 */
int
day_in_week(day, month, year)
	int day, month, year;
{
	long temp;

	temp = (long)(year - 1) * 365 + leap_years_since_year_1(year - 1)
	    + day_in_year(day, month, year);
	if (temp < FIRST_MISSING_DAY)
		return ((temp - 1 + SATURDAY) % 7);
	if (temp >= (FIRST_MISSING_DAY + NUMBER_MISSING_DAYS))
		return (((temp - 1 + SATURDAY) - NUMBER_MISSING_DAYS) % 7);
	return (THURSDAY);
}

void
ascii_day(p, day)
	char *p;
	int day;
{
	int display, val;
	static char *aday[] = {
		"",
		" 1", " 2", " 3", " 4", " 5", " 6", " 7",
		" 8", " 9", "10", "11", "12", "13", "14",
		"15", "16", "17", "18", "19", "20", "21",
		"22", "23", "24", "25", "26", "27", "28",
		"29", "30", "31",
	};

	if (day == SPACE) {
		memset(p, ' ', julian ? J_DAY_LEN : DAY_LEN);
		return;
	}
	if (julian) {
		if (val = day / 100) {
			day %= 100;
			*p++ = val + '0';
			display = 1;
		} else {
			*p++ = ' ';
			display = 0;
		}
		val = day / 10;
		if (val || display)
			*p++ = val + '0';
		else
			*p++ = ' ';
		*p++ = day % 10 + '0';
	} else {
		*p++ = aday[day][0];
		*p++ = aday[day][1];
	}
	*p = ' ';
}

void
trim_trailing_spaces(s)
	char *s;
{
	char *p;

	for (p = s; *p; ++p)
		continue;
	while (p > s && isspace(*--p))
		continue;
	if (p > s)
		++p;
	*p = '\0';
}

void
center(str, len, separate)
	char *str;
	int len;
	int separate;
{

	len -= strlen(str);
	(void)printf("%*s%s%*s", len / 2, "", str, len / 2 + len % 2, "");
	if (separate)
		(void)printf("%*s", separate, "");
}

void
usage()
{

	(void)fprintf(stderr, "usage: cal [-jy] [[month] year]\n");
	exit(1);
}
