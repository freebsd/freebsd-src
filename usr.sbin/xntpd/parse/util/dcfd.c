/*
 * /src/NTP/REPOSITORY/v3/parse/util/dcfd.c,v 3.8 1993/11/04 20:02:05 kardel Exp
 *  
 * dcfd.c,v 3.8 1993/11/04 20:02:05 kardel Exp
 *
 * DCF77 100/200ms pulse synchronisation daemon program (via 50Baud serial line)
 *
 * Features:
 *  DCF77 decoding
 *  NTP loopfilter logic for local clock
 *  interactive display for debugging
 *
 * Lacks:
 *  Leap second handling (at that level you should switch to xntp3 - really!)
 *
 * Copyright (c) 1993
 * Frank Kardel, Friedrich-Alexander Universitaet Erlangen-Nuernberg
 *                                    
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * This program may not be sold or used for profit without prior
 * written consent of the author.
 */

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <syslog.h>

#ifdef USE_PROTOTYPES
#include "ntp_stdlib.h"
extern int sigvec P((int, struct sigvec *, struct sigvec *));
extern int fscanf P((FILE *, char *, ...));
#endif

#ifdef SYS_LINUX
#include "ntp_timex.h"
#endif

#if defined(HAVE_TERMIOS) || defined(STREAM)
#include <termios.h>
#define TTY_GETATTR(_FD_, _ARG_) tcgetattr((_FD_), (_ARG_))
#define TTY_SETATTR(_FD_, _ARG_) tcsetattr((_FD_), TCSANOW, (_ARG_))
#endif

#if defined(HAVE_TERMIO) || defined(HAVE_SYSV_TTYS)
#include <termio.h>
#define TTY_GETATTR(_FD_, _ARG_) ioctl((_FD_), TCGETA, (_ARG_))
#define TTY_SETATTR(_FD_, _ARG_) ioctl((_FD_), TCSETAW, (_ARG_))
#endif

#ifndef TTY_GETATTR
MUST DEFINE ONE OF "HAVE_TERMIOS" or "HAVE_TERMIO"
#endif

#ifndef dysize
#define dysize(_x_) (((_x_) % 4) ? 365 : (((_x_) % 400) ? 365 : 366))
#endif

#define timernormalize(_a_, _b_) \
		if ((_a_)->tv_usec >= 1000000) \
			{ \
				(_a_)->tv_sec  += (_a_)->tv_usec / 1000000; \
				(_a_)->tv_usec -= (_a_)->tv_usec % 1000000; \
			} \

#define timeradd(_a_, _b_) \
		(_a_)->tv_sec  += (_b_)->tv_sec; \
		(_a_)->tv_usec += (_b_)->tv_usec; \
		timernormalize((_a_), (_b_))

#define timersub(_a_, _b_) \
		(_a_)->tv_sec  -= (_b_)->tv_sec; \
		(_a_)->tv_usec -= (_b_)->tv_usec; \
		timernormalize((_a_), (_b_))

#define PRINTF if (interactive) printf

#ifdef DEBUG
#define dprintf(_x_) PRINTF _x_
#else
#define dprintf(_x_)
#endif

extern int errno;

static int interactive = 0;
static int loop_filter_debug = 0;

#define NO_SYNC		0x01
#define SYNC		0x02

static int    sync_state = NO_SYNC;
static time_t last_sync;

static unsigned long ticks = 0;

static char pat[] = "-\\|/";

#define LINES		(24-2)	/* error lines after which the two headlines are repeated */

#define MAX_UNSYNC	(5*60)	/* allow synchronisation loss for 5 minutes */
#define NOTICE_INTERVAL (20*60)	/* mention missing synchronisation every 20 minutes */

/*
 * clock adjustment PLL - see NTP protocol spec (RFC1305) for details
 */

#define USECSCALE	10
#define TIMECONSTANT	0
#define ADJINTERVAL	0
#define FREQ_WEIGHT	18
#define PHASE_WEIGHT	7
#define MAX_DRIFT	0x3FFFFFFF

#define R_SHIFT(_X_, _Y_) (((_X_) < 0) ? -(-(_X_) >> (_Y_)) : ((_X_) >> (_Y_)))

static struct timeval max_adj_offset = { 0, 128000 };

static long clock_adjust = 0;	/* current adjustment value (usec * 2^USECSCALE) */
static long drift_comp   = 0;	/* accumulated drift value  (usec / ADJINTERVAL) */
static long adjustments  = 0;
static char skip_adjust  = 1;	/* discard first adjustment (bad samples) */

/*
 * state flags
 */
#define DCFB_ANNOUNCE           0x0001 /* switch time zone warning (DST switch) */
#define DCFB_DST                0x0002 /* DST in effect */
#define DCFB_LEAP		0x0004 /* LEAP warning (1 hour prior to occurence) */
#define DCFB_ALTERNATE		0x0008 /* alternate antenna used */

struct clocktime		/* clock time broken up from time code */
{
  long wday;
  long day;
  long month;
  long year;
  long hour;
  long minute;
  long second;
  long usecond;
  long utcoffset;	/* in minutes */
  long flags;		/* current clock status */
};

typedef struct clocktime clocktime_t;

#define TIMES10(_X_) (((_X_) << 3) + ((_X_) << 1))	/* *8 + *2 */
#define TIMES24(_X_) (((_X_) << 4) + ((_X_) << 3))      /* *16 + *8 */
#define TIMES60(_X_) ((((_X_) << 4)  - (_X_)) << 2)     /* *(16 - 1) *4 */
#define abs(_x_)     (((_x_) < 0) ? -(_x_) : (_x_))

/*
 * parser related return/error codes
 */
#define CVT_MASK	0x0000000F /* conversion exit code */
#define   CVT_NONE	0x00000001 /* format not applicable */
#define   CVT_FAIL	0x00000002 /* conversion failed - error code returned */
#define   CVT_OK	0x00000004 /* conversion succeeded */
#define CVT_BADFMT	0x00000010 /* general format error - (unparsable) */
#define CVT_BADDATE	0x00000020 /* invalid date */
#define CVT_BADTIME	0x00000040 /* invalid time */

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

static struct dcfparam
{
  unsigned char onebits[60];
  unsigned char zerobits[60];
} dcfparam = 
{
  "###############RADMLS1248124P124812P1248121241248112481248P", /* 'ONE' representation */
  "--------------------s-------p------p----------------------p"  /* 'ZERO' representation */
};

#define DCF_P_P1	0
#define DCF_P_P2	1
#define DCF_P_P3	2

#define DCF_Z_MET 0x2
#define DCF_Z_MED 0x1

static unsigned long ext_bf(buf, idx)
  register unsigned char *buf;
  register int   idx;
{
  register unsigned long sum = 0;
  register int i, first;

  first = rawdcfcode[idx].offset;
  
  for (i = rawdcfcode[idx+1].offset - 1; i >= first; i--)
    {
      sum <<= 1;
      sum |= (buf[i] != dcfparam.zerobits[i]);
    }
  return sum;
}

static unsigned pcheck(buf, idx)
  register unsigned char *buf;
  register int   idx;
{
  register int i,last;
  register unsigned psum = 1;

  last = partab[idx+1].offset;

  for (i = partab[idx].offset; i < last; i++)
    psum ^= (buf[i] != dcfparam.zerobits[i]);

  return psum;
}

static unsigned long convert_rawdcf(buffer, size, clock)
  register unsigned char   *buffer;
  register int              size;
  register clocktime_t     *clock;
{
  if (size < 57)
    {
      PRINTF("%-30s", "*** INCOMPLETE");
      return CVT_NONE;
    }
  
  /*
   * check Start and Parity bits
   */
  if ((ext_bf(buffer, DCF_S) == 1) &&
      pcheck(buffer, DCF_P_P1) &&
      pcheck(buffer, DCF_P_P2) &&
      pcheck(buffer, DCF_P_P3))
    {
      /*
       * buffer OK
       */

      clock->flags  = 0;
      clock->usecond= 0;
      clock->second = 0;
      clock->minute = ext_bf(buffer, DCF_M10);
      clock->minute = TIMES10(clock->minute) + ext_bf(buffer, DCF_M1);
      clock->hour   = ext_bf(buffer, DCF_H10);
      clock->hour   = TIMES10(clock->hour)   + ext_bf(buffer, DCF_H1);
      clock->day    = ext_bf(buffer, DCF_D10);
      clock->day    = TIMES10(clock->day)    + ext_bf(buffer, DCF_D1);
      clock->month  = ext_bf(buffer, DCF_MO0);
      clock->month  = TIMES10(clock->month)  + ext_bf(buffer, DCF_MO);
      clock->year   = ext_bf(buffer, DCF_Y10);
      clock->year   = TIMES10(clock->year)   + ext_bf(buffer, DCF_Y1);
      clock->wday   = ext_bf(buffer, DCF_DW);

      switch (ext_bf(buffer, DCF_Z))
	{
	case DCF_Z_MET:
	  clock->utcoffset = -60;
	  break;

	case DCF_Z_MED:
	  clock->flags     |= DCFB_DST;
	  clock->utcoffset  = -120;
	  break;

	default:
	  PRINTF("%-30s", "*** BAD TIME ZONE");
	  return CVT_FAIL|CVT_BADFMT;
	}

      if (ext_bf(buffer, DCF_A1))
	clock->flags |= DCFB_ANNOUNCE;

      if (ext_bf(buffer, DCF_A2))
	clock->flags |= DCFB_LEAP;

      if (ext_bf(buffer, DCF_R))
	clock->flags |= DCFB_ALTERNATE;

      return CVT_OK;
    }
  else
    {
      /*
       * bad format - not for us
       */
      PRINTF("%-30s", "*** BAD FORMAT (invalid/parity)");
      return CVT_FAIL|CVT_BADFMT;
    }
}

/*
 * raw dcf input routine - fix up 50 baud
 * characters for 1/0 decision
 */
static unsigned long cvt_rawdcf(buffer, size, clock)
  register unsigned char   *buffer;
  register int              size;
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
	  dprintf(("parse: cvt_rawdcf: character check for 0x%x@%d FAILED\n", *s, s - buffer));
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

  dprintf(("parse: cvt_rawdcf: average bit count: %d\n", cutoff));

  lowmax = 0;
  highmax = 0;

  dprintf(("parse: cvt_rawdcf: histogram:"));
  for (i = 0; i <= cutoff; i++)
    {
      lowmax+=histbuf[i] * i;
      highmax += histbuf[i];
      dprintf((" %d", histbuf[i]));
    }
  dprintf((" <M>"));

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
      dprintf((" %d", histbuf[i]));
    }
  dprintf(("\n"));

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

  dprintf(("parse: cvt_rawdcf: lower maximum %d, higher maximum %d, cutoff %d\n", lowmax, highmax, cutoff));

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

  return (rtc == CVT_NONE) ? convert_rawdcf(buffer, size, clock) : rtc;
}

time_t
parse_to_unixtime(clock, cvtrtc)
  register clocktime_t   *clock;
  register unsigned long *cvtrtc;
{
#define SETRTC(_X_)	{ if (cvtrtc) *cvtrtc = (_X_); }
  static int days_of_month[] = 
    {
      0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
  register int i;
  time_t t;
  
  if (clock->year < 100)
    clock->year += 1900;

  if (clock->year < 1970)
    clock->year += 100;		/* XXX this will do it till <2070 */

  if (clock->year < 0)
    {
      SETRTC(CVT_FAIL|CVT_BADDATE);
      return -1;
    }
  
  /*
   * sorry, slow section here - but it's not time critical anyway
   */
  t =  (clock->year - 1970) * 365;
  t += (clock->year >> 2) - (1970 >> 2);
  t -= clock->year / 400 - 1970 / 400;

  				/* month */
  if (clock->month <= 0 || clock->month > 12)
    {
      SETRTC(CVT_FAIL|CVT_BADDATE);
      return -1;		/* bad month */
    }
				/* adjust leap year */
  if (clock->month >= 3 && dysize(clock->year) == 366)
      t++;

  for (i = 1; i < clock->month; i++)
    {
      t += days_of_month[i];
    }
				/* day */
  if (clock->day < 1 || ((clock->month == 2 && dysize(clock->year) == 366) ?
		  clock->day > 29 : clock->day > days_of_month[clock->month]))
    {
      SETRTC(CVT_FAIL|CVT_BADDATE);
      return -1;		/* bad day */
    }

  t += clock->day - 1;
				/* hour */
  if (clock->hour < 0 || clock->hour >= 24)
    {
      SETRTC(CVT_FAIL|CVT_BADTIME);
      return -1;		/* bad hour */
    }

  t = TIMES24(t) + clock->hour;

  				/* min */
  if (clock->minute < 0 || clock->minute > 59)
    {
      SETRTC(CVT_FAIL|CVT_BADTIME);
      return -1;		/* bad min */
    }

  t = TIMES60(t) + clock->minute;
				/* sec */
  
  t += clock->utcoffset;	/* warp to UTC */

  if (clock->second < 0 || clock->second > 60)	/* allow for LEAPs */
    {
      SETRTC(CVT_FAIL|CVT_BADTIME);
      return -1;		/* bad sec */
    }

  t  = TIMES60(t) + clock->second;
				/* done */
  return t;
}

/*
 * cheap half baked 1/0 decision - for interactive operation only
 */
static char type(c)
unsigned char c;
{
  c ^= 0xFF;
  return (c > 0xF);
}

static char *wday[8] =
{
  "??",
  "Mo",
  "Tu",
  "We",
  "Th",
  "Fr",
  "Sa",
  "Su"
};

static char * pr_timeval(val)
  struct timeval *val;
{
  static char buf[20];

  if (val->tv_sec == 0)
    sprintf(buf, "%c0.%06d", (val->tv_usec < 0) ? '-' : '+', abs(val->tv_usec));
  else
    sprintf(buf, "%d.%06d", val->tv_sec, abs(val->tv_usec));
  return buf;
}

static void set_time(offset)
  struct timeval *offset;
{
  struct timeval the_time;
  /*XXX*/ if (loop_filter_debug) printf("set_time: %s ", pr_timeval(offset));
  if (gettimeofday(&the_time, 0L) == -1)
    {
      perror("gettimeofday()");
    }
  else
    {
      timeradd(&the_time, offset);
      if (settimeofday(&the_time, 0L) == -1)
	{
	  perror("settimeofday()");
	}
    }
}

static void adj_time(offset)
  register long offset;
{
  struct timeval time_offset;

  time_offset.tv_sec  = offset / 1000000;
  time_offset.tv_usec = offset % 1000000;

  /*XXX*/ if (loop_filter_debug)
	    printf("adj_time: %d us ", offset);
  if (adjtime(&time_offset, 0L) == -1)
    perror("adjtime()");
}

static void read_drift(drift_file)
  char *drift_file;
{
  FILE *df;

  df = fopen(drift_file, "r");
  if (df != NULL)
    {
      int idrift, fdrift;

      fscanf(df, "%4d.%03d", &idrift, &fdrift);
      fclose(df);
  /*XXX*/ if (loop_filter_debug)
	    printf("read_drift: %d.%03d ppm ", idrift, fdrift);

      drift_comp = idrift << USECSCALE;
      fdrift     = (fdrift << USECSCALE) / 1000;
      drift_comp += fdrift & (1<<USECSCALE);
  /*XXX*/ if (loop_filter_debug)
	    printf("read_drift: drift_comp %d ", drift_comp);
    }
}

static void update_drift(drift_file, offset, reftime)
  char *drift_file;
  long offset;
  time_t reftime;
{
  FILE *df;

  df = fopen(drift_file, "w");
  if (df != NULL)
    {
      int idrift = R_SHIFT(drift_comp, USECSCALE);
      int fdrift = drift_comp & ((1<<USECSCALE)-1);

  /*XXX*/ if (loop_filter_debug)
	    printf("update_drift: drift_comp %d ", drift_comp);
      fdrift = (fdrift * 1000) / (1<<USECSCALE);
      fprintf(df, "%4d.%03d %c%d.%06d %.24s\n", idrift, fdrift,
	      (offset < 0) ? '-' : '+', abs(offset) / 1000000, abs(offset) % 1000000,
	      asctime(localtime(&reftime)));
      fclose(df);
  /*XXX*/ if (loop_filter_debug)
	    printf("update_drift: %d.%03d ppm ", idrift, fdrift);
    }
}

static void adjust_clock(offset, drift_file, reftime)
  struct timeval *offset;
  char *drift_file;
  time_t reftime;
{
  struct timeval toffset;
  register long usecoffset;
  int tmp;

  if (skip_adjust)
    {
      skip_adjust = 0;
      return;
    }

  toffset = *offset;
  toffset.tv_sec  = abs(toffset.tv_sec);
  toffset.tv_usec = abs(toffset.tv_usec);
  if (timercmp(&toffset, &max_adj_offset, >))
    {
      /*
       * hopeless - set the clock - and clear the timing
       */
      set_time(offset);
      clock_adjust = 0;
      skip_adjust  = 1;
      return;
    }

  usecoffset   = offset->tv_sec * 1000000 + offset->tv_usec;

  clock_adjust = R_SHIFT(usecoffset, TIMECONSTANT);	/* adjustment to make for next period */

  tmp = 0;
  while (adjustments > (1 << tmp))
    tmp++;
  adjustments = 0;
  if (tmp > FREQ_WEIGHT)
    tmp = FREQ_WEIGHT;

  drift_comp  += R_SHIFT(usecoffset << USECSCALE, TIMECONSTANT+TIMECONSTANT+FREQ_WEIGHT-tmp);

  if (drift_comp > MAX_DRIFT)		/* clamp into interval */
    drift_comp = MAX_DRIFT;
  else
    if (drift_comp < -MAX_DRIFT)
      drift_comp = -MAX_DRIFT;

  update_drift(drift_file, usecoffset, reftime);
  /*XXX*/ if (loop_filter_debug)
	    printf("clock_adjust: %s, clock_adjust %d, drift_comp %d(%d) ",
		  pr_timeval(offset), R_SHIFT(clock_adjust, USECSCALE) , R_SHIFT(drift_comp, USECSCALE), drift_comp);
}

static void periodic_adjust()
{
  register long adjustment;

  adjustments++;

  adjustment = R_SHIFT(clock_adjust, PHASE_WEIGHT);

  clock_adjust -= adjustment;

  adjustment += R_SHIFT(drift_comp, USECSCALE+ADJINTERVAL);

  adj_time(adjustment);
}

static void tick()
{
  static unsigned long last_notice;

#ifndef SV_ONSTACK
  (void)signal(SIGALRM, tick);
#endif

  periodic_adjust();

  ticks += 1<<ADJINTERVAL;

  if ((sync_state == NO_SYNC) && ((ticks - last_sync) > MAX_UNSYNC) &&
      ((last_notice - ticks) > NOTICE_INTERVAL))
    {
      syslog(LOG_NOTICE, "still not synchronized - check receiver/signal");
      last_notice = ticks;
    }

#ifndef ITIMER_REAL
  (void) alarm(1<<ADJINTERVAL);
#endif
}

static void detach()
{
  int s;

  if (fork())
    exit(0);

  for (s = 0; s < 3; s++)
    (void) close(s);
  (void) open("/", 0);
  (void) dup2(0, 1);
  (void) dup2(0, 2);

#if defined(NTP_POSIX_SOURCE) || defined(_POSIX_)
  (void) setsid();
#else  /* _POSIX_ */
#ifndef BSD
  (void) setpgrp();
#else  /* BSD */
  (void) setpgrp(0, getpid());
#endif /* BSD */
#endif /* _POSIX_ */
#if defined(hpux)
  if (fork())
    exit(0);
#endif /* hpux */
}

static void usage(program)
  char *program;
{
  fprintf(stderr, "usage: %s [-f] [-l] [-t] [-i] [-o] [-d <drift_file>] <device>\n", program);
  fprintf(stderr, "\t-i              interactive\n");
  fprintf(stderr, "\t-t              trace (print all datagrams)\n");
  fprintf(stderr, "\t-f              print all databits (includes PTB private data)\n");
  fprintf(stderr, "\t-l              print loop filter debug information\n");
  fprintf(stderr, "\t-o              print offet average for current minute\n");
  fprintf(stderr, "\t-d <drift_file> specify alternate drift file\n");
}

int
main(argc, argv)
  int argc;
  char **argv;
{
  unsigned char c;
  char **a = argv;
  int  ac = argc;
  char *file = NULL;
  char *drift_file = "/etc/dcfd.drift";
  int fd;
  int offset = 15;
  int offsets = 0;
  int trace = 0;
  int errs = 0;

  while (--ac)
    {
      char *arg = *++a;
      if (*arg == '-')
	while ((c = *++arg))
	  switch (c)
	    {
	      case 't':
		    trace = 1;
		    interactive = 1;
		    break;

	      case 'f':
		    offset = 0;
		    interactive = 1;
		    break;

	      case 'l':
		    loop_filter_debug = 1;
		    offsets = 1;
		    interactive = 1;
		    break;

	      case 'o':
		    offsets = 1;
		    interactive = 1;
		    break;

	      case 'i':
		    interactive = 1;
		    break;

	      case 'd':
		    if (ac > 1)
		      {
			drift_file = *++a;
			ac--;
		      }
		    else
		      {
			fprintf(stderr, "%s: -d requires file name argument\n", argv[0]);
			errs=1;
		      }
		    break;
	      
	      default:
		    fprintf(stderr, "%s: unknown option -%c\n", argv[0], c);
		    errs=1;
		    break;
	    }
      else
	if (file == NULL)
	  file = arg;
	else
	  {
	    fprintf(stderr, "%s: device specified twice\n", argv[0]);
	    errs=1;
	  }
    }

  if (errs)
    {
      usage(argv[0]);
      exit(1);
    }
  else
    if (file == NULL)
      {
	fprintf(stderr, "%s: device not specified\n", argv[0]);
	usage(argv[0]);
	exit(1);
      }

  errs = LINES+1;

  fd = open(file, O_RDONLY);
  if (fd == -1)
    {
      perror(file);
      exit(1);
    }
  else
    {
      int i, rrc;
      struct timeval t, tt, tlast;
      struct timeval timeout;
      struct timeval phase;
      struct timeval time_offset;
      char pbuf[61];		/* printable version */
      char buf[61];		/* raw data */
      clocktime_t clock;	/* wall clock time */
      time_t utc_time = 0;
      long usecerror = 0;
      long lasterror = 0;
#if defined(HAVE_TERMIOS) || defined(STREAM)
      struct termios term;
#endif
#if defined(HAVE_TERMIO) || defined(HAVE_SYSV_TTYS)
      struct termio term;
#endif
      int rtc = CVT_NONE;

      timeout.tv_sec  = 1;
      timeout.tv_usec = 500000;

      phase.tv_sec    = 0;
      phase.tv_usec   = 230000;

      if (TTY_GETATTR(fd,  &term) == -1)
	{
	  perror("tcgetattr");
	  exit(1);
	}

      memset(term.c_cc, 0, sizeof(term.c_cc));
      term.c_cc[VMIN] = 1;
      term.c_cflag = B50|CS8|CREAD|CLOCAL;
      term.c_iflag = 0;
      term.c_oflag = 0;
      term.c_lflag = 0;

      if (TTY_SETATTR(fd, &term) == -1)
	{
	  perror("tcsetattr");
	  exit(1);
	}

      if (!interactive)
	detach();
      
#ifdef LOG_DAEMON
      openlog("dcfd", LOG_PID, LOG_DAEMON);
#else
      openlog("dcfd", LOG_PID);
#endif

#ifdef SV_ONSTACK
      {
	struct sigvec vec;

	vec.sv_handler = tick;
	vec.sv_mask    = 0;
	vec.sv_flags   = 0;

	if (sigvec(SIGALRM, &vec, (struct sigvec *)0) == -1)
	  {
	    syslog(LOG_ERR, "sigvec(SIGALRM): %m");
	    exit(1);
	  }
      }
#else
      (void) signal(SIGALRM, tick);
#endif

#ifdef ITIMER_REAL
      {
	struct itimerval it;

	it.it_interval.tv_sec  = 1<<ADJINTERVAL;
	it.it_interval.tv_usec = 0;
	it.it_value.tv_sec     = 1<<ADJINTERVAL;
	it.it_value.tv_usec    = 0;
	
      if (setitimer(ITIMER_REAL, &it, (struct itimerval *)0) == -1)
	{
	  syslog(LOG_ERR, "setitimer: %m");
	  exit(1);
	}
      }
#else
      (void) alarm(1<<ADJINTERVAL);
#endif

      PRINTF("  DCF77 monitor - Copyright 1993, Frank Kardel\n\n");

      pbuf[60] = '\0';
      for ( i = 0; i < 60; i++)
	pbuf[i] = '.';

      read_drift(drift_file);

      gettimeofday(&tlast, 0L);
      i = 0;
      do
	{
	  while ((rrc = read(fd, &c, 1)) == 1)
	    {
	      gettimeofday(&t, 0L);
	      tt = t;
	      timersub(&t, &tlast);

	      if (errs > LINES)
		{
		  PRINTF("  %s", &"PTB private....RADMLSMin....PHour..PMDay..DayMonthYear....P\n"[offset]);
		  PRINTF("  %s", &"---------------RADMLS1248124P124812P1248121241248112481248P\n"[offset]);
		  errs = 0;
		}

	      if (timercmp(&t, &timeout, >))
		{
		  PRINTF("%c %.*s ", pat[i % (sizeof(pat)-1)], 59 - offset, &pbuf[offset]);

		  if ((rtc = cvt_rawdcf(buf, i, &clock)) != CVT_OK)
		    {
		      PRINTF("\n");
		      if (sync_state == SYNC)
			{
			  sync_state = NO_SYNC;
			  syslog(LOG_DEBUG, "DCF77 reception lost");
			}
		      errs++;
		    }

		  buf[0] = c;

		  if (((c^0xFF)+1) & (c^0xFF))
		    pbuf[0] = '?';
		  else
		    pbuf[0] = type(c) ? '#' : '-';

		  for ( i = 1; i < 60; i++)
		    pbuf[i] = '.';

		  i = 0;
		}
	      else
		{
		  buf[i] = c;

		  /*
		   * initial guess (usually correct)
		   */
		  if (((c^0xFF)+1) & (c^0xFF))
		    pbuf[i] = '?';
		  else
		    pbuf[i] = type(c) ? '#' : '-';

		  PRINTF("%c %.*s ", pat[i % (sizeof(pat)-1)], 59 - offset, &pbuf[offset]);
		}

	      if (i == 0 && rtc == CVT_OK)
		{
		  if ((utc_time = parse_to_unixtime(&clock, &rtc)) == -1)
		    {
		      PRINTF("*** BAD CONVERSION\n");
		    }

		  usecerror = 0;
		}

	      if (rtc == CVT_OK)
		{
		  if (trace && (i == 0))
		    {
		      PRINTF("\r  %.*s ", 59 - offset, &buf[offset]);
		    }

		  if (i == 0)
		    {
		      time_offset.tv_sec  = lasterror / 1000000;
		      time_offset.tv_usec = lasterror % 1000000;
		      adjust_clock(&time_offset, drift_file, utc_time+i);
		      last_sync = ticks;
		      if (sync_state == NO_SYNC)
			{
			  syslog(LOG_INFO, "receiving DCF77");
			}
		      sync_state = SYNC;
		    }

		  time_offset.tv_sec  = utc_time + i;
		  time_offset.tv_usec = 0;

		  timeradd(&time_offset, &phase);

		  usecerror += (time_offset.tv_sec - tt.tv_sec) * 1000000 + time_offset.tv_usec
		    -tt.tv_usec;

		  PRINTF(offsets ? "%s, %2d:%02d:%02d, %d.%02d.%02d, <%s%s%s%s> (%c%d.%06ds)" :
			 "%s, %2d:%02d:%02d, %d.%02d.%02d, <%s%s%s%s>",
			 wday[clock.wday],
			 clock.hour, clock.minute, i, clock.day, clock.month,
			 clock.year,
			 (clock.flags & DCFB_ALTERNATE) ? "R" : "_",
			 (clock.flags & DCFB_ANNOUNCE) ? "A" : "_",
			 (clock.flags & DCFB_DST) ? "D" : "_",
			 (clock.flags & DCFB_LEAP) ? "L" : "_",
			 (lasterror < 0) ? '-' : '+', abs(lasterror) / 1000000, abs(lasterror) % 1000000
			 );

		  if (trace && (i == 0))
		    {
		      PRINTF("\n");
		      errs++;
		    }
		  lasterror = usecerror / (i+1);
		}

	      PRINTF("\r");

	      if (i < 60)
		{
		  i++;
		}

	      tlast = tt;

	      if (interactive)
		fflush(stdout);
	    }
	} while ((rrc == -1) && (errno == EINTR));
      
      syslog(LOG_ERR, "TERMINATING - cannot read from device %s", file);

      (void)close(fd);
    }

  closelog();
  
  return 0;
}
