/*
 * /src/NTP/ntp-4/libparse/clk_dcf7000.c,v 4.6 1999/11/28 09:13:49 kardel RELEASE_19991128_A
 *  
 * clk_dcf7000.c,v 4.6 1999/11/28 09:13:49 kardel RELEASE_19991128_A
 *
 * ELV DCF7000 module
 *
 * Copyright (C) 1992, 1993, 1994, 1995, 1996, 1997, 1998 by Frank Kardel
 * Friedrich-Alexander Universität Erlangen-Nürnberg, Germany
 *                                    
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_PARSE) && defined(CLOCK_DCF7000)

#include "ntp_fp.h"
#include "ntp_unixtime.h"
#include "ntp_calendar.h"

#include "parse.h"

#ifndef PARSESTREAM
#include "ntp_stdlib.h"
#include <stdio.h>
#else
#include "sys/parsestreams.h"
extern void printf P((const char *, ...));
#endif

static struct format dcf7000_fmt =
{				/* ELV DCF7000 */
	{
		{  6, 2}, {  3, 2}, {  0, 2},
		{ 12, 2}, { 15, 2}, { 18, 2},
		{  9, 2}, { 21, 2},
	},
	(const unsigned char *)"  -  -  -  -  -  -  -  \r",
	0
};    
static u_long cvt_dcf7000 P((unsigned char *, int, struct format *, clocktime_t *, void *));
static unsigned long inp_dcf7000 P((parse_t *, unsigned int, timestamp_t *));

clockformat_t clock_dcf7000 =
{
  inp_dcf7000,			/* DCF7000 input handling */
  cvt_dcf7000,			/* ELV DCF77 conversion */
  0,				/* no direct PPS monitoring */
  (void *)&dcf7000_fmt,		/* conversion configuration */
  "ELV DCF7000",		/* ELV clock */
  24,				/* string buffer */
  0				/* no private data (complete pakets) */
};

/*
 * cvt_dcf7000
 *
 * convert dcf7000 type format
 */
static u_long
cvt_dcf7000(
	    unsigned char *buffer,
	    int            size,
	    struct format *format,
	    clocktime_t   *clock_time,
	    void          *local
	    )
{
	if (!Strok(buffer, format->fixed_string))
	{
		return CVT_NONE;
	}
	else
	{
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
			 format->field_offsets[O_SEC].length))
		{
			return CVT_FAIL|CVT_BADFMT;
		}
		else
		{
			unsigned char *f = &buffer[format->field_offsets[O_FLAGS].offset];
			long flags;
	  
			clock_time->flags = 0;
			clock_time->usecond = 0;

			if (Stoi(f, &flags, format->field_offsets[O_FLAGS].length))
			{
				return CVT_FAIL|CVT_BADFMT;
			}
			else
			{
				if (flags & 0x1)
				    clock_time->utcoffset = -2*60*60;
				else
				    clock_time->utcoffset = -1*60*60;

				if (flags & 0x2)
				    clock_time->flags |= PARSEB_ANNOUNCE;

				if (flags & 0x4)
				    clock_time->flags |= PARSEB_NOSYNC;
			}
			return CVT_OK;
		}
	}
}

/*
 * inp_dcf700
 *
 * grep data from input stream
 */
static u_long
inp_dcf7000(
	  parse_t      *parseio,
	  unsigned int  ch,
	  timestamp_t  *tstamp
	  )
{
	unsigned int rtc;
	
	parseprintf(DD_PARSE, ("inp_dcf7000(0x%lx, 0x%x, ...)\n", (long)parseio, ch));
	
	switch (ch)
	{
	case '\r':
		parseprintf(DD_PARSE, ("inp_dcf7000: EOL seen\n"));
		parseio->parse_dtime.parse_stime = *tstamp; /* collect timestamp */
		if ((rtc = parse_addchar(parseio, ch)) == PARSE_INP_SKIP)
			return parse_end(parseio);
		else
			return rtc;

	default:
		return parse_addchar(parseio, ch);
	}
}

#else /* not (REFCLOCK && CLOCK_PARSE && CLOCK_DCF7000) */
int clk_dcf7000_bs;
#endif /* not (REFCLOCK && CLOCK_PARSE && CLOCK_DCF7000) */

/*
 * History:
 *
 * clk_dcf7000.c,v
 * Revision 4.6  1999/11/28 09:13:49  kardel
 * RECON_4_0_98F
 *
 * Revision 4.5  1998/06/14 21:09:34  kardel
 * Sun acc cleanup
 *
 * Revision 4.4  1998/06/13 12:01:59  kardel
 * fix SYSV clock name clash
 *
 * Revision 4.3  1998/06/12 15:22:27  kardel
 * fix prototypes
 *
 * Revision 4.2  1998/06/12 09:13:24  kardel
 * conditional compile macros fixed
 * printf prototype
 *
 * Revision 4.1  1998/05/24 09:39:51  kardel
 * implementation of the new IO handling model
 *
 * Revision 4.0  1998/04/10 19:45:28  kardel
 * Start 4.0 release version numbering
 *
 * from V3 3.18 log info deleted 1998/04/11 kardel
 */
