#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_PARSE) && defined(CLOCK_VARITEXT)
/*
 * clk_varitext.c,v 1.0 1997/01/19 A.McConnell
 * 
 * Supports Varitext's Radio Clock
 * 
 * Used the Meinberg/Computime clock as a template for Varitext Radio Clock
 *
 * Copyright (C) 1992-1996 by Frank Kardel
 * Friedrich-Alexander Universitt Erlangen-Nrnberg, Germany
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 * 
 */

#include "ntp_fp.h"
#include "ntp_unixtime.h"
#include "ntp_calendar.h"

#include "parse.h"

#ifndef PARSESTREAM
# include "ntp_stdlib.h"
# include <stdio.h>
#else
# include "sys/parsestreams.h"
extern void printf P((const char *, ...));
#endif

static const u_char VT_INITIALISED      = 0x01;
static const u_char VT_SYNCHRONISED     = 0x02;
static const u_char VT_ALARM_STATE      = 0x04;
static const u_char VT_BST              = 0x08;
static const u_char VT_SEASON_CHANGE    = 0x10;
static const u_char VT_LAST_TELEGRAM_OK = 0x20;

/*
 * The Varitext receiver sends a datagram in the following format every minute
 * 
 * Timestamp	T:YY:MM:MD:WD:HH:MM:SSCRLFSTXXX 
 * Pos          0123456789012345678901 2 3 4567
 *              0000000000111111111122 2 2 2222
 * Parse        T:  :  :  :  :  :  :  \r\n    
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
 * ST	Status character
 *	Bit 0 - Set= Initialised; Reset=Time Invalid (DO NOT USE)
 *	Bit 1 - Set= Synchronised; Reset= Unsynchronised 
 * 	Bit 2 - Set= Alarm state; Reset= No alarm
 * 	Bit 3 - Set= BST; Reset= GMT
 * 	Bit 4 - Set= Seasonal change in approx hour; Reset= No seasonal change expected
 *	Bit 5 - Set= Last MSF telegram was OK; Reset= Last telegram was in error;
 * 	Bit 6 - Always set
 *	Bit 7 - Unused
 * XXX	Checksum calculated using Fletcher's method (ignored for now). 
 */

static struct format varitext_fmt =
{
  {
    {8, 2},  {5,  2}, {2,  2},	/* day, month, year */
    {14, 2}, {17, 2}, {20, 2},	/* hour, minute, second */
    {11, 2}, {24, 1}		/* dayofweek, status */
  },
  (const unsigned char*)"T:  :  :  :  :  :  :  \r\n    ",
  0
};

static u_long   cvt_varitext P((unsigned char *, int, struct format *, clocktime_t *, void *));
static u_long   inp_varitext P((parse_t *, unsigned int, timestamp_t *));

struct varitext {
  unsigned char start_found;
  unsigned char end_found;
  unsigned char end_count;
  unsigned char previous_ch;
  timestamp_t   tstamp;
};

clockformat_t   clock_varitext =
{
  inp_varitext,			/* Because of the strange format we need to parse it ourselves */
  cvt_varitext,			/* Varitext conversion */
  0,				/* no PPS monitoring */
  (void *)&varitext_fmt,	/* conversion configuration */
  "Varitext Radio Clock",	/* Varitext Radio Clock */
  30,				/* string buffer */
  sizeof(struct varitext),	/* Private data size required to hold current parse state */
};

/*
 * cvt_varitext
 * 
 * convert simple type format
 */
static          u_long
cvt_varitext(
	     unsigned char	*buffer,
	     int    		size,
	     struct format	*format,
	     clocktime_t	*clock_time,
	     void		*local
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
      u_char *f = (u_char*) &buffer[format->field_offsets[O_FLAGS].offset];

      clock_time->flags = 0;
      clock_time->utcoffset = 0;

      if (((*f) & VT_BST))	/* BST flag is set so set to indicate daylight saving time is active and utc offset */
	{
	  clock_time->utcoffset = -1*60*60;
	  clock_time->flags |= PARSEB_DST;
	}
      /*
	 if (!((*f) & VT_INITIALISED))  Clock not initialised 
	 clock_time->flags |= PARSEB_POWERUP;
	 
	 if (!((*f) & VT_SYNCHRONISED))   Clock not synchronised 
	 clock_time->flags |= PARSEB_NOSYNC;
	 
	 if (((*f) & VT_SEASON_CHANGE))  Seasonal change expected in the next hour 
	 clock_time->flags |= PARSEB_ANNOUNCE;
	 */
      return CVT_OK; 
    }
  }
}

static u_long 
inp_varitext(
	     parse_t	 *parseio,
	     unsigned int ch,
	     timestamp_t *tstamp
	     )
{
  struct varitext *t = (struct varitext *)parseio->parse_pdata;
  int    rtc;

  parseprintf(DD_PARSE, ("inp_varitext(0x%lx, 0x%x, ...)\n", (long)parseio, ch));

  if (!t) 
    return PARSE_INP_SKIP;	/* local data not allocated - sigh! */

  if (ch == 'T') 
    t->tstamp = *tstamp;

  if ((t->previous_ch == 'T') && (ch == ':'))
    {
      parseprintf(DD_PARSE, ("inp_varitext: START seen\n"));

      parseio->parse_data[0] = 'T';
      parseio->parse_index=1;
      parseio->parse_dtime.parse_stime = t->tstamp; /* Time stamp at packet start */
      t->start_found = 1;
      t->end_found = 0;
      t->end_count = 0;
    }

  if (t->start_found)
    {
      if ((rtc = parse_addchar(parseio, ch)) != PARSE_INP_SKIP)
	{
	  parseprintf(DD_PARSE, ("inp_varitext: ABORTED due to too many characters\n"));

	  memset(t, 0, sizeof(struct varitext));
	  return rtc;
	}

      if (t->end_found) 
	{
	  if (++(t->end_count) == 4) /* Finally found the end of the message */
	    {
	      parseprintf(DD_PARSE, ("inp_varitext: END seen\n"));

	      memset(t, 0, sizeof(struct varitext));
	      if ((rtc = parse_addchar(parseio, 0)) == PARSE_INP_SKIP)
		return parse_end(parseio);
	      else
		return rtc;
	    }	
	}

      if ((t->previous_ch == '\r') && (ch == '\n')) 
	{
	  t->end_found = 1;
	}

    }
 
  t->previous_ch = ch;

  return PARSE_INP_SKIP;
}

#else /* not (REFCLOCK && CLOCK_PARSE && CLOCK_VARITEXT) */
int clk_varitext_bs;
#endif /* not (REFCLOCK && CLOCK_PARSE && CLOCK_VARITEXT) */

/*
 * Revision 1.0  1997/06/02 13:16:30  McConnell
 * File created
 *
 */
