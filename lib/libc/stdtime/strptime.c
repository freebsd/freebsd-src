/*
 * Copyright (c) 1994 Powerdog Industries.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgement:
 *      This product includes software developed by Powerdog Industries.
 * 4. The name of Powerdog Industries may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY POWERDOG INDUSTRIES ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE POWERDOG INDUSTRIES BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1994 Powerdog Industries.  All rights reserved.";
static char sccsid[] = "@(#)strptime.c  0.1 (Powerdog) 94/03/27";
#endif /* not lint */

#include <time.h>
#include <ctype.h>
#include <locale.h>
#include <string.h>

#define asizeof(a)      (sizeof (a) / sizeof ((a)[0]))

/* #ifndef sun */
struct dtconv {
        char    *abbrev_month_names[12];
        char    *month_names[12];
        char    *abbrev_weekday_names[7];
        char    *weekday_names[7];
        char    *time_format;
        char    *sdate_format;
        char    *dtime_format;
        char    *am_string;
        char    *pm_string;
        char    *ldate_format;
};
/* #endif */

static struct dtconv    En_US = {
        { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" },
        { "January", "February", "March", "April",
          "May", "June", "July", "August",
          "September", "October", "November", "December" },
        { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" },
        { "Sunday", "Monday", "Tuesday", "Wednesday",
          "Thursday", "Friday", "Saturday" },
        "%H:%M:%S",
        "%m/%d/%y",
        "%a %b %e %T %Z %Y",
        "AM",
        "PM",
        "%A, %B, %e, %Y"
};

#ifdef SUNOS4
extern int      strncasecmp();
#endif

char    *
strptime(char *buf, char *fmt, struct tm *tm)
{
        char    c,
                *ptr;
        int     i,
                len;

        ptr = fmt;
        while (*ptr != 0) {
                if (*buf == 0)
                        break;

                c = *ptr++;

                if (c != '%') {
                        if (isspace(c))
                                while (*buf != 0 && isspace(*buf))
                                        buf++;
                        else if (c != *buf++)
                                return 0;
                        continue;
                }

                c = *ptr++;
                switch (c) {
                case 0:
                case '%':
                        if (*buf++ != '%')
                                return 0;
                        break;

                case 'C':
                        buf = strptime(buf, En_US.ldate_format, tm);
                        if (buf == 0)
                                return 0;
                        break;

                case 'c':
                        buf = strptime(buf, "%x %X", tm);
                        if (buf == 0)
                                return 0;
                        break;

                case 'D':
                        buf = strptime(buf, "%m/%d/%y", tm);
                        if (buf == 0)
                                return 0;
                        break;

                case 'R':
                        buf = strptime(buf, "%H:%M", tm);
                        if (buf == 0)
                                return 0;
                        break;

                case 'r':
                        buf = strptime(buf, "%I:%M:%S %p", tm);
                        if (buf == 0)
                                return 0;
                        break;

                case 'T':
                        buf = strptime(buf, "%H:%M:%S", tm);
                        if (buf == 0)
                                return 0;
                        break;

                case 'X':
                        buf = strptime(buf, En_US.time_format, tm);
                        if (buf == 0)
                                return 0;
                        break;

                case 'x':
                        buf = strptime(buf, En_US.sdate_format, tm);
                        if (buf == 0)
                                return 0;
                        break;

                case 'j':
                        if (!isdigit(*buf))
                                return 0;

                        for (i = 0; *buf != 0 && isdigit(*buf); buf++) {
                                i *= 10;
                                i += *buf - '0';
                        }
                        if (i > 365)
                                return 0;

                        tm->tm_yday = i;
                        break;

                case 'M':
                case 'S':
                        if (*buf == 0 || isspace(*buf))
                                break;

                        if (!isdigit(*buf))
                                return 0;

                        for (i = 0; *buf != 0 && isdigit(*buf); buf++) {
                                i *= 10;
                                i += *buf - '0';
                        }
                        if (i > 59)
                                return 0;

                        if (c == 'M')
                                tm->tm_min = i;
                        else
                                tm->tm_sec = i;

                        if (*buf != 0 && isspace(*buf))
                                while (*ptr != 0 && !isspace(*ptr))
                                        ptr++;
                        break;

                case 'H':
                case 'I':
                case 'k':
                case 'l':
                        if (!isdigit(*buf))
                                return 0;

                        for (i = 0; *buf != 0 && isdigit(*buf); buf++) {
                                i *= 10;
                                i += *buf - '0';
                        }
                        if (c == 'H' || c == 'k') {
                                if (i > 23)
                                        return 0;
                        } else if (i > 11)
                                return 0;

                        tm->tm_hour = i;

                        if (*buf != 0 && isspace(*buf))
                                while (*ptr != 0 && !isspace(*ptr))
                                        ptr++;
                        break;

                case 'p':
                        len = strlen(En_US.am_string);
                        if (strncasecmp(buf, En_US.am_string, len) == 0) {
                                if (tm->tm_hour > 12)
                                        return 0;
                                if (tm->tm_hour == 12)
                                        tm->tm_hour = 0;
                                buf += len;
                                break;
                        }

                        len = strlen(En_US.pm_string);
                        if (strncasecmp(buf, En_US.pm_string, len) == 0) {
                                if (tm->tm_hour > 12)
                                        return 0;
                                if (tm->tm_hour != 12)
                                        tm->tm_hour += 12;
                                buf += len;
                                break;
                        }

                        return 0;

                case 'A':
                case 'a':
                        for (i = 0; i < asizeof(En_US.weekday_names); i++) {
                                len = strlen(En_US.weekday_names[i]);
                                if (strncasecmp(buf,
                                                En_US.weekday_names[i],
                                                len) == 0)
                                        break;

                                len = strlen(En_US.abbrev_weekday_names[i]);
                                if (strncasecmp(buf,
                                                En_US.abbrev_weekday_names[i],
                                                len) == 0)
                                        break;
                        }
                        if (i == asizeof(En_US.weekday_names))
                                return 0;

                        tm->tm_wday = i;
                        buf += len;
                        break;

                case 'd':
                case 'e':
                        if (!isdigit(*buf))
                                return 0;

                        for (i = 0; *buf != 0 && isdigit(*buf); buf++) {
                                i *= 10;
                                i += *buf - '0';
                        }
                        if (i > 31)
                                return 0;

                        tm->tm_mday = i;

                        if (*buf != 0 && isspace(*buf))
                                while (*ptr != 0 && !isspace(*ptr))
                                        ptr++;
                        break;

                case 'B':
                case 'b':
                case 'h':
                        for (i = 0; i < asizeof(En_US.month_names); i++) {
                                len = strlen(En_US.month_names[i]);
                                if (strncasecmp(buf,
                                                En_US.month_names[i],
                                                len) == 0)
                                        break;

                                len = strlen(En_US.abbrev_month_names[i]);
                                if (strncasecmp(buf,
                                                En_US.abbrev_month_names[i],
                                                len) == 0)
                                        break;
                        }
                        if (i == asizeof(En_US.month_names))
                                return 0;

                        tm->tm_mon = i;
                        buf += len;
                        break;

                case 'm':
                        if (!isdigit(*buf))
                                return 0;

                        for (i = 0; *buf != 0 && isdigit(*buf); buf++) {
                                i *= 10;
                                i += *buf - '0';
                        }
                        if (i < 1 || i > 12)
                                return 0;

                        tm->tm_mon = i - 1;

                        if (*buf != 0 && isspace(*buf))
                                while (*ptr != 0 && !isspace(*ptr))
                                        ptr++;
                        break;

                case 'Y':
                case 'y':
                        if (*buf == 0 || isspace(*buf))
                                break;

                        if (!isdigit(*buf))
                                return 0;

                        for (i = 0; *buf != 0 && isdigit(*buf); buf++) {
                                i *= 10;
                                i += *buf - '0';
                        }
                        if (c == 'Y')
                                i -= 1900;
                        if (i < 0)
                                return 0;

                        tm->tm_year = i;

                        if (*buf != 0 && isspace(*buf))
                                while (*ptr != 0 && !isspace(*ptr))
                                        ptr++;
                        break;
                }
        }

        return buf;
}

