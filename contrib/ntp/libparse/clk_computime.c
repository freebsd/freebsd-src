#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_PARSE) && defined(CLOCK_COMPUTIME)
/*
 * /src/NTP/ntp-4/libparse/clk_computime.c,v 4.6 1999/11/28 09:13:49 kardel RELEASE_19991128_A
 *
 * clk_computime.c,v 4.6 1999/11/28 09:13:49 kardel RELEASE_19991128_A
 * 
 * Supports Diem's Computime Radio Clock
 * 
 * Used the Meinberg clock as a template for Diem's Computime Radio Clock
 *
 * adapted by Alois Camenzind <alois.camenzind@ubs.ch>
 * 
 * Copyright (C) 1992-1998 by Frank Kardel
 * Friedrich-Alexander Universität Erlangen-Nürnberg, Germany
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 * 
 */

#include "ntp_fp.h"
#include "ntp_unixtime.h"
#include "ntp_calendar.h"
#include "ntp_stdlib.h"

#include "parse.h"

#ifndef PARSESTREAM
#include <stdio.h>
#else
#include "sys/parsestreams.h"
extern void printf P((const char *, ...));
#endif

/*
 * The Computime receiver sends a datagram in the following format every minute
 * 
 * Timestamp	T:YY:MM:MD:WD:HH:MM:SSCRLF 
 * Pos          0123456789012345678901 2 3
 *              0000000000111111111122 2 2
 * Parse        T:  :  :  :  :  :  :  rn
 * 
 * T	Startcharacter "T" specifies start of the timestamp 
 * YY	Year MM	Month 1-12 
 * MD	Day of the month 
 * WD	Day of week 
 * HH	Hour 
 * MM   Minute 
 * SS   Second
 * CR   Carriage return 
 * LF   Linefeed
 * 
 */

static struct format computime_fmt =
{
	{
		{8, 2},  {5,  2}, {2,  2},	/* day, month, year */
		{14, 2}, {17, 2}, {20, 2},	/* hour, minute, second */
		{11, 2},                        /* dayofweek,  */
	},
	(const unsigned char *)"T:  :  :  :  :  :  :  \r\n",
	0
};

static u_long cvt_computime P((unsigned char *, int, struct format *, clocktime_t *, void *));
static unsigned long inp_computime P((parse_t *, unsigned int, timestamp_t *));

clockformat_t   clock_computime =
{
	inp_computime,		/* Computime input handling */
	cvt_computime,		/* Computime conversion */
	0,			/* no PPS monitoring */
	(void *)&computime_fmt,	/* conversion configuration */
	"Diem's Computime Radio Clock",	/* Computime Radio Clock */
	24,			/* string buffer */
	0			/* no private data (complete pakets) */
};

/*
 * cvt_computime
 * 
 * convert simple type format
 */
static          u_long
cvt_computime(
	unsigned char *buffer,
	int            size,
	struct format *format,
	clocktime_t   *clock_time,
	void          *local
	)
{

	if (!Strok(buffer, format->fixed_string)) { 
		return CVT_NONE;
	} else {
		if (Stoi(&buffer[format->field_offsets[O_DAY].offset], &clock_time->day,
			 format->field_offsets[O_DAY].length) ||
		    Stoi(&buffer[format->field_offsets[O_MONTH].offset], &clock_time->month,
			 format->field_offsets[O_MONTH].length) ||
		    Stoi(&buffer[format->field_offsets[O_YEAR].offset], &clock_time->year,
			 format->field_offsets[O_YEAR].length) ||
		    Stoi(&buffer[format->field_offsets[O_HOUR].offset], &clock_time->hour,
			 format->field_offsets[O_HOUR].length) ||
		    Stoi(&buffer[format->field_offsets[O_MIN].offset], &clock_time->minute,
			 format->field_offsets[O_MIN].length) ||
		    Stoi(&buffer[format->field_offsets[O_SEC].offset], &clock_time->second,
			 format->field_offsets[O_SEC].length)) { 
			return CVT_FAIL | CVT_BADFMT;
		} else {

			clock_time->flags = 0;
			clock_time->utcoffset = 0;	/* We have UTC time */

			return CVT_OK;
		}
	}
}

/*
 * inp_computime
 *
 * grep data from input stream
 */
static u_long
inp_computime(
	      parse_t      *parseio,
	      unsigned int  ch,
	      timestamp_t  *tstamp
	      )
{
	unsigned int rtc;
	
	parseprintf(DD_PARSE, ("inp_computime(0x%lx, 0x%x, ...)\n", (long)parseio, ch));
	
	switch (ch)
	{
	case 'T':
		parseprintf(DD_PARSE, ("inp_computime: START seen\n"));
		
		parseio->parse_index = 1;
		parseio->parse_data[0] = ch;
		parseio->parse_dtime.parse_stime = *tstamp; /* collect timestamp */
		return PARSE_INP_SKIP;
	  
	case '\n':
		parseprintf(DD_PARSE, ("inp_computime: END seen\n"));
		if ((rtc = parse_addchar(parseio, ch)) == PARSE_INP_SKIP)
			return parse_end(parseio);
		else
			return rtc;

	default:
		return parse_addchar(parseio, ch);
	}
}

#else /* not (REFCLOCK && CLOCK_PARSE && CLOCK_COMPUTIME) */
int clk_computime_bs;
#endif /* not (REFCLOCK && CLOCK_PARSE && CLOCK_COMPUTIME) */

/*
 * clk_computime.c,v
 * Revision 4.6  1999/11/28 09:13:49  kardel
 * RECON_4_0_98F
 *
 * Revision 4.5  1998/06/14 21:09:34  kardel
 * Sun acc cleanup
 *
 * Revision 4.4  1998/06/13 12:00:38  kardel
 * fix SYSV clock name clash
 *
 * Revision 4.3  1998/06/12 15:22:26  kardel
 * fix prototypes
 *
 * Revision 4.2  1998/06/12 09:13:24  kardel
 * conditional compile macros fixed
 * printf prototype
 *
 * Revision 4.1  1998/05/24 09:39:51  kardel
 * implementation of the new IO handling model
 *
 * Revision 4.0  1998/04/10 19:45:27  kardel
 * Start 4.0 release version numbering
 *
 * from V3 1.8 log info deleted 1998/04/11 kardel
 */
