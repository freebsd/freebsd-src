/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* This code placed in the public domain by Mark W. Eichin */

#include "autoconf.h"
#include <stdio.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#include <time.h>
#endif

#include "k5-gmt_mktime.h"

#if !HAVE_TIMEGM || TEST_LEAP
static time_t gmt_mktime(struct tm *t);
#endif

/*
 * Use the nonstandard timegm() (if available) to convert broken-down
 * UTC times into time_t values.  Use our custom gmt_mktime() if
 * timegm() is not available.
 *
 * We use gmtime() (or gmtime_r()) when encoding ASN.1 GeneralizedTime
 * values.  On systems where a "right" (leap-second-aware) time zone
 * is configured, gmtime() adjusts for the presence of accumulated
 * leap seconds in the input time_t value.  POSIX requires that time_t
 * values omit leap seconds; systems configured to include leap
 * seconds in their time_t values are non-conforming and will have
 * difficulties exchanging timestamp information with other systems.
 *
 * We use krb5int_gmt_mktime() for decoding ASN.1 GeneralizedTime
 * values.  If timegm() is not available, krb5int_gmt_mktime() won't
 * be the inverse of gmtime() on a system that counts leap seconds.  A
 * system configured with a "right" time zone probably has timegm()
 * available; without it, an application would have no reliable way of
 * converting broken-down UTC times into time_t values.
 */
time_t
krb5int_gmt_mktime(struct tm *t)
{
#if HAVE_TIMEGM
    return timegm(t);
#else
    return gmt_mktime(t);
#endif
}

#if !HAVE_TIMEGM || TEST_LEAP

/* take a struct tm, return seconds from GMT epoch */
/* like mktime, this ignores tm_wday and tm_yday. */
/* unlike mktime, this does not set them... it only passes a return value. */

static const int days_in_month[12] = {
    0,                              /* jan 31 */
    31,                             /* feb 28 */
    59,                             /* mar 31 */
    90,                             /* apr 30 */
    120,                            /* may 31 */
    151,                            /* jun 30 */
    181,                            /* jul 31 */
    212,                            /* aug 31 */
    243,                            /* sep 30 */
    273,                            /* oct 31 */
    304,                            /* nov 30 */
    334                             /* dec 31 */
};

#define hasleapday(year) (year%400?(year%100?(year%4?0:1):0):1)

static time_t
gmt_mktime(struct tm *t)
{
    uint32_t accum;

#define assert_time(cnd) if(!(cnd)) return (time_t) -1

    /*
     * For 32-bit unsigned time values starting on 1/1/1970, the range is:
     * time 0x00000000 -> Thu Jan  1 00:00:00 1970
     * time 0xffffffff -> Sun Feb  7 06:28:15 2106
     *
     * We can't encode all dates in 2106, and we're not doing overflow checking
     * for such cases.
     */
    assert_time(t->tm_year>=70);
    assert_time(t->tm_year<=206);

    assert_time(t->tm_mon>=0);
    assert_time(t->tm_mon<=11);
    assert_time(t->tm_mday>=1);
    assert_time(t->tm_mday<=31);
    assert_time(t->tm_hour>=0);
    assert_time(t->tm_hour<=23);
    assert_time(t->tm_min>=0);
    assert_time(t->tm_min<=59);
    assert_time(t->tm_sec>=0);
    assert_time(t->tm_sec<=62);

#undef assert_time


    accum = t->tm_year - 70;
    accum *= 365;                 /* 365 days/normal year */

    /* add in leap day for all previous years */
    if (t->tm_year >= 70)
        accum += (t->tm_year - 69) / 4;
    else
        accum -= (72 - t->tm_year) / 4;
    /* add in leap day for this year */
    if(t->tm_mon >= 2)            /* march or later */
        if(hasleapday((t->tm_year + 1900))) accum += 1;

    accum += days_in_month[t->tm_mon];
    accum += t->tm_mday-1;        /* days of month are the only 1-based field */
    accum *= 24;                  /* 24 hour/day */
    accum += t->tm_hour;
    accum *= 60;                  /* 60 minute/hour */
    accum += t->tm_min;
    accum *= 60;                  /* 60 seconds/minute */
    accum += t->tm_sec;

    return accum;
}
#endif /* !HAVE_TIMEGM || TEST_LEAP */

#ifdef TEST_LEAP
int
main (int argc, char *argv[])
{
    int yr;
    time_t t;
    struct tm tm = {
        .tm_mon = 0, .tm_mday = 1,
        .tm_hour = 0, .tm_min = 0, .tm_sec = 0,
    };
    for (yr = 60; yr <= 104; yr++)
    {
        printf ("1/1/%d%c -> ", 1900 + yr, hasleapday((1900+yr)) ? '*' : ' ');
        tm.tm_year = yr;
        t = gmt_mktime (&tm);
        if (t == (time_t) -1)
            printf ("-1\n");
        else
        {
            long u;
            if (t % (24 * 60 * 60))
                printf ("(not integral multiple of days) ");
            u = t / (24 * 60 * 60);
            printf ("%3ld*365%+ld\t0x%08lx\n",
                    (long) (u / 365), (long) (u % 365),
                    (long) t);
        }
    }
    t = 0x80000000, printf ("time 0x%lx -> %s", t, ctime (&t));
    t = 0x7fffffff, printf ("time 0x%lx -> %s", t, ctime (&t));
    return 0;
}
#endif
