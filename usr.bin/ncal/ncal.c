/*-
 * Copyright (c) 1997 Wolfgang Helbig
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

#ifndef lint
static const char rcsid[] =
	"$Id: ncal.c,v 1.5.2.1 1998/01/12 05:11:06 obrien Exp $";
#endif /* not lint */

#include <calendar.h>
#include <err.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

/* Width of one month with backward compatibility */
#define MONTH_WIDTH_B_J 27
#define MONTH_WIDTH_B 20

#define MONTH_WIDTH_J 24
#define MONTH_WIDTH 18

#define MAX_WIDTH 28

typedef struct date date;

struct monthlines {
	char name[MAX_WIDTH + 1];
	char lines[7][MAX_WIDTH + 1];
	char weeks[MAX_WIDTH + 1];
};

struct weekdays {
	char names[7][4];
};

/* The switches from Julian to Gregorian in some countries */
static struct djswitch {
	char *cc;	/* Country code according to ISO 3166 */
	char *nm;	/* Name of country */
	date dt;	/* Last day of Julian calendar */
	char *efmt;	/* strftime format for printing date of easter */
} switches[] = {
	{"AL", "Albania",	{1912, 11, 30}, "%e %B %Y"},
	{"AT", "Austria",	{1583, 10,  5}, "%e. %B %Y"},
	{"AU", "Australia",	{1752,  9,  2}, "%B %e, %Y"},
	{"BE", "Belgium",	{1582, 12, 14}, "%B %e, %Y"},
	{"BG", "Bulgaria",	{1916,  3, 18}, "%e %B %Y"},
	{"CA", "Canada",	{1752,  9,  2}, "%B %e, %Y"},
	{"CH", "Switzerland",	{1655,  2, 28}, "%e. %B %Y"},
	{"CN", "China",		{1911, 12, 18}, "%e %B %Y"},
	{"CZ", "Czech Republic",{1584,  1,  6}, "%e %B %Y"},
	{"DE", "Germany",	{1700,  2, 18}, "%e. %B %Y"},
	{"DK", "Denmark",	{1700,  2, 18}, "%e. %B %Y"},
	{"ES", "Spain",		{1582, 10,  4}, "%e de %B de %Y"},
	{"FI", "Finland",	{1753,  2, 17}, "%e %B %Y"},
	{"FR", "France",	{1582, 12,  9}, "%e. %B %Y"},
	{"GB", "United Kingdom",{1752,  9,  2}, "%e %B %Y"},
	{"GR", "Greece",	{1924,  3,  9}, "%e %B %Y"},
	{"HU", "Hungary",	{1587, 10, 21}, "%e %B %Y"},
	{"IS", "Iceland",	{1700, 11, 16}, "%e %B %Y"},
	{"IT", "Italy",		{1582, 10,  4}, "%e %B %Y"},
	{"JP", "Japan",		{1918, 12, 18}, "%Y\x94N %B%e"},
	{"LI", "Lithuania",	{1918,  2,  1}, "%e %B %Y"},
	{"LN", "Latin",		{9999, 31, 12}, "%e %B %Y"},
	{"LU", "Luxembourg",	{1582, 12, 14}, "%e %B %Y"},
	{"LV", "Latvia",	{1918,  2,  1}, "%e %B %Y"},
	{"NL", "Netherlands",	{1582, 12, 14}, "%e %B %Y"},
	{"NO", "Norway",	{1700,  2, 18}, "%e %B %Y"},
	{"PL", "Poland",	{1582, 10,  4}, "%e %B %Y"},
	{"PT", "Portugal",	{1582, 10,  4}, "%e %B %Y"},
	{"RO", "Romania",	{1919,  3, 31}, "%e %B %Y"},
	{"RU", "Russia",	{1918,  1, 31}, "%e %B %Y"},
	{"SI", "Slovenia",	{1919,  3,  4}, "%e %B %Y"},
	{"SU", "USSR",		{1920,  3,  4}, "%e %B %Y"},
	{"SW", "Sweden",	{1753,  2, 17}, "%e %B %Y"},
	{"TR", "Turkey",	{1926, 12, 18}, "%e %B %Y"},
	{"US", "United States",	{1752,  9,  2}, "%B %e, %Y"},
	{"YU", "Yugoslavia",	{1919,  3,  4}, "%e %B %Y"}
};

struct djswitch *dftswitch =
    switches + sizeof(switches) / sizeof(struct djswitch) - 2;
    /* default switch (should be "US") */

/* Table used to print day of month and week numbers */
char daystr[] = "     1  2  3  4  5  6  7  8  9 10 11 12 13 14 15"
		" 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31"
		" 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47"
		" 48 49 50 51 52 53";

/* Table used to print day of year and week numbers */
char jdaystr[] = "       1   2   3   4   5   6   7   8   9"
		 "  10  11  12  13  14  15  16  17  18  19"
		 "  20  21  22  23  24  25  26  27  28  29"
		 "  30  31  32  33  34  35  36  37  38  39"
		 "  40  41  42  43  44  45  46  47  48  49"
		 "  50  51  52  53  54  55  56  57  58  59"
		 "  60  61  62  63  64  65  66  67  68  69"
		 "  70  71  72  73  74  75  76  77  78  79"
		 "  80  81  82  83  84  85  86  87  88  89"
		 "  90  91  92  93  94  95  96  97  98  99"
		 " 100 101 102 103 104 105 106 107 108 109"
		 " 110 111 112 113 114 115 116 117 118 119"
		 " 120 121 122 123 124 125 126 127 128 129"
		 " 130 131 132 133 134 135 136 137 138 139"
		 " 140 141 142 143 144 145 146 147 148 149"
		 " 150 151 152 153 154 155 156 157 158 159"
		 " 160 161 162 163 164 165 166 167 168 169"
		 " 170 171 172 173 174 175 176 177 178 179"
		 " 180 181 182 183 184 185 186 187 188 189"
		 " 190 191 192 193 194 195 196 197 198 199"
		 " 200 201 202 203 204 205 206 207 208 209"
		 " 210 211 212 213 214 215 216 217 218 219"
		 " 220 221 222 223 224 225 226 227 228 229"
		 " 230 231 232 233 234 235 236 237 238 239"
		 " 240 241 242 243 244 245 246 247 248 249"
		 " 250 251 252 253 254 255 256 257 258 259"
		 " 260 261 262 263 264 265 266 267 268 269"
		 " 270 271 272 273 274 275 276 277 278 279"
		 " 280 281 282 283 284 285 286 287 288 289"
		 " 290 291 292 293 294 295 296 297 298 299"
		 " 300 301 302 303 304 305 306 307 308 309"
		 " 310 311 312 313 314 315 316 317 318 319"
		 " 320 321 322 323 324 325 326 327 328 329"
		 " 330 331 332 333 334 335 336 337 338 339"
		 " 340 341 342 343 344 345 346 347 348 349"
		 " 350 351 352 353 354 355 356 357 358 359"
		 " 360 361 362 363 364 365 366";

int     flag_weeks;		/* user wants number of week */
int     nswitch;		/* user defined switch date */
int	nswitchb;		/* switch date for backward compatibility */
char   *efmt;			/* strftime format string for printeaster() */

char   *center(char *s, char *t, int w);
void	mkmonth(int year, int month, int jd_flag, struct monthlines * monthl);
void    mkmonthb(int year, int month, int jd_flag, struct monthlines * monthl);
void    mkweekdays(struct weekdays * wds);
void    printcc(void);
void    printeaster(int year, int julian, int orthodox);
void    printmonth(int year, int month, int jd_flag);
void    printmonthb(int year, int month, int jd_flag);
void    printyear(int year, int jd_flag);
void    printyearb(int year, int jd_flag);
int	firstday(int y, int m);
date   *sdate(int ndays, struct date * d);
date   *sdateb(int ndays, struct date * d);
int     sndays(struct date * d);
int     sndaysb(struct date * d);
static void usage(void);
int     weekdayb(int nd);

int
main(int argc, char *argv[])
{
	struct  djswitch *p, *q;	/* to search user defined switch date */
	date	never = {10000, 1, 1};	/* outside valid range of dates */
	date	ukswitch = {1752, 9, 2};/* switch date for Great Britain */
	int     ch;			/* holds the option character */
	int     m = 0;			/* month */
	int	y = 0;			/* year */
	int     flag_backward = 0;	/* user called cal--backward compat. */
	int     flag_hole_year = 0;	/* user wants the whole year */
	int	flag_julian_cal = 0;	/* user wants Julian Calendar */
	int     flag_julian_day = 0;	/* user wants the Julian day
					 * numbers */
	int	flag_orthodox = 0;	/* use wants Orthodox easter */
	int	flag_easter = 0;	/* use wants easter date */
	char	*cp;			/* character pointer */
	char    *locale;		/* locale to get country code */

	/*
	 * Use locale to determine the country code,
	 * and use the country code to determine the default
	 * switchdate and date format from the switches table.
	 */
	if ((locale = setlocale(LC_TIME, "")) == NULL)
		warn("setlocale");
	if (locale == NULL || locale == "C")
		locale = "_US";
	q = switches + sizeof(switches) / sizeof(struct djswitch);
	for (p = switches; p != q; p++)
		if ((cp = strstr(locale, p->cc)) != NULL && *(cp - 1) == '_')
			break;
	if (p == q) {
		nswitch = ndaysj(&dftswitch->dt);
		efmt = dftswitch->efmt;
	} else {
		nswitch = ndaysj(&p->dt);
		efmt = p->efmt;
		dftswitch = p;
	}


	/*
	 * Get the filename portion of argv[0] and set flag_backward if
	 * this program is called "cal".
	 */
	for (cp = argv[0]; *cp; cp++)
		;
	while (cp >= argv[0] && *cp != '/')
		cp--;
	if (strcmp("cal", ++cp) == 0)
		flag_backward = 1;

	/* Set the switch date to United Kingdom if backwards compatible */
	if (flag_backward)
		nswitchb = ndaysj(&ukswitch);

	while ((ch = getopt(argc, argv, "Jejops:wy")) != -1)
		switch (ch) {
		case 'J':
			if (flag_backward)
				usage();
			nswitch = ndaysj(&never);
			flag_julian_cal = 1;
			break;
		case 'e':
			if (flag_backward)
				usage();
			flag_easter = 1;
			break;
		case 'j':
			flag_julian_day = 1;
			break;
		case 'o':
			if (flag_backward)
				usage();
			flag_orthodox = 1;
			flag_easter = 1;
			break;
		case 'p':
			if (flag_backward)
				usage();
			printcc();
			return (0);
			break;
		case 's':
			if (flag_backward)
				usage();
			q = switches +
			    sizeof(switches) / sizeof(struct djswitch);
			for (p = switches;
			     p != q && strcmp(p->cc, optarg) != 0; p++)
				;
			if (p == q)
				errx(EX_USAGE,
				    "%s: invalid country code", optarg);
			nswitch = ndaysj(&(p->dt));
			break;
		case 'w':
			if (flag_backward)
				usage();
			flag_weeks = 1;
			break;
		case 'y':
			flag_hole_year = 1;
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		time_t t;
		struct tm *tm;

		t = time(NULL);
		tm = localtime(&t);
		y = tm->tm_year + 1900;
		m = tm->tm_mon + 1;
	}

	switch (argc) {
	case 2:
		if (flag_easter)
			usage();
		m = atoi(*argv++);
		if (m < 1 || m > 12)
			errx(EX_USAGE, "month %d not in range 1..12", m);
		/* FALLTHROUGH */
	case 1:
		y = atoi(*argv++);
		if (y < 1 || y > 9999)
			errx(EX_USAGE, "year %d not in range 1..9999", y);
		break;
	case 0:
		break;
	default:
		usage();
	}

	if (flag_easter)
		printeaster(y, flag_julian_cal, flag_orthodox);
	else if (argc == 1 || flag_hole_year)
		if (flag_backward)
			printyearb(y, flag_julian_day);
		else
			printyear(y, flag_julian_day);
	else
		if (flag_backward)
			printmonthb(y, m, flag_julian_day);
		else
			printmonth(y, m, flag_julian_day);

	return (0);
}

static void
usage(void)
{

	fprintf(stderr, "%s\n%s\n%s\n",
	    "usage: cal [-jy] [month[year]]",
	    "       ncal [-Jjpwy] [-s country_code] [[month] year]",
	    "       ncal [-Jeo] [year]");
	exit(EX_USAGE);
}

/* print the assumed switches for all countries */
void
printcc(void)
{
	struct djswitch *p;
	int n;	/* number of lines to print */
	int m;	/* offset from left to right table entry on the same line */

#define FSTR "%c%s %-15s%4d-%02d-%02d"
#define DFLT(p) ((p) == dftswitch ? '*' : ' ')
#define FSTRARG(p) DFLT(p), (p)->cc, (p)->nm, (p)->dt.y, (p)->dt.m, (p)->dt.d

	n = sizeof(switches) / sizeof(struct djswitch);
	m = (n + 1) / 2;
	n /= 2;
	for (p = switches; p != switches + n; p++)
		printf(FSTR"     "FSTR"\n", FSTRARG(p), FSTRARG(p+m));
	if (m != n)
		printf(FSTR"\n", FSTRARG(p));
}

/* print the date of easter sunday */
void
printeaster(int y, int julian, int orthodox)
{
	date    dt;
	struct tm tm;
	char    buf[80];

	/* force orthodox easter for years before 1583 */
	if (y < 1583)
		orthodox = 1;

	if (orthodox)
		if (julian)
			easteroj(y, &dt);
		else
			easterog(y, &dt);
	else
		easterg(y, &dt);

	memset(&tm, 0, sizeof(tm));
	tm.tm_year = dt.y - 1900;
	tm.tm_mon  = dt.m - 1;
	tm.tm_mday = dt.d;
	strftime(buf, sizeof(buf), efmt,  &tm);
	printf("%s\n", buf);
}

void
printmonth(int y, int m, int jd_flag)
{
	struct monthlines month;
	struct weekdays wds;
	int i;

	mkmonth(y, m - 1, jd_flag, &month);
	mkweekdays(&wds);
	printf("    %s %d\n", month.name, y);
	for (i = 0; i != 7; i++)
		printf("%.2s%s\n", wds.names[i], month.lines[i]);
	if (flag_weeks)
		printf("  %s\n", month.weeks);
}

void
printmonthb(int y, int m, int jd_flag)
{
	struct monthlines month;
	struct weekdays wds;
	char s[MAX_WIDTH], t[MAX_WIDTH];
	int i;
	int mw;

	mkmonthb(y, m - 1, jd_flag, &month);
	mkweekdays(&wds);

	mw = jd_flag ? MONTH_WIDTH_B_J : MONTH_WIDTH_B;

	sprintf(s, "%s %d", month.name, y);
	printf("%s\n", center(t, s, mw));

	if (jd_flag)
		printf(" %s %s %s %s %s %s %.2s\n", wds.names[6], wds.names[0],
			wds.names[1], wds.names[2], wds.names[3],
			wds.names[4], wds.names[5]);
	else
		printf("%s%s%s%s%s%s%.2s\n", wds.names[6], wds.names[0],
			wds.names[1], wds.names[2], wds.names[3],
			wds.names[4], wds.names[5]);

	for (i = 0; i != 6; i++)
		printf("%s\n", month.lines[i]+1);
}

void
printyear(int y, int jd_flag)
{
	struct monthlines year[12];
	struct weekdays wds;
	char    s[80], t[80];
	int     i, j;
	int     mpl;
	int     mw;

	for (i = 0; i != 12; i++)
		mkmonth(y, i, jd_flag, year + i);
	mkweekdays(&wds);
	mpl = jd_flag ? 3 : 4;
	mw = jd_flag ? MONTH_WIDTH_J : MONTH_WIDTH;

	sprintf(s, "%d", y);
	printf("%s\n", center(t, s, mpl * mw));

	for (j = 0; j != 12; j += mpl) {
		printf("    %-*s%-*s",
		    mw, year[j].name,
		    mw, year[j + 1].name);
		if (mpl == 3)
			printf("%s\n", year[j + 2].name);
		else
			printf("%-*s%s\n",
		    	    mw, year[j + 2].name,
		    	    year[j + 3].name);
		for (i = 0; i != 7; i++) {
			printf("%.2s%-*s%-*s",
			    wds.names[i],
			    mw, year[j].lines[i],
			    mw, year[j + 1].lines[i]);
			if (mpl == 3)
				printf("%s\n", year[j + 2].lines[i]);
			else
				printf("%-*s%s\n",
			    	    mw, year[j + 2].lines[i],
			    	    year[j + 3].lines[i]);
		}
		if (flag_weeks)
			if (mpl == 3)
				printf("  %-*s%-*s%-s\n",
				    mw, year[j].weeks,
				    mw, year[j + 1].weeks,
				    year[j + 2].weeks);
			else
				printf("  %-*s%-*s%-*s%-s\n",
				    mw, year[j].weeks,
				    mw, year[j + 1].weeks,
				    mw, year[j + 2].weeks,
				    year[j + 3].weeks);
	}
}

void
printyearb(int y, int jd_flag)
{
	struct monthlines year[12];
	struct weekdays wds;
	char	s[80], t[80];
	int     i, j;
	int     mpl;
	int     mw;

	for (i = 0; i != 12; i++)
		mkmonthb(y, i, jd_flag, year + i);
	mkweekdays(&wds);
	mpl = jd_flag ? 2 : 3;
	mw = jd_flag ? MONTH_WIDTH_B_J : MONTH_WIDTH_B;

	sprintf(s, "%d", y);
	printf("%s\n\n", center(t, s, mw * mpl + mpl));

	for (j = 0; j != 12; j += mpl) {
		printf("%-*s  ", mw, center(s, year[j].name, mw));
		if (mpl == 2)
			printf("%s\n", center(s, year[j + 1].name, mw));
		else
			printf("%-*s  %s\n", mw,
			    center(s, year[j + 1].name, mw),
			    center(t, year[j + 2].name, mw));

		if (mpl == 2)
			printf(" %s %s %s %s %s %s %s "
			       " %s %s %s %s %s %s %.2s\n",
				wds.names[6], wds.names[0], wds.names[1],
				wds.names[2], wds.names[3], wds.names[4],
				wds.names[5],
				wds.names[6], wds.names[0], wds.names[1],
				wds.names[2], wds.names[3], wds.names[4],
				wds.names[5]);
		else
			printf("%s%s%s%s%s%s%s "
				"%s%s%s%s%s%s%s "
				"%s%s%s%s%s%s%.2s\n",
				wds.names[6], wds.names[0], wds.names[1],
				wds.names[2], wds.names[3], wds.names[4],
				wds.names[5],
				wds.names[6], wds.names[0], wds.names[1],
				wds.names[2], wds.names[3], wds.names[4],
				wds.names[5],
				wds.names[6], wds.names[0], wds.names[1],
				wds.names[2], wds.names[3], wds.names[4],
				wds.names[5]);
		for (i = 0; i != 6; i++) {
			if (mpl == 2)
				printf("%-*s  %s\n",
			    mw, year[j].lines[i]+1,
			    year[j + 1].lines[i]+1);
			else
				printf("%-*s  %-*s  %s\n",
			    mw, year[j].lines[i]+1,
			    mw, year[j + 1].lines[i]+1,
			    year[j + 2].lines[i]+1);

		}
	}
}

void
mkmonth(int y, int m, int jd_flag, struct monthlines *mlines)
{

	struct tm tm;		/* for strftime printing local names of
				 * months */
	date    dt;		/* handy date */
	int     dw;		/* width of numbers */
	int     first;		/* first day of month */
	int     firstm;		/* first day of first week of month */
	int     i, j, k;	/* just indices */
	int     last;		/* the first day of next month */
	int     jan1 = 0;	/* the first day of this year */
	char   *ds;		/* pointer to day strings (daystr or
				 * jdaystr) */

	/* Set name of month. */
	memset(&tm, 0, sizeof(tm));
	tm.tm_mon = m;
	strftime(mlines->name, sizeof(mlines->name), "%B", &tm);

	/*
	 * Set first and last to the day number of the first day of this
	 * month and the first day of next month respectively. Set jan1 to
	 * the day number of the first day of this year.
	 */
	first = firstday(y, m + 1);
	if (m == 11)
		last = firstday(y + 1, 1);
	else
		last = firstday(y, m + 2);

	if (jd_flag)
		jan1 = firstday(m, 1);

	/*
	 * Set firstm to the day number of monday of the first week of
	 * this month. (This might be in the last month)
	 */
	firstm = first - weekday(first);

	/* Set ds (daystring) and dw (daywidth) according to the jd_flag */
	if (jd_flag) {
		ds = jdaystr;
		dw = 4;
	} else {
		ds = daystr;
		dw = 3;
	}

	/*
	 * Fill the lines with day of month or day of year (julian day)
	 * line index: i, each line is one weekday. column index: j, each
	 * column is one day number. print column index: k.
	 */
	for (i = 0; i != 7; i++) {
		for (j = firstm + i, k = 0; j < last; j += 7, k += dw)
			if (j >= first) {
				if (jd_flag)
					dt.d = j - jan1 + 1;
				else
					sdate(j, &dt);
				memcpy(mlines->lines[i] + k,
				       ds + dt.d * dw, dw);
			} else
				memcpy(mlines->lines[i] + k, "    ", dw);
		mlines->lines[i][k] = '\0';
				
	}

	/* fill the weeknumbers */
	if (flag_weeks) {
		for (j = firstm, k = 0; j < last;  k += dw, j += 7)
			if (j <= nswitch)
				memset(mlines->weeks + k, ' ', dw);
			else
				memcpy(mlines->weeks + k,
				    ds + week(j, &i)*dw, dw);
		mlines->weeks[k] = '\0';
	}
}

void
mkmonthb(int y, int m, int jd_flag, struct monthlines *mlines)
{

	struct tm tm;		/* for strftime printing local names of
				 * months */
	date    dt;		/* handy date */
	int     dw;		/* width of numbers */
	int     first;		/* first day of month */
	int     firsts;		/* sunday of first week of month */
	int     i, j, k;	/* just indices */
	int     jan1 = 0;	/* the first day of this year */
	int     last;		/* the first day of next month */
	char   *ds;		/* pointer to day strings (daystr or
				 * jdaystr) */

	/* Set ds (daystring) and dw (daywidth) according to the jd_flag */
	if (jd_flag) {
		ds = jdaystr;
		dw = 4;
	} else {
		ds = daystr;
		dw = 3;
	}

	/* Set name of month centered */
	memset(&tm, 0, sizeof(tm));
	tm.tm_mon = m;
	strftime(mlines->name, sizeof(mlines->name), "%B", &tm);

	/*
	 * Set first and last to the day number of the first day of this
	 * month and the first day of next month respectively. Set jan1 to
	 * the day number of Jan 1st of this year.
	 */
	dt.y = y;
	dt.m = m + 1;
	dt.d = 1;
	first = sndaysb(&dt);
	if (m == 11) {
		dt.y = y + 1;
		dt.m = 1;
		dt.d = 1;
	} else {
		dt.y = y;
		dt.m = m + 2;
		dt.d = 1;
	}
	last = sndaysb(&dt);

	if (jd_flag) {
		dt.y = y;
		dt.m = 1;
		dt.d = 1;
		jan1 = sndaysb(&dt);
	}

	/*
	 * Set firsts to the day number of sunday of the first week of
	 * this month. (This might be in the last month)
	 */
	firsts = first - (weekday(first)+1) % 7;

	/*
	 * Fill the lines with day of month or day of year (Julian day)
	 * line index: i, each line is one week. column index: j, each
	 * column is one day number. print column index: k.
	 */
	for (i = 0; i != 6; i++) {
		for (j = firsts + 7 * i, k = 0; j < last && k != dw * 7;
		     j++, k += dw)
			if (j >= first) {
				if (jd_flag)
					dt.d = j - jan1 + 1;
				else
					sdateb(j, &dt);
				memcpy(mlines->lines[i] + k,
				       ds + dt.d * dw, dw);
			} else
				memcpy(mlines->lines[i] + k, "    ", dw);
		if (k == 0)
			mlines->lines[i][1] = '\0';
		else
			mlines->lines[i][k] = '\0';
	}
}

/* Put the local names of weekdays into the wds */
void
mkweekdays(struct weekdays *wds)
{
	int i;
	struct tm tm;

	memset(&tm, 0, sizeof(tm));

	for (i = 0; i != 7; i++) {
		tm.tm_wday = (i+1) % 7;
		strftime(wds->names[i], 4, "%a", &tm);
		wds->names[i][2] = ' '; 
	}
}

/*
 * Compute the day number of the first day in month.
 * The day y-m-1 might be dropped due to Gregorian Reformation,
 * so the answer is the smallest day number nd with sdate(nd) in
 * the month m.
 */
int
firstday(int y, int m)
{
	date dt;
	int nd;

	dt.y = y;
	dt.m = m;
	dt.d = 1;
	for (nd = sndays(&dt); sdate(nd, &dt)->m != m; nd++)
		;
	return (nd);
}

/*
 * Compute the number of days from date, obey the local switch from
 * Julian to Gregorian if specified by the user.
 */
int
sndays(struct date *d)
{

	if (nswitch != 0)
		if (nswitch < ndaysj(d))
			return (ndaysg(d));
		else
			return (ndaysj(d));
	else
		return ndaysg(d);
}

/*
 * Compute the number of days from date, obey the switch from
 * Julian to Gregorian as used by UK and her colonies.
 */
int
sndaysb(struct date *d)
{

	if (nswitchb < ndaysj(d))
		return (ndaysg(d));
	else
		return (ndaysj(d));
}

/* Inverse of sndays */
struct date *
sdate(int nd, struct date *d)
{

	if (nswitch < nd)
		return (gdate(nd, d));
	else
		return (jdate(nd, d));
}

/* Inverse of sndaysb */
struct date *
sdateb(int nd, struct date *d)
{

	if (nswitchb < nd)
		return (gdate(nd, d));
	else
		return (jdate(nd, d));
}

/* Center string t in string s of length w by putting enough leading blanks */
char *
center(char *s, char *t, int w)
{
	char blanks[80];

	memset(blanks, ' ', sizeof(blanks));
	sprintf(s, "%.*s%s", (int)(w - strlen(t)) / 2, blanks, t);
	return (s);
}
