/*
 * refclock_irig - clock driver for the IRIG audio decoder
 */
#if defined(REFCLOCK) && defined(IRIG) && defined(sun)

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/ioccom.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_unixtime.h"
#include <sys/bsd_audioirig.h>
#include "ntp_stdlib.h"

/*
 * This driver supports the IRIG audio decoder. This clever gadget uses
 * a modified BSD audio driver for the Sun SPARCstation which provides
 * a timestamp, raw binary timecode, status byte and decoded ASCII
 * timecode. The data are represented in the structure in the
 * sys/bsd_audioirig.h header file:
 *
 * struct irig_time {
 *	struct timeval stamp;	timestamp
 *	u_char	bits[13];	100 IRIG data bits
 *	u_char	status;		status byte
 *	char	time[14];	time string (null terminated)
 *
 * where stamp represents a timestamp at the zero crossing of the index
 * marker at the second's epoch, bits is a 13-octet, zero-padded binary-
 * coded string representing code elements 1 through 100 in the IRIG-B
 * code format, and status is a status bute, The decoded timestamp is a
 * 13-octet, null-terminated ASCII string "ddd hh:mm:ss*", where ddd is
 * the day of year, hh:mm:ss the time of day and * is a status
 * indicator, with " " indicating valid time and "?" indicating
 * something wrong.
 *
 * The timestamp is in Unix timeval format, consisting of two 32-bit
 * words, the first of which is the seconds since 1970 and the second is
 * the fraction of the second in microseconds. The status byte is zero
 * if (a) the input signal is within amplitude tolerances, (b) the raw
 * binary timecode contains only valid code elements, (c) 11 position
 * identifiers have been found at the expected element positions, (d)
 * the clock status byte contained in the timecode is valid, and (e) a
 * time determination has been made since the last read() system call.
 *
 * The 100 elements of the IRIG-B timecode are numbered from 0 through
 * 99. Position identifiers occur at elements 0, 9, 19 and every ten
 * thereafter to 99. The control function (CF) elements begin at element
 * 50 (CF 1) and extend to element 78 (CF 27). The straight-binary-
 * seconds (SBS) field, which encodes the seconds of the UTC day, begins
 * at element 80 (CF 28) and extends to element 97 (CF 44). The encoding
 * of elements 50 (CF 1) through 78 (CF 27) is device dependent. This
 * driver presently does not use the CF elements.
 *
 * Where feasible, the interface should be operated with signature
 * control, so that, if the IRIG signal is lost or malformed, the
 * interface produces an unmodulated signal, rather than possibly random
 * digits. The driver will declare "unsynchronized" in this case.
 *
 * Spectracom Netclock/2 WWVB Synchronized Clock
 *
 * Element	CF	Function
 * -------------------------------------
 * 55		6	time sync status
 * 60-63	10-13	bcd year units
 * 65-68	15-18	bcd year tens
 *
 */

/*
 * IRIG interface definitions
 */
#define	DEVICE		"/dev/irig%d" /* device name and unit */
#define	PRECISION	(-13)	/* precision assumed (100 us) */
#define	REFID		"IRIG"	/* reference ID */
#define	DESCRIPTION	"IRIG Audio Decoder" /* WRU */

#define	NSAMPLES	3	/* stages of median filter */
#define IRIG_FORMAT	1	/* IRIG timestamp format */

/*
 * Imported from ntp_timer module
 */
extern u_long current_time;	/* current time (s) */

/*
 * Imported from ntpd module
 */
extern int debug;		/* global debug flag */

/*
 * Function prototypes
 */
static	int	irig_start	P((int, struct peer *));
static	void	irig_shutdown	P((int, struct peer *));
static	void	irig_poll	P((int, struct peer *));

/*
 * Transfer vector
 */
struct	refclock refclock_irig = {
	irig_start,		/* start up driver */
	irig_shutdown,		/* shut down driver */
	irig_poll,		/* transmit poll message */
	noentry,		/* not used (old irig_control) */
	noentry,		/* initialize driver (not used) */
	noentry,		/* not used (old irig_buginfo) */
	NOFLAGS			/* not used */
};


/*
 * irig_start - open the device and initialize data for processing
 */
static int
irig_start(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct refclockproc *pp;
	char device[20];
	int fd;
	int format = IRIG_FORMAT;

	/*
         * Open audio device and set format
         */
	(void)sprintf(device, DEVICE, unit);
	fd = open(device, O_RDONLY | O_NDELAY, 0777);
	if (fd == -1) {
		syslog(LOG_ERR, "irig_start: open of %s: %m", device);
		return (0);
	}
	if (ioctl(fd, AUDIO_IRIG_OPEN, 0) < 0) {
		syslog(LOG_ERR, "irig_start: AUDIO_IRIG_OPEN %m");
		close(fd);
		return (0);
	}
	if (ioctl(fd, AUDIO_IRIG_SETFORMAT, (char *)&format) < 0) {
		syslog(LOG_ERR, "irig_start: AUDIO_IRIG_SETFORMAT %m",
		    DEVICE);
		close(fd);
		return (0);
	}
	pp = peer->procptr;
	pp->io.clock_recv = noentry;
	pp->io.srcclock = (caddr_t)peer;
	pp->io.datalen = 0;
	pp->io.fd = fd;

	/*
	 * Initialize miscellaneous variables
	 */
	peer->precision = PRECISION;
	pp->clockdesc = DESCRIPTION;
	memcpy((char *)&pp->refid, REFID, 4);
	return (1);
}


/*
 * irig_shutdown - shut down the clock
 */
static void
irig_shutdown(unit, peer)
	int unit;
	struct peer *peer;
{
	struct refclockproc *pp;

	pp = peer->procptr;
	io_closeclock(&pp->io);
}


/*
 * irig_poll - called by the transmit procedure
 */
static void
irig_poll(unit, peer)
	int unit;
	struct peer *peer;
{

	struct refclockproc *pp;
	struct irig_time buf;
	char *cp, *dp;
	u_char *dpt;
	int i;

	pp = peer->procptr;
	if (read(pp->io.fd, (char *) &buf, sizeof(buf)) != sizeof(buf)) {
		refclock_report(peer, CEVNT_FAULT);
		return;
	}
	pp->polls++;

#ifdef DEBUG
	if (debug) {
		dpt = (u_char *)&buf;
		printf("irig: ");
		for (i = 0; i < sizeof(buf); i++)
			printf("%02x", *dpt++);
		printf("\n");
	}
#endif

	buf.stamp.tv_sec += JAN_1970;
	TVTOTS(&buf.stamp, &pp->lastrec);
	cp = buf.time;
	dp = pp->lastcode;
	for (i = 0; i < sizeof(buf.time); i++)
		*dp++ = *cp++;
	*--dp = '\0';
	pp->lencode = dp - pp->lastcode;

#ifdef DEBUG
	if (debug)
		printf("irig: time %s timecode %d %s\n",
		    ulfptoa(&pp->lastrec, 6), pp->lencode,
		    pp->lastcode);
#endif
	record_clock_stats(&peer->srcadr, pp->lastcode);

	/*
         * Get IRIG time and convert to timestamp format
         */
	if (sscanf(pp->lastcode, "%3d %2d:%2d:%2d",
	    &pp->day, &pp->hour, &pp->minute, &pp->second) != 4) {
		refclock_report(peer, CEVNT_BADREPLY);
		return;
	}
	if (pp->lastcode[12] != ' ') {
		pp->leap = LEAP_NOTINSYNC;
	} else {
		pp->leap = 0;
		pp->lasttime = current_time;
	}

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
}

#endif
