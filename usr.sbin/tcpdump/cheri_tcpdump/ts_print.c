/*
 * Copyright (c) 1990, 1991, 1993, 1994, 1995, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define NETDISSECT_REWORKED
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <netdissect-stdinc.h>

#include <stdlib.h>
#include <string.h>

#include "netdissect.h"

int32_t thiszone;		/* seconds offset from gmt to local time */

/*
 * Format the timestamp
 */
static char *
ts_format(netdissect_options *ndo, int sec, int usec)
{
	static char buf[sizeof("00:00:00.000000000")];
	const char *format;

#ifdef HAVE_PCAP_SET_TSTAMP_PRECISION
	switch (ndo->ndo_tstamp_precision) {

	case PCAP_TSTAMP_PRECISION_MICRO:
		format = "%02d:%02d:%02d.%06u";
		break;

	case PCAP_TSTAMP_PRECISION_NANO:
		format = "%02d:%02d:%02d.%09u";
		break;

	default:
		format = "%02d:%02d:%02d.{unknown precision}";
		break;
	}
#else
	format = "%02d:%02d:%02d.%06u";
#endif

	snprintf(buf, sizeof(buf), format,
                 sec / 3600, (sec % 3600) / 60, sec % 60, usec);

        return buf;
}

/*
 * Print the timestamp
 */
void
ts_print(netdissect_options *ndo,
         register const struct timeval *tvp)
{
	register int s;
	struct tm *tm;
	time_t Time;
	static unsigned b_sec;
	static unsigned b_usec;
	int d_usec;
	int d_sec;

	switch (ndo->ndo_tflag) {

	case 0: /* Default */
		s = (tvp->tv_sec + thiszone) % 86400;
		printf("%s ", ts_format(ndo, s, tvp->tv_usec));
		break;

	case 1: /* No time stamp */
		break;

	case 2: /* Unix timeval style */
		printf("%u.%06u ",
			     (unsigned)tvp->tv_sec,
			     (unsigned)tvp->tv_usec);
		break;

	case 3: /* Microseconds since previous packet */
        case 5: /* Microseconds since first packet */
		if (b_sec == 0) {
                        /* init timestamp for first packet */
                        b_usec = tvp->tv_usec;
                        b_sec = tvp->tv_sec;
                }

                d_usec = tvp->tv_usec - b_usec;
                d_sec = tvp->tv_sec - b_sec;

                while (d_usec < 0) {
                    d_usec += 1000000;
                    d_sec--;
                }

                printf("%s ", ts_format(ndo, d_sec, d_usec));

                if (ndo->ndo_tflag == 3) { /* set timestamp for last packet */
                    b_sec = tvp->tv_sec;
                    b_usec = tvp->tv_usec;
                }
		break;

	case 4: /* Default + Date*/
		s = (tvp->tv_sec + thiszone) % 86400;
		Time = (tvp->tv_sec + thiszone) - s;
		tm = gmtime (&Time);
		if (!tm)
			printf("Date fail  ");
		else
			printf("%04d-%02d-%02d %s ",
                               tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
                               ts_format(ndo, s, tvp->tv_usec));
		break;
	}
}
