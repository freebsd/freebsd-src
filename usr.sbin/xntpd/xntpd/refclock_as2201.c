/*
 * refclock_as2201 - clock driver for the Austron 2201A GPS
 *	Timing Receiver
 */
#if defined(REFCLOCK) && defined(AS2201)

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_unixtime.h"
#include "ntp_stdlib.h"

#ifdef PPS
#include <sys/ppsclock.h>
#endif /* PPS */

/*
 * This driver supports the Austron 2200A/2201A GPS Receiver with
 * Buffered RS-232-C Interface Module. Note that the original 2200/2201
 * receivers will not work reliably with this driver, since the older
 * design cannot accept input commands at any reasonable data rate.
 *
 * The program sends a "*toc\r" to the radio and expects a response of
 * the form "yy:ddd:hh:mm:ss.mmm\r" where yy = year of century, ddd =
 * day of year, hh:mm:ss = second of day and mmm = millisecond of
 * second. Then, it sends statistics commands to the radio and expects
 * a multi-line reply showing the corresponding statistics or other
 * selected data. Statistics commands are sent in order as determined by
 * a vector of commands; these might have to be changed with different
 * radio options. If flag4 of the fudge configuration command is set to
 * 1, the statistics data are written to the clockstats file for later
 * processing.
 *
 * In order for this code to work, the radio must be placed in non-
 * interactive mode using the "off" command and with a single <cr>
 * resonse using the "term cr" command. The setting of the "echo"
 * and "df" commands does not matter. The radio should select UTC
 * timescale using the "ts utc" command.
 *
 * There are two modes of operation for this driver. The first with
 * default configuration is used with stock kernels and serial-line
 * drivers and works with almost any machine. In this mode the driver
 * assumes the radio captures a timestamp upon receipt of the "*" that
 * begins the driver query. Accuracies in this mode are in the order of
 * a millisecond or two and the receiver can be connected to only one
 * host.
 *
 * The second mode of operation can be used for SunOS kernels that have
 * been modified with the ppsclock streams module included in this
 * distribution. The mode is enabled if flag3 of the fudge configuration
 * command has been set to 1. In this mode a precise timestamp is
 * available using a gadget box and 1-pps signal from the receiver. This
 * improves the accuracy to the order of a few tens of microseconds. In
 * addition, the serial output and 1-pps signal can be bussed to
 * additional receivers. 
 */

/*
 * GPS Definitions
 */
#define SMAX		200	/* statistics buffer length */
#define	DEVICE		"/dev/gps%d" /* device name and unit */
#define	SPEED232	B9600	/* uart speed (9600 baud) */
#define	PRECISION	(-10)	/* precision assumed (about 1 ms) */
#define	REFID		"GPS\0"	/* reference ID */
#define	DESCRIPTION	"Austron 2201A GPS Receiver" /* WRU */

#define	NSAMPLES	3	/* stages of median filter */
#define	LENTOC		19	/* yy:ddd:hh:mm:ss.mmm timecode lngth */

/*
 * Imported from ntp_timer module
 */
extern u_long current_time;	/* current time (s) */

/*
 * Imported from ntpd module
 */
extern int debug;		/* global debug flag */

#ifdef PPS
/*
 * Imported from loop_filter module
 */
extern int fdpps;		/* ppsclock file descriptor */
#endif /* PPS */

/*
 * AS2201 unit control structure.
 */
struct as2201unit {
	int	pollcnt;	/* poll message counter */

	char	*lastptr;	/* statistics buffer pointer */
	char	stats[SMAX];	/* statistics buffer */

#ifdef PPS
	u_long lastev;		/* last ppsclock second */
#endif /* PPS */

	int	linect;		/* count of lines remaining */
	int	index;		/* current statistics command */
};

/*
 * Radio commands to extract statitistics
 *
 * A command consists of an ASCII string terminated by a <cr> (\r). The
 * command list consist of a sequence of commands terminated by a null
 * string ("\0"). One command from the list is sent immediately
 * following each received timecode (*toc\r command) and the ASCII
 * strings received from the radio are saved along with the timecode in
 * the clockstats file. Subsequent commands are sent at each timecode,
 * with the last one in the list followed by the first one. The data
 * received from the radio consist of ASCII strings, each terminated by
 * a <cr> (\r) character. The number of strings for each command is
 * specified as the first line of output as an ASCII-encode number. Note
 * that the ETF command requires the Input Buffer Module and the LORAN
 * commands require the LORAN Assist Module. However, if these modules
 * are not installed, the radio and this driver will continue to operate
 * successfuly, but no data will be captured for these commands.
 */
static char stat_command[][30] = {
	"ITF\r",		/* internal time/frequency */
	"ETF\r",		/* external time/frequency */
	"LORAN ENSEMBLE\r",	/* GPS/LORAN ensemble statistics */
	"LORAN TDATA\r",	/* LORAN signal data */
	"ID;OPT;VER\r",		/* model; options; software version */

	"ITF\r",		/* internal time/frequency */
	"ETF\r",		/* external time/frequency */
	"LORAN ENSEMBLE\r",	/* GPS/LORAN ensemble statistics */
	"TRSTAT\r",		/* satellite tracking status */
	"POS;PPS;PPSOFF\r",	/* position, pps source, offsets */

	"ITF\r",		/* internal time/frequency */
	"ETF\r",		/* external time/frequency */
	"LORAN ENSEMBLE\r",	/* GPS/LORAN ensemble statistics */
	"LORAN TDATA\r",	/* LORAN signal data */
	"UTC\r",			/* UTC leap info */

	"ITF\r",		/* internal time/frequency */
	"ETF\r",		/* external time/frequency */
	"LORAN ENSEMBLE\r",	/* GPS/LORAN ensemble statistics */
	"TRSTAT\r",		/* satellite tracking status */
	"OSC;ET;TEMP\r",	/* osc type; tune volts; oven temp */
	"\0"			/* end of table */
};

/*
 * Function prototypes
 */
static	int	as2201_start	P((int, struct peer *));
static	void	as2201_shutdown	P((int, struct peer *));
static	void	as2201_receive	P((struct recvbuf *));
static	void	as2201_poll	P((int, struct peer *));

/*
 * Transfer vector
 */
struct	refclock refclock_as2201 = {
	as2201_start,		/* start up driver */
	as2201_shutdown,	/* shut down driver */
	as2201_poll,		/* transmit poll message */
	noentry,		/* not used (old as2201_control) */
	noentry,		/* initialize driver (not used) */
	noentry,		/* not used (old as2201_buginfo) */
	NOFLAGS			/* not used */
};


/*
 * as2201_start - open the devices and initialize data for processing
 */
static int
as2201_start(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct as2201unit *up;
	struct refclockproc *pp;
	int fd;
	char gpsdev[20];

	/*
	 * Open serial port. Use CLK line discipline, if available.
	 */
	(void)sprintf(gpsdev, DEVICE, unit);
	if (!(fd = refclock_open(gpsdev, SPEED232, LDISC_CLK)))
		return (0);

	/*
	 * Allocate and initialize unit structure
	 */
	if (!(up = (struct as2201unit *)
	    emalloc(sizeof(struct as2201unit)))) {
		(void) close(fd);
		return (0);
	}
	memset((char *)up, 0, sizeof(struct as2201unit));
	pp = peer->procptr;
	pp->io.clock_recv = as2201_receive;
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

	up->lastptr = up->stats;
	up->index = 0;
	return (1);
}


/*
 * as2201_shutdown - shut down the clock
 */
static void
as2201_shutdown(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct as2201unit *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct as2201unit *)pp->unitptr;
	io_closeclock(&pp->io);
	free(up);
}


/*
 * as2201__receive - receive data from the serial interface
 */
static void
as2201_receive(rbufp)
	struct recvbuf *rbufp;
{
	register struct as2201unit *up;
	struct refclockproc *pp;
	struct peer *peer;
	l_fp trtmp;
#ifdef PPS
	long ltemp;
	struct ppsclockev ev;
#endif /* PPS */

	/*
	 * Initialize pointers and read the timecode and timestamp.
	 */
	peer = (struct peer *)rbufp->recv_srcclock;
	pp = peer->procptr;
	up = (struct as2201unit *)pp->unitptr;
	pp->lencode = refclock_gtlin(rbufp, pp->lastcode, BMAX, &trtmp);
#ifdef DEBUG
	if (debug)
        	printf("gps: timecode %d %d %s\n",
		    up->linect, pp->lencode, pp->lastcode);
#endif
	if (pp->lencode == 0)
		return;

	/*
	 * If linect is greater than zero, we must be in the middle of a
	 * statistics operation, so simply tack the received data at the
	 * end of the statistics string. If not, we could either have
	 * just received the timecode itself or a decimal number
	 * indicating the number of following lines of the statistics
	 * reply. In the former case, write the accumulated statistics
	 * data to the clockstats file and continue onward to process
	 * the timecode; in the later case, save the number of lines and
	 * quietly return.
	 */
	if (up->linect > 0) {
		up->linect--;
		if (up->lastptr - up->stats + pp->lencode > SMAX - 2)
			return;
		*up->lastptr++ = ' ';
		(void)strcpy(up->lastptr, pp->lastcode);
		up->lastptr += pp->lencode;
		return;
	} else {
		if (pp->lencode == 1) {
			up->linect = atoi(pp->lastcode);
			return;
		} else {
			up->pollcnt = 2;
			record_clock_stats(&peer->srcadr, up->stats);
#ifdef DEBUG
			if (debug)
				printf("gps: stat %s\n", up->stats);
#endif
		}
	}
	up->lastptr = up->stats;
	*up->lastptr = '\0';

	/*
	 * We get down to business, check the timecode format and decode
	 * its contents. If the timecode has invalid length or is not in
	 * proper format, we declare bad format and exit.
	 */
	if (pp->lencode < LENTOC) {
		refclock_report(peer, CEVNT_BADREPLY);
		return;
	}

	/*
	 * Timecode format: "yy:ddd:hh:mm:ss.mmm"
	 */
	if (sscanf(pp->lastcode, "%2d:%3d:%2d:%2d:%2d.%3d", &pp->year,
	    &pp->day, &pp->hour, &pp->minute, &pp->second, &pp->msec)
	    != 6) {
		refclock_report(peer, CEVNT_BADREPLY);
		return;
	}

	/*
	 * Test for synchronization (this is a temporary crock).
	 */
	if (pp->lastcode[2] != ':') {
		pp->leap = LEAP_NOTINSYNC;
	} else {
		pp->leap = 0;
		pp->lasttime = current_time;
	}
#ifdef PPS

	/*
	 * If CLK_FLAG3 is set and the local time is within +-0.5 second
	 * of the timecode, use the pps offset instead. Note that we
	 * believe the ppsclock timestamp only if the ioctl works and
	 * the new timestamp is greater than the previous one.
	 */
	if (pp->sloppyclockflag & CLK_FLAG3 && fdpps != -1) {
		if (!clocktime(pp->day, pp->hour, pp->minute,
		    pp->second, GMT, pp->lastrec.l_ui, &pp->yearstart,
		    &pp->lastref.l_ui)) {
			refclock_report(peer, CEVNT_BADTIME);
			return;
		}
		MSUTOTSF(pp->msec, pp->lastref.l_uf);
		pp->lastrec = trtmp;
		L_SUB(&trtmp, &pp->lastref);
		if (L_ISNEG(&trtmp))
			L_NEG(&trtmp);
		if (trtmp.l_i < CLOCK_MAX_I || (trtmp.l_i == CLOCK_MAX_I
		    && trtmp.l_uf < CLOCK_MAX_F)) {
			if (ioctl(fdpps, CIOGETEV, (caddr_t)&ev) >= 0) {
				if (up->lastev < ev.tv.tv_sec) {
					TVUTOTSF(ev.tv.tv_usec, ltemp);
					pp->lastrec = pp->lastref;
					L_ADDF(&pp->lastrec, ltemp);
				}
				up->lastev = ev.tv.tv_sec;
			}
		}
	}
#endif /* PPS */
#ifdef DEBUG
	if (debug)
		printf("gps: times %s %s %s\n",
		    ulfptoa(&pp->lastref, 6), ulfptoa(&pp->lastrec, 6),
		    lfptoa(&trtmp, 6));
#endif

	/*
	 * Process the new sample in the median filter and determine the
	 * reference clock offset and dispersion. We use lastrec as both
	 * the reference time and receive time in order to avoid being
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

	/*
	 * If CLK_FLAG4 is set, initialize the statistics buffer and
	 * send the next command. If not, simply write the timecode to
	 * the clockstats file.
	 */
	(void)strcpy(up->lastptr, pp->lastcode);
	up->lastptr += pp->lencode;
	if (pp->sloppyclockflag & CLK_FLAG4) {
		*up->lastptr++ = ' ';
		(void)strcpy(up->lastptr, stat_command[up->index]);
		up->lastptr += strlen(stat_command[up->index]);
		up->lastptr--;
		*up->lastptr = '\0';
		(void)write(pp->io.fd, stat_command[up->index],
		    strlen(stat_command[up->index]));
		up->index++;
		if (*stat_command[up->index] == '\0')
			up->index = 0;
	}
}


/*
 * as2201_poll - called by the transmit procedure
 *
 * We go to great pains to avoid changing state here, since there may be
 * more than one eavesdropper receiving the same timecode.
 */
static void
as2201_poll(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct as2201unit *up;
	struct refclockproc *pp;

	/*
	 * Send a "\r*toc\r" to get things going. We go to great pains
	 * to avoid changing state, since there may be more than one
	 * eavesdropper watching the radio.
	 */
	pp = peer->procptr;
	up = (struct as2201unit *)pp->unitptr;
	if (up->pollcnt == 0)
		refclock_report(peer, CEVNT_TIMEOUT);
	else
		up->pollcnt--;
	gettstamp(&pp->lastrec);
	if (write(pp->io.fd, "\r*toc\r", 6) != 6) {
		refclock_report(peer, CEVNT_FAULT);
	} else
		pp->polls++;
}

#endif /* REFCLOCK */
