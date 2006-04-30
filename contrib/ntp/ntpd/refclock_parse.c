/*
 * /src/NTP/ntp-4/ntpd/refclock_parse.c,v 4.36 1999/11/28 17:18:20 kardel RELEASE_19991128_A
 *
 * refclock_parse.c,v 4.36 1999/11/28 17:18:20 kardel RELEASE_19991128_A
 *
 * generic reference clock driver for receivers
 *
 * optionally make use of a STREAMS module for input processing where
 * available and configured. Currently the STREAMS module
 * is only available for Suns running SunOS 4.x and SunOS5.x
 *
 * the STREAMS module is not required for operation and may be omitted
 * at the cost of reduced accuracy. As new kernel interfaces emerger this
 * restriction may be lifted in future.
 *
 * Copyright (c) 1995-1999 by Frank Kardel <kardel@acm.org>
 * Copyright (c) 1989-1994 by Frank Kardel, Friedrich-Alexander Universität Erlangen-Nürnberg, Germany
 *
 * This software may not be sold for profit without a written consent
 * from the author.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_PARSE)

/*
 * This driver currently provides the support for
 *   - Meinberg receiver DCF77 PZF 535 (TCXO version)       (DCF)
 *   - Meinberg receiver DCF77 PZF 535 (OCXO version)       (DCF)
 *   - Meinberg receiver DCF77 PZF 509                      (DCF)
 *   - Meinberg receiver DCF77 AM receivers (e.g. C51)      (DCF)
 *   - IGEL CLOCK                                           (DCF)
 *   - ELV DCF7000                                          (DCF)
 *   - Schmid clock                                         (DCF)
 *   - Conrad DCF77 receiver module                         (DCF)
 *   - FAU DCF77 NTP receiver (TimeBrick)                   (DCF)
 *
 *   - Meinberg GPS166/GPS167                               (GPS)
 *   - Trimble (TSIP and TAIP protocol)                     (GPS)
 *
 *   - RCC8000 MSF Receiver                                 (MSF)
 *   - WHARTON 400A Series clock			    (DCF)
 *   - VARITEXT clock					    (MSF)
 */

/*
 * Meinberg receivers are usually connected via a
 * 9600 baud serial line
 *
 * The Meinberg GPS receivers also have a special NTP time stamp
 * format. The firmware release is Uni-Erlangen.
 *
 * Meinberg generic receiver setup:
 *	output time code every second
 *	Baud rate 9600 7E2S
 *
 * Meinberg GPS16x setup:
 *      output time code every second
 *      Baudrate 19200 8N1
 *
 * This software supports the standard data formats used
 * in Meinberg receivers.
 *
 * Special software versions are only sensible for the
 * GPS 16x family of receivers.
 *
 * Meinberg can be reached via: http://www.meinberg.de/
 */

#include "ntpd.h"
#include "ntp_refclock.h"
#include "ntp_unixtime.h"	/* includes <sys/time.h> */
#include "ntp_control.h"

#include <stdio.h>
#include <ctype.h>
#ifndef TM_IN_SYS_TIME
# include <time.h>
#endif

#if !defined(STREAM) && !defined(HAVE_SYSV_TTYS) && !defined(HAVE_BSD_TTYS) && !defined(HAVE_TERMIOS)
# include "Bletch:  Define one of {STREAM,HAVE_SYSV_TTYS,HAVE_TERMIOS}"
#endif

#ifdef STREAM
# include <sys/stream.h>
# include <sys/stropts.h>
#endif

#ifdef HAVE_TERMIOS
# define TTY_GETATTR(_FD_, _ARG_) tcgetattr((_FD_), (_ARG_))
# define TTY_SETATTR(_FD_, _ARG_) tcsetattr((_FD_), TCSANOW, (_ARG_))
# undef HAVE_SYSV_TTYS
#endif

#ifdef HAVE_SYSV_TTYS
# define TTY_GETATTR(_FD_, _ARG_) ioctl((_FD_), TCGETA, (_ARG_))
# define TTY_SETATTR(_FD_, _ARG_) ioctl((_FD_), TCSETAW, (_ARG_))
#endif

#ifdef HAVE_BSD_TTYS
/* #error CURRENTLY NO BSD TTY SUPPORT */
# include "Bletch: BSD TTY not currently supported"
#endif

#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif

#ifdef PPS
#ifdef HAVE_SYS_PPSCLOCK_H
#include <sys/ppsclock.h>
#endif
#ifdef HAVE_TIO_SERIAL_STUFF
#include <linux/serial.h>
#endif
#endif

#include "ntp_io.h"
#include "ntp_stdlib.h"

#include "parse.h"
#include "mbg_gps166.h"
#include "trimble.h"
#include "binio.h"
#include "ascii.h"
#include "ieee754io.h"

static char rcsid[]="refclock_parse.c,v 4.36 1999/11/28 17:18:20 kardel RELEASE_19991128_A";

/**===========================================================================
 ** external interface to ntp mechanism
 **/

static	void	parse_init	P((void));
static	int	parse_start	P((int, struct peer *));
static	void	parse_shutdown	P((int, struct peer *));
static	void	parse_poll	P((int, struct peer *));
static	void	parse_control	P((int, struct refclockstat *, struct refclockstat *, struct peer *));

#define	parse_buginfo	noentry

struct	refclock refclock_parse = {
	parse_start,
	parse_shutdown,
	parse_poll,
	parse_control,
	parse_init,
	parse_buginfo,
	NOFLAGS
};

/*
 * Definitions
 */
#define	MAXUNITS	4	/* maximum number of "PARSE" units permitted */
#define PARSEDEVICE	"/dev/refclock-%d" /* device to open %d is unit number */

#undef ABS
#define ABS(_X_) (((_X_) < 0) ? -(_X_) : (_X_))

/**===========================================================================
 ** function vector for dynamically binding io handling mechanism
 **/

struct parseunit;		/* to keep inquiring minds happy */

typedef struct bind
{
  const char *bd_description;	                                /* name of type of binding */
  int	(*bd_init)     P((struct parseunit *));			/* initialize */
  void	(*bd_end)      P((struct parseunit *));			/* end */
  int   (*bd_setcs)    P((struct parseunit *, parsectl_t *));	/* set character size */
  int	(*bd_disable)  P((struct parseunit *));			/* disable */
  int	(*bd_enable)   P((struct parseunit *));			/* enable */
  int	(*bd_getfmt)   P((struct parseunit *, parsectl_t *));	/* get format */
  int	(*bd_setfmt)   P((struct parseunit *, parsectl_t *));	/* setfmt */
  int	(*bd_timecode) P((struct parseunit *, parsectl_t *));	/* get time code */
  void	(*bd_receive)  P((struct recvbuf *));			/* receive operation */
  int	(*bd_io_input) P((struct recvbuf *));			/* input operation */
} bind_t;

#define PARSE_END(_X_)			(*(_X_)->binding->bd_end)(_X_)
#define PARSE_SETCS(_X_, _CS_)		(*(_X_)->binding->bd_setcs)(_X_, _CS_)
#define PARSE_ENABLE(_X_)		(*(_X_)->binding->bd_enable)(_X_)
#define PARSE_DISABLE(_X_)		(*(_X_)->binding->bd_disable)(_X_)
#define PARSE_GETFMT(_X_, _DCT_)	(*(_X_)->binding->bd_getfmt)(_X_, _DCT_)
#define PARSE_SETFMT(_X_, _DCT_)	(*(_X_)->binding->bd_setfmt)(_X_, _DCT_)
#define PARSE_GETTIMECODE(_X_, _DCT_)	(*(_X_)->binding->bd_timecode)(_X_, _DCT_)

/*
 * io modes
 */
#define PARSE_F_PPSPPS		0x0001 /* use loopfilter PPS code (CIOGETEV) */
#define PARSE_F_PPSONSECOND	0x0002 /* PPS pulses are on second */


/**===========================================================================
 ** error message regression handling
 **
 ** there are quite a few errors that can occur in rapid succession such as
 ** noisy input data or no data at all. in order to reduce the amount of
 ** syslog messages in such case, we are using a backoff algorithm. We limit
 ** the number of error messages of a certain class to 1 per time unit. if a
 ** configurable number of messages is displayed that way, we move on to the
 ** next time unit / count for that class. a count of messages that have been
 ** suppressed is held and displayed whenever a corresponding message is
 ** displayed. the time units for a message class will also be displayed.
 ** whenever an error condition clears we reset the error message state,
 ** thus we would still generate much output on pathological conditions
 ** where the system oscillates between OK and NOT OK states. coping
 ** with that condition is currently considered too complicated.
 **/

#define ERR_ALL	        (unsigned)~0	/* "all" errors */
#define ERR_BADDATA	(unsigned)0	/* unusable input data/conversion errors */
#define ERR_NODATA	(unsigned)1	/* no input data */
#define ERR_BADIO	(unsigned)2	/* read/write/select errors */
#define ERR_BADSTATUS	(unsigned)3	/* unsync states */
#define ERR_BADEVENT	(unsigned)4	/* non nominal events */
#define ERR_INTERNAL	(unsigned)5	/* internal error */
#define ERR_CNT		(unsigned)(ERR_INTERNAL+1)

#define ERR(_X_)	if (list_err(parse, (_X_)))

struct errorregression
{
	u_long err_count;	/* number of repititions per class */
	u_long err_delay;	/* minimum delay between messages */
};

static struct errorregression
err_baddata[] =			/* error messages for bad input data */
{
	{ 1,       0 },		/* output first message immediately */
	{ 5,      60 },		/* output next five messages in 60 second intervals */
	{ 3,    3600 },		/* output next 3 messages in hour intervals */
	{ 0, 12*3600 }		/* repeat messages only every 12 hours */
};

static struct errorregression
err_nodata[] =			/* error messages for missing input data */
{
	{ 1,       0 },		/* output first message immediately */
	{ 5,      60 },		/* output next five messages in 60 second intervals */
	{ 3,    3600 },		/* output next 3 messages in hour intervals */
	{ 0, 12*3600 }		/* repeat messages only every 12 hours */
};

static struct errorregression
err_badstatus[] =		/* unsynchronized state messages */
{
	{ 1,       0 },		/* output first message immediately */
	{ 5,      60 },		/* output next five messages in 60 second intervals */
	{ 3,    3600 },		/* output next 3 messages in hour intervals */
	{ 0, 12*3600 }		/* repeat messages only every 12 hours */
};

static struct errorregression
err_badio[] =			/* io failures (bad reads, selects, ...) */
{
	{ 1,       0 },		/* output first message immediately */
	{ 5,      60 },		/* output next five messages in 60 second intervals */
	{ 5,    3600 },		/* output next 3 messages in hour intervals */
	{ 0, 12*3600 }		/* repeat messages only every 12 hours */
};

static struct errorregression
err_badevent[] =		/* non nominal events */
{
	{ 20,      0 },		/* output first message immediately */
	{ 6,      60 },		/* output next five messages in 60 second intervals */
	{ 5,    3600 },		/* output next 3 messages in hour intervals */
	{ 0, 12*3600 }		/* repeat messages only every 12 hours */
};

static struct errorregression
err_internal[] =		/* really bad things - basically coding/OS errors */
{
	{ 0,       0 },		/* output all messages immediately */
};

static struct errorregression *
err_tbl[] =
{
	err_baddata,
	err_nodata,
	err_badio,
	err_badstatus,
	err_badevent,
	err_internal
};

struct errorinfo
{
	u_long err_started;	/* begin time (ntp) of error condition */
	u_long err_last;	/* last time (ntp) error occurred */
	u_long err_cnt;	/* number of error repititions */
	u_long err_suppressed;	/* number of suppressed messages */
	struct errorregression *err_stage; /* current error stage */
};

/**===========================================================================
 ** refclock instance data
 **/

struct parseunit
{
	/*
	 * NTP management
	 */
	struct peer         *peer;		/* backlink to peer structure - refclock inactive if 0  */
	struct refclockproc *generic;		/* backlink to refclockproc structure */

	/*
	 * PARSE io
	 */
	bind_t	     *binding;	        /* io handling binding */
	
	/*
	 * parse state
	 */
	parse_t	      parseio;	        /* io handling structure (user level parsing) */

	/*
	 * type specific parameters
	 */
	struct parse_clockinfo   *parse_type;	        /* link to clock description */

	/*
	 * clock state handling/reporting
	 */
	u_char	      flags;	        /* flags (leap_control) */
	u_long	      lastchange;       /* time (ntp) when last state change accured */
	u_long	      statetime[CEVNT_MAX+1]; /* accumulated time of clock states */
	u_long        pollneeddata; 	/* current_time(!=0) for receive sample expected in PPS mode */
	u_short	      lastformat;       /* last format used */
	u_long        lastsync;		/* time (ntp) when clock was last seen fully synchronized */
	u_long        lastmissed;       /* time (ntp) when poll didn't get data (powerup heuristic) */
	u_long        ppsserial;        /* magic cookie for ppsclock serials (avoids stale ppsclock data) */
	parsetime_t   time;		/* last (parse module) data */
	void         *localdata;        /* optional local, receiver-specific data */
        unsigned long localstate;       /* private local state */
	struct errorinfo errors[ERR_CNT];  /* error state table for suppressing excessive error messages */
	struct ctl_var *kv;	        /* additional pseudo variables */
	u_long        laststatistic;    /* time when staticstics where output */
};


/**===========================================================================
 ** Clockinfo section all parameter for specific clock types
 ** includes NTP parameters, TTY parameters and IO handling parameters
 **/

static	void	poll_dpoll	P((struct parseunit *));
static	void	poll_poll	P((struct peer *));
static	int	poll_init	P((struct parseunit *));

typedef struct poll_info
{
	u_long      rate;		/* poll rate - once every "rate" seconds - 0 off */
	const char *string;		/* string to send for polling */
	u_long      count;		/* number of characters in string */
} poll_info_t;

#define NO_CL_FLAGS	0
#define NO_POLL		0
#define NO_INIT		0
#define NO_END		0
#define NO_EVENT	0
#define NO_DATA		0
#define NO_MESSAGE	0
#define NO_PPSDELAY     0

#define DCF_ID		"DCF"	/* generic DCF */
#define DCF_A_ID	"DCFa"	/* AM demodulation */
#define DCF_P_ID	"DCFp"	/* psuedo random phase shift */
#define GPS_ID		"GPS"	/* GPS receiver */

#define	NOCLOCK_ROOTDELAY	0.0
#define	NOCLOCK_BASEDELAY	0.0
#define	NOCLOCK_DESCRIPTION	0
#define NOCLOCK_MAXUNSYNC       0
#define NOCLOCK_CFLAG           0
#define NOCLOCK_IFLAG           0
#define NOCLOCK_OFLAG           0
#define NOCLOCK_LFLAG           0
#define NOCLOCK_ID		"TILT"
#define NOCLOCK_POLL		NO_POLL
#define NOCLOCK_INIT		NO_INIT
#define NOCLOCK_END		NO_END
#define NOCLOCK_DATA		NO_DATA
#define NOCLOCK_FORMAT		""
#define NOCLOCK_TYPE		CTL_SST_TS_UNSPEC
#define NOCLOCK_SAMPLES		0
#define NOCLOCK_KEEP		0 

#define DCF_TYPE		CTL_SST_TS_LF
#define GPS_TYPE		CTL_SST_TS_UHF

/*
 * receiver specific constants
 */
#define MBG_SPEED		(B9600)
#define MBG_CFLAG		(CS7|PARENB|CREAD|CLOCAL|HUPCL)
#define MBG_IFLAG		(IGNBRK|IGNPAR|ISTRIP)
#define MBG_OFLAG		0
#define MBG_LFLAG		0
#define MBG_FLAGS               PARSE_F_PPSONSECOND

/*
 * Meinberg DCF77 receivers
 */
#define	DCFUA31_ROOTDELAY	0.0  /* 0 */
#define	DCFUA31_BASEDELAY	0.010  /* 10.7421875ms: 10 ms (+/- 3 ms) */
#define	DCFUA31_DESCRIPTION	"Meinberg DCF77 C51 or compatible"
#define DCFUA31_MAXUNSYNC       60*30       /* only trust clock for 1/2 hour */
#define DCFUA31_SPEED		MBG_SPEED
#define DCFUA31_CFLAG           MBG_CFLAG
#define DCFUA31_IFLAG           MBG_IFLAG
#define DCFUA31_OFLAG           MBG_OFLAG
#define DCFUA31_LFLAG           MBG_LFLAG
#define DCFUA31_SAMPLES		5
#define DCFUA31_KEEP		3
#define DCFUA31_FORMAT		"Meinberg Standard"

/*
 * Meinberg DCF PZF535/TCXO (FM/PZF) receiver
 */
#define	DCFPZF535_ROOTDELAY	0.0
#define	DCFPZF535_BASEDELAY	0.001968  /* 1.968ms +- 104us (oscilloscope) - relative to start (end of STX) */
#define	DCFPZF535_DESCRIPTION	"Meinberg DCF PZF 535/509 / TCXO"
#define DCFPZF535_MAXUNSYNC     60*60*12           /* only trust clock for 12 hours
						    * @ 5e-8df/f we have accumulated
						    * at most 2.16 ms (thus we move to
						    * NTP synchronisation */
#define DCFPZF535_SPEED		MBG_SPEED
#define DCFPZF535_CFLAG         MBG_CFLAG
#define DCFPZF535_IFLAG         MBG_IFLAG
#define DCFPZF535_OFLAG         MBG_OFLAG
#define DCFPZF535_LFLAG         MBG_LFLAG
#define DCFPZF535_SAMPLES	       5
#define DCFPZF535_KEEP		       3
#define DCFPZF535_FORMAT	"Meinberg Standard"

/*
 * Meinberg DCF PZF535/OCXO receiver
 */
#define	DCFPZF535OCXO_ROOTDELAY	0.0
#define	DCFPZF535OCXO_BASEDELAY	0.001968 /* 1.968ms +- 104us (oscilloscope) - relative to start (end of STX) */
#define	DCFPZF535OCXO_DESCRIPTION "Meinberg DCF PZF 535/509 / OCXO"
#define DCFPZF535OCXO_MAXUNSYNC     60*60*96       /* only trust clock for 4 days
						    * @ 5e-9df/f we have accumulated
						    * at most an error of 1.73 ms
						    * (thus we move to NTP synchronisation) */
#define DCFPZF535OCXO_SPEED	    MBG_SPEED
#define DCFPZF535OCXO_CFLAG         MBG_CFLAG
#define DCFPZF535OCXO_IFLAG         MBG_IFLAG
#define DCFPZF535OCXO_OFLAG         MBG_OFLAG
#define DCFPZF535OCXO_LFLAG         MBG_LFLAG
#define DCFPZF535OCXO_SAMPLES		   5
#define DCFPZF535OCXO_KEEP	           3
#define DCFPZF535OCXO_FORMAT	    "Meinberg Standard"

/*
 * Meinberg GPS16X receiver
 */
static	void	gps16x_message	 P((struct parseunit *, parsetime_t *));
static  int     gps16x_poll_init P((struct parseunit *));

#define	GPS16X_ROOTDELAY	0.0         /* nothing here */
#define	GPS16X_BASEDELAY	0.001968         /* XXX to be fixed ! 1.968ms +- 104us (oscilloscope) - relative to start (end of STX) */
#define	GPS16X_DESCRIPTION      "Meinberg GPS16x receiver"
#define GPS16X_MAXUNSYNC        60*60*96       /* only trust clock for 4 days
						* @ 5e-9df/f we have accumulated
						* at most an error of 1.73 ms
						* (thus we move to NTP synchronisation) */
#define GPS16X_SPEED		B19200
#define GPS16X_CFLAG            (CS8|CREAD|CLOCAL|HUPCL)
#define GPS16X_IFLAG            (IGNBRK|IGNPAR)
#define GPS16X_OFLAG            MBG_OFLAG
#define GPS16X_LFLAG            MBG_LFLAG
#define GPS16X_POLLRATE	6
#define GPS16X_POLLCMD	""
#define GPS16X_CMDSIZE	0

static poll_info_t gps16x_pollinfo = { GPS16X_POLLRATE, GPS16X_POLLCMD, GPS16X_CMDSIZE };

#define GPS16X_INIT		gps16x_poll_init
#define GPS16X_POLL	        0
#define GPS16X_END		0
#define GPS16X_DATA		((void *)(&gps16x_pollinfo))
#define GPS16X_MESSAGE		gps16x_message
#define GPS16X_ID		GPS_ID
#define GPS16X_FORMAT		"Meinberg GPS Extended"
#define GPS16X_SAMPLES		5
#define GPS16X_KEEP		3

/*
 * ELV DCF7000 Wallclock-Receiver/Switching Clock (Kit)
 *
 * This is really not the hottest clock - but before you have nothing ...
 */
#define DCF7000_ROOTDELAY	0.0 /* 0 */
#define DCF7000_BASEDELAY	0.405 /* slow blow */
#define DCF7000_DESCRIPTION	"ELV DCF7000"
#define DCF7000_MAXUNSYNC	(60*5) /* sorry - but it just was not build as a clock */
#define DCF7000_SPEED		(B9600)
#define DCF7000_CFLAG           (CS8|CREAD|PARENB|PARODD|CLOCAL|HUPCL)
#define DCF7000_IFLAG		(IGNBRK)
#define DCF7000_OFLAG		0
#define DCF7000_LFLAG		0
#define DCF7000_SAMPLES		5
#define DCF7000_KEEP		3
#define DCF7000_FORMAT		"ELV DCF7000"

/*
 * Schmid DCF Receiver Kit
 *
 * When the WSDCF clock is operating optimally we want the primary clock
 * distance to come out at 300 ms.  Thus, peer.distance in the WSDCF peer
 * structure is set to 290 ms and we compute delays which are at least
 * 10 ms long.  The following are 290 ms and 10 ms expressed in u_fp format
 */
#define WS_POLLRATE	1	/* every second - watch interdependency with poll routine */
#define WS_POLLCMD	"\163"
#define WS_CMDSIZE	1

static poll_info_t wsdcf_pollinfo = { WS_POLLRATE, WS_POLLCMD, WS_CMDSIZE };

#define WSDCF_INIT		poll_init
#define WSDCF_POLL		poll_dpoll
#define WSDCF_END		0
#define WSDCF_DATA		((void *)(&wsdcf_pollinfo))
#define	WSDCF_ROOTDELAY		0.0	/* 0 */
#define	WSDCF_BASEDELAY	 	0.010	/*  ~  10ms */
#define WSDCF_DESCRIPTION	"WS/DCF Receiver"
#define WSDCF_FORMAT		"Schmid"
#define WSDCF_MAXUNSYNC		(60*60)	/* assume this beast hold at 1 h better than 2 ms XXX-must verify */
#define WSDCF_SPEED		(B1200)
#define WSDCF_CFLAG		(CS8|CREAD|CLOCAL)
#define WSDCF_IFLAG		0
#define WSDCF_OFLAG		0
#define WSDCF_LFLAG		0
#define WSDCF_SAMPLES		5
#define WSDCF_KEEP		3

/*
 * RAW DCF77 - input of DCF marks via RS232 - many variants
 */
#define RAWDCF_FLAGS		0
#define RAWDCF_ROOTDELAY	0.0 /* 0 */
#define RAWDCF_BASEDELAY	0.258
#define RAWDCF_FORMAT		"RAW DCF77 Timecode"
#define RAWDCF_MAXUNSYNC	(0) /* sorry - its a true receiver - no signal - no time */
#define RAWDCF_SPEED		(B50)
#ifdef NO_PARENB_IGNPAR /* Was: defined(SYS_IRIX4) || defined(SYS_IRIX5) */
/* somehow doesn't grok PARENB & IGNPAR (mj) */
# define RAWDCF_CFLAG            (CS8|CREAD|CLOCAL)
#else
# define RAWDCF_CFLAG            (CS8|CREAD|CLOCAL|PARENB)
#endif
#ifdef RAWDCF_NO_IGNPAR /* Was: defined(SYS_LINUX) && defined(CLOCK_RAWDCF) */
# define RAWDCF_IFLAG		0
#else
# define RAWDCF_IFLAG		(IGNPAR)
#endif
#define RAWDCF_OFLAG		0
#define RAWDCF_LFLAG		0
#define RAWDCF_SAMPLES		20
#define RAWDCF_KEEP		12
#define RAWDCF_INIT		0

/*
 * RAW DCF variants
 */
/*
 * Conrad receiver
 *
 * simplest (cheapest) DCF clock - e. g. DCF77 receiver by Conrad
 * (~40DM - roughly $30 ) followed by a level converter for RS232
 */
#define CONRAD_BASEDELAY	0.292 /* Conrad receiver @ 50 Baud on a Sun */
#define CONRAD_DESCRIPTION	"RAW DCF77 CODE (Conrad DCF77 receiver module)"

/*
 * TimeBrick receiver
 */
#define TIMEBRICK_BASEDELAY	0.210 /* TimeBrick @ 50 Baud on a Sun */
#define TIMEBRICK_DESCRIPTION	"RAW DCF77 CODE (TimeBrick)"

/*
 * IGEL:clock receiver
 */
#define IGELCLOCK_BASEDELAY	0.258 /* IGEL:clock receiver */
#define IGELCLOCK_DESCRIPTION	"RAW DCF77 CODE (IGEL:clock)"
#define IGELCLOCK_SPEED		(B1200)
#define IGELCLOCK_CFLAG		(CS8|CREAD|HUPCL|CLOCAL)

/*
 * RAWDCF receivers that need to be powered from DTR
 * (like Expert mouse clock)
 */
static	int	rawdcf_init_1	P((struct parseunit *));
#define RAWDCFDTRSET_DESCRIPTION	"RAW DCF77 CODE (DTR SET/RTS CLR)"
#define RAWDCFDTRSET_INIT 		rawdcf_init_1

/*
 * RAWDCF receivers that need to be powered from
 * DTR CLR and RTS SET
 */
static	int	rawdcf_init_2	P((struct parseunit *));
#define RAWDCFDTRCLRRTSSET_DESCRIPTION	"RAW DCF77 CODE (DTR CLR/RTS SET)"
#define RAWDCFDTRCLRRTSSET_INIT	rawdcf_init_2

/*
 * Trimble GPS receivers (TAIP and TSIP protocols)
 */
#ifndef TRIM_POLLRATE
#define TRIM_POLLRATE	0	/* only true direct polling */
#endif

#define TRIM_TAIPPOLLCMD	">SRM;FR_FLAG=F;EC_FLAG=F<>QTM<"
#define TRIM_TAIPCMDSIZE	(sizeof(TRIM_TAIPPOLLCMD)-1)

static poll_info_t trimbletaip_pollinfo = { TRIM_POLLRATE, TRIM_TAIPPOLLCMD, TRIM_TAIPCMDSIZE };
static	int	trimbletaip_init	P((struct parseunit *));
static	void	trimbletaip_event	P((struct parseunit *, int));

/* query time & UTC correction data */
static char tsipquery[] = { DLE, 0x21, DLE, ETX, DLE, 0x2F, DLE, ETX };

static poll_info_t trimbletsip_pollinfo = { TRIM_POLLRATE, tsipquery, sizeof(tsipquery) };
static	int	trimbletsip_init	P((struct parseunit *));
static	void	trimbletsip_end   	P((struct parseunit *));
static	void	trimbletsip_message	P((struct parseunit *, parsetime_t *));
static	void	trimbletsip_event	P((struct parseunit *, int));

#define TRIMBLETSIP_IDLE_TIME	    (300) /* 5 minutes silence at most */

#define TRIMBLETAIP_SPEED	    (B4800)
#define TRIMBLETAIP_CFLAG           (CS8|CREAD|CLOCAL)
#define TRIMBLETAIP_IFLAG           (BRKINT|IGNPAR|ISTRIP|ICRNL|IXON)
#define TRIMBLETAIP_OFLAG           (OPOST|ONLCR)
#define TRIMBLETAIP_LFLAG           (0)

#define TRIMBLETSIP_SPEED	    (B9600)
#define TRIMBLETSIP_CFLAG           (CS8|CLOCAL|CREAD|PARENB|PARODD)
#define TRIMBLETSIP_IFLAG           (IGNBRK)
#define TRIMBLETSIP_OFLAG           (0)
#define TRIMBLETSIP_LFLAG           (ICANON)

#define TRIMBLETSIP_SAMPLES	    5
#define TRIMBLETSIP_KEEP	    3
#define TRIMBLETAIP_SAMPLES	    5
#define TRIMBLETAIP_KEEP	    3

#define TRIMBLETAIP_FLAGS	    (PARSE_F_PPSONSECOND)
#define TRIMBLETSIP_FLAGS	    (TRIMBLETAIP_FLAGS)

#define TRIMBLETAIP_POLL	    poll_dpoll
#define TRIMBLETSIP_POLL	    poll_dpoll

#define TRIMBLETAIP_INIT	    trimbletaip_init
#define TRIMBLETSIP_INIT	    trimbletsip_init

#define TRIMBLETAIP_EVENT	    trimbletaip_event   

#define TRIMBLETSIP_EVENT	    trimbletsip_event   
#define TRIMBLETSIP_MESSAGE	    trimbletsip_message

#define TRIMBLETAIP_END		    0
#define TRIMBLETSIP_END		    trimbletsip_end

#define TRIMBLETAIP_DATA	    ((void *)(&trimbletaip_pollinfo))
#define TRIMBLETSIP_DATA	    ((void *)(&trimbletsip_pollinfo))

#define TRIMBLETAIP_ID		    GPS_ID
#define TRIMBLETSIP_ID		    GPS_ID

#define TRIMBLETAIP_FORMAT	    "Trimble TAIP"
#define TRIMBLETSIP_FORMAT	    "Trimble TSIP"

#define TRIMBLETAIP_ROOTDELAY        0x0
#define TRIMBLETSIP_ROOTDELAY        0x0

#define TRIMBLETAIP_BASEDELAY        0.0
#define TRIMBLETSIP_BASEDELAY        0.020	/* GPS time message latency */

#define TRIMBLETAIP_DESCRIPTION      "Trimble GPS (TAIP) receiver"
#define TRIMBLETSIP_DESCRIPTION      "Trimble GPS (TSIP) receiver"

#define TRIMBLETAIP_MAXUNSYNC        0
#define TRIMBLETSIP_MAXUNSYNC        0

#define TRIMBLETAIP_EOL		    '<'

/*
 * RadioCode Clocks RCC 800 receiver
 */
#define RCC_POLLRATE   0       /* only true direct polling */
#define RCC_POLLCMD    "\r"
#define RCC_CMDSIZE    1

static poll_info_t rcc8000_pollinfo = { RCC_POLLRATE, RCC_POLLCMD, RCC_CMDSIZE };
#define RCC8000_FLAGS		0
#define RCC8000_POLL            poll_dpoll
#define RCC8000_INIT            poll_init
#define RCC8000_END             0
#define RCC8000_DATA            ((void *)(&rcc8000_pollinfo))
#define RCC8000_ROOTDELAY       0.0
#define RCC8000_BASEDELAY       0.0
#define RCC8000_ID              "MSF"
#define RCC8000_DESCRIPTION     "RCC 8000 MSF Receiver"
#define RCC8000_FORMAT          "Radiocode RCC8000"
#define RCC8000_MAXUNSYNC       (60*60) /* should be ok for an hour */
#define RCC8000_SPEED		(B2400)
#define RCC8000_CFLAG           (CS8|CREAD|CLOCAL)
#define RCC8000_IFLAG           (IGNBRK|IGNPAR)
#define RCC8000_OFLAG           0
#define RCC8000_LFLAG           0
#define RCC8000_SAMPLES         5
#define RCC8000_KEEP	        3

/*
 * Hopf Radio clock 6021 Format 
 *
 */
#define HOPF6021_ROOTDELAY	0.0
#define HOPF6021_BASEDELAY	0.0
#define HOPF6021_DESCRIPTION	"HOPF 6021"
#define HOPF6021_FORMAT         "hopf Funkuhr 6021"
#define HOPF6021_MAXUNSYNC	(60*60)  /* should be ok for an hour */
#define HOPF6021_SPEED         (B9600)
#define HOPF6021_CFLAG          (CS8|CREAD|CLOCAL)
#define HOPF6021_IFLAG		(IGNBRK|ISTRIP)
#define HOPF6021_OFLAG		0
#define HOPF6021_LFLAG		0
#define HOPF6021_FLAGS          0
#define HOPF6021_SAMPLES        5
#define HOPF6021_KEEP	        3

/*
 * Diem's Computime Radio Clock Receiver
 */
#define COMPUTIME_FLAGS       0
#define COMPUTIME_ROOTDELAY   0.0
#define COMPUTIME_BASEDELAY   0.0
#define COMPUTIME_ID          DCF_ID
#define COMPUTIME_DESCRIPTION "Diem's Computime receiver"
#define COMPUTIME_FORMAT      "Diem's Computime Radio Clock"
#define COMPUTIME_TYPE        DCF_TYPE
#define COMPUTIME_MAXUNSYNC   (60*60)       /* only trust clock for 1 hour */
#define COMPUTIME_SPEED       (B9600)
#define COMPUTIME_CFLAG       (CSTOPB|CS7|CREAD|CLOCAL)
#define COMPUTIME_IFLAG       (IGNBRK|IGNPAR|ISTRIP)
#define COMPUTIME_OFLAG       0
#define COMPUTIME_LFLAG       0
#define COMPUTIME_SAMPLES     5
#define COMPUTIME_KEEP        3

/*
 * Varitext Radio Clock Receiver
 */
#define VARITEXT_FLAGS       0
#define VARITEXT_ROOTDELAY   0.0
#define VARITEXT_BASEDELAY   0.0
#define VARITEXT_ID          "MSF"
#define VARITEXT_DESCRIPTION "Varitext receiver"
#define VARITEXT_FORMAT      "Varitext Radio Clock"
#define VARITEXT_TYPE        DCF_TYPE
#define VARITEXT_MAXUNSYNC   (60*60)       /* only trust clock for 1 hour */
#define VARITEXT_SPEED       (B9600)
#define VARITEXT_CFLAG       (CS7|CREAD|CLOCAL|PARENB|PARODD)
#define VARITEXT_IFLAG       (IGNPAR|IGNBRK|INPCK) /*|ISTRIP)*/
#define VARITEXT_OFLAG       0
#define VARITEXT_LFLAG       0
#define VARITEXT_SAMPLES     32
#define VARITEXT_KEEP        20

static struct parse_clockinfo
{
	u_long  cl_flags;		/* operation flags (io modes) */
  void  (*cl_poll)    P((struct parseunit *));			/* active poll routine */
  int   (*cl_init)    P((struct parseunit *));			/* active poll init routine */
  void  (*cl_event)   P((struct parseunit *, int));		/* special event handling (e.g. reset clock) */
  void  (*cl_end)     P((struct parseunit *));			/* active poll end routine */
  void  (*cl_message) P((struct parseunit *, parsetime_t *));	/* process a lower layer message */
	void   *cl_data;		/* local data area for "poll" mechanism */
	double    cl_rootdelay;		/* rootdelay */
	double    cl_basedelay;		/* current offset by which the RS232
				time code is delayed from the actual time */
	const char *cl_id;		/* ID code */
	const char *cl_description;		/* device name */
	const char *cl_format;		/* fixed format */
	u_char  cl_type;		/* clock type (ntp control) */
	u_long  cl_maxunsync;		/* time to trust oscillator after losing synch */
	u_long  cl_speed;		/* terminal input & output baudrate */
	u_long  cl_cflag;             /* terminal control flags */
	u_long  cl_iflag;             /* terminal input flags */
	u_long  cl_oflag;             /* terminal output flags */
	u_long  cl_lflag;             /* terminal local flags */
	u_long  cl_samples;	      /* samples for median filter */
	u_long  cl_keep;              /* samples for median filter to keep */
} parse_clockinfo[] =
{
	{				/* mode 0 */
		MBG_FLAGS,
		NO_POLL,
		NO_INIT,
		NO_EVENT,
		NO_END,
		NO_MESSAGE,
		NO_DATA,
		DCFPZF535_ROOTDELAY,
		DCFPZF535_BASEDELAY,
		DCF_P_ID,
		DCFPZF535_DESCRIPTION,
		DCFPZF535_FORMAT,
		DCF_TYPE,
		DCFPZF535_MAXUNSYNC,
		DCFPZF535_SPEED,
		DCFPZF535_CFLAG,
		DCFPZF535_IFLAG,
		DCFPZF535_OFLAG,
		DCFPZF535_LFLAG,
		DCFPZF535_SAMPLES,
		DCFPZF535_KEEP
	},
	{				/* mode 1 */
		MBG_FLAGS,
		NO_POLL,
		NO_INIT,
		NO_EVENT,
		NO_END,
		NO_MESSAGE,
		NO_DATA,
		DCFPZF535OCXO_ROOTDELAY,
		DCFPZF535OCXO_BASEDELAY,
		DCF_P_ID,
		DCFPZF535OCXO_DESCRIPTION,
		DCFPZF535OCXO_FORMAT,
		DCF_TYPE,
		DCFPZF535OCXO_MAXUNSYNC,
		DCFPZF535OCXO_SPEED,
		DCFPZF535OCXO_CFLAG,
		DCFPZF535OCXO_IFLAG,
		DCFPZF535OCXO_OFLAG,
		DCFPZF535OCXO_LFLAG,
		DCFPZF535OCXO_SAMPLES,
		DCFPZF535OCXO_KEEP
	},
	{				/* mode 2 */
		MBG_FLAGS,
		NO_POLL,
		NO_INIT,
		NO_EVENT,
		NO_END,
		NO_MESSAGE,
		NO_DATA,
		DCFUA31_ROOTDELAY,
		DCFUA31_BASEDELAY,
		DCF_A_ID,
		DCFUA31_DESCRIPTION,
		DCFUA31_FORMAT,
		DCF_TYPE,
		DCFUA31_MAXUNSYNC,
		DCFUA31_SPEED,
		DCFUA31_CFLAG,
		DCFUA31_IFLAG,
		DCFUA31_OFLAG,
		DCFUA31_LFLAG,
		DCFUA31_SAMPLES,
		DCFUA31_KEEP
	},
	{				/* mode 3 */
		MBG_FLAGS,
		NO_POLL,
		NO_INIT,
		NO_EVENT,
		NO_END,
		NO_MESSAGE,
		NO_DATA,
		DCF7000_ROOTDELAY,
		DCF7000_BASEDELAY,
		DCF_A_ID,
		DCF7000_DESCRIPTION,
		DCF7000_FORMAT,
		DCF_TYPE,
		DCF7000_MAXUNSYNC,
		DCF7000_SPEED,
		DCF7000_CFLAG,
		DCF7000_IFLAG,
		DCF7000_OFLAG,
		DCF7000_LFLAG,
		DCF7000_SAMPLES,
		DCF7000_KEEP
	},
	{				/* mode 4 */
		NO_CL_FLAGS,
		WSDCF_POLL,
		WSDCF_INIT,
		NO_EVENT,
		WSDCF_END,
		NO_MESSAGE,
		WSDCF_DATA,
		WSDCF_ROOTDELAY,
		WSDCF_BASEDELAY,
		DCF_A_ID,
		WSDCF_DESCRIPTION,
		WSDCF_FORMAT,
		DCF_TYPE,
		WSDCF_MAXUNSYNC,
		WSDCF_SPEED,
		WSDCF_CFLAG,
		WSDCF_IFLAG,
		WSDCF_OFLAG,
		WSDCF_LFLAG,
		WSDCF_SAMPLES,
		WSDCF_KEEP
	},
	{				/* mode 5 */
		RAWDCF_FLAGS,
		NO_POLL,
		RAWDCF_INIT,
		NO_EVENT,
		NO_END,
		NO_MESSAGE,
		NO_DATA,
		RAWDCF_ROOTDELAY,
		CONRAD_BASEDELAY,
		DCF_A_ID,
		CONRAD_DESCRIPTION,
		RAWDCF_FORMAT,
		DCF_TYPE,
		RAWDCF_MAXUNSYNC,
		RAWDCF_SPEED,
		RAWDCF_CFLAG,
		RAWDCF_IFLAG,
		RAWDCF_OFLAG,
		RAWDCF_LFLAG,
		RAWDCF_SAMPLES,
		RAWDCF_KEEP
	},
	{				/* mode 6 */
		RAWDCF_FLAGS,
		NO_POLL,
		RAWDCF_INIT,
		NO_EVENT,
		NO_END,
		NO_MESSAGE,
		NO_DATA,
		RAWDCF_ROOTDELAY,
		TIMEBRICK_BASEDELAY,
		DCF_A_ID,
		TIMEBRICK_DESCRIPTION,
		RAWDCF_FORMAT,
		DCF_TYPE,
		RAWDCF_MAXUNSYNC,
		RAWDCF_SPEED,
		RAWDCF_CFLAG,
		RAWDCF_IFLAG,
		RAWDCF_OFLAG,
		RAWDCF_LFLAG,
		RAWDCF_SAMPLES,
		RAWDCF_KEEP
	},
	{				/* mode 7 */
		MBG_FLAGS,
		GPS16X_POLL,
		GPS16X_INIT,
		NO_EVENT,
		GPS16X_END,
		GPS16X_MESSAGE,
		GPS16X_DATA,
		GPS16X_ROOTDELAY,
		GPS16X_BASEDELAY,
		GPS16X_ID,
		GPS16X_DESCRIPTION,
		GPS16X_FORMAT,
		GPS_TYPE,
		GPS16X_MAXUNSYNC,
		GPS16X_SPEED,
		GPS16X_CFLAG,
		GPS16X_IFLAG,
		GPS16X_OFLAG,
		GPS16X_LFLAG,
		GPS16X_SAMPLES,
		GPS16X_KEEP
	},
	{				/* mode 8 */
		RAWDCF_FLAGS,
		NO_POLL,
		NO_INIT,
		NO_EVENT,
		NO_END,
		NO_MESSAGE,
		NO_DATA,
		RAWDCF_ROOTDELAY,
		IGELCLOCK_BASEDELAY,
		DCF_A_ID,
		IGELCLOCK_DESCRIPTION,
		RAWDCF_FORMAT,
		DCF_TYPE,
		RAWDCF_MAXUNSYNC,
		IGELCLOCK_SPEED,
		IGELCLOCK_CFLAG,
		RAWDCF_IFLAG,
		RAWDCF_OFLAG,
		RAWDCF_LFLAG,
		RAWDCF_SAMPLES,
		RAWDCF_KEEP
	},
	{				/* mode 9 */
		TRIMBLETAIP_FLAGS,
#if TRIM_POLLRATE		/* DHD940515: Allow user config */
		NO_POLL,
#else
		TRIMBLETAIP_POLL,
#endif
		TRIMBLETAIP_INIT,
		TRIMBLETAIP_EVENT,
		TRIMBLETAIP_END,
		NO_MESSAGE,
		TRIMBLETAIP_DATA,
		TRIMBLETAIP_ROOTDELAY,
		TRIMBLETAIP_BASEDELAY,
		TRIMBLETAIP_ID,
		TRIMBLETAIP_DESCRIPTION,
		TRIMBLETAIP_FORMAT,
		GPS_TYPE,
		TRIMBLETAIP_MAXUNSYNC,
		TRIMBLETAIP_SPEED,
		TRIMBLETAIP_CFLAG,
		TRIMBLETAIP_IFLAG,
		TRIMBLETAIP_OFLAG,
		TRIMBLETAIP_LFLAG,
		TRIMBLETAIP_SAMPLES,
		TRIMBLETAIP_KEEP
	},
	{				/* mode 10 */
		TRIMBLETSIP_FLAGS,
#if TRIM_POLLRATE		/* DHD940515: Allow user config */
		NO_POLL,
#else
		TRIMBLETSIP_POLL,
#endif
		TRIMBLETSIP_INIT,
		TRIMBLETSIP_EVENT,
		TRIMBLETSIP_END,
		TRIMBLETSIP_MESSAGE,
		TRIMBLETSIP_DATA,
		TRIMBLETSIP_ROOTDELAY,
		TRIMBLETSIP_BASEDELAY,
		TRIMBLETSIP_ID,
		TRIMBLETSIP_DESCRIPTION,
		TRIMBLETSIP_FORMAT,
		GPS_TYPE,
		TRIMBLETSIP_MAXUNSYNC,
		TRIMBLETSIP_SPEED,
		TRIMBLETSIP_CFLAG,
		TRIMBLETSIP_IFLAG,
		TRIMBLETSIP_OFLAG,
		TRIMBLETSIP_LFLAG,
		TRIMBLETSIP_SAMPLES,
		TRIMBLETSIP_KEEP
	},
	{                             /* mode 11 */
		NO_CL_FLAGS,
		RCC8000_POLL,
		RCC8000_INIT,
		NO_EVENT,
		RCC8000_END,
		NO_MESSAGE,
		RCC8000_DATA,
		RCC8000_ROOTDELAY,
		RCC8000_BASEDELAY,
		RCC8000_ID,
		RCC8000_DESCRIPTION,
		RCC8000_FORMAT,
		DCF_TYPE,
		RCC8000_MAXUNSYNC,
		RCC8000_SPEED,
		RCC8000_CFLAG,
		RCC8000_IFLAG,
		RCC8000_OFLAG,
		RCC8000_LFLAG,
		RCC8000_SAMPLES,
		RCC8000_KEEP
	},
	{                             /* mode 12 */
		HOPF6021_FLAGS,
		NO_POLL,     
		NO_INIT,
		NO_EVENT,
		NO_END,
		NO_MESSAGE,
		NO_DATA,
		HOPF6021_ROOTDELAY,
		HOPF6021_BASEDELAY,
		DCF_ID,
		HOPF6021_DESCRIPTION,
		HOPF6021_FORMAT,
		DCF_TYPE,
		HOPF6021_MAXUNSYNC,
		HOPF6021_SPEED,
		HOPF6021_CFLAG,
		HOPF6021_IFLAG,
		HOPF6021_OFLAG,
		HOPF6021_LFLAG,
		HOPF6021_SAMPLES,
		HOPF6021_KEEP
	},
	{                            /* mode 13 */
		COMPUTIME_FLAGS,
		NO_POLL,
		NO_INIT,
		NO_EVENT,
		NO_END,
		NO_MESSAGE,
		NO_DATA,
		COMPUTIME_ROOTDELAY,
		COMPUTIME_BASEDELAY,
		COMPUTIME_ID,
		COMPUTIME_DESCRIPTION,
		COMPUTIME_FORMAT,
		COMPUTIME_TYPE,
		COMPUTIME_MAXUNSYNC,
		COMPUTIME_SPEED,
		COMPUTIME_CFLAG,
		COMPUTIME_IFLAG,
		COMPUTIME_OFLAG,
		COMPUTIME_LFLAG,
		COMPUTIME_SAMPLES,
		COMPUTIME_KEEP
	},
	{				/* mode 14 */
		RAWDCF_FLAGS,
		NO_POLL,
		RAWDCFDTRSET_INIT,
		NO_EVENT,
		NO_END,
		NO_MESSAGE,
		NO_DATA,
		RAWDCF_ROOTDELAY,
		RAWDCF_BASEDELAY,
		DCF_A_ID,
		RAWDCFDTRSET_DESCRIPTION,
		RAWDCF_FORMAT,
		DCF_TYPE,
		RAWDCF_MAXUNSYNC,
		RAWDCF_SPEED,
		RAWDCF_CFLAG,
		RAWDCF_IFLAG,
		RAWDCF_OFLAG,
		RAWDCF_LFLAG,
		RAWDCF_SAMPLES,
		RAWDCF_KEEP
	},
	{				/* mode 15 */
		0,				/* operation flags (io modes) */
  		NO_POLL,			/* active poll routine */
		NO_INIT,			/* active poll init routine */
  		NO_EVENT,		        /* special event handling (e.g. reset clock) */
  		NO_END,				/* active poll end routine */
  		NO_MESSAGE,			/* process a lower layer message */
		NO_DATA,			/* local data area for "poll" mechanism */
		0,				/* rootdelay */
		11.0 /* bits */ / 9600,		/* current offset by which the RS232
				           	time code is delayed from the actual time */
		DCF_ID,				/* ID code */
		"WHARTON 400A Series clock",	/* device name */
		"WHARTON 400A Series clock Output Format 1",	/* fixed format */
			/* Must match a format-name in a libparse/clk_xxx.c file */
		DCF_TYPE,			/* clock type (ntp control) */
		(1*60*60),		        /* time to trust oscillator after losing synch */
		B9600,				/* terminal input & output baudrate */
		(CS8|CREAD|PARENB|CLOCAL|HUPCL),/* terminal control flags */
		0,				/* terminal input flags */
		0,				/* terminal output flags */
		0,				/* terminal local flags */
		5,				/* samples for median filter */
		3,				/* samples for median filter to keep */
	},
	{				/* mode 16 - RAWDCF RTS set, DTR clr */
		RAWDCF_FLAGS,
		NO_POLL,
		RAWDCFDTRCLRRTSSET_INIT,
		NO_EVENT,
		NO_END,
		NO_MESSAGE,
		NO_DATA,
		RAWDCF_ROOTDELAY,
		RAWDCF_BASEDELAY,
		DCF_A_ID,
		RAWDCFDTRCLRRTSSET_DESCRIPTION,
		RAWDCF_FORMAT,
		DCF_TYPE,
		RAWDCF_MAXUNSYNC,
		RAWDCF_SPEED,
		RAWDCF_CFLAG,
		RAWDCF_IFLAG,
		RAWDCF_OFLAG,
		RAWDCF_LFLAG,
		RAWDCF_SAMPLES,
		RAWDCF_KEEP
	},
        {                            /* mode 17 */
                VARITEXT_FLAGS,
                NO_POLL,
                NO_INIT,
                NO_EVENT,
                NO_END,
                NO_MESSAGE,
                NO_DATA,
                VARITEXT_ROOTDELAY,
                VARITEXT_BASEDELAY,
                VARITEXT_ID,
                VARITEXT_DESCRIPTION,
                VARITEXT_FORMAT,
                VARITEXT_TYPE,
                VARITEXT_MAXUNSYNC,
                VARITEXT_SPEED,
                VARITEXT_CFLAG,
                VARITEXT_IFLAG,
                VARITEXT_OFLAG,
                VARITEXT_LFLAG,
                VARITEXT_SAMPLES,
                VARITEXT_KEEP
        }
};

static int ncltypes = sizeof(parse_clockinfo) / sizeof(struct parse_clockinfo);

#define CLK_REALTYPE(x) ((int)(((x)->ttl) & 0x7F))
#define CLK_TYPE(x)	((CLK_REALTYPE(x) >= ncltypes) ? ~0 : CLK_REALTYPE(x))
#define CLK_UNIT(x)	((int)REFCLOCKUNIT(&(x)->srcadr))
#define CLK_PPS(x)	(((x)->ttl) & 0x80)

/*
 * Other constant stuff
 */
#define	PARSEHSREFID	0x7f7f08ff	/* 127.127.8.255 refid for hi strata */

#define PARSESTATISTICS   (60*60)	        /* output state statistics every hour */

static struct parseunit *parseunits[MAXUNITS];

static int notice = 0;

#define PARSE_STATETIME(parse, i) ((parse->generic->currentstatus == i) ? parse->statetime[i] + current_time - parse->lastchange : parse->statetime[i])

static void parse_event   P((struct parseunit *, int));
static void parse_process P((struct parseunit *, parsetime_t *));
static void clear_err     P((struct parseunit *, u_long));
static int  list_err      P((struct parseunit *, u_long));
static char * l_mktime    P((u_long));

/**===========================================================================
 ** implementation error message regression module
 **/
static void
clear_err(
	struct parseunit *parse,
	u_long            lstate
	)
{
	if (lstate == ERR_ALL)
	{
		int i;

		for (i = 0; i < ERR_CNT; i++)
		{
			parse->errors[i].err_stage   = err_tbl[i];
			parse->errors[i].err_cnt     = 0;
			parse->errors[i].err_last    = 0;
			parse->errors[i].err_started = 0;
			parse->errors[i].err_suppressed = 0;
		}
	}
	else
	{
		parse->errors[lstate].err_stage   = err_tbl[lstate];
		parse->errors[lstate].err_cnt     = 0;
		parse->errors[lstate].err_last    = 0;
		parse->errors[lstate].err_started = 0;
		parse->errors[lstate].err_suppressed = 0;
	}
}

static int
list_err(
	struct parseunit *parse,
	u_long            lstate
	)
{
	int do_it;
	struct errorinfo *err = &parse->errors[lstate];

	if (err->err_started == 0)
	{
		err->err_started = current_time;
	}

	do_it = (current_time - err->err_last) >= err->err_stage->err_delay;

	if (do_it)
	    err->err_cnt++;
  
	if (err->err_stage->err_count &&
	    (err->err_cnt >= err->err_stage->err_count))
	{
		err->err_stage++;
		err->err_cnt = 0;
	}

	if (!err->err_cnt && do_it)
	    msyslog(LOG_INFO, "PARSE receiver #%d: interval for following error message class is at least %s",
		    CLK_UNIT(parse->peer), l_mktime(err->err_stage->err_delay));

	if (!do_it)
	    err->err_suppressed++;
	else
	    err->err_last = current_time;

	if (do_it && err->err_suppressed)
	{
		msyslog(LOG_INFO, "PARSE receiver #%d: %ld message%s suppressed, error condition class persists for %s",
			CLK_UNIT(parse->peer), err->err_suppressed, (err->err_suppressed == 1) ? " was" : "s where",
			l_mktime(current_time - err->err_started));
		err->err_suppressed = 0;
	}
  
	return do_it;
}

/*--------------------------------------------------
 * mkreadable - make a printable ascii string (without
 * embedded quotes so that the ntpq protocol isn't
 * fooled
 */
#ifndef isprint
#define isprint(_X_) (((_X_) > 0x1F) && ((_X_) < 0x7F))
#endif

static char *
mkreadable(
	char  *buffer,
	long  blen,
	const char  *src,
	u_long  srclen,
	int hex
	)
{
	char *b    = buffer;
	char *endb = (char *)0;

	if (blen < 4)
		return (char *)0;		/* don't bother with mini buffers */

	endb = buffer + blen - 4;

	blen--;			/* account for '\0' */

	while (blen && srclen--)
	{
		if (!hex &&             /* no binary only */
		    (*src != '\\') &&   /* no plain \ */
		    (*src != '"') &&    /* no " */
		    isprint((int)*src))	/* only printables */
		{			/* they are easy... */
			*buffer++ = *src++;
			blen--;
		}
		else
		{
			if (blen < 4)
			{
				while (blen--)
				{
					*buffer++ = '.';
				}
				*buffer = '\0';
				return b;
			}
			else
			{
				if (*src == '\\')
				{
					strcpy(buffer,"\\\\");
					buffer += 2;
					blen   -= 2;
					src++;
				}
				else
				{
					sprintf(buffer, "\\x%02x", *src++);
					blen   -= 4;
					buffer += 4;
				}
			}
		}
		if (srclen && !blen && endb) /* overflow - set last chars to ... */
			strcpy(endb, "...");
	}

	*buffer = '\0';
	return b;
}


/*--------------------------------------------------
 * mkascii - make a printable ascii string
 * assumes (unless defined better) 7-bit ASCII
 */
static char *
mkascii(
	char  *buffer,
	long  blen,
	const char  *src,
	u_long  srclen
	)
{
	return mkreadable(buffer, blen, src, srclen, 0);
}

/**===========================================================================
 ** implementation of i/o handling methods
 ** (all STREAM, partial STREAM, user level)
 **/

/*
 * define possible io handling methods
 */
#ifdef STREAM
static int  ppsclock_init   P((struct parseunit *));
static int  stream_init     P((struct parseunit *));
static void stream_end      P((struct parseunit *));
static int  stream_enable   P((struct parseunit *));
static int  stream_disable  P((struct parseunit *));
static int  stream_setcs    P((struct parseunit *, parsectl_t *));
static int  stream_getfmt   P((struct parseunit *, parsectl_t *));
static int  stream_setfmt   P((struct parseunit *, parsectl_t *));
static int  stream_timecode P((struct parseunit *, parsectl_t *));
static void stream_receive  P((struct recvbuf *));
#endif
					 
static int  local_init     P((struct parseunit *));
static void local_end      P((struct parseunit *));
static int  local_nop      P((struct parseunit *));
static int  local_setcs    P((struct parseunit *, parsectl_t *));
static int  local_getfmt   P((struct parseunit *, parsectl_t *));
static int  local_setfmt   P((struct parseunit *, parsectl_t *));
static int  local_timecode P((struct parseunit *, parsectl_t *));
static void local_receive  P((struct recvbuf *));
static int  local_input    P((struct recvbuf *));

static bind_t io_bindings[] =
{
#ifdef STREAM
	{
		"parse STREAM",
		stream_init,
		stream_end,
		stream_setcs,
		stream_disable,
		stream_enable,
		stream_getfmt,
		stream_setfmt,
		stream_timecode,
		stream_receive,
		0,
	},
	{
		"ppsclock STREAM",
		ppsclock_init,
		local_end,
		local_setcs,
		local_nop,
		local_nop,
		local_getfmt,
		local_setfmt,
		local_timecode,
		local_receive,
		local_input,
	},
#endif
	{
		"normal",
		local_init,
		local_end,
		local_setcs,
		local_nop,
		local_nop,
		local_getfmt,
		local_setfmt,
		local_timecode,
		local_receive,
		local_input,
	},
	{
		(char *)0,
	}
};

#ifdef STREAM

#define fix_ts(_X_) \
                        if ((&(_X_))->tv.tv_usec >= 1000000)                \
                          {                                                 \
			    (&(_X_))->tv.tv_usec -= 1000000;                \
			    (&(_X_))->tv.tv_sec  += 1;                      \
			  }

#define cvt_ts(_X_, _Y_) \
                        {                                                   \
			  l_fp ts;				            \
			  fix_ts((_X_));                                    \
			  if (!buftvtots((const char *)&(&(_X_))->tv, &ts)) \
			    {                                               \
                              ERR(ERR_BADDATA)	 		            \
                                msyslog(LOG_ERR,"parse: stream_receive: timestamp conversion error (buftvtots) (%s) (%ld.%06ld) ", (_Y_), (long)(&(_X_))->tv.tv_sec, (long)(&(_X_))->tv.tv_usec);\
			      return;                                       \
			    }                                               \
			  else                                              \
			    {                                               \
			      (&(_X_))->fp = ts;                            \
			    }                                               \
		        }

/*--------------------------------------------------
 * ppsclock STREAM init
 */
static int
ppsclock_init(
	struct parseunit *parse
	)
{
        static char m1[] = "ppsclocd";
	static char m2[] = "ppsclock";
	
	/*
	 * now push the parse streams module
	 * it will ensure exclusive access to the device
	 */
	if (ioctl(parse->generic->io.fd, I_PUSH, (caddr_t)m1) == -1 &&
	    ioctl(parse->generic->io.fd, I_PUSH, (caddr_t)m2) == -1)
	{
		if (errno != EINVAL)
		{
			msyslog(LOG_ERR, "PARSE receiver #%d: ppsclock_init: ioctl(fd, I_PUSH, \"ppsclock\"): %m",
				CLK_UNIT(parse->peer));
		}
		return 0;
	}
	if (!local_init(parse))
	{
		(void)ioctl(parse->generic->io.fd, I_POP, (caddr_t)0);
		return 0;
	}

	parse->flags |= PARSE_PPSCLOCK;
	return 1;
}

/*--------------------------------------------------
 * parse STREAM init
 */
static int
stream_init(
	struct parseunit *parse
	)
{
	static char m1[] = "parse";
	/*
	 * now push the parse streams module
	 * to test whether it is there (neat interface 8-( )
	 */
	if (ioctl(parse->generic->io.fd, I_PUSH, (caddr_t)m1) == -1)
	{
		if (errno != EINVAL) /* accept non-existence */
		{
			msyslog(LOG_ERR, "PARSE receiver #%d: stream_init: ioctl(fd, I_PUSH, \"parse\"): %m", CLK_UNIT(parse->peer));
		}
		return 0;
	}
	else
	{
		while(ioctl(parse->generic->io.fd, I_POP, (caddr_t)0) == 0)
		    /* empty loop */;

		/*
		 * now push it a second time after we have removed all
		 * module garbage
		 */
		if (ioctl(parse->generic->io.fd, I_PUSH, (caddr_t)m1) == -1)
		{
			msyslog(LOG_ERR, "PARSE receiver #%d: stream_init: ioctl(fd, I_PUSH, \"parse\"): %m", CLK_UNIT(parse->peer));
			return 0;
		}
		else
		{
			return 1;
		}
	}
}

/*--------------------------------------------------
 * parse STREAM end
 */
static void
stream_end(
	struct parseunit *parse
	)
{
	while(ioctl(parse->generic->io.fd, I_POP, (caddr_t)0) == 0)
	    /* empty loop */;
}

/*--------------------------------------------------
 * STREAM setcs
 */
static int
stream_setcs(
	struct parseunit *parse,
	parsectl_t  *tcl
	)
{
	struct strioctl strioc;
  
	strioc.ic_cmd     = PARSEIOC_SETCS;
	strioc.ic_timout  = 0;
	strioc.ic_dp      = (char *)tcl;
	strioc.ic_len     = sizeof (*tcl);

	if (ioctl(parse->generic->io.fd, I_STR, (caddr_t)&strioc) == -1)
	{
		msyslog(LOG_ERR, "PARSE receiver #%d: stream_setcs: ioctl(fd, I_STR, PARSEIOC_SETCS): %m", CLK_UNIT(parse->peer));
		return 0;
	}
	return 1;
}

/*--------------------------------------------------
 * STREAM enable
 */
static int
stream_enable(
	struct parseunit *parse
	)
{
	struct strioctl strioc;
  
	strioc.ic_cmd     = PARSEIOC_ENABLE;
	strioc.ic_timout  = 0;
	strioc.ic_dp      = (char *)0;
	strioc.ic_len     = 0;

	if (ioctl(parse->generic->io.fd, I_STR, (caddr_t)&strioc) == -1)
	{
		msyslog(LOG_ERR, "PARSE receiver #%d: stream_enable: ioctl(fd, I_STR, PARSEIOC_ENABLE): %m", CLK_UNIT(parse->peer));
		return 0;
	}
	parse->generic->io.clock_recv = stream_receive; /* ok - parse input in kernel */
	return 1;
}

/*--------------------------------------------------
 * STREAM disable
 */
static int
stream_disable(
	struct parseunit *parse
	)
{
	struct strioctl strioc;
  
	strioc.ic_cmd     = PARSEIOC_DISABLE;
	strioc.ic_timout  = 0;
	strioc.ic_dp      = (char *)0;
	strioc.ic_len     = 0;

	if (ioctl(parse->generic->io.fd, I_STR, (caddr_t)&strioc) == -1)
	{
		msyslog(LOG_ERR, "PARSE receiver #%d: stream_disable: ioctl(fd, I_STR, PARSEIOC_DISABLE): %m", CLK_UNIT(parse->peer));
		return 0;
	}
	parse->generic->io.clock_recv = local_receive; /* ok - parse input in daemon */
	return 1;
}

/*--------------------------------------------------
 * STREAM getfmt
 */
static int
stream_getfmt(
	struct parseunit *parse,
	parsectl_t  *tcl
	)
{
	struct strioctl strioc;
  
	strioc.ic_cmd     = PARSEIOC_GETFMT;
	strioc.ic_timout  = 0;
	strioc.ic_dp      = (char *)tcl;
	strioc.ic_len     = sizeof (*tcl);
	if (ioctl(parse->generic->io.fd, I_STR, (caddr_t)&strioc) == -1)
	{
		msyslog(LOG_ERR, "PARSE receiver #%d: ioctl(fd, I_STR, PARSEIOC_GETFMT): %m", CLK_UNIT(parse->peer));
		return 0;
	}
	return 1;
}

/*--------------------------------------------------
 * STREAM setfmt
 */
static int
stream_setfmt(
	struct parseunit *parse,
	parsectl_t  *tcl
	)
{
	struct strioctl strioc;
  
	strioc.ic_cmd     = PARSEIOC_SETFMT;
	strioc.ic_timout  = 0;
	strioc.ic_dp      = (char *)tcl;
	strioc.ic_len     = sizeof (*tcl);

	if (ioctl(parse->generic->io.fd, I_STR, (caddr_t)&strioc) == -1)
	{
		msyslog(LOG_ERR, "PARSE receiver #%d: stream_setfmt: ioctl(fd, I_STR, PARSEIOC_SETFMT): %m", CLK_UNIT(parse->peer));
		return 0;
	}
	return 1;
}


/*--------------------------------------------------
 * STREAM timecode
 */
static int
stream_timecode(
	struct parseunit *parse,
	parsectl_t  *tcl
	)
{
	struct strioctl strioc;
  
	strioc.ic_cmd     = PARSEIOC_TIMECODE;
	strioc.ic_timout  = 0;
	strioc.ic_dp      = (char *)tcl;
	strioc.ic_len     = sizeof (*tcl);
	
	if (ioctl(parse->generic->io.fd, I_STR, (caddr_t)&strioc) == -1)
	{
		ERR(ERR_INTERNAL)
			msyslog(LOG_ERR, "PARSE receiver #%d: stream_timecode: ioctl(fd, I_STR, PARSEIOC_TIMECODE): %m", CLK_UNIT(parse->peer));
		return 0;
	}
	clear_err(parse, ERR_INTERNAL);
	return 1;
}

/*--------------------------------------------------
 * STREAM receive
 */
static void
stream_receive(
	struct recvbuf *rbufp
	)
{
	struct parseunit *parse = (struct parseunit *)((void *)rbufp->recv_srcclock);
	parsetime_t parsetime;

	if (!parse->peer)
	    return;

	if (rbufp->recv_length != sizeof(parsetime_t))
	{
		ERR(ERR_BADIO)
			msyslog(LOG_ERR,"PARSE receiver #%d: stream_receive: bad size (got %d expected %d)",
				CLK_UNIT(parse->peer), rbufp->recv_length, (int)sizeof(parsetime_t));
		parse->generic->baddata++;
		parse_event(parse, CEVNT_BADREPLY);
		return;
	}
	clear_err(parse, ERR_BADIO);
  
	memmove((caddr_t)&parsetime,
		(caddr_t)rbufp->recv_buffer,
		sizeof(parsetime_t));

#ifdef DEBUG
	if (debug > 3)
	  {
	    printf("PARSE receiver #%d: status %06x, state %08x, time %lx.%08lx, stime %lx.%08lx, ptime %lx.%08lx\n",
		   CLK_UNIT(parse->peer),
		   (unsigned int)parsetime.parse_status,
		   (unsigned int)parsetime.parse_state,
		   (long)parsetime.parse_time.tv.tv_sec,
		   (long)parsetime.parse_time.tv.tv_usec,
		   (long)parsetime.parse_stime.tv.tv_sec,
		   (long)parsetime.parse_stime.tv.tv_usec,
		   (long)parsetime.parse_ptime.tv.tv_sec,
		   (long)parsetime.parse_ptime.tv.tv_usec);
	  }
#endif

	/*
	 * switch time stamp world - be sure to normalize small usec field
	 * errors.
	 */

	cvt_ts(parsetime.parse_stime, "parse_stime");

	if (PARSE_TIMECODE(parsetime.parse_state))
	{
	    cvt_ts(parsetime.parse_time, "parse_time");
	}

	if (PARSE_PPS(parsetime.parse_state))
	    cvt_ts(parsetime.parse_ptime, "parse_ptime");

	parse_process(parse, &parsetime);
}
#endif

/*--------------------------------------------------
 * local init
 */
static int
local_init(
	struct parseunit *parse
	)
{
	return parse_ioinit(&parse->parseio);
}

/*--------------------------------------------------
 * local end
 */
static void
local_end(
	struct parseunit *parse
	)
{
	parse_ioend(&parse->parseio);
}


/*--------------------------------------------------
 * local nop
 */
static int
local_nop(
	struct parseunit *parse
	)
{
	return 1;
}

/*--------------------------------------------------
 * local setcs
 */
static int
local_setcs(
	struct parseunit *parse,
	parsectl_t  *tcl
	)
{
	return parse_setcs(tcl, &parse->parseio);
}

/*--------------------------------------------------
 * local getfmt
 */
static int
local_getfmt(
	struct parseunit *parse,
	parsectl_t  *tcl
	)
{
	return parse_getfmt(tcl, &parse->parseio);
}

/*--------------------------------------------------
 * local setfmt
 */
static int
local_setfmt(
	struct parseunit *parse,
	parsectl_t  *tcl
	)
{
	return parse_setfmt(tcl, &parse->parseio);
}

/*--------------------------------------------------
 * local timecode
 */
static int
local_timecode(
	struct parseunit *parse,
	parsectl_t  *tcl
	)
{
	return parse_timecode(tcl, &parse->parseio);
}


/*--------------------------------------------------
 * local input
 */
static int
local_input(
	struct recvbuf *rbufp
	)
{
	struct parseunit *parse = (struct parseunit *)((void *)rbufp->recv_srcclock);
	int count;
	unsigned char *s;
	timestamp_t ts;

	if (!parse->peer)
		return 0;

	/*
	 * eat all characters, parsing then and feeding complete samples
	 */
	count = rbufp->recv_length;
	s = (unsigned char *)rbufp->recv_buffer;
	ts.fp = rbufp->recv_time;

	while (count--)
	{
		if (parse_ioread(&parse->parseio, (unsigned int)(*s++), &ts))
		{
			struct recvbuf buf;

			/*
			 * got something good to eat
			 */
			if (!PARSE_PPS(parse->parseio.parse_dtime.parse_state))
			{
#ifdef TIOCDCDTIMESTAMP
				struct timeval dcd_time;
				
				if (ioctl(rbufp->fd, TIOCDCDTIMESTAMP, &dcd_time) != -1)
				{
					l_fp tstmp;
					
					TVTOTS(&dcd_time, &tstmp);
					tstmp.l_ui += JAN_1970;
					L_SUB(&ts.fp, &tstmp);
					if (ts.fp.l_ui == 0)
					{
#ifdef DEBUG
						if (debug)
						{
							printf(
							       "parse: local_receive: fd %d DCDTIMESTAMP %s\n",
							       rbufp->fd,
							       lfptoa(&tstmp, 6));
							printf(" sigio %s\n",
							       lfptoa(&ts.fp, 6));
						}
#endif
						parse->parseio.parse_dtime.parse_ptime.fp = tstmp;
						parse->parseio.parse_dtime.parse_state |= PARSEB_PPS|PARSEB_S_PPS;
					}
				}
#else /* TIOCDCDTIMESTAMP */
#if defined(HAVE_STRUCT_PPSCLOCKEV) && (defined(HAVE_CIOGETEV) || defined(HAVE_TIOCGPPSEV))
				if (parse->flags & PARSE_PPSCLOCK)
				{
					l_fp tts;
					struct ppsclockev ev;

#ifdef HAVE_CIOGETEV
					if (ioctl(parse->generic->io.fd, CIOGETEV, (caddr_t)&ev) == 0)
#endif
#ifdef HAVE_TIOCGPPSEV
					if (ioctl(parse->generic->io.fd, TIOCGPPSEV, (caddr_t)&ev) == 0)
#endif
					{
						if (ev.serial != parse->ppsserial)
						{
							/*
							 * add PPS time stamp if available via ppsclock module
							 * and not supplied already.
							 */
							if (!buftvtots((const char *)&ev.tv, &tts))
							{
								ERR(ERR_BADDATA)
									msyslog(LOG_ERR,"parse: local_receive: timestamp conversion error (buftvtots) (ppsclockev.tv)");
							}
							else
							{
								parse->parseio.parse_dtime.parse_ptime.fp = tts;
								parse->parseio.parse_dtime.parse_state |= PARSEB_PPS|PARSEB_S_PPS;
							}
						}
						parse->ppsserial = ev.serial;
					}
				}
#endif
#endif /* TIOCDCDTIMESTAMP */
			}
			if (count)
			{	/* simulate receive */
				memmove((caddr_t)buf.recv_buffer,
					(caddr_t)&parse->parseio.parse_dtime,
					sizeof(parsetime_t));
				parse_iodone(&parse->parseio);
				buf.recv_length = sizeof(parsetime_t);
				buf.recv_time = rbufp->recv_time;
				buf.srcadr = rbufp->srcadr;
				buf.dstadr = rbufp->dstadr;
				buf.fd     = rbufp->fd;
				buf.next = 0;
				buf.X_from_where = rbufp->X_from_where;
				rbufp->receiver(&buf);
			}
			else
			{
				memmove((caddr_t)rbufp->recv_buffer,
					(caddr_t)&parse->parseio.parse_dtime,
					sizeof(parsetime_t));
				parse_iodone(&parse->parseio);
				rbufp->recv_length = sizeof(parsetime_t);
				return 1; /* got something & in place return */
			}
		}
	}
	return 0;		/* nothing to pass up */
}

/*--------------------------------------------------
 * local receive
 */
static void
local_receive(
	struct recvbuf *rbufp
	)
{
	struct parseunit *parse = (struct parseunit *)((void *)rbufp->recv_srcclock);
	parsetime_t parsetime;

	if (!parse->peer)
	    return;

	if (rbufp->recv_length != sizeof(parsetime_t))
	{
		ERR(ERR_BADIO)
			msyslog(LOG_ERR,"PARSE receiver #%d: local_receive: bad size (got %d expected %d)",
				CLK_UNIT(parse->peer), rbufp->recv_length, (int)sizeof(parsetime_t));
		parse->generic->baddata++;
		parse_event(parse, CEVNT_BADREPLY);
		return;
	}
	clear_err(parse, ERR_BADIO);
  
	memmove((caddr_t)&parsetime,
		(caddr_t)rbufp->recv_buffer,
		sizeof(parsetime_t));

#ifdef DEBUG
	if (debug > 3)
	  {
	    printf("PARSE receiver #%d: status %06x, state %08x, time %lx.%08lx, stime %lx.%08lx, ptime %lx.%08lx\n",
		   CLK_UNIT(parse->peer),
		   (unsigned int)parsetime.parse_status,
		   (unsigned int)parsetime.parse_state,
		   (long)parsetime.parse_time.tv.tv_sec,
		   (long)parsetime.parse_time.tv.tv_usec,
		   (long)parsetime.parse_stime.tv.tv_sec,
		   (long)parsetime.parse_stime.tv.tv_usec,
		   (long)parsetime.parse_ptime.tv.tv_sec,
		   (long)parsetime.parse_ptime.tv.tv_usec);
	  }
#endif

	parse_process(parse, &parsetime);
}

/*--------------------------------------------------
 * init_iobinding - find and initialize lower layers
 */
static bind_t *
init_iobinding(
	struct parseunit *parse
	)
{
  bind_t *b = io_bindings;

	while (b->bd_description != (char *)0)
	{
		if ((*b->bd_init)(parse))
		{
			return b;
		}
		b++;
	}
	return (bind_t *)0;
}

/**===========================================================================
 ** support routines
 **/

/*--------------------------------------------------
 * convert a flag field to a string
 */
static char *
parsestate(
	u_long lstate,
	char *buffer
	)
{
	static struct bits
	{
		u_long      bit;
		const char *name;
	} flagstrings[] =
	  {
		  { PARSEB_ANNOUNCE,   "DST SWITCH WARNING" },
		  { PARSEB_POWERUP,    "NOT SYNCHRONIZED" },
		  { PARSEB_NOSYNC,     "TIME CODE NOT CONFIRMED" },
		  { PARSEB_DST,        "DST" },
		  { PARSEB_UTC,        "UTC DISPLAY" },
		  { PARSEB_LEAPADD,    "LEAP ADD WARNING" },
		  { PARSEB_LEAPDEL,    "LEAP DELETE WARNING" },
		  { PARSEB_LEAPSECOND, "LEAP SECOND" },
		  { PARSEB_ALTERNATE,  "ALTERNATE ANTENNA" },
		  { PARSEB_TIMECODE,   "TIME CODE" },
		  { PARSEB_PPS,        "PPS" },
		  { PARSEB_POSITION,   "POSITION" },
		  { 0 }
	  };

	static struct sbits
	{
		u_long      bit;
		const char *name;
	} sflagstrings[] =
	  {
		  { PARSEB_S_LEAP,     "LEAP INDICATION" },
		  { PARSEB_S_PPS,      "PPS SIGNAL" },
		  { PARSEB_S_ANTENNA,  "ANTENNA" },
		  { PARSEB_S_POSITION, "POSITION" },
		  { 0 }
	  };
	int i;

	*buffer = '\0';

	i = 0;
	while (flagstrings[i].bit)
	{
		if (flagstrings[i].bit & lstate)
		{
			if (buffer[0])
			    strcat(buffer, "; ");
			strcat(buffer, flagstrings[i].name);
		}
		i++;
	}

	if (lstate & (PARSEB_S_LEAP|PARSEB_S_ANTENNA|PARSEB_S_PPS|PARSEB_S_POSITION))
	{
      char *s, *t;

		if (buffer[0])
		    strcat(buffer, "; ");

		strcat(buffer, "(");

		t = s = buffer + strlen(buffer);

		i = 0;
		while (sflagstrings[i].bit)
		{
			if (sflagstrings[i].bit & lstate)
			{
				if (t != s)
				{
					strcpy(t, "; ");
					t += 2;
				}
	
				strcpy(t, sflagstrings[i].name);
				t += strlen(t);
			}
			i++;
		}
		strcpy(t, ")");
	}
	return buffer;
}

/*--------------------------------------------------
 * convert a status flag field to a string
 */
static char *
parsestatus(
	u_long lstate,
	char *buffer
	)
{
	static struct bits
	{
		u_long      bit;
		const char *name;
	} flagstrings[] =
	  {
		  { CVT_OK,      "CONVERSION SUCCESSFUL" },
		  { CVT_NONE,    "NO CONVERSION" },
		  { CVT_FAIL,    "CONVERSION FAILED" },
		  { CVT_BADFMT,  "ILLEGAL FORMAT" },
		  { CVT_BADDATE, "DATE ILLEGAL" },
		  { CVT_BADTIME, "TIME ILLEGAL" },
		  { CVT_ADDITIONAL, "ADDITIONAL DATA" },
		  { 0 }
	  };
	int i;

	*buffer = '\0';

	i = 0;
	while (flagstrings[i].bit)
	{
		if (flagstrings[i].bit & lstate)
		{
			if (buffer[0])
			    strcat(buffer, "; ");
			strcat(buffer, flagstrings[i].name);
		}
		i++;
	}

	return buffer;
}

/*--------------------------------------------------
 * convert a clock status flag field to a string
 */
static const char *
clockstatus(
	u_long lstate
	)
{
	static char buffer[20];
	static struct status
	{
		u_long      value;
		const char *name;
	} flagstrings[] =
	  {
		  { CEVNT_NOMINAL, "NOMINAL" },
		  { CEVNT_TIMEOUT, "NO RESPONSE" },
		  { CEVNT_BADREPLY,"BAD FORMAT" },
		  { CEVNT_FAULT,   "FAULT" },
		  { CEVNT_PROP,    "PROPAGATION DELAY" },
		  { CEVNT_BADDATE, "ILLEGAL DATE" },
		  { CEVNT_BADTIME, "ILLEGAL TIME" },
		  { (unsigned)~0L }
	  };
	int i;

	i = 0;
	while (flagstrings[i].value != ~0)
	{
		if (flagstrings[i].value == lstate)
		{
			return flagstrings[i].name;
		}
		i++;
	}

	sprintf(buffer, "unknown #%ld", (u_long)lstate);

	return buffer;
}


/*--------------------------------------------------
 * l_mktime - make representation of a relative time
 */
static char *
l_mktime(
	u_long delta
	)
{
	u_long tmp, m, s;
	static char buffer[40];

	buffer[0] = '\0';

	if ((tmp = delta / (60*60*24)) != 0)
	{
		sprintf(buffer, "%ldd+", (u_long)tmp);
		delta -= tmp * 60*60*24;
	}

	s = delta % 60;
	delta /= 60;
	m = delta % 60;
	delta /= 60;

	sprintf(buffer+strlen(buffer), "%02d:%02d:%02d",
		(int)delta, (int)m, (int)s);

	return buffer;
}


/*--------------------------------------------------
 * parse_statistics - list summary of clock states
 */
static void
parse_statistics(
	struct parseunit *parse
	)
{
	int i;

	NLOG(NLOG_CLOCKSTATIST) /* conditional if clause for conditional syslog */
		{
			msyslog(LOG_INFO, "PARSE receiver #%d: running time: %s",
				CLK_UNIT(parse->peer),
				l_mktime(current_time - parse->generic->timestarted));

			msyslog(LOG_INFO, "PARSE receiver #%d: current status: %s",
				CLK_UNIT(parse->peer),
				clockstatus(parse->generic->currentstatus));

			for (i = 0; i <= CEVNT_MAX; i++)
			{
				u_long s_time;
				u_long percent, d = current_time - parse->generic->timestarted;

				percent = s_time = PARSE_STATETIME(parse, i);

				while (((u_long)(~0) / 10000) < percent)
				{
					percent /= 10;
					d       /= 10;
				}

				if (d)
				    percent = (percent * 10000) / d;
				else
				    percent = 10000;

				if (s_time)
				    msyslog(LOG_INFO, "PARSE receiver #%d: state %18s: %13s (%3ld.%02ld%%)",
					    CLK_UNIT(parse->peer),
					    clockstatus((unsigned int)i),
					    l_mktime(s_time),
					    percent / 100, percent % 100);
			}
		}
}

/*--------------------------------------------------
 * cparse_statistics - wrapper for statistics call
 */
static void
cparse_statistics(
	register struct parseunit *parse
	)
{
	if (parse->laststatistic + PARSESTATISTICS < current_time)
		parse_statistics(parse);
	parse->laststatistic = current_time;
}

/**===========================================================================
 ** ntp interface routines
 **/

/*--------------------------------------------------
 * parse_init - initialize internal parse driver data
 */
static void
parse_init(void)
{
	memset((caddr_t)parseunits, 0, sizeof parseunits);
}


/*--------------------------------------------------
 * parse_shutdown - shut down a PARSE clock
 */
static void
parse_shutdown(
	int unit,
	struct peer *peer
	)
{
	struct parseunit *parse = (struct parseunit *)peer->procptr->unitptr;

	if (parse && !parse->peer)
	{
		msyslog(LOG_ERR,
			"PARSE receiver #%d: parse_shutdown: INTERNAL ERROR, unit not in use", unit);
		return;
	}

	/*
	 * print statistics a last time and
	 * stop statistics machine
	 */
	parse_statistics(parse);

	if (parse->parse_type->cl_end)
	{
		parse->parse_type->cl_end(parse);
	}
	
	if (parse->binding)
	    PARSE_END(parse);

	/*
	 * Tell the I/O module to turn us off.  We're history.
	 */
	io_closeclock(&parse->generic->io);

	free_varlist(parse->kv);
  
	NLOG(NLOG_CLOCKINFO) /* conditional if clause for conditional syslog */
		msyslog(LOG_INFO, "PARSE receiver #%d: reference clock \"%s\" removed",
			CLK_UNIT(parse->peer), parse->parse_type->cl_description);

	parse->peer = (struct peer *)0; /* unused now */
	free(parse);
}

/*--------------------------------------------------
 * parse_start - open the PARSE devices and initialize data for processing
 */
static int
parse_start(
	int sysunit,
	struct peer *peer
	)
{
	u_int unit;
	int fd232;
#ifdef HAVE_TERMIOS
	struct termios tio;		/* NEEDED FOR A LONG TIME ! */
#endif
#ifdef HAVE_SYSV_TTYS
	struct termio tio;		/* NEEDED FOR A LONG TIME ! */
#endif
	struct parseunit * parse;
	char parsedev[sizeof(PARSEDEVICE)+20];
	parsectl_t tmp_ctl;
	u_int type;

	type = CLK_TYPE(peer);
	unit = CLK_UNIT(peer);

	if ((type == ~0) || (parse_clockinfo[type].cl_description == (char *)0))
	{
		msyslog(LOG_ERR, "PARSE receiver #%d: parse_start: unsupported clock type %d (max %d)",
			unit, CLK_REALTYPE(peer), ncltypes-1);
		return 0;
	}

	/*
	 * Unit okay, attempt to open the device.
	 */
	(void) sprintf(parsedev, PARSEDEVICE, unit);

#ifndef O_NOCTTY
#define O_NOCTTY 0
#endif

	fd232 = open(parsedev, O_RDWR | O_NOCTTY
#ifdef O_NONBLOCK
		     | O_NONBLOCK
#endif
		     , 0777);

	if (fd232 == -1)
	{
		msyslog(LOG_ERR, "PARSE receiver #%d: parse_start: open of %s failed: %m", unit, parsedev);
		return 0;
	}

	parse = (struct parseunit *)emalloc(sizeof(struct parseunit));

	memset((char *)parse, 0, sizeof(struct parseunit));

	parse->generic = peer->procptr;	 /* link up */
	parse->generic->unitptr = (caddr_t)parse; /* link down */

	/*
	 * Set up the structures
	 */
	parse->generic->timestarted    = current_time;
	parse->lastchange     = current_time;

	parse->generic->currentstatus	        = CEVNT_TIMEOUT; /* expect the worst */

	parse->flags          = 0;
	parse->pollneeddata   = 0;
	parse->laststatistic  = current_time;
	parse->lastformat     = (unsigned short)~0;	/* assume no format known */
	parse->time.parse_status = (unsigned short)~0;	/* be sure to mark initial status change */
	parse->lastmissed     = 0;	/* assume got everything */
	parse->ppsserial      = 0;
	parse->localdata      = (void *)0;
	parse->localstate     = 0;
	parse->kv             = (struct ctl_var *)0;

	clear_err(parse, ERR_ALL);
  
	parse->parse_type     = &parse_clockinfo[type];
	
	parse->generic->fudgetime1 = parse->parse_type->cl_basedelay;

	parse->generic->fudgetime2 = 0.0;

	parse->generic->clockdesc = parse->parse_type->cl_description;

	peer->rootdelay       = parse->parse_type->cl_rootdelay;
	peer->sstclktype      = parse->parse_type->cl_type;
	peer->precision       = sys_precision;
	
	peer->stratum         = STRATUM_REFCLOCK;
	if (peer->stratum <= 1)
	    memmove((char *)&parse->generic->refid, parse->parse_type->cl_id, 4);
	else
	    parse->generic->refid = htonl(PARSEHSREFID);
	
	parse->generic->io.fd = fd232;
	
	parse->peer = peer;		/* marks it also as busy */

	/*
	 * configure terminal line
	 */
	if (TTY_GETATTR(fd232, &tio) == -1)
	{
		msyslog(LOG_ERR, "PARSE receiver #%d: parse_start: tcgetattr(%d, &tio): %m", unit, fd232);
		parse_shutdown(CLK_UNIT(parse->peer), peer); /* let our cleaning staff do the work */
		return 0;
	}
	else
	{
#ifndef _PC_VDISABLE
		memset((char *)tio.c_cc, 0, sizeof(tio.c_cc));
#else
		int disablec;
		errno = 0;		/* pathconf can deliver -1 without changing errno ! */

		disablec = fpathconf(parse->generic->io.fd, _PC_VDISABLE);
		if (disablec == -1 && errno)
		{
			msyslog(LOG_ERR, "PARSE receiver #%d: parse_start: fpathconf(fd, _PC_VDISABLE): %m", CLK_UNIT(parse->peer));
			memset((char *)tio.c_cc, 0, sizeof(tio.c_cc)); /* best guess */
		}
		else
		    if (disablec != -1)
			memset((char *)tio.c_cc, disablec, sizeof(tio.c_cc));
#endif

#if defined (VMIN) || defined(VTIME)
		if ((parse_clockinfo[type].cl_lflag & ICANON) == 0)
		{
#ifdef VMIN
			tio.c_cc[VMIN]   = 1;
#endif
#ifdef VTIME
			tio.c_cc[VTIME]  = 0;
#endif
		}
#endif

		tio.c_cflag = parse_clockinfo[type].cl_cflag;
		tio.c_iflag = parse_clockinfo[type].cl_iflag;
		tio.c_oflag = parse_clockinfo[type].cl_oflag;
		tio.c_lflag = parse_clockinfo[type].cl_lflag;
	

#ifdef HAVE_TERMIOS
		if ((cfsetospeed(&tio, parse_clockinfo[type].cl_speed) == -1) ||
		    (cfsetispeed(&tio, parse_clockinfo[type].cl_speed) == -1))
		{
			msyslog(LOG_ERR, "PARSE receiver #%d: parse_start: tcset{i,o}speed(&tio, speed): %m", unit);
			parse_shutdown(CLK_UNIT(parse->peer), peer); /* let our cleaning staff do the work */
			return 0;
		}
#else
		tio.c_cflag     |= parse_clockinfo[type].cl_speed;
#endif

#if defined(HAVE_TIO_SERIAL_STUFF)		/* Linux hack: define PPS interface */
		{
		  struct serial_struct	ss;
		  if (ioctl(fd232, TIOCGSERIAL, &ss) < 0 ||
		      (
#ifdef ASYNC_LOW_LATENCY
		       ss.flags |= ASYNC_LOW_LATENCY,
#endif
#ifdef ASYNC_PPS_CD_NEG
		       ss.flags |= ASYNC_PPS_CD_NEG,
#endif
		       ioctl(fd232, TIOCSSERIAL, &ss)) < 0) {
		    msyslog(LOG_NOTICE, "refclock_parse: TIOCSSERIAL fd %d, %m", fd232);
		    msyslog(LOG_NOTICE,
			    "refclock_parse: optional PPS processing not available");
		  } else {
		    parse->flags    |= PARSE_PPSCLOCK;
		    msyslog(LOG_INFO,
			    "refclock_parse: PPS detection on");
		  }
		}
#endif
#ifdef HAVE_TIOCSPPS			/* SUN PPS support */
		if (CLK_PPS(parse->peer))
		  {
		    int i = 1;
		    
		    if (ioctl(fd232, TIOCSPPS, (caddr_t)&i) == 0)
		      {
			parse->flags |= PARSE_PPSCLOCK;
		      }
		  }
#endif

		if (TTY_SETATTR(fd232, &tio) == -1)
		{
			msyslog(LOG_ERR, "PARSE receiver #%d: parse_start: tcsetattr(%d, &tio): %m", unit, fd232);
			parse_shutdown(CLK_UNIT(parse->peer), peer); /* let our cleaning staff do the work */
			return 0;
		}
	}

	/*
	 * Insert in async io device list.
	 */
	parse->generic->io.srcclock = (caddr_t)parse;
	parse->generic->io.datalen = 0;
	
	if (!io_addclock(&parse->generic->io))
        {
		msyslog(LOG_ERR,
			"PARSE receiver #%d: parse_start: addclock %s fails (ABORT - clock type requires async io)", CLK_UNIT(parse->peer), parsedev);
		parse_shutdown(CLK_UNIT(parse->peer), peer); /* let our cleaning staff do the work */
		return 0;
	}

	parse->binding = init_iobinding(parse);
	parse->generic->io.clock_recv = parse->binding->bd_receive; /* pick correct receive routine */
	parse->generic->io.io_input   = parse->binding->bd_io_input; /* pick correct input routine */

	if (parse->binding == (bind_t *)0)
		{
			msyslog(LOG_ERR, "PARSE receiver #%d: parse_start: io sub system initialisation failed.", CLK_UNIT(parse->peer));
			parse_shutdown(CLK_UNIT(parse->peer), peer); /* let our cleaning staff do the work */
			return 0;			/* well, ok - special initialisation broke */
		}      

	/*
	 * as we always(?) get 8 bit chars we want to be
	 * sure, that the upper bits are zero for less
	 * than 8 bit I/O - so we pass that information on.
	 * note that there can be only one bit count format
	 * per file descriptor
	 */

	switch (tio.c_cflag & CSIZE)
	{
	    case CS5:
		tmp_ctl.parsesetcs.parse_cs = PARSE_IO_CS5;
		break;

	    case CS6:
		tmp_ctl.parsesetcs.parse_cs = PARSE_IO_CS6;
		break;

	    case CS7:
		tmp_ctl.parsesetcs.parse_cs = PARSE_IO_CS7;
		break;

	    case CS8:
		tmp_ctl.parsesetcs.parse_cs = PARSE_IO_CS8;
		break;
	}

	if (!PARSE_SETCS(parse, &tmp_ctl))
	{
		msyslog(LOG_ERR, "PARSE receiver #%d: parse_start: parse_setcs() FAILED.", unit);
		parse_shutdown(CLK_UNIT(parse->peer), peer); /* let our cleaning staff do the work */
		return 0;			/* well, ok - special initialisation broke */
	}
  
	strcpy(tmp_ctl.parseformat.parse_buffer, parse->parse_type->cl_format);
	tmp_ctl.parseformat.parse_count = strlen(tmp_ctl.parseformat.parse_buffer);

	if (!PARSE_SETFMT(parse, &tmp_ctl))
	{
		msyslog(LOG_ERR, "PARSE receiver #%d: parse_start: parse_setfmt() FAILED.", unit);
		parse_shutdown(CLK_UNIT(parse->peer), peer); /* let our cleaning staff do the work */
		return 0;			/* well, ok - special initialisation broke */
	}
  
	/*
	 * get rid of all IO accumulated so far
	 */
#ifdef HAVE_TERMIOS
	(void) tcflush(parse->generic->io.fd, TCIOFLUSH);
#else
#ifdef TCFLSH
	{
#ifndef TCIOFLUSH
#define TCIOFLUSH 2
#endif
		int flshcmd = TCIOFLUSH;

		(void) ioctl(parse->generic->io.fd, TCFLSH, (caddr_t)&flshcmd);
	}
#endif
#endif

	/*
	 * try to do any special initializations
	 */
	if (parse->parse_type->cl_init)
		{
			if (parse->parse_type->cl_init(parse))
				{
					parse_shutdown(CLK_UNIT(parse->peer), peer); /* let our cleaning staff do the work */
					return 0;		/* well, ok - special initialisation broke */
				}
		}
	
	/*
	 * get out Copyright information once
	 */
	if (!notice)
        {
		NLOG(NLOG_CLOCKINFO) /* conditional if clause for conditional syslog */
			msyslog(LOG_INFO, "NTP PARSE support: Copyright (c) 1989-1999, Frank Kardel");
		notice = 1;
	}

	/*
	 * print out configuration
	 */
	NLOG(NLOG_CLOCKINFO)
		{
			/* conditional if clause for conditional syslog */
			msyslog(LOG_INFO, "PARSE receiver #%d: reference clock \"%s\" (device %s) added",
				CLK_UNIT(parse->peer),
				parse->parse_type->cl_description, parsedev);

			msyslog(LOG_INFO, "PARSE receiver #%d:  Stratum %d, %sPPS support, trust time %s, precision %d",
				CLK_UNIT(parse->peer),
				parse->peer->stratum, CLK_PPS(parse->peer) ? "" : "no ",
				l_mktime(parse->parse_type->cl_maxunsync), parse->peer->precision);

			msyslog(LOG_INFO, "PARSE receiver #%d:  rootdelay %.6f s, phaseadjust %.6f s, %s IO handling",
				CLK_UNIT(parse->peer),
				parse->parse_type->cl_rootdelay,
				parse->generic->fudgetime1,
				parse->binding->bd_description);

			msyslog(LOG_INFO, "PARSE receiver #%d:  Format recognition: %s", CLK_UNIT(parse->peer),
				parse->parse_type->cl_format);
#ifdef PPS
                        msyslog(LOG_INFO, "PARSE receiver #%d:  %sPPS ioctl support", CLK_UNIT(parse->peer),
				(parse->flags & PARSE_PPSCLOCK) ? "" : "NO ");
#endif
		}

	return 1;
}

/*--------------------------------------------------
 * parse_poll - called by the transmit procedure
 */
static void
parse_poll(
	int unit,
	struct peer *peer
	)
{
	struct parseunit *parse = (struct parseunit *)peer->procptr->unitptr;

	if (peer != parse->peer)
	{
		msyslog(LOG_ERR,
			"PARSE receiver #%d: poll: INTERNAL: peer incorrect",
			unit);
		return;
	}

	/*
	 * Update clock stat counters
	 */
	parse->generic->polls++;

	if (parse->pollneeddata && 
	    ((current_time - parse->pollneeddata) > (1<<(max(min(parse->peer->hpoll, parse->peer->ppoll), parse->peer->minpoll)))))
	{
		/*
		 * start worrying when exceeding a poll inteval
		 * bad news - didn't get a response last time
		 */
		parse->generic->noreply++;
		parse->lastmissed = current_time;
		parse_event(parse, CEVNT_TIMEOUT);
		
		ERR(ERR_NODATA)
			msyslog(LOG_WARNING, "PARSE receiver #%d: no data from device within poll interval (check receiver / cableling)", CLK_UNIT(parse->peer));
	}

	/*
	 * we just mark that we want the next sample for the clock filter
	 */
	parse->pollneeddata = current_time;

	if (parse->parse_type->cl_poll)
	{
		parse->parse_type->cl_poll(parse);
	}

	cparse_statistics(parse);

	return;
}

#define LEN_STATES 300		/* length of state string */

/*--------------------------------------------------
 * parse_control - set fudge factors, return statistics
 */
static void
parse_control(
	int unit,
	struct refclockstat *in,
	struct refclockstat *out,
	struct peer *peer
	)
{
	register struct parseunit *parse = (struct parseunit *)peer->procptr->unitptr;
	parsectl_t tmpctl;

	static char outstatus[400];	/* status output buffer */

	if (out)
	{
		out->lencode       = 0;
		out->p_lastcode    = 0;
		out->kv_list       = (struct ctl_var *)0;
	}

	if (!parse || !parse->peer)
	{
		msyslog(LOG_ERR, "PARSE receiver #%d: parse_control: unit invalid (UNIT INACTIVE)",
			unit);
		return;
	}

	unit = CLK_UNIT(parse->peer);

	if (in)
	{
		if (in->haveflags & (CLK_HAVEFLAG1|CLK_HAVEFLAG2|CLK_HAVEFLAG3|CLK_HAVEFLAG4))
		{
			parse->flags = in->flags & (CLK_FLAG1|CLK_FLAG2|CLK_FLAG3|CLK_FLAG4);
		}
	}

	if (out)
	{
		u_long sum = 0;
		char *t, *tt, *start;
		int i;

		outstatus[0] = '\0';

		out->type       = REFCLK_PARSE;
		out->haveflags |= CLK_HAVETIME2;

		/*
		 * figure out skew between PPS and RS232 - just for informational
		 * purposes - returned in time2 value
		 */
		if (PARSE_SYNC(parse->time.parse_state))
		{
			if (PARSE_PPS(parse->time.parse_state) && PARSE_TIMECODE(parse->time.parse_state))
			{
				l_fp off;

				/*
				 * we have a PPS and RS232 signal - calculate the skew
				 * WARNING: assumes on TIMECODE == PULSE (timecode after pulse)
				 */
				off = parse->time.parse_stime.fp;
				L_SUB(&off, &parse->time.parse_ptime.fp); /* true offset */
				tt = add_var(&out->kv_list, 80, RO);
				sprintf(tt, "refclock_ppsskew=%s", lfptoms(&off, 6));
			}
		}

		if (PARSE_PPS(parse->time.parse_state))
		{
			tt = add_var(&out->kv_list, 80, RO|DEF);
			sprintf(tt, "refclock_ppstime=\"%s\"", gmprettydate(&parse->time.parse_ptime.fp));
		}

		tt = add_var(&out->kv_list, 128, RO|DEF);
		sprintf(tt, "refclock_time=\"");
		tt += strlen(tt);

		if (parse->time.parse_time.fp.l_ui == 0)
		{
			strcpy(tt, "<UNDEFINED>\"");
		}
		else
		{
			sprintf(tt, "%s\"", gmprettydate(&parse->time.parse_time.fp));
			t = tt + strlen(tt);
		}

		if (!PARSE_GETTIMECODE(parse, &tmpctl))
		{
			ERR(ERR_INTERNAL)
				msyslog(LOG_ERR, "PARSE receiver #%d: parse_control: parse_timecode() FAILED", unit);
		}
		else
		{
			tt = add_var(&out->kv_list, 512, RO|DEF);
			sprintf(tt, "refclock_status=\"");
			tt += strlen(tt);

			/*
			 * copy PPS flags from last read transaction (informational only)
			 */
			tmpctl.parsegettc.parse_state |= parse->time.parse_state &
				(PARSEB_PPS|PARSEB_S_PPS);

			(void) parsestate(tmpctl.parsegettc.parse_state, tt);

			strcat(tt, "\"");

			if (tmpctl.parsegettc.parse_count)
			    mkascii(outstatus+strlen(outstatus), (int)(sizeof(outstatus)- strlen(outstatus) - 1),
				    tmpctl.parsegettc.parse_buffer, (unsigned)(tmpctl.parsegettc.parse_count - 1));

			parse->generic->badformat += tmpctl.parsegettc.parse_badformat;
		}
	
		tmpctl.parseformat.parse_format = tmpctl.parsegettc.parse_format;
	
		if (!PARSE_GETFMT(parse, &tmpctl))
		{
			ERR(ERR_INTERNAL)
				msyslog(LOG_ERR, "PARSE receiver #%d: parse_control: parse_getfmt() FAILED", unit);
		}
		else
		{
			tt = add_var(&out->kv_list, 80, RO|DEF);
			sprintf(tt, "refclock_format=\"");

			strncat(tt, tmpctl.parseformat.parse_buffer, tmpctl.parseformat.parse_count);
			strcat(tt,"\"");
		}

		/*
		 * gather state statistics
		 */

		start = tt = add_var(&out->kv_list, LEN_STATES, RO|DEF);
		strcpy(tt, "refclock_states=\"");
		tt += strlen(tt);

		for (i = 0; i <= CEVNT_MAX; i++)
		{
			u_long s_time;
			u_long d = current_time - parse->generic->timestarted;
			u_long percent;

			percent = s_time = PARSE_STATETIME(parse, i);

			while (((u_long)(~0) / 10000) < percent)
			{
				percent /= 10;
				d       /= 10;
			}
	
			if (d)
			    percent = (percent * 10000) / d;
			else
			    percent = 10000;

			if (s_time)
			{
				char item[80];
				int count;
				
				sprintf(item, "%s%s%s: %s (%d.%02d%%)",
					sum ? "; " : "",
					(parse->generic->currentstatus == i) ? "*" : "",
					clockstatus((unsigned int)i),
					l_mktime(s_time),
					(int)(percent / 100), (int)(percent % 100));
				if ((count = strlen(item)) < (LEN_STATES - 40 - (tt - start)))
					{
						strcpy(tt, item);
						tt  += count;
					}
				sum += s_time;
			}
		}
		
		sprintf(tt, "; running time: %s\"", l_mktime(sum));
		
		tt = add_var(&out->kv_list, 32, RO);
		sprintf(tt, "refclock_id=\"%s\"", parse->parse_type->cl_id);
		
		tt = add_var(&out->kv_list, 80, RO);
		sprintf(tt, "refclock_iomode=\"%s\"", parse->binding->bd_description);

		tt = add_var(&out->kv_list, 128, RO);
		sprintf(tt, "refclock_driver_version=\"%s\"", rcsid);
		
		{
			struct ctl_var *k;
			
			k = parse->kv;
			while (k && !(k->flags & EOV))
			{
				set_var(&out->kv_list, k->text, strlen(k->text)+1, k->flags);
				k++;
			}
		}
      
		out->lencode       = strlen(outstatus);
		out->p_lastcode    = outstatus;
	}
}

/**===========================================================================
 ** processing routines
 **/

/*--------------------------------------------------
 * event handling - note that nominal events will also be posted
 */
static void
parse_event(
	struct parseunit *parse,
	int event
	)
{
	if (parse->generic->currentstatus != (u_char) event)
	{
		parse->statetime[parse->generic->currentstatus] += current_time - parse->lastchange;
		parse->lastchange              = current_time;

		parse->generic->currentstatus    = (u_char)event;

		if (parse->parse_type->cl_event)
		    parse->parse_type->cl_event(parse, event);
      
		if (event != CEVNT_NOMINAL)
		{
	  parse->generic->lastevent = parse->generic->currentstatus;
		}
		else
		{
			NLOG(NLOG_CLOCKSTATUS)
				msyslog(LOG_INFO, "PARSE receiver #%d: SYNCHRONIZED",
					CLK_UNIT(parse->peer));
		}

		if (event == CEVNT_FAULT)
		{
			NLOG(NLOG_CLOCKEVENT) /* conditional if clause for conditional syslog */
				ERR(ERR_BADEVENT)
				msyslog(LOG_ERR,
					"clock %s fault '%s' (0x%02x)", refnumtoa(&parse->peer->srcadr), ceventstr(event),
					(u_int)event);
		}
		else
		{
			NLOG(NLOG_CLOCKEVENT) /* conditional if clause for conditional syslog */
				if (event == CEVNT_NOMINAL || list_err(parse, ERR_BADEVENT))
				    msyslog(LOG_INFO,
					    "clock %s event '%s' (0x%02x)", refnumtoa(&parse->peer->srcadr), ceventstr(event),
					    (u_int)event);
		}

		report_event(EVNT_PEERCLOCK, parse->peer);
		report_event(EVNT_CLOCKEXCPT, parse->peer);
	}
}

/*--------------------------------------------------
 * process a PARSE time sample
 */
static void
parse_process(
	struct parseunit *parse,
	parsetime_t      *parsetime
	)
{
	l_fp off, rectime, reftime;
	double fudge;
	
	/*
	 * check for changes in conversion status
	 * (only one for each new status !)
	 */
	if (((parsetime->parse_status & CVT_MASK) != CVT_OK) &&
	    ((parsetime->parse_status & CVT_MASK) != CVT_NONE) &&
	    (parse->time.parse_status != parsetime->parse_status))
	{
		char buffer[400];
		
		NLOG(NLOG_CLOCKINFO) /* conditional if clause for conditional syslog */
			msyslog(LOG_WARNING, "PARSE receiver #%d: conversion status \"%s\"",
				CLK_UNIT(parse->peer), parsestatus(parsetime->parse_status, buffer));
		
		if ((parsetime->parse_status & CVT_MASK) == CVT_FAIL)
		{
			/*
			 * tell more about the story - list time code
			 * there is a slight change for a race condition and
			 * the time code might be overwritten by the next packet
			 */
			parsectl_t tmpctl;
			
			if (!PARSE_GETTIMECODE(parse, &tmpctl))
			{
				ERR(ERR_INTERNAL)
					msyslog(LOG_ERR, "PARSE receiver #%d: parse_process: parse_timecode() FAILED", CLK_UNIT(parse->peer));
			}
			else
			{
				ERR(ERR_BADDATA)
					msyslog(LOG_WARNING, "PARSE receiver #%d: FAILED TIMECODE: \"%s\" (check receiver configuration / cableling)",
						CLK_UNIT(parse->peer), mkascii(buffer, sizeof buffer, tmpctl.parsegettc.parse_buffer, (unsigned)(tmpctl.parsegettc.parse_count - 1)));
				parse->generic->badformat += tmpctl.parsegettc.parse_badformat;
			}
		}
	}

	/*
	 * examine status and post appropriate events
	 */
	if ((parsetime->parse_status & CVT_MASK) != CVT_OK)
	{
		/*
		 * got bad data - tell the rest of the system
		 */
		switch (parsetime->parse_status & CVT_MASK)
		{
		case CVT_NONE:
			if ((parsetime->parse_status & CVT_ADDITIONAL) &&
			    parse->parse_type->cl_message)
				parse->parse_type->cl_message(parse, parsetime);
			break; 		/* well, still waiting - timeout is handled at higher levels */
			    
		case CVT_FAIL:
			parse->generic->badformat++;
			if (parsetime->parse_status & CVT_BADFMT)
			{
				parse_event(parse, CEVNT_BADREPLY);
			}
			else
				if (parsetime->parse_status & CVT_BADDATE)
				{
					parse_event(parse, CEVNT_BADDATE);
				}
				else
					if (parsetime->parse_status & CVT_BADTIME)
					{
						parse_event(parse, CEVNT_BADTIME);
					}
					else
					{
						parse_event(parse, CEVNT_BADREPLY); /* for the lack of something better */
					}
		}
		return;			/* skip the rest - useless */
	}

	/*
	 * check for format changes
	 * (in case somebody has swapped clocks 8-)
	 */
	if (parse->lastformat != parsetime->parse_format)
	{
		parsectl_t tmpctl;
	
		tmpctl.parseformat.parse_format = parsetime->parse_format;

		if (!PARSE_GETFMT(parse, &tmpctl))
		{
			ERR(ERR_INTERNAL)
				msyslog(LOG_ERR, "PARSE receiver #%d: parse_getfmt() FAILED", CLK_UNIT(parse->peer));
		}
		else
		{
			NLOG(NLOG_CLOCKINFO) /* conditional if clause for conditional syslog */
				msyslog(LOG_INFO, "PARSE receiver #%d: packet format \"%s\"",
					CLK_UNIT(parse->peer), tmpctl.parseformat.parse_buffer);
		}
		parse->lastformat = parsetime->parse_format;
	}

	/*
	 * now, any changes ?
	 */
	if (parse->time.parse_state != parsetime->parse_state)
	{
		char tmp1[200];
		char tmp2[200];
		/*
		 * something happend
		 */
	
		(void) parsestate(parsetime->parse_state, tmp1);
		(void) parsestate(parse->time.parse_state, tmp2);
	
		NLOG(NLOG_CLOCKINFO) /* conditional if clause for conditional syslog */
			msyslog(LOG_INFO,"PARSE receiver #%d: STATE CHANGE: %s -> %s",
				CLK_UNIT(parse->peer), tmp2, tmp1);
	}

	/*
	 * remember for future
	 */
	parse->time = *parsetime;

	/*
	 * check to see, whether the clock did a complete powerup or lost PZF signal
	 * and post correct events for current condition
	 */
	if (PARSE_POWERUP(parsetime->parse_state))
	{
		/*
		 * this is bad, as we have completely lost synchronisation
		 * well this is a problem with the receiver here
		 * for PARSE Meinberg DCF77 receivers the lost synchronisation
		 * is true as it is the powerup state and the time is taken
		 * from a crude real time clock chip
		 * for the PZF series this is only partly true, as
		 * PARSE_POWERUP only means that the pseudo random
		 * phase shift sequence cannot be found. this is only
		 * bad, if we have never seen the clock in the SYNC
		 * state, where the PHASE and EPOCH are correct.
		 * for reporting events the above business does not
		 * really matter, but we can use the time code
		 * even in the POWERUP state after having seen
		 * the clock in the synchronized state (PZF class
		 * receivers) unless we have had a telegram disruption
		 * after having seen the clock in the SYNC state. we
		 * thus require having seen the clock in SYNC state
		 * *after* having missed telegrams (noresponse) from
		 * the clock. one problem remains: we might use erroneously
		 * POWERUP data if the disruption is shorter than 1 polling
		 * interval. fortunately powerdowns last usually longer than 64
		 * seconds and the receiver is at least 2 minutes in the
		 * POWERUP or NOSYNC state before switching to SYNC
		 */
		parse_event(parse, CEVNT_FAULT);
		NLOG(NLOG_CLOCKSTATUS)
			ERR(ERR_BADSTATUS)
			msyslog(LOG_ERR,"PARSE receiver #%d: NOT SYNCHRONIZED",
				CLK_UNIT(parse->peer));
	}
	else
	{
		/*
		 * we have two states left
		 *
		 * SYNC:
		 *  this state means that the EPOCH (timecode) and PHASE
		 *  information has be read correctly (at least two
		 *  successive PARSE timecodes were received correctly)
		 *  this is the best possible state - full trust
		 *
		 * NOSYNC:
		 *  The clock should be on phase with respect to the second
		 *  signal, but the timecode has not been received correctly within
		 *  at least the last two minutes. this is a sort of half baked state
		 *  for PARSE Meinberg DCF77 clocks this is bad news (clock running
		 *  without timecode confirmation)
		 *  PZF 535 has also no time confirmation, but the phase should be
		 *  very precise as the PZF signal can be decoded
		 */

		if (PARSE_SYNC(parsetime->parse_state))
		{
			/*
			 * currently completely synchronized - best possible state
			 */
			parse->lastsync = current_time;
			clear_err(parse, ERR_BADSTATUS);
		}
		else
		{
			/*
			 * we have had some problems receiving the time code
			 */
			parse_event(parse, CEVNT_PROP);
			NLOG(NLOG_CLOCKSTATUS)
				ERR(ERR_BADSTATUS)
				msyslog(LOG_ERR,"PARSE receiver #%d: TIMECODE NOT CONFIRMED",
					CLK_UNIT(parse->peer));
		}
	}

	fudge = parse->generic->fudgetime1; /* standard RS232 Fudgefactor */
	
	if (PARSE_TIMECODE(parsetime->parse_state))
	{
		rectime = parsetime->parse_stime.fp;
		off = reftime = parsetime->parse_time.fp;
	
		L_SUB(&off, &rectime); /* prepare for PPS adjustments logic */

#ifdef DEBUG
		if (debug > 3)
			printf("PARSE receiver #%d: Reftime %s, Recvtime %s - initial offset %s\n",
			       CLK_UNIT(parse->peer),
			       prettydate(&reftime),
			       prettydate(&rectime),
			       lfptoa(&off,6));
#endif
	}

	if (PARSE_PPS(parsetime->parse_state) && CLK_PPS(parse->peer))
	{
		l_fp offset;

		/*
		 * we have a PPS signal - much better than the RS232 stuff (we hope)
		 */
		offset = parsetime->parse_ptime.fp;

#ifdef DEBUG
		if (debug > 3)
			printf("PARSE receiver #%d: PPStime %s\n",
				CLK_UNIT(parse->peer),
				prettydate(&offset));
#endif
		if (PARSE_TIMECODE(parsetime->parse_state))
		{
			if (M_ISGEQ(off.l_i, off.l_f, -1, 0x80000000) &&
			    M_ISGEQ(0, 0x7fffffff, off.l_i, off.l_f))
			{
				fudge = parse->generic->fudgetime2; /* pick PPS fudge factor */
			
				/*
				 * RS232 offsets within [-0.5..0.5[ - take PPS offsets
				 */

				if (parse->parse_type->cl_flags & PARSE_F_PPSONSECOND)
				{
					reftime = off = offset;
					if (reftime.l_uf & (unsigned)0x80000000)
						reftime.l_ui++;
					reftime.l_uf = 0;

					
					/*
					 * implied on second offset
					 */
					off.l_uf = ~off.l_uf; /* map [0.5..1[ -> [-0.5..0[ */
					off.l_ui = (off.l_f < 0) ? ~0 : 0; /* sign extend */
				}
				else
				{
					/*
					 * time code describes pulse
					 */
					reftime = off = parsetime->parse_time.fp;

					L_SUB(&off, &offset); /* true offset */
				}
			}
			/*
			 * take RS232 offset when PPS when out of bounds
			 */
		}
		else
		{
			fudge = parse->generic->fudgetime2; /* pick PPS fudge factor */
			/*
			 * Well, no time code to guide us - assume on second pulse
			 * and pray, that we are within [-0.5..0.5[
			 */
			off = offset;
			reftime = offset;
			if (reftime.l_uf & (unsigned)0x80000000)
				reftime.l_ui++;
			reftime.l_uf = 0;
			/*
			 * implied on second offset
			 */
			off.l_uf = ~off.l_uf; /* map [0.5..1[ -> [-0.5..0[ */
			off.l_ui = (off.l_f < 0) ? ~0 : 0; /* sign extend */
		}
	}
	else
	{
		if (!PARSE_TIMECODE(parsetime->parse_state))
		{
			/*
			 * Well, no PPS, no TIMECODE, no more work ...
			 */
			if ((parsetime->parse_status & CVT_ADDITIONAL) &&
			    parse->parse_type->cl_message)
				parse->parse_type->cl_message(parse, parsetime);
			return;
		}
	}

#ifdef DEBUG
	if (debug > 3)
		printf("PARSE receiver #%d: Reftime %s, Recvtime %s - final offset %s\n",
			CLK_UNIT(parse->peer),
			prettydate(&reftime),
			prettydate(&rectime),
			lfptoa(&off,6));
#endif


	rectime = reftime;
	L_SUB(&rectime, &off);	/* just to keep the ntp interface happy */
	
#ifdef DEBUG
	if (debug > 3)
		printf("PARSE receiver #%d: calculated Reftime %s, Recvtime %s\n",
			CLK_UNIT(parse->peer),
			prettydate(&reftime),
			prettydate(&rectime));
#endif

	if ((parsetime->parse_status & CVT_ADDITIONAL) &&
	    parse->parse_type->cl_message)
		parse->parse_type->cl_message(parse, parsetime);

	if (PARSE_SYNC(parsetime->parse_state))
	{
		/*
		 * log OK status
		 */
		parse_event(parse, CEVNT_NOMINAL);
	}

	clear_err(parse, ERR_BADIO);
	clear_err(parse, ERR_BADDATA);
	clear_err(parse, ERR_NODATA);
	clear_err(parse, ERR_INTERNAL);
  
#ifdef DEBUG
	if (debug > 2) 
		{
			printf("PARSE receiver #%d: refclock_process_offset(reftime=%s, rectime=%s, Fudge=%f)\n",
				CLK_UNIT(parse->peer),
				prettydate(&reftime),
				prettydate(&rectime),
				fudge);
		}
#endif

	refclock_process_offset(parse->generic, reftime, rectime, fudge);
	if (PARSE_PPS(parsetime->parse_state) && CLK_PPS(parse->peer))
	{
		(void) pps_sample(&parse->time.parse_ptime.fp);
	}
	

	/*
	 * and now stick it into the clock machine
	 * samples are only valid iff lastsync is not too old and
	 * we have seen the clock in sync at least once
	 * after the last time we didn't see an expected data telegram
	 * see the clock states section above for more reasoning
	 */
	if (((current_time - parse->lastsync) > parse->parse_type->cl_maxunsync) ||
	    (parse->lastsync <= parse->lastmissed))
	{
		parse->generic->leap = LEAP_NOTINSYNC;
	}
	else
	{
		if (PARSE_LEAPADD(parsetime->parse_state))
		{
			/*
			 * we pick this state also for time code that pass leap warnings
			 * without direction information (as earth is currently slowing
			 * down).
			 */
			parse->generic->leap = (parse->flags & PARSE_LEAP_DELETE) ? LEAP_DELSECOND : LEAP_ADDSECOND;
		}
		else
		    if (PARSE_LEAPDEL(parsetime->parse_state))
		    {
			    parse->generic->leap = LEAP_DELSECOND;
		    }
		    else
		    {
			    parse->generic->leap = LEAP_NOWARNING;
		    }
	}
  
	/*
	 * ready, unless the machine wants a sample
	 */
	if (!parse->pollneeddata)
	    return;

	parse->pollneeddata = 0;

	refclock_receive(parse->peer);
}

/**===========================================================================
 ** special code for special clocks
 **/

static void
mk_utcinfo(
	   char *t,
	   int wnt,
	   int wnlsf,
	   int dn,
	   int dtls,
	   int dtlsf
	   )
{
  l_fp leapdate;
  
  sprintf(t, "current correction %d sec", dtls);
  t += strlen(t);
  
  if (wnlsf < 990)
    wnlsf += 1024;
  
  if (wnt < 990)
    wnt += 1024;
  
  gpstolfp((unsigned short)wnlsf, (unsigned short)dn, 0, &leapdate);
  
  if ((dtlsf != dtls) &&
      ((wnlsf - wnt) < 52))
    {
      sprintf(t, ", next correction %d sec on %s, new GPS-UTC offset %d",
	      dtlsf - dtls, gmprettydate(&leapdate), dtlsf);
    }
  else
    {
      sprintf(t, ", last correction on %s",
	      gmprettydate(&leapdate));
    }
}

#ifdef CLOCK_MEINBERG
/**===========================================================================
 ** Meinberg GPS166/GPS167 support
 **/

/*------------------------------------------------------------
 * gps16x_message - process GPS16x messages
 */
static void
gps16x_message(
	       struct parseunit *parse,
	       parsetime_t      *parsetime
	       )
{
	if (parse->time.parse_msglen && parsetime->parse_msg[0] == SOH)
	{
		GPS_MSG_HDR header;
		unsigned char *bufp = (unsigned char *)parsetime->parse_msg + 1;
		
#ifdef DEBUG
		if (debug > 2)
		{
			char msgbuffer[600];
			
			mkreadable(msgbuffer, sizeof(msgbuffer), (char *)parsetime->parse_msg, parsetime->parse_msglen, 1);
			printf("PARSE receiver #%d: received message (%d bytes) >%s<\n",
				CLK_UNIT(parse->peer),
				parsetime->parse_msglen,
				msgbuffer);
		}
#endif
		get_mbg_header(&bufp, &header);
		if (header.gps_hdr_csum == mbg_csum(parsetime->parse_msg + 1, 6) &&
		    (header.gps_len == 0 ||
		     (header.gps_len < sizeof(parsetime->parse_msg) &&
		      header.gps_data_csum == mbg_csum(bufp, header.gps_len))))
		{
			/*
			 * clean message
			 */
			switch (header.gps_cmd)
			{
			case GPS_SW_REV:
				{
					char buffer[64];
					SW_REV gps_sw_rev;
					
					get_mbg_sw_rev(&bufp, &gps_sw_rev);
					sprintf(buffer, "meinberg_gps_version=\"%x.%02x%s%s\"",
						(gps_sw_rev.code >> 8) & 0xFF,
						gps_sw_rev.code & 0xFF,
						gps_sw_rev.name[0] ? " " : "",
						gps_sw_rev.name);
					set_var(&parse->kv, buffer, 64, RO|DEF);
				}
			break;

			case GPS_STAT:
				{
					static struct state
					{
						unsigned short flag; /* status flag */
						unsigned const char *string; /* bit name */
					} states[] =
					  {
						  { TM_ANT_DISCONN, (const unsigned char *)"ANTENNA FAULTY" },
						  { TM_SYN_FLAG,    (const unsigned char *)"NO SYNC SIGNAL" },
						  { TM_NO_SYNC,     (const unsigned char *)"NO SYNC POWERUP" },
						  { TM_NO_POS,      (const unsigned char *)"NO POSITION" },
						  { 0, (const unsigned char *)"" }
					  };
					unsigned short status;
					struct state *s = states;
					char buffer[512];
					char *p, *b;
					
					status = get_lsb_short(&bufp);
					sprintf(buffer, "meinberg_gps_status=\"[0x%04x] ", status);
					
					if (status)
					{
						p = b = buffer + strlen(buffer);
						while (s->flag)
						{
							if (status & s->flag)
							{
								if (p != b)
								{
									*p++ = ',';
									*p++ = ' ';
								}
								
								strcat(p, (const char *)s->string);
							}
							s++;
						}
		
						*p++ = '"';
						*p   = '\0';
					}
					else
					{
						strcat(buffer, "<OK>\"");
					}
		
					set_var(&parse->kv, buffer, 64, RO|DEF);
				}
			break;

			case GPS_POS_XYZ:
				{
					XYZ xyz;
					char buffer[256];
					
					get_mbg_xyz(&bufp, xyz);
					sprintf(buffer, "gps_position(XYZ)=\"%s m, %s m, %s m\"",
						mfptoa(xyz[XP].l_ui, xyz[XP].l_uf, 1),
						mfptoa(xyz[YP].l_ui, xyz[YP].l_uf, 1),
						mfptoa(xyz[ZP].l_ui, xyz[ZP].l_uf, 1));
					
					set_var(&parse->kv, buffer, sizeof(buffer), RO|DEF);
				}
			break;
	      
			case GPS_POS_LLA:
				{
					LLA lla;
					char buffer[256];
					
					get_mbg_lla(&bufp, lla);
					
					sprintf(buffer, "gps_position(LLA)=\"%s deg, %s deg, %s m\"",
						mfptoa(lla[LAT].l_ui, lla[LAT].l_uf, 4),
						mfptoa(lla[LON].l_ui, lla[LON].l_uf, 4), 
						mfptoa(lla[ALT].l_ui, lla[ALT].l_uf, 1));
					
					set_var(&parse->kv, buffer, sizeof(buffer), RO|DEF);
				}
			break;
	      
			case GPS_TZDL:
				break;
	      
			case GPS_PORT_PARM:
				break;
	      
			case GPS_SYNTH:
				break;
				
			case GPS_ANT_INFO:
				{
					ANT_INFO antinfo;
					u_char buffer[512];
					u_char *p;
					
					get_mbg_antinfo(&bufp, &antinfo);
					sprintf(buffer, "meinberg_antenna_status=\"");
					p = buffer + strlen(buffer);
					
					switch (antinfo.status)
					{
					case ANT_INVALID:
						strcat(p, "<OK>");
						p += strlen(p);
						break;
						
					case ANT_DISCONN:
						strcat(p, "DISCONNECTED since ");
						NLOG(NLOG_CLOCKSTATUS)
							ERR(ERR_BADSTATUS)
							msyslog(LOG_ERR,"PARSE receiver #%d: ANTENNA FAILURE: %s",
								CLK_UNIT(parse->peer), p);
						
						p += strlen(p);
						mbg_tm_str(&p, &antinfo.tm_disconn);
						*p = '\0';
						break;
		    
					case ANT_RECONN:
						strcat(p, "RECONNECTED on ");
						p += strlen(p);
						mbg_tm_str(&p, &antinfo.tm_reconn);
						sprintf(p, ", reconnect clockoffset %c%ld.%07ld s, disconnect time ",
							(antinfo.delta_t < 0) ? '-' : '+',
							ABS(antinfo.delta_t) / 10000,
							ABS(antinfo.delta_t) % 10000);
						p += strlen(p);
						mbg_tm_str(&p, &antinfo.tm_disconn);
						*p = '\0';
						break;
		    
					default:
						sprintf(p, "bad status 0x%04x", antinfo.status);
						p += strlen(p);
						break;
					}
					
					*p++ = '"';
					*p   = '\0';
					
					set_var(&parse->kv, buffer, sizeof(buffer), RO|DEF);
				}
			break;
	      
			case GPS_UCAP:
				break;
				
			case GPS_CFGH:
				{
					CFGH cfgh;
					u_char buffer[512];
					u_char *p;
					
					get_mbg_cfgh(&bufp, &cfgh);
					if (cfgh.valid)
					{
						int i;
						
						p = buffer;
						strcpy(buffer, "gps_tot_51=\"");
						p += strlen(p);
						mbg_tgps_str(&p, &cfgh.tot_51);
						*p++ = '"';
						*p   = '\0';
						set_var(&parse->kv, buffer, sizeof(buffer), RO);
						
						p = buffer;
						strcpy(buffer, "gps_tot_63=\"");
						p += strlen(p);
						mbg_tgps_str(&p, &cfgh.tot_63);
						*p++ = '"';
						*p   = '\0';
						set_var(&parse->kv, buffer, sizeof(buffer), RO);
						
						p = buffer;
						strcpy(buffer, "gps_t0a=\"");
						p += strlen(p);
						mbg_tgps_str(&p, &cfgh.t0a);
						*p++ = '"';
						*p   = '\0';
						set_var(&parse->kv, buffer, sizeof(buffer), RO);
						
						for (i = MIN_SVNO; i <= MAX_SVNO; i++)
						{
							p = buffer;
							sprintf(p, "gps_cfg[%d]=\"[0x%x] ", i, cfgh.cfg[i]);
							p += strlen(p);
							switch (cfgh.cfg[i] & 0x7)
							{
							case 0:
								strcpy(p, "BLOCK I");
								break;
							case 1:
								strcpy(p, "BLOCK II");
								break;
							default:
								sprintf(p, "bad CFG");
								break;
							}
							strcat(p, "\"");
							set_var(&parse->kv, buffer, sizeof(buffer), RO);
							
							p = buffer;
							sprintf(p, "gps_health[%d]=\"[0x%x] ", i, cfgh.health[i]);
							p += strlen(p);
							switch ((cfgh.health[i] >> 5) & 0x7 )
							{
							case 0:
								strcpy(p, "OK;");
								break;
							case 1:
								strcpy(p, "PARITY;");
								break;
							case 2:
								strcpy(p, "TLM/HOW;");
								break;
							case 3:
								strcpy(p, "Z-COUNT;");
								break;
							case 4:
								strcpy(p, "SUBFRAME 1,2,3;");
								break;
							case 5:
								strcpy(p, "SUBFRAME 4,5;");
								break;
							case 6:
								strcpy(p, "UPLOAD BAD;");
								break;
							case 7:
								strcpy(p, "DATA BAD;");
								break;
							}
							
							p += strlen(p);
							
							switch (cfgh.health[i] & 0x1F)
							{
							case 0:
								strcpy(p, "SIGNAL OK");
								break;
							case 0x1C:
								strcpy(p, "SV TEMP OUT");
								break;
							case 0x1D:
								strcpy(p, "SV WILL BE TEMP OUT");
								break;
							case 0x1E:
								break;
							case 0x1F:
								strcpy(p, "MULTIPLE ERRS");
								break;
							default:
								strcpy(p, "TRANSMISSION PROBLEMS");
								break;
							}
							
							strcat(p, "\"");
							set_var(&parse->kv, buffer, sizeof(buffer), RO);
						}
					}
				}
			break;
	      
			case GPS_ALM:
				break;
				
			case GPS_EPH:
				break;
				
			case GPS_UTC:
				{
					UTC utc;
					char buffer[512];
					char *p;
					
					p = buffer;
					
					get_mbg_utc(&bufp, &utc);
					
					if (utc.valid)
					{
						strcpy(p, "gps_utc_correction=\"");
						p += strlen(p);
						mk_utcinfo(p, utc.t0t.wn, utc.WNlsf, utc.DNt, utc.delta_tls, utc.delta_tlsf);
						strcat(p, "\"");
					}
					else
					{
						strcpy(p, "gps_utc_correction=\"<NO UTC DATA>\"");
					}
					set_var(&parse->kv, buffer, sizeof(buffer), RO|DEF);
				}
			break;
			
			case GPS_IONO:
				break;
				
			case GPS_ASCII_MSG:
				{
					ASCII_MSG gps_ascii_msg;
					char buffer[128];
		
					get_mbg_ascii_msg(&bufp, &gps_ascii_msg);
					
					if (gps_ascii_msg.valid)
						{
							char buffer1[128];
							mkreadable(buffer1, sizeof(buffer1), gps_ascii_msg.s, strlen(gps_ascii_msg.s), (int)0);
							
							sprintf(buffer, "gps_message=\"%s\"", buffer1);
						}
					else
						strcpy(buffer, "gps_message=<NONE>");
					
					set_var(&parse->kv, buffer, 128, RO|DEF);
				}
			
			break;
	      
			default:
				break;
			}
		}
		else
		{
			msyslog(LOG_DEBUG, "PARSE receiver #%d: gps16x_message: message checksum error: hdr_csum = 0x%x (expected 0x%lx), data_len = %d, data_csum = 0x%x (expected 0x%lx)",
				CLK_UNIT(parse->peer),
				header.gps_hdr_csum, mbg_csum(parsetime->parse_msg + 1, 6),
				header.gps_len,
				header.gps_data_csum, mbg_csum(bufp, (unsigned)((header.gps_len < sizeof(parsetime->parse_msg)) ? header.gps_len : 0)));
		}
	}
  
	return;
}

/*------------------------------------------------------------
 * gps16x_poll - query the reciver peridically
 */
static void
gps16x_poll(
	    struct peer *peer
	    )
{
	struct parseunit *parse = (struct parseunit *)peer->procptr->unitptr;
	
	static GPS_MSG_HDR sequence[] = 
	{
		{ GPS_SW_REV,          0, 0, 0 },
		{ GPS_STAT,            0, 0, 0 },
		{ GPS_UTC,             0, 0, 0 },
		{ GPS_ASCII_MSG,       0, 0, 0 },
		{ GPS_ANT_INFO,        0, 0, 0 },
		{ GPS_CFGH,            0, 0, 0 },
		{ GPS_POS_XYZ,         0, 0, 0 },
		{ GPS_POS_LLA,         0, 0, 0 },
		{ (unsigned short)~0,  0, 0, 0 }
	};
      
	int rtc;
	unsigned char cmd_buffer[64];
	unsigned char *outp = cmd_buffer;
	GPS_MSG_HDR *header;
	
	if (((poll_info_t *)parse->parse_type->cl_data)->rate)
	{
		parse->peer->nextaction = current_time + ((poll_info_t *)parse->parse_type->cl_data)->rate;
	}

	if (sequence[parse->localstate].gps_cmd == (unsigned short)~0)
		parse->localstate = 0;
	
	header = sequence + parse->localstate++;
	
	*outp++ = SOH;		/* start command */
	
	put_mbg_header(&outp, header);
	outp = cmd_buffer + 1;
	
	header->gps_hdr_csum = (short)mbg_csum(outp, 6);
	put_mbg_header(&outp, header);
	
#ifdef DEBUG
	if (debug > 2)
	{
		char buffer[128];
		
		mkreadable(buffer, sizeof(buffer), (char *)cmd_buffer, (unsigned)(outp - cmd_buffer), 1);
		printf("PARSE receiver #%d: transmitted message #%ld (%d bytes) >%s<\n",
		       CLK_UNIT(parse->peer),
		       parse->localstate - 1,
		       (int)(outp - cmd_buffer),
		       buffer); 
	}
#endif
  
	rtc = write(parse->generic->io.fd, cmd_buffer, (unsigned long)(outp - cmd_buffer));
	
	if (rtc < 0)
	{
		ERR(ERR_BADIO)
			msyslog(LOG_ERR, "PARSE receiver #%d: gps16x_poll: failed to send cmd to clock: %m", CLK_UNIT(parse->peer));
	}
	else
	if (rtc != outp - cmd_buffer)
	{
		ERR(ERR_BADIO)
			msyslog(LOG_ERR, "PARSE receiver #%d: gps16x_poll: failed to send cmd incomplete (%d of %d bytes sent)", CLK_UNIT(parse->peer), rtc, (int)(outp - cmd_buffer));
	}

	clear_err(parse, ERR_BADIO);
	return;
}

/*--------------------------------------------------
 * init routine - setup timer
 */
static int
gps16x_poll_init(
	struct parseunit *parse
	)
{
	if (((poll_info_t *)parse->parse_type->cl_data)->rate)
	{
		parse->peer->action = gps16x_poll;
		gps16x_poll(parse->peer);
	}

	return 0;
}

#else
static void
gps16x_message(
	       struct parseunit *parse,
	       parsetime_t      *parsetime
	       )
{}
static int
gps16x_poll_init(
	struct parseunit *parse
	)
{
	return 1;
}
#endif /* CLOCK_MEINBERG */

/**===========================================================================
 ** clock polling support
 **/

/*--------------------------------------------------
 * direct poll routine
 */
static void
poll_dpoll(
	struct parseunit *parse
	)
{
	int rtc;
	const char *ps = ((poll_info_t *)parse->parse_type->cl_data)->string;
	int   ct = ((poll_info_t *)parse->parse_type->cl_data)->count;

	rtc = write(parse->generic->io.fd, ps, (unsigned long)ct);
	if (rtc < 0)
	{
		ERR(ERR_BADIO)
			msyslog(LOG_ERR, "PARSE receiver #%d: poll_dpoll: failed to send cmd to clock: %m", CLK_UNIT(parse->peer));
	}
	else
	    if (rtc != ct)
	    {
		    ERR(ERR_BADIO)
			    msyslog(LOG_ERR, "PARSE receiver #%d: poll_dpoll: failed to send cmd incomplete (%d of %d bytes sent)", CLK_UNIT(parse->peer), rtc, ct);
	    }
	clear_err(parse, ERR_BADIO);
}

/*--------------------------------------------------
 * periodic poll routine
 */
static void
poll_poll(
	struct peer *peer
	)
{
	struct parseunit *parse = (struct parseunit *)peer->procptr->unitptr;
	
	if (parse->parse_type->cl_poll)
		parse->parse_type->cl_poll(parse);

	if (((poll_info_t *)parse->parse_type->cl_data)->rate)
	{
		parse->peer->nextaction = current_time + ((poll_info_t *)parse->parse_type->cl_data)->rate;
	}
}

/*--------------------------------------------------
 * init routine - setup timer
 */
static int
poll_init(
	struct parseunit *parse
	)
{
	if (((poll_info_t *)parse->parse_type->cl_data)->rate)
	{
		parse->peer->action = poll_poll;
		poll_poll(parse->peer);
	}

	return 0;
}

/**===========================================================================
 ** Trimble support
 **/

/*-------------------------------------------------------------
 * trimble TAIP init routine - setup EOL and then do poll_init.
 */
static int
trimbletaip_init(
	struct parseunit *parse
	)
{
#ifdef HAVE_TERMIOS
	struct termios tio;
#endif
#ifdef HAVE_SYSV_TTYS
	struct termio tio;
#endif
	/*
	 * configure terminal line for trimble receiver
	 */
	if (TTY_GETATTR(parse->generic->io.fd, &tio) == -1)
	{
		msyslog(LOG_ERR, "PARSE receiver #%d: trimbletaip_init: tcgetattr(fd, &tio): %m", CLK_UNIT(parse->peer));
		return 0;
	}
	else
	{
		tio.c_cc[VEOL] = TRIMBLETAIP_EOL;
	
		if (TTY_SETATTR(parse->generic->io.fd, &tio) == -1)
		{
			msyslog(LOG_ERR, "PARSE receiver #%d: trimbletaip_init: tcsetattr(fd, &tio): %m", CLK_UNIT(parse->peer));
			return 0;
		}
	}
	return poll_init(parse);
}

/*--------------------------------------------------
 * trimble TAIP event routine - reset receiver upon data format trouble
 */
static const char *taipinit[] = {
	">FPV00000000<",
	">SRM;ID_FLAG=F;CS_FLAG=T;EC_FLAG=F;FR_FLAG=T;CR_FLAG=F<",
	">FTM00020001<",
	(char *)0
};
      
static void
trimbletaip_event(
	struct parseunit *parse,
	int event
	)
{
	switch (event)
	{
	    case CEVNT_BADREPLY:	/* reset on garbled input */
	    case CEVNT_TIMEOUT:		/* reset on no input */
		    {
			    const char **iv;

			    iv = taipinit;
			    while (*iv)
			    {
				    int rtc = write(parse->generic->io.fd, *iv, strlen(*iv));
				    if (rtc < 0)
				    {
					    msyslog(LOG_ERR, "PARSE receiver #%d: trimbletaip_event: failed to send cmd to clock: %m", CLK_UNIT(parse->peer));
					    return;
				    }
				    else
				    {
					    if (rtc != strlen(*iv))
					    {
						    msyslog(LOG_ERR, "PARSE receiver #%d: trimbletaip_event: failed to send cmd incomplete (%d of %d bytes sent)",
							    CLK_UNIT(parse->peer), rtc, (int)strlen(*iv));
						    return;
					    }
				    }
				    iv++;
			    }

			    NLOG(NLOG_CLOCKINFO)
				    ERR(ERR_BADIO)
				    msyslog(LOG_ERR, "PARSE receiver #%d: trimbletaip_event: RECEIVER INITIALIZED",
					    CLK_UNIT(parse->peer));
		    }
		    break;

	    default:			/* ignore */
		break;
	}
}

/*
 * This driver supports the Trimble SVee Six Plus GPS receiver module.
 * It should support other Trimble receivers which use the Trimble Standard
 * Interface Protocol (see below).
 *
 * The module has a serial I/O port for command/data and a 1 pulse-per-second
 * output, about 1 microsecond wide. The leading edge of the pulse is
 * coincident with the change of the GPS second. This is the same as
 * the change of the UTC second +/- ~1 microsecond. Some other clocks
 * specifically use a feature in the data message as a timing reference, but
 * the SVee Six Plus does not do this. In fact there is considerable jitter
 * on the timing of the messages, so this driver only supports the use
 * of the PPS pulse for accurate timing. Where it is determined that
 * the offset is way off, when first starting up ntpd for example,
 * the timing of the data stream is used until the offset becomes low enough
 * (|offset| < CLOCK_MAX), at which point the pps offset is used.
 *
 * It can use either option for receiving PPS information - the 'ppsclock'
 * stream pushed onto the serial data interface to timestamp the Carrier
 * Detect interrupts, where the 1PPS connects to the CD line. This only
 * works on SunOS 4.1.x currently. To select this, define PPSPPS in
 * Config.local. The other option is to use a pulse-stretcher/level-converter
 * to convert the PPS pulse into a RS232 start pulse & feed this into another
 * tty port. To use this option, define PPSCLK in Config.local. The pps input,
 * by whichever method, is handled in ntp_loopfilter.c
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

#define PI		3.1415926535898	/* lots of sig figs */
#define D2R		PI/180.0

/*-------------------------------------------------------------------
 * sendcmd, sendbyte, sendetx, sendflt, sendint implement the command
 * interface to the receiver.
 *
 * CAVEAT: the sendflt, sendint routines are byte order dependend and
 * float implementation dependend - these must be converted to portable
 * versions !
 *
 * CURRENT LIMITATION: float implementation. This runs only on systems
 * with IEEE754 floats as native floats
 */

typedef struct trimble
{
	u_long last_msg;	/* last message received */
	u_char qtracking;	/* query tracking status */
	u_long ctrack;		/* current tracking set */
	u_long ltrack;		/* last tracking set */
} trimble_t;

union uval {
	u_char  bd[8];
	int     iv;
	float   fv;
	double  dv;
};
  
struct txbuf
{
	short idx;			/* index to first unused byte */
	u_char *txt;			/* pointer to actual data buffer */
};

void	sendcmd		P((struct txbuf *buf, int c)); 
void	sendbyte	P((struct txbuf *buf, int b)); 
void	sendetx		P((struct txbuf *buf, struct parseunit *parse)); 
void	sendint		P((struct txbuf *buf, int a)); 
void	sendflt		P((struct txbuf *buf, double a)); 
 
void
sendcmd(
	struct txbuf *buf,
	int c
	)
{
	buf->txt[0] = DLE;
	buf->txt[1] = (u_char)c;
	buf->idx = 2;
}

void
sendbyte(
	struct txbuf *buf,
	int b
	)
{
	if (b == DLE)
	    buf->txt[buf->idx++] = DLE;
	buf->txt[buf->idx++] = (u_char)b;
}

void
sendetx(
	struct txbuf *buf,
	struct parseunit *parse
	)
{
	buf->txt[buf->idx++] = DLE;
	buf->txt[buf->idx++] = ETX;

	if (write(parse->generic->io.fd, buf->txt, (unsigned long)buf->idx) != buf->idx)
	{
		ERR(ERR_BADIO)
			msyslog(LOG_ERR, "PARSE receiver #%d: sendetx: failed to send cmd to clock: %m", CLK_UNIT(parse->peer));
	}
	else
	{
#ifdef DEBUG
	  if (debug > 2)
	  {
		  char buffer[256];
		  
		  mkreadable(buffer, sizeof(buffer), (char *)buf->txt, (unsigned)buf->idx, 1);
		  printf("PARSE receiver #%d: transmitted message (%d bytes) >%s<\n",
			 CLK_UNIT(parse->peer),
			 buf->idx, buffer); 
	  }
#endif
		clear_err(parse, ERR_BADIO);
	}
}

void  
sendint(
	struct txbuf *buf,
	int a
	)
{
	/* send 16bit int, msbyte first */
	sendbyte(buf, (u_char)((a>>8) & 0xff));
	sendbyte(buf, (u_char)(a & 0xff));
}

void
sendflt(
	struct txbuf *buf,
	double a
	)
{
	int i;
	union uval uval;

	uval.fv = a;
#ifdef WORDS_BIGENDIAN
	for (i=0; i<=3; i++)
#else
	    for (i=3; i>=0; i--)
#endif
		sendbyte(buf, uval.bd[i]);
}

#define TRIM_POS_OPT	0x13	/* output position with high precision */
#define TRIM_TIME_OPT	0x03	/* use UTC time stamps, on second */

/*--------------------------------------------------
 * trimble TSIP setup routine
 */
static int
trimbletsip_setup(
		  struct parseunit *parse,
		  const char *reason
		  )
{
	u_char buffer[256];
	struct txbuf buf;

	buf.txt = buffer;
  
	sendcmd(&buf, CMD_CVERSION);	/* request software versions */
	sendetx(&buf, parse);
	
	sendcmd(&buf, CMD_COPERPARAM);	/* set operating parameters */
	sendbyte(&buf, 4);	/* static */
	sendflt(&buf, 5.0*D2R);	/* elevation angle mask = 10 deg XXX */
	sendflt(&buf, 4.0);	/* s/n ratio mask = 6 XXX */
	sendflt(&buf, 12.0);	/* PDOP mask = 12 */
	sendflt(&buf, 8.0);	/* PDOP switch level = 8 */
	sendetx(&buf, parse);
	
	sendcmd(&buf, CMD_CMODESEL);	/* fix mode select */
	sendbyte(&buf, 0);	/* automatic */
	sendetx(&buf, parse);
	
	sendcmd(&buf, CMD_CMESSAGE);	/* request system message */
	sendetx(&buf, parse);
	
	sendcmd(&buf, CMD_CSUPER);	/* superpacket fix */
	sendbyte(&buf, 0x2);	/* binary mode */
	sendetx(&buf, parse);
	
	sendcmd(&buf, CMD_CIOOPTIONS);	/* set I/O options */
	sendbyte(&buf, TRIM_POS_OPT);	/* position output */
	sendbyte(&buf, 0x00);	/* no velocity output */
	sendbyte(&buf, TRIM_TIME_OPT);	/* UTC, compute on seconds */
	sendbyte(&buf, 0x00);	/* no raw measurements */
	sendetx(&buf, parse);
	
	sendcmd(&buf, CMD_CUTCPARAM);	/* request UTC correction data */
	sendetx(&buf, parse);

	NLOG(NLOG_CLOCKINFO)
		ERR(ERR_BADIO)
		msyslog(LOG_ERR, "PARSE receiver #%d: trimbletsip_setup: RECEIVER RE-INITIALIZED (%s)", CLK_UNIT(parse->peer), reason);

	return 0;
}

/*--------------------------------------------------
 * TRIMBLE TSIP check routine
 */
static void
trimble_check(
	      struct peer *peer
	      )
{
	struct parseunit *parse = (struct parseunit *)peer->procptr->unitptr;
	trimble_t *t = parse->localdata;
	u_char buffer[256];
	struct txbuf buf;
	buf.txt = buffer;
	
	if (t)
	{
		if (current_time > t->last_msg + TRIMBLETSIP_IDLE_TIME)
			(void)trimbletsip_setup(parse, "message timeout");
	}
	poll_poll(parse->peer);	/* emit query string and re-arm timer */
	
	if (t->qtracking)
	{
		u_long oldsats = t->ltrack & ~t->ctrack;
		
		t->qtracking = 0;
		t->ltrack = t->ctrack;
		
		if (oldsats)
		{
			int i;
				
			for (i = 0; oldsats; i++)
				if (oldsats & (1 << i))
					{
						sendcmd(&buf, CMD_CSTATTRACK);
						sendbyte(&buf, i+1);	/* old sat */
						sendetx(&buf, parse);
					}
			oldsats &= ~(1 << i);
		}
						
		sendcmd(&buf, CMD_CSTATTRACK);
		sendbyte(&buf, 0x00);	/* current tracking set */
		sendetx(&buf, parse);
	}
}

/*--------------------------------------------------
 * TRIMBLE TSIP end routine
 */
static void
trimbletsip_end(
	      struct parseunit *parse
	      )
{	trimble_t *t = parse->localdata;
	
	if (t)
	{
		free(t);
		parse->localdata = (void *)0;
	}
	parse->peer->nextaction = 0;
	parse->peer->action = (void (*) P((struct peer *)))0;
}

/*--------------------------------------------------
 * TRIMBLE TSIP init routine
 */
static int
trimbletsip_init(
	struct parseunit *parse
	)
{
#if defined(VEOL) || defined(VEOL2)
#ifdef HAVE_TERMIOS
	struct termios tio;		/* NEEDED FOR A LONG TIME ! */
#endif
#ifdef HAVE_SYSV_TTYS
	struct termio tio;		/* NEEDED FOR A LONG TIME ! */
#endif
	/*
	 * allocate local data area
	 */
	if (!parse->localdata)
	{
		trimble_t *t;
		
		t = (trimble_t *)(parse->localdata = emalloc(sizeof(trimble_t)));
		
		if (t)
		{
			memset((char *)t, 0, sizeof(trimble_t));
			t->last_msg = current_time;
		}
	}

	parse->peer->action     = trimble_check;
	parse->peer->nextaction = current_time;

	/*
	 * configure terminal line for ICANON mode with VEOL characters
	 */
	if (TTY_GETATTR(parse->generic->io.fd, &tio) == -1)
	{
		msyslog(LOG_ERR, "PARSE receiver #%d: trimbletsip_init: tcgetattr(%d, &tio): %m", CLK_UNIT(parse->peer), parse->generic->io.fd);
		return 0;
	}
	else
	{
		if ((parse_clockinfo[CLK_TYPE(parse->peer)].cl_lflag & ICANON))
		{
#ifdef VEOL
			tio.c_cc[VEOL]  = ETX;
#endif
#ifdef VEOL2
			tio.c_cc[VEOL2]  = DLE;
#endif
		}

		if (TTY_SETATTR(parse->generic->io.fd, &tio) == -1)
		{
			msyslog(LOG_ERR, "PARSE receiver #%d: trimbletsip_init: tcsetattr(%d, &tio): %m", CLK_UNIT(parse->peer), parse->generic->io.fd);
			return 0;
		}
	}
#endif
	return trimbletsip_setup(parse, "initial startup");
}

/*------------------------------------------------------------
 * trimbletsip_event - handle Trimble events
 * simple evente handler - attempt to re-initialize receiver
 */
static void
trimbletsip_event(
	struct parseunit *parse,
	int event
	)
{
	switch (event)
	{
	    case CEVNT_BADREPLY:	/* reset on garbled input */
	    case CEVNT_TIMEOUT:		/* reset on no input */
		    (void)trimbletsip_setup(parse, "event BAD_REPLY/TIMEOUT");
		    break;

	    default:			/* ignore */
		break;
	}
}

/*
 * getflt, getint convert fields in the incoming data into the
 * appropriate type of item
 *
 * CAVEAT: these routines are currently definitely byte order dependent
 * and assume Representation(float) == IEEE754
 * These functions MUST be converted to portable versions (especially
 * converting the float representation into ntp_fp formats in order
 * to avoid floating point operations at all!
 */

static float
getflt(
	u_char *bp
	)
{
	union uval uval;
	
#ifdef WORDS_BIGENDIAN
	uval.bd[0] = *bp++;
	uval.bd[1] = *bp++;
	uval.bd[2] = *bp++;
	uval.bd[3] = *bp;
#else  /* ! WORDS_BIGENDIAN */
	uval.bd[3] = *bp++;
	uval.bd[2] = *bp++;
	uval.bd[1] = *bp++;
	uval.bd[0] = *bp;
#endif /* ! WORDS_BIGENDIAN */
	return uval.fv;
}

static double
getdbl(
	u_char *bp
	)
{
	union uval uval;
	
#ifdef WORDS_BIGENDIAN
	uval.bd[0] = *bp++;
	uval.bd[1] = *bp++;
	uval.bd[2] = *bp++;
	uval.bd[3] = *bp++;
	uval.bd[4] = *bp++;
	uval.bd[5] = *bp++;
	uval.bd[6] = *bp++;
	uval.bd[7] = *bp;
#else  /* ! WORDS_BIGENDIAN */
	uval.bd[7] = *bp++;
	uval.bd[6] = *bp++;
	uval.bd[5] = *bp++;
	uval.bd[4] = *bp++;
	uval.bd[3] = *bp++;
	uval.bd[2] = *bp++;
	uval.bd[1] = *bp++;
	uval.bd[0] = *bp;
#endif /* ! WORDS_BIGENDIAN */
	return uval.dv;
}

static int
getshort(
	 unsigned char *p
	 )
{
	return get_msb_short(&p);
}

/*--------------------------------------------------
 * trimbletsip_message - process trimble messages
 */
#define RTOD (180.0 / 3.1415926535898)
#define mb(_X_) (buffer[2+(_X_)]) /* shortcut for buffer access */

static void
trimbletsip_message(
		    struct parseunit *parse,
		    parsetime_t      *parsetime
		    )
{
	unsigned char *buffer = parsetime->parse_msg;
	unsigned int   size   = parsetime->parse_msglen;
	
	if ((size < 4) ||
	    (buffer[0]      != DLE) ||
	    (buffer[size-1] != ETX) ||
	    (buffer[size-2] != DLE))
	{
#ifdef DEBUG
		if (debug > 2) {
			int i;

			printf("TRIMBLE BAD packet, size %d:\n	", size);
			for (i = 0; i < size; i++) {
				printf ("%2.2x, ", buffer[i]&0xff);
				if (i%16 == 15) printf("\n\t");
			}
			printf("\n");
		}
#endif
		return;
	}
	else
	{
		int var_flag;
		trimble_t *tr = parse->localdata;
		unsigned int cmd = buffer[1];
		char pbuffer[200];
		char *t = pbuffer;
		cmd_info_t *s;
		
#ifdef DEBUG
		if (debug > 3) {
			int i;

			printf("TRIMBLE packet 0x%02x, size %d:\n	", cmd, size);
			for (i = 0; i < size; i++) {
				printf ("%2.2x, ", buffer[i]&0xff);
				if (i%16 == 15) printf("\n\t");
			}
			printf("\n");
		}
#endif

		if (tr)
			tr->last_msg = current_time;
		
		s = trimble_convert(cmd, trimble_rcmds);
		
		if (s)
		{
			sprintf(t, "%s=\"", s->varname);
		}
		else
		{
			printf("TRIMBLE unknown command 0x%02x\n", cmd);
			return;
		}

		var_flag = s->varmode;

		t += strlen(t);
		
		switch(cmd)
		{
		case CMD_RCURTIME:
			sprintf(t, "%f, %d, %f",
				getflt((unsigned char *)&mb(0)), getshort((unsigned char *)&mb(4)),
				getflt((unsigned char *)&mb(6)));
			break;
			
		case CMD_RBEST4:
			strcpy(t, "mode: ");
			t += strlen(t);
			switch (mb(0) & 0xF)
			{
			default:
				sprintf(t, "0x%x", mb(0) & 0x7);
				break;

			case 1:
				strcat(t, "0D");
				break;
				
			case 3:
				strcat(t, "2D");
				break;
				
			case 4:
				strcat(t, "3D");
				break;
			}
			t += strlen(t);
			if (mb(0) & 0x10)
				strcpy(t, "-MANUAL, ");
			else
				strcpy(t, "-AUTO, ");
			t += strlen(t);
			
			sprintf(t, "satellites %02d %02d %02d %02d, PDOP %.2f, HDOP %.2f, VDOP %.2f, TDOP %.2f",
				mb(1), mb(2), mb(3), mb(4),
				getflt((unsigned char *)&mb(5)),
				getflt((unsigned char *)&mb(9)),
				getflt((unsigned char *)&mb(13)),
				getflt((unsigned char *)&mb(17)));

			break;
			
		case CMD_RVERSION:
			sprintf(t, "%d.%d (%d/%d/%d)",
				mb(0)&0xff, mb(1)&0xff, 1900+(mb(4)&0xff), mb(2)&0xff, mb(3)&0xff);
			break;
			
		case CMD_RRECVHEALTH:
		{
			static const char *msgs[] =
			{
				"Battery backup failed",
				"Signal processor error",
				"Alignment error, channel or chip 1",
				"Alignment error, channel or chip 2",
				"Antenna feed line fault",
				"Excessive ref freq. error",
				"<BIT 6>",
				"<BIT 7>"
			};
			
			int i, bits;
			
			switch (mb(0) & 0xFF)
			{
			default:
				sprintf(t, "illegal value 0x%02x", mb(0) & 0xFF);
				break;
			case 0x00:
				strcpy(t, "doing position fixes");
				break;
			case 0x01:
				strcpy(t, "no GPS time yet");
				break;
			case 0x03:
				strcpy(t, "PDOP too high");
				break;
			case 0x08:
				strcpy(t, "no usable satellites");
				break;
			case 0x09:
				strcpy(t, "only ONE usable satellite");
				break;
			case 0x0A:
				strcpy(t, "only TWO usable satellites");
				break;
			case 0x0B:
				strcpy(t, "only THREE usable satellites");
				break;
			case 0x0C:
				strcpy(t, "the chosen satellite is unusable");
				break;
			}

			t += strlen(t);

			bits = mb(1) & 0xFF;
			
			for (i = 0; i < 8; i++)
				if (bits & (0x1<<i))
				{
					sprintf(t, ", %s", msgs[i]);
					t += strlen(t);
				}
		}
		break;
			
		case CMD_RMESSAGE:
			mkreadable(t, (int)(sizeof(pbuffer) - (t - pbuffer)), (char *)&mb(0), (unsigned)(size - 2 - (&mb(0) - buffer)), 0);
			break;
			
		case CMD_RMACHSTAT:
		{
			static const char *msgs[] =
			{
				"Synthesizer Fault",
				"Battery Powered Time Clock Fault",
				"A-to-D Converter Fault",
				"The almanac stored in the receiver is not complete and current",
				"<BIT 4>",
				"<BIT 5",
				"<BIT 6>",
				"<BIT 7>"
			};
			
			int i, bits;

			sprintf(t, "machine id 0x%02x", mb(0) & 0xFF);
			t += strlen(t);
			
			bits = mb(1) & 0xFF;
			
			for (i = 0; i < 8; i++)
				if (bits & (0x1<<i))
				{
					sprintf(t, ", %s", msgs[i]);
					t += strlen(t);
				}

			sprintf(t, ", Superpackets %ssupported", (mb(2) & 0xFF) ? "" :"un" );
		}
		break;
			
		case CMD_ROPERPARAM:
			sprintf(t, "%2x %.1f %.1f %.1f %.1f",
				mb(0), getflt((unsigned char *)&mb(1)), getflt((unsigned char *)&mb(5)),
				getflt((unsigned char *)&mb(9)), getflt((unsigned char *)&mb(13)));
			break;
			
		case CMD_RUTCPARAM:
		{
			float t0t = getflt((unsigned char *)&mb(14));
			short wnt = getshort((unsigned char *)&mb(18));
			short dtls = getshort((unsigned char *)&mb(12));
			short wnlsf = getshort((unsigned char *)&mb(20));
			short dn = getshort((unsigned char *)&mb(22));
			short dtlsf = getshort((unsigned char *)&mb(24));

			if ((int)t0t != 0)
			  {
			    mk_utcinfo(t, wnt, wnlsf, dn, dtls, dtlsf);
			  }
			else
			  {
			    strcpy(t, "<NO UTC DATA>");
			  }
		}
		break;

		case CMD_RSAT1BIAS:
			sprintf(t, "%.1fm %.2fm/s at %.1fs",
				getflt(&mb(0)), getflt(&mb(4)), getflt(&mb(8)));
			break;

		case CMD_RIOOPTIONS:
		{
			sprintf(t, "%02x %02x %02x %02x",
				mb(0), mb(1), mb(2), mb(3));
			if (mb(0) != TRIM_POS_OPT ||
			    mb(2) != TRIM_TIME_OPT)
			{
				(void)trimbletsip_setup(parse, "bad io options");
			}
		}
		break;
		
		case CMD_RSPOSXYZ:
		{
			double x = getflt((unsigned char *)&mb(0));
			double y = getflt((unsigned char *)&mb(4));
			double z = getflt((unsigned char *)&mb(8));
			double f = getflt((unsigned char *)&mb(12));
			
			if (f > 0.0)
			  sprintf(t, "x= %.1fm, y= %.1fm, z= %.1fm, time_of_fix= %f sec",
				  x, y, z,
				  f);
			else
			  return;
		}
		break;

		case CMD_RSLLAPOS:
		{
			double lat = getflt((unsigned char *)&mb(0));
			double lng = getflt((unsigned char *)&mb(4));
			double f   = getflt((unsigned char *)&mb(12));
			
			if (f > 0.0)
			  sprintf(t, "lat %f %c, long %f %c, alt %.2fm",
				  ((lat < 0.0) ? (-lat) : (lat))*RTOD, (lat < 0.0 ? 'S' : 'N'),
				  ((lng < 0.0) ? (-lng) : (lng))*RTOD, (lng < 0.0 ? 'W' : 'E'),
				  getflt((unsigned char *)&mb(8)));
			else
			  return;
		}
		break;

		case CMD_RDOUBLEXYZ:
		{
			double x = getdbl((unsigned char *)&mb(0));
			double y = getdbl((unsigned char *)&mb(8));
			double z = getdbl((unsigned char *)&mb(16));
			sprintf(t, "x= %.1fm, y= %.1fm, z= %.1fm",
				x, y, z);
		}
		break;
				
		case CMD_RDOUBLELLA:
		{
			double lat = getdbl((unsigned char *)&mb(0));
			double lng = getdbl((unsigned char *)&mb(8));
			sprintf(t, "lat %f %c, lon %f %c, alt %.2fm",
				((lat < 0.0) ? (-lat) : (lat))*RTOD, (lat < 0.0 ? 'S' : 'N'),
				((lng < 0.0) ? (-lng) : (lng))*RTOD, (lng < 0.0 ? 'W' : 'E'),
				getdbl((unsigned char *)&mb(16)));
		}
		break;

		case CMD_RALLINVIEW:
		{
			int i, sats;
			
			strcpy(t, "mode: ");
			t += strlen(t);
			switch (mb(0) & 0x7)
			{
			default:
				sprintf(t, "0x%x", mb(0) & 0x7);
				break;

			case 3:
				strcat(t, "2D");
				break;
				
			case 4:
				strcat(t, "3D");
				break;
			}
			t += strlen(t);
			if (mb(0) & 0x8)
				strcpy(t, "-MANUAL, ");
			else
				strcpy(t, "-AUTO, ");
			t += strlen(t);
			
			sats = (mb(0)>>4) & 0xF;
			
			sprintf(t, "PDOP %.2f, HDOP %.2f, VDOP %.2f, TDOP %.2f, %d satellite%s in view: ",
				getflt((unsigned char *)&mb(1)),
				getflt((unsigned char *)&mb(5)),
				getflt((unsigned char *)&mb(9)),
				getflt((unsigned char *)&mb(13)),
				sats, (sats == 1) ? "" : "s");
			t += strlen(t);

			for (i=0; i < sats; i++)
			{
				sprintf(t, "%s%02d", i ? ", " : "", mb(17+i));
				t += strlen(t);
				if (tr)
					tr->ctrack |= (1 << (mb(17+i)-1));
			}

			if (tr)
                        { /* mark for tracking status query */
				tr->qtracking = 1;
			}
		}
		break;
		
		case CMD_RSTATTRACK:
		{
			sprintf(t-2, "[%02d]=\"", mb(0)); /* add index to var name */
			t += strlen(t);

			if (getflt((unsigned char *)&mb(4)) < 0.0)
			{
				strcpy(t, "<NO MEASUREMENTS>");
				var_flag &= ~DEF;
			}
			else
			{	
				sprintf(t, "ch=%d, acq=%s, eph=%d, signal_level= %5.2f, elevation= %5.2f, azimuth= %6.2f",
					(mb(1) & 0xFF)>>3,
					mb(2) ? ((mb(2) == 1) ? "ACQ" : "SRCH") : "NEVER",
					mb(3),
					getflt((unsigned char *)&mb(4)),
					getflt((unsigned char *)&mb(12)) * RTOD,
					getflt((unsigned char *)&mb(16)) * RTOD);
				t += strlen(t);
				if (mb(20))
				{
					var_flag &= ~DEF;
					strcpy(t, ", OLD");
				}
				t += strlen(t);
				if (mb(22))
				{
					if (mb(22) == 1)
						strcpy(t, ", BAD PARITY");
					else
						if (mb(22) == 2)
							strcpy(t, ", BAD EPH HEALTH");
				}
				t += strlen(t);
				if (mb(23))
					strcpy(t, ", collecting data");
			}
		}
		break;
		
		default:
			strcpy(t, "<UNDECODED>");
			break;
		}
		strcat(t,"\"");
		set_var(&parse->kv, pbuffer, sizeof(pbuffer), var_flag);
	}
}


/**============================================================
 ** RAWDCF support
 **/

/*--------------------------------------------------
 * rawdcf_init_1 - set up modem lines for RAWDCF receivers
 * SET DTR line
 */
#if defined(TIOCMSET) && (defined(TIOCM_DTR) || defined(CIOCM_DTR))
static int
rawdcf_init_1(
	struct parseunit *parse
	)
{
	/* fixed 2000 for using with Linux by Wolfram Pienkoss <wp@bszh.de> */
	/*
	 * You can use the RS232 to supply the power for a DCF77 receiver.
	 * Here a voltage between the DTR and the RTS line is used. Unfortunately
	 * the name has changed from CIOCM_DTR to TIOCM_DTR recently.
	 */
	int sl232;

	if (ioctl(parse->generic->io.fd, TIOCMGET, (caddr_t)&sl232) == -1)
	{
		msyslog(LOG_NOTICE, "PARSE receiver #%d: rawdcf_init_1: WARNING: ioctl(fd, TIOCMGET, [C|T]IOCM_DTR): %m", CLK_UNIT(parse->peer));
		return 0;
	}

#ifdef TIOCM_DTR
	sl232 = (sl232 & ~TIOCM_RTS) | TIOCM_DTR;	/* turn on DTR, clear RTS for power supply */
#else
	sl232 = (sl232 & ~CIOCM_RTS) | CIOCM_DTR;	/* turn on DTR, clear RTS for power supply */
#endif

	if (ioctl(parse->generic->io.fd, TIOCMSET, (caddr_t)&sl232) == -1)
	{
		msyslog(LOG_NOTICE, "PARSE receiver #%d: rawdcf_init_1: WARNING: ioctl(fd, TIOCMSET, [C|T]IOCM_DTR): %m", CLK_UNIT(parse->peer));
	}
	return 0;
}
#else
static int
rawdcfdtr_init_1(
	struct parseunit *parse
	)
{
	msyslog(LOG_NOTICE, "PARSE receiver #%d: rawdcf_init_1: WARNING: OS interface incapable of setting DTR to power DCF modules", CLK_UNIT(parse->peer));
	return 0;
}
#endif  /* DTR initialisation type */

/*--------------------------------------------------
 * rawdcf_init_2 - set up modem lines for RAWDCF receivers
 * CLR DTR line, SET RTS line
 */
#if defined(TIOCMSET) &&  (defined(TIOCM_RTS) || defined(CIOCM_RTS))
static int
rawdcf_init_2(
	struct parseunit *parse
	)
{
	/* fixed 2000 for using with Linux by Wolfram Pienkoss <wp@bszh.de> */
	/*
	 * You can use the RS232 to supply the power for a DCF77 receiver.
	 * Here a voltage between the DTR and the RTS line is used. Unfortunately
	 * the name has changed from CIOCM_DTR to TIOCM_DTR recently.
	 */
	int sl232;

	if (ioctl(parse->generic->io.fd, TIOCMGET, (caddr_t)&sl232) == -1)
	{
		msyslog(LOG_NOTICE, "PARSE receiver #%d: rawdcf_init_2: WARNING: ioctl(fd, TIOCMGET, [C|T]IOCM_RTS): %m", CLK_UNIT(parse->peer));
		return 0;
	}

#ifdef TIOCM_RTS
	sl232 = (sl232 & ~TIOCM_DTR) | TIOCM_RTS;	/* turn on RTS, clear DTR for power supply */
#else
	sl232 = (sl232 & ~CIOCM_DTR) | CIOCM_RTS;	/* turn on RTS, clear DTR for power supply */
#endif

	if (ioctl(parse->generic->io.fd, TIOCMSET, (caddr_t)&sl232) == -1)
	{
		msyslog(LOG_NOTICE, "PARSE receiver #%d: rawdcf_init_2: WARNING: ioctl(fd, TIOCMSET, [C|T]IOCM_RTS): %m", CLK_UNIT(parse->peer));
	}
	return 0;
}
#else
static int
rawdcf_init_2(
	struct parseunit *parse
	)
{
	msyslog(LOG_NOTICE, "PARSE receiver #%d: rawdcf_init_2: WARNING: OS interface incapable of setting RTS to power DCF modules", CLK_UNIT(parse->peer));
	return 0;
}
#endif  /* DTR initialisation type */

#else	/* defined(REFCLOCK) && defined(PARSE) */
int refclock_parse_bs;
#endif	/* defined(REFCLOCK) && defined(PARSE) */

/*
 * History:
 *
 * refclock_parse.c,v
 * Revision 4.36  1999/11/28 17:18:20  kardel
 * disabled burst mode
 *
 * Revision 4.35  1999/11/28 09:14:14  kardel
 * RECON_4_0_98F
 *
 * Revision 4.34  1999/05/14 06:08:05  kardel
 * store current_time in a suitable container (u_long)
 *
 * Revision 4.33  1999/05/13 21:48:38  kardel
 * double the no response timeout interval
 *
 * Revision 4.32  1999/05/13 20:09:13  kardel
 * complain only about missing polls after a full poll interval
 *
 * Revision 4.31  1999/05/13 19:59:32  kardel
 * add clock type 16 for RTS set DTR clr in RAWDCF
 *
 * Revision 4.30  1999/02/28 20:36:43  kardel
 * fixed printf fmt
 *
 * Revision 4.29  1999/02/28 19:58:23  kardel
 * updated copyright information
 *
 * Revision 4.28  1999/02/28 19:01:50  kardel
 * improved debug out on sent Meinberg messages
 *
 * Revision 4.27  1999/02/28 18:05:55  kardel
 * no linux/ppsclock.h stuff
 *
 * Revision 4.26  1999/02/28 15:27:27  kardel
 * wharton clock integration
 *
 * Revision 4.25  1999/02/28 14:04:46  kardel
 * added missing double quotes to UTC information string
 *
 * Revision 4.24  1999/02/28 12:06:50  kardel
 * (parse_control): using gmprettydate instead of prettydate()
 * (mk_utcinfo): new function for formatting GPS derived UTC information
 * (gps16x_message): changed to use mk_utcinfo()
 * (trimbletsip_message): changed to use mk_utcinfo()
 * ignoring position information in unsynchronized mode
 * (parse_start): augument linux support for optional ASYNC_LOW_LATENCY
 *
 * Revision 4.23  1999/02/23 19:47:53  kardel
 * fixed #endifs
 * (stream_receive): fixed formats
 *
 * Revision 4.22  1999/02/22 06:21:02  kardel
 * use new autoconfig symbols
 *
 * Revision 4.21  1999/02/21 12:18:13  kardel
 * 4.91f reconcilation
 *
 * Revision 4.20  1999/02/21 10:53:36  kardel
 * initial Linux PPSkit version
 *
 * Revision 4.19  1999/02/07 09:10:45  kardel
 * clarify STREAMS mitigation rules in comment
 *
 * Revision 4.18  1998/12/20 23:45:34  kardel
 * fix types and warnings
 *
 * Revision 4.17  1998/11/15 21:24:51  kardel
 * cannot access mbg_ routines when CLOCK_MEINBERG
 * is not defined
 *
 * Revision 4.16  1998/11/15 20:28:17  kardel
 * Release 4.0.73e13 reconcilation
 *
 * Revision 4.15  1998/08/22 21:56:08  kardel
 * fixed IO handling for non-STREAM IO
 *
 * Revision 4.14  1998/08/16 19:00:48  kardel
 * (gps16x_message): reduced UTC parameter information (dropped A0,A1)
 * made uval a local variable (killed one of the last globals)
 * (sendetx): added logging of messages when in debug mode
 * (trimble_check): added periodic checks to facilitate re-initialization
 * (trimbletsip_init): made use of EOL character if in non-kernel operation
 * (trimbletsip_message): extended message interpretation
 * (getdbl): fixed data conversion
 *
 * Revision 4.13  1998/08/09 22:29:13  kardel
 * Trimble TSIP support
 *
 * Revision 4.12  1998/07/11 10:05:34  kardel
 * Release 4.0.73d reconcilation
 *
 * Revision 4.11  1998/06/14 21:09:42  kardel
 * Sun acc cleanup
 *
 * Revision 4.10  1998/06/13 12:36:45  kardel
 * signed/unsigned, name clashes
 *
 * Revision 4.9  1998/06/12 15:30:00  kardel
 * prototype fixes
 *
 * Revision 4.8  1998/06/12 11:19:42  kardel
 * added direct input processing routine for refclocks in
 * order to avaiod that single character io gobbles up all
 * receive buffers and drops input data. (Problem started
 * with fast machines so a character a buffer was possible
 * one of the few cases where faster machines break existing
 * allocation algorithms)
 *
 * Revision 4.7  1998/06/06 18:35:20  kardel
 * (parse_start): added BURST mode initialisation
 *
 * Revision 4.6  1998/05/27 06:12:46  kardel
 * RAWDCF_BASEDELAY default added
 * old comment removed
 * casts for ioctl()
 *
 * Revision 4.5  1998/05/25 22:05:09  kardel
 * RAWDCF_SETDTR option removed
 * clock type 14 attempts to set DTR for
 * power supply of RAWDCF receivers
 *
 * Revision 4.4  1998/05/24 16:20:47  kardel
 * updated comments referencing Meinberg clocks
 * added RAWDCF clock with DTR set option as type 14
 *
 * Revision 4.3  1998/05/24 10:48:33  kardel
 * calibrated CONRAD RAWDCF default fudge factor
 *
 * Revision 4.2  1998/05/24 09:59:35  kardel
 * corrected version information (ntpq support)
 *
 * Revision 4.1  1998/05/24 09:52:31  kardel
 * use fixed format only (new IO model)
 * output debug to stdout instead of msyslog()
 * don't include >"< in ASCII output in order not to confuse
 * ntpq parsing
 *
 * Revision 4.0  1998/04/10 19:52:11  kardel
 * Start 4.0 release version numbering
 *
 * Revision 1.2  1998/04/10 19:28:04  kardel
 * initial NTP VERSION 4 integration of PARSE with GPS166 binary support
 * derived from 3.105.1.2 from V3 tree
 *
 * Revision information 3.1 - 3.105 from log deleted 1998/04/10 kardel
 *
 */
