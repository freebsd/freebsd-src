/*
 * refclock_jjy - clock driver for JJY receivers
 */

/**********************************************************************/
/*                                                                    */
/*  Copyright (C) 2001, Takao Abe.  All rights reserved.              */
/*                                                                    */
/*  Permission to use, copy, modify, and distribute this software     */
/*  and its documentation for any purpose is hereby granted           */
/*  without fee, provided that the following conditions are met:      */
/*                                                                    */
/*  One retains the entire copyright notice properly, and both the    */
/*  copyright notice and this license. in the documentation and/or    */
/*  other materials provided with the distribution.                   */
/*                                                                    */
/*  This software and the name of the author must not be used to      */
/*  endorse or promote products derived from this software without    */
/*  prior written permission.                                         */
/*                                                                    */
/*  THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESSED OR IMPLIED    */
/*  WARRANTIES OF ANY KIND, INCLUDING, BUT NOT LIMITED TO, THE        */
/*  IMPLIED WARRANTIES OF MERCHANTABLILITY AND FITNESS FOR A          */
/*  PARTICULAR PURPOSE.                                               */
/*  IN NO EVENT SHALL THE AUTHOR TAKAO ABE BE LIABLE FOR ANY DIRECT,  */
/*  INDIRECT, GENERAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES   */
/*  ( INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE        */
/*  GOODS OR SERVICES; LOSS OF USE, DATA OR PROFITS; OR BUSINESS      */
/*  INTERRUPTION ) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,     */
/*  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT ( INCLUDING        */
/*  NEGLIGENCE OR OTHERWISE ) ARISING IN ANY WAY OUT OF THE USE OF    */
/*  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */
/*                                                                    */
/*  This driver is developed in my private time, and is opened as     */
/*  voluntary contributions for the NTP.                              */
/*  The manufacturer of the JJY receiver has not participated in      */
/*  a development of this driver.                                     */
/*  The manufacturer does not warrant anything about this driver,     */
/*  and is not liable for anything about this driver.                 */
/*                                                                    */
/**********************************************************************/
/*                                                                    */
/*  Author     Takao Abe                                              */
/*  Email      abetakao@bea.hi-ho.ne.jp                               */
/*  Homepage   http://www.bea.hi-ho.ne.jp/abetakao/                   */
/*                                                                    */
/**********************************************************************/
/*                                                                    */
/*  History                                                           */
/*                                                                    */
/*  2001/07/15                                                        */
/*    [New]    Support the Tristate Ltd. JJY receiver                 */
/*                                                                    */
/*  2001/08/04                                                        */
/*    [Change] Log to clockstats even if bad reply                    */
/*    [Fix]    PRECISION = (-3) (about 100 ms)                        */
/*    [Add]    Support the C-DEX Co.Ltd. JJY receiver                 */
/*  2001/12/04                                                        */
/*    [Fix]    C-DEX JST2000 ( fukusima@goto.info.waseda.ac.jp )      */
/*                                                                    */
/**********************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_JJY)

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_tty.h"
#include "ntp_refclock.h"
#include "ntp_calendar.h"
#include "ntp_stdlib.h"

/**********************************************************************/
/*                                                                    */
/*  The Tristate Ltd. JJY receiver JJY01                              */
/*                                                                    */
/*  Command        Response                 Remarks                   */
/*  ------------   ----------------------   ---------------------     */
/*  date<CR><LF>   YYYY/MM/DD XXX<CR><LF>                             */
/*  time<CR><LF>   HH:MM:SS<CR><LF>                                   */
/*  stim<CR><LF>   HH:MM:SS<CR><LF>         Reply at just second      */
/*                                                                    */
/*  During synchronization after a receiver is turned on,             */
/*  It replies the past time from 2000/01/01 00:00:00.                */
/*  The function "refclock_process" checks the time and tells         */
/*  as an insanity time.                                              */
/*                                                                    */
/**********************************************************************/
/*                                                                    */
/*  The C-DEX Co. Ltd. JJY receiver JST2000                           */
/*                                                                    */
/*  Command        Response                 Remarks                   */
/*  ------------   ----------------------   ---------------------     */
/*  <ENQ>1J<ETX>   <STX>JYYMMDD HHMMSSS<ETX>                          */
/*                                                                    */
/**********************************************************************/

/*
 * Interface definitions
 */
#define	DEVICE  	"/dev/jjy%d"    /* device name and unit */
#define	SPEED232	B9600           /* uart speed (9600 baud) */
#define	REFID   	"JJY"           /* reference ID */
#define	DESCRIPTION	"JJY Receiver"
#define	PRECISION	(-3)           /* precision assumed (about 100 ms) */

/*
 * JJY unit control structure
 */
struct jjyunit {
	char	unittype ;	/* UNITTYPE_XXXXXXXXXX */
	short	version ;
	short	linediscipline ;	/* LDISC_CLK or LDISC_RAW */
	int 	linecount ;
	int 	lineerror ;
	int 	year, month, day, hour, minute, second, msecond ;
/* LDISC_RAW only */
#define	MAX_LINECOUNT	8
#define	MAX_RAWBUF   	64
	int 	lineexpect ;
	int 	charexpect [ MAX_LINECOUNT ] ;
	int 	charcount ;
	char	rawbuf [ MAX_RAWBUF ] ;
};

#define	UNITTYPE_TRISTATE_JJY01	1
#define	UNITTYPE_CDEX_JST2000  	2

/*
 * Function prototypes
 */
static	int 	jjy_start                   P((int, struct peer *));
static	void	jjy_shutdown                P((int, struct peer *));
static	void	jjy_poll                    P((int, struct peer *));
static	void	jjy_poll_tristate_jjy01     P((int, struct peer *));
static	void	jjy_poll_cdex_jst2000       P((int, struct peer *));
static	void	jjy_receive                 P((struct recvbuf *));
static	int 	jjy_receive_tristate_jjy01  P((struct recvbuf *));
static	int 	jjy_receive_cdex_jst2000    P((struct recvbuf *));

/*
 * Transfer vector
 */
struct	refclock refclock_jjy = {
	jjy_start,      /* start up driver */
	jjy_shutdown,   /* shutdown driver */
	jjy_poll,       /* transmit poll message */
	noentry,        /* not used */
	noentry,        /* not used */
	noentry,        /* not used */
	NOFLAGS         /* not used */
};

/*
 * Start up driver return code
 */
#define	RC_START_SUCCESS	1
#define	RC_START_ERROR  	0

/*
 * Local constants definition
 */

#define	MAX_LOGTEXT	64


/**************************************************************************************************/
/*  jjy_start - open the devices and initialize data for processing                               */
/**************************************************************************************************/
static int
jjy_start ( int unit, struct peer *peer )
{

	struct jjyunit      *up ;
	struct refclockproc *pp ;
	int 	fd ;
	char	*pDeviceName ;
	short	iDiscipline ;

#ifdef DEBUG
	if ( debug ) {
		printf ( "jjy_start (refclock_jjy.c) : %s  mode=%d  ", ntoa(&peer->srcadr), peer->ttl ) ;
		printf ( DEVICE, unit ) ;
		printf ( "\n" ) ;
	}
#endif
	/*
	 * Open serial port
	 */
	if ( ! ( pDeviceName = (char*) emalloc ( strlen(DEVICE) + 10 ) ) ) {
		return RC_START_ERROR ;
	}
	sprintf ( pDeviceName, DEVICE, unit ) ;

	/*
	 * peer->ttl is a mode number specified by "127.127.40.X mode N" in the ntp.conf
	 */
	switch ( peer->ttl ) {
	case 0 :
	case 1 : iDiscipline = LDISC_CLK ; break ;
	case 2 : iDiscipline = LDISC_RAW ; break ;
	default :
		msyslog ( LOG_ERR, "JJY receiver [ %s mode %d ] : Unsupported mode",
		          ntoa(&peer->srcadr), peer->ttl ) ;
		free ( (void*) pDeviceName ) ;
		return RC_START_ERROR ;
	}

	if ( ! ( fd = refclock_open ( pDeviceName, SPEED232, iDiscipline ) ) ) {
		free ( (void*) pDeviceName ) ;
		return RC_START_ERROR ;
	}
	free ( (void*) pDeviceName ) ;

	/*
	 * Allocate and initialize unit structure
	 */
	if ( ! ( up = (struct jjyunit *) emalloc (sizeof(struct jjyunit)) ) ) {
		close ( fd ) ;
		return RC_START_ERROR ;
	}

	memset ( (char*)up, 0, sizeof(struct jjyunit) ) ;
	up->linediscipline = iDiscipline ;

	/*
	 * peer->ttl is a mode number specified by "127.127.40.X mode N" in the ntp.conf
	 */
	switch ( peer->ttl ) {
	case 0 :
		/*
		 * The mode 0 is a default clock type at this time.
		 * But this will be change to auto-detect mode in the future.
		 */
	case 1 :
		up->unittype = UNITTYPE_TRISTATE_JJY01 ;
		up->version  = 100 ;
		up->lineexpect = 2 ;
		up->charexpect[0] = 14 ; /* YYYY/MM/DD WWW<CR><LF> */
		up->charexpect[1] =  8 ; /* HH:MM:SS<CR><LF> */
		break ;
	case 2 :
		up->unittype = UNITTYPE_CDEX_JST2000 ;
		up->lineexpect = 1 ;
		up->charexpect[0] = 15 ; /* <STX>JYYMMDD HHMMSSS<ETX> */
		break ;
	default :
		msyslog ( LOG_ERR, "JJY receiver [ %s mode %d ] : Unsupported mode",
		          ntoa(&peer->srcadr), peer->ttl ) ;
		close ( fd ) ;
		free ( (void*) up ) ;
		return RC_START_ERROR ;
	}

	pp = peer->procptr ;
	pp->unitptr       = (caddr_t) up ;
	pp->io.clock_recv = jjy_receive ;
	pp->io.srcclock   = (caddr_t) peer ;
	pp->io.datalen    = 0 ;
	pp->io.fd         = fd ;
	if ( ! io_addclock(&pp->io) ) {
		close ( fd ) ;
		free ( (void*) up ) ;
		return RC_START_ERROR ;
	}

	/*
	 * Initialize miscellaneous variables
	 */
	peer->precision = PRECISION ;
	peer->burst     = 1 ;
	pp->clockdesc   = DESCRIPTION ;
	memcpy ( (char*)&pp->refid, REFID, strlen(REFID) ) ;

	return RC_START_SUCCESS ;

}


/**************************************************************************************************/
/*  jjy_shutdown - shutdown the clock                                                             */
/**************************************************************************************************/
static void
jjy_shutdown ( int unit, struct peer *peer )
{

	struct jjyunit      *up;
	struct refclockproc *pp;

	pp = peer->procptr ;
	up = (struct jjyunit *) pp->unitptr ;
	io_closeclock ( &pp->io ) ;
	free ( (void*) up ) ;

}


/**************************************************************************************************/
/*  jjy_receive - receive data from the serial interface                                          */
/**************************************************************************************************/
static void
jjy_receive ( struct recvbuf *rbufp )
{

	struct jjyunit      *up ;
	struct refclockproc *pp ;
	struct peer         *peer;

	l_fp	tRecvTimestamp;		/* arrival timestamp */
	int 	rc ;
	char	sLogText [ MAX_LOGTEXT ] ;
	int 	i, bCntrlChar ;

	/*
	 * Initialize pointers and read the timecode and timestamp
	 */
	peer = (struct peer *) rbufp->recv_srcclock ;
	pp = peer->procptr ;
	up = (struct jjyunit *) pp->unitptr ;

	/*
	 * Get next input line
	 */
	pp->lencode  = refclock_gtlin ( rbufp, pp->a_lastcode, BMAX, &tRecvTimestamp ) ;

	if ( up->linediscipline == LDISC_RAW ) {
		/*
		 * The reply with <STX> and <ETX> may give a blank line
		 */
		if ( pp->lencode == 0  &&  up->charcount == 0 ) return ;
		/*
		 * Copy received charaters to temporary buffer 
		 */
		for ( i = 0 ; i < pp->lencode && up->charcount < MAX_RAWBUF - 2 ; i ++ , up->charcount ++ ) {
			up->rawbuf[up->charcount] = pp->a_lastcode[i] ;
		}
		while ( up->charcount > 0 && up->rawbuf[0] < ' ' ) {
			for ( i = 0 ; i < up->charcount - 1 ; i ++ ) up->rawbuf[i] = up->rawbuf[i+1] ;
			up->charcount -- ;
		}
		bCntrlChar = 0 ;
		for ( i = 0 ; i < up->charcount ; i ++ ) {
			if ( up->rawbuf[i] < ' ' ) {
				bCntrlChar = 1 ;
				break ;
			}
		}
		if ( pp->lencode > 0  &&  up->linecount < up->lineexpect ) {
			if ( bCntrlChar == 0  &&  up->charcount < up->charexpect[up->linecount] ) return ;
		}
		up->rawbuf[up->charcount] = 0 ;
	} else {
		/*
		 * The reply with <CR><LF> gives a blank line
		 */
		if ( pp->lencode == 0 ) return ;
	}
	/*
	 * We get down to business
	 */

	pp->lastrec = tRecvTimestamp ;

	up->linecount ++ ;

	if ( up->lineerror != 0 ) return ;

	switch ( up->unittype ) {
	
	case UNITTYPE_TRISTATE_JJY01 :
		rc = jjy_receive_tristate_jjy01  ( rbufp ) ;
		break ;

	case UNITTYPE_CDEX_JST2000 :
		rc = jjy_receive_cdex_jst2000 ( rbufp ) ;
		break ;

	default :
		rc = 0 ;
		break ;

	}

	if ( up->linediscipline == LDISC_RAW ) {
		if ( up->linecount <= up->lineexpect  &&  up->charcount > up->charexpect[up->linecount-1] ) {
			for ( i = 0 ; i < up->charcount - up->charexpect[up->linecount-1] ; i ++ ) {
				up->rawbuf[i] = up->rawbuf[i+up->charexpect[up->linecount-1]] ;
			}
			up->charcount -= up->charexpect[up->linecount-1] ;
		} else {
			up->charcount = 0 ;
		}
	}

	if ( rc == 0 ) return ;

	if ( up->lineerror != 0 ) {
		refclock_report ( peer, CEVNT_BADREPLY ) ;
		strcpy  ( sLogText, "BAD REPLY [" ) ;
		if ( up->linediscipline == LDISC_RAW ) {
			strncat ( sLogText, up->rawbuf, MAX_LOGTEXT - strlen ( sLogText ) - 1 ) ;
		} else {
			strncat ( sLogText, pp->a_lastcode, MAX_LOGTEXT - strlen ( sLogText ) - 1 ) ;
		}
		sLogText[MAX_LOGTEXT-1] = 0 ;
		if ( strlen ( sLogText ) < MAX_LOGTEXT - 2 ) strcat ( sLogText, "]" ) ;
		record_clock_stats ( &peer->srcadr, sLogText ) ;
		return ;
	}

	pp->year   = up->year ;
	pp->day    = ymd2yd ( up->year, up->month, up->day ) ;
	pp->hour   = up->hour ;
	pp->minute = up->minute ;
	pp->second = up->second ;
	pp->nsec   = up->msecond * 1000000;

	/* 
	 * JST to UTC 
	 */
	pp->hour -= 9 ;
	if ( pp->hour < 0 ) {
		pp->hour += 24 ;
		pp->day -- ;
		if ( pp->day < 1 ) {
			pp->year -- ;
			pp->day  = ymd2yd ( pp->year, 12, 31 ) ;
		}
	}
#ifdef DEBUG
	if ( debug ) {
		printf ( "jjy_receive (refclock_jjy.c) : %04d/%02d/%02d %02d:%02d:%02d JST   ", 
		          up->year, up->month, up->day, up->hour, up->minute, up->second ) ;
		printf ( "( %04d/%03d %02d:%02d:%02d UTC )\n",
		          pp->year, pp->day, pp->hour, pp->minute, pp->second ) ;
	}
#endif

	/*
	 * Process the new sample in the median filter and determine the
	 * timecode timestamp.
	 */
	if ( ! refclock_process ( pp ) ) {
		refclock_report(peer, CEVNT_BADTIME);
		sprintf ( sLogText, "BAD TIME %04d/%02d/%02d %02d:%02d:%02d JST",
		          up->year, up->month, up->day, up->hour, up->minute, up->second ) ;
		record_clock_stats ( &peer->srcadr, sLogText ) ;
		return ;
	}

	sprintf ( sLogText, "%04d/%02d/%02d %02d:%02d:%02d JST",
	          up->year, up->month, up->day, up->hour, up->minute, up->second ) ;
	pp->lastref = pp->lastrec;
	refclock_receive(peer);
	record_clock_stats ( &peer->srcadr, sLogText ) ;
}

/**************************************************************************************************/

static int
jjy_receive_tristate_jjy01 ( struct recvbuf *rbufp )
{

	struct jjyunit      *up ;
	struct refclockproc *pp ;
	struct peer         *peer;

	char	*pBuf ;
	int 	iLen ;
	int 	rc ;

	/*
	 * Initialize pointers and read the timecode and timestamp
	 */
	peer = (struct peer *) rbufp->recv_srcclock ;
	pp = peer->procptr ;
	up = (struct jjyunit *) pp->unitptr ;

	if ( up->linediscipline == LDISC_RAW ) {
		pBuf = up->rawbuf ;
		iLen = up->charcount ;
	} else {
	    pBuf = pp->a_lastcode ;
	    iLen = pp->lencode ;
	}

	switch ( up->linecount ) {

	case 1 : /* YYYY/MM/DD */

		if ( iLen < 10 ) {
			up->lineerror = 1 ;
			break ;
		}
		rc = sscanf ( pBuf, "%4d/%2d/%2d", &up->year, &up->month, &up->day ) ;
		if ( rc != 3 || up->year < 2000 || up->month < 1 || up->month > 12 || up->day < 1 || up->day > 31 ) {
			up->lineerror = 1 ;
			break ;
		}
		return 0 ;

	case 2 : /* HH:MM:SS */

		if ( iLen < 8 ) {
			up->lineerror = 1 ;
			break ;
		}
		rc = sscanf ( pBuf, "%2d:%2d:%2d", &up->hour, &up->minute, &up->second ) ;
		if ( rc != 3 || up->hour > 23 || up->minute > 59 || up->second > 60 ) {
			up->lineerror = 1 ;
			break ;
		}
		up->msecond = 0 ;
		if ( up->hour == 0 && up->minute == 0 && up->second <= 2 ) {
			/*
			 * The command "date" and "time" ( or "stim" ) were sent to the JJY receiver continuously.
			 * But the JJY receiver replies a date and time separately.
			 * Just after midnight transtions, we ignore this time.
			 */
			return 0 ;
		}
		break ;

	default : /*  Unexpected reply */

		up->lineerror = 1 ;
		break ;

	}

	return 1 ;

}

/**************************************************************************************************/

static int
jjy_receive_cdex_jst2000 ( struct recvbuf *rbufp )
{

	struct jjyunit      *up ;
	struct refclockproc *pp ;
	struct peer         *peer;

	char	*pBuf ;
	int 	iLen ;
	int 	rc ;

	/*
	 * Initialize pointers and read the timecode and timestamp
	 */
	peer = (struct peer *) rbufp->recv_srcclock ;
	pp = peer->procptr ;
	up = (struct jjyunit *) pp->unitptr ;

	if ( up->linediscipline == LDISC_RAW ) {
		pBuf = up->rawbuf ;
		iLen = up->charcount ;
	} else {
	    pBuf = pp->a_lastcode ;
	    iLen = pp->lencode ;
	}

	switch ( up->linecount ) {

	case 1 : /* JYYMMDD HHMMSSS */

		if ( iLen < 15 ) {
			up->lineerror = 1 ;
			break ;
		}
		rc = sscanf ( pBuf, "J%2d%2d%2d%*1d%2d%2d%2d%1d",
		              &up->year, &up->month, &up->day, &up->hour, &up->minute, &up->second, &up->msecond ) ;
		if ( rc != 7 || up->month < 1 || up->month > 12 || up->day < 1 || up->day > 31
		  || up->hour > 23 || up->minute > 59 || up->second > 60 ) {
			up->lineerror = 1 ;
			break ;
		}
		up->year    += 2000 ;
		up->msecond *= 100 ;
		break ;

	default : /*  Unexpected reply */

		up->lineerror = 1 ;
		break ;

	}

	return 1 ;

}

/**************************************************************************************************/
/*  jjy_poll - called by the transmit procedure                                                   */
/**************************************************************************************************/
static void
jjy_poll ( int unit, struct peer *peer )
{

	struct jjyunit      *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct jjyunit *) pp->unitptr ;

	if ( pp->polls > 0  &&  up->linecount == 0 ) {
		/*
		 * No reply for last command
		 */
		refclock_report ( peer, CEVNT_TIMEOUT ) ;
	}

#ifdef DEBUG
	if ( debug ) {
		printf ( "jjy_poll (refclock_jjy.c) : %ld\n", pp->polls ) ;
	}
#endif

	pp->polls ++ ;

	up->linecount = 0 ;
	up->lineerror = 0 ;
	up->charcount = 0 ;

	switch ( up->unittype ) {
	
	case UNITTYPE_TRISTATE_JJY01 :
		jjy_poll_tristate_jjy01  ( unit, peer ) ;
		break ;

	case UNITTYPE_CDEX_JST2000 :
		jjy_poll_cdex_jst2000 ( unit, peer ) ;
		break ;

	default :
		break ;

	}

}

/**************************************************************************************************/

static void
jjy_poll_tristate_jjy01  ( int unit, struct peer *peer )
{

	struct jjyunit      *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct jjyunit *) pp->unitptr ;

	/*
	 * Send "date<CR><LF>" command
	 */

	if ( write ( pp->io.fd, "date\r\n",6 ) != 6  ) {
		refclock_report ( peer, CEVNT_FAULT ) ;
	}

	/*
	 * Send "stim<CR><LF>" or "time<CR><LF>" command
	 */

	if ( up->version >= 100 ) {
		if ( write ( pp->io.fd, "stim\r\n",6 ) != 6  ) {
			refclock_report ( peer, CEVNT_FAULT ) ;
		}
	} else {
		if ( write ( pp->io.fd, "time\r\n",6 ) != 6  ) {
			refclock_report ( peer, CEVNT_FAULT ) ;
		}
	}

}

/**************************************************************************************************/

static void
jjy_poll_cdex_jst2000 ( int unit, struct peer *peer )
{

	struct refclockproc *pp;

	pp = peer->procptr;

	/*
	 * Send "<ENQ>1J<ETX>" command
	 */

	if ( write ( pp->io.fd, "\0051J\003", 4 ) != 4  ) {
		refclock_report ( peer, CEVNT_FAULT ) ;
	}

}

#else
int refclock_jjy_bs ;
#endif /* REFCLOCK */
