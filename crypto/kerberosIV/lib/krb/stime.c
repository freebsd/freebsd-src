/*
 * $Id: stime.c,v 1.6 1997/05/02 14:29:20 assar Exp $
 *
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 */

#include "krb_locl.h"

RCSID("$Id: stime.c,v 1.6 1997/05/02 14:29:20 assar Exp $");

/*
 * Given a pointer to a long containing the number of seconds
 * since the beginning of time (midnight 1 Jan 1970 GMT), return
 * a string containing the local time in the form:
 *
 * "25-Jan-1988 10:17:56"
 */

const char *
krb_stime(time_t *t)
{
    static char st[40];
    struct tm *tm;

    tm = localtime(t);
    snprintf(st, sizeof(st),
	     "%2d-%s-%04d %02d:%02d:%02d",tm->tm_mday,
	     month_sname(tm->tm_mon + 1),tm->tm_year + 1900,
	     tm->tm_hour, tm->tm_min, tm->tm_sec);
    return st;
}
