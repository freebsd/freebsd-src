/*
 * refclock_nmea.c - clock driver for an NMEA GPS CLOCK
 *		Michael Petry Jun 20, 1994
 *		 based on refclock_heath.c
 */
#if defined(REFCLOCK) && defined(NMEA)

#define	DEBUG	1

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"

/*
 * This driver supports the NMEA GPS Receiver with
 *
 * Protype was refclock_trak.c, Thanks a lot.
 *
 * The reciever used spits out the NMEA sentences for boat navigation.
 * And you thought it was an information superhighway.  Try a raging river
 * filled with rapids and whirlpools that rip away your data and warp time.
 */

/*
 * Definitions
 */
#define	DEVICE		"/dev/gps%d"	/* name of radio device */
#define	SPEED232	B4800	/* uart speed (4800 bps) */
#define	PRECISION	(-9)	/* precision assumed (about 2 ms) */
#define	REFID		"GPS\0"	/* reference id */
#define	DESCRIPTION	"NMEA GPS Clock" /* who we are */

#define	NSAMPLES	3	/* stages of median filter */
#define LENNMEA		75	/* min timecode length */

/*
 * Imported from ntp_timer module
 */
extern u_long current_time;	/* current time (s) */

/*
 * Imported from ntp_loopfilter module
 */
extern int fdpps;		/* pps file descriptor */

/*
 * Imported from ntpd module
 */
extern int debug;		/* global debug flag */

/*
 * Tables to compute the ddd of year form icky dd/mm timecode. Viva la
 * leap.
 */
static day1tab[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static day2tab[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/*
 * Unit control structure
 */
struct nmeaunit {
	int	pollcnt;	/* poll message counter */
	l_fp	tstamp;		/* timestamp of last poll */
};

/*
 * Function prototypes
 */
static	int	nmea_start	P((int, struct peer *));
static	void	nmea_shutdown	P((int, struct peer *));
static	void	nmea_receive	P((struct recvbuf *));
static	void	nmea_poll	P((int, struct peer *));
static	void	gps_send	P((int, char *, struct peer *));

/*
 * Transfer vector
 */
struct	refclock refclock_nmea = {
	nmea_start,		/* start up driver */
	nmea_shutdown,		/* shut down driver */
	nmea_poll,		/* transmit poll message */
	noentry,		/* handle control */
	noentry,		/* initialize driver */
	noentry,		/* buginfo */
	NOFLAGS			/* not used */
};

/*
 * nmea_start - open the GPS devices and initialize data for processing
 */
static int
nmea_start(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct nmeaunit *up;
	struct refclockproc *pp;
	int fd;
	char device[20];

	/*
	 * Open serial port. Use CLK line discipline, if available.
	 */
	(void)sprintf(device, DEVICE, unit);
	if (!(fd = refclock_open(device, SPEED232, LDISC_CLK)))
		return (0);

	/*
	 * Allocate and initialize unit structure
	 */
	if (!(up = (struct nmeaunit *)
	    emalloc(sizeof(struct nmeaunit)))) {
		(void) close(fd);
		return (0);
	}
	memset((char *)up, 0, sizeof(struct nmeaunit));
	pp = peer->procptr;
	pp->io.clock_recv = nmea_receive;
	pp->io.srcclock = (caddr_t)peer;
	pp->io.datalen = 0;
	pp->io.fd = fd;
	if (!io_addclock(&pp->io)) {
		(void) close(fd);
		free(up);
		return (0);
	}
	pp->unitptr = (caddr_t)up;

	/*
	 * Initialize miscellaneous variables
	 */
	peer->precision = PRECISION;
	pp->clockdesc = DESCRIPTION;
	memcpy((char *)&pp->refid, REFID, 4);
	up->pollcnt = 2;
	gps_send(pp->io.fd,"$PMOTG,RMC,0000\n", peer);
	return (1);
}

/*
 * nmea_shutdown - shut down a GPS clock
 */
static void
nmea_shutdown(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct nmeaunit *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct nmeaunit *)pp->unitptr;
	io_closeclock(&pp->io);
	free(up);
}

/*
 * nmea_receive - receive data from the serial interface
 */
static void
nmea_receive(rbufp)
	struct recvbuf *rbufp;
{
	register struct nmeaunit *up;
	struct refclockproc *pp;
	struct peer *peer;
	l_fp trtmp;
	int month, day;
	int i;
	char *cp, *dp;
	int cmdtype;
	char *field_parse();

	/*
	 * Initialize pointers and read the timecode and timestamp
	 */
	peer = (struct peer *)rbufp->recv_srcclock;
	pp = peer->procptr;
	up = (struct nmeaunit *)pp->unitptr;
	pp->lencode = refclock_gtlin(rbufp, pp->lastcode, BMAX, &trtmp);

	/*
	 * There is a case that a <CR><LF> gives back a "blank" line
	 */
	if (pp->lencode == 0)
		return;

	/*
	 * We get a buffer and timestamp for each <cr>; however, we use
	 * the timestamp of "now" since this may be a broadcast instead
	 * of a poll.  This needs to be checked empeerically
	 */
	gettstamp(&up->tstamp);		/* HACK */
	pp->lastrec = up->tstamp;
	up->pollcnt = 2;
	record_clock_stats(&peer->srcadr, pp->lastcode);
#ifdef DEBUG
	if (debug)
        	printf("nmea: timecode %d %s\n", pp->lencode,
		    pp->lastcode);
#endif

	/*
	 * We check the timecode format and decode its contents. The
	 * we only care about a few of them.  The most important being
	 * the $GPRMC format
	 * $GPRMC,hhmmss,a,fddmm.xx,n,dddmmm.xx,w,zz.z,yyy.,ddmmyy,dd,v*CC
  	 * $GPGGA,162617.0,4548.339,N,00837.719,E,1,07,0.97,00262,M,048,M,,*5D
	 */
#define GPRMC	0
#define GPXXX	1
#define GPGCA	2
	cp = pp->lastcode;
	pp->leap = 0;
	cmdtype=0;
	if(strncmp(cp,"$GPRMC",6)==0) {
		cmdtype=GPRMC;
		}
	else if(strncmp(cp,"$GPGGA",6)==0) {
		cmdtype=GPGCA;
		}
	else if(strncmp(cp,"$GPXXX",6)==0) {
		cmdtype=GPXXX;
		}
	else
		return;

	switch( cmdtype ) {
	case GPRMC:
	case GPGCA:
		/*
		 *	Check time code format of NMEA
		 */

		dp = field_parse(cp,1);
		if( !isdigit(dp[0]) ||
		   !isdigit(dp[1]) ||
		   !isdigit(dp[2]) ||
		   !isdigit(dp[3]) ||
		   !isdigit(dp[4]) ||
		   !isdigit(dp[5])
		  ) {
		    refclock_report(peer, CEVNT_BADREPLY);
		    return;
		}
		break;
	case GPXXX:
		return;
	default:
		return;

	}

	if (cmdtype ==GPGCA) {
		/* only time */
		time_t tt = time(NULL);
		struct tm * t = gmtime(&tt);
		day = t->tm_mday;
		month = t->tm_mon + 1;
		pp->year= t->tm_year;
	} else {
		dp = field_parse(cp,9);
		/*
		 * Convert date and check values.
		 */
		day = dp[0] - '0';
		day = (day * 10) + dp[1] - '0';
		month = dp[2] - '0';
		month = (month * 10) + dp[3] - '0';
		pp->year = dp[4] - '0';
		pp->year = (pp->year * 10) + dp[5] - '0';
	}

	if (month < 1 || month > 12 || day < 1) {
		refclock_report(peer, CEVNT_BADTIME);
		return;
	}

	if (pp->year % 4) {
		if (day > day1tab[month - 1]) {
			refclock_report(peer, CEVNT_BADTIME);
			return;
		}
		for (i = 0; i < month - 1; i++)
			day += day1tab[i];
	} else {
		if (day > day2tab[month - 1]) {
			refclock_report(peer, CEVNT_BADTIME);
			return;
		}
		for (i = 0; i < month - 1; i++)
			day += day2tab[i];
	}
	pp->day = day;

	dp = field_parse(cp,1);
	/*
	 * Convert time and check values.
	 */
	pp->hour = ((dp[0] - '0') * 10) + dp[1] - '0';
	pp->minute = ((dp[2] - '0') * 10) + dp[3] -  '0';
	pp->second = ((dp[4] - '0') * 10) + dp[5] - '0';
	pp->msec = 0;

	if (pp->hour > 23 || pp->minute > 59 || pp->second > 59) {
		refclock_report(peer, CEVNT_BADTIME);
		return;
	}

	/*
	 * Test for synchronization  Check for quality byte. (soon)
	 */
	pp->leap = 0;
	pp->lasttime = current_time;

	/*
	 * Process the new sample in the median filter and determine the
	 * reference clock offset and dispersion. We use lastrec as both
	 * the reference time and receive time, in order to avoid being
	 * cute, like setting the reference time later than the receive
	 * time, which may cause a paranoid protocol module to chuck out
	 * the data.
 	 */
	if (!refclock_process(pp, NSAMPLES, NSAMPLES)) {
		refclock_report(peer, CEVNT_BADTIME);
		return;
	}

	refclock_receive(peer, &pp->offset, 0, pp->dispersion,
	    &pp->lastrec, &pp->lastrec, pp->leap);
}

/*
 * nmea_poll - called by the transmit procedure
 *
 * We go to great pains to avoid changing state here, since there may be
 * more than one eavesdropper receiving the same timecode.
 */
static void
nmea_poll(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct nmeaunit *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct nmeaunit *)pp->unitptr;
	if (up->pollcnt == 0)
		refclock_report(peer, CEVNT_TIMEOUT);
	else
		up->pollcnt--;
	pp->polls++;

	/*
	 * usually nmea_receive can get a timestamp every second
	 */

	gps_send(pp->io.fd,"$PMOTG,RMC,0\n", peer);
}

/*
 *
 *	gps_send(fd,cmd, peer)  Sends a command to the GPS receiver.
 *	 as	gps_send(fd,"rqts,u\r", peer);
 *
 *	We don't currently send any data, but would like to send
 *	RTCM SC104 messages for differential positioning. It should
 *	also give us better time. Without a PPS output, we're
 *	Just fooling ourselves because of the serial code paths
 *
 */
static void
gps_send(fd, cmd, peer)
	int fd;
	char *cmd;
        struct peer *peer;
{

	if (write(fd, cmd, strlen(cmd)) == -1) {
		refclock_report(peer, CEVNT_FAULT);
	}
}

char *
field_parse(cp, fn)
	char *cp;
	int fn;
{
	char *tp;
	int i = fn;

	for (tp = cp; *tp != '\0'; tp++) {
		if (*tp == ',')
			i--;
		if (i == 0)
			break;
	}
	return (++tp);
}
#endif
