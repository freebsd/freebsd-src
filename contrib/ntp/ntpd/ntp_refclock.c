/*
 * ntp_refclock - processing support for reference clocks
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif /* HAVE_SYS_IOCTL_H */

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_unixtime.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"

#ifdef REFCLOCK

#ifdef TTYCLK
# ifdef HAVE_SYS_CLKDEFS_H
#  include <sys/clkdefs.h>
# endif
# ifdef HAVE_SYS_SIO_H
#  include <sys/sio.h>
# endif
#endif /* TTYCLK */

#ifdef HAVE_PPSCLOCK_H
#include <sys/ppsclock.h>
#endif /* HAVE_PPSCLOCK_H */

#ifdef HAVE_PPSAPI
# ifdef HAVE_TIMEPPS_H
#  include <timepps.h>
# else
#  ifdef HAVE_SYS_TIMEPPS_H
#   include <sys/timepps.h>
#  endif
# endif
#endif /* HAVE_PPSAPI */

/*
 * Reference clock support is provided here by maintaining the fiction
 * that the clock is actually a peer. As no packets are exchanged with a
 * reference clock, however, we replace the transmit, receive and packet
 * procedures with separate code to simulate them. Routines
 * refclock_transmit() and refclock_receive() maintain the peer
 * variables in a state analogous to an actual peer and pass reference
 * clock data on through the filters. Routines refclock_peer() and
 * refclock_unpeer() are called to initialize and terminate reference
 * clock associations. A set of utility routines is included to open
 * serial devices, process sample data, edit input lines to extract
 * embedded timestamps and to peform various debugging functions.
 *
 * The main interface used by these routines is the refclockproc
 * structure, which contains for most drivers the decimal equivalants of
 * the year, day, month, hour, second and millisecond/microsecond
 * decoded from the ASCII timecode. Additional information includes the
 * receive timestamp, exception report, statistics tallies, etc. In
 * addition, there may be a driver-specific unit structure used for
 * local control of the device.
 *
 * The support routines are passed a pointer to the peer structure,
 * which is used for all peer-specific processing and contains a pointer
 * to the refclockproc structure, which in turn containes a pointer to
 * the unit structure, if used. The peer structure is identified by an
 * interface address in the dotted quad form 127.127.t.u, where t is the
 * clock type and u the unit. Some legacy drivers derive the
 * refclockproc structure pointer from the table typeunit[type][unit].
 * This interface is strongly discouraged and may be abandoned in
 * future.
 *
 * The routines include support for the 1-pps signal provided by some
 * radios and connected via a level converted described in the gadget
 * directory. The signal is captured using a serial port and one of
 * three STREAMS modules described in the refclock_atom.c file. For the
 * highest precision, the signal is captured using the carrier-detect
 * line of a serial port and either the ppsclock or ppsapi streams
 * module or some devilish ioctl() folks keep slipping in as a patch. Be
 * advised ALL support for other than the duly standardized ppsapi
 * interface will eventually be withdrawn.
 */
#define MAXUNIT 	4	/* max units */

#if defined(PPS) || defined(HAVE_PPSAPI)
int fdpps;			/* pps file descriptor */
#endif /* PPS HAVE_PPSAPI */

#define FUDGEFAC	.1	/* fudge correction factor */

/*
 * Type/unit peer index. Used to find the peer structure for control and
 * debugging. When all clock drivers have been converted to new style,
 * this dissapears.
 */
static struct peer *typeunit[REFCLK_MAX + 1][MAXUNIT];

/*
 * Forward declarations
 */
#ifdef QSORT_USES_VOID_P
static int refclock_cmpl_fp P((const void *, const void *));
#else
static int refclock_cmpl_fp P((const double *, const double *));
#endif /* QSORT_USES_VOID_P */
static int refclock_sample P((struct refclockproc *));

#ifdef HAVE_PPSAPI
extern int pps_assert;		/* capture edge 1:assert, 0:clear */
extern int pps_hardpps;		/* PPS kernel 1:on, 0:off */
#endif /* HAVE_PPSAPI */

/*
 * refclock_report - note the occurance of an event
 *
 * This routine presently just remembers the report and logs it, but
 * does nothing heroic for the trap handler. It tries to be a good
 * citizen and bothers the system log only if things change.
 */
void
refclock_report(
	struct peer *peer,
	int code
	)
{
	struct refclockproc *pp;

	if (!(pp = peer->procptr))
		return;
	if (code == CEVNT_BADREPLY)
		pp->badformat++;
	if (code == CEVNT_BADTIME)
		pp->baddata++;
	if (code == CEVNT_TIMEOUT)
		pp->noreply++;
	if (pp->currentstatus != code) {
		pp->currentstatus = code;
		pp->lastevent = code;
		if (code == CEVNT_FAULT)
			msyslog(LOG_ERR,
				"clock %s event '%s' (0x%02x)",
				refnumtoa(peer->srcadr.sin_addr.s_addr),
				ceventstr(code), code);
		else {
			NLOG(NLOG_CLOCKEVENT)
				msyslog(LOG_INFO,
				"clock %s event '%s' (0x%02x)",
				refnumtoa(peer->srcadr.sin_addr.s_addr),
				ceventstr(code), code);
		}
	}
#ifdef DEBUG
	if (debug)
		printf("clock %s event '%s' (0x%02x)\n",
			refnumtoa(peer->srcadr.sin_addr.s_addr),
			ceventstr(code), code);
#endif
}


/*
 * init_refclock - initialize the reference clock drivers
 *
 * This routine calls each of the drivers in turn to initialize internal
 * variables, if necessary. Most drivers have nothing to say at this
 * point.
 */
void
init_refclock(void)
{
	int i, j;

	for (i = 0; i < (int)num_refclock_conf; i++) {
		if (refclock_conf[i]->clock_init != noentry)
			(refclock_conf[i]->clock_init)();
		for (j = 0; j < MAXUNIT; j++)
			typeunit[i][j] = 0;
	}
}


/*
 * refclock_newpeer - initialize and start a reference clock
 *
 * This routine allocates and initializes the interface structure which
 * supports a reference clock in the form of an ordinary NTP peer. A
 * driver-specific support routine completes the initialization, if
 * used. Default peer variables which identify the clock and establish
 * its reference ID and stratum are set here. It returns one if success
 * and zero if the clock address is invalid or already running,
 * insufficient resources are available or the driver declares a bum
 * rap.
 */
int
refclock_newpeer(
	struct peer *peer	/* peer structure pointer */
	)
{
	struct refclockproc *pp;
	u_char clktype;
	int unit;

	/*
	 * Check for valid clock address. If already running, shut it
	 * down first.
	 */
	if (!ISREFCLOCKADR(&peer->srcadr)) {
		msyslog(LOG_ERR,
			"refclock_newpeer: clock address %s invalid",
			ntoa(&peer->srcadr));
		return (0);
	}
	clktype = (u_char)REFCLOCKTYPE(&peer->srcadr);
	unit = REFCLOCKUNIT(&peer->srcadr);
	if (clktype >= num_refclock_conf || unit >= MAXUNIT ||
		refclock_conf[clktype]->clock_start == noentry) {
		msyslog(LOG_ERR,
			"refclock_newpeer: clock type %d invalid\n",
			clktype);
		return (0);
	}
	refclock_unpeer(peer);

	/*
	 * Allocate and initialize interface structure
	 */
	if (!(pp = (struct refclockproc *)emalloc(sizeof(struct refclockproc))))
		return (0);
	memset((char *)pp, 0, sizeof(struct refclockproc));
	typeunit[clktype][unit] = peer;
	peer->procptr = pp;

	/*
	 * Initialize structures
	 */
	peer->refclktype = clktype;
	peer->refclkunit = unit;
	peer->flags |= FLAG_REFCLOCK;
	peer->stratum = STRATUM_REFCLOCK;
	peer->refid = peer->srcadr.sin_addr.s_addr;
	peer->maxpoll = peer->minpoll;

	pp->type = clktype;
	pp->timestarted = current_time;

	/*
	 * If the interface has been set to any_interface, set it to the
	 * loopback address if we have one. This is so that peers which
	 * are unreachable are easy to see in the peer display.
	 */
	if (peer->dstadr == any_interface && loopback_interface != 0)
		peer->dstadr = loopback_interface;

	/*
	 * Set peer.pmode based on the hmode. For appearances only.
	 */
	switch (peer->hmode) {

		case MODE_ACTIVE:
		peer->pmode = MODE_PASSIVE;
		break;

		default:
		peer->pmode = MODE_SERVER;
		break;
	}

	/*
	 * Do driver dependent initialization. The above defaults
	 * can be wiggled, then finish up for consistency.
	 */
	if (!((refclock_conf[clktype]->clock_start)(unit, peer))) {
		free(pp);
		return (0);
	}
	peer->hpoll = peer->minpoll;
	peer->ppoll = peer->maxpoll;
	if (peer->stratum <= 1)
		peer->refid = pp->refid;
	else
		peer->refid = peer->srcadr.sin_addr.s_addr;
	return (1);
}


/*
 * refclock_unpeer - shut down a clock
 */
void
refclock_unpeer(
	struct peer *peer	/* peer structure pointer */
	)
{
	u_char clktype;
	int unit;

	/*
	 * Wiggle the driver to release its resources, then give back
	 * the interface structure.
	 */
	if (!peer->procptr)
		return;
	clktype = peer->refclktype;
	unit = peer->refclkunit;
	if (refclock_conf[clktype]->clock_shutdown != noentry)
		(refclock_conf[clktype]->clock_shutdown)(unit, peer);
	free(peer->procptr);
	peer->procptr = 0;
}


/*
 * refclock_transmit - simulate the transmit procedure
 *
 * This routine implements the NTP transmit procedure for a reference
 * clock. This provides a mechanism to call the driver at the NTP poll
 * interval, as well as provides a reachability mechanism to detect a
 * broken radio or other madness.
 */
void
refclock_transmit(
	struct peer *peer	/* peer structure pointer */
	)
{
	u_char clktype;
	int unit;
	int hpoll;
	u_long next;

	clktype = peer->refclktype;
	unit = peer->refclkunit;
	peer->sent++;

	/*
	 * This is a ripoff of the peer transmit routine, but
	 * specialized for reference clocks. We do a little less
	 * protocol here and call the driver-specific transmit routine.
	 */
	hpoll = peer->hpoll;
	next = peer->outdate;
	if (peer->burst == 0) {
		u_char oreach;
#ifdef DEBUG
		if (debug)
			printf("refclock_transmit: at %ld %s\n",
			    current_time, ntoa(&(peer->srcadr)));
#endif

		/*
		 * Update reachability and poll variables like the
		 * network code.
		 */
		oreach = peer->reach;
		if (oreach & 0x01)
			peer->valid++;
		if (oreach & 0x80)
			peer->valid--;
				peer->reach <<= 1;
		if (peer->reach == 0) {
			if (oreach != 0) {
				report_event(EVNT_UNREACH, peer);
				peer->timereachable = current_time;
				peer_clear(peer);
			}
		} else {
			if ((oreach & 0x03) == 0) {
				clock_filter(peer, 0., 0., MAXDISPERSE);
				clock_select();
			}
			if (peer->valid <= 2) {
				hpoll--;
			} else if (peer->valid > NTP_SHIFT - 2)
				hpoll++;
			if (peer->flags & FLAG_BURST)
				peer->burst = NSTAGE;
		}
		next = current_time;
	}
	get_systime(&peer->xmt);
	if (refclock_conf[clktype]->clock_poll != noentry)
		(refclock_conf[clktype]->clock_poll)(unit, peer);
	peer->outdate = next;
	poll_update(peer, hpoll);
	if (peer->burst > 0)
		peer->burst--;
	poll_update(peer, hpoll);
}


/*
 * Compare two doubles - used with qsort()
 */
#ifdef QSORT_USES_VOID_P
static int
refclock_cmpl_fp(
	const void *p1,
	const void *p2
	)
{
	const double *dp1 = (const double *)p1;
	const double *dp2 = (const double *)p2;

	if (*dp1 < *dp2)
		return (-1);
	if (*dp1 > *dp2)
		return (1);
	return (0);
}
#else
static int
refclock_cmpl_fp(
	const double *dp1,
	const double *dp2
	)
{
	if (*dp1 < *dp2)
		return (-1);
	if (*dp1 > *dp2)
		return (1);
	return (0);
}
#endif /* QSORT_USES_VOID_P */


/*
 * refclock_process_offset - update median filter
 *
 * This routine uses the given offset and timestamps to construct a new entry in the median filter circular buffer. Samples that overflow the filter are quietly discarded.
 */
void
refclock_process_offset(
	struct refclockproc *pp,
	l_fp offset,
	l_fp lastrec,
	double fudge
	)
{
	double doffset;

	pp->lastref = offset;
	pp->lastrec = lastrec;
	pp->variance = 0;
	L_SUB(&offset, &lastrec);
	LFPTOD(&offset, doffset);
	SAMPLE(doffset + fudge);
}

/*
 * refclock_process - process a sample from the clock
 *
 * This routine converts the timecode in the form days, hours, minutes,
 * seconds and milliseconds/microseconds to internal timestamp format,
 * then constructs a new entry in the median filter circular buffer.
 * Return success (1) if the data are correct and consistent with the
 * converntional calendar.
*/
int
refclock_process(
	struct refclockproc *pp
	)
{
	l_fp offset;

	/*
	 * Compute the timecode timestamp from the days, hours, minutes,
	 * seconds and milliseconds/microseconds of the timecode. Use
	 * clocktime() for the aggregate seconds and the msec/usec for
	 * the fraction, when present. Note that this code relies on the
	 * filesystem time for the years and does not use the years of
	 * the timecode.
	 */
	if (!clocktime(pp->day, pp->hour, pp->minute, pp->second, GMT,
		pp->lastrec.l_ui, &pp->yearstart, &offset.l_ui))
		return (0);
	if (pp->usec) {
		TVUTOTSF(pp->usec, offset.l_uf);
	} else {
		MSUTOTSF(pp->msec, offset.l_uf);
	}
	refclock_process_offset(pp, offset, pp->lastrec,
	    pp->fudgetime1);
	return (1);
}

/*
 * refclock_sample - process a pile of samples from the clock
 *
 * This routine implements a recursive median filter to suppress spikes
 * in the data, as well as determine a performance statistic. It
 * calculates the mean offset and mean-square variance. A time
 * adjustment fudgetime1 can be added to the final offset to compensate
 * for various systematic errors. The routine returns the number of
 * samples processed, which could be 0.
 */
static int
refclock_sample(
	struct refclockproc *pp
	)
{
	int i, j, k, n;
	double offset, disp;
	double off[MAXSTAGE];

	/*
	 * Copy the raw offsets and sort into ascending order. Don't do
	 * anything if the buffer is empty.
	 */
	if (pp->codeproc == pp->coderecv)
		return (0);
	n = 0;
	while (pp->codeproc != pp->coderecv)
		off[n++] = pp->filter[pp->codeproc++ % MAXSTAGE];
	if (n > 1)
		qsort((char *)off, n, sizeof(double), refclock_cmpl_fp);

	/*
	 * Reject the furthest from the median of the samples until
	 * approximately 60 percent of the samples remain.
	 */
	i = 0; j = n;
	k = n - (n * 2) / NSTAGE;
	while ((j - i) > k) {
		offset = off[(j + i) / 2];
		if (off[j - 1] - offset < offset - off[i])
			i++;	/* reject low end */
		else
			j--;	/* reject high end */
	}

	/*
	 * Determine the offset and variance.
	 */
	offset = disp = 0;
	for (; i < j; i++) {
		offset += off[i];
		disp += SQUARE(off[i]);
	}
	offset /= k;
	pp->offset = offset;
	pp->variance += disp / k - SQUARE(offset);
#ifdef DEBUG
	if (debug)
		printf(
		    "refclock_sample: n %d offset %.6f disp %.6f std %.6f\n",
		    n, pp->offset, pp->disp, SQRT(pp->variance));
#endif
	return (n);
}


/*
 * refclock_receive - simulate the receive and packet procedures
 *
 * This routine simulates the NTP receive and packet procedures for a
 * reference clock. This provides a mechanism in which the ordinary NTP
 * filter, selection and combining algorithms can be used to suppress
 * misbehaving radios and to mitigate between them when more than one is
 * available for backup.
 */
void
refclock_receive(
	struct peer *peer	/* peer structure pointer */
	)
{
	struct refclockproc *pp;

#ifdef DEBUG
	if (debug)
		printf("refclock_receive: at %lu %s\n",
		    current_time, ntoa(&peer->srcadr));
#endif

	/*
	 * Do a little sanity dance and update the peer structure. Groom
	 * the median filter samples and give the data to the clock
	 * filter.
	 */
	peer->received++;
	pp = peer->procptr;
	peer->processed++;
	peer->timereceived = current_time;
	peer->leap = pp->leap;
	if (peer->leap == LEAP_NOTINSYNC) {
		refclock_report(peer, CEVNT_FAULT);
		return;
	}
	if (peer->reach == 0)
		report_event(EVNT_REACH, peer);
	peer->reach |= 1;
	peer->reftime = peer->org = pp->lastrec;
	peer->rootdispersion = pp->disp + SQRT(pp->variance);
	get_systime(&peer->rec);
	if (!refclock_sample(pp))
		return;
	clock_filter(peer, pp->offset, 0., 0.);
	clock_select();
	record_peer_stats(&peer->srcadr, ctlpeerstatus(peer),
	    peer->offset, peer->delay, CLOCK_PHI * (current_time -
	    peer->epoch), SQRT(peer->variance));
	if (pps_control && pp->sloppyclockflag & CLK_FLAG1)
		pp->fudgetime1 -= pp->offset * FUDGEFAC;
}

/*
 * refclock_gtlin - groom next input line and extract timestamp
 *
 * This routine processes the timecode received from the clock and
 * removes the parity bit and control characters. If a timestamp is
 * present in the timecode, as produced by the tty_clk STREAMS module,
 * it returns that as the timestamp; otherwise, it returns the buffer
 *  timestamp. The routine return code is the number of characters in
 * the line.
 */
int
refclock_gtlin(
	struct recvbuf *rbufp,	/* receive buffer pointer */
	char *lineptr,		/* current line pointer */
	int bmax,		/* remaining characters in line */
	l_fp *tsptr 	/* pointer to timestamp returned */
	)
{
	char *dpt, *dpend, *dp;
	int i;
	l_fp trtmp, tstmp;
	char c;
#ifdef TIOCDCDTIMESTAMP
	struct timeval dcd_time;
#endif /* TIOCDCDTIMESTAMP */
#ifdef HAVE_PPSAPI
	pps_info_t pi;
	struct timespec timeout, *tsp;
	double a;
#endif /* HAVE_PPSAPI */

	/*
	 * Check for the presence of a timestamp left by the tty_clock
	 * module and, if present, use that instead of the buffer
	 * timestamp captured by the I/O routines. We recognize a
	 * timestamp by noting its value is earlier than the buffer
	 * timestamp, but not more than one second earlier.
	 */
	dpt = (char *)&rbufp->recv_space;
	dpend = dpt + rbufp->recv_length;
	trtmp = rbufp->recv_time;

#ifdef HAVE_PPSAPI
	timeout.tv_sec = 0;
	timeout.tv_nsec = 0;
	if ((rbufp->fd == fdpps) &&
	    (time_pps_fetch(fdpps, PPS_TSFMT_TSPEC, &pi, &timeout) >= 0)) {
		if(pps_assert)
			tsp = &pi.assert_timestamp;
		else
			tsp = &pi.clear_timestamp;
		a = tsp->tv_nsec;
		a /= 1e9;
		tstmp.l_uf =  a * 4294967296.0;
		tstmp.l_ui = tsp->tv_sec;
		tstmp.l_ui += JAN_1970;
		L_SUB(&trtmp, &tstmp);
		if (trtmp.l_ui == 0) {
#ifdef DEBUG
			if (debug > 1) {
				printf(
				    "refclock_gtlin: fd %d time_pps_fetch %s",
				    fdpps, lfptoa(&tstmp, 6));
				printf(" sigio %s\n", lfptoa(&trtmp, 6));
			}
#endif
			trtmp = tstmp;
			goto gotit;
		} else
			trtmp = rbufp->recv_time;
	}
#endif /* HAVE_PPSAPI */
#ifdef TIOCDCDTIMESTAMP
	if(ioctl(rbufp->fd, TIOCDCDTIMESTAMP, &dcd_time) != -1) {
		TVTOTS(&dcd_time, &tstmp);
		tstmp.l_ui += JAN_1970;
		L_SUB(&trtmp, &tstmp);
		if (trtmp.l_ui == 0) {
#ifdef DEBUG
			if (debug > 1) {
				printf(
				    "refclock_gtlin: fd %d DCDTIMESTAMP %s",
				    rbufp->fd, lfptoa(&tstmp, 6));
				printf(" sigio %s\n", lfptoa(&trtmp, 6));
			}
#endif
			trtmp = tstmp;
			goto gotit;
		} else
			trtmp = rbufp->recv_time;
	}
	else
	/* XXX fallback to old method if kernel refuses TIOCDCDTIMESTAMP */
#endif  /* TIOCDCDTIMESTAMP */
	if (dpend >= dpt + 8) {
		if (buftvtots(dpend - 8, &tstmp)) {
			L_SUB(&trtmp, &tstmp);
			if (trtmp.l_ui == 0) {
#ifdef DEBUG
				if (debug > 1) {
					printf(
					    "refclock_gtlin: fd %d ldisc %s",
					    rbufp->fd, lfptoa(&trtmp, 6));
					get_systime(&trtmp);
					L_SUB(&trtmp, &tstmp);
					printf(" sigio %s\n", lfptoa(&trtmp, 6));
				}
#endif
				dpend -= 8;
				trtmp = tstmp;
			} else
				trtmp = rbufp->recv_time;
		}
	}

#if defined(HAVE_PPSAPI) || defined(TIOCDCDTIMESTAMP)
gotit:
#endif
	/*
	 * Edit timecode to remove control chars. Don't monkey with the
	 * line buffer if the input buffer contains no ASCII printing
	 * characters.
	 */
	if (dpend - dpt > bmax - 1)
		dpend = dpt + bmax - 1;
	for (dp = lineptr; dpt < dpend; dpt++) {
		c = *dpt & 0x7f;
		if (c >= ' ')
			*dp++ = c;
	}
	i = dp - lineptr;
	if (i > 0)
		*dp = '\0';
#ifdef DEBUG
	if (debug > 1 && i > 0)
		printf("refclock_gtlin: fd %d time %s timecode %d %s\n",
		    rbufp->fd, ulfptoa(&trtmp, 6), i, lineptr);
#endif
	*tsptr = trtmp;
	return (i);
}

/*
 * The following code does not apply to WINNT & VMS ...
 */
#if !defined SYS_VXWORKS && !defined SYS_WINNT
#if defined(HAVE_TERMIOS) || defined(HAVE_SYSV_TTYS) || defined(HAVE_BSD_TTYS)

/*
 * refclock_open - open serial port for reference clock
 *
 * This routine opens a serial port for I/O and sets default options. It
 * returns the file descriptor if success and zero if failure.
 */
int
refclock_open(
	char *dev,		/* device name pointer */
	int speed,		/* serial port speed (code) */
	int lflags		/* line discipline flags */
	)
{
	int fd, i;
	int flags;
#ifdef HAVE_TERMIOS
	struct termios ttyb, *ttyp;
#endif /* HAVE_TERMIOS */
#ifdef HAVE_SYSV_TTYS
	struct termio ttyb, *ttyp;
#endif /* HAVE_SYSV_TTYS */
#ifdef HAVE_BSD_TTYS
	struct sgttyb ttyb, *ttyp;
#endif /* HAVE_BSD_TTYS */
#ifdef TIOCMGET
	u_long ltemp;
#endif /* TIOCMGET */

	/*
	 * Open serial port and set default options
	 */
	flags = lflags;
	if (strcmp(dev, pps_device) == 0)
		flags |= LDISC_PPS;
#ifdef O_NONBLOCK
	fd = open(dev, O_RDWR | O_NONBLOCK, 0777);
#else
	fd = open(dev, O_RDWR, 0777);
#endif /* O_NONBLOCK */
	if (fd == -1) {
		msyslog(LOG_ERR, "refclock_open: %s: %m", dev);
		return (0);
	}

	/*
	 * The following sections initialize the serial line port in
	 * canonical (line-oriented) mode and set the specified line
	 * speed, 8 bits and no parity. The modem control, break, erase
	 * and kill functions are normally disabled. There is a
	 * different section for each terminal interface, as selected at
	 * compile time.
	 */
	ttyp = &ttyb;

#ifdef HAVE_TERMIOS
	/*
	 * POSIX serial line parameters (termios interface)
	 */
	if (tcgetattr(fd, ttyp) < 0) {
		msyslog(LOG_ERR,
			"refclock_open: fd %d tcgetattr: %m", fd);
		return (0);
	}

	/*
	 * Set canonical mode and local connection; set specified speed,
	 * 8 bits and no parity; map CR to NL; ignore break.
	 */
	ttyp->c_iflag = IGNBRK | IGNPAR | ICRNL;
	ttyp->c_oflag = 0;
	ttyp->c_cflag = CS8 | CLOCAL | CREAD;
	(void)cfsetispeed(&ttyb, (u_int)speed);
	(void)cfsetospeed(&ttyb, (u_int)speed);
	ttyp->c_lflag = ICANON;
	for (i = 0; i < NCCS; ++i)
	{
		ttyp->c_cc[i] = '\0';
	}

	/*
	 * Some special cases
	 */
	if (flags & LDISC_RAW) {
		ttyp->c_iflag = 0;
		ttyp->c_lflag = 0;
		ttyp->c_cc[VMIN] = 1;
	}
#if defined(TIOCMGET) && !defined(SCO5_CLOCK)
	/*
	 * If we have modem control, check to see if modem leads are
	 * active; if so, set remote connection. This is necessary for
	 * the kernel pps mods to work.
	 */
	ltemp = 0;
	if (ioctl(fd, TIOCMGET, (char *)&ltemp) < 0)
		msyslog(LOG_ERR,
			"refclock_open: fd %d TIOCMGET failed: %m", fd);
#ifdef DEBUG
	if (debug)
		printf("refclock_open: fd %d modem status 0x%lx\n",
		    fd, ltemp);
#endif
	if (ltemp & TIOCM_DSR)
		ttyp->c_cflag &= ~CLOCAL;
#endif /* TIOCMGET */
	if (tcsetattr(fd, TCSANOW, ttyp) < 0) {
		msyslog(LOG_ERR,
		    "refclock_open: fd %d TCSANOW failed: %m", fd);
		return (0);
	}
	if (tcflush(fd, TCIOFLUSH) < 0) {
		msyslog(LOG_ERR,
		    "refclock_open: fd %d TCIOFLUSH failed: %m", fd);
		return (0);
	}
#endif /* HAVE_TERMIOS */

#ifdef HAVE_SYSV_TTYS

	/*
	 * System V serial line parameters (termio interface)
	 *
	 */
	if (ioctl(fd, TCGETA, ttyp) < 0) {
		msyslog(LOG_ERR,
		    "refclock_open: fd %d TCGETA failed: %m", fd);
		return (0);
	}

	/*
	 * Set canonical mode and local connection; set specified speed,
	 * 8 bits and no parity; map CR to NL; ignore break.
	 */
	ttyp->c_iflag = IGNBRK | IGNPAR | ICRNL;
	ttyp->c_oflag = 0;
	ttyp->c_cflag = speed | CS8 | CLOCAL | CREAD;
	ttyp->c_lflag = ICANON;
	ttyp->c_cc[VERASE] = ttyp->c_cc[VKILL] = '\0';

	/*
	 * Some special cases
	 */
	if (flags & LDISC_RAW) {
		ttyp->c_iflag = 0;
		ttyp->c_lflag = 0;
	}
#ifdef TIOCMGET
	/*
	 * If we have modem control, check to see if modem leads are
	 * active; if so, set remote connection. This is necessary for
	 * the kernel pps mods to work.
	 */
	ltemp = 0;
	if (ioctl(fd, TIOCMGET, (char *)&ltemp) < 0)
		msyslog(LOG_ERR,
		    "refclock_open: fd %d TIOCMGET failed: %m", fd);
#ifdef DEBUG
	if (debug)
		printf("refclock_open: fd %d modem status %lx\n",
		    fd, ltemp);
#endif
	if (ltemp & TIOCM_DSR)
		ttyp->c_cflag &= ~CLOCAL;
#endif /* TIOCMGET */
	if (ioctl(fd, TCSETA, ttyp) < 0) {
		msyslog(LOG_ERR,
		    "refclock_open: fd %d TCSETA failed: %m", fd);
		return (0);
	}
#endif /* HAVE_SYSV_TTYS */

#ifdef HAVE_BSD_TTYS

	/*
	 * 4.3bsd serial line parameters (sgttyb interface)
	 */
	if (ioctl(fd, TIOCGETP, (char *)ttyp) < 0) {
		msyslog(LOG_ERR,
		    "refclock_open: fd %d TIOCGETP %m", fd);
		return (0);
	}
	ttyp->sg_ispeed = ttyp->sg_ospeed = speed;
	ttyp->sg_flags = EVENP | ODDP | CRMOD;
	if (ioctl(fd, TIOCSETP, (char *)ttyp) < 0) {
		msyslog(LOG_ERR,
		    "refclock_open: TIOCSETP failed: %m");
		return (0);
	}
#endif /* HAVE_BSD_TTYS */
	if (!refclock_ioctl(fd, flags)) {
		(void)close(fd);
		msyslog(LOG_ERR,
		    "refclock_open: fd %d ioctl failed: %m", fd);
		return (0);
	}
	return (fd);
}
#endif /* HAVE_TERMIOS || HAVE_SYSV_TTYS || HAVE_BSD_TTYS */
#endif /* SYS_VXWORKS SYS_WINNT */

/*
 * refclock_ioctl - set serial port control functions
 *
 * This routine attempts to hide the internal, system-specific details
 * of serial ports. It can handle POSIX (termios), SYSV (termio) and BSD
 * (sgtty) interfaces with varying degrees of success. The routine sets
 * up optional features such as tty_clk, ppsclock and ppsapi, as well as
 * their many other variants. The routine returns 1 if success and 0 if
 * failure.
 */
int
refclock_ioctl(
	int fd, 		/* file descriptor */
	int flags		/* line discipline flags */
	)
{
	/* simply return 1 if no UNIX line discipline is supported */
#if !defined SYS_VXWORKS && !defined SYS_WINNT
#if defined(HAVE_TERMIOS) || defined(HAVE_SYSV_TTYS) || defined(HAVE_BSD_TTYS)

#ifdef TTYCLK
#ifdef HAVE_TERMIOS
	struct termios ttyb, *ttyp;
#endif /* HAVE_TERMIOS */
#ifdef HAVE_SYSV_TTYS
	struct termio ttyb, *ttyp;
#endif /* HAVE_SYSV_TTYS */
#ifdef HAVE_BSD_TTYS
	struct sgttyb ttyb, *ttyp;
#endif /* HAVE_BSD_TTYS */
#endif /* TTYCLK */

#ifdef DEBUG
	if (debug)
		printf("refclock_ioctl: fd %d flags 0x%x\n", fd, flags);
#endif

	/*
	 * The following sections select optional features, such as
	 * modem control, PPS capture and so forth. Some require
	 * specific operating system support in the form of STREAMS
	 * modules, which can be loaded and unloaded at run time without
	 * rebooting the kernel. The STREAMS modules require System
	 * V STREAMS support. The checking frenzy is attenuated here,
	 * since the device is already open.
	 *
	 * Note that the tty_clk and ppsclock modules are optional; if
	 * configured and unavailable, the dang thing still works, but
	 * the accuracy improvement using them will not be available.
	 * The only known implmentations of these moldules are specific
	 * to SunOS 4.x. Use the ppsclock module ONLY with Sun baseboard
	 * ttya or ttyb. Using it with the SPIF multipexor crashes the
	 * kernel.
	 *
	 * The preferred way to capture PPS timestamps is using the
	 * ppsapi interface, which is machine independent. The SunOS 4.x
	 * and Digital Unix 4.x interfaces use STREAMS modules and
	 * support both the ppsapi specification and ppsclock
	 * functionality, but other systems may vary widely.
	 */
	if (flags == 0)
		return (1);
#if !(defined(HAVE_TERMIOS) || defined(HAVE_BSD_TTYS))
	if (flags & (LDISC_CLK | LDISC_PPS | LDISC_ACTS)) {
		msyslog(LOG_ERR,
			"refclock_ioctl: unsupported terminal interface");
		return (0);
	}
#endif /* HAVE_TERMIOS HAVE_BSD_TTYS */
#ifdef TTYCLK
	ttyp = &ttyb;
#endif /* TTYCLK */

	/*
	 * The following features may or may not require System V
	 * STREAMS support, depending on the particular implementation.
	 */
#if defined(TTYCLK)
	/*
	 * The TTYCLK option provides timestamping at the driver level.
	 * It requires the tty_clk streams module and System V STREAMS
	 * support. If not available, don't complain.
	 */
	if (flags & (LDISC_CLK | LDISC_CLKPPS | LDISC_ACTS)) {
		int rval = 0;

		if (ioctl(fd, I_PUSH, "clk") < 0) {
			msyslog(LOG_NOTICE,
			    "refclock_ioctl: I_PUSH clk failed: %m");
		} else {
			char *str;

			if (flags & LDISC_CLKPPS)
				str = "\377";
			else if (flags & LDISC_ACTS)
				str = "*";
			else
				str = "\n";
#ifdef CLK_SETSTR
			if ((rval = ioctl(fd, CLK_SETSTR, str)) < 0)
				msyslog(LOG_ERR,
				    "refclock_ioctl: CLK_SETSTR failed: %m");
			if (debug)
				printf("refclock_ioctl: fd %d CLK_SETSTR %d str %s\n",
				    fd, rval, str);
#endif
		}
	}
#endif /* TTYCLK */

#if defined(PPS) && !defined(HAVE_PPSAPI)
	/*
	 * The PPS option provides timestamping at the driver level.
	 * It uses a 1-pps signal and level converter (gadget box) and
	 * requires the ppsclock streams module and System V STREAMS
	 * support. This option has been superseded by the ppsapi
	 * option and may be withdrawn in future.
	 */
	if (flags & LDISC_PPS) {
		int rval = 0;
#ifdef HAVE_TIOCSPPS		/* Solaris */
		int one = 1;
#endif /* HAVE_TIOCSPPS */

		if (fdpps > 0) {
			msyslog(LOG_ERR,
				"refclock_ioctl: PPS already configured");
			return (0);
		}
#ifdef HAVE_TIOCSPPS		/* Solaris */
		if (ioctl(fd, TIOCSPPS, &one) < 0) {
			msyslog(LOG_NOTICE,
				"refclock_ioctl: TIOCSPPS failed: %m");
			return (0);
		}
		if (debug)
			printf("refclock_ioctl: fd %d TIOCSPPS %d\n",
			    fd, rval);
#else
		if (ioctl(fd, I_PUSH, "ppsclock") < 0) {
			msyslog(LOG_NOTICE,
				"refclock_ioctl: I_PUSH ppsclock failed: %m");
			return (0);
		}
		if (debug)
			printf("refclock_ioctl: fd %d ppsclock %d\n",
			    fd, rval);
#endif /* not HAVE_TIOCSPPS */
		fdpps = fd;
	}
#endif /* PPS HAVE_PPSAPI */

#ifdef HAVE_PPSAPI
	/*
	 * The PPSAPI option provides timestamping at the driver level.
	 * It uses a 1-pps signal and level converter (gadget box) and
	 * requires ppsapi compiled into the kernel on non STREAMS
	 * systems. This is the preferred way to capture PPS timestamps
	 * and is expected to become an IETF cross-platform standard.
	 */
	if (flags & (LDISC_PPS | LDISC_CLKPPS)) {
		pps_params_t pp;
		int mode, temp;
		pps_handle_t handle;

		memset((char *)&pp, 0, sizeof(pp));
		if (fdpps > 0) {
			msyslog(LOG_ERR,
			    "refclock_ioctl: ppsapi already configured");
			return (0);
		}
		if (time_pps_create(fd, &handle) < 0) {
			msyslog(LOG_ERR,
			    "refclock_ioctl: time_pps_create failed: %m");
			return (0);
		}
		if (time_pps_getcap(handle, &mode) < 0) {
			msyslog(LOG_ERR,
			    "refclock_ioctl: time_pps_getcap failed: %m");
			return (0);
		}
		pp.mode = mode & PPS_CAPTUREBOTH;
		if (time_pps_setparams(handle, &pp) < 0) {
			msyslog(LOG_ERR,
			    "refclock_ioctl: time_pps_setparams failed: %m");
			return (0);
		}
		if (!pps_hardpps)
			temp = 0;
		else if (pps_assert)
			temp = mode & PPS_CAPTUREASSERT;
		else
			temp = mode & PPS_CAPTURECLEAR;
		if (time_pps_kcbind(handle, PPS_KC_HARDPPS, temp,
		    PPS_TSFMT_TSPEC) < 0) {
			msyslog(LOG_ERR,
			    "refclock_ioctl: time_pps_kcbind failed: %m");
			return (0);
		}
		(void)time_pps_getparams(handle, &pp);
		fdpps = (int)handle;
		if (debug)
			printf(
			    "refclock_ioctl: fd %d ppsapi vers %d mode 0x%x cap 0x%x\n",
			    fdpps, pp.api_version, pp.mode, mode);
	}
#endif /* HAVE_PPSAPI */
#endif /* HAVE_TERMIOS || HAVE_SYSV_TTYS || HAVE_BSD_TTYS */
#endif /* SYS_VXWORKS SYS_WINNT */
	return (1);
}

/*
 * refclock_control - set and/or return clock values
 *
 * This routine is used mainly for debugging. It returns designated
 * values from the interface structure that can be displayed using
 * ntpdc and the clockstat command. It can also be used to initialize
 * configuration variables, such as fudgetimes, fudgevalues, reference
 * ID and stratum.
 */
void
refclock_control(
	struct sockaddr_in *srcadr,
	struct refclockstat *in,
	struct refclockstat *out
	)
{
	struct peer *peer;
	struct refclockproc *pp;
	u_char clktype;
	int unit;

	/*
	 * Check for valid address and running peer
	 */
	if (!ISREFCLOCKADR(srcadr))
		return;
	clktype = (u_char)REFCLOCKTYPE(srcadr);
	unit = REFCLOCKUNIT(srcadr);
	if (clktype >= num_refclock_conf || unit >= MAXUNIT)
		return;
	if (!(peer = typeunit[clktype][unit]))
		return;
	pp = peer->procptr;

	/*
	 * Initialize requested data
	 */
	if (in != 0) {
		if (in->haveflags & CLK_HAVETIME1)
			pp->fudgetime1 = in->fudgetime1;
		if (in->haveflags & CLK_HAVETIME2)
			pp->fudgetime2 = in->fudgetime2;
		if (in->haveflags & CLK_HAVEVAL1)
			peer->stratum = (u_char) in->fudgeval1;
		if (in->haveflags & CLK_HAVEVAL2)
			pp->refid = in->fudgeval2;
		if (peer->stratum <= 1)
			peer->refid = pp->refid;
		else
			peer->refid = peer->srcadr.sin_addr.s_addr;
		if (in->haveflags & CLK_HAVEFLAG1) {
			pp->sloppyclockflag &= ~CLK_FLAG1;
			pp->sloppyclockflag |= in->flags & CLK_FLAG1;
		}
		if (in->haveflags & CLK_HAVEFLAG2) {
			pp->sloppyclockflag &= ~CLK_FLAG2;
			pp->sloppyclockflag |= in->flags & CLK_FLAG2;
		}
		if (in->haveflags & CLK_HAVEFLAG3) {
			pp->sloppyclockflag &= ~CLK_FLAG3;
			pp->sloppyclockflag |= in->flags & CLK_FLAG3;
		}
		if (in->haveflags & CLK_HAVEFLAG4) {
			pp->sloppyclockflag &= ~CLK_FLAG4;
			pp->sloppyclockflag |= in->flags & CLK_FLAG4;
		}
	}

	/*
	 * Readback requested data
	 */
	if (out != 0) {
		out->haveflags = CLK_HAVETIME1 | CLK_HAVEVAL1 |
			CLK_HAVEVAL2 | CLK_HAVEFLAG4;
		out->fudgetime1 = pp->fudgetime1;
		out->fudgetime2 = pp->fudgetime2;
		out->fudgeval1 = peer->stratum;
		out->fudgeval2 = pp->refid;
		out->flags = (u_char) pp->sloppyclockflag;

		out->timereset = current_time - pp->timestarted;
		out->polls = pp->polls;
		out->noresponse = pp->noreply;
		out->badformat = pp->badformat;
		out->baddata = pp->baddata;

		out->lastevent = pp->lastevent;
		out->currentstatus = pp->currentstatus;
		out->type = pp->type;
		out->clockdesc = pp->clockdesc;
		out->lencode = pp->lencode;
		out->p_lastcode = pp->a_lastcode;
	}

	/*
	 * Give the stuff to the clock
	 */
	if (refclock_conf[clktype]->clock_control != noentry)
		(refclock_conf[clktype]->clock_control)(unit, in, out, peer);
}


/*
 * refclock_buginfo - return debugging info
 *
 * This routine is used mainly for debugging. It returns designated
 * values from the interface structure that can be displayed using
 * ntpdc and the clkbug command.
 */
void
refclock_buginfo(
	struct sockaddr_in *srcadr, /* clock address */
	struct refclockbug *bug /* output structure */
	)
{
	struct peer *peer;
	struct refclockproc *pp;
	u_char clktype;
	int unit;
	int i;

	/*
	 * Check for valid address and peer structure
	 */
	if (!ISREFCLOCKADR(srcadr))
		return;
	clktype = (u_char) REFCLOCKTYPE(srcadr);
	unit = REFCLOCKUNIT(srcadr);
	if (clktype >= num_refclock_conf || unit >= MAXUNIT)
		return;
	if (!(peer = typeunit[clktype][unit]))
		return;
	pp = peer->procptr;

	/*
	 * Copy structure values
	 */
	bug->nvalues = 8;
	bug->svalues = 0x0000003f;
	bug->values[0] = pp->year;
	bug->values[1] = pp->day;
	bug->values[2] = pp->hour;
	bug->values[3] = pp->minute;
	bug->values[4] = pp->second;
	bug->values[5] = pp->msec;
	bug->values[6] = pp->yearstart;
	bug->values[7] = pp->coderecv;
	bug->stimes = 0xfffffffc;
	bug->times[0] = pp->lastref;
	bug->times[1] = pp->lastrec;
	for (i = 2; i < (int)bug->ntimes; i++)
		DTOLFP(pp->filter[i - 2], &bug->times[i]);

	/*
	 * Give the stuff to the clock
	 */
	if (refclock_conf[clktype]->clock_buginfo != noentry)
		(refclock_conf[clktype]->clock_buginfo)(unit, bug, peer);
}

#endif /* REFCLOCK */
