/*
 * /src/NTP/REPOSITORY/v3/include/parse.h,v 3.13 1994/01/25 19:04:21 kardel Exp
 *
 * parse.h,v 3.13 1994/01/25 19:04:21 kardel Exp
 *
 * Copyright (c) 1989,1990,1991,1992,1993,1994
 * Frank Kardel Friedrich-Alexander Universitaet Erlangen-Nuernberg
 *                                    
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#ifndef __PARSE_H__
#define __PARSE_H__
#if	!(defined(lint) || defined(__GNUC__))
  static char parsehrcsid[]="parse.h,v 3.13 1994/01/25 19:04:21 kardel Exp FAU";
#endif

#include "ntp_types.h"

#include "parse_conf.h"

/*
 * we use the following datastructures in two modes
 * either in the NTP itself where we use NTP time stamps at some places
 * or in the kernel, where only struct timeval will be used.
 */
#undef PARSEKERNEL
#if defined(KERNEL) || defined(_KERNEL)
#ifndef PARSESTREAM
#define PARSESTREAM
#endif
#endif
#if defined(PARSESTREAM) && defined(STREAM)
#define PARSEKERNEL
#endif
#ifdef PARSEKERNEL
#ifndef _KERNEL
extern caddr_t kmem_alloc P((unsigned int));
extern caddr_t kmem_free P((caddr_t, unsigned int));
extern int splx();
extern int splhigh();
#define MALLOC(_X_) (char *)kmem_alloc(_X_)
#define FREE(_X_, _Y_) kmem_free((caddr_t)_X_, _Y_)
#else
#include <sys/kmem.h>
#define MALLOC(_X_) (char *)kmem_alloc(_X_, KM_SLEEP)
#define FREE(_X_, _Y_) kmem_free((caddr_t)_X_, _Y_)
#endif
#else
/* extern char *malloc();		XXX defined elsewhere */
#define MALLOC(_X_) malloc(_X_)
#define FREE(_X_, _Y_) free(_X_)
#endif

#if defined(PARSESTREAM) && defined(STREAM)
#include "sys/stream.h"
#include "sys/stropts.h"
#ifndef _KERNEL
extern int printf();
#endif
#else	/* STREAM */
#include <stdio.h>
#include "ntp_syslog.h"
#ifdef	DEBUG
extern int debug;
#define DD_PARSE 5
#define DD_RAWDCF 4
#define parseprintf(LEVEL, ARGS) if (debug > LEVEL) printf ARGS
#else	/* DEBUG */
#define parseprintf(LEVEL, ARGS)
#endif	/* DEBUG */
#endif	/* PARSESTREAM */

#ifndef TIMES10
#define TIMES10(_X_)	(((_X_) << 3) + ((_X_) << 1))
#endif

/*
 * state flags
 */
#define PARSEB_ANNOUNCE           0x0001 /* switch time zone warning (DST switch) */
#define PARSEB_POWERUP            0x0002 /* no synchronisation */
#define PARSEB_NOSYNC             0x0004 /* timecode currently not confirmed */
#define PARSEB_DST                0x0008 /* DST in effect */
#define PARSEB_UTC		  0x0010 /* UTC time */
#define PARSEB_LEAP		  0x0020 /* LEAP warning (1 hour prior to occurence) */
#define PARSEB_ALTERNATE	  0x0040 /* alternate antenna used */
#define PARSEB_POSITION		  0x0080 /* position available */
#define PARSEB_LEAPSECOND	  0x0100 /* actual leap second */

#define PARSEB_S_LEAP		  0x0200 /* supports LEAP */
#define PARSEB_S_ANTENNA	  0x0400 /* supports antenna information */
#define PARSEB_S_PPS     	  0x0800 /* supports PPS time stamping */
#define PARSEB_S_POSITION	  0x1000 /* supports position information (GPS) */

#define PARSEB_TIMECODE		  0x2000 /* valid time code sample */
#define PARSEB_PPS		  0x4000 /* valid PPS sample */

#define PARSE_TCINFO		(PARSEB_ANNOUNCE|PARSEB_POWERUP|PARSEB_NOSYNC|PARSEB_DST|\
				 PARSEB_UTC|PARSEB_LEAP|PARSEB_ALTERNATE|PARSEB_S_LEAP|\
				 PARSEB_S_LOCATION|PARSEB_TIMECODE)

#define PARSE_POWERUP(x)          ((x) & PARSEB_POWERUP)
#define PARSE_NOSYNC(x)           (((x) & (PARSEB_POWERUP|PARSEB_NOSYNC)) == PARSEB_NOSYNC)
#define PARSE_SYNC(x)             (((x) & (PARSEB_POWERUP|PARSEB_NOSYNC)) == 0)
#define PARSE_ANNOUNCE(x)         ((x) & PARSEB_ANNOUNCE)
#define PARSE_DST(x)              ((x) & PARSEB_DST)
#define PARSE_UTC(x)		((x) & PARSEB_UTC)
#define PARSE_LEAP(x)		(PARSE_SYNC(x) && ((x) & PARSEB_LEAP))
#define PARSE_ALTERNATE(x)	((x) & PARSEB_ALTERNATE)
#define PARSE_LEAPSECOND(x)	(PARSE_SYNC(x) && ((x) & PARSEB_LEAP_SECOND))

#define PARSE_S_LEAP(x)		((x) & PARSEB_S_LEAP)
#define PARSE_S_ANTENNA(x)	((x) & PARSEB_S_ANTENNA)
#define PARSE_S_PPS(x)		((x) & PARSEB_S_PPS)
#define PARSE_S_POSITION(x)	((x) & PARSEB_S_POSITION)

#define PARSE_TIMECODE(x)		((x) & PARSEB_TIMECODE)
#define PARSE_PPS(x)		((x) & PARSEB_PPS)
#define PARSE_POSITION(x)		((x) & PARSEB_POSITION)

/*
 * operation flags - some are also fudge flags
 */
#define PARSE_STAT_FLAGS    0x03	/* interpreted by io module */
#define   PARSE_STAT_FILTER 0x01	/* filter incoming data */
#define   PARSE_STAT_AVG	  0x02	/* 1:median average / 0: median point */
#define PARSE_LEAP_DELETE   0x04	/* delete leap */
#define PARSE_FIXED_FMT     0x10  /* fixed format */
#define PARSE_PPSCLOCK      0x20  /* try to get PPS time stamp via ppsclock ioctl */

typedef union timestamp
{
  struct timeval tv;		/* timeval - usually kernel view */
  l_fp           fp;		/* fixed point - xntp view */
} timestamp_t;

/*
 * standard time stamp structure
 */
struct parsetime
{
  unsigned LONG  parse_status;	/* data status - CVT_OK, CVT_NONE, CVT_FAIL ... */
  timestamp_t	 parse_time;	/* PARSE timestamp */
  timestamp_t	 parse_stime;	/* telegram sample timestamp */
  timestamp_t	 parse_ptime;	/* PPS time stamp */
  LONG           parse_usecerror;	/* sampled/filtered usec error */
  LONG           parse_usecdisp;	/* sampled usecdispersion */
  unsigned LONG	 parse_state;	/* current receiver state */
  unsigned short parse_format;	/* format code */
};

typedef struct parsetime parsetime_t;

/*---------- STREAMS interface ----------*/

#ifdef STREAM
/*
 * ioctls
 */
#define PARSEIOC_ENABLE		(('D'<<8) + 'E')
#define PARSEIOC_DISABLE	(('D'<<8) + 'D')
#define PARSEIOC_SETSTAT	(('D'<<8) + 'S')
#define PARSEIOC_GETSTAT	(('D'<<8) + 'G')
#define PARSEIOC_SETFMT         (('D'<<8) + 'f')
#define PARSEIOC_GETFMT	        (('D'<<8) + 'F')
#define PARSEIOC_SETCS	        (('D'<<8) + 'C')
#define PARSEIOC_TIMECODE	(('D'<<8) + 'T')

#endif

/*------ IO handling flags (sorry) ------*/

#define PARSE_IO_CSIZE	0x00000003
#define PARSE_IO_CS5	0x00000000
#define PARSE_IO_CS6	0x00000001
#define PARSE_IO_CS7	0x00000002 
#define PARSE_IO_CS8	0x00000003 

/*
 * sizes
 */
#define PARSE_TCMAX	128

/*
 * ioctl structure
 */
union parsectl 
{
  struct parsestatus
    {
      unsigned LONG flags;	/* new/old flags */
    } parsestatus;

  struct parsegettc
    {
      unsigned LONG  parse_state;	/* last state */
      unsigned LONG  parse_badformat; /* number of bad packets since last query */
      unsigned short parse_format;/* last decoded format */
      unsigned short parse_count;	/* count of valid time code bytes */
      char           parse_buffer[PARSE_TCMAX+1]; /* timecode buffer */
    } parsegettc;

  struct parseformat
    {
      unsigned short parse_format;/* number of examined format */
      unsigned short parse_count;	/* count of valid string bytes */
      char           parse_buffer[PARSE_TCMAX+1]; /* format code string */
    } parseformat;

  struct parsesetcs
    {
      unsigned LONG  parse_cs;	/* character size (needed for stripping) */
    } parsesetcs;
};
  
typedef union parsectl parsectl_t;

/*------ for conversion routines --------*/

#define PARSE_DELTA        16

struct parse			/* parse module local data */
{
  int            parse_flags;	/* operation and current status flags */
  
  int		 parse_ioflags;	   /* io handling flags (5-8 Bit control currently) */
  int		 parse_syncflags;	   /* possible sync events (START/END/character) */
  /*
   * RS232 input parser information
   */
  unsigned char  parse_startsym[32]; /* possible start packet values */
  unsigned char  parse_endsym[32];   /* possible end packet values */
  unsigned char	 parse_syncsym[32];  /* sync characters */
  struct timeval parse_timeout;	   /* max gap between characters (us) */

  /*
   * PPS 'input' buffer
   */
  struct timeval parse_lastone;	/* time stamp of last PPS 1 transition */
  struct timeval parse_lastzero;	/* time stamp of last PPS 0 transition */

  /*
   * character input buffer
   */
  timestamp_t    parse_lastchar;	/* time stamp of last received character */

  /*
   * time code input buffer (from RS232 or PPS)
   */
  unsigned short parse_index;	/* current buffer index */
  char          *parse_data;    /* data buffer */
  unsigned short parse_dsize;	/* size of data buffer */
  unsigned short parse_lformat;	/* last format used */
  unsigned LONG  parse_lstate;	/* last state code */
  char          *parse_ldata;	/* last data buffer */
  unsigned short parse_ldsize;	/* last data buffer length */
  unsigned LONG  parse_badformat;	/* number of unparsable pakets */
  
  /*
   * time stamp filtering
   */
  LONG           parse_delta[PARSE_DELTA]; /* delta buffer */
  int            parse_dindex;

  parsetime_t      parse_dtime;	/* external data prototype */
};

typedef struct parse parse_t;

struct clocktime		/* clock time broken up from time code */
{
  LONG day;
  LONG month;
  LONG year;
  LONG hour;
  LONG minute;
  LONG second;
  LONG usecond;
  LONG utcoffset;	/* in seconds */
  LONG flags;		/* current clock status */
};

typedef struct clocktime clocktime_t;

/*
 * clock formats specify routines to be called to
 * convert the buffer into a struct clock.
 * functions are called
 *   fn(buffer, data, clock) -> CVT_NONE, CVT_FAIL, CVT_OK
 *
 * the private data pointer can be used to
 * distingush between different formats of a common
 * base type
 */
#define F_START		0x00000001 /* start packet delimiter */
#define F_END		0x00000002 /* end packet delimiter */
#define SYNC_TIMEOUT	0x00000004 /* packet restart after timeout */
#define SYNC_START	0x00000008 /* packet start is sync event */
#define SYNC_END	0x00000010 /* packet end is sync event */
#define SYNC_CHAR	0x00000020 /* special character is sync event */
#define SYNC_ONE	0x00000040 /* PPS synchronize on 'ONE' transition */
#define SYNC_ZERO	0x00000080 /* PPS synchronize on 'ZERO' transition */
#define SYNC_SYNTHESIZE 0x00000100 /* generate intermediate time stamps */
#define CVT_FIXEDONLY   0x00010000 /* convert only in fixed configuration */

/*
 * parser related return/error codes
 */
#define CVT_MASK	0x0000000F /* conversion exit code */
#define   CVT_NONE	0x00000001 /* format not applicable */
#define   CVT_FAIL	0x00000002 /* conversion failed - error code returned */
#define   CVT_OK	0x00000004 /* conversion succeeded */
#define CVT_BADFMT	0x00000010 /* general format error - (unparsable) */
#define CVT_BADDATE     0x00000020 /* date field incorrect */
#define CVT_BADTIME	0x00000040 /* time field incorrect */

struct clockformat
{
  unsigned LONG (*convert)();	/* conversion routine */
  void          (*syncevt)();	/* routine for handling RS232 sync events (time stamps) */
  unsigned LONG (*syncpps)();	/* PPS input routine */
  unsigned LONG (*synth)();	/* time code synthesizer */
  void           *data;		/* local parameters */
  char           *name;		/* clock format name */
  unsigned short  length;	/* maximum length of data packet */
  unsigned LONG   flags;	/* valid start symbols etc. */
  struct timeval  timeout;	/* buffer restart after timeout (us) */
  unsigned char   startsym;	/* start symbol */
  unsigned char   endsym;	/* end symbol */
  unsigned char   syncsym;	/* sync symbol */
};

typedef struct clockformat clockformat_t;

/*
 * parse interface
 */
extern int  parse_ioinit(/* parse_t *parseio */);
extern void parse_ioend(/* parse_t *parseio */);
extern int  parse_ioread(/* parse_t *parseio, char ch, timestamp_t *ctime */);
extern int  parse_iopps(/* parse_t *parseio, int status, struct timeval *ptime, parsetime_t *dtime */);
extern void parse_iodone(/* parse_t *parseio */);

extern int  parse_getstat(/* parsectl_t *dct, parse_t *parse */);
extern int  parse_setstat(/* parsectl_t *dct, parse_t *parse */);
extern int  parse_timecode(/* parsectl_t *dct, parse_t *parse */);
extern int  parse_getfmt(/* parsectl_t *dct, parse_t *parse */);
extern int  parse_setfmt(/* parsectl_t *dct, parse_t *parse */);
extern int  parse_setcs(/* parsectl_t *dct, parse_t *parse */);

extern int Strok P((char *, char *));
extern int Stoi P((char *, LONG *, int));

extern time_t parse_to_unixtime P((clocktime_t *, unsigned LONG *));
extern unsigned LONG updatetimeinfo P((parse_t *, time_t, unsigned LONG, unsigned LONG));
extern void syn_simple P((parse_t *, timestamp_t *, struct format *, unsigned LONG));
extern unsigned LONG pps_simple P((parse_t *, int status, timestamp_t *));
#endif

/*
 * History:
 *
 * parse.h,v
 * Revision 3.13  1994/01/25  19:04:21  kardel
 * 94/01/23 reconcilation
 *
 * Revision 3.12  1994/01/23  17:23:05  kardel
 * 1994 reconcilation
 *
 * Revision 3.11  1993/11/11  11:20:18  kardel
 * declaration fixes
 *
 * Revision 3.10  1993/11/01  19:59:48  kardel
 * parse Solaris support (initial version)
 *
 * Revision 3.9  1993/10/06  00:14:57  kardel
 * include fixes
 *
 * Revision 3.8  1993/10/05  23:15:41  kardel
 * more STREAM protection
 *
 * Revision 3.7  1993/10/05  22:56:10  kardel
 * STREAM must be defined for PARSESTREAMS
 *
 * Revision 3.6  1993/10/03  19:10:28  kardel
 * restructured I/O handling
 *
 * Revision 3.5  1993/09/26  23:41:13  kardel
 * new parse driver logic
 *
 * Revision 3.4  1993/09/01  21:46:31  kardel
 * conditional cleanup
 *
 * Revision 3.3  1993/08/27  00:29:29  kardel
 * compilation cleanup
 *
 * Revision 3.2  1993/07/09  11:37:05  kardel
 * Initial restructured version + GPS support
 *
 * Revision 3.1  1993/07/06  09:59:12  kardel
 * DCF77 driver goes generic...
 *
 */
