/* 
 * tclGetDate.y --
 *
 *	Contains yacc grammar for parsing date and time strings
 *	based on getdate.y.
 *
 * Copyright (c) 1992-1995 Karl Lehenbauer and Mark Diekhans.
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclGetDate.y 1.26 96/07/23 16:09:45
 */

%{
/* 
 * tclDate.c --
 *
 *	This file is generated from a yacc grammar defined in
 *	the file tclGetDate.y
 *
 * Copyright (c) 1992-1995 Karl Lehenbauer and Mark Diekhans.
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCSID
 */

#include "tclInt.h"
#include "tclPort.h"

#ifdef MAC_TCL
#   define EPOCH           1904
#   define START_OF_TIME   1904
#   define END_OF_TIME     2039
#else
#   define EPOCH           1970
#   define START_OF_TIME   1902
#   define END_OF_TIME     2037
#endif

#define HOUR(x)         ((int) (60 * x))
#define SECSPERDAY      (24L * 60L * 60L)


/*
 *  An entry in the lexical lookup table.
 */
typedef struct _TABLE {
    char        *name;
    int         type;
    time_t      value;
} TABLE;


/*
 *  Daylight-savings mode:  on, off, or not yet known.
 */
typedef enum _DSTMODE {
    DSTon, DSToff, DSTmaybe
} DSTMODE;

/*
 *  Meridian:  am, pm, or 24-hour style.
 */
typedef enum _MERIDIAN {
    MERam, MERpm, MER24
} MERIDIAN;


/*
 *  Global variables.  We could get rid of most of these by using a good
 *  union as the yacc stack.  (This routine was originally written before
 *  yacc had the %union construct.)  Maybe someday; right now we only use
 *  the %union very rarely.
 */
static char     *yyInput;
static DSTMODE  yyDSTmode;
static time_t   yyDayOrdinal;
static time_t   yyDayNumber;
static int      yyHaveDate;
static int      yyHaveDay;
static int      yyHaveRel;
static int      yyHaveTime;
static int      yyHaveZone;
static time_t   yyTimezone;
static time_t   yyDay;
static time_t   yyHour;
static time_t   yyMinutes;
static time_t   yyMonth;
static time_t   yySeconds;
static time_t   yyYear;
static MERIDIAN yyMeridian;
static time_t   yyRelMonth;
static time_t   yyRelSeconds;


/*
 * Prototypes of internal functions.
 */
static void
yyerror _ANSI_ARGS_((char *s));

static time_t
ToSeconds _ANSI_ARGS_((time_t      Hours,
                       time_t      Minutes,
                       time_t      Seconds,
                       MERIDIAN    Meridian));

static int
Convert _ANSI_ARGS_((time_t      Month,
                     time_t      Day,
                     time_t      Year,
                     time_t      Hours,
                     time_t      Minutes,
                     time_t      Seconds,
                     MERIDIAN    Meridia,
                     DSTMODE     DSTmode,
                     time_t     *TimePtr));

static time_t
DSTcorrect _ANSI_ARGS_((time_t      Start,
                        time_t      Future));

static time_t
RelativeDate _ANSI_ARGS_((time_t      Start,
                          time_t      DayOrdinal,
                          time_t      DayNumber));

static int
RelativeMonth _ANSI_ARGS_((time_t      Start,
                           time_t      RelMonth,
                           time_t     *TimePtr));
static int
LookupWord _ANSI_ARGS_((char  *buff));

static int
yylex _ANSI_ARGS_((void));

int
yyparse _ANSI_ARGS_((void));
%}

%union {
    time_t              Number;
    enum _MERIDIAN      Meridian;
}

%token  tAGO tDAY tDAYZONE tID tMERIDIAN tMINUTE_UNIT tMONTH tMONTH_UNIT
%token  tSEC_UNIT tSNUMBER tUNUMBER tZONE tEPOCH tDST

%type   <Number>        tDAY tDAYZONE tMINUTE_UNIT tMONTH tMONTH_UNIT tDST
%type   <Number>        tSEC_UNIT tSNUMBER tUNUMBER tZONE
%type   <Meridian>      tMERIDIAN o_merid

%%

spec    : /* NULL */
        | spec item
        ;

item    : time {
            yyHaveTime++;
        }
        | zone {
            yyHaveZone++;
        }
        | date {
            yyHaveDate++;
        }
        | day {
            yyHaveDay++;
        }
        | rel {
            yyHaveRel++;
        }
        | number
        ;

time    : tUNUMBER tMERIDIAN {
            yyHour = $1;
            yyMinutes = 0;
            yySeconds = 0;
            yyMeridian = $2;
        }
        | tUNUMBER ':' tUNUMBER o_merid {
            yyHour = $1;
            yyMinutes = $3;
            yySeconds = 0;
            yyMeridian = $4;
        }
        | tUNUMBER ':' tUNUMBER tSNUMBER {
            yyHour = $1;
            yyMinutes = $3;
            yyMeridian = MER24;
            yyDSTmode = DSToff;
            yyTimezone = - ($4 % 100 + ($4 / 100) * 60);
        }
        | tUNUMBER ':' tUNUMBER ':' tUNUMBER o_merid {
            yyHour = $1;
            yyMinutes = $3;
            yySeconds = $5;
            yyMeridian = $6;
        }
        | tUNUMBER ':' tUNUMBER ':' tUNUMBER tSNUMBER {
            yyHour = $1;
            yyMinutes = $3;
            yySeconds = $5;
            yyMeridian = MER24;
            yyDSTmode = DSToff;
            yyTimezone = - ($6 % 100 + ($6 / 100) * 60);
        }
        ;

zone    : tZONE tDST {
            yyTimezone = $1;
            yyDSTmode = DSTon;
        }
        | tZONE {
            yyTimezone = $1;
            yyDSTmode = DSToff;
        }
        | tDAYZONE {
            yyTimezone = $1;
            yyDSTmode = DSTon;
        }
        ;

day     : tDAY {
            yyDayOrdinal = 1;
            yyDayNumber = $1;
        }
        | tDAY ',' {
            yyDayOrdinal = 1;
            yyDayNumber = $1;
        }
        | tUNUMBER tDAY {
            yyDayOrdinal = $1;
            yyDayNumber = $2;
        }
        ;

date    : tUNUMBER '/' tUNUMBER {
            yyMonth = $1;
            yyDay = $3;
        }
        | tUNUMBER '/' tUNUMBER '/' tUNUMBER {
            yyMonth = $1;
            yyDay = $3;
            yyYear = $5;
        }
        | tMONTH tUNUMBER {
            yyMonth = $1;
            yyDay = $2;
        }
        | tMONTH tUNUMBER ',' tUNUMBER {
            yyMonth = $1;
            yyDay = $2;
            yyYear = $4;
        }
        | tUNUMBER tMONTH {
            yyMonth = $2;
            yyDay = $1;
        }
		  | tEPOCH {
				yyMonth = 1;
				yyDay = 1;
				yyYear = EPOCH;
		  }
        | tUNUMBER tMONTH tUNUMBER {
            yyMonth = $2;
            yyDay = $1;
            yyYear = $3;
        }
        ;

rel     : relunit tAGO {
            yyRelSeconds = -yyRelSeconds;
            yyRelMonth = -yyRelMonth;
        }
        | relunit
        ;

relunit : tUNUMBER tMINUTE_UNIT {
            yyRelSeconds += $1 * $2 * 60L;
        }
        | tSNUMBER tMINUTE_UNIT {
            yyRelSeconds += $1 * $2 * 60L;
        }
        | tMINUTE_UNIT {
            yyRelSeconds += $1 * 60L;
        }
        | tSNUMBER tSEC_UNIT {
            yyRelSeconds += $1;
        }
        | tUNUMBER tSEC_UNIT {
            yyRelSeconds += $1;
        }
        | tSEC_UNIT {
            yyRelSeconds++;
        }
        | tSNUMBER tMONTH_UNIT {
            yyRelMonth += $1 * $2;
        }
        | tUNUMBER tMONTH_UNIT {
            yyRelMonth += $1 * $2;
        }
        | tMONTH_UNIT {
            yyRelMonth += $1;
        }
        ;

number  : tUNUMBER {
            if (yyHaveTime && yyHaveDate && !yyHaveRel)
                yyYear = $1;
            else {
                yyHaveTime++;
                if ($1 < 100) {
                    yyHour = $1;
                    yyMinutes = 0;
                }
                else {
                    yyHour = $1 / 100;
                    yyMinutes = $1 % 100;
                }
                yySeconds = 0;
                yyMeridian = MER24;
            }
        }
        ;

o_merid : /* NULL */ {
            $$ = MER24;
        }
        | tMERIDIAN {
            $$ = $1;
        }
        ;

%%

/*
 * Month and day table.
 */
static TABLE    MonthDayTable[] = {
    { "january",        tMONTH,  1 },
    { "february",       tMONTH,  2 },
    { "march",          tMONTH,  3 },
    { "april",          tMONTH,  4 },
    { "may",            tMONTH,  5 },
    { "june",           tMONTH,  6 },
    { "july",           tMONTH,  7 },
    { "august",         tMONTH,  8 },
    { "september",      tMONTH,  9 },
    { "sept",           tMONTH,  9 },
    { "october",        tMONTH, 10 },
    { "november",       tMONTH, 11 },
    { "december",       tMONTH, 12 },
    { "sunday",         tDAY, 0 },
    { "monday",         tDAY, 1 },
    { "tuesday",        tDAY, 2 },
    { "tues",           tDAY, 2 },
    { "wednesday",      tDAY, 3 },
    { "wednes",         tDAY, 3 },
    { "thursday",       tDAY, 4 },
    { "thur",           tDAY, 4 },
    { "thurs",          tDAY, 4 },
    { "friday",         tDAY, 5 },
    { "saturday",       tDAY, 6 },
    { NULL }
};

/*
 * Time units table.
 */
static TABLE    UnitsTable[] = {
    { "year",           tMONTH_UNIT,    12 },
    { "month",          tMONTH_UNIT,    1 },
    { "fortnight",      tMINUTE_UNIT,   14 * 24 * 60 },
    { "week",           tMINUTE_UNIT,   7 * 24 * 60 },
    { "day",            tMINUTE_UNIT,   1 * 24 * 60 },
    { "hour",           tMINUTE_UNIT,   60 },
    { "minute",         tMINUTE_UNIT,   1 },
    { "min",            tMINUTE_UNIT,   1 },
    { "second",         tSEC_UNIT,      1 },
    { "sec",            tSEC_UNIT,      1 },
    { NULL }
};

/*
 * Assorted relative-time words.
 */
static TABLE    OtherTable[] = {
    { "tomorrow",       tMINUTE_UNIT,   1 * 24 * 60 },
    { "yesterday",      tMINUTE_UNIT,   -1 * 24 * 60 },
    { "today",          tMINUTE_UNIT,   0 },
    { "now",            tMINUTE_UNIT,   0 },
    { "last",           tUNUMBER,       -1 },
    { "this",           tMINUTE_UNIT,   0 },
    { "next",           tUNUMBER,       2 },
#if 0
    { "first",          tUNUMBER,       1 },
/*  { "second",         tUNUMBER,       2 }, */
    { "third",          tUNUMBER,       3 },
    { "fourth",         tUNUMBER,       4 },
    { "fifth",          tUNUMBER,       5 },
    { "sixth",          tUNUMBER,       6 },
    { "seventh",        tUNUMBER,       7 },
    { "eighth",         tUNUMBER,       8 },
    { "ninth",          tUNUMBER,       9 },
    { "tenth",          tUNUMBER,       10 },
    { "eleventh",       tUNUMBER,       11 },
    { "twelfth",        tUNUMBER,       12 },
#endif
    { "ago",            tAGO,   1 },
    { "epoch",          tEPOCH,   0 },
    { NULL }
};

/*
 * The timezone table.  (Note: This table was modified to not use any floating
 * point constants to work around an SGI compiler bug).
 */
static TABLE    TimezoneTable[] = {
    { "gmt",    tZONE,     HOUR( 0) },      /* Greenwich Mean */
    { "ut",     tZONE,     HOUR( 0) },      /* Universal (Coordinated) */
    { "utc",    tZONE,     HOUR( 0) },
    { "wet",    tZONE,     HOUR( 0) } ,     /* Western European */
    { "bst",    tDAYZONE,  HOUR( 0) },      /* British Summer */
    { "wat",    tZONE,     HOUR( 1) },      /* West Africa */
    { "at",     tZONE,     HOUR( 2) },      /* Azores */
#if     0
    /* For completeness.  BST is also British Summer, and GST is
     * also Guam Standard. */
    { "bst",    tZONE,     HOUR( 3) },      /* Brazil Standard */
    { "gst",    tZONE,     HOUR( 3) },      /* Greenland Standard */
#endif
    { "nft",    tZONE,     HOUR( 7/2) },    /* Newfoundland */
    { "nst",    tZONE,     HOUR( 7/2) },    /* Newfoundland Standard */
    { "ndt",    tDAYZONE,  HOUR( 7/2) },    /* Newfoundland Daylight */
    { "ast",    tZONE,     HOUR( 4) },      /* Atlantic Standard */
    { "adt",    tDAYZONE,  HOUR( 4) },      /* Atlantic Daylight */
    { "est",    tZONE,     HOUR( 5) },      /* Eastern Standard */
    { "edt",    tDAYZONE,  HOUR( 5) },      /* Eastern Daylight */
    { "cst",    tZONE,     HOUR( 6) },      /* Central Standard */
    { "cdt",    tDAYZONE,  HOUR( 6) },      /* Central Daylight */
    { "mst",    tZONE,     HOUR( 7) },      /* Mountain Standard */
    { "mdt",    tDAYZONE,  HOUR( 7) },      /* Mountain Daylight */
    { "pst",    tZONE,     HOUR( 8) },      /* Pacific Standard */
    { "pdt",    tDAYZONE,  HOUR( 8) },      /* Pacific Daylight */
    { "yst",    tZONE,     HOUR( 9) },      /* Yukon Standard */
    { "ydt",    tDAYZONE,  HOUR( 9) },      /* Yukon Daylight */
    { "hst",    tZONE,     HOUR(10) },      /* Hawaii Standard */
    { "hdt",    tDAYZONE,  HOUR(10) },      /* Hawaii Daylight */
    { "cat",    tZONE,     HOUR(10) },      /* Central Alaska */
    { "ahst",   tZONE,     HOUR(10) },      /* Alaska-Hawaii Standard */
    { "nt",     tZONE,     HOUR(11) },      /* Nome */
    { "idlw",   tZONE,     HOUR(12) },      /* International Date Line West */
    { "cet",    tZONE,    -HOUR( 1) },      /* Central European */
    { "met",    tZONE,    -HOUR( 1) },      /* Middle European */
    { "mewt",   tZONE,    -HOUR( 1) },      /* Middle European Winter */
    { "mest",   tDAYZONE, -HOUR( 1) },      /* Middle European Summer */
    { "swt",    tZONE,    -HOUR( 1) },      /* Swedish Winter */
    { "sst",    tDAYZONE, -HOUR( 1) },      /* Swedish Summer */
    { "fwt",    tZONE,    -HOUR( 1) },      /* French Winter */
    { "fst",    tDAYZONE, -HOUR( 1) },      /* French Summer */
    { "eet",    tZONE,    -HOUR( 2) },      /* Eastern Europe, USSR Zone 1 */
    { "bt",     tZONE,    -HOUR( 3) },      /* Baghdad, USSR Zone 2 */
    { "it",     tZONE,    -HOUR( 7/2) },    /* Iran */
    { "zp4",    tZONE,    -HOUR( 4) },      /* USSR Zone 3 */
    { "zp5",    tZONE,    -HOUR( 5) },      /* USSR Zone 4 */
    { "ist",    tZONE,    -HOUR(11/2) },    /* Indian Standard */
    { "zp6",    tZONE,    -HOUR( 6) },      /* USSR Zone 5 */
#if     0
    /* For completeness.  NST is also Newfoundland Stanard, nad SST is
     * also Swedish Summer. */
    { "nst",    tZONE,    -HOUR(13/2) },    /* North Sumatra */
    { "sst",    tZONE,    -HOUR( 7) },      /* South Sumatra, USSR Zone 6 */
#endif  /* 0 */
    { "wast",   tZONE,    -HOUR( 7) },      /* West Australian Standard */
    { "wadt",   tDAYZONE, -HOUR( 7) },      /* West Australian Daylight */
    { "jt",     tZONE,    -HOUR(15/2) },    /* Java (3pm in Cronusland!) */
    { "cct",    tZONE,    -HOUR( 8) },      /* China Coast, USSR Zone 7 */
    { "jst",    tZONE,    -HOUR( 9) },      /* Japan Standard, USSR Zone 8 */
    { "cast",   tZONE,    -HOUR(19/2) },    /* Central Australian Standard */
    { "cadt",   tDAYZONE, -HOUR(19/2) },    /* Central Australian Daylight */
    { "east",   tZONE,    -HOUR(10) },      /* Eastern Australian Standard */
    { "eadt",   tDAYZONE, -HOUR(10) },      /* Eastern Australian Daylight */
    { "gst",    tZONE,    -HOUR(10) },      /* Guam Standard, USSR Zone 9 */
    { "nzt",    tZONE,    -HOUR(12) },      /* New Zealand */
    { "nzst",   tZONE,    -HOUR(12) },      /* New Zealand Standard */
    { "nzdt",   tDAYZONE, -HOUR(12) },      /* New Zealand Daylight */
    { "idle",   tZONE,    -HOUR(12) },      /* International Date Line East */
    /* ADDED BY Marco Nijdam */
    { "dst",    tDST,     HOUR( 0) },       /* DST on (hour is ignored) */
    /* End ADDED */
    {  NULL  }
};

/*
 * Military timezone table.
 */
static TABLE    MilitaryTable[] = {
    { "a",      tZONE,  HOUR(  1) },
    { "b",      tZONE,  HOUR(  2) },
    { "c",      tZONE,  HOUR(  3) },
    { "d",      tZONE,  HOUR(  4) },
    { "e",      tZONE,  HOUR(  5) },
    { "f",      tZONE,  HOUR(  6) },
    { "g",      tZONE,  HOUR(  7) },
    { "h",      tZONE,  HOUR(  8) },
    { "i",      tZONE,  HOUR(  9) },
    { "k",      tZONE,  HOUR( 10) },
    { "l",      tZONE,  HOUR( 11) },
    { "m",      tZONE,  HOUR( 12) },
    { "n",      tZONE,  HOUR(- 1) },
    { "o",      tZONE,  HOUR(- 2) },
    { "p",      tZONE,  HOUR(- 3) },
    { "q",      tZONE,  HOUR(- 4) },
    { "r",      tZONE,  HOUR(- 5) },
    { "s",      tZONE,  HOUR(- 6) },
    { "t",      tZONE,  HOUR(- 7) },
    { "u",      tZONE,  HOUR(- 8) },
    { "v",      tZONE,  HOUR(- 9) },
    { "w",      tZONE,  HOUR(-10) },
    { "x",      tZONE,  HOUR(-11) },
    { "y",      tZONE,  HOUR(-12) },
    { "z",      tZONE,  HOUR(  0) },
    { NULL }
};


/*
 * Dump error messages in the bit bucket.
 */
static void
yyerror(s)
    char  *s;
{
}


static time_t
ToSeconds(Hours, Minutes, Seconds, Meridian)
    time_t      Hours;
    time_t      Minutes;
    time_t      Seconds;
    MERIDIAN    Meridian;
{
    if (Minutes < 0 || Minutes > 59 || Seconds < 0 || Seconds > 59)
        return -1;
    switch (Meridian) {
    case MER24:
        if (Hours < 0 || Hours > 23)
            return -1;
        return (Hours * 60L + Minutes) * 60L + Seconds;
    case MERam:
        if (Hours < 1 || Hours > 12)
            return -1;
        return ((Hours % 12) * 60L + Minutes) * 60L + Seconds;
    case MERpm:
        if (Hours < 1 || Hours > 12)
            return -1;
        return (((Hours % 12) + 12) * 60L + Minutes) * 60L + Seconds;
    }
    return -1;  /* Should never be reached */
}


static int
Convert(Month, Day, Year, Hours, Minutes, Seconds, Meridian, DSTmode, TimePtr)
    time_t      Month;
    time_t      Day;
    time_t      Year;
    time_t      Hours;
    time_t      Minutes;
    time_t      Seconds;
    MERIDIAN    Meridian;
    DSTMODE     DSTmode;
    time_t     *TimePtr;
{
    static int  DaysInMonth[12] = {
        31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    time_t      tod;
    time_t      Julian;
    int         i;

    if (Year < 0)
        Year = -Year;
    if (Year < 100)
        Year += 1900;
    DaysInMonth[1] = Year % 4 == 0 && (Year % 100 != 0 || Year % 400 == 0)
                    ? 29 : 28;
    if (Month < 1 || Month > 12
     || Year < START_OF_TIME || Year > END_OF_TIME
     || Day < 1 || Day > DaysInMonth[(int)--Month])
        return -1;

    for (Julian = Day - 1, i = 0; i < Month; i++)
        Julian += DaysInMonth[i];
    if (Year >= EPOCH) {
        for (i = EPOCH; i < Year; i++)
            Julian += 365 + (i % 4 == 0);
    } else {
        for (i = Year; i < EPOCH; i++)
            Julian -= 365 + (i % 4 == 0);
    }
    Julian *= SECSPERDAY;
    Julian += yyTimezone * 60L;
    if ((tod = ToSeconds(Hours, Minutes, Seconds, Meridian)) < 0)
        return -1;
    Julian += tod;
    if (DSTmode == DSTon
     || (DSTmode == DSTmaybe && TclpGetDate(&Julian, 0)->tm_isdst))
        Julian -= 60 * 60;
    *TimePtr = Julian;
    return 0;
}


static time_t
DSTcorrect(Start, Future)
    time_t      Start;
    time_t      Future;
{
    time_t      StartDay;
    time_t      FutureDay;

    StartDay = (TclpGetDate(&Start, 0)->tm_hour + 1) % 24;
    FutureDay = (TclpGetDate(&Future, 0)->tm_hour + 1) % 24;
    return (Future - Start) + (StartDay - FutureDay) * 60L * 60L;
}


static time_t
RelativeDate(Start, DayOrdinal, DayNumber)
    time_t      Start;
    time_t      DayOrdinal;
    time_t      DayNumber;
{
    struct tm   *tm;
    time_t      now;

    now = Start;
    tm = TclpGetDate(&now, 0);
    now += SECSPERDAY * ((DayNumber - tm->tm_wday + 7) % 7);
    now += 7 * SECSPERDAY * (DayOrdinal <= 0 ? DayOrdinal : DayOrdinal - 1);
    return DSTcorrect(Start, now);
}


static int
RelativeMonth(Start, RelMonth, TimePtr)
    time_t      Start;
    time_t      RelMonth;
    time_t     *TimePtr;
{
    struct tm   *tm;
    time_t      Month;
    time_t      Year;
    time_t      Julian;

    if (RelMonth == 0) {
        *TimePtr = 0;
        return 0;
    }
    tm = TclpGetDate(&Start, 0);
    Month = 12 * tm->tm_year + tm->tm_mon + RelMonth;
    Year = Month / 12;
    Month = Month % 12 + 1;
    if (Convert(Month, (time_t)tm->tm_mday, Year,
                (time_t)tm->tm_hour, (time_t)tm->tm_min, (time_t)tm->tm_sec,
                MER24, DSTmaybe, &Julian) < 0)
        return -1;
    *TimePtr = DSTcorrect(Start, Julian);
    return 0;
}


static int
LookupWord(buff)
    char                *buff;
{
    register char       *p;
    register char       *q;
    register TABLE      *tp;
    int                 i;
    int                 abbrev;

    /*
     * Make it lowercase.
     */
    for (p = buff; *p; p++) {
        if (isupper(*p)) {
            *p = (char) tolower(*p);
	}
    }

    if (strcmp(buff, "am") == 0 || strcmp(buff, "a.m.") == 0) {
        yylval.Meridian = MERam;
        return tMERIDIAN;
    }
    if (strcmp(buff, "pm") == 0 || strcmp(buff, "p.m.") == 0) {
        yylval.Meridian = MERpm;
        return tMERIDIAN;
    }

    /*
     * See if we have an abbreviation for a month.
     */
    if (strlen(buff) == 3) {
        abbrev = 1;
    } else if (strlen(buff) == 4 && buff[3] == '.') {
        abbrev = 1;
        buff[3] = '\0';
    } else {
        abbrev = 0;
    }

    for (tp = MonthDayTable; tp->name; tp++) {
        if (abbrev) {
            if (strncmp(buff, tp->name, 3) == 0) {
                yylval.Number = tp->value;
                return tp->type;
            }
        } else if (strcmp(buff, tp->name) == 0) {
            yylval.Number = tp->value;
            return tp->type;
        }
    }

    for (tp = TimezoneTable; tp->name; tp++) {
        if (strcmp(buff, tp->name) == 0) {
            yylval.Number = tp->value;
            return tp->type;
        }
    }

    for (tp = UnitsTable; tp->name; tp++) {
        if (strcmp(buff, tp->name) == 0) {
            yylval.Number = tp->value;
            return tp->type;
        }
    }

    /*
     * Strip off any plural and try the units table again.
     */
    i = strlen(buff) - 1;
    if (buff[i] == 's') {
        buff[i] = '\0';
        for (tp = UnitsTable; tp->name; tp++) {
            if (strcmp(buff, tp->name) == 0) {
                yylval.Number = tp->value;
                return tp->type;
            }
	}
    }

    for (tp = OtherTable; tp->name; tp++) {
        if (strcmp(buff, tp->name) == 0) {
            yylval.Number = tp->value;
            return tp->type;
        }
    }

    /*
     * Military timezones.
     */
    if (buff[1] == '\0' && isalpha(*buff)) {
        for (tp = MilitaryTable; tp->name; tp++) {
            if (strcmp(buff, tp->name) == 0) {
                yylval.Number = tp->value;
                return tp->type;
            }
	}
    }

    /*
     * Drop out any periods and try the timezone table again.
     */
    for (i = 0, p = q = buff; *q; q++)
        if (*q != '.')
            *p++ = *q;
        else
            i++;
    *p = '\0';
    if (i)
        for (tp = TimezoneTable; tp->name; tp++) {
            if (strcmp(buff, tp->name) == 0) {
                yylval.Number = tp->value;
                return tp->type;
            }
	}

    return tID;
}


static int
yylex()
{
    register char       c;
    register char       *p;
    char                buff[20];
    int                 Count;
    int                 sign;

    for ( ; ; ) {
        while (isspace((unsigned char) (*yyInput))) {
            yyInput++;
	}

        if (isdigit(c = *yyInput) || c == '-' || c == '+') {
            if (c == '-' || c == '+') {
                sign = c == '-' ? -1 : 1;
                if (!isdigit(*++yyInput)) {
                    /*
		     * skip the '-' sign
		     */
                    continue;
		}
            } else {
                sign = 0;
	    }
            for (yylval.Number = 0; isdigit(c = *yyInput++); ) {
                yylval.Number = 10 * yylval.Number + c - '0';
	    }
            yyInput--;
            if (sign < 0) {
                yylval.Number = -yylval.Number;
	    }
            return sign ? tSNUMBER : tUNUMBER;
        }
        if (isalpha(c)) {
            for (p = buff; isalpha(c = *yyInput++) || c == '.'; ) {
                if (p < &buff[sizeof buff - 1]) {
                    *p++ = c;
		}
	    }
            *p = '\0';
            yyInput--;
            return LookupWord(buff);
        }
        if (c != '(') {
            return *yyInput++;
	}
        Count = 0;
        do {
            c = *yyInput++;
            if (c == '\0') {
                return c;
	    } else if (c == '(') {
                Count++;
	    } else if (c == ')') {
                Count--;
	    }
        } while (Count > 0);
    }
}

/*
 * Specify zone is of -50000 to force GMT.  (This allows BST to work).
 */

int
TclGetDate(p, now, zone, timePtr)
    char          *p;
    unsigned long  now;
    long           zone;
    unsigned long *timePtr;
{
    struct tm           *tm;
    time_t              Start;
    time_t              Time;
    time_t              tod;

    yyInput = p;
    tm = TclpGetDate((time_t *) &now, 0);
    yyYear = tm->tm_year;
    yyMonth = tm->tm_mon + 1;
    yyDay = tm->tm_mday;
    yyTimezone = zone;
    if (zone == -50000) {
        yyDSTmode = DSToff;  /* assume GMT */
        yyTimezone = 0;
    } else {
        yyDSTmode = DSTmaybe;
    }
    yyHour = 0;
    yyMinutes = 0;
    yySeconds = 0;
    yyMeridian = MER24;
    yyRelSeconds = 0;
    yyRelMonth = 0;
    yyHaveDate = 0;
    yyHaveDay = 0;
    yyHaveRel = 0;
    yyHaveTime = 0;
    yyHaveZone = 0;

    if (yyparse() || yyHaveTime > 1 || yyHaveZone > 1 || yyHaveDate > 1 ||
	    yyHaveDay > 1) {
        return -1;
    }
    
    if (yyHaveDate || yyHaveTime || yyHaveDay) {
        if (Convert(yyMonth, yyDay, yyYear, yyHour, yyMinutes, yySeconds,
                    yyMeridian, yyDSTmode, &Start) < 0)
            return -1;
    }
    else {
        Start = now;
        if (!yyHaveRel)
            Start -= ((tm->tm_hour * 60L) + tm->tm_min * 60L) + tm->tm_sec;
    }

    Start += yyRelSeconds;
    if (RelativeMonth(Start, yyRelMonth, &Time) < 0) {
        return -1;
    }
    Start += Time;

    if (yyHaveDay && !yyHaveDate) {
        tod = RelativeDate(Start, yyDayOrdinal, yyDayNumber);
        Start += tod;
    }

    *timePtr = Start;
    return 0;
}
