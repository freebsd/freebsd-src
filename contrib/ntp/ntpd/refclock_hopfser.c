/*
 *
 * refclock_hopfser.c
 * - clock driver for hopf serial boards (GPS or DCF77)
 *
 * Date: 30.03.2000 Revision: 01.10
 *
 * latest source and further information can be found at:
 * http://www.ATLSoft.de/ntp
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#if defined(REFCLOCK) && (defined(CLOCK_HOPF_SERIAL))

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

/*
 * clock definitions
 */
#define	DESCRIPTION	"hopf Elektronik serial clock" /* Long name */
#define	PRECISION	(-10)	/* precision assumed (about 1 ms) */
#define	REFID		"hopf\0"	/* reference ID */
/*
 * I/O definitions
 */
#define	DEVICE		"/dev/hopfclock%d" 	/* device name and unit */
#define	SPEED232	B9600		    	/* uart speed (9600 baud) */


#define STX 0x02
#define ETX 0x03
#define CR  0x0c
#define LF  0x0a

/* parse states */
#define REC_QUEUE_EMPTY       0
#define REC_QUEUE_FULL        1

#define	HOPF_OPMODE	0x0C	/* operation mode mask */
#define HOPF_INVALID	0x00	/* no time code available */
#define HOPF_INTERNAL	0x04	/* internal clock */
#define HOPF_RADIO	0x08	/* radio clock */
#define HOPF_RADIOHP	0x0C	/* high precision radio clock */

/*
 * hopfclock unit control structure.
 */
struct hopfclock_unit {
	l_fp	laststamp;	/* last receive timestamp */
	short	unit;		/* NTP refclock unit number */
	u_long	polled;		/* flag to detect noreplies */
	char	leap_status;	/* leap second flag */
	int	rpt_next;
};

/*
 * Function prototypes
 */

static	int	hopfserial_start	P((int, struct peer *));
static	void	hopfserial_shutdown	P((int, struct peer *));
static	void	hopfserial_receive	P((struct recvbuf *));
static	void	hopfserial_poll		P((int, struct peer *));
/* static  void hopfserial_io		P((struct recvbuf *)); */
/*
 * Transfer vector
 */
struct refclock refclock_hopfser = {
	hopfserial_start,	/* start up driver */
	hopfserial_shutdown,	/* shut down driver */
	hopfserial_poll,	/* transmit poll message */
	noentry,		/* not used  */
	noentry,		/* initialize driver (not used) */
	noentry,		/* not used */
	NOFLAGS			/* not used */
};

/*
 * hopfserial_start - open the devices and initialize data for processing
 */
static int
hopfserial_start (
	int unit,
	struct peer *peer
	)
{
	register struct hopfclock_unit *up;
	struct refclockproc *pp;
	int fd;
	char gpsdev[20];

#ifdef SYS_WINNT
	(void) sprintf(gpsdev, "COM%d:", unit);
#else
	(void) sprintf(gpsdev, DEVICE, unit);
#endif
	/* LDISC_STD, LDISC_RAW
	 * Open serial port. Use CLK line discipline, if available.
	 */
	fd = refclock_open(gpsdev, SPEED232, LDISC_CLK);
	if (fd <= 0) {
#ifdef DEBUG
		printf("hopfSerialClock(%d) start: open %s failed\n", unit, gpsdev);
#endif
		return 0;
	}

	msyslog(LOG_NOTICE, "hopfSerialClock(%d) fd: %d dev: %s", unit, fd,
		gpsdev);

	/*
	 * Allocate and initialize unit structure
	 */
	up = (struct hopfclock_unit *) emalloc(sizeof(struct hopfclock_unit));

	if (!(up)) {
                msyslog(LOG_ERR, "hopfSerialClock(%d) emalloc: %m",unit);
#ifdef DEBUG
                printf("hopfSerialClock(%d) emalloc\n",unit);
#endif
		(void) close(fd);
		return (0);
	}

	memset((char *)up, 0, sizeof(struct hopfclock_unit));
	pp = peer->procptr;
	pp->unitptr = (caddr_t)up;
	pp->io.clock_recv = hopfserial_receive;
	pp->io.srcclock = (caddr_t)peer;
	pp->io.datalen = 0;
	pp->io.fd = fd;
	if (!io_addclock(&pp->io)) {
#ifdef DEBUG
                printf("hopfSerialClock(%d) io_addclock\n",unit);
#endif
		(void) close(fd);
		free(up);
		return (0);
	}

	/*
	 * Initialize miscellaneous variables
	 */
	pp->clockdesc = DESCRIPTION;
	peer->precision = PRECISION;
	peer->burst = NSTAGE;
	memcpy((char *)&pp->refid, REFID, 4);

	up->leap_status = 0;
	up->unit = (short) unit;

	return (1);
}


/*
 * hopfserial_shutdown - shut down the clock
 */
static void
hopfserial_shutdown (
	int unit,
	struct peer *peer
	)
{
	register struct hopfclock_unit *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct hopfclock_unit *)pp->unitptr;
	io_closeclock(&pp->io);
	free(up);
}



/*
 * hopfserial_receive - receive data from the serial interface
 */

static void
hopfserial_receive (
	struct recvbuf *rbufp
	)
{
	struct hopfclock_unit *up;
	struct refclockproc *pp;
	struct peer *peer;

	int		sync;	/* synchronization indicator */
	int		DoW;	/* Dow */

	int	day, month;	/* ddd conversion */

	/*
	 * Initialize pointers and read the timecode and timestamp.
	 */
	peer = (struct peer *)rbufp->recv_srcclock;
	pp = peer->procptr;
	up = (struct hopfclock_unit *)pp->unitptr;

	if (up->rpt_next == 0 )
		return;


	up->rpt_next = 0; /* wait until next poll interval occur */

	pp->lencode = refclock_gtlin(rbufp, pp->a_lastcode, BMAX, &pp->lastrec);

	if (pp->lencode  == 0)
		return;

	sscanf(pp->a_lastcode,
#if 1
	       "%1x%1x%2d%2d%2d%2d%2d%2d",   /* ...cr,lf */
#else
	       "%*c%1x%1x%2d%2d%2d%2d%2d%2d", /* stx...cr,lf,etx */
#endif
	       &sync,
	       &DoW,
	       &pp->hour,
	       &pp->minute,
	       &pp->second,
	       &day,
	       &month,
	       &pp->year);


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
		refclock_report(peer, CEVNT_BADREPLY);
		return;
	}
	/*
	  some preparations
	*/
	pp->day    = ymd2yd(pp->year,month,day);
	pp->leap=0;

	/* Year-2000 check! */
	/* wrap 2-digit date into 4-digit */

	if(pp->year < YEAR_PIVOT) { pp->year += 100; }		/* < 98 */
	pp->year += 1900;

	/* preparation for timecode ntpq rl command ! */

#if 0
	wsprintf(pp->a_lastcode,
		 "STATUS: %1X%1X, DATE: %02d.%02d.%04d  TIME: %02d:%02d:%02d",
		 sync,
		 DoW,
		 day,
		 month,
		 pp->year,
		 pp->hour,
		 pp->minute,
		 pp->second);

	pp->lencode = strlen(pp->a_lastcode);
	if ((sync && 0xc) == 0 ){  /* time ok? */
		refclock_report(peer, CEVNT_BADTIME);
		pp->leap = LEAP_NOTINSYNC;
		return;
	}
#endif
	/*
	 * If clock has no valid status then report error and exit
	 */
	if ((sync & HOPF_OPMODE) == HOPF_INVALID ){  /* time ok? */
		refclock_report(peer, CEVNT_BADTIME);
		pp->leap = LEAP_NOTINSYNC;
		return;
	}

	/*
	 * Test if time is running on internal quarz
	 * if CLK_FLAG1 is set, sychronize even if no radio operation
	 */

	if ((sync & HOPF_OPMODE) == HOPF_INTERNAL){
		if ((pp->sloppyclockflag & CLK_FLAG1) == 0) {
			refclock_report(peer, CEVNT_BADTIME);
			pp->leap = LEAP_NOTINSYNC;
			return;
		}
	}


	if (!refclock_process(pp)) {
		refclock_report(peer, CEVNT_BADTIME);
		return;
	}
	refclock_receive(peer);

#if 0
	msyslog(LOG_ERR, " D:%x  D:%d D:%d",sync,pp->minute,pp->second);
#endif

	record_clock_stats(&peer->srcadr, pp->a_lastcode);

	return;
}


/*
 * hopfserial_poll - called by the transmit procedure
 *
 */
static void
hopfserial_poll (
	int unit,
	struct peer *peer
	)
{
	register struct hopfclock_unit *up;
	struct refclockproc *pp;
	pp = peer->procptr;

	up = (struct hopfclock_unit *)pp->unitptr;

	pp->polls++;
	up->rpt_next = 1;

#if 0
	record_clock_stats(&peer->srcadr, pp->a_lastcode);
#endif

	return;
}

#else
int refclock_hopfser_bs;
#endif /* REFCLOCK */
