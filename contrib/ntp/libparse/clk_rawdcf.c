/*
 * /src/NTP/ntp-4/libparse/clk_rawdcf.c,v 4.9 1999/12/06 13:42:23 kardel Exp
 *  
 * clk_rawdcf.c,v 4.9 1999/12/06 13:42:23 kardel Exp
 *
 * Raw DCF77 pulse clock support
 *
 * Copyright (C) 1992-1998 by Frank Kardel
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

#if defined(REFCLOCK) && defined(CLOCK_PARSE) && defined(CLOCK_RAWDCF)

#include <sys/types.h>
#include <sys/time.h>

#include "ntp_fp.h"
#include "ntp_unixtime.h"
#include "ntp_calendar.h"

#include "parse.h"
#ifdef PARSESTREAM
# include <sys/parsestreams.h>
#endif

#ifndef PARSEKERNEL
# include "ntp_stdlib.h"
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

static u_long pps_rawdcf P((parse_t *, int, timestamp_t *));
static u_long cvt_rawdcf P((unsigned char *, int, struct format *, clocktime_t *, void *));
static u_long inp_rawdcf P((parse_t *, unsigned int, timestamp_t  *));

typedef struct last_tcode {
	time_t tcode;	/* last converted time code */
} last_tcode_t;

clockformat_t clock_rawdcf =
{
  inp_rawdcf,			/* DCF77 input handling */
  cvt_rawdcf,			/* raw dcf input conversion */
  pps_rawdcf,			/* examining PPS information */
  0,				/* no private configuration data */
  "RAW DCF77 Timecode",		/* direct decoding / time synthesis */

  61,				/* bit buffer */
  sizeof(last_tcode_t)
};

static struct dcfparam
{
	unsigned char onebits[60];
	unsigned char zerobits[60];
} dcfparameter = 
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

static u_long
ext_bf(
	register unsigned char *buf,
	register int   idx,
	register unsigned char *zero
	)
{
	register u_long sum = 0;
	register int i, first;

	first = rawdcfcode[idx].offset;
  
	for (i = rawdcfcode[idx+1].offset - 1; i >= first; i--)
	{
		sum <<= 1;
		sum |= (buf[i] != zero[i]);
	}
	return sum;
}

static unsigned
pcheck(
       unsigned char *buf,
       int   idx,
       unsigned char *zero
       )
{
	int i,last;
	unsigned psum = 1;

	last = partab[idx+1].offset;

	for (i = partab[idx].offset; i < last; i++)
	    psum ^= (buf[i] != zero[i]);

	return psum;
}

static u_long
convert_rawdcf(
	       unsigned char   *buffer,
	       int              size,
	       struct dcfparam *dcfprm,
	       clocktime_t     *clock_time
	       )
{
	register unsigned char *s = buffer;
	register unsigned char *b = dcfprm->onebits;
	register unsigned char *c = dcfprm->zerobits;
	register int i;

	parseprintf(DD_RAWDCF,("parse: convert_rawdcf: \"%s\"\n", buffer));

	if (size < 57)
	{
#ifndef PARSEKERNEL
		msyslog(LOG_ERR, "parse: convert_rawdcf: INCOMPLETE DATA - time code only has %d bits\n", size);
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
#ifndef PARSEKERNEL
			msyslog(LOG_ERR, "parse: convert_rawdcf: BAD DATA - no conversion for \"%s\"\n", buffer);
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
	if ((ext_bf(buffer, DCF_S, dcfprm->zerobits) == 1) &&
	    pcheck(buffer, DCF_P_P1, dcfprm->zerobits) &&
	    pcheck(buffer, DCF_P_P2, dcfprm->zerobits) &&
	    pcheck(buffer, DCF_P_P3, dcfprm->zerobits))
	{
		/*
		 * buffer OK
		 */
		parseprintf(DD_RAWDCF,("parse: convert_rawdcf: parity check passed\n"));

		clock_time->flags  = PARSEB_S_ANTENNA|PARSEB_S_LEAP;
		clock_time->utctime= 0;
		clock_time->usecond= 0;
		clock_time->second = 0;
		clock_time->minute = ext_bf(buffer, DCF_M10, dcfprm->zerobits);
		clock_time->minute = TIMES10(clock_time->minute) + ext_bf(buffer, DCF_M1, dcfprm->zerobits);
		clock_time->hour   = ext_bf(buffer, DCF_H10, dcfprm->zerobits);
		clock_time->hour   = TIMES10(clock_time->hour) + ext_bf(buffer, DCF_H1, dcfprm->zerobits);
		clock_time->day    = ext_bf(buffer, DCF_D10, dcfprm->zerobits);
		clock_time->day    = TIMES10(clock_time->day) + ext_bf(buffer, DCF_D1, dcfprm->zerobits);
		clock_time->month  = ext_bf(buffer, DCF_MO0, dcfprm->zerobits);
		clock_time->month  = TIMES10(clock_time->month) + ext_bf(buffer, DCF_MO, dcfprm->zerobits);
		clock_time->year   = ext_bf(buffer, DCF_Y10, dcfprm->zerobits);
		clock_time->year   = TIMES10(clock_time->year) + ext_bf(buffer, DCF_Y1, dcfprm->zerobits);

		switch (ext_bf(buffer, DCF_Z, dcfprm->zerobits))
		{
		    case DCF_Z_MET:
			clock_time->utcoffset = -1*60*60;
			break;

		    case DCF_Z_MED:
			clock_time->flags     |= PARSEB_DST;
			clock_time->utcoffset  = -2*60*60;
			break;

		    default:
			parseprintf(DD_RAWDCF,("parse: convert_rawdcf: BAD TIME ZONE\n"));
			return CVT_FAIL|CVT_BADFMT;
		}

		if (ext_bf(buffer, DCF_A1, dcfprm->zerobits))
		    clock_time->flags |= PARSEB_ANNOUNCE;

		if (ext_bf(buffer, DCF_A2, dcfprm->zerobits))
		    clock_time->flags |= PARSEB_LEAPADD; /* default: DCF77 data format deficiency */

		if (ext_bf(buffer, DCF_R, dcfprm->zerobits))
		    clock_time->flags |= PARSEB_ALTERNATE;

		parseprintf(DD_RAWDCF,("parse: convert_rawdcf: TIME CODE OK: %d:%d, %d.%d.%d, flags 0x%lx\n",
				       (int)clock_time->hour, (int)clock_time->minute, (int)clock_time->day, (int)clock_time->month,(int) clock_time->year,
				       (u_long)clock_time->flags));
		return CVT_OK;
	}
	else
	{
		/*
		 * bad format - not for us
		 */
#ifndef PARSEKERNEL
		msyslog(LOG_ERR, "parse: convert_rawdcf: parity check FAILED for \"%s\"\n", buffer);
#endif
		return CVT_FAIL|CVT_BADFMT;
	}
}

/*
 * raw dcf input routine - needs to fix up 50 baud
 * characters for 1/0 decision
 */
static u_long
cvt_rawdcf(
	   unsigned char   *buffer,
	   int              size,
	   struct format   *param,
	   clocktime_t     *clock_time,
	   void            *local
	   )
{
	         last_tcode_t  *t = (last_tcode_t *)local;
	register unsigned char *s = (unsigned char *)buffer;
	register unsigned char *e = s + size;
	register unsigned char *b = dcfparameter.onebits;
	register unsigned char *c = dcfparameter.zerobits;
	         u_long   rtc = CVT_NONE;
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
			parseprintf(DD_RAWDCF,("parse: cvt_rawdcf: character check for 0x%x@%d FAILED\n", *s, (int)(s - (unsigned char *)buffer)));
			*s = (unsigned char)~0;
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

	s = (unsigned char *)buffer;
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

        if (rtc == CVT_NONE)
        {
	       rtc = convert_rawdcf(buffer, size, &dcfparameter, clock_time);
	       if (rtc == CVT_OK)
	       {
			time_t newtime;

			newtime = parse_to_unixtime(clock_time, &rtc);
			if ((rtc == CVT_OK) && t)
			{
				if ((newtime - t->tcode) == 60) /* guard against multi bit errors */
				{
					clock_time->utctime = newtime;
				}
				else
				{
					rtc = CVT_FAIL|CVT_BADTIME;
				}
				t->tcode            = newtime;
			}
	       }
        }
	 
    	return rtc;
}

/*
 * pps_rawdcf
 *
 * currently a very stupid version - should be extended to decode
 * also ones and zeros (which is easy)
 */
/*ARGSUSED*/
static u_long
pps_rawdcf(
	register parse_t *parseio,
	register int status,
	register timestamp_t *ptime
	)
{
	if (!status)		/* negative edge for simpler wiring (Rx->DCD) */
	{
		parseio->parse_dtime.parse_ptime  = *ptime;
		parseio->parse_dtime.parse_state |= PARSEB_PPS|PARSEB_S_PPS;
	}

	return CVT_NONE;
}

static u_long
snt_rawdcf(
	register parse_t *parseio,
	register timestamp_t *ptime
	)
{
	if ((parseio->parse_dtime.parse_status & CVT_MASK) == CVT_OK)
	{
		parseio->parse_dtime.parse_stime = *ptime;

#ifdef PARSEKERNEL
		parseio->parse_dtime.parse_time.tv.tv_sec++;
#else
		parseio->parse_dtime.parse_time.fp.l_ui++;
#endif
		
		parseprintf(DD_RAWDCF,("parse: snt_rawdcf: time stamp synthesized offset %d seconds\n", parseio->parse_index - 1));
		
		return updatetimeinfo(parseio, parseio->parse_lstate);
	}
	return CVT_NONE;
}

/*
 * inp_rawdcf
 *
 * grep DCF77 data from input stream
 */
static u_long
inp_rawdcf(
	  parse_t      *parseio,
	  unsigned int  ch,
	  timestamp_t  *tstamp
	  )
{
	static struct timeval timeout = { 1, 500000 }; /* 1.5 secongs denote second #60 */
	
	parseprintf(DD_PARSE, ("inp_rawdcf(0x%x, 0x%x, ...)\n", (int)parseio, (int)ch));
	
	parseio->parse_dtime.parse_stime = *tstamp; /* collect timestamp */

	if (parse_timedout(parseio, tstamp, &timeout))
	{
		parseprintf(DD_PARSE, ("inp_rawdcf: time out seen\n"));

		(void) parse_end(parseio);
		(void) parse_addchar(parseio, ch);
		return PARSE_INP_TIME;
	}
	else
	{
		unsigned int rtc;
		
		rtc = parse_addchar(parseio, ch);
		if (rtc == PARSE_INP_SKIP)
		{
			if (snt_rawdcf(parseio, tstamp) == CVT_OK)
				return PARSE_INP_SYNTH;
		}
		return rtc;
	}
}

#else /* not (REFCLOCK && CLOCK_PARSE && CLOCK_RAWDCF) */
int clk_rawdcf_bs;
#endif /* not (REFCLOCK && CLOCK_PARSE && CLOCK_RAWDCF) */

/*
 * History:
 *
 * clk_rawdcf.c,v
 * Revision 4.9  1999/12/06 13:42:23  kardel
 * transfer correctly converted time codes always into tcode
 *
 * Revision 4.8  1999/11/28 09:13:50  kardel
 * RECON_4_0_98F
 *
 * Revision 4.7  1999/04/01 20:07:20  kardel
 * added checking for minutie increment of timestamps in clk_rawdcf.c
 *
 * Revision 4.6  1998/06/14 21:09:37  kardel
 * Sun acc cleanup
 *
 * Revision 4.5  1998/06/13 12:04:16  kardel
 * fix SYSV clock name clash
 *
 * Revision 4.4  1998/06/12 15:22:28  kardel
 * fix prototypes
 *
 * Revision 4.3  1998/06/06 18:33:36  kardel
 * simplified condidional compile expression
 *
 * Revision 4.2  1998/05/24 11:04:18  kardel
 * triggering PPS on negative edge for simpler wiring (Rx->DCD)
 *
 * Revision 4.1  1998/05/24 09:39:53  kardel
 * implementation of the new IO handling model
 *
 * Revision 4.0  1998/04/10 19:45:30  kardel
 * Start 4.0 release version numbering
 *
 * from V3 3.24 log info deleted 1998/04/11 kardel
 *
 */
