/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
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
#if !defined(lint) && !defined(__GNUC__)
static char rcsid[] =
    "@(#)$Header: /home/ncvs/src/usr.sbin/tcpdump/tcpslice/gwtm2secs.c,v 1.3 1995/08/23 05:18:56 pst Exp $ (LBL)";
#endif

/*
 * gwtm2secs.c - convert "tm" structs for Greenwich time to Unix timestamp
 */

#include "tcpslice.h"

static int days_in_month[] =
	/* Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec */
	{  31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

#define IS_LEAP_YEAR(year)	\
	(year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))

time_t gwtm2secs( struct tm *tm )
	{
	int i, days, year;

	year = tm->tm_year;

	/* Allow for year being specified with either 2 digits or 4 digits.
	 * 2-digit years are either 19xx or 20xx - a simple heuristic
	 * distinguishes them, since we can't represent any time < 1970.
	 */
	if ( year < 100 )
		if ( year >= 70 )
			year += 1900;
		else
			year += 2000;

	days = 0;
	for ( i = 1970; i < year; ++i )
		{
		days += 365;
		if ( IS_LEAP_YEAR(i) )
			++days;
		}

	for ( i = 0; i < tm->tm_mon; ++i )
		days += days_in_month[i];

	if ( IS_LEAP_YEAR(year) && tm->tm_mon > 1 ) /* 1 is February */
		++days;

	days += tm->tm_mday - 1; /* -1 since days are numbered starting at 1 */

	return days * 86400 + tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;
	}
