#if defined(REFCLOCK) && (defined(PARSE) || defined(PARSEPPS))
/*
 * /src/NTP/REPOSITORY/v3/parse/parse.c,v 3.22 1994/02/25 12:34:49 kardel Exp
 *  
 * parse.c,v 3.22 1994/02/25 12:34:49 kardel Exp
 *
 * Parser module for reference clock
 *
 * PARSEKERNEL define switches between two personalities of the module
 * if PARSEKERNEL is defined this module can be used with dcf77sync.c as
 * a PARSEKERNEL kernel module. In this case the time stamps will be
 * a struct timeval.
 * when PARSEKERNEL is not defined NTP time stamps will be used.
 *
 * Copyright (c) 1992,1993,1994
 * Frank Kardel Friedrich-Alexander Universitaet Erlangen-Nuernberg
 *                                    
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#if	!(defined(lint) || defined(__GNUC__))
static char rcsid[] = "parse.c,v 3.19 1994/01/25 19:05:20 kardel Exp";
#endif

#include "sys/types.h"
#include "sys/time.h"
#include "sys/errno.h"

#include "ntp_machine.h"

#if defined(PARSESTREAM) && (defined(SYS_SUNOS4) || defined(SYS_SOLARIS)) && defined(STREAM)
/*
 * Sorry, but in SunOS 4.x AND Solaris 2.x kernels there are no
 * mem* operations. I don't want them - bcopy, bzero
 * are fine in the kernel
 */
#ifndef NTP_NEED_BOPS
#define NTP_NEED_BOPS
#endif
#else
#ifndef NTP_NEED_BOPS
#ifndef bzero
#define bzero(_X_, _Y_) memset(_X_, 0, _Y_)
#define bcopy(_X_, _Y_, _Z_) memmove(_Y_, _X_, _Z_)
#endif
#endif
#endif

#include "ntp_fp.h"
#include "ntp_unixtime.h"
#include "ntp_calendar.h"

#include "parse.h"

#include "ntp_stdlib.h"

#ifdef PARSESTREAM
#include "sys/parsestreams.h"
#endif

extern clockformat_t *clockformats[];
extern unsigned short nformats;

static unsigned LONG timepacket();

/*
 * strings support usually not in kernel - duplicated, but what the heck
 */
static int
Strlen(s)
  register char *s;
{
  register int c;

  c = 0;
  if (s)
    {
      while (*s++)
	{
	  c++;
	}
    }
  return c;
}

static int
Strcmp(s, t)
  register char *s;
  register char *t;
{
  register int c = 0;

  if (!s || !t || (s == t))
    {
      return 0;
    }

  while (!(c = *s++ - *t++) && *s && *t)
    /* empty loop */;
  
  return c;
}

static int
timedout(parseio, ctime)
  register parse_t *parseio;
  register timestamp_t *ctime;
{
  struct timeval delta;

#ifdef PARSEKERNEL
  delta.tv_sec = ctime->tv.tv_sec - parseio->parse_lastchar.tv.tv_sec;
  delta.tv_usec = ctime->tv.tv_usec - parseio->parse_lastchar.tv.tv_usec;
  if (delta.tv_usec < 0)
    {
      delta.tv_sec  -= 1;
      delta.tv_usec += 1000000;
    }
#else
  extern LONG tstouslo[];
  extern LONG tstousmid[];
  extern LONG tstoushi[];

  l_fp delt;

  delt = ctime->fp;
  L_SUB(&delt, &parseio->parse_lastchar.fp);
  TSTOTV(&delt, &delta);
#endif

  if (timercmp(&delta, &parseio->parse_timeout, >))
    {
      parseprintf(DD_PARSE, ("parse: timedout: TRUE\n"));
      return 1;
    }
  else
    {
      parseprintf(DD_PARSE, ("parse: timedout: FALSE\n"));
      return 0;
    }
}

/*
 * setup_bitmaps
 */
static int
setup_bitmaps(parseio, low, high)
  register parse_t *parseio;
  register unsigned short low;
  register unsigned short high;
{
  register unsigned short i;
  register int f = 0;
  register clockformat_t *fmt;
  register unsigned index, mask;

  if ((low >= high) ||
      (high > nformats))
    {
      parseprintf(DD_PARSE, ("setup_bitmaps: failed: bounds error (low=%d, high=%d, nformats=%d)\n", low, high, nformats));
      return 0;
    }
  
  bzero(parseio->parse_startsym, sizeof (parseio->parse_startsym));
  bzero(parseio->parse_endsym, sizeof (parseio->parse_endsym));
  bzero(parseio->parse_syncsym, sizeof (parseio->parse_syncsym));
  parseio->parse_syncflags = 0;
  parseio->parse_timeout.tv_sec = 0;
  parseio->parse_timeout.tv_usec = 0;
  
  /*
   * gather bitmaps of possible start and end values
   */
  for (i=low; i < high; i++)
    {
      fmt = clockformats[i];

      if (fmt->flags & F_START)
	{
	  index = fmt->startsym / 8;
	  mask  = 1 << (fmt->startsym % 8);

	  if (parseio->parse_endsym[index] & mask)
	    {
#ifdef PARSEKERNEL
	      printf("parse: setup_bitmaps: failed: START symbol collides with END symbol (format %d)\n", i);
#else
	      syslog(LOG_ERR, "parse: setup_bitmaps: failed: START symbol collides with END symbol (format %d)\n", i);
#endif
	      return 0;
	    }
	  else
	    {
	      parseio->parse_startsym[index] |= mask;
	      f = 1;
	    }
	}

      if (fmt->flags & F_END)
	{
	  index = fmt->endsym / 8;
	  mask  = 1 << (fmt->endsym % 8);

	  if (parseio->parse_startsym[index] & mask)
	    {
#ifdef PARSEKERNEL
	      printf("parse: setup_bitmaps: failed: END symbol collides with START symbol (format %d)\n", i);
#else
	      syslog(LOG_ERR, "parse: setup_bitmaps: failed: END symbol collides with START symbol (format %d)\n", i);
#endif
	      return 0;
	    }
	  else
	    {
	      parseio->parse_endsym[index] |= mask;
	      f = 1;
	    }
	}

      if (fmt->flags & SYNC_CHAR)
	{
	  parseio->parse_syncsym[fmt->syncsym / 8] |= (1 << (fmt->syncsym % 8));
	}

      parseio->parse_syncflags |= fmt->flags & (SYNC_START|SYNC_END|SYNC_CHAR|SYNC_ONE|SYNC_ZERO|SYNC_TIMEOUT|SYNC_SYNTHESIZE);

      if ((fmt->flags & SYNC_TIMEOUT) &&
	  ((parseio->parse_timeout.tv_sec || parseio->parse_timeout.tv_usec) ? timercmp(&parseio->parse_timeout, &fmt->timeout, >) : 1))
	{
	  parseio->parse_timeout = fmt->timeout;
	}
      
      if (parseio->parse_dsize < fmt->length)
	parseio->parse_dsize = fmt->length;
    }

  if (!f && ((int)(high - low) > 1))
    {
      /*
       * need at least one start or end symbol
       */
#ifdef PARSEKERNEL
      printf("parse: setup_bitmaps: failed: neither START nor END symbol defined\n");
#else
      syslog(LOG_ERR, "parse: setup_bitmaps: failed: neither START nor END symbol defined\n");
#endif
      return 0;
    }

  return 1;
}

/*ARGSUSED*/
int
parse_ioinit(parseio)
  register parse_t *parseio;
{
  parseprintf(DD_PARSE, ("parse_iostart\n"));
  
  if (!setup_bitmaps(parseio, 0, nformats))
    return 0;

  parseio->parse_data = MALLOC(parseio->parse_dsize * 2 + 2);
  if (!parseio->parse_data)
    {
      parseprintf(DD_PARSE, ("init failed: malloc for data area failed\n"));
      return 0;
    }

  /*
   * leave room for '\0'
   */
  parseio->parse_ldata     = parseio->parse_data + parseio->parse_dsize + 1;
  parseio->parse_lformat   = 0;
  parseio->parse_badformat = 0;
  parseio->parse_ioflags   = PARSE_IO_CS7;	/* usual unix default */
  parseio->parse_flags     = 0;		/* true samples */
  parseio->parse_index     = 0;
  parseio->parse_ldsize    = 0;
  
  return 1;
}

/*ARGSUSED*/
void
parse_ioend(parseio)
  register parse_t *parseio;
{
  parseprintf(DD_PARSE, ("parse_ioend\n"));
  if (parseio->parse_data)
    FREE(parseio->parse_data, parseio->parse_dsize * 2 + 2);
}

/*ARGSUSED*/
int
parse_ioread(parseio, ch, ctime)
  register parse_t *parseio;
  register unsigned char ch;
  register timestamp_t *ctime;
{
  register unsigned updated = CVT_NONE;
  register unsigned short low, high;
  register unsigned index, mask;

  parseprintf(DD_PARSE, ("parse_ioread(0x%x, char=0x%x, ..., ...)\n", (unsigned int)parseio, ch & 0xFF));

  if (parseio->parse_flags & PARSE_FIXED_FMT)
    {
      if (!clockformats[parseio->parse_lformat]->convert)
	{
	  parseprintf(DD_PARSE, ("parse_ioread: input dropped.\n"));
	  return CVT_NONE;
	}
      low = parseio->parse_lformat;
      high = low + 1;
    }
  else
    {
      low = 0;
      high = nformats;
    }

  /*
   * within STREAMS CSx (x < 8) chars still have the upper bits set
   * so we normalize the characters by masking unecessary bits off.
   */
  switch (parseio->parse_ioflags & PARSE_IO_CSIZE)
    {
    case PARSE_IO_CS5:
      ch &= 0x1F;
      break;

    case PARSE_IO_CS6:
      ch &= 0x3F;
      break;

    case PARSE_IO_CS7:
      ch &= 0x7F;
      break;
      
    case PARSE_IO_CS8:
      break;
    }

  index = ch / 8;
  mask  = 1 << (ch % 8);

  if ((parseio->parse_syncflags & SYNC_CHAR) &&
      (parseio->parse_syncsym[index] & mask))
    {
      register clockformat_t *fmt;
      register unsigned short i;
      /*
       * got a sync event - call sync routine
       */

      for (i = low; i < high; i++)
	{
	  fmt = clockformats[i];

	  if ((fmt->flags & SYNC_CHAR) &&
	      (fmt->syncsym == ch))
	    {
	      parseprintf(DD_PARSE, ("parse_ioread: SYNC_CHAR event\n"));
	      if (fmt->syncevt)
		fmt->syncevt(parseio, ctime, fmt->data, SYNC_CHAR);
	    }
	}
    }
		
  if ((((parseio->parse_syncflags & SYNC_START) &&
	(parseio->parse_startsym[index] & mask)) ||
       (parseio->parse_index == 0)) ||
      ((parseio->parse_syncflags & SYNC_TIMEOUT) &&
       timedout(parseio, ctime)))
    {
      register unsigned short i;
      /*
       * packet start - re-fill buffer
       */
      if (parseio->parse_index)
	{
	  /*
	   * filled buffer - thus not end character found
	   * do processing now
	   */
	  parseio->parse_data[parseio->parse_index] = '\0';

	  updated = timepacket(parseio);
	  bcopy(parseio->parse_data, parseio->parse_ldata, parseio->parse_index+1);
	  parseio->parse_ldsize = parseio->parse_index+1;
	  if (parseio->parse_syncflags & SYNC_TIMEOUT)
	    parseio->parse_dtime.parse_stime = *ctime;
	}

      /*
       * could be a sync event - call sync routine if needed
       */
      if (parseio->parse_syncflags & SYNC_START)
	for (i = low; i < high; i++)
	  {
	    register clockformat_t *fmt = clockformats[i];

	    if ((parseio->parse_index == 0) ||
		((fmt->flags & SYNC_START) && (fmt->startsym == ch)))
	      {
		parseprintf(DD_PARSE, ("parse_ioread: SYNC_START event\n"));
		if (fmt->syncevt)
		  fmt->syncevt(parseio, ctime, fmt->data, SYNC_START);
	      }
	  }
      parseio->parse_index = 1;
      parseio->parse_data[0] = ch;
      parseprintf(DD_PARSE, ("parse: parse_ioread: buffer start\n"));
    }
  else
    {
      register unsigned short i;

      if (parseio->parse_index < parseio->parse_dsize)
	{
	  /*
	   * collect into buffer
	   */
          parseprintf(DD_PARSE, ("parse: parse_ioread: buffer[%d] = 0x%x\n", parseio->parse_index, ch));
	  parseio->parse_data[parseio->parse_index++] = ch;
	}

      if ((parseio->parse_endsym[index] & mask) ||
	  (parseio->parse_index >= parseio->parse_dsize))
	{
	  /*
	   * packet end - process buffer
	   */
	  if (parseio->parse_syncflags & SYNC_END)
	    for (i = low; i < high; i++)
	      {
		register clockformat_t *fmt = clockformats[i];

		if ((fmt->flags & SYNC_END) && (fmt->endsym == ch))
		  {
		    parseprintf(DD_PARSE, ("parse_ioread: SYNC_END event\n"));
		    if (fmt->syncevt)
		      fmt->syncevt(parseio, ctime, fmt->data, SYNC_END);
		  }
	      }
	  parseio->parse_data[parseio->parse_index] = '\0';
	  updated = timepacket(parseio);
	  bcopy(parseio->parse_data, parseio->parse_ldata, parseio->parse_index+1);
	  parseio->parse_ldsize = parseio->parse_index+1;
	  parseio->parse_index = 0;
          parseprintf(DD_PARSE, ("parse: parse_ioread: buffer end\n"));
	}
    }

  if ((updated == CVT_NONE) &&
      (parseio->parse_flags & PARSE_FIXED_FMT) &&
      (parseio->parse_syncflags & SYNC_SYNTHESIZE) &&
      ((parseio->parse_dtime.parse_status & CVT_MASK) == CVT_OK) &&
      clockformats[parseio->parse_lformat]->synth)
    {
      updated = clockformats[parseio->parse_lformat]->synth(parseio, ctime);
    }
  
  /*
   * remember last character time
   */
  parseio->parse_lastchar = *ctime;

#ifdef DEBUG
  if ((updated & CVT_MASK) != CVT_NONE)
    parseprintf(DD_PARSE, ("parse_ioread: time sample accumulated (status=0x%x)\n", updated));
#endif

  parseio->parse_dtime.parse_status = updated;

  return (updated & CVT_MASK) != CVT_NONE;
}

/*
 * parse_iopps
 *
 * take status line indication and derive synchronisation information
 * from it.
 * It can also be used to decode a serial serial data format (such as the
 * ONE, ZERO, MINUTE sync data stream from DCF77)
 */
/*ARGSUSED*/
int
parse_iopps(parseio, status, ptime)
  register parse_t *parseio;
  register int status;
  register timestamp_t *ptime;
{
  register unsigned updated = CVT_NONE;

  /*
   * PPS pulse information will only be delivered to ONE clock format
   * this is either the last successful conversion module with a ppssync
   * routine, or a fixed format with a ppssync routine
   */
  parseprintf(DD_PARSE, ("parse_iopps: STATUS %s\n", (status == SYNC_ONE) ? "ONE" : "ZERO"));

  if (((parseio->parse_flags & PARSE_FIXED_FMT) ||
       ((parseio->parse_dtime.parse_status & CVT_MASK) == CVT_OK)) &&
      clockformats[parseio->parse_lformat]->syncpps &&
      (status & clockformats[parseio->parse_lformat]->flags))
    {
      updated = clockformats[parseio->parse_lformat]->syncpps(parseio, status == SYNC_ONE, ptime);
      parseprintf(DD_PARSE, ("parse_iopps: updated = 0x%x\n", updated));
    }
  else
    {
      parseprintf(DD_PARSE, ("parse_iopps: STATUS dropped\n"));
    }

  return (updated & CVT_MASK) != CVT_NONE;
}

/*
 * parse_iodone
 *
 * clean up internal status for new round
 */
/*ARGSUSED*/
void
parse_iodone(parseio)
  register parse_t *parseio;
{
  /*
   * we need to clean up certain flags for the next round
   */
  parseio->parse_dtime.parse_state = 0; /* no problems with ISRs */
}

/*---------- conversion implementation --------------------*/

/*
 * convert a struct clock to UTC since Jan, 1st 1970 0:00 (the UNIX EPOCH)
 */
#define dysize(x)	((x) % 4 ? 365 : ((x % 400) ? 365 :366))

time_t
parse_to_unixtime(clock, cvtrtc)
  register clocktime_t   *clock;
  register unsigned LONG *cvtrtc;
{
#define SETRTC(_X_)	{ if (cvtrtc) *cvtrtc = (_X_); }
  static int days_of_month[] = 
    {
      0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
  register int i;
  time_t t;
  
  if (clock->utctime)
    return clock->utctime;	/* if the conversion routine gets it right away - why not */

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
  
  if (clock->second < 0 || clock->second > 60)	/* allow for LEAPs */
    {
      SETRTC(CVT_FAIL|CVT_BADTIME);
      return -1;		/* bad sec */
    }

  t  = TIMES60(t) + clock->second;

  t += clock->utcoffset;	/* warp to UTC */

				/* done */

  clock->utctime = t;		/* documentray only */

  return t;
}

/*--------------- format conversion -----------------------------------*/

int
Stoi(s, zp, cnt)
  char *s;
  LONG *zp;
  int cnt;
{
  char *b = s;
  int f,z,v;
  char c;

  f=z=v=0;

  while(*s == ' ')
    s++;
  
  if (*s == '-')
    {
      s++;
      v = 1;
    }
  else
    if (*s == '+')
      s++;
  
  for(;;)
    {
      c = *s++;
      if (c == '\0' || c < '0' || c > '9' || (cnt && ((s-b) > cnt)))
	{
	  if (f == 0)
	    {
	      return(-1);
	    }
	  if (v)
	    z = -z;
	  *zp = z;
	  return(0);
	}
      z = (z << 3) + (z << 1) + ( c - '0' );
      f=1;
    }
}


int
Strok(s, m)
  char *s;
  char *m;
{
  if (!s || !m)
    return 0;

  while(*s && *m)
    {
      if ((*m == ' ') ? 1 : (*s == *m))
	{
	  s++;
	  m++;
	}
      else
	{
	  return 0;
	}
    }
  return !*m;
}

unsigned LONG
updatetimeinfo(parseio, t, usec, flags)
  register parse_t         *parseio;
  register time_t         t;
  register unsigned LONG  usec;
  register unsigned LONG  flags;
{
  register LONG usecoff;
  register LONG mean;
  LONG delta[PARSE_DELTA];

#ifdef PARSEKERNEL
    usecoff = (t - parseio->parse_dtime.parse_stime.tv.tv_sec) * 1000000
      - parseio->parse_dtime.parse_stime.tv.tv_usec + usec;
#else
    extern LONG tstouslo[];
    extern LONG tstousmid[];
    extern LONG tstoushi[];

    TSFTOTVU(parseio->parse_dtime.parse_stime.fp.l_uf, usecoff);
    usecoff  = -usecoff;
    usecoff += (t - parseio->parse_dtime.parse_stime.fp.l_ui + JAN_1970) * 1000000
              + usec;
#endif
  
  /*
   * filtering (median) if requested
   */
  if (parseio->parse_flags & PARSE_STAT_FILTER)
    {
      register int n, i, s, k;
      
      parseio->parse_delta[parseio->parse_dindex] = usecoff;

      parseio->parse_dindex = (parseio->parse_dindex + 1) % PARSE_DELTA;

      /*
       * sort always - thus every sample gets its data
       */
      bcopy((caddr_t)parseio->parse_delta, (caddr_t)delta, sizeof(delta));
  
      for (s = 0; s < PARSE_DELTA; s++)
	for (k = s+1; k < PARSE_DELTA; k++)
	  {			/* Yes - it's slow sort */
	    if (delta[s] > delta[k]) 
	      {
		register LONG tmp;
		
		tmp      = delta[k];
		delta[k] = delta[s];
		delta[s] = tmp;
	      }
	  }

      i = 0;
      n = PARSE_DELTA;

      /*
       * you know this median loop if you have read the other code
       */
      while ((n - i) > 8)
	{
	  register LONG top = delta[n-1];
	  register LONG mid = delta[(n+i)>>1];
	  register LONG low = delta[i];
	  
	  if ((top - mid) > (mid - low))
	    {
	      /*
	       * cut off high end
	       */
	      n--;
	    }
	  else
	    {
	      /*
	       * cut off low end
	       */
	      i++;
	    }
	}
      
      parseio->parse_dtime.parse_usecdisp  = delta[n-1] - delta[i];

      if (parseio->parse_flags & PARSE_STAT_AVG)
	{
	  /*
	   * take the average of the median samples as this clock
	   * is a little bumpy
	   */
	  mean = 0;

	  while (i < n)
	    {
	      mean += delta[i++];
	    }

	  mean >>= 3;
	}
      else
	{
	  mean = delta[(n+i)>>1];
	}
      
      parseio->parse_dtime.parse_usecerror = mean;
    }
  else
    {
      parseio->parse_dtime.parse_usecerror = usecoff;
      parseio->parse_dtime.parse_usecdisp  = 0;
    }
  
  parseprintf(DD_PARSE,("parse: updatetimeinfo: T=%x+%d usec, useccoff=%d, usecerror=%d, usecdisp=%d\n",
		      t, usec, usecoff, parseio->parse_dtime.parse_usecerror, parseio->parse_dtime.parse_usecdisp));
  

#ifdef PARSEKERNEL
  {
    int s = splhigh();
#endif
  
    parseio->parse_lstate          = parseio->parse_dtime.parse_state | flags | PARSEB_TIMECODE;
    
    parseio->parse_dtime.parse_state = parseio->parse_lstate;

#ifdef PARSEKERNEL
    (void)splx(s);
  }
#endif
  
  return CVT_OK;		/* everything fine and dandy... */
}


/*
 * syn_simple
 *
 * handle a sync time stamp
 */
/*ARGSUSED*/
void
syn_simple(parseio, ts, format, why)
  register parse_t *parseio;
  register timestamp_t *ts;
  register struct format *format;
  register unsigned LONG why;
{
  parseio->parse_dtime.parse_stime = *ts;
}

/*
 * pps_simple
 *
 * handle a pps time stamp
 */
/*ARGSUSED*/
unsigned LONG
pps_simple(parseio, status, ptime)
  register parse_t *parseio;
  register int status;
  register timestamp_t *ptime;
{
  parseio->parse_dtime.parse_ptime  = *ptime;
  parseio->parse_dtime.parse_state |= PARSEB_PPS|PARSEB_S_PPS;
  
  return CVT_NONE;
}

/*
 * timepacket
 *
 * process a data packet
 */
static unsigned LONG
timepacket(parseio)
  register parse_t *parseio;
{
  register int k;
  register unsigned short format;
  register time_t t;
  register unsigned LONG cvtsum = 0;/* accumulated CVT_FAIL errors */
  unsigned LONG cvtrtc;		/* current conversion result */
  clocktime_t clock;
  
  format = parseio->parse_lformat;

  k = 0;

  if (parseio->parse_flags & PARSE_FIXED_FMT)
    {
      clock.utctime = 0;

      switch ((cvtrtc = clockformats[format]->convert ? clockformats[format]->convert(parseio->parse_data, parseio->parse_index, clockformats[format]->data, &clock) : CVT_NONE) & CVT_MASK)
	{
	case CVT_FAIL:
	  parseio->parse_badformat++;
	  cvtsum = cvtrtc & ~CVT_MASK;
	      
	  /*
	   * may be too often ... but is nice to know when it happens
	   */
#ifdef PARSEKERNEL
	  printf("parse: \"%s\" failed to convert\n", clockformats[format]->name);
#else
	  syslog(LOG_WARNING, "parse: \"%s\" failed to convert\n", clockformats[format]->name);
#endif
	  break;
	  
	case CVT_NONE:
	  /*
	   * too bad - pretend bad format
	   */
	  parseio->parse_badformat++;
	  cvtsum = CVT_BADFMT;

	  break;
	  
	case CVT_OK:
	  k = 1;
	  break;

	default:
	  /* shouldn't happen */
#ifdef PARSEKERNEL
	  printf("parse: INTERNAL error: bad return code of convert routine \"%s\"\n", clockformats[format]->name);
#else
	  syslog(LOG_WARNING, "parse: INTERNAL error: bad return code of convert routine \"%s\"\n", clockformats[format]->name);
#endif	  
	  return CVT_FAIL|cvtrtc;
	}
    }
  else
    {
      /*
       * find correct conversion routine
       * and convert time packet
       * RR search starting at last successful conversion routine
       */
  
      if (nformats)		/* very careful ... */
	{
	  do
	    {
              clock.utctime = 0;

	      switch ((cvtrtc = (clockformats[format]->convert && !(clockformats[format]->flags & CVT_FIXEDONLY)) ?
		       clockformats[format]->convert(parseio->parse_data, parseio->parse_index, clockformats[format]->data, &clock) :
		       CVT_NONE) & CVT_MASK)
		{
		case CVT_FAIL:
		  parseio->parse_badformat++;
		  cvtsum |= cvtrtc & ~CVT_MASK;
	      
		  /*
		   * may be too often ... but is nice to know when it happens
		   */
#ifdef PARSEKERNEL
		  printf("parse: \"%s\" failed to convert\n", clockformats[format]->name);
#else		  
		  syslog(LOG_WARNING, "parse: \"%s\" failed to convert\n", clockformats[format]->name);
#endif
		  /*FALLTHROUGH*/
		case CVT_NONE:
		  format++;
		  break;
	  
		case CVT_OK:
		  k = 1;
		  break;

		default:
		  /* shouldn't happen */
#ifdef PARSEKERNEL
		  printf("parse: INTERNAL error: bad return code of convert routine \"%s\"\n", clockformats[format]->name);
#else
		  syslog(LOG_WARNING, "parse: INTERNAL error: bad return code of convert routine \"%s\"\n", clockformats[format]->name);
#endif
		  return CVT_BADFMT;
		}
	      if (format >= nformats)
		format = 0;
	    }
	  while (!k && (format != parseio->parse_lformat));
	}
    }

  if (!k)
    {
#ifdef PARSEKERNEL
      printf("parse: time format \"%s\" not convertable\n", parseio->parse_data);
#else
      syslog(LOG_WARNING, "parse: time format \"%s\" not convertable\n", parseio->parse_data);
#endif
      return CVT_FAIL|cvtsum;
    }
  
  if ((t = parse_to_unixtime(&clock, &cvtrtc)) == -1)
    {
#ifdef PARSEKERNEL
      printf("parse: bad time format \"%s\"\n", parseio->parse_data);
#else
      syslog(LOG_WARNING,"parse: bad time format \"%s\"\n", parseio->parse_data);
#endif
      return CVT_FAIL|cvtrtc;
    }
  
  parseio->parse_lformat = format;

  /*
   * time stamp
   */
#ifdef PARSEKERNEL
  parseio->parse_dtime.parse_time.tv.tv_sec  = t;
  parseio->parse_dtime.parse_time.tv.tv_usec = clock.usecond;
#else
  parseio->parse_dtime.parse_time.fp.l_ui = t + JAN_1970;
  TVUTOTSF(clock.usecond, parseio->parse_dtime.parse_time.fp.l_uf);
#endif

  parseio->parse_dtime.parse_format       = format;

  return updatetimeinfo(parseio, t, clock.usecond, clock.flags);
}


/*
 * control operations
 */
/*ARGSUSED*/
int
parse_getstat(dct, parse)
  parsectl_t *dct;
  parse_t    *parse;
{
  dct->parsestatus.flags = parse->parse_flags & PARSE_STAT_FLAGS;
  return 1;
}

		  
/*ARGSUSED*/
int
parse_setstat(dct, parse)
  parsectl_t *dct;
  parse_t    *parse;
{
  parse->parse_flags = (parse->parse_flags & ~PARSE_STAT_FLAGS) | dct->parsestatus.flags;
  return 1;
}

		  
/*ARGSUSED*/
int
parse_timecode(dct, parse)
  parsectl_t *dct;
  parse_t    *parse;
{
  dct->parsegettc.parse_state  = parse->parse_lstate;
  dct->parsegettc.parse_format = parse->parse_lformat;
  /*
   * move out current bad packet count
   * user program is expected to sum these up
   * this is not a problem, as "parse" module are
   * exclusive open only
   */
  dct->parsegettc.parse_badformat = parse->parse_badformat;
  parse->parse_badformat = 0;
		  
  if (parse->parse_ldsize <= PARSE_TCMAX)
    {
      dct->parsegettc.parse_count = parse->parse_ldsize;
      bcopy(parse->parse_ldata, dct->parsegettc.parse_buffer, dct->parsegettc.parse_count);
      return 1;
    }
  else
    {
      return 0;
    }
}

		  
/*ARGSUSED*/
int
parse_setfmt(dct, parse)
  parsectl_t *dct;
  parse_t    *parse;
{
  if (dct->parseformat.parse_count <= PARSE_TCMAX)
    {
      if (dct->parseformat.parse_count)
	{
	  register unsigned short i;

	  for (i = 0; i < nformats; i++)
	    {
	      if (!Strcmp(dct->parseformat.parse_buffer, clockformats[i]->name))
		{
		  parse->parse_lformat  = i;
		  parse->parse_flags   |= PARSE_FIXED_FMT; /* set fixed format indication */
		  return setup_bitmaps(parse, i, i+1);
		}
	    }

	  return 0;
	}
      else
	{
	  parse->parse_flags &= ~PARSE_FIXED_FMT; /* clear fixed format indication */
	  return setup_bitmaps(parse, 0, nformats);
	}
    }
  else
    {
      return 0;
    }
}

/*ARGSUSED*/
int
parse_getfmt(dct, parse)
  parsectl_t *dct;
  parse_t    *parse;
{
  if (dct->parseformat.parse_format < nformats &&
      Strlen(clockformats[dct->parseformat.parse_format]->name) <= PARSE_TCMAX)
    {
      dct->parseformat.parse_count = Strlen(clockformats[dct->parseformat.parse_format]->name)+1;
      bcopy(clockformats[dct->parseformat.parse_format]->name, dct->parseformat.parse_buffer, dct->parseformat.parse_count);
      return 1;
    }
  else
    {
      return 0;
    }
}

/*ARGSUSED*/
int
parse_setcs(dct, parse)
  parsectl_t *dct;
  parse_t    *parse;
{
  parse->parse_ioflags &= ~PARSE_IO_CSIZE;
  parse->parse_ioflags |= dct->parsesetcs.parse_cs & PARSE_IO_CSIZE;
  return 1;
}

#endif /* defined(REFCLOCK) && defined(PARSE) */

/*
 * History:
 *
 * parse.c,v
 * Revision 3.22  1994/02/25  12:34:49  kardel
 * allow for converter generated utc times
 *
 * Revision 3.21  1994/02/02  17:45:30  kardel
 * rcs ids fixed
 *
 * Revision 3.19  1994/01/25  19:05:20  kardel
 * 94/01/23 reconcilation
 *
 * Revision 3.18  1994/01/23  17:21:59  kardel
 * 1994 reconcilation
 *
 * Revision 3.17  1993/11/11  11:20:29  kardel
 * declaration fixes
 *
 * Revision 3.16  1993/11/06  22:26:07  duwe
 * Linux cleanup after config change
 *
 * Revision 3.15  1993/11/04  11:14:18  kardel
 * ansi/K&R traps
 *
 * Revision 3.14  1993/11/04  10:03:28  kardel
 * disarmed ansiism
 *
 * Revision 3.13  1993/11/01  20:14:13  kardel
 * useless comparision removed
 *
 * Revision 3.12  1993/11/01  20:00:22  kardel
 * parse Solaris support (initial version)
 *
 * Revision 3.11  1993/10/30  09:41:25  kardel
 * minor optimizations
 *
 * Revision 3.10  1993/10/22  14:27:51  kardel
 * Oct. 22nd 1993 reconcilation
 *
 * Revision 3.9  1993/10/05  23:15:09  kardel
 * more STREAM protection
 *
 * Revision 3.8  1993/09/27  21:08:00  kardel
 * utcoffset now in seconds
 *
 * Revision 3.7  1993/09/26  23:40:16  kardel
 * new parse driver logic
 *
 * Revision 3.6  1993/09/07  10:12:46  kardel
 * September 7th reconcilation - 3.2 (alpha)
 *
 * Revision 3.5  1993/09/01  21:44:48  kardel
 * conditional cleanup
 *
 * Revision 3.4  1993/08/27  00:29:39  kardel
 * compilation cleanup
 *
 * Revision 3.3  1993/08/24  22:27:13  kardel
 * cleaned up AUTOCONF DCF77 mess 8-) - wasn't too bad
 *
 * Revision 3.2  1993/07/09  11:37:11  kardel
 * Initial restructured version + GPS support
 *
 * Revision 3.1  1993/07/06  10:00:08  kardel
 * DCF77 driver goes generic...
 *
 */
