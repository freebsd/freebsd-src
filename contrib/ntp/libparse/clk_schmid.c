/*
 * /src/NTP/ntp-4/libparse/clk_schmid.c,v 4.5 1999/11/28 09:13:51 kardel RELEASE_19991128_A
 *  
 * clk_schmid.c,v 4.5 1999/11/28 09:13:51 kardel RELEASE_19991128_A
 *
 * Schmid clock support
 *
 * Copyright (C) 1992-1998 by Frank Kardel
 * Friedrich-Alexander Universität Erlangen-Nürnberg, Germany
 *                                    
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_PARSE) && defined(CLOCK_SCHMID)

#include <sys/types.h>
#include <sys/time.h>

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

/*
 * Description courtesy of Adam W. Feigin et. al (Swisstime iis.ethz.ch)
 *
 * The command to Schmid's DCF77 clock is a single byte; each bit
 * allows the user to select some part of the time string, as follows (the
 * output for the lsb is sent first).
 * 
 * Bit 0:	time in MEZ, 4 bytes *binary, not BCD*; hh.mm.ss.tenths
 * Bit 1:	date 3 bytes *binary, not BCD: dd.mm.yy
 * Bit 2:	week day, 1 byte (unused here)
 * Bit 3:	time zone, 1 byte, 0=MET, 1=MEST. (unused here)
 * Bit 4:	clock status, 1 byte,	0=time invalid,
 *					1=time from crystal backup,
 *					3=time from DCF77
 * Bit 5:	transmitter status, 1 byte,
 *					bit 0: backup antenna
 *					bit 1: time zone change within 1h
 *					bit 3,2: TZ 01=MEST, 10=MET
 *					bit 4: leap second will be
 *						added within one hour
 *					bits 5-7: Zero
 * Bit 6:	time in backup mode, units of 5 minutes (unused here)
 *
 */
#define WS_TIME		0x01
#define WS_SIGNAL	0x02

#define WS_ALTERNATE	0x01
#define WS_ANNOUNCE	0x02
#define WS_TZ		0x0c
#define   WS_MET	0x08
#define   WS_MEST	0x04
#define WS_LEAP		0x10

static u_long cvt_schmid P((unsigned char *, int, struct format *, clocktime_t *, void *));
static unsigned long inp_schmid P((parse_t *, unsigned int, timestamp_t *));

clockformat_t clock_schmid =
{
  inp_schmid,			/* no input handling */
  cvt_schmid,			/* Schmid conversion */
  0,				/* not direct PPS monitoring */
  0,				/* conversion configuration */
  "Schmid",			/* Schmid receiver */
  12,				/* binary data buffer */
  0,				/* no private data (complete messages) */
};


static u_long
cvt_schmid(
	   unsigned char *buffer,
	   int            size,
	   struct format *format,
	   clocktime_t   *clock_time,
	   void          *local
	)
{
	if ((size != 11) || (buffer[10] != (unsigned char)'\375'))
	{
		return CVT_NONE;
	}
	else
	{
		if (buffer[0] > 23 || buffer[1] > 59 || buffer[2] > 59 || buffer[3] >  9) /* Time */
		{
			return CVT_FAIL|CVT_BADTIME;
		}
		else
		    if (buffer[4] <  1 || buffer[4] > 31 || buffer[5] <  1 || buffer[5] > 12
			||  buffer[6] > 99)
		    {
			    return CVT_FAIL|CVT_BADDATE;
		    }
		    else
		    {
			    clock_time->hour    = buffer[0];
			    clock_time->minute  = buffer[1];
			    clock_time->second  = buffer[2];
			    clock_time->usecond = buffer[3] * 100000;
			    clock_time->day     = buffer[4];
			    clock_time->month   = buffer[5];
			    clock_time->year    = buffer[6];

			    clock_time->flags   = 0;

			    switch (buffer[8] & WS_TZ)
			    {
				case WS_MET:
				    clock_time->utcoffset = -1*60*60;
				    break;

				case WS_MEST:
				    clock_time->utcoffset = -2*60*60;
				    clock_time->flags    |= PARSEB_DST;
				    break;

				default:
				    return CVT_FAIL|CVT_BADFMT;
			    }
	  
			    if (!(buffer[7] & WS_TIME))
			    {
				    clock_time->flags |= PARSEB_POWERUP;
			    }

			    if (!(buffer[7] & WS_SIGNAL))
			    {
				    clock_time->flags |= PARSEB_NOSYNC;
			    }

			    if (buffer[7] & WS_SIGNAL)
			    {
				    if (buffer[8] & WS_ALTERNATE)
				    {
					    clock_time->flags |= PARSEB_ALTERNATE;
				    }

				    if (buffer[8] & WS_ANNOUNCE)
				    {
					    clock_time->flags |= PARSEB_ANNOUNCE;
				    }

				    if (buffer[8] & WS_LEAP)
				    {
					    clock_time->flags |= PARSEB_LEAPADD; /* default: DCF77 data format deficiency */
				    }
			    }

			    clock_time->flags |= PARSEB_S_LEAP|PARSEB_S_ANTENNA;
	  
			    return CVT_OK;
		    }
	}
}

/*
 * inp_schmid
 *
 * grep data from input stream
 */
static u_long
inp_schmid(
	  parse_t      *parseio,
	  unsigned int  ch,
	  timestamp_t  *tstamp
	  )
{
	unsigned int rtc;
	
	parseprintf(DD_PARSE, ("inp_schmid(0x%x, 0x%x, ...)\n", (int)parseio, (int)ch));
	
	switch (ch)
	{
	case 0xFD:		/*  */
		parseprintf(DD_PARSE, ("mbg_input: ETX seen\n"));
		if ((rtc = parse_addchar(parseio, ch)) == PARSE_INP_SKIP)
			return parse_end(parseio);
		else
			return rtc;

	default:
		return parse_addchar(parseio, ch);
	}
}

#else /* not (REFCLOCK && CLOCK_PARSE && CLOCK_SCHMID) */
int clk_schmid_bs;
#endif /* not (REFCLOCK && CLOCK_PARSE && CLOCK_SCHMID) */

/*
 * History:
 *
 * clk_schmid.c,v
 * Revision 4.5  1999/11/28 09:13:51  kardel
 * RECON_4_0_98F
 *
 * Revision 4.4  1998/06/13 12:06:03  kardel
 * fix SYSV clock name clash
 *
 * Revision 4.3  1998/06/12 15:22:29  kardel
 * fix prototypes
 *
 * Revision 4.2  1998/06/12 09:13:26  kardel
 * conditional compile macros fixed
 * printf prototype
 *
 * Revision 4.1  1998/05/24 09:39:53  kardel
 * implementation of the new IO handling model
 *
 * Revision 4.0  1998/04/10 19:45:31  kardel
 * Start 4.0 release version numbering
 *
 * from V3 3.22 log info deleted 1998/04/11 kardel
 */
