/*
 * Copyright (c) 1992, 1993, 1996
 *	Berkeley Software Design, Inc.  All rights reserved.
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
 *	This product includes software developed by Berkeley Software
 *	Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI cmos.c,v 2.3 1996/04/08 19:32:20 bostic Exp
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "doscmd.h"

#define ALARM_ON     ((unsigned char) 0x20)
#define FAST_TIMER   ((unsigned char) 0x40)
#define SEC_SIZE     1
#define MIN_SIZE     60
#define HOUR_SIZE    (MIN_SIZE * 60)
#define DAY_SIZE     (HOUR_SIZE * 24)
#define YEAR_DAY     365

#define SEC_MS 1000000
#define FAST_TICK_BSD 0x3D00

#define Jan 31
#define Feb 28
#define Mar 31
#define Apr 30
#define May 31
#define Jun 30
#define Jul 31
#define Aug 31
#define Sep 31
#define Oct 31
#define Nov 30
#define Dec 31

static unsigned char cmos_last_port_70 = 0;
static unsigned char cmos_data[0x40] = {
    0x00,                /* 0x00 Current Second */
    0x00,                /* 0x01 Alarm Second */
    0x00,                /* 0x02 Current minute */
    0x00,                /* 0x03 Alarm minute */
    0x00,                /* 0x04 Current hour */
    0x00,                /* 0x05 Alarm hour */
    0x00,                /* 0x06 Current week day */
    0x00,                /* 0x07 Current day */
    0x00,                /* 0x08 Current month */
    0x00,                /* 0x09 Current year */
    0x26,                /* 0x0A Status register A */
    0x02,                /* 0x0B Status register B */
    0x00,                /* 0x0C Status register C */
    0x80,                /* 0x0D Status register D */
    0x00,                /* 0x0E Diagnostic status */
    0x00,                /* 0x0F Shutdown Code */
    0x00,                /* 0x10 Drive types (1 FDHD disk) */
    0x00,                /* 0x11 Fixed disk 0 type */
    0x00,                /* 0x12 Fixed disk 1 type */
    0x00,
    0x00,                /* Installed equipment */
};

int day_in_year [12] = {
    0, Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov
};

/* consumed by dos.c */
time_t 			delta_clock = 0;

/* locals */
static struct timeval	fast_clock;
static int		fast_tick;

static struct timeval	glob_clock;
static int 		cmos_alarm_time = 0;
static int		cmos_alarm_daytime = 0;

static __inline int
day_in_mon_year(int mon, int year)
{
    return day_in_year[mon] + (mon > 2 && (year % 4 == 0));
}

static __inline int
to_BCD (int n)
{
    n &= 0xFF;
    return n%10 + ((n/10)<<4);
}

static __inline int
from_BCD (int n)
{
    n &= 0xFF;
    return (n & 0xF) + (n >> 4) * 10;
}

/*
** inb() from clock ports.
**
** 0x70 is scratchpad/register select
** 0x71 is data
*/
static unsigned char
cmos_inb(int portnum)
{
    unsigned char ret_val;
    int cmos_reg;
    struct timezone tz;
    struct tm tm;
    time_t now;

    switch (portnum) {
    case 0x70:
	ret_val = cmos_last_port_70;
	break;
    case 0x71:
	cmos_reg = cmos_last_port_70 & 0x3f;
	if (cmos_reg < 0xa) {
	    gettimeofday(&glob_clock, &tz);
	    now = glob_clock.tv_sec + delta_clock;
	    tm = *localtime(&now);
	}
	switch (cmos_reg) {
	case 0:
	    ret_val = to_BCD(tm.tm_sec);
	    break;
	case 2:
	    ret_val = to_BCD(tm.tm_min);
	    break;
	case 4:
	    ret_val = to_BCD(tm.tm_hour);
	    break;
	case 6:
	    ret_val = to_BCD(tm.tm_wday);
	    break;
	case 7:
	    ret_val = to_BCD(tm.tm_mday);
	    break;
	case 8:
	    ret_val = to_BCD(tm.tm_mon + 1);
	    break;
	case 9:
	    ret_val = to_BCD((tm.tm_year + 1900) % 100);
	    break;
	default:
	    ret_val = cmos_data[cmos_reg];
	    break;
	}
	break;
    }
    return (ret_val);
}

static void
cmos_outb(int portnum, unsigned char byte)
{
    int cmos_reg;
    int year;
    int time00;
    struct timezone tz;
    struct tm tm;
    time_t now;

    switch (portnum) {
    case 0x70:
	cmos_last_port_70 = byte;
	break;
    case 0x71:
	cmos_reg = cmos_last_port_70 & 0x3f;
	if (cmos_reg < 0xa) {
	    gettimeofday(&glob_clock, &tz);
	    now = glob_clock.tv_sec + delta_clock;
	    tm = *localtime(&now);
	}
	switch (cmos_reg) {
	case 0:
	    delta_clock += SEC_SIZE * (from_BCD(byte) - tm.tm_sec);
	    break;
	case 1:
	    cmos_alarm_daytime +=
		SEC_SIZE * (from_BCD(byte) - from_BCD(cmos_data[1]));
	    break;
	case 2:
	    delta_clock += MIN_SIZE * (from_BCD(byte) - tm.tm_min);
	    break;
	case 3:
	    cmos_alarm_daytime +=
		MIN_SIZE * (from_BCD(byte) - from_BCD(cmos_data[3]));
	    break;
	case 4:
	    delta_clock += HOUR_SIZE * (from_BCD(byte) - tm.tm_hour);
	    break;
	case 5:
	    cmos_alarm_daytime += 
		HOUR_SIZE * (from_BCD(byte) - from_BCD(cmos_data[5]));
	    break;
	case 7:
	    delta_clock += DAY_SIZE * (from_BCD(byte) - tm.tm_mday);
	    break;
	case 8:
	    delta_clock += DAY_SIZE *
		(day_in_mon_year(from_BCD(byte), tm.tm_year) -
		 day_in_mon_year(tm.tm_mon + 1, tm.tm_year));
	    break;
	case 9:
	    year = from_BCD(byte);
	    delta_clock += DAY_SIZE * (YEAR_DAY * (year - tm.tm_year)
				       + (year/4 - tm.tm_year/4));
	    break;
	case 0xB:
	    cmos_data[0xc] = byte;
	    if (byte & ALARM_ON) {
		debug(D_ALWAYS, "Alarm turned on\n");
		time00 = glob_clock.tv_sec + delta_clock -
		    (tm.tm_sec + MIN_SIZE * tm.tm_min
		     + HOUR_SIZE * tm.tm_hour);
		cmos_alarm_time = time00 + cmos_alarm_daytime;
		if (cmos_alarm_time < (glob_clock.tv_sec + delta_clock))
		    cmos_alarm_time += DAY_SIZE;
	    }
	    if (byte & FAST_TIMER) {
		debug(D_ALWAYS, "Fast timer turned on\n");
		fast_clock = glob_clock;
		fast_tick = 0;
	    }
	    break;
	}
	cmos_data[cmos_reg] = byte;
	break;
    }
}


void
cmos_init(void)
{
    int numflops = 0;
    int checksum = 0;
    int i;

    cmos_data[0x0e] = 0;

    numflops = nfloppies;
    cmos_data[0x10] = (search_floppy(0) << 4) | search_floppy(1);

    if (numflops)			/* floppy drives present + numflops */
        cmos_data[0x14] = ((numflops - 1) << 6) | 1;

    cmos_data[0x15] = 0x80;		/* base memory 640k */
    cmos_data[0x16] = 0x2;
    for (i=0x10; i<=0x2d; i++)
        checksum += cmos_data[i];
    cmos_data[0x2e] = checksum >>8;	/* High byte */
    cmos_data[0x2f] = checksum & 0xFF;	/* Low    byte */

    cmos_data[0x32] = 0x19;		/* Century in BCD ; temporary */

    for (i = 1; i < 12; i++){
        day_in_year[i] += day_in_year[i-1];
    }
    
    define_input_port_handler(0x70, cmos_inb);
    define_input_port_handler(0x71, cmos_inb);
    define_output_port_handler(0x70, cmos_outb);
    define_output_port_handler(0x71, cmos_outb);
}
