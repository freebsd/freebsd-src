#if defined(REFCLOCK) && (defined(PARSE) || defined(PARSEPPS)) && defined(CLOCK_RAWDCF)
/*
 * /src/NTP/REPOSITORY/v3/parse/clk_rawdcf.c,v 3.9 1994/01/25 19:05:12 kardel Exp
 *  
 * clk_rawdcf.c,v 3.9 1994/01/25 19:05:12 kardel Exp
 *
 * Raw DCF77 pulse clock support
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
#ifdef PARSESTREAM
#include "sys/parsestreams.h"
#endif

#ifndef PARSEKERNEL
#include "ntp_stdlib.h"
#endif

/*
 * DCF77 raw time code
 *
 * From "Zur Zeit", Physikalisch-Technische Bundesanstalt (PTB), Braunschweig
 * und Berlin, Maerz 1989
 *
 * Timecode transmission:
 * AM:
 *	time marks are send every second except for the second before the
 *	next minute mark
 *	time marks consist of a reduction of transmitter power to 25%
 *	of the nominal level
 *	the falling edge is the time indication (on time)
 *	time marks of a 100ms duration constitute a logical 0
 *	time marks of a 200ms duration constitute a logical 1
 * FM:
 *	see the spec. (basically a (non-)inverted psuedo random phase shift)
 *
 * Encoding:
 * Second	Contents
 * 0  - 10	AM: free, FM: 0
 * 11 - 14	free
 * 15		R     - alternate antenna
 * 16		A1    - expect zone change (1 hour before)
 * 17 - 18	Z1,Z2 - time zone
 *		 0  0 illegal
 *		 0  1 MEZ  (MET)
 *		 1  0 MESZ (MED, MET DST)
 *		 1  1 illegal
 * 19		A2    - expect leap insertion/deletion (1 hour before)
 * 20		S     - start of time code (1)
 * 21 - 24	M1    - BCD (lsb first) Minutes
 * 25 - 27	M10   - BCD (lsb first) 10 Minutes
 * 28		P1    - Minute Parity (even)
 * 29 - 32	H1    - BCD (lsb first) Hours
 * 33 - 34      H10   - BCD (lsb first) 10 Hours
 * 35		P2    - Hour Parity (even)
 * 36 - 39	D1    - BCD (lsb first) Days
 * 40 - 41	D10   - BCD (lsb first) 10 Days
 * 42 - 44	DW    - BCD (lsb first) day of week (1: Monday -> 7: Sunday)
 * 45 - 49	MO    - BCD (lsb first) Month
 * 50           MO0   - 10 Months
 * 51 - 53	Y1    - BCD (lsb first) Years
 * 54 - 57	Y10   - BCD (lsb first) 10 Years
 * 58 		P3    - Date Parity (even)
 * 59		      - usually missing (minute indication), except for leap insertion
 */

static unsigned LONG cvt_rawdcf();
static unsigned LONG pps_rawdcf();
static unsigned LONG snt_rawdcf();

clockformat_t clock_rawdcf =
{
  cvt_rawdcf,			/* raw dcf input conversion */
  (void (*)())0,		/* no character bound synchronisation */
  pps_rawdcf,			/* examining PPS information */
  snt_rawdcf,			/* synthesize time code from input */
  (void *)0,			/* buffer bit representation */
  "RAW DCF77 Timecode",		/* direct decoding / time synthesis */
  61,				/* bit buffer */
  SYNC_ONE|SYNC_ZERO|SYNC_TIMEOUT|SYNC_SYNTHESIZE|CVT_FIXEDONLY,
  /* catch all transitions, buffer restart on timeout, fixed configuration only */
  { 1, 500000},			/* restart after 1.5 seconds */
  '\0',
  '\0',
  '\0'
};

static struct dcfparam
{
  unsigned char onebits[60];
  unsigned char zerobits[60];
} dcfparam = 
{
  "###############RADMLS1248124P124812P1248121241248112481248P", /* 'ONE' representation */
  "--------------------s-------p------p----------------------p"  /* 'ZERO' representation */
};

static struct rawdcfcode 
{
  char offset;			/* start bit */
} rawdcfcode[] =
{
  {  0 }, { 15 }, { 16 }, { 17 }, { 19 }, { 20 }, { 21 }, { 25 }, { 28 }, { 29 },
  { 33 }, { 35 }, { 36 }, { 40 }, { 42 }, { 45 }, { 49 }, { 50 }, { 54 }, { 58 }, { 59 }
};

#define DCF_M	0
#define DCF_R	1
#define DCF_A1	2
#define DCF_Z	3
#define DCF_A2	4
#define DCF_S	5
#define DCF_M1	6
#define DCF_M10	7
#define DCF_P1	8
#define DCF_H1	9
#define DCF_H10	10
#define DCF_P2	11
#define DCF_D1	12
#define DCF_D10	13
#define DCF_DW	14
#define DCF_MO	15
#define DCF_MO0	16
#define DCF_Y1	17
#define DCF_Y10	18
#define DCF_P3	19

static struct partab
{
  char offset;			/* start bit of parity field */
} partab[] =
{
  { 21 }, { 29 }, { 36 }, { 59 }
};

#define DCF_P_P1	0
#define DCF_P_P2	1
#define DCF_P_P3	2

#define DCF_Z_MET 0x2
#define DCF_Z_MED 0x1

static unsigned LONG ext_bf(buf, idx, zero)
  register char *buf;
  register int   idx;
  register char *zero;
{
  register unsigned LONG sum = 0;
  register int i, first;

  first = rawdcfcode[idx].offset;
  
  for (i = rawdcfcode[idx+1].offset - 1; i >= first; i--)
    {
      sum <<= 1;
      sum |= (buf[i] != zero[i]);
    }
  return sum;
}

static unsigned pcheck(buf, idx, zero)
  register char *buf;
  register int   idx;
  register char *zero;
{
  register int i,last;
  register unsigned psum = 1;

  last = partab[idx+1].offset;

  for (i = partab[idx].offset; i < last; i++)
    psum ^= (buf[i] != zero[i]);

  return psum;
}

static unsigned LONG convert_rawdcf(buffer, size, dcfparam, clock)
  register unsigned char   *buffer;
  register int              size;
  register struct dcfparam *dcfparam;
  register clocktime_t     *clock;
{
  register unsigned char *s = buffer;
  register unsigned char *b = dcfparam->onebits;
  register unsigned char *c = dcfparam->zerobits;
  register int i;

  parseprintf(DD_RAWDCF,("parse: convert_rawdcf: \"%s\"\n", buffer));

  if (size < 57)
    {
#ifdef PARSEKERNEL
          printf("parse: convert_rawdcf: INCOMPLETE DATA - time code only has %d bits\n", size);
#else
          syslog(LOG_ERR, "parse: convert_rawdcf: INCOMPLETE DATA - time code only has %d bits\n", size);
#endif
	  return CVT_NONE;
    }
  
  for (i = 0; i < 58; i++)
    {
      if ((*s != *b) && (*s != *c))
	{
	  /*
	   * we only have two types of bytes (ones and zeros)
	   */
#ifdef PARSEKERNEL
          printf("parse: convert_rawdcf: BAD DATA - no conversion for \"%s\"\n", buffer);
#else
          syslog(LOG_ERR, "parse: convert_rawdcf: BAD DATA - no conversion for \"%s\"\n", buffer);
#endif
	  return CVT_NONE;
	}
      b++;
      c++;
      s++;
    }
  
  /*
   * check Start and Parity bits
   */
  if ((ext_bf(buffer, DCF_S, dcfparam->zerobits) == 1) &&
      pcheck(buffer, DCF_P_P1, dcfparam->zerobits) &&
      pcheck(buffer, DCF_P_P2, dcfparam->zerobits) &&
      pcheck(buffer, DCF_P_P3, dcfparam->zerobits))
    {
      /*
       * buffer OK
       */
      parseprintf(DD_RAWDCF,("parse: convert_rawdcf: parity check passed\n"));

      clock->flags  = PARSEB_S_ANTENNA|PARSEB_S_LEAP;
      clock->usecond= 0;
      clock->second = 0;
      clock->minute = ext_bf(buffer, DCF_M10, dcfparam->zerobits);
      clock->minute = TIMES10(clock->minute) + ext_bf(buffer, DCF_M1, dcfparam->zerobits);
      clock->hour   = ext_bf(buffer, DCF_H10, dcfparam->zerobits);
      clock->hour   = TIMES10(clock->hour) + ext_bf(buffer, DCF_H1, dcfparam->zerobits);
      clock->day    = ext_bf(buffer, DCF_D10, dcfparam->zerobits);
      clock->day    = TIMES10(clock->day) + ext_bf(buffer, DCF_D1, dcfparam->zerobits);
      clock->month  = ext_bf(buffer, DCF_MO0, dcfparam->zerobits);
      clock->month  = TIMES10(clock->month) + ext_bf(buffer, DCF_MO, dcfparam->zerobits);
      clock->year   = ext_bf(buffer, DCF_Y10, dcfparam->zerobits);
      clock->year   = TIMES10(clock->year) + ext_bf(buffer, DCF_Y1, dcfparam->zerobits);

      switch (ext_bf(buffer, DCF_Z, dcfparam->zerobits))
	{
	case DCF_Z_MET:
	  clock->utcoffset = -1*60*60;
	  break;

	case DCF_Z_MED:
	  clock->flags     |= PARSEB_DST;
	  clock->utcoffset  = -2*60*60;
	  break;

	default:
          parseprintf(DD_RAWDCF,("parse: convert_rawdcf: BAD TIME ZONE\n"));
	  return CVT_FAIL|CVT_BADFMT;
	}

      if (ext_bf(buffer, DCF_A1, dcfparam->zerobits))
	clock->flags |= PARSEB_ANNOUNCE;

      if (ext_bf(buffer, DCF_A2, dcfparam->zerobits))
	clock->flags |= PARSEB_LEAP;

      if (ext_bf(buffer, DCF_R, dcfparam->zerobits))
	clock->flags |= PARSEB_ALTERNATE;

      parseprintf(DD_RAWDCF,("parse: convert_rawdcf: TIME CODE OK: %d:%d, %d.%d.%d, flags 0x%x\n",
			  clock->hour, clock->minute, clock->day, clock->month, clock->year,
			  clock->flags));
      return CVT_OK;
    }
  else
    {
      /*
       * bad format - not for us
       */
#ifdef PARSEKERNEL
      printf("parse: convert_rawdcf: parity check FAILED for \"%s\"\n", buffer);
#else
      syslog(LOG_ERR, "parse: convert_rawdcf: parity check FAILED for \"%s\"\n", buffer);
#endif
      return CVT_FAIL|CVT_BADFMT;
    }
}

/*
 * raw dcf input routine - needs to fix up 50 baud
 * characters for 1/0 decision
 */
static unsigned LONG cvt_rawdcf(buffer, size, param, clock)
  register unsigned char   *buffer;
  register int              size;
  register void            *param;
  register clocktime_t     *clock;
{
  register unsigned char *s = buffer;
  register unsigned char *e = buffer + size;
  register unsigned char *b = dcfparam.onebits;
  register unsigned char *c = dcfparam.zerobits;
  register unsigned rtc = CVT_NONE;
  register unsigned int i, lowmax, highmax, cutoff, span;
#define BITS 9
  unsigned char     histbuf[BITS];
  /*
   * the input buffer contains characters with runs of consecutive
   * bits set. These set bits are an indication of the DCF77 pulse
   * length. We assume that we receive the pulse at 50 Baud. Thus
   * a 100ms pulse would generate a 4 bit train (20ms per bit and
   * start bit)
   * a 200ms pulse would create all zeroes (and probably a frame error)
   */

  for (i = 0; i < BITS; i++)
    {
      histbuf[i] = 0;
    }

  cutoff = 0;
  lowmax = 0;

  while (s < e)
    {
      register unsigned int ch = *s ^ 0xFF;
      /*
       * these lines are left as an excercise to the reader 8-)
       */
      if (!((ch+1) & ch) || !*s)
	{

	  for (i = 0; ch; i++)
	    {
	      ch >>= 1;
	    }

	  *s = i;
	  histbuf[i]++;
	  cutoff += i;
	  lowmax++;
	}
      else
	{
	  parseprintf(DD_RAWDCF,("parse: cvt_rawdcf: character check for 0x%x@%d FAILED\n", *s, s - buffer));
	  *s = ~0;
	  rtc = CVT_FAIL|CVT_BADFMT;
	}
      s++;
    }

  if (lowmax)
    {
      cutoff /= lowmax;
    }
  else
    {
      cutoff = 4;	/* doesn't really matter - it'll fail anyway, but gives error output */
    }

  parseprintf(DD_RAWDCF,("parse: cvt_rawdcf: average bit count: %d\n", cutoff));

  lowmax = 0;
  highmax = 0;

  parseprintf(DD_RAWDCF,("parse: cvt_rawdcf: histogram:"));
  for (i = 0; i <= cutoff; i++)
    {
      lowmax+=histbuf[i] * i;
      highmax += histbuf[i];
      parseprintf(DD_RAWDCF,(" %d", histbuf[i]));
    }
  parseprintf(DD_RAWDCF, (" <M>"));

  lowmax += highmax / 2;

  if (highmax)
    {
      lowmax /= highmax;
    }
  else
    {
      lowmax = 0;
    }

  highmax = 0;
  cutoff = 0;

  for (; i < BITS; i++)
    {
      highmax+=histbuf[i] * i;
      cutoff +=histbuf[i];
      parseprintf(DD_RAWDCF,(" %d", histbuf[i]));
    }
  parseprintf(DD_RAWDCF,("\n"));

  if (cutoff)
    {
      highmax /= cutoff;
    }
  else
    {
      highmax = BITS-1;
    }

  span = cutoff = lowmax;
  for (i = lowmax; i <= highmax; i++)
    {
      if (histbuf[cutoff] > histbuf[i])
	{
	  cutoff = i;
	  span = i;
	}
      else
        if (histbuf[cutoff] == histbuf[i])
	  {
	    span = i;
	  }
    }

  cutoff = (cutoff + span) / 2;

  parseprintf(DD_RAWDCF,("parse: cvt_rawdcf: lower maximum %d, higher maximum %d, cutoff %d\n", lowmax, highmax, cutoff));

  s = buffer;
  while ((s < e) && *c && *b)
    {
      if (*s == (unsigned char)~0)
	{
	  *s = '?';
	}
      else
	{
	  *s = (*s >= cutoff) ? *b : *c;
	}
      s++;
      b++;
      c++;
    }

  return (rtc == CVT_NONE) ? convert_rawdcf(buffer, size, &dcfparam, clock) : rtc;
}

/*
 * pps_rawdcf
 *
 * currently a very stupid version - should be extended to decode
 * also ones and zeros (which is easy)
 */
/*ARGSUSED*/
static unsigned LONG pps_rawdcf(parseio, status, ptime)
  register parse_t *parseio;
  register int status;
  register timestamp_t *ptime;
{
  if (!status)
    {
      parseio->parse_dtime.parse_ptime  = *ptime;
      parseio->parse_dtime.parse_state |= PARSEB_PPS|PARSEB_S_PPS;
    }

  return CVT_NONE;
}

/*ARGSUSED*/
static unsigned LONG snt_rawdcf(parseio, ptime)
  register parse_t *parseio;
  register timestamp_t *ptime;
{
  clocktime_t clock;
  unsigned LONG cvtrtc;
  time_t t;
  
  /*
   * start at last sample and add second index - gross, may have to be much more careful
   */
  if (convert_rawdcf(parseio->parse_ldata, parseio->parse_ldsize - 1, &dcfparam, &clock) == CVT_OK)
    {
      if ((t = parse_to_unixtime(&clock, &cvtrtc)) == -1)
	{
	  parseprintf(DD_RAWDCF,("parse: snt_rawdcf: time conversion FAILED\n"));
	  return CVT_FAIL|cvtrtc;
	}
    }
  else
    {
      parseprintf(DD_RAWDCF,("parse: snt_rawdcf: data conversion FAILED\n"));
      return CVT_NONE;
    }

  parseio->parse_dtime.parse_stime = *ptime;

  t += parseio->parse_index - 1;

  /*
   * time stamp
   */
#ifdef PARSEKERNEL
  parseio->parse_dtime.parse_time.tv.tv_sec  = t;
  parseio->parse_dtime.parse_time.tv.tv_usec = clock.usecond;
#else
  parseio->parse_dtime.parse_time.fp.l_ui  = t + JAN_1970;
  TVUTOTSF(clock.usecond, parseio->parse_dtime.parse_time.fp.l_uf);
#endif

  parseprintf(DD_RAWDCF,("parse: snt_rawdcf: time stamp synthesized offset %d seconds\n", parseio->parse_index - 1));

  return updatetimeinfo(parseio, t, clock.usecond, clock.flags);
}
#endif /* defined(PARSE) && defined(CLOCK_RAWDCF) */

/*
 * History:
 *
 * clk_rawdcf.c,v
 * Revision 3.9  1994/01/25  19:05:12  kardel
 * 94/01/23 reconcilation
 *
 * Revision 3.8  1994/01/22  11:24:11  kardel
 * fixed PPS handling
 *
 * Revision 3.7  1993/10/30  09:44:41  kardel
 * conditional compilation flag cleanup
 *
 * Revision 3.6  1993/10/03  19:10:45  kardel
 * restructured I/O handling
 *
 * Revision 3.5  1993/09/27  21:08:07  kardel
 * utcoffset now in seconds
 *
 * Revision 3.4  1993/09/26  23:40:25  kardel
 * new parse driver logic
 *
 * Revision 3.3  1993/09/01  21:44:54  kardel
 * conditional cleanup
 *
 * Revision 3.2  1993/07/09  11:37:18  kardel
 * Initial restructured version + GPS support
 *
 * Revision 3.1  1993/07/06  10:00:19  kardel
 * DCF77 driver goes generic...
 *
 */
