#if defined(REFCLOCK) && (defined(PARSE) || defined(PARSEPPS)) && defined(CLOCK_TRIMTSIP)
/*
 * $Header: /home/ncvs/src/usr.sbin/xntpd/parse/clk_trimtsip.c,v 1.2 1995/05/30 03:54:13 rgrimes Exp $
 *
 * Trimble TSIP support - CURRENTLY VERY MUCH UNDER CONSTRUCTION
 */

#include "sys/types.h"
#include "sys/time.h"
#include "sys/errno.h"

#include "ntp_syslog.h"
#include "ntp_types.h"
#include "ntp_fp.h"
#include "ntp_unixtime.h"
#include "ntp_calendar.h"

#include "parse.h"

/*
 * Trimble TSIP parser
 *
 * The receiver uses a serial message protocol called Trimble Standard
 * Interface Protocol (it can support others but this driver only supports
 * TSIP). Messages in this protocol have the following form:
 *
 * <DLE><id> ... <data> ... <DLE><ETX>
 *
 * Any bytes within the <data> portion of value 10 hex (<DLE>) are doubled
 * on transmission and compressed back to one on reception. Otherwise
 * the values of data bytes can be anything. The serial interface is RS-422
 * asynchronous using 9600 baud, 8 data bits with odd party (**note** 9 bits
 * in total!), and 1 stop bit. The protocol supports byte, integer, single,
 * and double datatypes. Integers are two bytes, sent most significant first.
 * Singles are IEEE754 single precision floating point numbers (4 byte) sent
 * sign & exponent first. Doubles are IEEE754 double precision floating point
 * numbers (8 byte) sent sign & exponent first.
 * The receiver supports a large set of messages, only a small subset of
 * which are used here. From driver to receiver the following are used:
 *
 *  ID    Description
 *
 *  21    Request current time
 *  22    Mode Select
 *  2C    Set/Request operating parameters
 *  2F    Request UTC info
 *  35    Set/Request I/O options
 *
 * From receiver to driver the following are recognised:
 *
 *  ID    Description
 *
 *  41    GPS Time
 *  44    Satellite selection, PDOP, mode
 *  46    Receiver health
 *  4B    Machine code/status
 *  4C    Report operating parameters (debug only)
 *  4F    UTC correction data (used to get leap second warnings)
 *  55    I/O options (debug only)
 *
 * All others are accepted but ignored.
 *
 */

extern int debug;

struct trimble
{
  u_char  t_in_pkt;		/* first DLE received */
  u_char  t_dle;		/* subsequent DLE received */
  u_char  t_status;		/* last status */
  u_char  t_error;		/* last error */
  u_short t_week;		/* GPS week */
  u_short t_weekleap;		/* GPS week of next/last week */
  u_short t_dayleap;		/* day in week */
  u_short t_gpsutc;		/* GPS - UTC offset */
  u_short t_gpsutcleap;		/* offset at next/last leap */
  u_char  t_operable;		/* receiver feels OK */
  u_char  t_leap;		/* possible leap warning */
};

static unsigned LONG inp_tsip();
static unsigned LONG cvt_trimtsip();

struct clockformat clock_trimtsip =
{
  inp_tsip,			/* Trimble TSIP input handler */
  cvt_trimtsip,			/* Trimble TSIP conversion */
  syn_simple,			/* easy time stamps for RS232 (fallback) */
  pps_simple,			/* easy PPS monitoring */
  (unsigned LONG (*)())0,	/* no time code synthesizer monitoring */
  (void *)0,			/* no configuration data */
  "Trimble SV6/TSIP",
  128,				/* input buffer */
  CVT_FIXEDONLY,                /* we do our own input handling */
  sizeof(struct trimble),	/* no private data */
  { 0, 0},
  '\0',
  '\0',
  '\0'
};

#define ADDSECOND	0x01
#define DELSECOND	0x02

#define DLE		0x10
#define ETX		0x03

static unsigned LONG inp_tsip(parseio, ch, ctime)
     register parse_t      *parseio;
     register unsigned char ch;
     register timestamp_t  *ctime;
{
  register struct trimble *t = (struct trimble *)parseio->parse_pdata;

  if (!t)
    return 0;		/* local data not allocated - sigh! */

  if (!t->t_in_pkt && ch != DLE) {
    /* wait for start of packet */
#ifdef DEBUG
    if (debug > 2)
	printf("sv6+ discarding %2.2x\n", ch);
#endif
    return 0;
  }

  switch (ch) {
  case DLE:
    if (!t->t_in_pkt) {
      t->t_dle = 0;
      t->t_in_pkt = 1;
      parseio->parse_index = 0;
      parseio->parse_data[parseio->parse_index++] = ch;
      parseio->parse_dtime.parse_stime = *ctime; /* pick up time stamp at packet start */
    } else if (t->t_dle) {
      /* Double DLE -> insert a DLE */
      t->t_dle = 0;
      parseio->parse_data[parseio->parse_index++] = DLE;
    } else
      t->t_dle = 1;
    break;
  case ETX:
    if (t->t_dle) {
      /* DLE,ETX -> end of packet */
      parseio->parse_data[parseio->parse_index++] = DLE;
      parseio->parse_data[parseio->parse_index++] = ch;
      parseio->parse_data[parseio->parse_index] = '\0';
      parseio->parse_ldsize = parseio->parse_index+1;
      bcopy(parseio->parse_data, parseio->parse_ldata, parseio->parse_ldsize);
      t->t_in_pkt = t->t_dle = 0;
      return 1;
    }
    /* fall into ... */
  default:
    t->t_dle = 0;
    parseio->parse_data[parseio->parse_index++] = ch;
  }

  return (parseio->parse_index == parseio->parse_dsize-1); /* buffer full - attempt to parse (likely to fail) */
}

#define GPSORIGIN	2524953600	/* NTP origin - GPS origin in seconds */
#define SECSPERWEEK	604800	/* seconds per week - GPS tells us about weeks */
#define L_UF_SCALE	4294967296.0	/* scale a float fraction to l_uf units */

/*
 * mapping union for ints, floats, doubles for both input & output to the
 * receiver
 *
 * CAVEAT: must disappear - non portable
 */

union {
    u_char  bd[8];
    int     iv;
    float   fv;
    double  dv;
}  uval;

static float  getflt P((u_char *));
static double getdbl P((u_char *));
static int    getint P((u_char *));

/*
 * cvt_trimtsip
 *
 * convert TSIP type format
 */
static unsigned LONG
cvt_trimtsip(buffer, size, format, clock, t)
  register char          *buffer;
  register int            size;
  register struct format *format;
  register clocktime_t   *clock;
  register struct trimble *t;
{
#define mb(_X_) (buffer[2+(_X_)]) /* shortcut for buffer access */
  register u_char cmd;

  if (!t) {
#ifdef DEBUG
    if (debug) printf("sv6+ BAD call (t=0)\n");
#endif
    return CVT_NONE;		/* local data not allocated - sigh! */
  }

  if ((size < 4) ||
      (buffer[0]      != DLE) ||
      (buffer[size-1] != ETX) ||
      (buffer[size-2] != DLE))
    {
#ifdef DEBUG
      if (debug > 2) {
	 int i;

         printf("sv6+ BAD packet, size %d:\n	", size);
	 for (i = 0; i < size; i++) {
		printf ("%2.2x, ", buffer[i]&0xff);
		if (i%16 == 15) printf("\n\t");
	 }
	 printf("\n");
      }
#endif
      return CVT_NONE;
    }
  else
    {
      cmd = buffer[1];

#ifdef DEBUG
      if (debug > 1)
      switch(cmd)
	{
	case 0x41:
	  printf("sv6+ gps time: %f, %d, %f\n",
		 getflt(&mb(0)), getint(&mb(4)), getflt(&mb(6)));
	  break;

	case 0x44:
	  printf("sv6+ sats: %2x, %2d %2d %2d %2d, %.2f\n",
		 mb(0), mb(1), mb(2), mb(3), mb(4), getflt(&mb(5)));
	  break;

	case 0x45:
	  printf("sv6+ software: %d.%d (%d/%d/%d)\n",
		 mb(0)&0xff, mb(1)&0xff, (mb(4)&0xff)+1900, mb(2)&0xff, mb(3)&0xff);
	  break;

	case 0x46:
	  printf("sv6+ health: %2x %2x\n",
		 mb(0), mb(1));
	  break;

	case 0x48:
	  printf("sv6+ gps message: '%.22s'\n", &mb(0));
	  break;

	case 0x4b:
	  printf("sv6+ status: %2d %2x\n",
		 mb(0), mb(1));
	  break;

	case 0x4c:
	  printf("sv6+ op params: %2x %.1f %.1f %.1f %.1f\n",
		 mb(0), getflt(&mb(1)), getflt(&mb(5)),
		 getflt(&mb(9)), getflt(&mb(13)));
	  break;

	case 0x4f:
	  printf("sv6+ utc data: %.3e %.3e %d %d %d %d %d\n",
		 getdbl(&mb(0)), getflt(&mb(8)), getint(&mb(18)),
		 getint(&mb(12)), getint(&mb(20)), getint(&mb(22)), getint(&mb(24)));
	  break;

	case 0x54:
	  /*printf("sv6+ bias and rate: %.1fm %.2fm/s at %.1fs\n",
		 getflt(&mb(0)), getflt(&mb(4)), getflt(&mb(8))); ignore it*/
	  break;

	case 0x55:
	  printf("sv6+ io opts: %2x %2x %2x %2x\n",
		 mb(0), mb(1), mb(2), mb(3));
	  break;

	case 0x8f:
	  {
#define RTOD (180.0 / 3.1415926535898)
	  double lat = getdbl(&mb(2));
	  double lng = getdbl(&mb(10));
	  printf("sv6+ last fix: %2.2x %d lat %f %c, long %f %c, alt %.2fm\n",
		 mb(1)&0xff, mb(40)&0xff,
		 ((lat < 0) ? (-lat) : (lat))*RTOD, (lat < 0 ? 'S' : 'N'),
		 ((lng < 0) ? (-lng) : (lng))*RTOD, (lng < 0 ? 'W' : 'E'),
		 getdbl(&mb(18)));
	  }
	  break;

	case 0x40:
	case 0x5b:
	case 0x6d:
	  /* Ignore */
	  break;

	default:
	  printf("sv6+ cmd ignored: %2x, length: %d\n",
		 cmd, size);
	  break;
	}
#endif /* DEBUG */

	switch(cmd)
	  {
	  case 0x41:
	    {			/* GPS time */
	        float secs = getflt(&mb(0));
	        int   week = getint(&mb(4));
	        int   secint;
	        float secfrac;
	        l_fp gpstime, off;

	        if (secs <= 0)
		{
#ifdef DEBUG
		  if (debug)
		    printf("sv6+ seconds <= 0 (%e), setting POWERUP\n");
#endif
		  clock->flags = PARSEB_POWERUP;
		  return CVT_OK;
	        }

		/* time OK */
		secint  = secs; /* integer part, hopefully */
		secfrac = secs - secint; /* 0.0 <= secfrac < 1.0 */
		secint -= getflt(&mb(6)); /* UTC offset */
		gpstime.l_ui = week*SECSPERWEEK + secint + GPSORIGIN; /* convert to NTP time */
		gpstime.l_uf = secfrac*L_UF_SCALE;

		clock->utctime = gpstime.l_ui - JAN_1970;
		TSFTOTVU(gpstime.l_uf, clock->usecond);

		if (t->t_leap == ADDSECOND)
		    clock->flags |= PARSEB_LEAPADD;

		if (t->t_leap == DELSECOND)
		    clock->flags |= PARSEB_LEAPDEL;

		if (t->t_operable)
		    clock->flags &= ~(PARSEB_NOSYNC|PARSEB_POWERUP);
		else
		    clock->flags |= PARSEB_NOSYNC;
		return CVT_OK;

	    } /* case 0x41 */
	    break;

	  case 0x46:
	    {
	      /* sv6+ health */
	      u_char status = t->t_status = mb(0);
	      u_char error  = t->t_error  = mb(1);

	      if (status == 0 || status == 9 || status == 10 || status == 11)
		{
		  if (!t->t_operable)
	            syslog(LOG_ERR, "Trimble clock synced");
		  t->t_operable = 1;
		}
	      else
		{
		  if (t->t_operable)
	            syslog(LOG_ERR, "Trimble clock unsynced");
		  t->t_operable = 0;
		}
	    }
	    break;

	  case 0x4f:
	    {
	      /* UTC correction data - derive a leap warning */
	      int tls   = t->t_gpsutc     = getint(&mb(12)); /* current leap correction (GPS-UTC) */
	      int wnlsf = t->t_weekleap   = getint(&mb(20)); /* week no of leap correction */
	      int dn    = t->t_dayleap    = getint(&mb(22)); /* day in week of leap correction */
	      int tlsf  = t->t_gpsutcleap = getint(&mb(24)); /* new leap correction */
	      U_LONG now, leaptime;

	      t->t_week = getint(&mb(18)); /* current week no */

	      /* this stuff hasn't been tested yet... */
	      now = clock->utctime + JAN_1970;	/* now in GPS seconds */
	      leaptime = (wnlsf*7 + dn)*86400;	/* time of leap in GPS seconds */
	      if ((leaptime > now) && ((leaptime-now) < 86400*28))
		{
		  /* generate a leap warning */
		  if (tlsf > tls)
		    t->t_leap = ADDSECOND;
		  else
		    t->t_leap = DELSECOND;
		}
	      else
		{
		  t->t_leap = 0;
		}
	    }
	    break;

	  default:
	    /* it's validly formed, but we don't care about it! */
	    break;
	  }
    }
    return CVT_SKIP;
}

/*
 * getflt, getdbl, getint convert fields in the incoming data into the
 * appropriate type of item
 *
 * CAVEAT: these routines are currently definitely byte order dependent
 * and assume Representation(float) == IEEE754
 * These functions MUST be converted to portable versions (especially
 * converting the float representation into ntp_fp formats in order
 * to avoid floating point operations at all!
 */

static float
getflt(bp)
  u_char *bp;
{
  uval.bd[0] = *bp++;
  uval.bd[1] = *bp++;
  uval.bd[2] = *bp++;
  uval.bd[3] = *bp;
  return uval.fv;
}

static double
getdbl(bp)
  u_char *bp;
{
  uval.bd[0] = *bp++;
  uval.bd[1] = *bp++;
  uval.bd[2] = *bp++;
  uval.bd[3] = *bp++;
  uval.bd[4] = *bp++;
  uval.bd[5] = *bp++;
  uval.bd[6] = *bp++;
  uval.bd[7] = *bp;
  return uval.dv;
}

static int
getint(bp)
  u_char *bp;
{
  uval.bd[2] = *bp++;
  uval.bd[3] = *bp;
  if (uval.bd[2] & 0x80) /* sign-extend */
    uval.bd[0] = uval.bd[1] = 0xff;
  else
    uval.bd[0] = uval.bd[1] = 0;
  return uval.iv;
}

#endif /* defined(PARSE) && defined(CLOCK_TRIMTSIP) */

/*
 * History:
 *
 * $Log: clk_trimtsip.c,v $
 * Revision 1.2  1995/05/30 03:54:13  rgrimes
 * Remove trailing whitespace.
 *
 * Revision 1.1.1.1  1994/09/29  23:01:31  wollman
 * xntp 3.4e from Dave Mills @ UDel
 *
 */
