/* 
 *  parsetime.c - parse time for at(1)
 *  Copyright (C) 1993, 1994  Thomas Koenig
 *
 *  modifications for english-language times
 *  Copyright (C) 1993  David Parsons
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author(s) may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  at [NOW] PLUS NUMBER MINUTES|HOURS|DAYS|WEEKS
 *     /NUMBER [DOT NUMBER] [AM|PM]\ /[MONTH NUMBER [NUMBER]]             \
 *     |NOON                       | |[TOMORROW]                          |
 *     |MIDNIGHT                   | |[DAY OF WEEK]                       |
 *     \TEATIME                    / |NUMBER [SLASH NUMBER [SLASH NUMBER]]|
 *                                   \PLUS NUMBER MINUTES|HOURS|DAYS|WEEKS/
 */

/* System Headers */


#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#ifndef __FreeBSD__
#include <getopt.h>
#endif

/* Local headers */

#include "at.h"
#include "panic.h"


/* Structures and unions */

enum {	/* symbols */
    MIDNIGHT, NOON, TEATIME,
    PM, AM, TOMORROW, TODAY, NOW,
    MINUTES, HOURS, DAYS, WEEKS,
    NUMBER, PLUS, DOT, SLASH, ID, JUNK,
    JAN, FEB, MAR, APR, MAY, JUN,
    JUL, AUG, SEP, OCT, NOV, DEC,
    SUN, MON, TUE, WED, THU, FRI, SAT
    };

/* parse translation table - table driven parsers can be your FRIEND!
 */
struct {
    char *name;	/* token name */
    int value;	/* token id */
    int plural;	/* is this plural? */
} Specials[] = {
    { "midnight", MIDNIGHT,0 },	/* 00:00:00 of today or tomorrow */
    { "noon", NOON,0 },		/* 12:00:00 of today or tomorrow */
    { "teatime", TEATIME,0 },	/* 16:00:00 of today or tomorrow */
    { "am", AM,0 },		/* morning times for 0-12 clock */
    { "pm", PM,0 },		/* evening times for 0-12 clock */
    { "tomorrow", TOMORROW,0 },	/* execute 24 hours from time */
    { "today", TODAY, 0 },	/* execute today - don't advance time */
    { "now", NOW,0 },		/* opt prefix for PLUS */

    { "minute", MINUTES,0 },	/* minutes multiplier */
    { "minutes", MINUTES,1 },	/* (pluralized) */
    { "hour", HOURS,0 },	/* hours ... */
    { "hours", HOURS,1 },	/* (pluralized) */
    { "day", DAYS,0 },		/* days ... */
    { "days", DAYS,1 },		/* (pluralized) */
    { "week", WEEKS,0 },	/* week ... */
    { "weeks", WEEKS,1 },	/* (pluralized) */
    { "jan", JAN,0 },
    { "feb", FEB,0 },
    { "mar", MAR,0 },
    { "apr", APR,0 },
    { "may", MAY,0 },
    { "jun", JUN,0 },
    { "jul", JUL,0 },
    { "aug", AUG,0 },
    { "sep", SEP,0 },
    { "oct", OCT,0 },
    { "nov", NOV,0 },
    { "dec", DEC,0 },
    { "january", JAN,0 },
    { "february", FEB,0 },
    { "march", MAR,0 },
    { "april", APR,0 },
    { "may", MAY,0 },
    { "june", JUN,0 },
    { "july", JUL,0 },
    { "august", AUG,0 },
    { "september", SEP,0 },
    { "october", OCT,0 },
    { "november", NOV,0 },
    { "december", DEC,0 },
    { "sunday", SUN, 0 },
    { "sun", SUN, 0 },
    { "monday", MON, 0 },
    { "mon", MON, 0 },
    { "tuesday", TUE, 0 },
    { "tue", TUE, 0 },
    { "wednesday", WED, 0 },
    { "wed", WED, 0 },
    { "thursday", THU, 0 },
    { "thu", THU, 0 },
    { "friday", FRI, 0 },
    { "fri", FRI, 0 },
    { "saturday", SAT, 0 },
    { "sat", SAT, 0 },
} ;

/* File scope variables */

static char **scp;	/* scanner - pointer at arglist */
static char scc;	/* scanner - count of remaining arguments */
static char *sct;	/* scanner - next char pointer in current argument */
static int need;	/* scanner - need to advance to next argument */

static char *sc_token;	/* scanner - token buffer */
static size_t sc_len;   /* scanner - lenght of token buffer */
static int sc_tokid;	/* scanner - token id */
static int sc_tokplur;	/* scanner - is token plural? */

static char rcsid[] = "$Id: parsetime.c,v 1.12 1998/05/05 01:53:15 alex Exp $";

/* Local functions */

/*
 * parse a token, checking if it's something special to us
 */
static int
parse_token(char *arg)
{
    int i;

    for (i=0; i<(sizeof Specials/sizeof Specials[0]); i++)
	if (strcasecmp(Specials[i].name, arg) == 0) {
	    sc_tokplur = Specials[i].plural;
	    return sc_tokid = Specials[i].value;
	}

    /* not special - must be some random id */
    return ID;
} /* parse_token */


/*
 * init_scanner() sets up the scanner to eat arguments
 */
static void
init_scanner(int argc, char **argv)
{
    scp = argv;
    scc = argc;
    need = 1;
    sc_len = 1;
    while (argc-- > 0)
	sc_len += strlen(*argv++);

    sc_token = (char *) mymalloc(sc_len);
} /* init_scanner */

/*
 * token() fetches a token from the input stream
 */
static int
token()
{
    int idx;

    while (1) {
	memset(sc_token, 0, sc_len);
	sc_tokid = EOF;
	sc_tokplur = 0;
	idx = 0;

	/* if we need to read another argument, walk along the argument list;
	 * when we fall off the arglist, we'll just return EOF forever
	 */
	if (need) {
	    if (scc < 1)
		return sc_tokid;
	    sct = *scp;
	    scp++;
	    scc--;
	    need = 0;
	}
	/* eat whitespace now - if we walk off the end of the argument,
	 * we'll continue, which puts us up at the top of the while loop
	 * to fetch the next argument in
	 */
	while (isspace(*sct))
	    ++sct;
	if (!*sct) {
	    need = 1;
	    continue;
	}

	/* preserve the first character of the new token
	 */
	sc_token[0] = *sct++;

	/* then see what it is
	 */
	if (isdigit(sc_token[0])) {
	    while (isdigit(*sct))
		sc_token[++idx] = *sct++;
	    sc_token[++idx] = 0;
	    return sc_tokid = NUMBER;
	}
	else if (isalpha(sc_token[0])) {
	    while (isalpha(*sct))
		sc_token[++idx] = *sct++;
	    sc_token[++idx] = 0;
	    return parse_token(sc_token);
	}
	else if (sc_token[0] == ':' || sc_token[0] == '.')
	    return sc_tokid = DOT;
	else if (sc_token[0] == '+')
	    return sc_tokid = PLUS;
	else if (sc_token[0] == '/')
	    return sc_tokid = SLASH;
	else
	    return sc_tokid = JUNK;
    } /* while (1) */
} /* token */


/*
 * plonk() gives an appropriate error message if a token is incorrect
 */
static void
plonk(int tok)
{
    panic((tok == EOF) ? "incomplete time"
		       : "garbled time");
} /* plonk */


/* 
 * expect() gets a token and dies most horribly if it's not the token we want
 */
static void
expect(int desired)
{
    if (token() != desired)
	plonk(sc_tokid);	/* and we die here... */
} /* expect */


/*
 * dateadd() adds a number of minutes to a date.  It is extraordinarily
 * stupid regarding day-of-month overflow, and will most likely not
 * work properly
 */
static void
dateadd(int minutes, struct tm *tm)
{
    /* increment days */

    while (minutes > 24*60) {
	minutes -= 24*60;
	tm->tm_mday++;
    }

    /* increment hours */
    while (minutes > 60) {
	minutes -= 60;
	tm->tm_hour++;
	if (tm->tm_hour > 23) {
	    tm->tm_mday++;
	    tm->tm_hour = 0;
	}
    }

    /* increment minutes */
    tm->tm_min += minutes;

    if (tm->tm_min > 59) {
	tm->tm_hour++;
	tm->tm_min -= 60;

	if (tm->tm_hour > 23) {
	    tm->tm_mday++;
	    tm->tm_hour = 0;
	}
    }
} /* dateadd */


/*
 * plus() parses a now + time
 *
 *  at [NOW] PLUS NUMBER [MINUTES|HOURS|DAYS|WEEKS]
 *
 */
static void
plus(struct tm *tm)
{
    int delay;
    int expectplur;

    expect(NUMBER);

    delay = atoi(sc_token);
    expectplur = (delay != 1) ? 1 : 0;

    switch (token()) {
    case WEEKS:
	    delay *= 7;
    case DAYS:
	    delay *= 24;
    case HOURS:
	    delay *= 60;
    case MINUTES:
	    if (expectplur != sc_tokplur)
		warnx("pluralization is wrong");
	    dateadd(delay, tm);
	    return;
    }
    plonk(sc_tokid);
} /* plus */


/*
 * tod() computes the time of day
 *     [NUMBER [DOT NUMBER] [AM|PM]]
 */
static void
tod(struct tm *tm)
{
    int hour, minute = 0;
    int tlen;

    hour = atoi(sc_token);
    tlen = strlen(sc_token);

    /* first pick out the time of day - if it's 4 digits, we assume
     * a HHMM time, otherwise it's HH DOT MM time
     */
    if (token() == DOT) {
	expect(NUMBER);
	minute = atoi(sc_token);
	if (minute > 59)
	    panic("garbled time");
	token();
    }
    else if (tlen == 4) {
	minute = hour%100;
	if (minute > 59)
	    panic("garbeld time");
	hour = hour/100;
    }

    /* check if an AM or PM specifier was given
     */
    if (sc_tokid == AM || sc_tokid == PM) {
	if (hour > 12)
	    panic("garbled time");

	if (sc_tokid == PM) {
	    if (hour != 12)	/* 12:xx PM is 12:xx, not 24:xx */
			hour += 12;
	} else {
	    if (hour == 12)	/* 12:xx AM is 00:xx, not 12:xx */
			hour = 0;
	}
	token();
    }
    else if (hour > 23)
	panic("garbled time");

    /* if we specify an absolute time, we don't want to bump the day even
     * if we've gone past that time - but if we're specifying a time plus
     * a relative offset, it's okay to bump things
     */
    if ((sc_tokid == EOF || sc_tokid == PLUS) && tm->tm_hour > hour) {
	tm->tm_mday++;
	tm->tm_wday++;
    }

    tm->tm_hour = hour;
    tm->tm_min = minute;
    if (tm->tm_hour == 24) {
	tm->tm_hour = 0;
	tm->tm_mday++;
    }
} /* tod */


/*
 * assign_date() assigns a date, wrapping to next year if needed
 */
static void
assign_date(struct tm *tm, long mday, long mon, long year)
{
    if (year > 99) {
	if (year > 1899)
	    year -= 1900;
	else
	    panic("garbled time");
    } else {
	struct tm *lt;
	time_t now;

	time(&now);
	lt = localtime(&now);

	/*
	 * check if the specified year is in the next century.
	 * allow for one year of user error as many people will
	 * enter n - 1 at the start of year n.
	 */
	if (year < (lt->tm_year % 100) - 1)
	    year += 100;
	/* adjust for the year 2000 and beyond */
	year += lt->tm_year - (lt->tm_year % 100);
    }

    if (year < 0 &&
	(tm->tm_mon > mon ||(tm->tm_mon == mon && tm->tm_mday > mday)))
	year = tm->tm_year + 1;

    tm->tm_mday = mday;
    tm->tm_mon = mon;

    if (year >= 0)
	tm->tm_year = year;
} /* assign_date */


/* 
 * month() picks apart a month specification
 *
 *  /[<month> NUMBER [NUMBER]]           \
 *  |[TOMORROW]                          |
 *  |[DAY OF WEEK]                       |
 *  |NUMBER [SLASH NUMBER [SLASH NUMBER]]|
 *  \PLUS NUMBER MINUTES|HOURS|DAYS|WEEKS/
 */
static void
month(struct tm *tm)
{
    long year= (-1);
    long mday, wday, mon;
    int tlen;

    switch (sc_tokid) {
    case PLUS:
	    plus(tm);
	    break;

    case TOMORROW:
	    /* do something tomorrow */
	    tm->tm_mday ++;
	    tm->tm_wday ++;
    case TODAY:	/* force ourselves to stay in today - no further processing */
	    token();
	    break;

    case JAN: case FEB: case MAR: case APR: case MAY: case JUN:
    case JUL: case AUG: case SEP: case OCT: case NOV: case DEC:
	    /* do month mday [year]
	     */
	    mon = (sc_tokid-JAN);
	    expect(NUMBER);
	    mday = atol(sc_token);
	    if (token() == NUMBER) {
		year = atol(sc_token);
		token();
	    }
	    assign_date(tm, mday, mon, year);
	    break;

    case SUN: case MON: case TUE:
    case WED: case THU: case FRI:
    case SAT:
	    /* do a particular day of the week
	     */
	    wday = (sc_tokid-SUN);

	    mday = tm->tm_mday;

	    /* if this day is < today, then roll to next week
	     */
	    if (wday < tm->tm_wday)
		mday += 7 - (tm->tm_wday - wday);
	    else
		mday += (wday - tm->tm_wday);

	    tm->tm_wday = wday;

	    assign_date(tm, mday, tm->tm_mon, tm->tm_year);
	    break;

    case NUMBER:
	    /* get numeric MMDDYY, mm/dd/yy, or dd.mm.yy
	     */
	    tlen = strlen(sc_token);
	    mon = atol(sc_token);
	    token();

	    if (sc_tokid == SLASH || sc_tokid == DOT) {
		int sep;

		sep = sc_tokid;
		expect(NUMBER);
		mday = atol(sc_token);
		if (token() == sep) {
		    expect(NUMBER);
		    year = atol(sc_token);
		    token();
		}

		/* flip months and days for european timing
		 */
		if (sep == DOT) {
		    int x = mday;
		    mday = mon;
		    mon = x;
		}
	    }
	    else if (tlen == 6 || tlen == 8) {
		if (tlen == 8) {
		    year = (mon % 10000) - 1900;
		    mon /= 10000;
		}
		else {
		    year = mon % 100;
		    mon /= 100;
		}
		mday = mon % 100;
		mon /= 100;
	    }
	    else
		panic("garbled time");

	    mon--;
	    if (mon < 0 || mon > 11 || mday < 1 || mday > 31)
		panic("garbled time");

	    assign_date(tm, mday, mon, year);
	    break;
    } /* case */
} /* month */


/* Global functions */

time_t
parsetime(int argc, char **argv)
{
/* Do the argument parsing, die if necessary, and return the time the job
 * should be run.
 */
    time_t nowtimer, runtimer;
    struct tm nowtime, runtime;
    int hr = 0;
    /* this MUST be initialized to zero for midnight/noon/teatime */

    nowtimer = time(NULL);
    nowtime = *localtime(&nowtimer);

    runtime = nowtime;
    runtime.tm_sec = 0;
    runtime.tm_isdst = 0;

    if (argc <= optind)
	usage();

    init_scanner(argc-optind, argv+optind);

    switch (token()) {
    case NOW:	/* now is optional prefix for PLUS tree */
	    expect(PLUS);
    case PLUS:
	    plus(&runtime);
	    break;

    case NUMBER:
	    tod(&runtime);
	    month(&runtime);
	    break;

	    /* evil coding for TEATIME|NOON|MIDNIGHT - we've initialised
	     * hr to zero up above, then fall into this case in such a
	     * way so we add +12 +4 hours to it for teatime, +12 hours
	     * to it for noon, and nothing at all for midnight, then
	     * set our runtime to that hour before leaping into the
	     * month scanner
	     */
    case TEATIME:
	    hr += 4;
    case NOON:
	    hr += 12;
    case MIDNIGHT:
	    if (runtime.tm_hour >= hr) {
		runtime.tm_mday++;
		runtime.tm_wday++;
	    }
	    runtime.tm_hour = hr;
	    runtime.tm_min = 0;
	    token();
	    /* fall through to month setting */
    default:
	    month(&runtime);
	    break;
    } /* ugly case statement */
    expect(EOF);

    /* adjust for daylight savings time
     */
    runtime.tm_isdst = -1;
    runtimer = mktime(&runtime);
    if (runtime.tm_isdst > 0) {
	runtimer -= 3600;
	runtimer = mktime(&runtime);
    }

    if (runtimer < 0)
	panic("garbled time");

    if (nowtimer > runtimer)
	panic("Trying to travel back in time");

    return runtimer;
} /* parsetime */
