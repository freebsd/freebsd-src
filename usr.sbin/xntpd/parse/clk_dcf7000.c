#if defined(REFCLOCK) && (defined(PARSE) || defined(PARSEPPS)) && defined(CLOCK_DCF7000)
/*
 * /src/NTP/REPOSITORY/v3/parse/clk_dcf7000.c,v 3.11 1994/02/02 17:45:14 kardel Exp
 *  
 * clk_dcf7000.c,v 3.11 1994/02/02 17:45:14 kardel Exp
 *
 * ELV DCF7000 module
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

static struct format dcf7000_fmt =
{				/* ELV DCF7000 */
  {
    {  6, 2}, {  3, 2}, {  0, 2},
    { 12, 2}, { 15, 2}, { 18, 2},
    {  9, 2}, { 21, 2},
  },
  "  -  -  -  -  -  -  -  \r",
  0
};    

static unsigned LONG cvt_dcf7000();

clockformat_t clock_dcf7000 =
{
  cvt_dcf7000,			/* ELV DCF77 conversion */
  syn_simple,			/* easy time stamps */
  (unsigned LONG (*)())0,	/* no direct PPS monitoring */
  (unsigned LONG (*)())0,	/* no time code synthesizer monitoring */
  (void *)&dcf7000_fmt,		/* conversion configuration */
  "ELV DCF7000",		/* ELV clock */
  24,				/* string buffer */
  F_END|SYNC_END,		/* END packet delimiter / synchronisation */
  { 0, 0},
  '\0',
  '\r',
  '\0'
};

/*
 * cvt_dcf7000
 *
 * convert dcf7000 type format
 */
static unsigned LONG
cvt_dcf7000(buffer, size, format, clock)
  register char          *buffer;
  register int            size;
  register struct format *format;
  register clocktime_t   *clock;
{
  if (!Strok(buffer, format->fixed_string))
    {
      return CVT_NONE;
    }
  else
    {
      if (Stoi(&buffer[format->field_offsets[O_DAY].offset], &clock->day,
	       format->field_offsets[O_DAY].length) ||
	  Stoi(&buffer[format->field_offsets[O_MONTH].offset], &clock->month,
	       format->field_offsets[O_MONTH].length) ||
	  Stoi(&buffer[format->field_offsets[O_YEAR].offset], &clock->year,
	       format->field_offsets[O_YEAR].length) ||
	  Stoi(&buffer[format->field_offsets[O_HOUR].offset], &clock->hour,
	       format->field_offsets[O_HOUR].length) ||
	  Stoi(&buffer[format->field_offsets[O_MIN].offset], &clock->minute,
	       format->field_offsets[O_MIN].length) ||
	  Stoi(&buffer[format->field_offsets[O_SEC].offset], &clock->second,
	       format->field_offsets[O_SEC].length))
	{
	  return CVT_FAIL|CVT_BADFMT;
	}
      else
	{
	  char *f = &buffer[format->field_offsets[O_FLAGS].offset];
	  LONG flags;
	  
	  clock->flags = 0;
	  clock->usecond = 0;

	  if (Stoi(f, &flags, format->field_offsets[O_FLAGS].length))
	    {
	      return CVT_FAIL|CVT_BADFMT;
	    }
	  else
	    {
	      if (flags & 0x1)
		clock->utcoffset = -2*60*60;
	      else
		clock->utcoffset = -1*60*60;

	      if (flags & 0x2)
		clock->flags |= PARSEB_ANNOUNCE;

	      if (flags & 0x4)
		clock->flags |= PARSEB_NOSYNC;
	    }
	  return CVT_OK;
	}
    }
}
#endif /* defined(PARSE) && defined(CLOCK_DCF7000) */

/*
 * History:
 *
 * clk_dcf7000.c,v
 * Revision 3.11  1994/02/02  17:45:14  kardel
 * rcs ids fixed
 *
 * Revision 3.6  1993/10/09  15:01:27  kardel
 * file structure unified
 *
 * Revision 3.5  1993/10/03  19:10:41  kardel
 * restructured I/O handling
 *
 * Revision 3.4  1993/09/27  21:08:02  kardel
 * utcoffset now in seconds
 *
 * Revision 3.3  1993/09/26  23:40:20  kardel
 * new parse driver logic
 *
 * Revision 3.2  1993/07/09  11:37:15  kardel
 * Initial restructured version + GPS support
 *
 * Revision 3.1  1993/07/06  10:00:14  kardel
 * DCF77 driver goes generic...
 *
 */
