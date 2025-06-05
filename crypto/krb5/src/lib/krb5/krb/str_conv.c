/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/str_conv.c - Convert between strings and krb5 data types */
/*
 * Copyright 1995, 1999, 2007 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/*
 * Table of contents:
 *
 * String decoding:
 * ----------------
 * krb5_string_to_salttype()    - Convert string to salttype (krb5_int32)
 * krb5_string_to_timestamp()   - Convert string to krb5_timestamp.
 * krb5_string_to_deltat()      - Convert string to krb5_deltat.
 *
 * String encoding:
 * ----------------
 * krb5_salttype_to_string()    - Convert salttype (krb5_int32) to string.
 * krb5_timestamp_to_string()   - Convert krb5_timestamp to string.
 * krb5_timestamp_to_sfstring() - Convert krb5_timestamp to short filled string
 * krb5_deltat_to_string()      - Convert krb5_deltat to string.
 */

#include "k5-int.h"
#include <ctype.h>

/* Salt type conversions */

/*
 * Local data structures.
 */
struct salttype_lookup_entry {
    krb5_int32          stt_enctype;            /* Salt type number */
    const char *        stt_name;               /* Salt type name */
};

/*
 * Lookup tables.
 */

#include "kdb.h"
static const struct salttype_lookup_entry salttype_table[] = {
    { KRB5_KDB_SALTTYPE_NORMAL,     "normal"     },
    { KRB5_KDB_SALTTYPE_NOREALM,    "norealm",   },
    { KRB5_KDB_SALTTYPE_ONLYREALM,  "onlyrealm", },
    { KRB5_KDB_SALTTYPE_SPECIAL,    "special",   },
};
static const int salttype_table_nents = sizeof(salttype_table)/
    sizeof(salttype_table[0]);

krb5_error_code KRB5_CALLCONV
krb5_string_to_salttype(char *string, krb5_int32 *salttypep)
{
    int i;
    int found;

    found = 0;
    for (i=0; i<salttype_table_nents; i++) {
        if (!strcasecmp(string, salttype_table[i].stt_name)) {
            found = 1;
            *salttypep = salttype_table[i].stt_enctype;
            break;
        }
    }
    return((found) ? 0 : EINVAL);
}

/*
 * Internal datatype to string routines.
 *
 * These routines return 0 for success, EINVAL for invalid parameter, ENOMEM
 * if the supplied buffer/length will not contain the output.
 */
krb5_error_code KRB5_CALLCONV
krb5_salttype_to_string(krb5_int32 salttype, char *buffer, size_t buflen)
{
    int i;
    const char *out;

    out = (char *) NULL;
    for (i=0; i<salttype_table_nents; i++) {
        if (salttype ==  salttype_table[i].stt_enctype) {
            out = salttype_table[i].stt_name;
            break;
        }
    }
    if (out) {
        if (strlcpy(buffer, out, buflen) >= buflen)
            return(ENOMEM);
        return(0);
    }
    else
        return(EINVAL);
}

/* (absolute) time conversions */

#ifdef HAVE_STRPTIME
#ifdef NEED_STRPTIME_PROTO
extern char *strptime (const char *, const char *,
                       struct tm *)
#ifdef __cplusplus
    throw()
#endif
    ;
#endif
#else /* HAVE_STRPTIME */
#undef strptime
#define strptime my_strptime
static char *strptime (const char *, const char *, struct tm *);
#endif

#ifndef HAVE_LOCALTIME_R
static inline struct tm *
localtime_r(const time_t *t, struct tm *buf)
{
    struct tm *tm = localtime(t);
    if (tm == NULL)
        return NULL;
    *buf = *tm;
    return buf;
}
#endif

krb5_error_code KRB5_CALLCONV
krb5_string_to_timestamp(char *string, krb5_timestamp *timestampp)
{
    int i;
    struct tm timebuf, timebuf2;
    time_t now, ret_time;
    char *s;
    static const char * const atime_format_table[] = {
        "%Y%m%d%H%M%S",         /* yyyymmddhhmmss               */
        "%Y.%m.%d.%H.%M.%S",    /* yyyy.mm.dd.hh.mm.ss          */
        "%y%m%d%H%M%S",         /* yymmddhhmmss                 */
        "%y.%m.%d.%H.%M.%S",    /* yy.mm.dd.hh.mm.ss            */
        "%y%m%d%H%M",           /* yymmddhhmm                   */
        "%H%M%S",               /* hhmmss                       */
        "%H%M",                 /* hhmm                         */
        "%T",                   /* hh:mm:ss                     */
        "%R",                   /* hh:mm                        */
        /* The following not really supported unless native strptime present */
        "%x:%X",                /* locale-dependent short format */
        "%d-%b-%Y:%T",          /* dd-month-yyyy:hh:mm:ss       */
        "%d-%b-%Y:%R"           /* dd-month-yyyy:hh:mm          */
    };
    static const int atime_format_table_nents =
        sizeof(atime_format_table)/sizeof(atime_format_table[0]);


    now = time((time_t *) NULL);
    if (localtime_r(&now, &timebuf2) == NULL)
        return EINVAL;
    for (i=0; i<atime_format_table_nents; i++) {
        /* We reset every time throughout the loop as the manual page
         * indicated that no guarantees are made as to preserving timebuf
         * when parsing fails
         */
        timebuf = timebuf2;
        if ((s = strptime(string, atime_format_table[i], &timebuf))
            && (s != string)) {
            /* See if at end of buffer - otherwise partial processing */
            while(*s != 0 && isspace((int) *s)) s++;
            if (*s != 0)
                continue;
            if (timebuf.tm_year <= 0)
                continue;       /* clearly confused */
            ret_time = mktime(&timebuf);
            if (ret_time == (time_t) -1)
                continue;       /* clearly confused */
            *timestampp = (krb5_timestamp) ret_time;
            return 0;
        }
    }
    return(EINVAL);
}

krb5_error_code KRB5_CALLCONV
krb5_timestamp_to_string(krb5_timestamp timestamp, char *buffer, size_t buflen)
{
    size_t ret;
    time_t timestamp2 = ts2tt(timestamp);
    struct tm tmbuf;
    const char *fmt = "%c"; /* This is to get around gcc -Wall warning that
                               the year returned might be two digits */

    if (localtime_r(&timestamp2, &tmbuf) == NULL)
        return(ENOMEM);
    ret = strftime(buffer, buflen, fmt, &tmbuf);
    if (ret == 0 || ret == buflen)
        return(ENOMEM);
    return(0);
}

krb5_error_code KRB5_CALLCONV
krb5_timestamp_to_sfstring(krb5_timestamp timestamp, char *buffer, size_t buflen, char *pad)
{
    struct tm   *tmp;
    size_t i;
    size_t      ndone;
    time_t timestamp2 = ts2tt(timestamp);
    struct tm tmbuf;

    static const char * const sftime_format_table[] = {
        "%c",                   /* Default locale-dependent date and time */
        "%d %b %Y %T",          /* dd mon yyyy hh:mm:ss                 */
        "%x %X",                /* locale-dependent short format        */
        "%x %T",                /* locale-dependent date + hh:mm:ss     */
        "%x %R",                /* locale-dependent date + hh:mm        */
        "%Y-%m-%dT%H:%M:%S",    /* ISO 8601 date + time                 */
        "%Y-%m-%dT%H:%M",       /* ISO 8601 date + hh:mm                */
        "%Y%m%d%H%M%S",         /* ISO 8601 date + time, basic          */
        "%Y%m%d%H%M"            /* ISO 8601 date + hh:mm, basic         */
    };
    static const unsigned int sftime_format_table_nents =
        sizeof(sftime_format_table)/sizeof(sftime_format_table[0]);

    tmp = localtime_r(&timestamp2, &tmbuf);
    if (tmp == NULL)
        return errno;
    ndone = 0;
    for (i=0; i<sftime_format_table_nents; i++) {
        if ((ndone = strftime(buffer, buflen, sftime_format_table[i], tmp)))
            break;
    }
    if (ndone && pad) {
        for (i=ndone; i<buflen-1; i++)
            buffer[i] = *pad;
        buffer[buflen-1] = '\0';
    }
    return((ndone) ? 0 : ENOMEM);
}

/* relative time (delta-t) conversions */

/* string->deltat is in deltat.y */

krb5_error_code KRB5_CALLCONV
krb5_deltat_to_string(krb5_deltat deltat, char *buffer, size_t buflen)
{
    int                 days, hours, minutes, seconds;
    krb5_deltat         dt;

    days = (int) (deltat / (24*3600L));
    dt = deltat % (24*3600L);
    hours = (int) (dt / 3600);
    dt %= 3600;
    minutes = (int) (dt / 60);
    seconds = (int) (dt % 60);

    if (days == 0)
        snprintf(buffer, buflen, "%d:%02d:%02d", hours, minutes, seconds);
    else if (hours || minutes || seconds)
        snprintf(buffer, buflen, "%d %s %02d:%02d:%02d", days,
                 (days > 1) ? "days" : "day",
                 hours, minutes, seconds);
    else
        snprintf(buffer, buflen, "%d %s", days,
                 (days > 1) ? "days" : "day");
    return 0;
}

#undef __P
#define __P(X) X

#ifndef HAVE_STRPTIME
#undef _CurrentTimeLocale
#define _CurrentTimeLocale (&dummy_locale_info)

struct dummy_locale_info_t {
    char d_t_fmt[15];
    char t_fmt_ampm[12];
    char t_fmt[9];
    char d_fmt[9];
    char day[7][10];
    char abday[7][4];
    char mon[12][10];
    char abmon[12][4];
    char am_pm[2][3];
};
static const struct dummy_locale_info_t dummy_locale_info = {
    "%a %b %d %X %Y",           /* %c */
    "%I:%M:%S %p",              /* %r */
    "%H:%M:%S",                 /* %X */
    "%m/%d/%y",                 /* %x */
    { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
      "Saturday" },
    { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" },
    { "January", "February", "March", "April", "May", "June",
      "July", "August", "September", "October", "November", "December" },
    { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" },
    { "AM", "PM" },
};
#undef  TM_YEAR_BASE
#define TM_YEAR_BASE 1900

#include "strptime.c"
#endif
