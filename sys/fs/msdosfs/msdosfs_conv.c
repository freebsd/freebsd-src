/* $FreeBSD$ */
/*	$NetBSD: msdosfs_conv.c,v 1.25 1997/11/17 15:36:40 ws Exp $	*/

/*-
 * Copyright (C) 1995, 1997 Wolfgang Solfrank.
 * Copyright (C) 1995, 1997 TooLs GmbH.
 * All rights reserved.
 * Original code by Paul Popelka (paulp@uts.amdahl.com) (see below).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 * You can do anything you want with this software, just don't say you wrote
 * it, and don't remove this notice.
 *
 * This software is provided "as is".
 *
 * The author supplies this software to be publicly redistributed on the
 * understanding that the author is not responsible for the correct
 * functioning of this software in any circumstances and is not liable for
 * any damages caused by this software.
 *
 * October 1992
 */

/*
 * System include files.
 */
#include <sys/param.h>
#include <sys/time.h>
#include <sys/kernel.h>		/* defines tz */
#include <sys/systm.h>
#include <machine/clock.h>
#include <sys/dirent.h>

/*
 * MSDOSFS include files.
 */
#include <fs/msdosfs/direntry.h>

/*
 * Total number of days that have passed for each month in a regular year.
 */
static u_short regyear[] = {
	31, 59, 90, 120, 151, 181,
	212, 243, 273, 304, 334, 365
};

/*
 * Total number of days that have passed for each month in a leap year.
 */
static u_short leapyear[] = {
	31, 60, 91, 121, 152, 182,
	213, 244, 274, 305, 335, 366
};

/*
 * Variables used to remember parts of the last time conversion.  Maybe we
 * can avoid a full conversion.
 */
static u_long  lasttime;
static u_long  lastday;
static u_short lastddate;
static u_short lastdtime;

static __inline u_int8_t find_lcode(u_int16_t code, u_int16_t *u2w);

/*
 * Convert the unix version of time to dos's idea of time to be used in
 * file timestamps. The passed in unix time is assumed to be in GMT.
 */
void
unix2dostime(tsp, ddp, dtp, dhp)
	struct timespec *tsp;
	u_int16_t *ddp;
	u_int16_t *dtp;
	u_int8_t *dhp;
{
	u_long t;
	u_long days;
	u_long inc;
	u_long year;
	u_long month;
	u_short *months;

	/*
	 * If the time from the last conversion is the same as now, then
	 * skip the computations and use the saved result.
	 */
	t = tsp->tv_sec - (tz_minuteswest * 60)
	    - (wall_cmos_clock ? adjkerntz : 0);
	    /* - daylight savings time correction */
	t &= ~1;
	if (lasttime != t) {
		lasttime = t;
		lastdtime = (((t / 2) % 30) << DT_2SECONDS_SHIFT)
		    + (((t / 60) % 60) << DT_MINUTES_SHIFT)
		    + (((t / 3600) % 24) << DT_HOURS_SHIFT);

		/*
		 * If the number of days since 1970 is the same as the last
		 * time we did the computation then skip all this leap year
		 * and month stuff.
		 */
		days = t / (24 * 60 * 60);
		if (days != lastday) {
			lastday = days;
			for (year = 1970;; year++) {
				inc = year & 0x03 ? 365 : 366;
				if (days < inc)
					break;
				days -= inc;
			}
			months = year & 0x03 ? regyear : leapyear;
			for (month = 0; days >= months[month]; month++)
				;
			if (month > 0)
				days -= months[month - 1];
			lastddate = ((days + 1) << DD_DAY_SHIFT)
			    + ((month + 1) << DD_MONTH_SHIFT);
			/*
			 * Remember dos's idea of time is relative to 1980.
			 * unix's is relative to 1970.  If somehow we get a
			 * time before 1980 then don't give totally crazy
			 * results.
			 */
			if (year > 1980)
				lastddate += (year - 1980) << DD_YEAR_SHIFT;
		}
	}
	if (dtp)
		*dtp = lastdtime;
	if (dhp)
		*dhp = (tsp->tv_sec & 1) * 100 + tsp->tv_nsec / 10000000;

	*ddp = lastddate;
}

/*
 * The number of seconds between Jan 1, 1970 and Jan 1, 1980. In that
 * interval there were 8 regular years and 2 leap years.
 */
#define	SECONDSTO1980	(((8 * 365) + (2 * 366)) * (24 * 60 * 60))

static u_short lastdosdate;
static u_long  lastseconds;

/*
 * Convert from dos' idea of time to unix'. This will probably only be
 * called from the stat(), and fstat() system calls and so probably need
 * not be too efficient.
 */
void
dos2unixtime(dd, dt, dh, tsp)
	u_int dd;
	u_int dt;
	u_int dh;
	struct timespec *tsp;
{
	u_long seconds;
	u_long month;
	u_long year;
	u_long days;
	u_short *months;

	if (dd == 0) {
		/*
		 * Uninitialized field, return the epoch.
		 */
		tsp->tv_sec = 0;
		tsp->tv_nsec = 0;
		return;
	}
	seconds = (((dt & DT_2SECONDS_MASK) >> DT_2SECONDS_SHIFT) << 1)
	    + ((dt & DT_MINUTES_MASK) >> DT_MINUTES_SHIFT) * 60
	    + ((dt & DT_HOURS_MASK) >> DT_HOURS_SHIFT) * 3600
	    + dh / 100;
	/*
	 * If the year, month, and day from the last conversion are the
	 * same then use the saved value.
	 */
	if (lastdosdate != dd) {
		lastdosdate = dd;
		days = 0;
		year = (dd & DD_YEAR_MASK) >> DD_YEAR_SHIFT;
		days = year * 365;
		days += year / 4 + 1;	/* add in leap days */
		if ((year & 0x03) == 0)
			days--;		/* if year is a leap year */
		months = year & 0x03 ? regyear : leapyear;
		month = (dd & DD_MONTH_MASK) >> DD_MONTH_SHIFT;
		if (month < 1 || month > 12) {
			printf("dos2unixtime(): month value out of range (%ld)\n",
			    month);
			month = 1;
		}
		if (month > 1)
			days += months[month - 2];
		days += ((dd & DD_DAY_MASK) >> DD_DAY_SHIFT) - 1;
		lastseconds = (days * 24 * 60 * 60) + SECONDSTO1980;
	}
	tsp->tv_sec = seconds + lastseconds + (tz_minuteswest * 60)
	     + adjkerntz;
	     /* + daylight savings time correction */
	tsp->tv_nsec = (dh % 100) * 10000000;
}

/*
 * 0 - character disallowed in long file name.
 * 1 - character should be replaced by '_' in DOS file name, 
 *     and generation number inserted.
 * 2 - character ('.' and ' ') should be skipped in DOS file name,
 *     and generation number inserted.
 */
static u_char
unix2dos[256] = {
	0,    0,    0,    0,    0,    0,    0,    0,	/* 00-07 */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 08-0f */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 10-17 */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 18-1f */
	2,    0x21, 0,    0x23, 0x24, 0x25, 0x26, 0x27,	/* 20-27 */
	0x28, 0x29, 0,    1,    1,    0x2d, 2,    0,	/* 28-2f */
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,	/* 30-37 */
	0x38, 0x39, 0,    1,    0,    1,    0,    0,	/* 38-3f */
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,	/* 40-47 */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,	/* 48-4f */
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,	/* 50-57 */
	0x58, 0x59, 0x5a, 1,    0,    1,    0x5e, 0x5f,	/* 58-5f */
	0x60, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,	/* 60-67 */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,	/* 68-6f */
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,	/* 70-77 */
	0x58, 0x59, 0x5a, 0x7b, 0,    0x7d, 0x7e, 0,	/* 78-7f */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 80-87 */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 88-8f */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 90-97 */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 98-9f */
	0,    0xad, 0xbd, 0x9c, 0xcf, 0xbe, 0xdd, 0xf5,	/* a0-a7 */
	0xf9, 0xb8, 0xa6, 0xae, 0xaa, 0xf0, 0xa9, 0xee,	/* a8-af */
	0xf8, 0xf1, 0xfd, 0xfc, 0xef, 0xe6, 0xf4, 0xfa,	/* b0-b7 */
	0xf7, 0xfb, 0xa7, 0xaf, 0xac, 0xab, 0xf3, 0xa8,	/* b8-bf */
	0xb7, 0xb5, 0xb6, 0xc7, 0x8e, 0x8f, 0x92, 0x80,	/* c0-c7 */
	0xd4, 0x90, 0xd2, 0xd3, 0xde, 0xd6, 0xd7, 0xd8,	/* c8-cf */
	0xd1, 0xa5, 0xe3, 0xe0, 0xe2, 0xe5, 0x99, 0x9e,	/* d0-d7 */
	0x9d, 0xeb, 0xe9, 0xea, 0x9a, 0xed, 0xe8, 0xe1,	/* d8-df */
	0xb7, 0xb5, 0xb6, 0xc7, 0x8e, 0x8f, 0x92, 0x80,	/* e0-e7 */
	0xd4, 0x90, 0xd2, 0xd3, 0xde, 0xd6, 0xd7, 0xd8,	/* e8-ef */
	0xd1, 0xa5, 0xe3, 0xe0, 0xe2, 0xe5, 0x99, 0xf6,	/* f0-f7 */
	0x9d, 0xeb, 0xe9, 0xea, 0x9a, 0xed, 0xe8, 0x98,	/* f8-ff */
};

static u_char
dos2unix[256] = {
	0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,	/* 00-07 */
	0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,	/* 08-0f */
	0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,	/* 10-17 */
	0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,	/* 18-1f */
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,	/* 20-27 */
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,	/* 28-2f */
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,	/* 30-37 */
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,	/* 38-3f */
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,	/* 40-47 */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,	/* 48-4f */
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,	/* 50-57 */
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,	/* 58-5f */
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,	/* 60-67 */
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,	/* 68-6f */
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,	/* 70-77 */
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,	/* 78-7f */
	0xc7, 0xfc, 0xe9, 0xe2, 0xe4, 0xe0, 0xe5, 0xe7,	/* 80-87 */
	0xea, 0xeb, 0xe8, 0xef, 0xee, 0xec, 0xc4, 0xc5,	/* 88-8f */
	0xc9, 0xe6, 0xc6, 0xf4, 0xf6, 0xf2, 0xfb, 0xf9,	/* 90-97 */
	0xff, 0xd6, 0xdc, 0xf8, 0xa3, 0xd8, 0xd7, 0x3f,	/* 98-9f */
	0xe1, 0xed, 0xf3, 0xfa, 0xf1, 0xd1, 0xaa, 0xba,	/* a0-a7 */
	0xbf, 0xae, 0xac, 0xbd, 0xbc, 0xa1, 0xab, 0xbb,	/* a8-af */
	0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0xc1, 0xc2, 0xc0,	/* b0-b7 */
	0xa9, 0x3f, 0x3f, 0x3f, 0x3f, 0xa2, 0xa5, 0x3f,	/* b8-bf */
	0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0xe3, 0xc3,	/* c0-c7 */
	0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0xa4,	/* c8-cf */
	0xf0, 0xd0, 0xca, 0xcb, 0xc8, 0x3f, 0xcd, 0xce,	/* d0-d7 */
	0xcf, 0x3f, 0x3f, 0x3f, 0x3f, 0xa6, 0xcc, 0x3f,	/* d8-df */
	0xd3, 0xdf, 0xd4, 0xd2, 0xf5, 0xd5, 0xb5, 0xfe,	/* e0-e7 */
	0xde, 0xda, 0xdb, 0xd9, 0xfd, 0xdd, 0xaf, 0x3f,	/* e8-ef */
	0xad, 0xb1, 0x3f, 0xbe, 0xb6, 0xa7, 0xf7, 0xb8,	/* f0-f7 */
	0xb0, 0xa8, 0xb7, 0xb9, 0xb3, 0xb2, 0x3f, 0x3f,	/* f8-ff */
};

static u_char
u2l[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, /* 00-07 */
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, /* 08-0f */
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, /* 10-17 */
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, /* 18-1f */
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, /* 20-27 */
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, /* 28-2f */
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* 30-37 */
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, /* 38-3f */
	0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, /* 40-47 */
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, /* 48-4f */
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, /* 50-57 */
	0x78, 0x79, 0x7a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, /* 58-5f */
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, /* 60-67 */
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, /* 68-6f */
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, /* 70-77 */
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, /* 78-7f */
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, /* 80-87 */
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, /* 88-8f */
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, /* 90-97 */
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, /* 98-9f */
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, /* a0-a7 */
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, /* a8-af */
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, /* b0-b7 */
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, /* b8-bf */
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, /* c0-c7 */
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, /* c8-cf */
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xd7, /* d0-d7 */
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xdf, /* d8-df */
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, /* e0-e7 */
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, /* e8-ef */
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, /* f0-f7 */
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, /* f8-ff */
};

static u_char
l2u[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, /* 00-07 */
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, /* 08-0f */
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, /* 10-17 */
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, /* 18-1f */
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, /* 20-27 */
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, /* 28-2f */
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* 30-37 */
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, /* 38-3f */
	0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, /* 40-47 */
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, /* 48-4f */
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, /* 50-57 */
	0x78, 0x79, 0x7a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, /* 58-5f */
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, /* 60-67 */
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, /* 68-6f */
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, /* 70-77 */
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, /* 78-7f */
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, /* 80-87 */
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, /* 88-8f */
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, /* 90-97 */
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, /* 98-9f */
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, /* a0-a7 */
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, /* a8-af */
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, /* b0-b7 */
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, /* b8-bf */
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, /* c0-c7 */
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, /* c8-cf */
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xd7, /* d0-d7 */
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xdf, /* d8-df */
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, /* e0-e7 */
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, /* e8-ef */
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, /* f0-f7 */
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, /* f8-ff */
};

/*
 * DOS filenames are made of 2 parts, the name part and the extension part.
 * The name part is 8 characters long and the extension part is 3
 * characters long.  They may contain trailing blanks if the name or
 * extension are not long enough to fill their respective fields.
 */

/*
 * Convert a DOS filename to a unix filename. And, return the number of
 * characters in the resulting unix filename excluding the terminating
 * null.
 */
int
dos2unixfn(dn, un, lower, d2u_loaded, d2u, ul_loaded, ul)
	u_char dn[11];
	u_char *un;
	int lower;
	int d2u_loaded;
	u_int8_t *d2u;
	int ul_loaded;
	u_int8_t *ul;
{
	int i;
	int thislong = 1;
	u_char c;

	/*
	 * If first char of the filename is SLOT_E5 (0x05), then the real
	 * first char of the filename should be 0xe5. But, they couldn't
	 * just have a 0xe5 mean 0xe5 because that is used to mean a freed
	 * directory slot. Another dos quirk.
	 */
	if (*dn == SLOT_E5)
		c = d2u_loaded ? d2u[0xe5 & 0x7f] : dos2unix[0xe5];
	else
		c = d2u_loaded && (*dn & 0x80) ? d2u[*dn & 0x7f] :
		    dos2unix[*dn];
	*un++ = (lower & LCASE_BASE) ? (ul_loaded && (c & 0x80) ?
			 ul[c & 0x7f] : u2l[c]) : c;
	dn++;

	/*
	 * Copy the name portion into the unix filename string.
	 */
	for (i = 1; i < 8 && *dn != ' '; i++) {
		c = d2u_loaded && (*dn & 0x80) ? d2u[*dn & 0x7f] :
		    dos2unix[*dn];
		dn++;
		*un++ = (lower & LCASE_BASE) ? (ul_loaded && (c & 0x80) ?
				 ul[c & 0x7f] : u2l[c]) : c;
		thislong++;
	}
	dn += 8 - i;

	/*
	 * Now, if there is an extension then put in a period and copy in
	 * the extension.
	 */
	if (*dn != ' ') {
		*un++ = '.';
		thislong++;
		for (i = 0; i < 3 && *dn != ' '; i++) {
			c = d2u_loaded && (*dn & 0x80) ? d2u[*dn & 0x7f] :
			    dos2unix[*dn];
			dn++;
			*un++ = (lower & LCASE_EXT) ? (ul_loaded && (c & 0x80) ?
					 ul[c & 0x7f] : u2l[c]) : c;
			thislong++;
		}
	}
	*un++ = 0;

	return (thislong);
}

/*
 * Convert a unix filename to a DOS filename according to Win95 rules.
 * If applicable and gen is not 0, it is inserted into the converted
 * filename as a generation number.
 * Returns
 *	0 if name couldn't be converted
 *	1 if the converted name is the same as the original
 *	  (no long filename entry necessary for Win95)
 *	2 if conversion was successful
 *	3 if conversion was successful and generation number was inserted
 */
int
unix2dosfn(un, dn, unlen, gen, u2d_loaded, u2d, lu_loaded, lu)
	const u_char *un;
	u_char dn[12];
	int unlen;
	u_int gen;
	int u2d_loaded;
	u_int8_t *u2d;
	int lu_loaded;
	u_int8_t *lu;
{
	int i, j, l;
	int conv = 1;
	const u_char *cp, *dp, *dp1;
	u_char gentext[6], *wcp;
	u_int8_t c;
#define U2D(c) (u2d_loaded && ((c) & 0x80) ? u2d[(c) & 0x7f] : unix2dos[c])

	/*
	 * Fill the dos filename string with blanks. These are DOS's pad
	 * characters.
	 */
	for (i = 0; i < 11; i++)
		dn[i] = ' ';
	dn[11] = 0;

	/*
	 * The filenames "." and ".." are handled specially, since they
	 * don't follow dos filename rules.
	 */
	if (un[0] == '.' && unlen == 1) {
		dn[0] = '.';
		return gen <= 1;
	}
	if (un[0] == '.' && un[1] == '.' && unlen == 2) {
		dn[0] = '.';
		dn[1] = '.';
		return gen <= 1;
	}

	/*
	 * Filenames with only blanks and dots are not allowed!
	 */
	for (cp = un, i = unlen; --i >= 0; cp++)
		if (*cp != ' ' && *cp != '.')
			break;
	if (i < 0)
		return 0;


	/*
	 * Filenames with some characters are not allowed!
	 */
	for (cp = un, i = unlen; --i >= 0; cp++)
		if (U2D(*cp) == 0)
			return 0;

	/*
	 * Now find the extension
	 * Note: dot as first char doesn't start extension
	 *	 and trailing dots and blanks are ignored
	 */
	dp = dp1 = 0;
	for (cp = un + 1, i = unlen - 1; --i >= 0;) {
		switch (*cp++) {
		case '.':
			if (!dp1)
				dp1 = cp;
			break;
		case ' ':
			break;
		default:
			if (dp1)
				dp = dp1;
			dp1 = 0;
			break;
		}
	}

	/*
	 * Now convert it
	 */
	if (dp) {
		if (dp1)
			l = dp1 - dp;
		else
			l = unlen - (dp - un);
		for (i = 0, j = 8; i < l && j < 11; i++, j++) {
			c = dp[i];
			c = lu_loaded && (c & 0x80) ?
			    lu[c & 0x7f] : l2u[c];
			c = U2D(c);
			if (dp[i] != (dn[j] = c)
			    && conv != 3)
				conv = 2;
			if (dn[j] == 1) {
				conv = 3;
				dn[j] = '_';
			}
			if (dn[j] == 2) {
				conv = 3;
				dn[j--] = ' ';
			}
		}
		if (i < l)
			conv = 3;
		dp--;
	} else {
		for (dp = cp; *--dp == ' ' || *dp == '.';);
		dp++;
	}

	/*
	 * Now convert the rest of the name
	 */
	for (i = j = 0; un < dp && j < 8; i++, j++, un++) {
		c = lu_loaded && (*un & 0x80) ?
		    lu[*un & 0x7f] : l2u[*un];
		c = U2D(c);
		if (*un != (dn[j] = c)
		    && conv != 3)
			conv = 2;
		if (dn[j] == 1) {
			conv = 3;
			dn[j] = '_';
		}
		if (dn[j] == 2) {
			conv = 3;
			dn[j--] = ' ';
		}
	}
	if (un < dp)
		conv = 3;
	/*
	 * If we didn't have any chars in filename,
	 * generate a default
	 */
	if (!j)
		dn[0] = '_';

	/*
	 * The first character cannot be E5,
	 * because that means a deleted entry
	 */
	if (dn[0] == 0xe5)
		dn[0] = SLOT_E5;

	/*
	 * If there wasn't any char dropped,
	 * there is no place for generation numbers
	 */
	if (conv != 3) {
		if (gen > 1)
			return 0;
		return conv;
	}

	/*
	 * Now insert the generation number into the filename part
	 */
	if (gen == 0)
		return conv;
	for (wcp = gentext + sizeof(gentext); wcp > gentext && gen; gen /= 10)
		*--wcp = gen % 10 + '0';
	if (gen)
		return 0;
	for (i = 8; dn[--i] == ' ';);
	i++;
	if (gentext + sizeof(gentext) - wcp + 1 > 8 - i)
		i = 8 - (gentext + sizeof(gentext) - wcp + 1);
	dn[i++] = '~';
	while (wcp < gentext + sizeof(gentext))
		dn[i++] = *wcp++;
	return 3;
#undef U2D
}

/*
 * Create a Win95 long name directory entry
 * Note: assumes that the filename is valid,
 *	 i.e. doesn't consist solely of blanks and dots
 */
int
unix2winfn(un, unlen, wep, cnt, chksum, table_loaded, u2w)
	const u_char *un;
	int unlen;
	struct winentry *wep;
	int cnt;
	int chksum;
	int table_loaded;
	u_int16_t *u2w;
{
	const u_int8_t *cp;
	u_int8_t *wcp;
	int i;
	u_int16_t code;

	/*
	 * Drop trailing blanks and dots
	 */
	for (cp = un + unlen; *--cp == ' ' || *cp == '.'; unlen--);

	un += (cnt - 1) * WIN_CHARS;
	unlen -= (cnt - 1) * WIN_CHARS;

	/*
	 * Initialize winentry to some useful default
	 */
	for (wcp = (u_int8_t *)wep, i = sizeof(*wep); --i >= 0; *wcp++ = 0xff);
	wep->weCnt = cnt;
	wep->weAttributes = ATTR_WIN95;
	wep->weReserved1 = 0;
	wep->weChksum = chksum;
	wep->weReserved2 = 0;

	/*
	 * Now convert the filename parts
	 */
	for (wcp = wep->wePart1, i = sizeof(wep->wePart1)/2; --i >= 0;) {
		if (--unlen < 0)
			goto done;
		if (table_loaded && (*un & 0x80)) {
			code = u2w[*un++ & 0x7f];
			*wcp++ = code;
			*wcp++ = code >> 8;
		} else {
			*wcp++ = *un++;
			*wcp++ = 0;
		}
	}
	for (wcp = wep->wePart2, i = sizeof(wep->wePart2)/2; --i >= 0;) {
		if (--unlen < 0)
			goto done;
		if (table_loaded && (*un & 0x80)) {
			code = u2w[*un++ & 0x7f];
			*wcp++ = code;
			*wcp++ = code >> 8;
		} else {
			*wcp++ = *un++;
			*wcp++ = 0;
		}
	}
	for (wcp = wep->wePart3, i = sizeof(wep->wePart3)/2; --i >= 0;) {
		if (--unlen < 0)
			goto done;
		if (table_loaded && (*un & 0x80)) {
			code = u2w[*un++ & 0x7f];
			*wcp++ = code;
			*wcp++ = code >> 8;
		} else {
			*wcp++ = *un++;
			*wcp++ = 0;
		}
	}
	if (!unlen)
		wep->weCnt |= WIN_LAST;
	return unlen;

done:
	*wcp++ = 0;
	*wcp++ = 0;
	wep->weCnt |= WIN_LAST;
	return 0;
}

static __inline u_int8_t
find_lcode(code, u2w)
	u_int16_t code;
	u_int16_t *u2w;
{
	int i;

	for (i = 0; i < 128; i++)
		if (u2w[i] == code)
			return (i | 0x80);
	return '?';
}

/*
 * Compare our filename to the one in the Win95 entry
 * Returns the checksum or -1 if no match
 */
int
winChkName(un, unlen, wep, chksum, u2w_loaded, u2w, ul_loaded, ul)
	const u_char *un;
	int unlen;
	struct winentry *wep;
	int chksum;
	int u2w_loaded;
	u_int16_t *u2w;
	int ul_loaded;
	u_int8_t *ul;
{
	u_int8_t *cp;
	int i;
	u_int16_t code;
	u_int8_t c1, c2;

	/*
	 * First compare checksums
	 */
	if (wep->weCnt&WIN_LAST)
		chksum = wep->weChksum;
	else if (chksum != wep->weChksum)
		chksum = -1;
	if (chksum == -1)
		return -1;

	/*
	 * Offset of this entry
	 */
	i = ((wep->weCnt&WIN_CNT) - 1) * WIN_CHARS;
	un += i;
	unlen -= i;

	/*
	 * unlen being zero must not be treated as length missmatch. It is
	 * possible if the entry is WIN_LAST and contains nothing but the
	 * terminating 0.
	 */
	if (unlen < 0)
		return -1;
	if ((wep->weCnt&WIN_LAST) && unlen > WIN_CHARS)
		return -1;

	/*
	 * Compare the name parts
	 */
	for (cp = wep->wePart1, i = sizeof(wep->wePart1)/2; --i >= 0;) {
		if (--unlen < 0) {
			if (!*cp++ && !*cp)
				return chksum;
			return -1;
		}
		code = (cp[1] << 8) | cp[0];
		if (code & 0xff80) {
			if (u2w_loaded)
				code = find_lcode(code, u2w);
			else if (code & 0xff00)
				code = '?';
		}
		c1 = ul_loaded && (code & 0x80) ?
		     ul[code & 0x7f] : u2l[code];
		c2 = ul_loaded && (*un & 0x80) ?
		     ul[*un & 0x7f] : u2l[*un];
		if (c1 != c2)
			return -1;
		cp += 2;
		un++;
	}
	for (cp = wep->wePart2, i = sizeof(wep->wePart2)/2; --i >= 0;) {
		if (--unlen < 0) {
			if (!*cp++ && !*cp)
				return chksum;
			return -1;
		}
		code = (cp[1] << 8) | cp[0];
		if (code & 0xff80) {
			if (u2w_loaded)
				code = find_lcode(code, u2w);
			else if (code & 0xff00)
				code = '?';
		}
		c1 = ul_loaded && (code & 0x80) ?
		     ul[code & 0x7f] : u2l[code];
		c2 = ul_loaded && (*un & 0x80) ?
		     ul[*un & 0x7f] : u2l[*un];
		if (c1 != c2)
			return -1;
		cp += 2;
		un++;
	}
	for (cp = wep->wePart3, i = sizeof(wep->wePart3)/2; --i >= 0;) {
		if (--unlen < 0) {
			if (!*cp++ && !*cp)
				return chksum;
			return -1;
		}
		code = (cp[1] << 8) | cp[0];
		if (code & 0xff80) {
			if (u2w_loaded)
				code = find_lcode(code, u2w);
			else if (code & 0xff00)
				code = '?';
		}
		c1 = ul_loaded && (code & 0x80) ?
		     ul[code & 0x7f] : u2l[code];
		c2 = ul_loaded && (*un & 0x80) ?
		     ul[*un & 0x7f] : u2l[*un];
		if (c1 != c2)
			return -1;
		cp += 2;
		un++;
	}
	return chksum;
}

/*
 * Convert Win95 filename to dirbuf.
 * Returns the checksum or -1 if impossible
 */
int
win2unixfn(wep, dp, chksum, table_loaded, u2w)
	struct winentry *wep;
	struct dirent *dp;
	int chksum;
	int table_loaded;
	u_int16_t *u2w;
{
	u_int8_t *cp;
	u_int8_t *np, *ep = dp->d_name + WIN_MAXLEN;
	u_int16_t code;
	int i;

	if ((wep->weCnt&WIN_CNT) > howmany(WIN_MAXLEN, WIN_CHARS)
	    || !(wep->weCnt&WIN_CNT))
		return -1;

	/*
	 * First compare checksums
	 */
	if (wep->weCnt&WIN_LAST) {
		chksum = wep->weChksum;
		/*
		 * This works even though d_namlen is one byte!
		 */
		dp->d_namlen = (wep->weCnt&WIN_CNT) * WIN_CHARS;
	} else if (chksum != wep->weChksum)
		chksum = -1;
	if (chksum == -1)
		return -1;

	/*
	 * Offset of this entry
	 */
	i = ((wep->weCnt&WIN_CNT) - 1) * WIN_CHARS;
	np = (u_int8_t *)dp->d_name + i;

	/*
	 * Convert the name parts
	 */
	for (cp = wep->wePart1, i = sizeof(wep->wePart1)/2; --i >= 0;) {
		code = (cp[1] << 8) | cp[0];
		switch (code) {
		case 0:
			*np = '\0';
			dp->d_namlen -= sizeof(wep->wePart2)/2
			    + sizeof(wep->wePart3)/2 + i + 1;
			return chksum;
		case '/':
			*np = '\0';
			return -1;
		default:
			if (code & 0xff80) {
				if (table_loaded)
					code = find_lcode(code, u2w);
				else if (code & 0xff00)
					code = '?';
			}
			*np++ = code;
			break;
		}
		/*
		 * The size comparison should result in the compiler
		 * optimizing the whole if away
		 */
		if (WIN_MAXLEN % WIN_CHARS < sizeof(wep->wePart1) / 2
		    && np > ep) {
			np[-1] = 0;
			return -1;
		}
		cp += 2;
	}
	for (cp = wep->wePart2, i = sizeof(wep->wePart2)/2; --i >= 0;) {
		code = (cp[1] << 8) | cp[0];
		switch (code) {
		case 0:
			*np = '\0';
			dp->d_namlen -= sizeof(wep->wePart3)/2 + i + 1;
			return chksum;
		case '/':
			*np = '\0';
			return -1;
		default:
			if (code & 0xff80) {
				if (table_loaded)
					code = find_lcode(code, u2w);
				else if (code & 0xff00)
					code = '?';
			}
			*np++ = code;
			break;
		}
		/*
		 * The size comparisons should be optimized away
		 */
		if (WIN_MAXLEN % WIN_CHARS >= sizeof(wep->wePart1) / 2
		    && WIN_MAXLEN % WIN_CHARS < (sizeof(wep->wePart1) + sizeof(wep->wePart2)) / 2
		    && np > ep) {
			np[-1] = 0;
			return -1;
		}
		cp += 2;
	}
	for (cp = wep->wePart3, i = sizeof(wep->wePart3)/2; --i >= 0;) {
		code = (cp[1] << 8) | cp[0];
		switch (code) {
		case 0:
			*np = '\0';
			dp->d_namlen -= i + 1;
			return chksum;
		case '/':
			*np = '\0';
			return -1;
		default:
			if (code & 0xff80) {
				if (table_loaded)
					code = find_lcode(code, u2w);
				else if (code & 0xff00)
					code = '?';
			}
			*np++ = code;
			break;
		}
		/*
		 * See above
		 */
		if (WIN_MAXLEN % WIN_CHARS >= (sizeof(wep->wePart1) + sizeof(wep->wePart2)) / 2
		    && np > ep) {
			np[-1] = 0;
			return -1;
		}
		cp += 2;
	}
	return chksum;
}

/*
 * Compute the checksum of a DOS filename for Win95 use
 */
u_int8_t
winChksum(name)
	u_int8_t *name;
{
	int i;
	u_int8_t s;

	for (s = 0, i = 11; --i >= 0; s += *name++)
		s = (s << 7)|(s >> 1);
	return s;
}

/*
 * Determine the number of slots necessary for Win95 names
 */
int
winSlotCnt(un, unlen)
	const u_char *un;
	int unlen;
{
	unlen = winLenFixup(un, unlen);
	if (unlen > WIN_MAXLEN)
		return 0;
	return howmany(unlen, WIN_CHARS);
}

/*
 * Determine the number of bytes neccesary for Win95 names
 */
int
winLenFixup(un, unlen)
	const u_char* un;
	int unlen;
{
	for (un += unlen; unlen > 0; unlen--)
		if (*--un != ' ' && *un != '.')
			break;
	return unlen;
}
