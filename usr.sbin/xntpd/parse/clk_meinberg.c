#if defined(REFCLOCK) && (defined(PARSE) || defined(PARSEPPS)) && defined(CLOCK_MEINBERG)
/*
 * /src/NTP/REPOSITORY/v3/parse/clk_meinberg.c,v 3.14 1994/02/20 13:04:37 kardel Exp
 *  
 * clk_meinberg.c,v 3.14 1994/02/20 13:04:37 kardel Exp
 *
 * Meinberg clock support
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
 * The Meinberg receiver every second sends a datagram of the following form
 * (Standard Format)
 * 
 *     <STX>D:<dd>.<mm>.<yy>;T:<w>;U:<hh>:<mm>:<ss>;<S><F><D><A><ETX>
 * pos:  0  00 00 0 00 0 11 111 1 111 12 2 22 2 22 2 2  2  3  3   3
 *       1  23 45 6 78 9 01 234 5 678 90 1 23 4 56 7 8  9  0  1   2
 * <STX>           = '\002' ASCII start of text
 * <ETX>           = '\003' ASCII end of text
 * <dd>,<mm>,<yy>  = day, month, year(2 digits!!)
 * <w>             = day of week (sunday= 0)
 * <hh>,<mm>,<ss>  = hour, minute, second
 * <S>             = '#' if never synced since powerup else ' ' for DCF U/A 31
 *                   '#' if not PZF sychronisation available else ' ' for PZF 535
 * <F>             = '*' if time comes from internal quartz else ' '
 * <D>             = 'S' if daylight saving time is active else ' '
 * <A>             = '!' during the hour preceeding an daylight saving time
 *                       start/end change
 *
 * For the university of Erlangen a special format was implemented to support
 * LEAP announcement and anouncement of alternate antenna.
 *
 * Version for UNI-ERLANGEN Software is: PZFUERL V4.6 (Meinberg)
 *
 * The use of this software release (or higher) is *ABSOLUTELY*
 * recommended (ask for PZFUERL version as some minor HW fixes have
 * been introduced) due to the LEAP second support and UTC indication.
 * The standard timecode does not indicate when the timecode is in
 * UTC (by front panel configuration) thus we have no chance to find
 * the correct utc offset. For the standard format do not ever use
 * UTC display as this is not detectable in the time code !!!
 *
 *     <STX><dd>.<mm>.<yy>; <w>; <hh>:<mm>:<ss>; <U><S><F><D><A><L><R><ETX>
 * pos:  0   00 0 00 0 00 11 1 11 11 1 11 2 22 22 2  2  2  2  2  3  3   3
 *       1   23 4 56 7 89 01 2 34 56 7 89 0 12 34 5  6  7  8  9  0  1   2
 * <STX>           = '\002' ASCII start of text
 * <ETX>           = '\003' ASCII end of text
 * <dd>,<mm>,<yy>  = day, month, year(2 digits!!)
 * <w>             = day of week (sunday= 0)
 * <hh>,<mm>,<ss>  = hour, minute, second
 * <U>             = 'U' UTC time display
 * <S>             = '#' if never synced since powerup else ' ' for DCF U/A 31
 *                   '#' if not PZF sychronisation available else ' ' for PZF 535
 * <F>             = '*' if time comes from internal quartz else ' '
 * <D>             = 'S' if daylight saving time is active else ' '
 * <A>             = '!' during the hour preceeding an daylight saving time
 *                       start/end change
 * <L>             = 'A' LEAP second announcement
 * <R>             = 'R' alternate antenna
 *
 * Meinberg GPS166 receiver
 *
 * You must get the Uni-Erlangen firmware for the GPS receiver support
 * to work to full satisfaction !
 *
 *     <STX><dd>.<mm>.<yy>; <w>; <hh>:<mm>:<ss>; <+/-><00:00>; <U><S><F><D><A><L><R><L>; <position...><ETX>
 *
 *        000000000111111111122222222223333333333444444444455555555556666666
 *        123456789012345678901234567890123456789012345678901234567890123456
 *     \x0209.07.93; 5; 08:48:26; +00:00;        ; 49.5736N  11.0280E  373m\x03
 *
 * 
 * <STX>           = '\002' ASCII start of text
 * <ETX>           = '\003' ASCII end of text
 * <dd>,<mm>,<yy>  = day, month, year(2 digits!!)
 * <w>             = day of week (sunday= 0)
 * <hh>,<mm>,<ss>  = hour, minute, second
 * <+/->,<00:00>   = offset to UTC
 * <S>             = '#' if never synced since powerup else ' ' for DCF U/A 31
 *                   '#' if not PZF sychronisation available else ' ' for PZF 535
 * <U>             = 'U' UTC time display
 * <F>             = '*' if time comes from internal quartz else ' '
 * <D>             = 'S' if daylight saving time is active else ' '
 * <A>             = '!' during the hour preceeding an daylight saving time
 *                       start/end change
 * <L>             = 'A' LEAP second announcement
 * <R>             = 'R' alternate antenna (reminiscent of PZF535) usually ' '
 * <L>		   = 'L' on 23:59:60
 */

static struct format meinberg_fmt[] =
{
  {
    {
      { 3, 2},  {  6, 2}, {  9, 2},
      { 18, 2}, { 21, 2}, { 24, 2},
      { 14, 1}, { 27, 4}, { 29, 1},
    },
    "\2D:  .  .  ;T: ;U:  .  .  ;    \3",
    0
  },
  {				/* special extended FAU Erlangen extended format */
    {
      { 1, 2},  { 4,  2}, {  7, 2},
      { 14, 2}, { 17, 2}, { 20, 2},
      { 11, 1}, { 25, 4}, { 27, 1},
    },
    "\2  .  .  ;  ;   :  :  ;        \3",
    MBG_EXTENDED
  },
  {				/* special extended FAU Erlangen GPS format */
    {
      { 1,  2}, {  4, 2}, {  7, 2},
      { 14, 2}, { 17, 2}, { 20, 2},
      { 11, 1}, { 32, 8}, { 35, 1},
      { 25, 2}, { 28, 2}, { 24, 1}
    },
    "\2  .  .  ;  ;   :  :  ;    :  ;        ;   .         .       ",
    0
  }
};

static unsigned LONG cvt_meinberg();
static unsigned LONG cvt_mgps();

clockformat_t clock_meinberg[] =
{
  {
    cvt_meinberg,		/* Meinberg conversion */
    syn_simple,			/* easy time stamps for RS232 (fallback) */
    pps_simple,			/* easy PPS monitoring */
    (unsigned LONG (*)())0,	/* no time code synthesizer monitoring */
    (void *)&meinberg_fmt[0],	/* conversion configuration */
    "Meinberg Standard",	/* Meinberg simple format - beware */
    32,				/* string buffer */
    F_START|F_END|SYNC_START|SYNC_ONE, /* paket START/END delimiter, START synchronisation, PPS ONE sampling */
    { 0, 0},
    '\2',
    '\3',
    '\0'
  },
  {
    cvt_meinberg,		/* Meinberg conversion */
    syn_simple,			/* easy time stamps for RS232 (fallback) */
    pps_simple,			/* easy PPS monitoring */
    (unsigned LONG (*)())0,	/* no time code synthesizer monitoring */
    (void *)&meinberg_fmt[1],	/* conversion configuration */
    "Meinberg Extended",	/* Meinberg enhanced format */
    32,				/* string buffer */
    F_START|F_END|SYNC_START|SYNC_ONE,	/* paket START/END delimiter, START synchronisation, PPS ONE sampling  */
    { 0, 0},
    '\2',
    '\3',
    '\0'
  },
  {
    cvt_mgps,			/* Meinberg GPS166 conversion */
    syn_simple,			/* easy time stamps for RS232 (fallback) */
    pps_simple,			/* easy PPS monitoring */
    (unsigned LONG (*)())0,	/* no time code synthesizer monitoring */
    (void *)&meinberg_fmt[2],	/* conversion configuration */
    "Meinberg GPS Extended",	/* Meinberg FAU GPS format */
    70,				/* string buffer */
    F_START|F_END|SYNC_START|SYNC_ONE,	/* paket START/END delimiter, START synchronisation, PPS ONE sampling  */
    { 0, 0},
    '\2',
    '\3',
    '\0'
  }
};

/*
 * cvt_meinberg
 *
 * convert simple type format
 */
static unsigned LONG
cvt_meinberg(buffer, size, format, clock)
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
	  
	  clock->flags = 0;
	  clock->usecond = 0;

	  /*
	   * in the extended timecode format we have also the
	   * indication that the timecode is in UTC
	   * for compatibilty reasons we start at the USUAL
	   * offset (POWERUP flag) and know that the UTC indication
	   * is the character before the powerup flag
	   */
	  if ((format->flags & MBG_EXTENDED) && (f[-1] == 'U'))
	    {
	      /*
	       * timecode is in UTC
	       */
	      clock->utcoffset = 0; /* UTC */
	      clock->flags    |= PARSEB_UTC;
	    }
	  else
	    {
	      /*
	       * only calculate UTC offset if MET/MED is in time code
	       * or we have the old time code format, where we do not
	       * know whether it is UTC time or MET/MED
	       * pray that nobody switches to UTC in the standard time code
	       * ROMS !!!!
	       */
	      switch (buffer[format->field_offsets[O_ZONE].offset])
		{
		case ' ':
		  clock->utcoffset = -1*60*60; /* MET */
		  break;
		  
		case 'S':
		  clock->utcoffset = -2*60*60; /* MED */
		  break;
		  
		default:
		  return CVT_FAIL|CVT_BADFMT;
		}
	    }
	  
	  /*
	   * gather status flags
	   */
	  if (buffer[format->field_offsets[O_ZONE].offset] == 'S')
	    clock->flags    |= PARSEB_DST;
	  
	  if (f[0] == '#')
	    clock->flags |= PARSEB_POWERUP;

	  if (f[1] == '*')
	    clock->flags |= PARSEB_NOSYNC;

	  if (f[3] == '!')
	    clock->flags |= PARSEB_ANNOUNCE;
	  
	  if (format->flags & MBG_EXTENDED)
	    {
	      clock->flags |= PARSEB_S_LEAP;
	      clock->flags |= PARSEB_S_ANTENNA;
	      
	      /*
	       * DCF77 does not encode the direction -
	       * so we take the current default -
	       * earth slowing down
	       */
	      if (f[4] == 'A')
		clock->flags |= PARSEB_LEAPADD;

	      if (f[5] == 'R')
		clock->flags |= PARSEB_ALTERNATE;
	    }
	  return CVT_OK;
	}
    }
}

/*
 * cvt_mgps
 *
 * convert Meinberg GPS format
 */
static unsigned LONG
cvt_mgps(buffer, size, format, clock)
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
	  LONG h;
	  char *f = &buffer[format->field_offsets[O_FLAGS].offset];
	  
	  clock->flags = PARSEB_S_LEAP|PARSEB_S_POSITION;
	      
	  clock->usecond = 0;

	  /*
	   * calculate UTC offset
	   */
	  if (Stoi(&buffer[format->field_offsets[O_UTCHOFFSET].offset], &h,
	       format->field_offsets[O_UTCHOFFSET].length))
	    {
	      return CVT_FAIL|CVT_BADFMT;
	    }
	  else
	    {
	      if (Stoi(&buffer[format->field_offsets[O_UTCMOFFSET].offset], &clock->utcoffset,
		       format->field_offsets[O_UTCMOFFSET].length))
		{
		  return CVT_FAIL|CVT_BADFMT;
		}

	      clock->utcoffset += TIMES60(h);

	      if (buffer[format->field_offsets[O_UTCSOFFSET].offset] != '-')
		{
		  clock->utcoffset = -clock->utcoffset;
		}
	    }
	  
	  /*
	   * gather status flags
	   */
	  if (buffer[format->field_offsets[O_ZONE].offset] == 'S')
	    clock->flags    |= PARSEB_DST;
	  
	  if ((f[0] == 'U') ||
	      (clock->utcoffset == 0))
	    clock->flags |= PARSEB_UTC;
	  
	  /*
	   * no sv's seen - no time & position
	   */
	  if (f[1] == '#')
	    clock->flags |= PARSEB_POWERUP;
	  
	  /*
	   * at least one sv seen - time (for last position)
	   */
	  if (f[2] == '*')
	    clock->flags |= PARSEB_NOSYNC;
	  else
	    if (!(clock->flags & PARSEB_POWERUP))
	      clock->flags |= PARSEB_POSITION;
	  
	  /*
	   * oncoming zone switch
	   */
	  if (f[4] == '!')
	    clock->flags |= PARSEB_ANNOUNCE;
	  
	  /*
	   * oncoming leap second
	   * data format does not (yet) specify whether
	   * to add or to delete a second - thus we
	   * pick the current default
	   */
	  if (f[5] == 'A')
	    clock->flags |= PARSEB_LEAPADD;
	  
	  /*
	   * this is the leap second
	   */
	  if (f[7] == 'L')
	    clock->flags |= PARSEB_LEAPSECOND;

	  return CVT_OK;
	}
    }
}
#endif /* defined(PARSE) && defined(CLOCK_MEINBERG) */

/*
 * History:
 *
 * clk_meinberg.c,v
 * Revision 3.14  1994/02/20  13:04:37  kardel
 * parse add/delete second support
 *
 * Revision 3.13  1994/02/02  17:45:21  kardel
 * rcs ids fixed
 *
 * Revision 3.11  1994/01/25  19:05:10  kardel
 * 94/01/23 reconcilation
 *
 * Revision 3.10  1994/01/23  17:21:54  kardel
 * 1994 reconcilation
 *
 * Revision 3.9  1993/10/30  09:44:38  kardel
 * conditional compilation flag cleanup
 *
 * Revision 3.8  1993/10/22  14:27:48  kardel
 * Oct. 22nd 1993 reconcilation
 *
 * Revision 3.7  1993/10/09  15:01:30  kardel
 * file structure unified
 *
 * Revision 3.6  1993/10/03  19:10:43  kardel
 * restructured I/O handling
 *
 * Revision 3.5  1993/09/27  21:08:04  kardel
 * utcoffset now in seconds
 *
 * Revision 3.4  1993/09/26  23:40:22  kardel
 * new parse driver logic
 *
 * Revision 3.3  1993/08/18  09:29:32  kardel
 * GPS format is somewhat variable length - variable length part holds position
 *
 * Revision 3.2  1993/07/09  11:37:16  kardel
 * Initial restructured version + GPS support
 *
 * Revision 3.1  1993/07/06  10:00:17  kardel
 * DCF77 driver goes generic...
 *
 */
