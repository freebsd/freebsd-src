/*
 *
 * refclock_neoclock4x.c
 * - NeoClock4X driver for DCF77 or FIA Timecode
 *
 * Date: 2002-04-27 1.0
 *
 * see http://www.linum.com/redir/jump/id=neoclock4x&action=redir
 * for details about the NeoClock4X device
 *
 * Copyright (C) 2002 by Linum Software GmbH <support@linum.com>
 *                                    
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#if defined(REFCLOCK) && (defined(CLOCK_NEOCLOCK4X))

#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <ctype.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_control.h"
#include "ntp_refclock.h"
#include "ntp_unixtime.h"
#include "ntp_stdlib.h"

#if defined HAVE_SYS_MODEM_H
# include <sys/modem.h>
# define TIOCMSET MCSETA
# define TIOCMGET MCGETA
# define TIOCM_RTS MRTS
#endif

#ifdef HAVE_TERMIOS_H
# ifdef TERMIOS_NEEDS__SVID3
#  define _SVID3
# endif
# include <termios.h>
# ifdef TERMIOS_NEEDS__SVID3
#  undef _SVID3
# endif
#endif

#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif

#define NEOCLOCK4X_TIMECODELEN 37

#define NEOCLOCK4X_OFFSET_SERIAL            3
#define NEOCLOCK4X_OFFSET_RADIOSIGNAL       9
#define NEOCLOCK4X_OFFSET_DAY              12
#define NEOCLOCK4X_OFFSET_MONTH            14
#define NEOCLOCK4X_OFFSET_YEAR             16
#define NEOCLOCK4X_OFFSET_HOUR             18
#define NEOCLOCK4X_OFFSET_MINUTE           20
#define NEOCLOCK4X_OFFSET_SECOND           22
#define NEOCLOCK4X_OFFSET_HSEC             24
#define NEOCLOCK4X_OFFSET_DOW              26
#define NEOCLOCK4X_OFFSET_TIMESOURCE       28
#define NEOCLOCK4X_OFFSET_DSTSTATUS        29
#define NEOCLOCK4X_OFFSET_QUARZSTATUS      30
#define NEOCLOCK4X_OFFSET_ANTENNA1         31
#define NEOCLOCK4X_OFFSET_ANTENNA2         33
#define NEOCLOCK4X_OFFSET_CRC              35

struct neoclock4x_unit {
  l_fp	laststamp;	/* last receive timestamp */
  short	unit;		/* NTP refclock unit number */
  u_long	polled;		/* flag to detect noreplies */
  char	leap_status;	/* leap second flag */
  int	recvnow;
  
  char  firmware[80];
  char  serial[7];
  char  radiosignal[4];
  char  timesource;
  char  dststatus;
  char  quarzstatus;
  int   antenna1;
  int   antenna2;
  int   utc_year;
  int   utc_month;
  int   utc_day;
  int   utc_hour;
  int   utc_minute;
  int   utc_second;
  int   utc_msec;
};

static	int	neoclock4x_start        P((int, struct peer *));
static	void	neoclock4x_shutdown	P((int, struct peer *));
static	void	neoclock4x_receive	P((struct recvbuf *));
static	void	neoclock4x_poll		P((int, struct peer *));
static	void	neoclock4x_control      P((int, struct refclockstat *, struct refclockstat *, struct peer *));

static int      neol_atoi_len           P((const char str[], int *, int));
static int      neol_hexatoi_len        P((const char str[], int *, int));
static void     neol_jdn_to_ymd         P((unsigned long, int *, int *, int *));
static void     neol_localtime          P((unsigned long, int* , int*, int*, int*, int*, int*));
static unsigned long neol_mktime        P((int, int, int, int, int, int));
static void     neol_mdelay             P((int));
static int      neol_query_firmware     P((int, int, char *, int));

struct refclock refclock_neoclock4x = {
  neoclock4x_start,	/* start up driver */
  neoclock4x_shutdown,	/* shut down driver */
  neoclock4x_poll,	/* transmit poll message */
  neoclock4x_control,
  noentry,		/* initialize driver (not used) */
  noentry,		/* not used */
  NOFLAGS			/* not used */
};

static int
neoclock4x_start(int unit,
		 struct peer *peer)
{
  struct neoclock4x_unit *up;
  struct refclockproc *pp;
  int fd;
  char dev[20];
  int sl232;
  struct termios termsettings;
  int tries;

  (void) sprintf(dev, "/dev/neoclock4x-%d", unit);
  
  /* LDISC_STD, LDISC_RAW
   * Open serial port. Use CLK line discipline, if available.
   */
  fd = refclock_open(dev, B2400, LDISC_CLK);
  if(fd <= 0)
    {
      return (0);
    }
  
#if defined(TIOCMSET) && (defined(TIOCM_RTS) || defined(CIOCM_RTS))
  /* turn on RTS, and DTR for power supply */
  /* NeoClock4x is powered from serial line */
  if(ioctl(fd, TIOCMGET, (caddr_t)&sl232) == -1)
    {
      msyslog(LOG_CRIT, "NeoClock4X(%d): can't query RTS/DTR state: %m", unit);
    }
#ifdef TIOCM_RTS
  sl232 = sl232 | TIOCM_DTR | TIOCM_RTS;	/* turn on RTS, and DTR for power supply */
#else
  sl232 = sl232 | CIOCM_DTR | CIOCM_RTS;	/* turn on RTS, and DTR for power supply */
#endif
  if(ioctl(fd, TIOCMSET, (caddr_t)&sl232) == -1)
    {
      msyslog(LOG_CRIT, "NeoClock4X(%d): can't set RTS/DTR to power neoclock4x: %m", unit);
    }
  
  if(ioctl(fd, TCGETS, (caddr_t)&termsettings) == -1)
    {
      msyslog(LOG_CRIT, "NeoClock4X(%d): can't query serial port settings: %m", unit);
    }
  
  /* 2400 Baud mit 8N2 */
  termsettings.c_cflag &= ~PARENB;
  termsettings.c_cflag |= CSTOPB;
  termsettings.c_cflag &= ~CSIZE;
  termsettings.c_cflag |= CS8;
  
  if(ioctl(fd, TCSETS, &termsettings) == -1)
    {
      msyslog(LOG_CRIT, "NeoClock4X(%d): can't set serial port to 2400 8N2: %m", unit);
    }
#else
  msyslog(LOG_EMERG, "NeoClock4X(%d): OS interface is incapable of setting DTR/RTS to power NeoClock4X",
	  unit);
#endif
  
  up = (struct neoclock4x_unit *) emalloc(sizeof(struct neoclock4x_unit));
  if(!(up))
    {
      msyslog(LOG_ERR, "NeoClock4X(%d): can't allocate memory for: %m",unit);
      (void) close(fd);
      return (0);
    }

  memset((char *)up, 0, sizeof(struct neoclock4x_unit));
  pp = peer->procptr;
  pp->clockdesc = "NeoClock4X";
  pp->unitptr = (caddr_t)up;
  pp->io.clock_recv = neoclock4x_receive;
  pp->io.srcclock = (caddr_t)peer;
  pp->io.datalen = 0;
  pp->io.fd = fd;
  /* no time is given by user! use 169.583333 ms to compensate the serial line delay
   * formula is:
   * 2400 Baud / 11 bit = 218.18 charaters per second
   *  (NeoClock4X timecode len)
   */
  pp->fudgetime1 = (NEOCLOCK4X_TIMECODELEN * 11) / 2400.0;

  if (!io_addclock(&pp->io))
    {
      msyslog(LOG_ERR, "NeoClock4X(%d): error add peer to ntpd: %m",unit);
      (void) close(fd);
      free(up);
      return (0);
    }
  
  /*
   * Initialize miscellaneous variables
   */
  peer->precision = -10;
  peer->burst = NSTAGE;
  memcpy((char *)&pp->refid, "neol", 4);
  
  up->leap_status = 0;
  up->unit = unit;
  strcpy(up->firmware, "?");
  strcpy(up->serial, "?");
  strcpy(up->radiosignal, "?");
  up->timesource  = '?';
  up->dststatus   = '?';
  up->quarzstatus = '?';
  up->antenna1    = -1;
  up->antenna2    = -1;
  up->utc_year    = 0;
  up->utc_month   = 0;
  up->utc_day     = 0;
  up->utc_hour    = 0;
  up->utc_minute  = 0;
  up->utc_second  = 0;
  up->utc_msec    = 0;

  for(tries=0; tries < 5; tries++)
    {
      /*
       * Wait 3 second for receiver to power up
       */
      NLOG(NLOG_CLOCKINFO)
	msyslog(LOG_INFO, "NeoClock4X(%d): try query NeoClock4X firmware version (%d/5)", unit, tries);
      sleep(3);
      if(neol_query_firmware(pp->io.fd, up->unit, up->firmware, sizeof(up->firmware)))
	{
	  break;
	}
    }

  NLOG(NLOG_CLOCKINFO)
    msyslog(LOG_INFO, "NeoClock4X(%d): receiver setup successful done", unit);
  
  return (1);
}

static void
neoclock4x_shutdown(int unit,
		   struct peer *peer)
{
  struct neoclock4x_unit *up;
  struct refclockproc *pp;
  int sl232;

  pp = peer->procptr;
  up = (struct neoclock4x_unit *)pp->unitptr;

#if defined(TIOCMSET) && (defined(TIOCM_RTS) || defined(CIOCM_RTS))
  /* turn on RTS, and DTR for power supply */
  /* NeoClock4x is powered from serial line */
  if(ioctl(pp->io.fd, TIOCMGET, (caddr_t)&sl232) == -1)
    {
      msyslog(LOG_CRIT, "NeoClock4X(%d): can't query RTS/DTR state: %m", unit);
    }
#ifdef TIOCM_RTS
  sl232 &= ~(TIOCM_DTR | TIOCM_RTS);	/* turn on RTS, and DTR for power supply */
#else
  sl232 &= ~(CIOCM_DTR | CIOCM_RTS);	/* turn on RTS, and DTR for power supply */
#endif
  if(ioctl(pp->io.fd, TIOCMSET, (caddr_t)&sl232) == -1)
    {
      msyslog(LOG_CRIT, "NeoClock4X(%d): can't set RTS/DTR to power neoclock4x: %m", unit);
    }
#endif
  msyslog(LOG_ERR, "NeoClock4X(%d): shutdown", unit);

  io_closeclock(&pp->io);
  free(up);
  NLOG(NLOG_CLOCKINFO)
    msyslog(LOG_INFO, "NeoClock4X(%d): receiver shutdown done", unit);
}

static void
neoclock4x_receive(struct recvbuf *rbufp)
{
  struct neoclock4x_unit *up;
  struct refclockproc *pp;
  struct peer *peer;
  unsigned long calc_utc;
  int day;
  int month;	/* ddd conversion */
  int c;
  unsigned char calc_chksum;
  int recv_chksum;
  
  peer = (struct peer *)rbufp->recv_srcclock;
  pp = peer->procptr;
  up = (struct neoclock4x_unit *)pp->unitptr;
  
  /* wait till poll interval is reached */
  if(0 == up->recvnow)
    return;
  
  /* reset poll interval flag */
  up->recvnow = 0;

  /* read last received timecode */
  pp->lencode = refclock_gtlin(rbufp, pp->a_lastcode, BMAX, &pp->lastrec);
  
  if(NEOCLOCK4X_TIMECODELEN != pp->lencode)
    {
      NLOG(NLOG_CLOCKEVENT)
	msyslog(LOG_WARNING, "NeoClock4X(%d): received data has invalid length, expected %d bytes, received %d bytes: %s",
		up->unit, NEOCLOCK4X_TIMECODELEN, pp->lencode, pp->a_lastcode);
      refclock_report(peer, CEVNT_BADREPLY);
      return;
    }

  neol_hexatoi_len(&pp->a_lastcode[NEOCLOCK4X_OFFSET_CRC], &recv_chksum, 2);

  /* calculate checksum */
  calc_chksum = 0;
  for(c=0; c < NEOCLOCK4X_OFFSET_CRC; c++)
    {
      calc_chksum += pp->a_lastcode[c];
    }
  if(recv_chksum != calc_chksum)
    {
      NLOG(NLOG_CLOCKEVENT)
	msyslog(LOG_WARNING, "NeoClock4X(%d): received data has invalid chksum: %s",
		up->unit, pp->a_lastcode);
      refclock_report(peer, CEVNT_BADREPLY);
      return;
    }

  /* Allow synchronization even is quartz clock is
   * never initialized.
   * WARNING: This is dangerous!
   */  
  up->quarzstatus = pp->a_lastcode[NEOCLOCK4X_OFFSET_QUARZSTATUS];
  if(0==(pp->sloppyclockflag & CLK_FLAG2))
    {
      if('I' != up->quarzstatus)
	{
	  NLOG(NLOG_CLOCKEVENT)
	    msyslog(LOG_NOTICE, "NeoClock4X(%d): quartz clock is not initialized: %s",
		    up->unit, pp->a_lastcode);
	  pp->leap = LEAP_NOTINSYNC;
	  refclock_report(peer, CEVNT_BADDATE);
	  return;
	}
    }
  if('I' != up->quarzstatus)
    {
      NLOG(NLOG_CLOCKEVENT)
	msyslog(LOG_NOTICE, "NeoClock4X(%d): using uninitialized quartz clock for time synchronization: %s",
		up->unit, pp->a_lastcode);
    }
  
  /*
   * If NeoClock4X is not synchronized to a radio clock 
   * check if we're allowed to synchronize with the quartz
   * clock.
   */
  up->timesource = pp->a_lastcode[NEOCLOCK4X_OFFSET_TIMESOURCE];
  if(0==(pp->sloppyclockflag & CLK_FLAG2))
    {
      if('A' != up->timesource)
	{
	  /* not allowed to sync with quartz clock */
	  if(0==(pp->sloppyclockflag & CLK_FLAG1))
	    {
	      refclock_report(peer, CEVNT_BADTIME);
	      pp->leap = LEAP_NOTINSYNC;
	      return;
	    }
	}
    }

  /* this should only used when first install is done */
  if(pp->sloppyclockflag & CLK_FLAG4)
    {
      msyslog(LOG_DEBUG, "NeoClock4X(%d): received data: %s",
	      up->unit, pp->a_lastcode);
    }

  /* 123456789012345678901234567890123456789012345 */
  /* S/N123456DCF1004021010001202ASX1213CR\r\n */

  neol_atoi_len(&pp->a_lastcode[NEOCLOCK4X_OFFSET_YEAR], &pp->year, 2);
  neol_atoi_len(&pp->a_lastcode[NEOCLOCK4X_OFFSET_MONTH], &month, 2);
  neol_atoi_len(&pp->a_lastcode[NEOCLOCK4X_OFFSET_DAY], &day, 2);
  neol_atoi_len(&pp->a_lastcode[NEOCLOCK4X_OFFSET_HOUR], &pp->hour, 2);
  neol_atoi_len(&pp->a_lastcode[NEOCLOCK4X_OFFSET_MINUTE], &pp->minute, 2);
  neol_atoi_len(&pp->a_lastcode[NEOCLOCK4X_OFFSET_SECOND], &pp->second, 2);
  neol_atoi_len(&pp->a_lastcode[NEOCLOCK4X_OFFSET_HSEC], &pp->msec, 2);
  pp->msec *= 10; /* convert 1/100s from neoclock to real miliseconds */
  
  memcpy(up->radiosignal, &pp->a_lastcode[NEOCLOCK4X_OFFSET_RADIOSIGNAL], 3);
  up->radiosignal[3] = 0;
  memcpy(up->serial, &pp->a_lastcode[NEOCLOCK4X_OFFSET_SERIAL], 6);
  up->serial[6] = 0;
  up->dststatus = pp->a_lastcode[NEOCLOCK4X_OFFSET_DSTSTATUS];
  neol_hexatoi_len(&pp->a_lastcode[NEOCLOCK4X_OFFSET_ANTENNA1], &up->antenna1, 2);
  neol_hexatoi_len(&pp->a_lastcode[NEOCLOCK4X_OFFSET_ANTENNA2], &up->antenna2, 2);

  /*
    Validate received values at least enough to prevent internal
    array-bounds problems, etc.
  */
  if((pp->hour < 0) || (pp->hour > 23) ||
     (pp->minute < 0) || (pp->minute > 59) ||
     (pp->second < 0) || (pp->second > 60) /*Allow for leap seconds.*/ ||
     (day < 1) || (day > 31) ||
     (month < 1) || (month > 12) ||
     (pp->year < 0) || (pp->year > 99)) {
    /* Data out of range. */
    NLOG(NLOG_CLOCKEVENT)
      msyslog(LOG_WARNING, "NeoClock4X(%d): date/time out of range: %s",
	      up->unit, pp->a_lastcode);
    refclock_report(peer, CEVNT_BADDATE);
    return;
  }
  
  /* Year-2000 check! */
  /* wrap 2-digit date into 4-digit */
  
  if(pp->year < YEAR_PIVOT) /* < 98 */
    {
      pp->year += 100;
    }
  pp->year += 1900;
  
  calc_utc = neol_mktime(pp->year, month, day, pp->hour, pp->minute, pp->second);
  calc_utc -= 3600;
  if('S' == up->dststatus)
    calc_utc -= 3600;
  neol_localtime(calc_utc, &pp->year, &month, &day, &pp->hour, &pp->minute, &pp->second);

  /*
    some preparations
  */
  pp->day = ymd2yd(pp->year,month,day);
  pp->leap = 0;
  

  if(pp->sloppyclockflag & CLK_FLAG4)
    {
      msyslog(LOG_DEBUG, "NeoClock4X(%d): calculated UTC date/time: %04d-%02d-%02d %02d:%02d:%02d.%03d",
	      up->unit,
	      pp->year, month, day,
	      pp->hour, pp->minute, pp->second, pp->msec);
    }

  up->utc_year   = pp->year;
  up->utc_month  = month;
  up->utc_day    = day;
  up->utc_hour   = pp->hour;
  up->utc_minute = pp->minute;
  up->utc_second = pp->second;
  up->utc_msec   = pp->msec;
  
  if(!refclock_process(pp))
    {
      NLOG(NLOG_CLOCKEVENT)
	msyslog(LOG_WARNING, "NeoClock4X(%d): refclock_process failed!", up->unit);
      refclock_report(peer, CEVNT_FAULT);
      return;
    }
  refclock_receive(peer);
  
  record_clock_stats(&peer->srcadr, pp->a_lastcode);
}

static void
neoclock4x_poll(int unit,
		struct peer *peer)
{
  struct neoclock4x_unit *up;
  struct refclockproc *pp;

  pp = peer->procptr;
  up = (struct neoclock4x_unit *)pp->unitptr;

  pp->polls++;
  up->recvnow = 1;
}

static void
neoclock4x_control(int unit,
		   struct refclockstat *in,
		   struct refclockstat *out,
		   struct peer *peer)
{
  struct neoclock4x_unit *up;
  struct refclockproc *pp;
  
  if(NULL == peer)
    {
      msyslog(LOG_ERR, "NeoClock4X(%d): control: unit invalid/inactive", unit);
      return;
    }

  pp = peer->procptr;
  if(NULL == pp)
    {
      msyslog(LOG_ERR, "NeoClock4X(%d): control: unit invalid/inactive", unit);
      return;
    }

  up = (struct neoclock4x_unit *)pp->unitptr;
  if(NULL == up)
    {
      msyslog(LOG_ERR, "NeoClock4X(%d): control: unit invalid/inactive", unit);
      return;
    }

  if(NULL != in)
    {
      /* check to see if a user supplied time offset is given */
      if(in->haveflags & CLK_HAVETIME1)
	{
	  pp->fudgetime1 = in->fudgetime1;
	  NLOG(NLOG_CLOCKINFO)
	    msyslog(LOG_NOTICE, "NeoClock4X(%d): using fudgetime1 with %0.5fs from ntp.conf.",
		    unit, pp->fudgetime1);
	}
      
      /* notify */
      if(pp->sloppyclockflag & CLK_FLAG1)
	{
	  NLOG(NLOG_CLOCKINFO)
	    msyslog(LOG_NOTICE, "NeoClock4X(%d): quartz clock is used to synchronize time if radio clock has no reception.", unit);
	}
      else
	{
	  NLOG(NLOG_CLOCKINFO)
	    msyslog(LOG_NOTICE, "NeoClock4X(%d): time is only adjusted with radio signal reception.", unit);
	}
    }

  if(NULL != out)
    {
      static char outstatus[800];	/* status output buffer */
      char *tt;
      char tmpbuf[80];
      
      outstatus[0] = '\0';
      out->kv_list = (struct ctl_var *)0;
      out->type    = REFCLK_NEOCLOCK4X;
      
      sprintf(tmpbuf, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
	      up->utc_year, up->utc_month, up->utc_day,
	      up->utc_hour, up->utc_minute, up->utc_second,
	      up->utc_msec);
      
      tt = add_var(&out->kv_list, 512, RO|DEF);
      tt += sprintf(tt, "calc_utc=\"%s\"", tmpbuf);
      tt = add_var(&out->kv_list, 512, RO|DEF);
      tt += sprintf(tt, "radiosignal=\"%s\"", up->radiosignal);
      tt = add_var(&out->kv_list, 512, RO|DEF);
      tt += sprintf(tt, "antenna1=\"%d\"", up->antenna1);
      tt = add_var(&out->kv_list, 512, RO|DEF);
      tt += sprintf(tt, "antenna2=\"%d\"", up->antenna2);
      tt = add_var(&out->kv_list, 512, RO|DEF);
      if('A' == up->timesource)
	tt += sprintf(tt, "timesource=\"radio\"");
      else if('C' == up->timesource)
	tt += sprintf(tt, "timesource=\"quartz\"");
      else
	tt += sprintf(tt, "timesource=\"unknown\"");
      tt = add_var(&out->kv_list, 512, RO|DEF);
      if('I' == up->quarzstatus)
	tt += sprintf(tt, "quartzstatus=\"synchronized\"");
      else if('X' == up->quarzstatus)
        tt += sprintf(tt, "quartzstatus=\"not synchronized\"");
      else
	tt += sprintf(tt, "quartzstatus=\"unknown\"");
      tt = add_var(&out->kv_list, 512, RO|DEF);
      if('S' == up->dststatus)
        tt += sprintf(tt, "dststatus=\"summer\"");
      else if('W' == up->dststatus)
        tt += sprintf(tt, "dststatus=\"winter\"");
      else
        tt += sprintf(tt, "dststatus=\"unknown\"");
      tt = add_var(&out->kv_list, 512, RO|DEF);
      tt += sprintf(tt, "firmware=\"%s\"", up->firmware);
      tt = add_var(&out->kv_list, 512, RO|DEF);
      tt += sprintf(tt, "serialnumber=\"%s\"", up->serial);
      tt = add_var(&out->kv_list, 512, RO|DEF);
    }
}

static int neol_hexatoi_len(const char str[],
			    int *result,
			    int maxlen)
{
  int hexdigit;
  int i;
  int n = 0;
  
  for(i=0; isxdigit(str[i]) && i < maxlen; i++)
    {
      hexdigit = isdigit(str[i]) ? toupper(str[i]) - '0' : toupper(str[i]) - 'A' + 10;
      n = 16 * n + hexdigit;
    }
  *result = n;
  return (n);
}

int neol_atoi_len(const char str[],
		  int *result,
		  int maxlen)
{
  int digit;
  int i;
  int n = 0;
  
  for(i=0; isdigit(str[i]) && i < maxlen; i++)
    {
      digit = str[i] - '0';
      n = 10 * n + digit;
    }
  *result = n;
  return (n);
}

/* Converts Gregorian date to seconds since 1970-01-01 00:00:00.
 * Assumes input in normal date format, i.e. 1980-12-31 23:59:59
 * => year=1980, mon=12, day=31, hour=23, min=59, sec=59.
 *
 * [For the Julian calendar (which was used in Russia before 1917,
 * Britain & colonies before 1752, anywhere else before 1582,
 * and is still in use by some communities) leave out the
 * -year/100+year/400 terms, and add 10.]
 *
 * This algorithm was first published by Gauss (I think).
 *
 * WARNING: this function will overflow on 2106-02-07 06:28:16 on
 * machines were long is 32-bit! (However, as time_t is signed, we
 * will already get problems at other places on 2038-01-19 03:14:08)
 */
static unsigned long neol_mktime(int year,
				 int mon,
				 int day, 
				 int hour,
				 int min,
				 int sec)
{
  if (0 >= (int) (mon -= 2)) {    /* 1..12 . 11,12,1..10 */
    mon += 12;      /* Puts Feb last since it has leap day */
    year -= 1;
  }
  return (((
            (unsigned long)(year/4 - year/100 + year/400 + 367*mon/12 + day) +
            year*365 - 719499
            )*24 + hour /* now have hours */
           )*60 + min /* now have minutes */
          )*60 + sec; /* finally seconds */
}

static void neol_localtime(unsigned long utc,
			   int* year,
			   int* month,
			   int* day,
			   int* hour,
			   int* minute,
			   int* second)
{
  ldiv_t d;
  
  /* Sekunden */
  d  = ldiv(utc, 60);
  *second = d.rem;
  
  /* Minute */
  d  = ldiv(d.quot, 60);
  *minute = d.rem;
  
  /* Stunden */
  d  = ldiv(d.quot, 24);
  *hour = d.rem;
  
  /*             JDN Date 1/1/1970 */
  neol_jdn_to_ymd(d.quot + 2440588L, year, month, day);
}

static void neol_jdn_to_ymd(unsigned long jdn, 
			    int *yy,
			    int *mm, 
			    int *dd)
{
  unsigned long x, z, m, d, y;
  unsigned long daysPer400Years = 146097UL;
  unsigned long fudgedDaysPer4000Years = 1460970UL + 31UL;
  
  x = jdn + 68569UL;
  z = 4UL * x / daysPer400Years;
  x = x - (daysPer400Years * z + 3UL) / 4UL;
  y = 4000UL * (x + 1) / fudgedDaysPer4000Years;
  x = x - 1461UL * y / 4UL + 31UL;
  m = 80UL * x / 2447UL;
  d = x - 2447UL * m / 80UL;
  x = m / 11UL;
  m = m + 2UL - 12UL * x;
  y = 100UL * (z - 49UL) + y + x;
  
  *yy = (int)y;
  *mm = (int)m;
  *dd = (int)d;
}

/* 
 *  delay in milliseconds 
 */ 
static void 
neol_mdelay(int milliseconds) 
{ 
  struct timeval        tv; 
  
  if (milliseconds)
    { 
      tv.tv_sec  = 0; 
      tv.tv_usec = milliseconds * 1000; 
      select(1, NULL, NULL, NULL, &tv); 
    } 
}

static int
neol_query_firmware(int fd,
		    int unit,
		    char *firmware,
		    int maxlen)
{
  unsigned char tmpbuf[256];
  int len;
  int lastsearch;
  unsigned char c;
  int last_c_was_crlf;
  int last_crlf_conv_len;
  int init;
  int read_tries;
  int flag = 0;

  /* wait a little bit */
  neol_mdelay(250);
  if(-1 != write(fd, "V", 1))
    {
      /* wait a little bit */
      neol_mdelay(250);
      memset(tmpbuf, 0x00, sizeof(tmpbuf));
      
      len = 0;
      lastsearch = 0;
      last_c_was_crlf = 0;
      last_crlf_conv_len = 0;
      init = 1;
      read_tries = 0;
      for(;;)
	{
	  if(read_tries++ > 500)
	    {
	      msyslog(LOG_ERR, "NeoClock4X(%d): can't read firmware version (timeout)", unit);
	      strcpy(tmpbuf, "unknown due to timeout");
	      break;
	    }
	  if(-1 == read(fd, &c, 1))
	    {
	      neol_mdelay(25);
	      continue;
	    }
	  if(init)
	    {
	      if(0xA9 != c) /* wait for (c) char in input stream */
		continue;
	      
	      strcpy(tmpbuf, "(c)");
	      len = 3;
	      init = 0;
	      continue;
	    }
	  
	  //msyslog(LOG_NOTICE, "NeoClock4X(%d): firmware %c = %02Xh", unit, c, c);
	  if(0x0A == c || 0x0D == c)
	    {
	      if(last_c_was_crlf)
		{
		  char *ptr;
		  ptr = strstr(&tmpbuf[lastsearch], "S/N");
		  if(NULL != ptr)
		    {
		      tmpbuf[last_crlf_conv_len] = 0;
		      flag = 1;
		      break;
		    }
		  /* convert \n to / */
		  last_crlf_conv_len = len;
		  tmpbuf[len++] = ' ';
		  tmpbuf[len++] = '/';
		  tmpbuf[len++] = ' ';
		  lastsearch = len;
		}
	      last_c_was_crlf = 1;
	    }
	  else
	    {
	      last_c_was_crlf = 0;
	      if(0x00 != c)
		tmpbuf[len++] = c;
	    }
	  tmpbuf[len] = '\0';
	  if(len > sizeof(tmpbuf)-5)
	    break;
	}
    }
  else
    {
      msyslog(LOG_ERR, "NeoClock4X(%d): can't query firmware version", unit);
      strcpy(tmpbuf, "unknown error");
    }
  strncpy(firmware, tmpbuf, maxlen);
  firmware[maxlen] = '\0';

  if(flag)
    {
      NLOG(NLOG_CLOCKINFO)
	msyslog(LOG_INFO, "NeoClock4X(%d): firmware version: %s", unit, firmware);
    }

  return (flag);
}
  
#else
int refclock_neoclock4x_bs;
#endif /* REFCLOCK */

/*
 * History:
 * refclock_neoclock4x.c
 *
 * 2002/04/27 cjh
 * Revision 1.0  first release
 *
 * 2002/0715 cjh
 * preparing for bitkeeper reposity
 *
 */
