#if defined(REFCLOCK) && (defined(PARSE) || defined(PARSEPPS))
/*
 * /src/NTP/REPOSITORY/v3/xntpd/refclock_parse.c,v 3.53 1994/03/25 13:07:39 kardel Exp
 *
 * refclock_parse.c,v 3.53 1994/03/25 13:07:39 kardel Exp
 *
 * generic reference clock driver for receivers
 *
 * Added support for the Boeder DCF77 receiver on FreeBSD
 * by Vincenzo Capuano 1995/04/18.
 *
 * make use of a STREAMS module for input processing where
 * available and configured. Currently the STREAMS module
 * is only available for Suns running SunOS 4.x and SunOS5.x (new - careful!)
 *
 * Copyright (c) 1989,1990,1991,1992,1993,1994
 * Frank Kardel Friedrich-Alexander Universitaet Erlangen-Nuernberg
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

/*
 * Defines:
 *  REFCLOCK && (PARSE||PARSEPPS)
 *                    - enable this mess
 *  STREAM            - allow for STREAMS modules
 *                      ("parse", "ppsclocd", "ppsclock")
 *  PARSEPPS          - provide PPS information to loopfilter (for
 *                      backward compatibilty only)
 *  PPS		      - supply loopfilter with PPS samples (if configured)
 *  PPSPPS            - notify loopfilter of PPS file descriptor
 *
 *  FREEBSD_CONRAD    - Make very cheap "Conrad DCF77 RS-232" gadget work
 *			with FreeBSD.
 *  BOEDER            - Make cheap "Boeder DCF77 RS-232" receiver work
 *			with FreeBSD.
 * TTY defines:
 *  HAVE_BSD_TTYS     - currently unsupported
 *  HAVE_SYSV_TTYS    - will use termio.h
 *  HAVE_TERMIOS      - will use termios.h
 *  STREAM            - will use streams and implies HAVE_TERMIOS
 */

/*
 * This driver currently provides the support for
 *   - Meinberg DCF77 receiver DCF77 PZF 535 (TCXO version) (DCF)
 *   - Meinberg DCF77 receiver DCF77 PZF 535 (OCXO version) (DCF)
 *   - Meinberg DCF77 receiver U/A 31                       (DCF)
 *   - ELV DCF7000                                          (DCF)
 *   - Schmid clock                                         (DCF)
 *   - Conrad DCF77 receiver module                         (DCF)
 *   - Boeder DCF77 receiver                                (DCF)
 *   - FAU DCF77 NTP receiver (TimeBrick)                   (DCF)
 *   - Meinberg GPS166                                      (GPS)
 *   - Trimble SV6 (TSIP and TAIP protocol)                 (GPS)
 *
 */

/*
 * Meinberg receivers are connected via a 9600 baud serial line
 *
 * Receivers that do NOT support:
 *          - leap second indication
 * 	DCF U/A 31
 *	DCF PZF535 (stock version)
 *
 * so...
 *          - for PZF535 please ask for revision PZFUERL4.6 or higher
 *            (support for leap second and alternate antenna)
 *
 * The Meinberg GPS receiver also has a special NTP time stamp
 * format. The firmware release is Uni-Erlangen. Only this
 * firmware release is supported by xntp3.
 *
 * Meinberg generic receiver setup:
 *	output time code every second
 *	Baud rate 9600 7E2S
 */

#include "ntpd.h"
#include "ntp_refclock.h"
#include "ntp_unixtime.h"
#include "ntp_control.h"

#include <stdio.h>
#include <ctype.h>
#include <time.h>

#include <sys/errno.h>
#ifdef FREEBSD_CONRAD
#include <sys/ioctl.h>
#endif
extern int errno;

#if !defined(STREAM) && !defined(HAVE_SYSV_TTYS) && !defined(HAVE_BSD_TTYS) && !defined(HAVE_TERMIOS)
/* #error NEED TO DEFINE ONE OF "STREAM" or "HAVE_SYSV_TTYS" */
NEED TO DEFINE ONE OF "STREAM", "HAVE_SYSV_TTYS" or "HAVE_TERMIOS"
#endif

#ifdef STREAM
#include <sys/stream.h>
#include <sys/stropts.h>
#ifndef HAVE_TERMIOS
#define HAVE_TERMIOS
#endif
#endif

#ifdef HAVE_TERMIOS
#include <termios.h>
#define TTY_GETATTR(_FD_, _ARG_) tcgetattr((_FD_), (_ARG_))
#define TTY_SETATTR(_FD_, _ARG_) tcsetattr((_FD_), TCSANOW, (_ARG_))
#undef HAVE_SYSV_TTYS
#endif

#ifdef HAVE_SYSV_TTYS
#include <termio.h>
#define TTY_GETATTR(_FD_, _ARG_) ioctl((_FD_), TCGETA, (_ARG_))
#define TTY_SETATTR(_FD_, _ARG_) ioctl((_FD_), TCSETAW, (_ARG_))
#endif

#ifdef HAVE_BSD_TTYS
/* #error CURRENTLY NO BSD TTY SUPPORT */
CURRENTLY NO BSD TTY SUPPORT
#endif

#if	!defined(O_RDWR)	/* XXX SOLARIS */
#include <fcntl.h>
#endif	/* !def(O_RDWR) */

#ifdef PPSPPS
#include <sys/ppsclock.h>
#endif

#include "ntp_select.h"
#include "ntp_stdlib.h"

#include "parse.h"

#if !defined(NO_SCCSID) && !defined(lint) && !defined(__GNUC__)
static char rcsid[]="refclock_parse.c,v 3.53 1994/03/25 13:07:39 kardel Exp";
#endif

/**===========================================================================
 ** external interface to xntp mechanism
 **/

static	void	parse_init	P((void));
static	int	parse_start	P((int, struct peer *));
static	void	parse_shutdown	P((int, struct peer *));
static	void	parse_poll	P((int, struct peer *));
static	void	parse_control	P((int, struct refclockstat *, struct refclockstat *));

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
 * the unit field selects for one the prototype to be used (lower 4 bits)
 * and for the other the clock type in case of different but similar
 * receivers (bits 4-6)
 * the most significant bit encodes PPS support
 * when the most significant bit is set the pps telegrams will be used
 * for controlling the local clock (ntp_loopfilter.c)
 * receiver specific configration data is kept in the clockinfo field.
 */

/*
 * Definitions
 */
#define	MAXUNITS	4	/* maximum number of "PARSE" units permitted */
#define PARSEDEVICE	"/dev/refclock-%d" /* device to open %d is unit number */

/**===========================================================================
 ** function vector for dynamically binding io handling mechanism
 **/

typedef struct bind
{
  char   *bd_description;	/* name of type of binding */
  int	(*bd_init)();		/* initialize */
  void	(*bd_end)();		/* end */
  int   (*bd_setcs)();		/* set character size */
  int	(*bd_disable)();	/* disable */
  int	(*bd_enable)();		/* enable */
  int	(*bd_getfmt)();		/* get format */
  int	(*bd_setfmt)();		/* setfmt */
  int	(*bd_getstat)();	/* getstat */
  int	(*bd_setstat)();	/* setstat */
  int	(*bd_timecode)();	/* get time code */
  void	(*bd_receive)();	/* receive operation */
  void	(*bd_poll)();		/* poll operation */
} bind_t;

#define PARSE_END(_X_)			(*(_X_)->binding->bd_end)(_X_)
#define PARSE_SETCS(_X_, _CS_)		(*(_X_)->binding->bd_setcs)(_X_, _CS_)
#define PARSE_ENABLE(_X_)		(*(_X_)->binding->bd_enable)(_X_)
#define PARSE_DISABLE(_X_)		(*(_X_)->binding->bd_disable)(_X_)
#define PARSE_GETFMT(_X_, _DCT_)	(*(_X_)->binding->bd_getfmt)(_X_, _DCT_)
#define PARSE_SETFMT(_X_, _DCT_)	(*(_X_)->binding->bd_setfmt)(_X_, _DCT_)
#define PARSE_GETSTAT(_X_, _DCT_)	(*(_X_)->binding->bd_getstat)(_X_, _DCT_)
#define PARSE_SETSTAT(_X_, _DCT_)	(*(_X_)->binding->bd_setstat)(_X_, _DCT_)
#define PARSE_GETTIMECODE(_X_, _DCT_)	(*(_X_)->binding->bd_timecode)(_X_, _DCT_)
#define PARSE_POLL(_X_)			(*(_X_)->binding->bd_poll)(_X_)

/*
 * io modes
 */
#define PARSE_F_NOPOLLONLY	0x0001 /* always do async io (possible PPS support via PARSE) */
#define PARSE_F_POLLONLY	0x0002 /* never do async io  (no PPS support via PARSE) */
#define PARSE_F_PPSPPS		0x0004 /* use loopfilter PPS code (CIOGETEV) */
#define PARSE_F_PPSONSECOND	0x0008 /* PPS pulses are on second */

/**===========================================================================
 ** refclock instance data
 **/

struct parseunit
{
  /*
   * XNTP management
   */
  struct peer        *peer;		/* backlink to peer structure - refclock inactive if 0  */
  int                 fd;		/* device file descriptor */
  u_char              unit;		/* encoded unit/type/PPS */

  /*
   * XNTP io
   */
  struct refclockio   io;		/* io system structure (used in PPS mode) */
  bind_t	     *binding;	        /* io handling binding */

  /*
   * parse state
   */
  parse_t	      parseio;	        /* io handling structure (user level parsing) */

  /*
   * type specific parameters
   */
  struct my_clockinfo   *parse_type;	        /* link to clock description */

  /*
   * clock specific configuration
   */
  l_fp                basedelay;        /* clock local phase offset */
  l_fp                ppsdelay;         /* clock local pps phase offset */

  /*
   * clock state handling/reporting
   */
  u_char	      flags;	        /* flags (leap_control) */
  u_char              status;		/* current status */
  u_char              lastevent; 	/* last not NORMAL status */
  U_LONG	      lastchange;       /* time (xntp) when last state change accured */
  U_LONG	      statetime[CEVNT_MAX+1]; /* accumulated time of clock states */
  struct event        stattimer;        /* statistics timer */
  U_LONG              polls;		/* polls from NTP protocol machine */
  U_LONG              noresponse; 	/* number of expected but not seen datagrams */
  U_LONG              badformat; 	/* bad format (failed format conversions) */
  U_LONG              baddata;		/* usually bad receive length, bad format */

  u_char              pollonly;		/* 1 for polling only (no PPS mode) */
  u_char              pollneeddata; 	/* 1 for receive sample expected in PPS mode */
  U_LONG              laststatus;       /* last packet status (error indication) */
  u_short	      lastformat;       /* last format used */
  U_LONG              lastsync;		/* time (xntp) when clock was last seen fully synchronized */
  U_LONG              timestarted; 	/* time (xntp) when peer clock was instantiated */
  U_LONG	      nosynctime; 	/* time (xntp) when last nosync message was posted */
  U_LONG              lastmissed;       /* time (xntp) when poll didn't get data (powerup heuristic) */
  U_LONG              ppsserial;        /* magic cookie for ppsclock serials (avoids stale ppsclock data) */
  parsetime_t         time;		/* last (parse module) data */
  void               *localdata;        /* optional local data */
};


/**===========================================================================
 ** Clockinfo section all parameter for specific clock types
 ** includes NTP paramaters, TTY parameters and IO handling parameters
 **/

static	void	poll_dpoll	P((struct parseunit *));
static	void	poll_poll	P((struct parseunit *));
static	int	poll_init	P((struct parseunit *));
static	void	poll_end	P((struct parseunit *));

typedef struct poll_info
{
  U_LONG rate;			/* poll rate - once every "rate" seconds - 0 off */
  char * string;		/* string to send for polling */
  U_LONG count;			/* number of charcters in string */
} poll_info_t;

#define NO_FLAGS	0
#define NO_POLL		(void (*)())0
#define NO_INIT		(int  (*)())0
#define NO_END		(void (*)())0
#define NO_DATA		(void *)0
#define NO_FORMAT	""
#define NO_PPSDELAY     0

#define DCF_ID		"DCF"	/* generic DCF */
#define DCF_A_ID	"DCFa"	/* AM demodulation */
#define DCF_P_ID	"DCFp"	/* psuedo random phase shift */
#define GPS_ID		"GPS"	/* GPS receiver */

#define	NOCLOCK_ROOTDELAY	0x00000000
#define	NOCLOCK_BASEDELAY	0x00000000
#define	NOCLOCK_DESCRIPTION	((char *)0)
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
#define NOCLOCK_FORMAT		NO_FORMAT
#define NOCLOCK_TYPE		CTL_SST_TS_UNSPEC

#define DCF_TYPE		CTL_SST_TS_LF
#define GPS_TYPE		CTL_SST_TS_UHF

/*
 * receiver specific constants
 */
#define MBG_CFLAG19200		(B19200|CS7|PARENB|CREAD|HUPCL)
#define MBG_CFLAG		(B9600|CS7|PARENB|CREAD|HUPCL)
#define MBG_IFLAG		(IGNBRK|IGNPAR|ISTRIP)
#define MBG_OFLAG		0
#define MBG_LFLAG		0
/*
 * Meinberg DCF U/A 31 (AM) receiver
 */
#define	DCFUA31_ROOTDELAY	0x00000D00  /* 50.78125ms */
#define	DCFUA31_BASEDELAY	0x02C00000  /* 10.7421875ms: 10 ms (+/- 3 ms) */
#define	DCFUA31_DESCRIPTION	"Meinberg DCF U/A 31"
#define DCFUA31_MAXUNSYNC       60*30       /* only trust clock for 1/2 hour */
#define DCFUA31_CFLAG           MBG_CFLAG
#define DCFUA31_IFLAG           MBG_IFLAG
#define DCFUA31_OFLAG           MBG_OFLAG
#define DCFUA31_LFLAG           MBG_LFLAG

/*
 * Meinberg DCF PZF535/TCXO (FM/PZF) receiver
 */
#define	DCFPZF535_ROOTDELAY	0x00000034  /* 800us */
#define	DCFPZF535_BASEDELAY	0x00800000  /* 1.968ms +- 104us (oscilloscope) - relative to start (end of STX) */
#define	DCFPZF535_DESCRIPTION	"Meinberg DCF PZF 535/TCXO"
#define DCFPZF535_MAXUNSYNC     60*60*12           /* only trust clock for 12 hours
						    * @ 5e-8df/f we have accumulated
						    * at most 2.16 ms (thus we move to
						    * NTP synchronisation */
#define DCFPZF535_CFLAG         MBG_CFLAG
#define DCFPZF535_IFLAG         MBG_IFLAG
#define DCFPZF535_OFLAG         MBG_OFLAG
#define DCFPZF535_LFLAG         MBG_LFLAG


/*
 * Meinberg DCF PZF535/OCXO receiver
 */
#define	DCFPZF535OCXO_ROOTDELAY	0x00000034 /* 800us (max error * 10) */
#define	DCFPZF535OCXO_BASEDELAY	0x00800000 /* 1.968ms +- 104us (oscilloscope) - relative to start (end of STX) */
#define	DCFPZF535OCXO_DESCRIPTION "Meinberg DCF PZF 535/OCXO"
#define DCFPZF535OCXO_MAXUNSYNC     60*60*96       /* only trust clock for 4 days
						    * @ 5e-9df/f we have accumulated
						    * at most an error of 1.73 ms
						    * (thus we move to NTP synchronisation) */
#define DCFPZF535OCXO_CFLAG         MBG_CFLAG
#define DCFPZF535OCXO_IFLAG         MBG_IFLAG
#define DCFPZF535OCXO_OFLAG         MBG_OFLAG
#define DCFPZF535OCXO_LFLAG         MBG_LFLAG

/*
 * Meinberg GPS166 receiver
 */
#define	GPS166_ROOTDELAY	0x00000000         /* nothing here */
#define	GPS166_BASEDELAY	0x00800000         /* XXX to be fixed ! 1.968ms +- 104us (oscilloscope) - relative to start (end of STX) */
#define	GPS166_DESCRIPTION      "Meinberg GPS166 receiver"
#define GPS166_MAXUNSYNC        0                  /* this clock is immediately lost */
#define GPS166_CFLAG            MBG_CFLAG
#define GPS166_IFLAG            MBG_IFLAG
#define GPS166_OFLAG            MBG_OFLAG
#define GPS166_LFLAG            MBG_LFLAG
#define GPS166_POLL		NO_POLL
#define GPS166_INIT		NO_INIT
#define GPS166_END		NO_END
#define GPS166_DATA		NO_DATA
#define GPS166_ID		GPS_ID
#define GPS166_FORMAT		NO_FORMAT

/*
 * ELV DCF7000 Wallclock-Receiver/Switching Clock (Kit)
 *
 * This is really not the hottest clock - but before you have nothing ...
 */
#define DCF7000_ROOTDELAY	0x00000364 /* 13 ms */
#define DCF7000_BASEDELAY	0x67AE0000 /* 405 ms - slow blow */
#define DCF7000_DESCRIPTION	"ELV DCF7000"
#define DCF7000_MAXUNSYNC	(60*5) /* sorry - but it just was not build as a clock */
#define DCF7000_CFLAG           (B9600|CS8|CREAD|PARENB|PARODD|CLOCAL|HUPCL)
#define DCF7000_IFLAG		(IGNBRK)
#define DCF7000_OFLAG		0
#define DCF7000_LFLAG		0

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
#define WSDCF_END		poll_end
#define WSDCF_DATA		((void *)(&wsdcf_pollinfo))
#define	WSDCF_ROOTDELAY		0X00004A3D	/*  ~ 290ms */
#define	WSDCF_BASEDELAY	 	0x028F5C29	/*  ~  10ms */
#define WSDCF_DESCRIPTION	"WS/DCF Receiver"
#define WSDCF_FORMAT		"Schmid"
#define WSDCF_MAXUNSYNC		(60*60)	/* assume this beast hold at 1 h better than 2 ms XXX-must verify */
#define WSDCF_CFLAG		(B1200|CS8|CREAD|CLOCAL)
#define WSDCF_IFLAG		0
#define WSDCF_OFLAG		0
#define WSDCF_LFLAG		0

/*
 * RAW DCF77 - input of DCF marks via RS232 - many variants
 */
#define RAWDCF_FLAGS		PARSE_F_NOPOLLONLY
#define RAWDCF_ROOTDELAY	0x00000364 /* 13 ms */
#define RAWDCF_FORMAT		"RAW DCF77 Timecode"
#define RAWDCF_MAXUNSYNC	(0) /* sorry - its a true receiver - no signal - no time */

#if defined(FREEBSD_CONRAD) || (defined(SYS_FREEBSD) && defined(BOEDER))
#define RAWDCF_CFLAG            (CS8|CREAD|CLOCAL)
#else
#define RAWDCF_CFLAG            (B50|CS8|CREAD|CLOCAL)
#endif
#define RAWDCF_IFLAG		0
#define RAWDCF_OFLAG		0
#define RAWDCF_LFLAG		0

/*
 * RAW DCF variants
 */
/*
 * Conrad receiver
 *
 * simplest (cheapest) DCF clock - e. g. DCF77 receiver by Conrad
 * (~40DM - roughly $30 ) followed by a level converter for RS232
 */
#define CONRAD_BASEDELAY	0x420C49B0 /* ~258 ms - Conrad receiver @ 50 Baud on a Sun */
#define CONRAD_DESCRIPTION	"RAW DCF77 CODE (Conrad DCF77 receiver module)"

/*
 * Boeder receiver
 *
 * simple (cheap) DCF clock - e. g. DCF77 receiver by Boeder
 * followed by a level converter for RS232
 */
#define BOEDER_BASEDELAY        0x420C49B0 /* ~258 ms - Conrad receiver @ 50 Baud */
#define BOEDER_DESCRIPTION      "RAW DCF77 CODE (BOEDER DCF77 receiver)"

/*
 * TimeBrick receiver
 */
#define TIMEBRICK_BASEDELAY	0x35C29000 /* ~210 ms - TimeBrick @ 50 Baud on a Sun */
#define TIMEBRICK_DESCRIPTION	"RAW DCF77 CODE (TimeBrick)"

/*
 * Trimble SV6 GPS receivers (TAIP and TSIP protocols)
 */
#define ETX	0x03
#define DLE	0x10

#define TRIM_POLLRATE	0	/* only true direct polling */

#define TRIM_TAIPPOLLCMD	">QTM<"
#define TRIM_TAIPCMDSIZE	5
static poll_info_t trimbletaip_pollinfo = { TRIM_POLLRATE, TRIM_TAIPPOLLCMD, TRIM_TAIPCMDSIZE };
static	int	trimbletaip_init	P((struct parseunit *));

/* query time & UTC correction data */
static char tsipquery[] = { DLE, 0x21, DLE, ETX, DLE, 0x2F, DLE, ETX };

static poll_info_t trimbletsip_pollinfo = { TRIM_POLLRATE, tsipquery, sizeof(tsipquery) };
static	int	trimbletsip_init	P((struct parseunit *));

#define TRIMBLETAIP_CFLAG           (B4800|CS8|CREAD)
#define TRIMBLETAIP_IFLAG           (BRKINT|IGNPAR|ISTRIP|ICRNL|IXON)
#define TRIMBLETAIP_OFLAG           (OPOST|ONLCR)
#define TRIMBLETAIP_LFLAG           (ICANON|ECHOK)
#define TRIMBLETSIP_CFLAG           (B9600|CS8|CLOCAL|CREAD|PARENB|PARODD)
#define TRIMBLETSIP_IFLAG           (IGNBRK)
#define TRIMBLETSIP_OFLAG           (0)
#define TRIMBLETSIP_LFLAG           (0)

#define TRIMBLETAIP_FLAGS	    (PARSE_F_PPSPPS|PARSE_F_PPSONSECOND)
#define TRIMBLETSIP_FLAGS	    (TRIMBLETAIP_FLAGS|PARSE_F_NOPOLLONLY)

#define TRIMBLETAIP_POLL	    poll_dpoll
#define TRIMBLETSIP_POLL	    poll_dpoll

#define TRIMBLETAIP_INIT		    trimbletaip_init
#define TRIMBLETSIP_INIT		    trimbletsip_init

#define TRIMBLETAIP_END		    poll_end
#define TRIMBLETSIP_END		    poll_end

#define TRIMBLETAIP_DATA	    ((void *)(&trimbletaip_pollinfo))
#define TRIMBLETSIP_DATA	    ((void *)(&trimbletsip_pollinfo))

#define TRIMBLETAIP_ID		    GPS_ID
#define TRIMBLETSIP_ID		    GPS_ID

#define TRIMBLETAIP_FORMAT	    NO_FORMAT
#define TRIMBLETSIP_FORMAT	    "Trimble SV6/TSIP"

#define TRIMBLETAIP_ROOTDELAY        0x0
#define TRIMBLETSIP_ROOTDELAY        0x0

#define TRIMBLETAIP_BASEDELAY        0x0
#define TRIMBLETSIP_BASEDELAY        0x51EB852	/* 20 ms as a l_uf - avg GPS time message latency */

#define TRIMBLETAIP_DESCRIPTION      "Trimble GPS (TAIP) receiver"
#define TRIMBLETSIP_DESCRIPTION      "Trimble GPS (TSIP) receiver"

#define TRIMBLETAIP_MAXUNSYNC        0
#define TRIMBLETSIP_MAXUNSYNC        0

#define TRIMBLETAIP_EOL		    '<'

static struct my_clockinfo
{
  U_LONG  cl_flags;		/* operation flags (io modes) */
  void  (*cl_poll)();		/* active poll routine */
  int   (*cl_init)();		/* active poll init routine */
  void  (*cl_end)();		/* active poll end routine */
  void   *cl_data;		/* local data area for "poll" mechanism */
  u_fp    cl_rootdelay;		/* rootdelay */
  U_LONG  cl_basedelay;		/* current offset - unsigned l_fp fractional part */
  U_LONG  cl_ppsdelay;		/* current PPS offset - unsigned l_fp fractional part */
  char   *cl_id;		/* ID code (usually "DCF") */
  char   *cl_description;	/* device name */
  char   *cl_format;		/* fixed format */
  u_char  cl_type;		/* clock type (ntp control) */
  U_LONG  cl_maxunsync;		/* time to trust oscillator after loosing synch */
  U_LONG  cl_cflag;             /* terminal io flags */
  U_LONG  cl_iflag;             /* terminal io flags */
  U_LONG  cl_oflag;             /* terminal io flags */
  U_LONG  cl_lflag;             /* terminal io flags */
} clockinfo[] =
{				/*   0.  0.0.128 - base offset for PPS support */
  {				/* 127.127.8.<device> */
    NO_FLAGS,
    NO_POLL,
    NO_INIT,
    NO_END,
    NO_DATA,
    DCFPZF535_ROOTDELAY,
    DCFPZF535_BASEDELAY,
    NO_PPSDELAY,
    DCF_P_ID,
    DCFPZF535_DESCRIPTION,
    NO_FORMAT,
    DCF_TYPE,
    DCFPZF535_MAXUNSYNC,
    DCFPZF535_CFLAG,
    DCFPZF535_IFLAG,
    DCFPZF535_OFLAG,
    DCFPZF535_LFLAG
  },
  {				/* 127.127.8.4+<device> */
    NO_FLAGS,
    NO_POLL,
    NO_INIT,
    NO_END,
    NO_DATA,
    DCFPZF535OCXO_ROOTDELAY,
    DCFPZF535OCXO_BASEDELAY,
    NO_PPSDELAY,
    DCF_P_ID,
    DCFPZF535OCXO_DESCRIPTION,
    NO_FORMAT,
    DCF_TYPE,
    DCFPZF535OCXO_MAXUNSYNC,
    DCFPZF535OCXO_CFLAG,
    DCFPZF535OCXO_IFLAG,
    DCFPZF535OCXO_OFLAG,
    DCFPZF535OCXO_LFLAG
  },
  {				/* 127.127.8.8+<device> */
    NO_FLAGS,
    NO_POLL,
    NO_INIT,
    NO_END,
    NO_DATA,
    DCFUA31_ROOTDELAY,
    DCFUA31_BASEDELAY,
    NO_PPSDELAY,
    DCF_A_ID,
    DCFUA31_DESCRIPTION,
    NO_FORMAT,
    DCF_TYPE,
    DCFUA31_MAXUNSYNC,
    DCFUA31_CFLAG,
    DCFUA31_IFLAG,
    DCFUA31_OFLAG,
    DCFUA31_LFLAG
  },
  {				/* 127.127.8.12+<device> */
    NO_FLAGS,
    NO_POLL,
    NO_INIT,
    NO_END,
    NO_DATA,
    DCF7000_ROOTDELAY,
    DCF7000_BASEDELAY,
    NO_PPSDELAY,
    DCF_A_ID,
    DCF7000_DESCRIPTION,
    NO_FORMAT,
    DCF_TYPE,
    DCF7000_MAXUNSYNC,
    DCF7000_CFLAG,
    DCF7000_IFLAG,
    DCF7000_OFLAG,
    DCF7000_LFLAG
  },
  {				/* 127.127.8.16+<device> */
    NO_FLAGS,
    WSDCF_POLL,
    WSDCF_INIT,
    WSDCF_END,
    WSDCF_DATA,
    WSDCF_ROOTDELAY,
    WSDCF_BASEDELAY,
    NO_PPSDELAY,
    DCF_A_ID,
    WSDCF_DESCRIPTION,
    WSDCF_FORMAT,
    DCF_TYPE,
    WSDCF_MAXUNSYNC,
    WSDCF_CFLAG,
    WSDCF_IFLAG,
    WSDCF_OFLAG,
    WSDCF_LFLAG
  },
  {				/* 127.127.8.20+<device> */
    RAWDCF_FLAGS,
    NO_POLL,
    NO_INIT,
    NO_END,
    NO_DATA,
    RAWDCF_ROOTDELAY,
    CONRAD_BASEDELAY,
    NO_PPSDELAY,
    DCF_A_ID,
    CONRAD_DESCRIPTION,
    RAWDCF_FORMAT,
    DCF_TYPE,
    RAWDCF_MAXUNSYNC,
    RAWDCF_CFLAG,
    RAWDCF_IFLAG,
    RAWDCF_OFLAG,
    RAWDCF_LFLAG
  },
  {				/* 127.127.8.24+<device> */
    RAWDCF_FLAGS,
    NO_POLL,
    NO_INIT,
    NO_END,
    NO_DATA,
    RAWDCF_ROOTDELAY,
    TIMEBRICK_BASEDELAY,
    NO_PPSDELAY,
    DCF_A_ID,
    TIMEBRICK_DESCRIPTION,
    RAWDCF_FORMAT,
    DCF_TYPE,
    RAWDCF_MAXUNSYNC,
    RAWDCF_CFLAG,
    RAWDCF_IFLAG,
    RAWDCF_OFLAG,
    RAWDCF_LFLAG
  },
  {				/* 127.127.8.28+<device> */
    NO_FLAGS,
    GPS166_POLL,
    GPS166_INIT,
    GPS166_END,
    GPS166_DATA,
    GPS166_ROOTDELAY,
    GPS166_BASEDELAY,
    NO_PPSDELAY,
    GPS166_ID,
    GPS166_DESCRIPTION,
    GPS166_FORMAT,
    GPS_TYPE,
    GPS166_MAXUNSYNC,
    GPS166_CFLAG,
    GPS166_IFLAG,
    GPS166_OFLAG,
    GPS166_LFLAG
  },
  {				/* 127.127.8.32+<device> */
    TRIMBLETAIP_FLAGS,
    TRIMBLETAIP_POLL,
    TRIMBLETAIP_INIT,
    TRIMBLETAIP_END,
    TRIMBLETAIP_DATA,
    TRIMBLETAIP_ROOTDELAY,
    TRIMBLETAIP_BASEDELAY,
    NO_PPSDELAY,
    TRIMBLETAIP_ID,
    TRIMBLETAIP_DESCRIPTION,
    TRIMBLETAIP_FORMAT,
    GPS_TYPE,
    TRIMBLETAIP_MAXUNSYNC,
    TRIMBLETAIP_CFLAG,
    TRIMBLETAIP_IFLAG,
    TRIMBLETAIP_OFLAG,
    TRIMBLETAIP_LFLAG
  },
  {				/* 127.127.8.36+<device> */
    TRIMBLETSIP_FLAGS,
    TRIMBLETSIP_POLL,
    TRIMBLETSIP_INIT,
    TRIMBLETSIP_END,
    TRIMBLETSIP_DATA,
    TRIMBLETSIP_ROOTDELAY,
    TRIMBLETSIP_BASEDELAY,
    NO_PPSDELAY,
    TRIMBLETSIP_ID,
    TRIMBLETSIP_DESCRIPTION,
    TRIMBLETSIP_FORMAT,
    GPS_TYPE,
    TRIMBLETSIP_MAXUNSYNC,
    TRIMBLETSIP_CFLAG,
    TRIMBLETSIP_IFLAG,
    TRIMBLETSIP_OFLAG,
    TRIMBLETSIP_LFLAG
  },
  {				/* 127.127.8.40+<device> */
    RAWDCF_FLAGS,
    NO_POLL,
    NO_INIT,
    NO_END,
    NO_DATA,
    RAWDCF_ROOTDELAY,
    BOEDER_BASEDELAY,
    NO_PPSDELAY,
    DCF_A_ID,
    BOEDER_DESCRIPTION,
    RAWDCF_FORMAT,
    DCF_TYPE,
    RAWDCF_MAXUNSYNC,
    RAWDCF_CFLAG,
    RAWDCF_IFLAG,
    RAWDCF_OFLAG,
    RAWDCF_LFLAG
  }
};

static int ncltypes = sizeof(clockinfo) / sizeof(struct my_clockinfo);

#define CL_REALTYPE(x) (((x) >> 2) & 0x1F)
#define CL_TYPE(x)  ((CL_REALTYPE(x) >= ncltypes) ? ~0 : CL_REALTYPE(x))
#define CL_PPS(x)   ((x) & 0x80)
#define CL_UNIT(x)  ((x) & 0x3)

/*
 * Other constant stuff
 */
#define	PARSEHSREFID	0x7f7f08ff	/* 127.127.8.255 refid for hi strata */

#define PARSENOSYNCREPEAT (10*60)		/* mention uninitialized clocks all 10 minutes */
#define PARSESTATISTICS   (60*60)	        /* output state statistics every hour */

static struct parseunit *parseunits[MAXUNITS];

extern U_LONG current_time;
extern s_char sys_precision;
extern struct event timerqueue[];
#ifdef PPSPPS
extern int fdpps;
#endif

static int notice = 0;

#define PARSE_STATETIME(parse, i) ((parse->status == i) ? parse->statetime[i] + current_time - parse->lastchange : parse->statetime[i])

static void parse_event   P((struct parseunit *, int));
static void parse_process P((struct parseunit *, parsetime_t *));

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
static void stream_nop      P((struct parseunit *));
static int  stream_enable   P((struct parseunit *));
static int  stream_disable  P((struct parseunit *));
static int  stream_setcs    P((struct parseunit *, parsectl_t *));
static int  stream_getfmt   P((struct parseunit *, parsectl_t *));
static int  stream_setfmt   P((struct parseunit *, parsectl_t *));
static int  stream_getstat  P((struct parseunit *, parsectl_t *));
static int  stream_setstat  P((struct parseunit *, parsectl_t *));
static int  stream_timecode P((struct parseunit *, parsectl_t *));
static void stream_receive  P((struct recvbuf *));
static void stream_poll     P((struct parseunit *));
#endif

static int  local_init     P((struct parseunit *));
static void local_end      P((struct parseunit *));
static int  local_nop      P((struct parseunit *));
static int  local_setcs    P((struct parseunit *, parsectl_t *));
static int  local_getfmt   P((struct parseunit *, parsectl_t *));
static int  local_setfmt   P((struct parseunit *, parsectl_t *));
static int  local_getstat  P((struct parseunit *, parsectl_t *));
static int  local_setstat  P((struct parseunit *, parsectl_t *));
static int  local_timecode P((struct parseunit *, parsectl_t *));
static void local_receive  P((struct recvbuf *));
static void local_poll     P((struct parseunit *));

static bind_t io_bindings[] =
{
#ifdef STREAM
  {
    "parse STREAM",
    stream_init,
    stream_nop,
    stream_setcs,
    stream_disable,
    stream_enable,
    stream_getfmt,
    stream_setfmt,
    stream_getstat,
    stream_setstat,
    stream_timecode,
    stream_receive,
    stream_poll
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
    local_getstat,
    local_setstat,
    local_timecode,
    local_receive,
    local_poll
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
    local_getstat,
    local_setstat,
    local_timecode,
    local_receive,
    local_poll
  },
  {
    (char *)0,
  }
};

#ifdef STREAM
/*--------------------------------------------------
 * ppsclock STREAM init
 */
static int
ppsclock_init(parse)
     struct parseunit *parse;
{
  /*
   * now push the parse streams module
   * it will ensure exclusive access to the device
   */
  if (ioctl(parse->fd, I_PUSH, (caddr_t)"ppsclocd") == -1 &&
      ioctl(parse->fd, I_PUSH, (caddr_t)"ppsclock") == -1)
    {
      syslog(LOG_ERR, "PARSE receiver #%d: ppsclock_init: ioctl(fd, I_PUSH, \"ppsclock\"): %m",
	     CL_UNIT(parse->unit));
      return 0;
    }
  if (!local_init(parse))
    {
      (void)ioctl(parse->fd, I_POP, (caddr_t)0);
      return 0;
    }

  parse->flags |= PARSE_PPSCLOCK;
  return 1;
}

/*--------------------------------------------------
 * parse STREAM init
 */
static int
stream_init(parse)
     struct parseunit *parse;
{
  /*
   * now push the parse streams module
   * to test whether it is there (Oh boy - neat kernel interface)
   */
  if (ioctl(parse->fd, I_PUSH, (caddr_t)"parse") == -1)
    {
      syslog(LOG_ERR, "PARSE receiver #%d: stream_init: ioctl(fd, I_PUSH, \"parse\"): %m", CL_UNIT(parse->unit));
      return 0;
    }
  else
    {
      while(ioctl(parse->fd, I_POP, (caddr_t)0) == 0)
	/* empty loop */;

      /*
       * now push it a second time after we have removed all
       * module garbage
       */
      if (ioctl(parse->fd, I_PUSH, (caddr_t)"parse") == -1)
	{
	  syslog(LOG_ERR, "PARSE receiver #%d: stream_init: ioctl(fd, I_PUSH, \"parse\"): %m", CL_UNIT(parse->unit));
	  return 0;
	}
      else
	{
	  return 1;
        }
    }
}

 /*--------------------------------------------------
 * STREAM setcs
 */
static int
stream_setcs(parse, tcl)
     struct parseunit *parse;
     parsectl_t  *tcl;
{
  struct strioctl strioc;

  strioc.ic_cmd     = PARSEIOC_SETCS;
  strioc.ic_timout  = 0;
  strioc.ic_dp      = (char *)tcl;
  strioc.ic_len     = sizeof (*tcl);

  if (ioctl(parse->fd, I_STR, (caddr_t)&strioc) == -1)
    {
      syslog(LOG_ERR, "PARSE receiver #%d: stream_setcs: ioctl(fd, I_STR, PARSEIOC_SETCS): %m", CL_UNIT(parse->unit));
      return 0;
    }
  return 1;
}

/*--------------------------------------------------
 * STREAM nop
 */
static void
stream_nop(parse)
     struct parseunit *parse;
{
}

/*--------------------------------------------------
 * STREAM enable
 */
static int
stream_enable(parse)
     struct parseunit *parse;
{
  struct strioctl strioc;

  strioc.ic_cmd     = PARSEIOC_ENABLE;
  strioc.ic_timout  = 0;
  strioc.ic_dp      = (char *)0;
  strioc.ic_len     = 0;

  if (ioctl(parse->fd, I_STR, (caddr_t)&strioc) == -1)
    {
      syslog(LOG_ERR, "PARSE receiver #%d: stream_enable: ioctl(fd, I_STR, PARSEIOC_ENABLE): %m", CL_UNIT(parse->unit));
      return 0;
    }
  return 1;
}

/*--------------------------------------------------
 * STREAM disable
 */
static int
stream_disable(parse)
     struct parseunit *parse;
{
  struct strioctl strioc;

  strioc.ic_cmd     = PARSEIOC_DISABLE;
  strioc.ic_timout  = 0;
  strioc.ic_dp      = (char *)0;
  strioc.ic_len     = 0;

  if (ioctl(parse->fd, I_STR, (caddr_t)&strioc) == -1)
    {
      syslog(LOG_ERR, "PARSE receiver #%d: stream_disable: ioctl(fd, I_STR, PARSEIOC_DISABLE): %m", CL_UNIT(parse->unit));
      return 0;
    }
  return 1;
}

/*--------------------------------------------------
 * STREAM getfmt
 */
static int
stream_getfmt(parse, tcl)
     struct parseunit *parse;
     parsectl_t  *tcl;
{
  struct strioctl strioc;

  strioc.ic_cmd     = PARSEIOC_GETFMT;
  strioc.ic_timout  = 0;
  strioc.ic_dp      = (char *)tcl;
  strioc.ic_len     = sizeof (*tcl);
  if (ioctl(parse->fd, I_STR, (caddr_t)&strioc) == -1)
    {
      syslog(LOG_ERR, "PARSE receiver #%d: ioctl(fd, I_STR, PARSEIOC_GETFMT): %m", CL_UNIT(parse->unit));
      return 0;
    }
  return 1;
}

/*--------------------------------------------------
 * STREAM setfmt
 */
static int
stream_setfmt(parse, tcl)
     struct parseunit *parse;
     parsectl_t  *tcl;
{
  struct strioctl strioc;

  strioc.ic_cmd     = PARSEIOC_SETFMT;
  strioc.ic_timout  = 0;
  strioc.ic_dp      = (char *)tcl;
  strioc.ic_len     = sizeof (*tcl);

  if (ioctl(parse->fd, I_STR, (caddr_t)&strioc) == -1)
    {
      syslog(LOG_ERR, "PARSE receiver #%d: stream_setfmt: ioctl(fd, I_STR, PARSEIOC_SETFMT): %m", CL_UNIT(parse->unit));
      return 0;
    }
  return 1;
}

/*--------------------------------------------------
 * STREAM getstat
 */
static int
stream_getstat(parse, tcl)
     struct parseunit *parse;
     parsectl_t  *tcl;
{
  struct strioctl strioc;

  strioc.ic_cmd     = PARSEIOC_GETSTAT;
  strioc.ic_timout  = 0;
  strioc.ic_dp      = (char *)tcl;
  strioc.ic_len     = sizeof (*tcl);

  if (ioctl(parse->fd, I_STR, (caddr_t)&strioc) == -1)
    {
      syslog(LOG_ERR, "PARSE receiver #%d: stream_getstat: ioctl(fd, I_STR, PARSEIOC_GETSTAT): %m", CL_UNIT(parse->unit));
      return 0;
    }
  return 1;
}

/*--------------------------------------------------
 * STREAM setstat
 */
static int
stream_setstat(parse, tcl)
     struct parseunit *parse;
     parsectl_t  *tcl;
{
  struct strioctl strioc;

  strioc.ic_cmd     = PARSEIOC_SETSTAT;
  strioc.ic_timout  = 0;
  strioc.ic_dp      = (char *)tcl;
  strioc.ic_len     = sizeof (*tcl);

  if (ioctl(parse->fd, I_STR, (caddr_t)&strioc) == -1)
    {
      syslog(LOG_ERR, "PARSE receiver #%d: stream_setstat: ioctl(fd, I_STR, PARSEIOC_SETSTAT): %m", CL_UNIT(parse->unit));
      return 0;
    }
  return 1;
}

/*--------------------------------------------------
 * STREAM timecode
 */
static int
stream_timecode(parse, tcl)
     struct parseunit *parse;
     parsectl_t  *tcl;
{
  struct strioctl strioc;

  strioc.ic_cmd     = PARSEIOC_TIMECODE;
  strioc.ic_timout  = 0;
  strioc.ic_dp      = (char *)tcl;
  strioc.ic_len     = sizeof (*tcl);

  if (ioctl(parse->fd, I_STR, (caddr_t)&strioc) == -1)
    {
      syslog(LOG_ERR, "PARSE receiver #%d: parse_process: ioctl(fd, I_STR, PARSEIOC_TIMECODE): %m", CL_UNIT(parse->unit), parse->fd);
      return 0;
    }
  return 1;
}

/*--------------------------------------------------
 * STREAM receive
 */
static void
stream_receive(rbufp)
     struct recvbuf *rbufp;
{
  struct parseunit *parse = (struct parseunit *)rbufp->recv_srcclock;
  parsetime_t parsetime;

  if (rbufp->recv_length != sizeof(parsetime_t))
    {
      syslog(LOG_ERR,"PARSE receiver #%d: parse_receive: bad size (got %d expected %d)",
	     CL_UNIT(parse->unit), rbufp->recv_length, sizeof(parsetime_t));
      parse->baddata++;
      parse_event(parse, CEVNT_BADREPLY);
      return;
    }
  memmove((caddr_t)&parsetime,
	  (caddr_t)&rbufp->recv_space,
	  sizeof(parsetime_t));

  /*
   * switch time stamp world - be sure to normalize small usec field
   * errors.
   */

#define fix_ts(_X_) \
  if ((&(_X_))->tv.tv_usec >= 1000000)                \
    {                                                 \
      (&(_X_))->tv.tv_usec -= 1000000;                \
      (&(_X_))->tv.tv_sec  += 1;                      \
    }

#define cvt_ts(_X_, _Y_) \
  {                                                   \
    l_fp ts;                                          \
                                                      \
    fix_ts((_X_));                                    \
    if (!buftvtots((const char *)&(&(_X_))->tv, &ts)) \
      {                                               \
	syslog(LOG_ERR,"parse: stream_receive: timestamp conversion error (buftvtots) (%s) (%d.%06d) ", (_Y_), (&(_X_))->tv.tv_sec, (&(_X_))->tv.tv_usec);\
	return;                                       \
      }                                               \
    else                                              \
      {                                               \
	(&(_X_))->fp = ts;                            \
      }                                               \
  }

  if (PARSE_TIMECODE(parsetime.parse_state))
    {
      cvt_ts(parsetime.parse_time, "parse_time");
      cvt_ts(parsetime.parse_stime, "parse_stime");
    }

  if (PARSE_PPS(parsetime.parse_state))
    cvt_ts(parsetime.parse_ptime, "parse_ptime");

  parse_process(parse, &parsetime);
}

/*--------------------------------------------------
 * STREAM poll
 */
static void
stream_poll(parse)
     struct parseunit *parse;
{
  register int fd, i, rtc;
  fd_set fdmask;
  struct timeval timeout, starttime, curtime, selecttime;
  parsetime_t parsetime;

  /*
   * now we do the following:
   *    - read the first packet from the parse module  (OLD !!!)
   *    - read the second packet from the parse module (fresh)
   *    - compute values for xntp
   */

  FD_ZERO(&fdmask);
  fd = parse->fd;
  FD_SET(fd, &fdmask);
  timeout.tv_sec = 0;
  timeout.tv_usec = 500000;	/* 0.5 sec */

  if (parse->parse_type->cl_poll)
    {
      parse->parse_type->cl_poll(parse);
    }

  if (GETTIMEOFDAY(&starttime, 0L) == -1)
    {
      syslog(LOG_ERR,"gettimeofday failed: %m");
      exit(1);
    }

  selecttime = timeout;

  while ((rtc = select(fd + 1, &fdmask, 0, 0, &selecttime)) != 1)
    {
      /* no data from the radio clock */

      if (rtc == -1)
	{
	  if (errno == EINTR)
	    {
	      if (GETTIMEOFDAY(&curtime, 0L) == -1)
		{
		  syslog(LOG_ERR,"gettimeofday failed: %m");
		  exit(1);
		}
	      selecttime.tv_sec = curtime.tv_sec - starttime.tv_sec;
	      if (curtime.tv_usec < starttime.tv_usec)
		{
		  selecttime.tv_sec  -= 1;
		  selecttime.tv_usec  = 1000000 + curtime.tv_usec - starttime.tv_usec;
		}
	      else
		{
		  selecttime.tv_usec = curtime.tv_usec - starttime.tv_usec;
		}


	      if (timercmp(&selecttime, &timeout, >))
		{
		  /*
		   * elapsed real time passed timeout value - consider it timed out
		   */
		  break;
		}

	      /*
	       * calculate residual timeout value
	       */
	      selecttime.tv_sec = timeout.tv_sec - selecttime.tv_sec;

	      if (selecttime.tv_usec > timeout.tv_usec)
		{
		  selecttime.tv_sec -= 1;
		  selecttime.tv_usec = 1000000 + timeout.tv_usec - selecttime.tv_usec;
		}
	      else
		{
		  selecttime.tv_usec = timeout.tv_usec - selecttime.tv_usec;
		}

	      FD_SET(fd, &fdmask);
	      continue;
	    }
	  else
	    {
              syslog(LOG_WARNING, "PARSE receiver #%d: no data[old] from device (select() error: %m)", CL_UNIT(parse->unit));
	    }
	}
      else
	{
          syslog(LOG_WARNING, "PARSE receiver #%d: no data[old] from device", CL_UNIT(parse->unit));
	}
      parse->noresponse++;
      parse->lastmissed = current_time;
      parse_event(parse, CEVNT_TIMEOUT);

      return;
    }

  while (((i = read(fd, (char *)&parsetime, sizeof(parsetime))) < sizeof(parsetime)))
    {
      /* bad packet */
      if ( i == -1)
	{
	  if (errno == EINTR)
	    {
	      continue;
	    }
	  else
	    {
              syslog(LOG_WARNING, "PARSE receiver #%d: bad read[old] from streams module (read() error: %m)", CL_UNIT(parse->unit), i, sizeof(parsetime));
	    }
	}
      else
	{
          syslog(LOG_WARNING, "PARSE receiver #%d: bad read[old] from streams module (got %d bytes - expected %d bytes)", CL_UNIT(parse->unit), i, sizeof(parsetime));
	}
      parse->baddata++;
      parse_event(parse, CEVNT_BADREPLY);

      return;
    }

  if (parse->parse_type->cl_poll)
    {
      parse->parse_type->cl_poll(parse);
    }

  timeout.tv_sec = 1;
  timeout.tv_usec = 500000;	/* 1.500 sec */
  FD_ZERO(&fdmask);
  FD_SET(fd, &fdmask);

  if (GETTIMEOFDAY(&starttime, 0L) == -1)
    {
      syslog(LOG_ERR,"gettimeofday failed: %m");
      exit(1);
    }

  selecttime = timeout;

  while ((rtc = select(fd + 1, &fdmask, 0, 0, &selecttime)) != 1)
    {
      /* no data from the radio clock */

      if (rtc == -1)
	{
	  if (errno == EINTR)
	    {
	      if (GETTIMEOFDAY(&curtime, 0L) == -1)
		{
		  syslog(LOG_ERR,"gettimeofday failed: %m");
		  exit(1);
		}
	      selecttime.tv_sec = curtime.tv_sec - starttime.tv_sec;
	      if (curtime.tv_usec < starttime.tv_usec)
		{
		  selecttime.tv_sec  -= 1;
		  selecttime.tv_usec  = 1000000 + curtime.tv_usec - starttime.tv_usec;
		}
	      else
		{
		  selecttime.tv_usec = curtime.tv_usec - starttime.tv_usec;
		}


	      if (timercmp(&selecttime, &timeout, >))
		{
		  /*
		   * elapsed real time passed timeout value - consider it timed out
		   */
		  break;
		}

	      /*
	       * calculate residual timeout value
	       */
	      selecttime.tv_sec = timeout.tv_sec - selecttime.tv_sec;

	      if (selecttime.tv_usec > timeout.tv_usec)
		{
		  selecttime.tv_sec -= 1;
		  selecttime.tv_usec = 1000000 + timeout.tv_usec - selecttime.tv_usec;
		}
	      else
		{
		  selecttime.tv_usec = timeout.tv_usec - selecttime.tv_usec;
		}

	      FD_SET(fd, &fdmask);
	      continue;
	    }
	  else
	    {
              syslog(LOG_WARNING, "PARSE receiver #%d: no data[new] from device (select() error: %m)", CL_UNIT(parse->unit));
	    }
	}
      else
	{
          syslog(LOG_WARNING, "PARSE receiver #%d: no data[new] from device", CL_UNIT(parse->unit));
	}

      /*
       * we will return here iff we got a good old sample as this would
       * be misinterpreted. bad samples are passed on to be logged into the
       * state statistics
       */
      if ((parsetime.parse_status & CVT_MASK) == CVT_OK)
	{
	  parse->noresponse++;
	  parse->lastmissed = current_time;
	  parse_event(parse, CEVNT_TIMEOUT);
	  return;
	}
    }

  /*
   * we get here either by a possible read() (rtc == 1 - while assertion)
   * or by a timeout or a system call error. when a read() is possible we
   * get the new data, otherwise we stick with the old
   */
  if ((rtc == 1) && ((i = read(fd, (char *)&parsetime, sizeof(parsetime))) < sizeof(parsetime)))
    {
      /* bad packet */
      if ( i== -1)
	{
          syslog(LOG_WARNING, "PARSE receiver #%d: bad read[new] from streams module (read() error: %m)", CL_UNIT(parse->unit), i, sizeof(parsetime));
	}
      else
	{
          syslog(LOG_WARNING, "PARSE receiver #%d: bad read[new] from streams module (got %d bytes - expected %d bytes)", CL_UNIT(parse->unit), i, sizeof(parsetime));
	}
      parse->baddata++;
      parse_event(parse, CEVNT_BADREPLY);

      return;
    }

  /*
   * process what we got
   */
  parse_process(parse, &parsetime);
}
#endif

/*--------------------------------------------------
 * local init
 */
static int
local_init(parse)
     struct parseunit *parse;
{
  return parse_ioinit(&parse->parseio);
}

/*--------------------------------------------------
 * local end
 */
static void
local_end(parse)
     struct parseunit *parse;
{
  parse_ioend(&parse->parseio);
}


/*--------------------------------------------------
 * local nop
 */
static int
local_nop(parse)
     struct parseunit *parse;
{
  return 1;
}

/*--------------------------------------------------
 * local setcs
 */
static int
local_setcs(parse, tcl)
     struct parseunit *parse;
     parsectl_t  *tcl;
{
  return parse_setcs(tcl, &parse->parseio);
}

/*--------------------------------------------------
 * local getfmt
 */
static int
local_getfmt(parse, tcl)
     struct parseunit *parse;
     parsectl_t  *tcl;
{
  return parse_getfmt(tcl, &parse->parseio);
}

/*--------------------------------------------------
 * local setfmt
 */
static int
local_setfmt(parse, tcl)
     struct parseunit *parse;
     parsectl_t  *tcl;
{
  return parse_setfmt(tcl, &parse->parseio);
}

/*--------------------------------------------------
 * local getstat
 */
static int
local_getstat(parse, tcl)
     struct parseunit *parse;
     parsectl_t  *tcl;
{
  return parse_getstat(tcl, &parse->parseio);
}

/*--------------------------------------------------
 * local setstat
 */
static int
local_setstat(parse, tcl)
     struct parseunit *parse;
     parsectl_t  *tcl;
{
  return parse_setstat(tcl, &parse->parseio);
}

/*--------------------------------------------------
 * local timecode
 */
static int
local_timecode(parse, tcl)
     struct parseunit *parse;
     parsectl_t  *tcl;
{
  return parse_timecode(tcl, &parse->parseio);
}


/*--------------------------------------------------
 * local receive
 */
static void
local_receive(rbufp)
     struct recvbuf *rbufp;
{
  struct parseunit *parse = (struct parseunit *)rbufp->recv_srcclock;
  register int count;
  register char *s;
#ifdef FREEBSD_CONRAD
  struct timeval foo;
#endif

  /*
   * eat all characters, parsing then and feeding complete samples
   */
  count = rbufp->recv_length;
  s = rbufp->recv_buffer;
#ifdef FREEBSD_CONRAD
  ioctl(parse->fd,TIOCTIMESTAMP,&foo);
  TVTOTS(&foo, &rbufp->recv_time);
  rbufp->recv_time.l_uf += TS_ROUNDBIT;
  rbufp->recv_time.l_ui += JAN_1970;
  rbufp->recv_time.l_uf &= TS_MASK;
#endif

  while (count--)
    {
      if (parse_ioread(&parse->parseio, *s++, (timestamp_t *)&rbufp->recv_time))
	{
	  /*
	   * got something good to eat
	   */
#ifdef PPSPPS
	  if (!PARSE_PPS(parse->parseio.parse_dtime.parse_state) &&
	      (parse->flags & PARSE_PPSCLOCK))
	    {
	      l_fp ts;
	      struct ppsclockev ev;

	      if (ioctl(parse->fd, CIOGETEV, (caddr_t)&ev) == 0)
		{
		  if (ev.serial != parse->ppsserial)
		    {
		      /*
                       * add PPS time stamp if available via ppsclock module
		       * and not supplied already.
		       */
		      if (!buftvtots((const char *)&ev.tv, &ts))
			{
			  syslog(LOG_ERR,"parse: local_receive: timestamp conversion error (buftvtots) (ppsclockev.tv)");
			}
		      else
			{
		          parse->parseio.parse_dtime.parse_ptime.fp = ts;
			  parse->parseio.parse_dtime.parse_state |= PARSEB_PPS|PARSEB_S_PPS;
			}
		    }
		  parse->ppsserial = ev.serial;
	       }
	    }
#endif
	  parse_process(parse, &parse->parseio.parse_dtime);
	  parse_iodone(&parse->parseio);
	}
    }
}

/*--------------------------------------------------
 * local poll
 */
static void
local_poll(parse)
     struct parseunit *parse;
{
  register int fd, i, rtc;
  fd_set fdmask;
  struct timeval timeout, starttime, curtime, selecttime;
  static struct timeval null_time = { 0, 0};
  timestamp_t ts;

  FD_ZERO(&fdmask);
  fd = parse->fd;
  FD_SET(fd, &fdmask);
  timeout.tv_sec  = 1;
  timeout.tv_usec = 500000;	/* 1.5 sec */

  if (parse->parse_type->cl_poll)
    {
      parse->parse_type->cl_poll(parse);
    }

  if (GETTIMEOFDAY(&starttime, 0L) == -1)
    {
      syslog(LOG_ERR,"gettimeofday failed: %m");
      exit(1);
    }

  selecttime = timeout;

  do
    {
      while ((rtc = select(fd + 1, &fdmask, 0, 0, &selecttime)) != 1)
	{
	  /* no data from the radio clock */

	  if (rtc == -1)
	    {
	      if (errno == EINTR)
		{
		  if (GETTIMEOFDAY(&curtime, 0L) == -1)
		    {
		      syslog(LOG_ERR,"gettimeofday failed: %m");
		      exit(1);
		    }
		  selecttime.tv_sec = curtime.tv_sec - starttime.tv_sec;
		  if (curtime.tv_usec < starttime.tv_usec)
		    {
		      selecttime.tv_sec  -= 1;
		      selecttime.tv_usec  = 1000000 + curtime.tv_usec - starttime.tv_usec;
		    }
		  else
		    {
		      selecttime.tv_usec = curtime.tv_usec - starttime.tv_usec;
		    }


		  if (!timercmp(&selecttime, &timeout, >))
		    {
		      /*
		       * calculate residual timeout value
		       */
		      selecttime.tv_sec = timeout.tv_sec - selecttime.tv_sec;

		      if (selecttime.tv_usec > timeout.tv_usec)
			{
			  selecttime.tv_sec -= 1;
			  selecttime.tv_usec = 1000000 + timeout.tv_usec - selecttime.tv_usec;
			}
		      else
			{
			  selecttime.tv_usec = timeout.tv_usec - selecttime.tv_usec;
			}

		      FD_SET(fd, &fdmask);
		      continue;
		    }
		}
	      else
		{
		  syslog(LOG_WARNING, "PARSE receiver #%d: no data from device (select() error: %m)", CL_UNIT(parse->unit));
		}
	    }
	  else
	    {
	      syslog(LOG_WARNING, "PARSE receiver #%d: no data from device", CL_UNIT(parse->unit));
	    }

	  parse->noresponse++;
	  parse->lastmissed = current_time;
	  parse_event(parse, CEVNT_TIMEOUT);

	  return;
	}

      /*
       * at least 1 character is available - gobble everthing up that is available
       */
      do
	{
	  char inbuf[256];

	  register char *s = inbuf;

	  rtc = i = read(fd, inbuf, sizeof(inbuf));

	  get_systime(&ts.fp);

	  while (i-- > 0)
	    {
	      if (parse_ioread(&parse->parseio, *s++, &ts))
		{
		  /*
		   * got something good to eat
		   */
		  parse_process(parse, &parse->parseio.parse_dtime);
		  parse_iodone(&parse->parseio);
		  /*
		   * done if no more characters are available
		   */
		  FD_SET(fd, &fdmask);
		  if ((i == 0) &&
		      (select(fd + 1, &fdmask, 0, 0, &null_time) == 0))
		    return;
		}
	    }
	  FD_SET(fd, &fdmask);
	} while ((rtc = select(fd + 1, &fdmask, 0, 0, &null_time)) == 1);
      FD_SET(fd, &fdmask);
    } while (1);
}

/*--------------------------------------------------
 * init_iobinding - find and initialize lower layers
 */
static bind_t *
init_iobinding(parse)
     struct parseunit *parse;
{
  register bind_t *b = io_bindings;

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
parsestate(state, buffer)
  unsigned LONG state;
  char *buffer;
{
  static struct bits
    {
      unsigned LONG bit;
      char         *name;
    } flagstrings[] =
    {
      { PARSEB_ANNOUNCE, "DST SWITCH WARNING" },
      { PARSEB_POWERUP,  "NOT SYNCHRONIZED" },
      { PARSEB_NOSYNC,   "TIME CODE NOT CONFIRMED" },
      { PARSEB_DST,      "DST" },
      { PARSEB_UTC,      "UTC DISPLAY" },
      { PARSEB_LEAPADD,  "LEAP ADD WARNING" },
      { PARSEB_LEAPDEL,  "LEAP DELETE WARNING" },
      { PARSEB_LEAPSECOND, "LEAP SECOND" },
      { PARSEB_ALTERNATE,"ALTERNATE ANTENNA" },
      { PARSEB_TIMECODE, "TIME CODE" },
      { PARSEB_PPS,      "PPS" },
      { PARSEB_POSITION, "POSITION" },
      { 0 }
    };

  static struct sbits
    {
      unsigned LONG bit;
      char         *name;
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
      if (flagstrings[i].bit & state)
	{
	  if (buffer[0])
	    strcat(buffer, "; ");
	  strcat(buffer, flagstrings[i].name);
	}
      i++;
    }

  if (state & (PARSEB_S_LEAP|PARSEB_S_ANTENNA|PARSEB_S_PPS|PARSEB_S_POSITION))
    {
      register char *s, *t;

      if (buffer[0])
	strcat(buffer, "; ");

      strcat(buffer, "(");

      t = s = buffer + strlen(buffer);

      i = 0;
      while (sflagstrings[i].bit)
	{
	  if (sflagstrings[i].bit & state)
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
parsestatus(state, buffer)
  unsigned LONG state;
  char *buffer;
{
  static struct bits
    {
      unsigned LONG bit;
      char         *name;
    } flagstrings[] =
    {
      { CVT_OK,      "CONVERSION SUCCESSFUL" },
      { CVT_NONE,    "NO CONVERSION" },
      { CVT_FAIL,    "CONVERSION FAILED" },
      { CVT_BADFMT,  "ILLEGAL FORMAT" },
      { CVT_BADDATE, "DATE ILLEGAL" },
      { CVT_BADTIME, "TIME ILLEGAL" },
      { 0 }
    };
  int i;

  *buffer = '\0';

  i = 0;
  while (flagstrings[i].bit)
    {
      if (flagstrings[i].bit & state)
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
static char *
clockstatus(state)
  unsigned LONG state;
{
  static char buffer[20];
  static struct status
    {
      unsigned LONG value;
      char         *name;
    } flagstrings[] =
    {
      { CEVNT_NOMINAL, "NOMINAL" },
      { CEVNT_TIMEOUT, "NO RESPONSE" },
      { CEVNT_BADREPLY,"BAD FORMAT" },
      { CEVNT_FAULT,   "FAULT" },
      { CEVNT_PROP,    "PROPAGATION DELAY" },
      { CEVNT_BADDATE, "ILLEGAL DATE" },
      { CEVNT_BADTIME, "ILLEGAL TIME" },
      { ~0 }
    };
  int i;

  i = 0;
  while (flagstrings[i].value != ~0)
    {
      if (flagstrings[i].value == state)
	{
	  return flagstrings[i].name;
	}
      i++;
    }

  sprintf(buffer, "unknown #%d", state);

  return buffer;
}

/*--------------------------------------------------
 * mkascii - make a printable ascii string
 * assumes (unless defined better) 7-bit ASCII
 */
#ifndef isprint
#define isprint(_X_) (((_X_) > 0x1F) && ((_X_) < 0x7F))
#endif

static char *
mkascii(buffer, blen, src, srclen)
  register char  *buffer;
  register LONG  blen;
  register char  *src;
  register LONG  srclen;
{
  register char *b    = buffer;
  register char *endb = (char *)0;

  if (blen < 4)
    return (char *)0;		/* don't bother with mini buffers */

  endb = buffer + blen - 4;

  blen--;			/* account for '\0' */

  while (blen && srclen--)
    {
      if ((*src != '\\') && isprint(*src))
	{			/* printables are easy... */
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
 * l_mktime - make representation of a relative time
 */
static char *
l_mktime(delta)
  unsigned LONG delta;
{
  unsigned LONG tmp, m, s;
  static char buffer[40];

  buffer[0] = '\0';

  if ((tmp = delta / (60*60*24)) != 0)
    {
      sprintf(buffer, "%dd+", tmp);
      delta -= tmp * 60*60*24;
    }

  s = delta % 60;
  delta /= 60;
  m = delta % 60;
  delta /= 60;

  sprintf(buffer+strlen(buffer), "%02d:%02d:%02d",
	  delta, m, s);

  return buffer;
}


/*--------------------------------------------------
 * parse_statistics - list summary of clock states
 */
static void
parse_statistics(parse)
  register struct parseunit *parse;
{
  register int i;

  syslog(LOG_INFO, "PARSE receiver #%d: running time: %s",
	 CL_UNIT(parse->unit),
	 l_mktime(current_time - parse->timestarted));

  syslog(LOG_INFO, "PARSE receiver #%d: current status: %s",
	 CL_UNIT(parse->unit),
	 clockstatus(parse->status));

  for (i = 0; i <= CEVNT_MAX; i++)
    {
      register unsigned LONG stime;
      register unsigned LONG percent, div = current_time - parse->timestarted;

      percent = stime = PARSE_STATETIME(parse, i);

      while (((unsigned LONG)(~0) / 10000) < percent)
	{
	  percent /= 10;
	  div     /= 10;
	}

      if (div)
	percent = (percent * 10000) / div;
      else
	percent = 10000;

      if (stime)
	syslog(LOG_INFO, "PARSE receiver #%d: state %18s: %13s (%3d.%02d%%)",
	       CL_UNIT(parse->unit),
	       clockstatus(i),
	       l_mktime(stime),
	       percent / 100, percent % 100);
    }
}

/*--------------------------------------------------
 * cparse_statistics - wrapper for statistics call
 */
static void
cparse_statistics(peer)
  register struct peer *peer;
{
  register struct parseunit *parse = (struct parseunit *)peer;

  parse_statistics(parse);
  parse->stattimer.event_time    = current_time + PARSESTATISTICS;
  TIMER_ENQUEUE(timerqueue, &parse->stattimer);
}

/**===========================================================================
 ** xntp interface routines
 **/

/*--------------------------------------------------
 * parse_init - initialize internal parse driver data
 */
static void
parse_init()
{
  memset((caddr_t)parseunits, 0, sizeof parseunits);
}


/*--------------------------------------------------
 * parse_shutdown - shut down a PARSE clock
 */
static void
parse_shutdown(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct parseunit *parse;

	unit = CL_UNIT(unit);

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR,
		  "PARSE receiver #%d: parse_shutdown: INTERNAL ERROR, unit invalid (max %d)",
		    unit,MAXUNITS);
		return;
	}

	parse = parseunits[unit];

	if (parse && !parse->peer) {
		syslog(LOG_ERR,
		 "PARSE receiver #%d: parse_shutdown: INTERNAL ERROR, unit not in use", unit);
		return;
	}

	/*
	 * print statistics a last time and
	 * stop statistics machine
	 */
	parse_statistics(parse);
	TIMER_DEQUEUE(&parse->stattimer);

#if PPSPPS
	{
	  /*
	   * kill possible PPS association
	   */
	  if (fdpps == parse->fd)
	    fdpps = -1;
	}
#endif

	if (parse->parse_type->cl_end)
	  {
	    parse->parse_type->cl_end(parse);
	  }

	if (parse->binding)
	  PARSE_END(parse);

	/*
	 * Tell the I/O module to turn us off.  We're history.
	 */
	if (!parse->pollonly)
	  io_closeclock(&parse->io);
	else
	  (void) close(parse->fd);

	syslog(LOG_INFO, "PARSE receiver #%d: reference clock \"%s\" removed",
	       CL_UNIT(parse->unit), parse->parse_type->cl_description);

	parse->peer = (struct peer *)0; /* unused now */
}

/*--------------------------------------------------
 * parse_start - open the PARSE devices and initialize data for processing
 */
static int
parse_start(sysunit, peer)
	int sysunit;
	struct peer *peer;
{
  u_int unit;
  int fd232, i;
#ifdef HAVE_TERMIOS
  struct termios tm;		/* NEEDED FOR A LONG TIME ! */
#endif
#ifdef HAVE_SYSV_TTYS
  struct termio tm;		/* NEEDED FOR A LONG TIME ! */
#endif
  struct parseunit * parse;
  char parsedev[sizeof(PARSEDEVICE)+20];
  parsectl_t tmp_ctl;
  u_int type;

  type = CL_TYPE(sysunit);
  unit = CL_UNIT(sysunit);

  if (unit >= MAXUNITS)
    {
      syslog(LOG_ERR, "PARSE receiver #%d: parse_start: unit number invalid (max %d)",
	     unit, MAXUNITS-1);
      return 0;
    }

  if ((type == ~0) || (clockinfo[type].cl_description == (char *)0))
    {
      syslog(LOG_ERR, "PARSE receiver #%d: parse_start: unsupported clock type %d (max %d)",
	     unit, CL_REALTYPE(sysunit), ncltypes-1);
      return 0;
    }

  if (parseunits[unit] && parseunits[unit]->peer)
    {
      syslog(LOG_ERR, "PARSE receiver #%d: parse_start: unit in use", unit);
      return 0;
    }

  /*
   * Unit okay, attempt to open the device.
   */
  (void) sprintf(parsedev, PARSEDEVICE, unit);

#if defined(SYS_FREEBSD) && defined(BOEDER)
  fd232 = open(parsedev, O_RDONLY | O_NONBLOCK, 0777);
#else
#ifndef O_NOCTTY
#define O_NOCTTY 0
#endif

  fd232 = open(parsedev, O_RDWR|O_NOCTTY, 0777);
#endif
  if (fd232 == -1)
    {
      syslog(LOG_ERR, "PARSE receiver #%d: parse_start: open of %s failed: %m", unit, parsedev);
      return 0;
    }

  /*
   * Looks like this might succeed.  Find memory for the structure.
   * Look to see if there are any unused ones, if not we malloc()
   * one.
   */
  if (parseunits[unit])
    {
      parse = parseunits[unit];	/* The one we want is okay - and free */
    }
  else
    {
      for (i = 0; i < MAXUNITS; i++)
	{
	  if (parseunits[i] && !parseunits[i]->peer)
	    break;
	}
      if (i < MAXUNITS)
	{
	  /*
	   * Reclaim this one
	   */
	  parse = parseunits[i];
	  parseunits[i] = (struct parseunit *)0;
	}
      else
	{
	  parse = (struct parseunit *)
	    emalloc(sizeof(struct parseunit));
	}
    }

  memset((char *)parse, 0, sizeof(struct parseunit));
  parseunits[unit] = parse;

  /*
   * Set up the structures
   */
  parse->unit           = (u_char)sysunit;
  parse->timestarted    = current_time;
  parse->lastchange     = current_time;
  /*
   * we want to filter input for the sake of
   * getting an impression on dispersion
   * also we like to average the median range
   */
  parse->flags          = PARSE_STAT_FILTER|PARSE_STAT_AVG;
  parse->pollneeddata   = 0;
  parse->pollonly       = 1;	/* go for default polling mode */
  parse->lastformat     = ~0;	/* assume no format known */
  parse->status	        = CEVNT_TIMEOUT; /* expect the worst */
  parse->laststatus     = ~0;	/* be sure to mark initial status change */
  parse->nosynctime     = 0;	/* assume clock reasonable */
  parse->lastmissed     = 0;	/* assume got everything */
  parse->ppsserial      = 0;
  parse->localdata      = (void *)0;

  parse->parse_type     = &clockinfo[type];

  parse->basedelay.l_ui = 0;	/* we can only pre-configure delays less than 1 second */
  parse->basedelay.l_uf = parse->parse_type->cl_basedelay;

  parse->ppsdelay.l_ui  = 0;	/* we can only pre-configure delays less than 1 second */
  parse->ppsdelay.l_uf  = parse->parse_type->cl_ppsdelay;

  peer->rootdelay       = parse->parse_type->cl_rootdelay;
  peer->sstclktype      = parse->parse_type->cl_type;
  peer->precision       = sys_precision;
  peer->stratum         = STRATUM_REFCLOCK;
  if (peer->stratum <= 1)
    memmove((char *)&peer->refid, parse->parse_type->cl_id, 4);
  else
    peer->refid = htonl(PARSEHSREFID);

  parse->fd = fd232;

  parse->peer = peer;		/* marks it also as busy */

  parse->binding = init_iobinding(parse);

  if (parse->binding == (bind_t *)0)
    {
      syslog(LOG_ERR, "PARSE receiver #%d: parse_start: io sub system initialisation failed.");
      parse_shutdown(parse->unit, peer); /* let our cleaning staff do the work */
      return 0;		/* well, ok - special initialisation broke */
    }

  /*
   * configure terminal line
   */
  if (TTY_GETATTR(fd232, &tm) == -1)
    {
      syslog(LOG_ERR, "PARSE receiver #%d: parse_start: tcgetattr(%d, &tm): %m", unit, fd232);
      parse_shutdown(parse->unit, peer); /* let our cleaning staff do the work */
      return 0;
    }
  else
    {
#ifndef _PC_VDISABLE
      memset((char *)tm.c_cc, 0, sizeof(tm.c_cc));
#else
      int disablec;
      errno = 0;	/* pathconf can deliver -1 without changing errno ! */

      disablec = fpathconf(parse->fd, _PC_VDISABLE);
      if (disablec == -1 && errno)
	{
          syslog(LOG_ERR, "PARSE receiver #%d: parse_start: fpathconf(fd, _PC_VDISABLE): %m", CL_UNIT(parse->unit));
          memset((char *)tm.c_cc, 0, sizeof(tm.c_cc)); /* best guess */
	}
      else
	if (disablec != -1)
	  memset((char *)tm.c_cc, disablec, sizeof(tm.c_cc));
#endif

      tm.c_cflag     = clockinfo[type].cl_cflag;
      tm.c_iflag     = clockinfo[type].cl_iflag;
      tm.c_oflag     = clockinfo[type].cl_oflag;
      tm.c_lflag     = clockinfo[type].cl_lflag;
#if defined(SYS_FREEBSD) && (defined(BOEDER) || defined(FREEBSD_CONRAD))
      if (cfsetspeed(&tm, B50) == -1)
	{
	  syslog(LOG_ERR,
		 "PARSE receiver #%d: parse_start: cfsetspeed(&tm, B50): %m",
		 unit);
	  parse_shutdown(parse->unit, peer); /* let our cleaning staff do the work */
	  return 0;
	}
#endif
      if (TTY_SETATTR(fd232, &tm) == -1)
	{
	  syslog(LOG_ERR, "PARSE receiver #%d: parse_start: tcsetattr(%d, &tm): %m", unit, fd232);
	  parse_shutdown(parse->unit, peer); /* let our cleaning staff do the work */
	  return 0;
	}
    }

  /*
   * as we always(?) get 8 bit chars we want to be
   * sure, that the upper bits are zero for less
   * than 8 bit I/O - so we pass that information on.
   * note that there can be only one bit count format
   * per file descriptor
   */

  switch (tm.c_cflag & CSIZE)
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
      syslog(LOG_ERR, "PARSE receiver #%d: parse_start: parse_setcs() FAILED.", unit);
      parse_shutdown(parse->unit, peer); /* let our cleaning staff do the work */
      return 0;		/* well, ok - special initialisation broke */
    }

#ifdef FREEBSD_CONRAD
      {
	int i,j;
	struct timeval tv;
	ioctl(parse->fd,TIOCTIMESTAMP,&tv);
	j = TIOCM_RTS;
	i = ioctl(fd232, TIOCMBIC, &j);
	if (i < 0) {
	  syslog(LOG_ERR,
	    "PARSE receiver #%d: lowrts_poll: failed to lower RTS: %m",
	    CL_UNIT(parse->unit));
	}
      }
#endif
#if defined(SYS_FREEBSD) && defined(BOEDER)
  if (fcntl(fd232, F_SETFL, fcntl(fd232, F_GETFL, 0) & ~O_NONBLOCK) == -1)
    {
      syslog(LOG_ERR,
	     "PARSE receiver #%d: parse_start: fcntl(%d, F_SETFL, ...): %m",
	     unit, fd232);
      parse_shutdown(parse->unit, peer); /* let our cleaning staff do the work */
      return 0;
    }

  if (ioctl(fd232, TIOCCDTR, 0) == -1)
    {
      syslog(LOG_ERR,
	     "PARSE receiver #%d: parse_start: ioctl(%d, TIOCCDTR, 0): %m",
	     unit, fd232);
      parse_shutdown(parse->unit, peer); /* let our cleaning staff do the work */
      return 0;
    }
#endif

  strcpy(tmp_ctl.parseformat.parse_buffer, parse->parse_type->cl_format);
  tmp_ctl.parseformat.parse_count = strlen(tmp_ctl.parseformat.parse_buffer);

  if (!PARSE_SETFMT(parse, &tmp_ctl))
    {
      syslog(LOG_ERR, "PARSE receiver #%d: parse_start: parse_setfmt() FAILED.", unit);
      parse_shutdown(parse->unit, peer); /* let our cleaning staff do the work */
      return 0;		/* well, ok - special initialisation broke */
    }

#ifdef TCFLSH
  /*
   * get rid of all IO accumulated so far
   */
  {
#ifndef TCIOFLUSH
#define TCIOFLUSH 2
#endif
    int flshcmd = TCIOFLUSH;

    (void) ioctl(parse->fd, TCFLSH, (caddr_t)&flshcmd);
  }
#endif

  tmp_ctl.parsestatus.flags = parse->flags & PARSE_STAT_FLAGS;

  if (!PARSE_SETSTAT(parse, &tmp_ctl))
    {
      syslog(LOG_ERR, "PARSE receiver #%d: parse_start: parse_setstat() FAILED.", unit);
      parse_shutdown(parse->unit, peer); /* let our cleaning staff do the work */
      return 0;		/* well, ok - special initialisation broke */
    }

  /*
   * try to do any special initializations
   */
  if (parse->parse_type->cl_init)
    {
      if (parse->parse_type->cl_init(parse))
	{
	  parse_shutdown(parse->unit, peer); /* let our cleaning staff do the work */
	  return 0;		/* well, ok - special initialisation broke */
	}
    }

  if (!(parse->parse_type->cl_flags & PARSE_F_POLLONLY) &&
      (CL_PPS(parse->unit) || (parse->parse_type->cl_flags & PARSE_F_NOPOLLONLY)))
    {
      /*
       * Insert in async io device list.
       */
      parse->io.clock_recv = parse->binding->bd_receive; /* pick correct receive routine */
      parse->io.srcclock = (caddr_t)parse;
      parse->io.datalen = 0;
      parse->io.fd = parse->fd;	/* replicated, but what the heck */
      if (!io_addclock(&parse->io))
	{
	  if (parse->parse_type->cl_flags & PARSE_F_NOPOLLONLY)
	    {
	      syslog(LOG_ERR,
		     "PARSE receiver #%d: parse_start: addclock %s fails (ABORT - clock type requires async io)", CL_UNIT(parse->unit), parsedev);
	      parse_shutdown(parse->unit, peer); /* let our cleaning staff do the work */
	      return 0;
	    }
	  else
	    {
	      syslog(LOG_ERR,
		     "PARSE receiver #%d: parse_start: addclock %s fails (switching to polling mode)", CL_UNIT(parse->unit), parsedev);
	    }
	}
      else
	{
	  parse->pollonly = 0;	/*
				 * update at receipt of time_stamp - also
				 * supports PPS processing
				 */
	}
    }

#ifdef PPSPPS
  if (parse->pollonly || (parse->parse_type->cl_flags & PARSE_F_PPSPPS))
    {
      if (fdpps == -1)
	{
	  fdpps = parse->fd;
	  if (!PARSE_DISABLE(parse))
	    {
	      syslog(LOG_ERR, "PARSE receiver #%d: parse_start: parse_disable() FAILED", CL_UNIT(parse->unit));
	      parse_shutdown(parse->unit, peer); /* let our cleaning staff do the work */
	      return 0;
	    }
	}
      else
	{
	  syslog(LOG_NOTICE, "PARSE receiver #%d: parse_start: loopfilter PPS already active - no PPS via CIOGETEV", CL_UNIT(parse->unit));
	}
    }
#endif

  /*
   * wind up statistics timer
   */
  parse->stattimer.peer = (struct peer *)parse; /* we know better, but what the heck */
  parse->stattimer.event_handler = cparse_statistics;
  parse->stattimer.event_time    = current_time + PARSESTATISTICS;
  TIMER_ENQUEUE(timerqueue, &parse->stattimer);

  /*
   * get out Copyright information once
   */
  if (!notice)
    {
      syslog(LOG_INFO, "NTP PARSE support: Copyright (c) 1989-1993, Frank Kardel");
      notice = 1;
    }

  /*
   * print out configuration
   */
  syslog(LOG_INFO, "PARSE receiver #%d: reference clock \"%s\" (device %s) added",
	 CL_UNIT(parse->unit),
	 parse->parse_type->cl_description, parsedev);

  syslog(LOG_INFO, "PARSE receiver #%d:  Stratum %d, %sPPS support, trust time %s, precision %d",
	 CL_UNIT(parse->unit),
	 parse->peer->stratum, (parse->pollonly || !CL_PPS(parse->unit)) ? "no " : "",
	 l_mktime(parse->parse_type->cl_maxunsync), parse->peer->precision);

  syslog(LOG_INFO, "PARSE receiver #%d:  rootdelay %s s, phaseadjust %s s, %s IO handling",
	 CL_UNIT(parse->unit),
	 ufptoa(parse->parse_type->cl_rootdelay, 6),
	 lfptoa(&parse->basedelay, 8),
	 parse->binding->bd_description);

  syslog(LOG_INFO, "PARSE receiver #%d:  Format recognition: %s", CL_UNIT(parse->unit),
	 !(*parse->parse_type->cl_format) ? "<AUTOMATIC>" : parse->parse_type->cl_format);

#ifdef PPSPPS
  syslog(LOG_INFO, "PARSE receiver #%d: %sCD PPS support",
	 CL_UNIT(parse->unit),
	 (fdpps == parse->fd) ? "" : "NO ");
#endif

  return 1;
}

/*--------------------------------------------------
 * parse_poll - called by the transmit procedure
 */
static void
parse_poll(unit, peer)
	int unit;
	struct peer *peer;
{
  register struct parseunit *parse;

  unit = CL_UNIT(unit);

  if (unit >= MAXUNITS)
    {
      syslog(LOG_ERR, "PARSE receiver #%d: poll: INTERNAL: unit invalid",
	     unit);
      return;
    }

  parse = parseunits[unit];

  if (!parse->peer)
    {
      syslog(LOG_ERR, "PARSE receiver #%d: poll: INTERNAL: unit unused",
	     unit);
      return;
    }

  if (peer != parse->peer)
    {
      syslog(LOG_ERR,
	     "PARSE receiver #%d: poll: INTERNAL: peer incorrect",
	     unit);
      return;
    }

  /*
   * Update clock stat counters
   */
  parse->polls++;

  /*
   * in PPS mode we just mark that we want the next sample
   * for the clock filter
   */
  if (!parse->pollonly)
    {
      if (parse->pollneeddata)
	{
	  /*
	   * bad news - didn't get a response last time
	   */
	  parse->noresponse++;
	  parse->lastmissed = current_time;
	  parse_event(parse, CEVNT_TIMEOUT);

          syslog(LOG_WARNING, "PARSE receiver #%d: no data from device within poll interval", CL_UNIT(parse->unit));
	}
      parse->pollneeddata = 1;
      if (parse->parse_type->cl_poll)
	{
	  parse->parse_type->cl_poll(parse);
	}
      return;
    }

  /*
   * the following code is only executed only when polling is used
   */

  PARSE_POLL(parse);
}

/*--------------------------------------------------
 * parse_leap - called when a leap second occurs
 */

static void
parse_leap()
{
	/*
	 * PARSE encodes the LEAP correction direction.
	 * For timecodes that do not pass on the leap correction direction
	 * the default PARSEB_LEAPADD must be used. It may then be modified
	 * with a fudge flag (flag2).
	 */
}


/*--------------------------------------------------
 * parse_control - set fudge factors, return statistics
 */
static void
parse_control(unit, in, out)
  int unit;
  struct refclockstat *in;
  struct refclockstat *out;
{
  register struct parseunit *parse;
  parsectl_t tmpctl;
  unsigned LONG type;
  static char outstatus[400];	/* status output buffer */

  type = CL_TYPE(unit);
  unit = CL_UNIT(unit);

  if (out)
    {
      out->lencode       = 0;
      out->lastcode      = 0;
      out->polls         = out->noresponse = 0;
      out->badformat     = out->baddata    = 0;
      out->timereset     = 0;
      out->currentstatus = out->lastevent = CEVNT_NOMINAL;
      out->kv_list       = (struct ctl_var *)0;
    }

  if (unit >= MAXUNITS)
    {
      syslog(LOG_ERR, "PARSE receiver #%d: parse_control: unit invalid (max %d)",
	     unit, MAXUNITS-1);
      return;
    }

  parse = parseunits[unit];

  if (!parse || !parse->peer)
    {
      syslog(LOG_ERR, "PARSE receiver #%d: parse_control: unit invalid (UNIT INACTIVE)",
	     unit);
      return;
    }

  if (in)
    {
      if (in->haveflags & CLK_HAVETIME1)
	parse->basedelay = in->fudgetime1;

      if (in->haveflags & CLK_HAVETIME2)
	{
	  parse->ppsdelay = in->fudgetime2;
	}

      if (in->haveflags & CLK_HAVEVAL1)
	{
	  parse->peer->stratum = (u_char)(in->fudgeval1 & 0xf);
	  if (parse->peer->stratum <= 1)
		memmove((char *)&parse->peer->refid,
			parse->parse_type->cl_id,
			4);
	      else
		parse->peer->refid = htonl(PARSEHSREFID);
	}

      /*
       * NOT USED - yet
       *
      if (in->haveflags & CLK_HAVEVAL2)
	{
	}
       */
      if (in->haveflags & (CLK_HAVEFLAG1|CLK_HAVEFLAG2|CLK_HAVEFLAG3|CLK_HAVEFLAG4))
	{
	  parse->flags = (in->flags & (CLK_FLAG1|CLK_FLAG2|CLK_FLAG3|CLK_FLAG4)) |
			 (parse->flags & ~PARSE_STAT_FLAGS);
	}

      if (in->haveflags & (CLK_HAVEVAL2|CLK_HAVETIME2|CLK_HAVEFLAG1|CLK_HAVEFLAG2|CLK_HAVEFLAG3|CLK_HAVEFLAG4))
	{
	  parsectl_t tmpctl;
	  tmpctl.parsestatus.flags = parse->flags & PARSE_STAT_FLAGS;

	  if (!PARSE_SETSTAT(parse, &tmpctl))
	    {
	      syslog(LOG_ERR, "PARSE receiver #%d: parse_control: parse_setstat() FAILED", unit);
	    }
	}
    }

  if (out)
    {
      register unsigned LONG sum = 0;
      register char *t, *tt;
      register struct tm *tm;
      register short utcoff;
      register char sign;
      register int i;
      time_t tim;

      outstatus[0] = '\0';

      out->haveflags = CLK_HAVETIME1|CLK_HAVETIME2|CLK_HAVEVAL1|CLK_HAVEFLAG1|CLK_HAVEFLAG2|CLK_HAVEFLAG3;
      out->clockdesc = parse->parse_type->cl_description;

      out->fudgetime1 = parse->basedelay;

      out->fudgetime2 = parse->ppsdelay;

      out->fudgeval1 = (LONG)parse->peer->stratum;

      out->fudgeval2 = 0;

      out->flags     = parse->flags & PARSE_STAT_FLAGS;

      out->type      = REFCLK_PARSE;

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
	       tt = add_var(&out->kv_list, 40, RO);
	       sprintf(tt, "refclock_ppsskew=%s", lfptoms(&off, 6));
	     }
	 }

      if (PARSE_PPS(parse->time.parse_state))
	{
	  tt = add_var(&out->kv_list, 80, RO|DEF);
	  sprintf(tt, "refclock_ppstime=\"%s\"", prettydate(&parse->time.parse_ptime.fp));
	}

      /*
       * all this for just finding out the +-xxxx part (there are always
       * new and changing fields in the standards 8-().
       *
       * but we do it for the human user...
       */
      tim  = parse->time.parse_time.fp.l_ui - JAN_1970;
      tm = gmtime(&tim);
      utcoff = tm->tm_hour * 60 + tm->tm_min;
      tm = localtime(&tim);
      utcoff = tm->tm_hour * 60 + tm->tm_min - utcoff + 12 * 60;
      utcoff += 24 * 60;
      utcoff %= 24 * 60;
      utcoff -= 12 * 60;
      if (utcoff < 0)
	{
	  utcoff = -utcoff;
	  sign = '-';
	}
      else
	{
	  sign = '+';
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
	  strcpy(tt, prettydate(&parse->time.parse_time.fp));
	  t = tt + strlen(tt);

	  sprintf(t, " (%c%02d%02d)\"", sign, utcoff / 60, utcoff % 60);
	}

      if (!PARSE_GETTIMECODE(parse, &tmpctl))
	{
	  syslog(LOG_ERR, "PARSE receiver #%d: parse_control: parse_timecode() FAILED", unit);
	}
      else
	{
	  tt = add_var(&out->kv_list, 128, RO|DEF);
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
	    mkascii(outstatus+strlen(outstatus), sizeof(outstatus)- strlen(outstatus) - 1,
		    tmpctl.parsegettc.parse_buffer, tmpctl.parsegettc.parse_count - 1);

	  parse->badformat += tmpctl.parsegettc.parse_badformat;
	}

      tmpctl.parseformat.parse_format = tmpctl.parsegettc.parse_format;

      if (!PARSE_GETFMT(parse, &tmpctl))
	{
	  syslog (LOG_ERR, "PARSE receiver #%d: parse_control: parse_getfmt() FAILED", unit);
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

      tt = add_var(&out->kv_list, 200, RO|DEF);
      strcpy(tt, "refclock_states=\"");
      tt += strlen(tt);

      for (i = 0; i <= CEVNT_MAX; i++)
	{
	  register unsigned LONG stime;
	  register unsigned LONG div = current_time - parse->timestarted;
	  register unsigned LONG percent;

	  percent = stime = PARSE_STATETIME(parse, i);

	  while (((unsigned LONG)(~0) / 10000) < percent)
	    {
	      percent /= 10;
	      div     /= 10;
	    }

	  if (div)
	    percent = (percent * 10000) / div;
	  else
	    percent = 10000;

	  if (stime)
	    {
	      sprintf(tt, "%s%s%s: %s (%d.%02d%%)",
		      sum ? "; " : "",
                      (parse->status == i) ? "*" : "",
		      clockstatus(i),
		      l_mktime(stime),
		      percent / 100, percent % 100);
	      sum += stime;
	      tt  += strlen(tt);
	    }
	}

      sprintf(tt, "; running time: %s\"", l_mktime(sum));

      tt = add_var(&out->kv_list, 32, RO);
      sprintf(tt, "refclock_id=\"%s\"", parse->parse_type->cl_id);

      tt = add_var(&out->kv_list, 80, RO);
      sprintf(tt, "refclock_iomode=\"%s\"", parse->binding->bd_description);

      tt = add_var(&out->kv_list, 128, RO);
      sprintf(tt, "refclock_driver_version=\"refclock_parse.c,v 3.53 1994/03/25 13:07:39 kardel Exp\"");

      out->lencode       = strlen(outstatus);
      out->lastcode      = outstatus;
      out->timereset     = parse->timestarted;
      out->polls         = parse->polls;
      out->noresponse    = parse->noresponse;
      out->badformat     = parse->badformat;
      out->baddata       = parse->baddata;
      out->lastevent     = parse->lastevent;
      out->currentstatus = parse->status;
    }
}

/**===========================================================================
 ** processing routines
 **/

/*--------------------------------------------------
 * event handling - note that nominal events will also be posted
 */
static void
parse_event(parse, event)
  struct parseunit *parse;
  int event;
{
  if (parse->status != (u_char) event)
    {
      parse->statetime[parse->status] += current_time - parse->lastchange;
      parse->lastchange              = current_time;

      parse->status    = (u_char)event;
      if (event != CEVNT_NOMINAL)
        parse->lastevent = parse->status;

      report_event(EVNT_PEERCLOCK, parse->peer);
    }
}

/*--------------------------------------------------
 * process a PARSE time sample
 */
static void
parse_process(parse, parsetime)
  struct parseunit *parse;
  parsetime_t      *parsetime;
{
  unsigned char leap;
  struct timeval usecdisp;
  l_fp off, rectime, reftime, dispersion;

  /*
   * check for changes in conversion status
   * (only one for each new status !)
   */
  if (parse->laststatus != parsetime->parse_status)
    {
      char buffer[200];

      syslog(LOG_WARNING, "PARSE receiver #%d: conversion status \"%s\"",
	     CL_UNIT(parse->unit), parsestatus(parsetime->parse_status, buffer));

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
	      syslog(LOG_ERR, "PARSE receiver #%d: parse_process: parse_timecode() FAILED", CL_UNIT(parse->unit));
	    }
	  else
	    {
	      syslog(LOG_WARNING, "PARSE receiver #%d: FAILED TIMECODE: \"%s\"",
		     CL_UNIT(parse->unit), mkascii(buffer, sizeof buffer, tmpctl.parsegettc.parse_buffer, tmpctl.parsegettc.parse_count - 1));
	      parse->badformat += tmpctl.parsegettc.parse_badformat;
	    }
	}

      parse->laststatus = parsetime->parse_status;
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
	  break;		/* well, still waiting - timeout is handled at higher levels */

	case CVT_FAIL:
	  parse->badformat++;
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
	  syslog(LOG_ERR, "PARSE receiver #%d: parse_getfmt() FAILED", CL_UNIT(parse->unit));
	}
      else
	{
	  syslog(LOG_INFO, "PARSE receiver #%d: new packet format \"%s\"",
		 CL_UNIT(parse->unit), tmpctl.parseformat.parse_buffer);
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

      syslog(LOG_INFO,"PARSE receiver #%d: STATE CHANGE: %s -> %s",
	     CL_UNIT(parse->unit), tmp2, tmp1);
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
       * for PARSE U/A 31 the lost synchronisation ist true
       * as it is the powerup state and the time is taken
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
      if (parse->nosynctime)
	{
	  /*
	   * repeated POWERUP/NOSYNC state - look whether
	   * the message should be repeated
	   */
	  if (current_time - parse->nosynctime > PARSENOSYNCREPEAT)
	    {
	      syslog(LOG_ERR,"PARSE receiver #%d: *STILL* NOT SYNCHRONIZED (POWERUP or no PZF signal)",
		     CL_UNIT(parse->unit));
	      parse->nosynctime = current_time;
	    }
	}
      else
	{
	  syslog(LOG_ERR,"PARSE receiver #%d: NOT SYNCHRONIZED",
		 CL_UNIT(parse->unit));
          parse->nosynctime = current_time;
	}
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
       *  for PARSE U/A 31 this is bad news (clock running without timecode
       *  confirmation)
       *  PZF 535 has also no time confirmation, but the phase should be
       *  very precise as the PZF signal can be decoded
       */
      parse->nosynctime = 0;	/* current state is better than worst state */

      if (PARSE_SYNC(parsetime->parse_state))
	{
	  /*
	   * currently completely synchronized - best possible state
	   */
	  parse->lastsync = current_time;
	  /*
	   * log OK status
	   */
	  parse_event(parse, CEVNT_NOMINAL);
	}
      else
	{
	  /*
	   * we have had some problems receiving the time code
	   */
	  parse_event(parse, CEVNT_PROP);
	}
    }

  if (PARSE_TIMECODE(parsetime->parse_state))
    {
      l_fp offset;

      /*
       * calculate time offset including systematic delays
       * off = PARSE-timestamp + propagation delay - kernel time stamp
       */
      offset = parse->basedelay;

      off = parsetime->parse_time.fp;

      reftime = off;

      L_ADD(&off, &offset);
      rectime = off;		/* this makes org time and xmt time somewhat artificial */

      L_SUB(&off, &parsetime->parse_stime.fp);

      if ((parse->flags & PARSE_STAT_FILTER) &&
	  (off.l_i > -60) &&
	  (off.l_i <  60))				/* take usec error only if within +- 60 secs */
	{
	  struct timeval usecerror;
	  /*
	   * offset is already calculated
	   */
	  usecerror.tv_sec  = parsetime->parse_usecerror / 1000000;
	  usecerror.tv_usec = parsetime->parse_usecerror % 1000000;

	  sTVTOTS(&usecerror, &off);
	  L_ADD(&off, &offset);
	}
    }

  if (PARSE_PPS(parsetime->parse_state) && CL_PPS(parse->unit))
    {
      l_fp offset;

      /*
       * we have a PPS signal - much better than the RS232 stuff (we hope)
       */
      offset = parsetime->parse_ptime.fp;

      L_ADD(&offset, &parse->ppsdelay);

      if (PARSE_TIMECODE(parsetime->parse_state))
	{
	  if (M_ISGEQ(off.l_i, off.l_f, -1, 0x80000000) &&
	      M_ISGEQ(0, 0x7fffffff, off.l_i, off.l_f))
	    {
	      /*
	       * RS232 offsets within [-0.5..0.5[ - take PPS offsets
	       */

	      if (parse->parse_type->cl_flags & PARSE_F_PPSONSECOND)
		{
		  reftime = off = offset;
		  rectime = offset;
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
		  off = parsetime->parse_time.fp;

		  rectime = reftime = off; /* take reference time - fake rectime */

		  L_SUB(&off, &offset); /* true offset */
		}
	    }
	  /*
	   * take RS232 offset when PPS when out of bounds
	   */
	}
      else
	{
	  /*
	   * Well, no time code to guide us - assume on second pulse
	   * and pray, that we are within [-0.5..0.5[
	   */
	  reftime = off = offset;
	  rectime = offset;
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
	  return;
	}
    }


#if defined(PPS) || defined(PPSCLK) || defined(PPSPPS) || defined(PARSEPPS)
  if (CL_PPS(parse->unit) && !parse->pollonly && PARSE_SYNC(parsetime->parse_state))
    {
      /*
       * only provide PPS information when clock
       * is in sync
       * thus PHASE and EPOCH are correct and PPS is not
       * done via the CIOGETEV loopfilter mechanism
       */
#ifdef PPSPPS
      if (fdpps != parse->fd)
#endif
	(void) pps_sample(&off);
    }
#endif /* PPS || PPSCLK || PPSPPS || PARSEPPS */

  /*
   * ready, unless the machine wants a sample
   */
  if (!parse->pollonly && !parse->pollneeddata)
    return;

  parse->pollneeddata = 0;

  if (PARSE_PPS(parsetime->parse_state))
    {
      L_CLR(&dispersion);
    }
  else
    {
      /*
       * convert usec dispersion into NTP TS world
       */

      usecdisp.tv_sec  = parsetime->parse_usecdisp / 1000000;
      usecdisp.tv_usec = parsetime->parse_usecdisp % 1000000;

      TVTOTS(&usecdisp, &dispersion);
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
      leap = LEAP_NOTINSYNC;
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
	  leap = (parse->flags & PARSE_LEAP_DELETE) ? LEAP_DELSECOND : LEAP_ADDSECOND;
	}
      else
        if (PARSE_LEAPDEL(parsetime->parse_state))
	  {
	    leap = LEAP_DELSECOND;
	  }
	else
	  {
	    leap = LEAP_NOWARNING;
	  }
    }

  refclock_receive(parse->peer, &off, 0, LFPTOFP(&dispersion), &reftime, &rectime, leap);
}

/**===========================================================================
 ** clock polling support
 **/

struct poll_timer
{
  struct event timer;		/* we'd like to poll a a higher rate than 1/64s */
};

typedef struct poll_timer poll_timer_t;

/*--------------------------------------------------
 * direct poll routine
 */
static void
poll_dpoll(parse)
  struct parseunit *parse;
{
  register int rtc;
  register char *ps = ((poll_info_t *)parse->parse_type->cl_data)->string;
  register int   ct = ((poll_info_t *)parse->parse_type->cl_data)->count;

  rtc = write(parse->fd, ps, ct);
  if (rtc < 0)
    {
      syslog(LOG_ERR, "PARSE receiver #%d: poll_dpoll: failed to send cmd to clock: %m", CL_UNIT(parse->unit));
    }
  else
    if (rtc != ct)
      {
	syslog(LOG_ERR, "PARSE receiver #%d: poll_dpoll: failed to send cmd incomplete (%d of %d bytes sent)", CL_UNIT(parse->unit), rtc, ct);
      }
}

/*--------------------------------------------------
 * periodic poll routine
 */
static void
poll_poll(parse)
  struct parseunit *parse;
{
  register poll_timer_t *pt = (poll_timer_t *)parse->localdata;

  poll_dpoll(parse);

  if (pt != (poll_timer_t *)0)
    {
      pt->timer.event_time = current_time + ((poll_info_t *)parse->parse_type->cl_data)->rate;
      TIMER_ENQUEUE(timerqueue, &pt->timer);
    }
}

/*--------------------------------------------------
 * init routine - setup timer
 */
static int
poll_init(parse)
  struct parseunit *parse;
{
  register poll_timer_t *pt;

  if (((poll_info_t *)parse->parse_type->cl_data)->rate)
    {
      parse->localdata = (void *)malloc(sizeof(poll_timer_t));
      memset((char *)parse->localdata, 0, sizeof(poll_timer_t));

      pt = (poll_timer_t *)parse->localdata;

      pt->timer.peer          = (struct peer *)parse; /* well, only we know what it is */
      pt->timer.event_handler = poll_poll;
      poll_poll(parse);
    }
  else
    {
      parse->localdata = (void *)0;
    }

  return 0;
}

/*--------------------------------------------------
 * end routine - clean up timer
 */
static void
poll_end(parse)
  struct parseunit *parse;
{
  if (parse->localdata != (void *)0)
    {
      TIMER_DEQUEUE(&((poll_timer_t *)parse->localdata)->timer);
      free((char *)parse->localdata);
      parse->localdata = (void *)0;
    }
}

/**===========================================================================
 ** special code for special clocks
 **/


/*--------------------------------------------------
 * trimble TAIP init routine - setup EOL and then do poll_init.
 */
static int
trimbletaip_init(parse)
  struct parseunit *parse;
{
#ifdef HAVE_TERMIOS
  struct termios tm;
#endif
#ifdef HAVE_SYSV_TTYS
  struct termio tm;
#endif
  /*
   * configure terminal line for trimble receiver
   */
  if (TTY_GETATTR(parse->fd, &tm) == -1)
    {
      syslog(LOG_ERR, "PARSE receiver #%d: trimbletaip_init: tcgetattr(fd, &tm): %m", CL_UNIT(parse->unit));
      return 0;
    }
  else
    {
      tm.c_cc[VEOL] = TRIMBLETAIP_EOL;

      if (TTY_SETATTR(parse->fd, &tm) == -1)
	{
	  syslog(LOG_ERR, "PARSE receiver #%d: trimbletaip_init: tcsetattr(fd, &tm): %m", CL_UNIT(parse->unit));
	  return 0;
	}
    }
  return poll_init(parse);
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
 * the offset is way off, when first starting up xntpd for example,
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
 */

union {
    u_char  bd[8];
    int     iv;
    float   fv;
    double  dv;
}  uval;

struct txbuf
{
  short idx;			/* index to first unused byte */
  u_char *txt;			/* pointer to actual data buffer */
};

void
sendcmd(buf, c)
  struct txbuf *buf;
  u_char c;
{
  buf->txt[0] = DLE;
  buf->txt[1] = c;
  buf->idx = 2;
}

void sendbyte(buf, b)
  struct txbuf *buf;
  u_char b;
{
  if (b == DLE)
    buf->txt[buf->idx++] = DLE;
  buf->txt[buf->idx++] = b;
}

void
sendetx(buf, parse)
  struct txbuf *buf;
  struct parseunit *parse;
{
  buf->txt[buf->idx++] = DLE;
  buf->txt[buf->idx++] = ETX;

  if (write(parse->fd, buf->txt, buf->idx) != buf->idx)
    {
      syslog(LOG_ERR, "PARSE receiver #%d: sendetx: failed to send cmd to clock: %m", CL_UNIT(parse->unit));
    }
}

void
sendint(buf, a)
  struct txbuf *buf;
  int a;
{
  uval.iv = a;
  sendbyte(buf, uval.bd[2]);
  sendbyte(buf, uval.bd[3]);
}

void
sendflt(buf, a)
  struct txbuf *buf;
  float a;
{
  int i;

  uval.fv = a;
  for (i=0; i<=3; i++)
    sendbyte(buf, uval.bd[i]);
}

/*--------------------------------------------------
 * trimble TSIP init routine
 */
static int
trimbletsip_init(parse)
  struct parseunit *parse;
{
  u_char buffer[256];
  struct txbuf buf;

  buf.txt = buffer;

  if (!poll_init(parse))
    {
      sendcmd(&buf, 0x1f);	/* request software versions */
      sendetx(&buf, parse);

      sendcmd(&buf, 0x2c);	/* set operating parameters */
      sendbyte(&buf, 4);	/* static */
      sendflt(&buf, 5.0*D2R);	/* elevation angle mask = 10 deg XXX */
      sendflt(&buf, 4.0);	/* s/n ratio mask = 6 XXX */
      sendflt(&buf, 12.0);	/* PDOP mask = 12 */
      sendflt(&buf, 8.0);	/* PDOP switch level = 8 */
      sendetx(&buf, parse);

      sendcmd(&buf, 0x22);	/* fix mode select */
      sendbyte(&buf, 0);	/* automatic */
      sendetx(&buf, parse);

      sendcmd(&buf, 0x28);	/* request system message */
      sendetx(&buf, parse);

      sendcmd(&buf, 0x8e);	/* superpacket fix */
      sendbyte(&buf, 0x2);	/* binary mode */
      sendetx(&buf, parse);

      sendcmd(&buf, 0x35);	/* set I/O options */
      sendbyte(&buf, 0);	/* no position output */
      sendbyte(&buf, 0);	/* no velocity output */
      sendbyte(&buf, 7);	/* UTC, compute on seconds, send only on request */
      sendbyte(&buf, 0);	/* no raw measurements */
      sendetx(&buf, parse);

      sendcmd(&buf, 0x2f);	/* request UTC correction data */
      sendetx(&buf, parse);
      return 0;
    }
  else
    return 1;
}

#endif	/* defined(REFCLOCK) && defined(PARSE) */

/*
 * History:
 *
 * refclock_parse.c,v
 * Revision 3.53  1994/03/25  13:07:39  kardel
 * fixed offset calculation for large (>4 Min) offsets
 *
 * Revision 3.52  1994/03/03  09:58:00  kardel
 * stick -kv in cvs is no fun
 *
 * Revision 3.49  1994/02/20  13:26:00  kardel
 * rcs id cleanup
 *
 * Revision 3.48  1994/02/20  13:04:56  kardel
 * parse add/delete second support
 *
 * Revision 3.47  1994/02/02  17:44:30  kardel
 * rcs ids fixed
 *
 * Revision 3.45  1994/01/25  19:06:27  kardel
 * 94/01/23 reconcilation
 *
 * Revision 3.44  1994/01/25  17:32:23  kardel
 * settable extended variables
 *
 * Revision 3.43  1994/01/23  16:28:39  kardel
 * HAVE_TERMIOS introduced
 *
 * Revision 3.42  1994/01/22  11:35:04  kardel
 * added HAVE_TERMIOS
 *
 * Revision 3.41  1993/11/27  18:44:37  kardel
 * can't trust GPS166 on unsync
 *
 * Revision 3.40  1993/11/21  18:03:36  kardel
 * useless declaration deleted
 *
 * Revision 3.39  1993/11/21  15:30:15  kardel
 * static funcitions may be declared only at outer level
 *
 * Revision 3.38  1993/11/15  21:26:49  kardel
 * conditional define comments fixed
 *
 * Revision 3.37  1993/11/11  11:20:49  kardel
 * declaration fixes
 *
 * Revision 3.36  1993/11/10  12:17:14  kardel
 * #ifdef glitch
 *
 * Revision 3.35  1993/11/01  21:15:06  kardel
 * comments updated
 *
 * Revision 3.34  1993/11/01  20:01:08  kardel
 * parse Solaris support (initial version)
 *
 * Revision 3.33  1993/10/30  09:44:58  kardel
 * conditional compilation flag cleanup
 *
 * Revision 3.32  1993/10/22  14:28:43  kardel
 * Oct. 22nd 1993 reconcilation
 *
 * Revision 3.31  1993/10/10  21:19:10  kardel
 * compilation cleanup - (minimal porting tests)
 *
 * Revision 3.30  1993/10/09  21:44:35  kardel
 * syslog strings fixed
 *
 * Revision 3.29  1993/10/09  14:40:15  kardel
 * default precision setting fixed
 *
 * Revision 3.28  1993/10/08  14:48:22  kardel
 * Changed offset determination logic:
 * 	Take the PPS offset if it is available and the time
 * 	code offset is within [-0.5..0.5[, otherwise stick
 * 	to the time code offset
 *
 * Revision 3.27  1993/10/08  00:53:17  kardel
 * announce also simulated PPS via CIOGETEV in ntpq cl
 *
 * Revision 3.26  1993/10/07  23:29:35  kardel
 * trimble fixes
 *
 * Revision 3.25  1993/10/06  21:13:35  kardel
 * test reversed (CIOGETEV support)
 *
 * Revision 3.24  1993/10/03  20:18:26  kardel
 * Well, values  > 999999 in the usec field from uniqtime() timestamps
 * can prove harmful.
 *
 * Revision 3.23  1993/10/03  19:49:54  kardel
 * buftvtots where failing on uninitialized time stamps
 *
 * Revision 3.22  1993/10/03  19:11:09  kardel
 * restructured I/O handling
 *
 * Revision 3.21  1993/09/29  11:30:18  kardel
 * special init for trimble to set EOL
 *
 * Revision 3.20  1993/09/27  22:46:28  kardel
 * preserve module stack if I_PUSH parse fails
 *
 * Revision 3.19  1993/09/27  21:10:11  kardel
 * wrong structure member
 *
 * Revision 3.18  1993/09/27  13:05:06  kardel
 * Trimble is true polling only
 *
 * Revision 3.17  1993/09/27  12:47:10  kardel
 * poll string support generalized
 *
 * Revision 3.16  1993/09/26  23:40:56  kardel
 * new parse driver logic
 *
 * Revision 3.15  1993/09/24  15:00:51  kardel
 * Sep 23rd distribution...
 *
 * Revision 3.14  1993/09/22  18:21:15  kardel
 * support ppsclock streams module (-DSTREAM -DPPSPPS -DPARSEPPS -UPARSESTREAM)
 *
 * Revision 3.13  1993/09/05  15:38:33  kardel
 * not every cpp understands #error...
 *
 * Revision 3.12  1993/09/02  20:04:19  kardel
 * TTY cleanup
 *
 * Revision 3.11  1993/09/01  21:48:47  kardel
 * conditional cleanup
 *
 * Revision 3.10  1993/09/01  11:32:45  kardel
 * assuming HAVE_POSIX_TTYS when STREAM defined
 *
 * Revision 3.9  1993/08/31  22:31:46  kardel
 * SINIX-M SysVR4 integration
 *
 * Revision 3.8  1993/08/27  00:29:50  kardel
 * compilation cleanup
 *
 * Revision 3.7  1993/08/24  22:27:30  kardel
 * cleaned up AUTOCONF DCF77 mess 8-) - wasn't too bad
 *
 * Revision 3.6  1993/08/24  21:36:23  kardel
 * casting and ifdefs
 *
 * Revision 3.5  1993/07/09  23:36:59  kardel
 * HAVE_POSIX_TTYS used to produce errors 8-( - BSD driver support still lacking
 *
 * Revision 3.4  1993/07/09  12:42:29  kardel
 * RAW DCF now officially released
 *
 * Revision 3.3  1993/07/09  11:50:37  kardel
 * running GPS also on 960 to be able to switch GPS/DCF77
 *
 * Revision 3.2  1993/07/09  11:37:34  kardel
 * Initial restructured version + GPS support
 *
 * Revision 3.1  1993/07/06  10:01:07  kardel
 * DCF77 driver goes generic...
 *
 */
