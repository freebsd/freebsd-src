#if defined(REFCLOCK) && (defined(PARSE) || defined(PARSEPPS)) && defined(CLOCK_SCHMID)
/*
 * /src/NTP/REPOSITORY/v3/parse/clk_schmid.c,v 3.10 1994/01/25 19:05:15 kardel Exp
 *  
 * clk_schmid.c,v 3.10 1994/01/25 19:05:15 kardel Exp
 *
 * Schmid clock support
 *
 * Copyright (c) 1992,1993,1994
 * Frank Kardel Friedrich-Alexander Universitaet Erlangen-Nuernberg
 *                                    
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#include "sys/types.h"
#include "sys/time.h"
#include "sys/errno.h"
#include "ntp_fp.h"
#include "ntp_unixtime.h"
#include "ntp_calendar.h"

#include "parse.h"

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

static unsigned LONG cvt_schmid();

clockformat_t clock_schmid =
{
  cvt_schmid,			/* Schmid conversion */
  syn_simple,			/* easy time stamps */
  (unsigned LONG (*)())0,	/* not direct PPS monitoring */
  (unsigned LONG (*)())0,	/* no time code synthesizer monitoring */
  (void *)0,			/* conversion configuration */
  "Schmid",			/* Schmid receiver */
  12,				/* binary data buffer */
  F_END|SYNC_START,		/* END packet delimiter / synchronisation */
  { 0, 0},
  '\0',
  (unsigned char)'\375',
  '\0'
};


static unsigned LONG
cvt_schmid(buffer, size, format, clock)
  register char          *buffer;
  register int            size;
  register struct format *format;
  register clocktime_t   *clock;
{
  if ((size != 11) || (buffer[10] != '\375'))
    {
      return CVT_NONE;
    }
  else
    {
      if (buffer[0] > 23 || buffer[1] > 59 || buffer[2] > 59 || buffer[3] >  9 /* Time */
	  ||  buffer[0] <  0 || buffer[1] <  0 || buffer[2] <  0 || buffer[3] <  0)
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
	    clock->hour    = buffer[0];
	    clock->minute  = buffer[1];
	    clock->second  = buffer[2];
	    clock->usecond = buffer[3] * 100000;
	    clock->day     = buffer[4];
	    clock->month   = buffer[5];
	    clock->year    = buffer[6];

	    clock->flags   = 0;

	    switch (buffer[8] & WS_TZ)
	      {
	      case WS_MET:
		clock->utcoffset = -1*60*60;
		break;

	      case WS_MEST:
		clock->utcoffset = -2*60*60;
		clock->flags    |= PARSEB_DST;
		break;

	      default:
		return CVT_FAIL|CVT_BADFMT;
	      }
	  
	    if (!(buffer[7] & WS_TIME))
	      {
		clock->flags |= PARSEB_POWERUP;
	      }

	    if (!(buffer[7] & WS_SIGNAL))
	      {
		clock->flags |= PARSEB_NOSYNC;
	      }

	    if (buffer[7] & WS_SIGNAL)
	      {
		if (buffer[8] & WS_ALTERNATE)
		  {
		    clock->flags |= PARSEB_ALTERNATE;
		  }

		if (buffer[8] & WS_ANNOUNCE)
		  {
		    clock->flags |= PARSEB_ANNOUNCE;
		  }

		if (buffer[8] & WS_LEAP)
		  {
		    clock->flags |= PARSEB_LEAP;
		  }
	      }

	    clock->flags |= PARSEB_S_LEAP|PARSEB_S_ANTENNA;
	  
	    return CVT_OK;
	  }
    }
}
#endif /* defined(PARSE) && defined(CLOCK_SCHMID) */

/*
 * History:
 *
 * clk_schmid.c,v
 * Revision 3.10  1994/01/25  19:05:15  kardel
 * 94/01/23 reconcilation
 *
 * Revision 3.9  1994/01/23  17:21:56  kardel
 * 1994 reconcilation
 *
 * Revision 3.8  1993/11/01  20:00:18  kardel
 * parse Solaris support (initial version)
 *
 * Revision 3.7  1993/10/30  09:44:43  kardel
 * conditional compilation flag cleanup
 *
 * Revision 3.6  1993/10/09  15:01:32  kardel
 * file structure unified
 *
 * Revision 3.5  1993/10/03  19:10:47  kardel
 * restructured I/O handling
 *
 * Revision 3.4  1993/09/27  21:08:09  kardel
 * utcoffset now in seconds
 *
 * Revision 3.3  1993/09/26  23:40:27  kardel
 * new parse driver logic
 *
 * Revision 3.2  1993/07/09  11:37:19  kardel
 * Initial restructured version + GPS support
 *
 * Revision 3.1  1993/07/06  10:00:22  kardel
 * DCF77 driver goes generic...
 *
 */
