/*
 * PARTIME		parse date/time string into a TM structure
 *
 * Returns:
 *	0 if parsing failed
 *	else time values in specified TM structure and zone (unspecified values
 *		set to TMNULL)
 * Notes:
 *	This code is quasi-public; it may be used freely in like software.
 *	It is not to be sold, nor used in licensed software without
 *	permission of the author.
 *	For everyone's benefit, please report bugs and improvements!
 * 	Copyright 1980 by Ken Harrenstien, SRI International.
 *	(ARPANET: KLH @ SRI)
 */

/* Hacknotes:
 *	If parsing changed so that no backup needed, could perhaps modify
 *		to use a FILE input stream.  Need terminator, though.
 *	Perhaps should return 0 on success, else a non-zero error val?
 */

/* $Log: partime.c,v $
 * Revision 5.6  1991/08/19  03:13:55  eggert
 * Update timezones.
 *
 * Revision 5.5  1991/04/21  11:58:18  eggert
 * Don't put , just before } in initializer.
 *
 * Revision 5.4  1990/10/04  06:30:15  eggert
 * Remove date vs time heuristics that fail between 2000 and 2400.
 * Check for overflow when lexing an integer.
 * Parse 'Jan 10 LT' as 'Jan 10, LT', not 'Jan, 10 LT'.
 *
 * Revision 5.3  1990/09/24  18:56:31  eggert
 * Update timezones.
 *
 * Revision 5.2  1990/09/04  08:02:16  eggert
 * Don't parse two-digit years, because it won't work after 1999/12/31.
 * Don't permit 'Aug Aug'.
 *
 * Revision 5.1  1990/08/29  07:13:49  eggert
 * Be able to parse our own date format.  Don't assume year<10000.
 *
 * Revision 5.0  1990/08/22  08:12:40  eggert
 * Switch to GMT and fix the bugs exposed thereby.  Update timezones.
 * Ansify and Posixate.  Fix peekahead and int-size bugs.
 *
 * Revision 1.4  89/05/01  14:48:46  narten
 * fixed #ifdef DEBUG construct
 * 
 * Revision 1.3  88/08/28  14:53:40  eggert
 * Remove unportable "#endif XXX"s.
 * 
 * Revision 1.2  87/03/27  14:21:53  jenkins
 * Port to suns
 * 
 * Revision 1.1  82/05/06  11:38:26  wft
 * Initial revision
 * 
 */

#include "rcsbase.h"

libId(partId, "$Id: partime.c,v 5.6 1991/08/19 03:13:55 eggert Exp $")

#define given(v) (0 <= (v))
#define TMNULL (-1) /* Items not given are given this value */
#define TZ_OFFSET (24*60) /* TMNULL  <  zone_offset - TZ_OFFSET */

struct tmwent {
	char const *went;
	short wval;
	char wflgs;
	char wtype;
};
	/* wflgs */
#define TWTIME 02	/* Word is a time value (absence implies date) */
#define TWDST  04	/* Word is a DST-type timezone */
	/* wtype */
#define TM_MON	1	/* month name */
#define TM_WDAY	2	/* weekday name */
#define TM_ZON	3	/* time zone name */
#define TM_LT	4	/* local time */
#define TM_DST	5	/* daylight savings time */
#define TM_12	6	/* AM, PM, NOON, or MIDNIGHT */
	/* wval (for wtype==TM_12) */
#define T12_AM 1
#define T12_PM 2
#define T12_NOON 12
#define T12_MIDNIGHT 0

static struct tmwent const tmwords [] = {
	{"january",      0, 0, TM_MON},
	{"february",     1, 0, TM_MON},
	{"march",        2, 0, TM_MON},
	{"april",        3, 0, TM_MON},
	{"may",          4, 0, TM_MON},
	{"june",         5, 0, TM_MON},
	{"july",         6, 0, TM_MON},
	{"august",       7, 0, TM_MON},
	{"september",    8, 0, TM_MON},
	{"october",      9, 0, TM_MON},
	{"november",     10, 0, TM_MON},
	{"december",     11, 0, TM_MON},

	{"sunday",       0, 0, TM_WDAY},
	{"monday",       1, 0, TM_WDAY},
	{"tuesday",      2, 0, TM_WDAY},
	{"wednesday",    3, 0, TM_WDAY},
	{"thursday",     4, 0, TM_WDAY},
	{"friday",       5, 0, TM_WDAY},
	{"saturday",     6, 0, TM_WDAY},

	{"gmt",          0*60, TWTIME, TM_ZON},   /* Greenwich */
	{"utc",          0*60, TWTIME, TM_ZON},
	{"ut",           0*60, TWTIME, TM_ZON},
	{"cut",		 0*60, TWTIME, TM_ZON},

	{"nzst",        -12*60, TWTIME, TM_ZON},  /* New Zealand */
	{"jst",         -9*60, TWTIME, TM_ZON},   /* Japan */
	{"kst",         -9*60, TWTIME, TM_ZON},   /* Korea */
	{"ist",         -5*60-30, TWTIME, TM_ZON},/* India */
	{"eet",         -2*60, TWTIME, TM_ZON},   /* Eastern Europe */
	{"cet",         -1*60, TWTIME, TM_ZON},   /* Central Europe */
	{"met",         -1*60, TWTIME, TM_ZON},   /* Middle Europe */
	{"wet",          0*60, TWTIME, TM_ZON},   /* Western Europe */
	{"nst",          3*60+30, TWTIME, TM_ZON},/* Newfoundland */
	{"ast",          4*60, TWTIME, TM_ZON},   /* Atlantic */
	{"est",          5*60, TWTIME, TM_ZON},   /* Eastern */
	{"cst",          6*60, TWTIME, TM_ZON},   /* Central */
	{"mst",          7*60, TWTIME, TM_ZON},   /* Mountain */
	{"pst",          8*60, TWTIME, TM_ZON},   /* Pacific */
	{"akst",         9*60, TWTIME, TM_ZON},   /* Alaska */
	{"hast",         10*60, TWTIME, TM_ZON},  /* Hawaii-Aleutian */
	{"hst",          10*60, TWTIME, TM_ZON},  /* Hawaii */
	{"sst",          11*60, TWTIME, TM_ZON},  /* Samoa */

	{"nzdt",        -12*60, TWTIME+TWDST, TM_ZON},    /* New Zealand */
	{"kdt",         -9*60, TWTIME+TWDST, TM_ZON},     /* Korea */
	{"bst",          0*60, TWTIME+TWDST, TM_ZON},     /* Britain */
	{"ndt",		 3*60+30, TWTIME+TWDST, TM_ZON},  /* Newfoundland */
	{"adt",          4*60, TWTIME+TWDST, TM_ZON},     /* Atlantic */
	{"edt",          5*60, TWTIME+TWDST, TM_ZON},     /* Eastern */
	{"cdt",          6*60, TWTIME+TWDST, TM_ZON},     /* Central */
	{"mdt",          7*60, TWTIME+TWDST, TM_ZON},     /* Mountain */
	{"pdt",          8*60, TWTIME+TWDST, TM_ZON},     /* Pacific */
	{"akdt",         9*60, TWTIME+TWDST, TM_ZON},     /* Alaska */
	{"hadt",         10*60, TWTIME+TWDST, TM_ZON},    /* Hawaii-Aleutian */

#if 0
	/*
	 * The following names are duplicates or are not well attested.
	 * A standard is needed.
	 */
	{"east",        -10*60, TWTIME, TM_ZON},  /* Eastern Australia */
	{"cast",        -9*60-30, TWTIME, TM_ZON},/* Central Australia */
	{"cst",         -8*60, TWTIME, TM_ZON},   /* China */
	{"hkt",         -8*60, TWTIME, TM_ZON},   /* Hong Kong */
	{"sst",         -8*60, TWTIME, TM_ZON},   /* Singapore */
	{"wast",        -8*60, TWTIME, TM_ZON},   /* Western Australia */
	{"?",		-6*60-30, TWTIME, TM_ZON},/* Burma */
	{"?",           -4*60-30, TWTIME, TM_ZON},/* Afghanistan */
	{"it",          -3*60-30, TWTIME, TM_ZON},/* Iran */
	{"ist",         -2*60, TWTIME, TM_ZON},   /* Israel */
	{"mez",		-1*60, TWTIME, TM_ZON},   /* Mittel-Europaeische Zeit */
	{"ast",          1*60, TWTIME, TM_ZON},   /* Azores */
	{"fst",          2*60, TWTIME, TM_ZON},   /* Fernando de Noronha */
	{"bst",          3*60, TWTIME, TM_ZON},   /* Brazil */
	{"wst",          4*60, TWTIME, TM_ZON},   /* Western Brazil */
	{"ast",          5*60, TWTIME, TM_ZON},   /* Acre Brazil */
	{"?",            9*60+30, TWTIME, TM_ZON},/* Marquesas */
	{"?",		 12*60, TWTIME, TM_ZON},  /* Kwajalein */

	{"eadt",        -10*60, TWTIME+TWDST, TM_ZON},    /* Eastern Australia */
	{"cadt",        -9*60-30, TWTIME+TWDST, TM_ZON},  /* Central Australia */
	{"cdt",         -8*60, TWTIME+TWDST, TM_ZON},     /* China */
	{"wadt",        -8*60, TWTIME+TWDST, TM_ZON},     /* Western Australia */
	{"idt",         -2*60, TWTIME+TWDST, TM_ZON},     /* Israel */
	{"eest",        -2*60, TWTIME+TWDST, TM_ZON},     /* Eastern Europe */
	{"cest",        -1*60, TWTIME+TWDST, TM_ZON},     /* Central Europe */
	{"mest",        -1*60, TWTIME+TWDST, TM_ZON},     /* Middle Europe */
	{"mesz",	-1*60, TWTIME+TWDST, TM_ZON},	  /* Mittel-Europaeische Sommerzeit */
	{"west",         0*60, TWTIME+TWDST, TM_ZON},     /* Western Europe */
	{"adt",          1*60, TWTIME+TWDST, TM_ZON},	  /* Azores */
	{"fdt",          2*60, TWTIME+TWDST, TM_ZON},     /* Fernando de Noronha */
	{"edt",          3*60, TWTIME+TWDST, TM_ZON},     /* Eastern Brazil */
	{"wdt",          4*60, TWTIME+TWDST, TM_ZON},     /* Western Brazil */
	{"adt",          5*60, TWTIME+TWDST, TM_ZON},     /* Acre Brazil */
#endif

	{"lt",           0, TWTIME, TM_LT},       /* local time */
	{"dst",          1*60, TWTIME, TM_DST},      /* daylight savings time */
	{"ddst",         2*60, TWTIME, TM_DST},      /* double dst */

	{"am",           T12_AM,	TWTIME, TM_12},
	{"pm",           T12_PM,	TWTIME, TM_12},
	{"noon",         T12_NOON,	TWTIME, TM_12},
	{"midnight",     T12_MIDNIGHT,	TWTIME, TM_12},

	{0, 0, 0, 0}	/* Zero entry to terminate searches */
};

struct token {
	char const *tcp;/* pointer to string */
	int tcnt;	/* # chars */
	char tbrk;	/* "break" char */
	char tbrkl;	/* last break char */
	char tflg;	/* 0 = alpha, 1 = numeric */
	union {         /* Resulting value; */
		int tnum;/* either a #, or */
		struct tmwent const *ttmw;/* a ptr to a tmwent.  */
	} tval;
};

static struct tmwent const*ptmatchstr P((char const*,int,struct tmwent const*));
static int pt12hack P((struct tm *,int));
static int ptitoken P((struct token *));
static int ptstash P((int *,int));
static int pttoken P((struct token *));

	static int
goodzone(t, offset, am)
	register struct token const *t;
	int offset;
	int *am;
{
	register int m;
	if (
		t->tflg  &&
		t->tcnt == 4+offset  &&
		(m = t->tval.tnum) <= 2400  &&
		isdigit(t->tcp[offset]) &&
		(m%=100) < 60
	) {
		m += t->tval.tnum/100 * 60;
		if (t->tcp[offset-1]=='+')
			m = -m;
		*am = m;
		return 1;
	}
	return 0;
}

    int
partime(astr, atm, zone)
char const *astr;
register struct tm *atm;
int *zone;
{
    register int i;
    struct token btoken, atoken;
    int zone_offset; /* minutes west of GMT, plus TZ_OFFSET */
    register char const *cp;
    register char ch;
    int ord, midnoon;
    int *atmfield, dst, m;
    int got1 = 0;

    atm->tm_sec = TMNULL;
    atm->tm_min = TMNULL;
    atm->tm_hour = TMNULL;
    atm->tm_mday = TMNULL;
    atm->tm_mon = TMNULL;
    atm->tm_year = TMNULL;
    atm->tm_wday = TMNULL;
    atm->tm_yday = TMNULL;
    midnoon = TMNULL;		/* and our own temp stuff */
    zone_offset = TMNULL;
    dst = TMNULL;
    btoken.tcnt = btoken.tbrk = 0;
    btoken.tcp = astr;

    for (;; got1=1) {
	if (!ptitoken(&btoken))				/* Get a token */
	  {     if(btoken.tval.tnum) return(0);         /* Read error? */
		if (given(midnoon))			/* EOF, wrap up */
			if (!pt12hack(atm, midnoon))
				return 0;
		if (!given(atm->tm_min))
			atm->tm_min = 0;
		*zone  =
				(given(zone_offset) ? zone_offset-TZ_OFFSET : 0)
			-	(given(dst) ? dst : 0);
		return got1;
	  }
	if(btoken.tflg == 0)		/* Alpha? */
	  {     i = btoken.tval.ttmw->wval;
		switch (btoken.tval.ttmw->wtype) {
		  default:
			return 0;
		  case TM_MON:
			atmfield = &atm->tm_mon;
			break;
		  case TM_WDAY:
			atmfield = &atm->tm_wday;
			break;
		  case TM_DST:
			atmfield = &dst;
			break;
		  case TM_LT:
			if (ptstash(&dst, 0))
				return 0;
			i = 48*60; /* local time magic number -- see maketime() */
			/* fall into */
		  case TM_ZON:
			i += TZ_OFFSET;
			if (btoken.tval.ttmw->wflgs & TWDST)
				if (ptstash(&dst, 60))
					return 0;
			/* Peek ahead for offset immediately afterwards. */
			if (
			    (btoken.tbrk=='-' || btoken.tbrk=='+') &&
			    (atoken=btoken, ++atoken.tcnt, ptitoken(&atoken)) &&
			    goodzone(&atoken, 0, &m)
			) {
				i += m;
				btoken = atoken;
			}
			atmfield = &zone_offset;
			break;
		  case TM_12:
			atmfield = &midnoon;
		}
		if (ptstash(atmfield, i))
			return(0);		/* ERR: val already set */
		continue;
	  }

	/* Token is number.  Lots of hairy heuristics. */
	if (!isdigit(*btoken.tcp)) {
		if (!goodzone(&btoken, 1, &m))
			return 0;
		zone_offset = TZ_OFFSET + m;
		continue;
	}

	i = btoken.tval.tnum;   /* Value now known to be valid; get it. */
	if (btoken.tcnt == 3)	/*  3 digits = HMM   */
	  {
hhmm4:		if (ptstash(&atm->tm_min, i%100))
			return(0);		/* ERR: min conflict */
		i /= 100;
hh2:            if (ptstash(&atm->tm_hour, i))
			return(0);		/* ERR: hour conflict */
		continue;
	  }

	if (4 < btoken.tcnt)
		goto year4; /* far in the future */
	if(btoken.tcnt == 4)	/* 4 digits = YEAR or HHMM */
	  {	if (given(atm->tm_year)) goto hhmm4;	/* Already got yr? */
		if (given(atm->tm_hour)) goto year4;	/* Already got hr? */
		if(btoken.tbrk == ':')			/* HHMM:SS ? */
			if ( ptstash(&atm->tm_hour, i/100)
			  || ptstash(&atm->tm_min, i%100))
				return(0);		/* ERR: hr/min clash */
			else goto coltm2;		/* Go handle SS */
		if(btoken.tbrk != ',' && btoken.tbrk != '/'
		  && (atoken=btoken, ptitoken(&atoken))	/* Peek */
		  && ( atoken.tflg
		     ? !isdigit(*atoken.tcp)
		     : atoken.tval.ttmw->wflgs & TWTIME)) /* HHMM-ZON */
			goto hhmm4;
		goto year4;			/* Give up, assume year. */
	  }

	/* From this point on, assume tcnt == 1 or 2 */
	/* 2 digits = MM, DD, or HH (MM and SS caught at coltime) */
	if(btoken.tbrk == ':')		/* HH:MM[:SS] */
		goto coltime;		/*  must be part of time. */
	if (31 < i)
		return 0;

	/* Check for numerical-format date */
	for (cp = "/-."; ch = *cp++;)
	  {	ord = (ch == '.' ? 0 : 1);	/* n/m = D/M or M/D */
		if(btoken.tbrk == ch)			/* "NN-" */
		  {	if(btoken.tbrkl != ch)
			  {
				atoken = btoken;
				atoken.tcnt++;
				if (ptitoken(&atoken)
				  && atoken.tflg == 0
				  && atoken.tval.ttmw->wtype == TM_MON)
					goto dd2;
				if(ord)goto mm2; else goto dd2; /* "NN-" */
			  }				/* "-NN-" */
			if (!given(atm->tm_mday)
			  && given(atm->tm_year))	/* If "YYYY-NN-" */
				goto mm2;		/* then always MM */
			if(ord)goto dd2; else goto mm2;
		  }
		if(btoken.tbrkl == ch			/* "-NN" */
		  && given(ord ? atm->tm_mon : atm->tm_mday))
			if (!given(ord ? atm->tm_mday : atm->tm_mon)) /* MM/DD */
				if(ord)goto dd2; else goto mm2;
	  }

	/* Now reduced to choice between HH and DD */
	if (given(atm->tm_hour)) goto dd2;	/* Have hour? Assume day. */
	if (given(atm->tm_mday)) goto hh2;	/* Have day? Assume hour. */
	if (given(atm->tm_mon)) goto dd2;	/* Have month? Assume day. */
	if(i > 24) goto dd2;			/* Impossible HH means DD */
	atoken = btoken;
	if (!ptitoken(&atoken))			/* Read ahead! */
		if(atoken.tval.tnum) return(0); /* ERR: bad token */
		else goto dd2;			/* EOF, assume day. */
	if ( atoken.tflg
	   ? !isdigit(*atoken.tcp)
	   : atoken.tval.ttmw->wflgs & TWTIME)
		/* If next token is a time spec, assume hour */
		goto hh2;		/* e.g. "3 PM", "11-EDT"  */

dd2:	if (ptstash(&atm->tm_mday, i))	/* Store day (1 based) */
		return(0);
	continue;

mm2:	if (ptstash(&atm->tm_mon, i-1))	/* Store month (make zero based) */
		return(0);
	continue;

year4:	if ((i-=1900) < 0  ||  ptstash(&atm->tm_year, i)) /* Store year-1900 */
		return(0);		/* ERR: year conflict */
	continue;

	/* Hack HH:MM[[:]SS] */
coltime:
	if (ptstash(&atm->tm_hour, i)) return 0;
	if (!ptitoken(&btoken))
		return(!btoken.tval.tnum);
	if(!btoken.tflg) return(0);	/* ERR: HH:<alpha> */
	if(btoken.tcnt == 4)		/* MMSS */
		if (ptstash(&atm->tm_min, btoken.tval.tnum/100)
		  || ptstash(&atm->tm_sec, btoken.tval.tnum%100))
			return(0);
		else continue;
	if(btoken.tcnt != 2
	  || ptstash(&atm->tm_min, btoken.tval.tnum))
		return(0);		/* ERR: MM bad */
	if (btoken.tbrk != ':') continue;	/* Seconds follow? */
coltm2:	if (!ptitoken(&btoken))
		return(!btoken.tval.tnum);
	if(!btoken.tflg || btoken.tcnt != 2	/* Verify SS */
	  || ptstash(&atm->tm_sec, btoken.tval.tnum))
		return(0);		/* ERR: SS bad */
    }
}

/* Store date/time value, return 0 if successful.
 * Fail if entry is already set.
 */
	static int
ptstash(adr,val)
int *adr;
int val;
{	register int *a;
	if (given(*(a=adr)))
		return 1;
	*a = val;
	return(0);
}

/* This subroutine is invoked for AM, PM, NOON and MIDNIGHT when wrapping up
 * just prior to returning from partime.
 */
	static int
pt12hack(tm, aval)
register struct tm *tm;
register int aval;
{	register int h = tm->tm_hour;
	switch (aval) {
	  case T12_AM:
	  case T12_PM:
		if (h > 12)
			return 0;
		if (h == 12)
			tm->tm_hour = 0;
		if (aval == T12_PM)
			tm->tm_hour += 12;
		break;
	  default:
		if (0 < tm->tm_min  ||  0 < tm->tm_sec)
			return 0;
		if (!given(h) || h==12)
			tm->tm_hour = aval;
		else if (aval==T12_MIDNIGHT  &&  (h==0 || h==24))
			return 0;
	}
	return 1;
}

/* Get a token and identify it to some degree.
 * Returns 0 on failure; token.tval will be 0 for normal EOF, otherwise
 * hit error of some sort
 */

	static int
ptitoken(tkp)
register struct token *tkp;
{
	register char const *cp;
	register int i, j, k;

	if (!pttoken(tkp))
#ifdef DEBUG
	    {
		VOID printf("EOF\n");
		return(0);
	    }
#else
		return(0);
#endif	
	cp = tkp->tcp;

#ifdef DEBUG
	VOID printf("Token: \"%.*s\" ", tkp->tcnt, cp);
#endif

	if (tkp->tflg) {
		i = tkp->tcnt;
		if (*cp == '+' || *cp == '-') {
			cp++;
			i--;
		}
		while (0 <= --i) {
			j = tkp->tval.tnum*10;
			k = j + (*cp++ - '0');
			if (j/10 != tkp->tval.tnum  ||  k < j) {
				/* arithmetic overflow */
				tkp->tval.tnum = 1;
				return 0;
			}
			tkp->tval.tnum = k;
		}
	} else if (!(tkp->tval.ttmw  =  ptmatchstr(cp, tkp->tcnt, tmwords)))
	  {
#ifdef DEBUG
		VOID printf("Not found!\n");
#endif
		tkp->tval.tnum = 1;
		return 0;
	  }

#ifdef DEBUG
	if(tkp->tflg)
		VOID printf("Val: %d.\n",tkp->tval.tnum);
	else VOID printf("Found: \"%s\", val: %d, type %d\n",
		tkp->tval.ttmw->went,tkp->tval.ttmw->wval,tkp->tval.ttmw->wtype);
#endif

	return(1);
}

/* Read token from input string into token structure */
	static int
pttoken(tkp)
register struct token *tkp;
{
	register char const *cp;
	register int c;
	char const *astr;

	tkp->tcp = astr = cp = tkp->tcp + tkp->tcnt;
	tkp->tbrkl = tkp->tbrk;		/* Set "last break" */
	tkp->tcnt = tkp->tbrk = tkp->tflg = 0;
	tkp->tval.tnum = 0;

	while(c = *cp++)
	  {	switch(c)
		  {	case ' ': case '\t':	/* Flush all whitespace */
			case '\r': case '\n':
			case '\v': case '\f':
				if (!tkp->tcnt) {	/* If no token yet */
					tkp->tcp = cp;	/* ignore the brk */
					continue;	/* and go on. */
				}
				/* fall into */
			case '(': case ')':	/* Perhaps any non-alphanum */
			case '-': case ',':	/* shd qualify as break? */
			case '+':
			case '/': case ':': case '.':	/* Break chars */
				if(tkp->tcnt == 0)	/* If no token yet */
				  {	tkp->tcp = cp;	/* ignore the brk */
					tkp->tbrkl = c;
				  	continue;	/* and go on. */
				  }
				tkp->tbrk = c;
				return(tkp->tcnt);
		  }
		if (!tkp->tcnt++) {		/* If first char of token, */
			if (isdigit(c)) {
				tkp->tflg = 1;
				if (astr<cp-2 && (cp[-2]=='-'||cp[-2]=='+')) {
					/* timezone is break+sign+digit */
					tkp->tcp--;
					tkp->tcnt++;
				}
			}
		} else if ((isdigit(c)!=0) != tkp->tflg) { /* else check type */
			tkp->tbrk = c;
			return --tkp->tcnt;	/* Wrong type, back up */
		}
	  }
	return(tkp->tcnt);		/* When hit EOF */
}


	static struct tmwent const *
ptmatchstr(astr,cnt,astruc)
	char const *astr;
	int cnt;
	struct tmwent const *astruc;
{
	register char const *cp, *mp;
	register int c;
	struct tmwent const *lastptr;
	int i;

	lastptr = 0;
	for(;mp = astruc->went; astruc += 1)
	  {	cp = astr;
		for(i = cnt; i > 0; i--)
		  {
			switch (*cp++ - (c = *mp++))
			  {	case 0: continue;	/* Exact match */
				case 'A'-'a':
				    if (ctab[c] == Letter)
					continue;
			  }
			break;
		  }
		if(i==0)
			if (!*mp) return astruc;	/* Exact match */
			else if(lastptr) return(0);	/* Ambiguous */
			else lastptr = astruc;		/* 1st ambig */
	  }
	return lastptr;
}
