/*-
 * Copyright (c) 1999, 2000
 * Intel Corporation.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 
 *    This product includes software developed by Intel Corporation and
 *    its contributors.
 * 
 * 4. Neither the name of Intel Corporation or its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL INTEL CORPORATION OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/boot/ia64/ski/time.c,v 1.6.18.1 2008/11/25 02:59:29 kensmith Exp $");

#include <time.h>
#include <sys/time.h>
#include <stand.h>

#include "libski.h"

/*
// Accurate only for the past couple of centuries;
// that will probably do.
//
// (#defines From FreeBSD 3.2 lib/libc/stdtime/tzfile.h)
*/

#define isleap(y)	(((y) % 4) == 0 && (((y) % 100) != 0 || ((y) % 400) == 0))
#define SECSPERHOUR ( 60*60 )
#define SECSPERDAY	(24 * SECSPERHOUR)

struct ssc_time {
	int	Year;
	int	Month;
	int	Day;
	int	Hour;
	int	Minute;
	int	Second;
	int	Msec;
	int	Wday;
};

time_t
EfiTimeToUnixTime(struct ssc_time *ETime)
{
    /*
    //  These arrays give the cumulative number of days up to the first of the
    //  month number used as the index (1 -> 12) for regular and leap years.
    //  The value at index 13 is for the whole year.
    */
    static time_t CumulativeDays[2][14] = {
    {0,
     0,
     31,
     31 + 28,
     31 + 28 + 31,
     31 + 28 + 31 + 30,
     31 + 28 + 31 + 30 + 31,
     31 + 28 + 31 + 30 + 31 + 30,
     31 + 28 + 31 + 30 + 31 + 30 + 31,
     31 + 28 + 31 + 30 + 31 + 30 + 31 + 31,
     31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30,
     31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31,
     31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30,
     31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30 + 31 },
    {0,
     0,
     31,
     31 + 29,
     31 + 29 + 31,
     31 + 29 + 31 + 30,
     31 + 29 + 31 + 30 + 31,
     31 + 29 + 31 + 30 + 31 + 30,
     31 + 29 + 31 + 30 + 31 + 30 + 31,
     31 + 29 + 31 + 30 + 31 + 30 + 31 + 31,
     31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30,
     31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31,
     31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30,
     31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30 + 31 }};

    time_t  UTime; 
    int     Year;

    ETime->Year += 1900;

    /*
    //  Do a santity check
    */
    if ( ETime->Year  <  1998 || ETime->Year   > 2099 ||
    	 ETime->Month ==    0 || ETime->Month  >   12 ||
    	 ETime->Day   ==    0 || ETime->Month  >   31 ||
    	                         ETime->Hour   >   23 ||
    	                         ETime->Minute >   59 ||
    	                         ETime->Second >   59 ) {
    	return (0);
    }

    /*
    // Years
    */
    UTime = 0;
    for (Year = 1970; Year != ETime->Year; ++Year) {
        UTime += (CumulativeDays[isleap(Year)][13] * SECSPERDAY);
    }

    /*
    // UTime should now be set to 00:00:00 on Jan 1 of the file's year.
    //
    // Months  
    */
    UTime += (CumulativeDays[isleap(ETime->Year)][ETime->Month] * SECSPERDAY);

    /*
    // UTime should now be set to 00:00:00 on the first of the file's month and year
    //
    // Days -- Don't count the file's day
    */
    UTime += (((ETime->Day > 0) ? ETime->Day-1:0) * SECSPERDAY);

    /*
    // Hours
    */
    UTime += (ETime->Hour * SECSPERHOUR);

    /*
    // Minutes
    */
    UTime += (ETime->Minute * 60);

    /*
    // Seconds
    */
    UTime += ETime->Second;

    return UTime;
}

time_t
time(time_t *tloc)
{
	struct ssc_time time;

	ssc((u_int64_t) &time, 0, 0, 0, SSC_GET_RTC);

	return *tloc = EfiTimeToUnixTime(&time);
}
