/*
 * ntp_refclock - processing support for reference clocks
 */
#ifdef REFCLOCK

#include <stdio.h>
#include <sys/types.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_unixtime.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"

#ifdef PPS
#include <sys/ppsclock.h>
#endif /* PPS */

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
 * the unit structure, if used. In addition, some routines expect an
 * address in the dotted quad form 127.127.t.u, where t is the clock
 * type and u the unit. A table typeunit[type][unit] contains the peer
 * structure pointer for each configured clock type and unit.
 *
 * Most drivers support the 1-pps signal provided by some radios and
 * connected via a level converted described in the gadget directory.
 * The signal is captured using a separate, dedicated serial port and
 * the tty_clk line discipline/streams modules described in the kernel
 * directory. For the highest precision, the signal is captured using
 * the carrier-detect line of the same serial port using the ppsclock
 * streams module described in the ppsclock directory.
 */
#define	REFCLOCKMAXDISPERSE (FP_SECOND/4) /* max sample dispersion */
#define MAXUNIT		44	/* max units */
#ifndef CLKLDISC
#define CLKLDISC	10	/* XXX temp tty_clk line discipline */
#endif
#ifndef CHULDISC
#define CHULDISC	10	/* XXX temp tty_chu line discipline */
#endif

/*
 * The refclock configuration table. Imported from refclock_conf
 */
extern	struct	refclock *refclock_conf[];
extern	u_char	num_refclock_conf;

/*
 * Imported from the I/O module
 */
extern	struct	interface *any_interface;
extern	struct	interface *loopback_interface;

/*
 * Imported from ntp_loopfilter module
 */
extern	int	fdpps;		/* pps file descriptor */

/*
 * Imported from the timer module
 */
extern	u_long	current_time;
extern	struct	event timerqueue[];

/*
 * Imported from the main and peer modules. We use the same algorithm
 * for spacing out timers at configuration time that the peer module
 * does.
 */
extern	u_long	init_peer_starttime;
extern	int	initializing;
extern	int	debug;

/*
 * Type/unit peer index. Used to find the peer structure for control and
 * debugging. When all clock drivers have been converted to new style,
 * this dissapears.
 */
static struct peer *typeunit[REFCLK_MAX + 1][MAXUNIT];


/*
 * refclock_report - note the occurance of an event
 *
 * This routine presently just remembers the report and logs it, but
 * does nothing heroic for the trap handler. It tries to be a good
 * citizen and bothers the system log only if things change.
 */
void
refclock_report(peer, code)
	struct peer *peer;
	u_char code;
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
		if (code == CEVNT_NOMINAL)
			return;
		pp->lastevent = code;
		if (code == CEVNT_FAULT)
			syslog(LOG_ERR,
		    "clock %s fault %x", ntoa(&peer->srcadr), code);
		else {
			syslog(LOG_INFO,
		    "clock %s event %x", ntoa(&peer->srcadr), code);
		}
	}
}


/*
 * init_refclock - initialize the reference clock drivers
 *
 * This routine calls each of the drivers in turn to initialize internal
 * variables, if necessary. Most drivers have nothing to say at this
 * point.
 */
void
init_refclock()
{
	int i, j;

	for (i = 0; i < num_refclock_conf; i++) {
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
refclock_newpeer(peer)
	struct peer *peer;	/* peer structure pointer */
{
	struct refclockproc *pp;
	u_char clktype;
	int unit;

	/*
	 * Check for valid clock address. If already running, shut it 	 	 * down first.
	 */
	if (!ISREFCLOCKADR(&peer->srcadr)) {
		syslog(LOG_ERR,
		    "refclock_newpeer: clock address %s invalid",
		    ntoa(&peer->srcadr));
		return (0);
	}
	clktype = REFCLOCKTYPE(&peer->srcadr);
	unit = REFCLOCKUNIT(&peer->srcadr);
	if (clktype >= num_refclock_conf || unit > MAXUNIT ||
	    refclock_conf[clktype]->clock_start == noentry) {
		syslog(LOG_ERR,
		    "refclock_newpeer: clock type %d invalid\n",
		    clktype);
		return (0);
	}
	refclock_unpeer(peer);

	/*
	 * Allocate and initialize interface structure
	 */
	if (!(pp = (struct refclockproc *)
	    emalloc(sizeof(struct refclockproc))))
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
	peer->event_timer.peer = peer;
	peer->event_timer.event_handler = refclock_transmit;
	pp->type = clktype;
	pp->timestarted = current_time;
	peer->stratum = STRATUM_REFCLOCK;
	peer->refid = peer->srcadr.sin_addr.s_addr;
	peer->maxpoll = peer->minpoll;

	/*
	 * Do driver dependent initialization
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

	/*
	 * Set up the timeout for polling and reachability determination
	 */
	if (initializing) {
		init_peer_starttime += (1 << EVENT_TIMEOUT);
		if (init_peer_starttime >= (1 << peer->minpoll))
			init_peer_starttime = (1 << EVENT_TIMEOUT);
		peer->event_timer.event_time = init_peer_starttime;
	} else {
		peer->event_timer.event_time = current_time +
		    (1 << peer->hpoll);
	}
	TIMER_ENQUEUE(timerqueue, &peer->event_timer);
	return (1);
}


/*
 * refclock_unpeer - shut down a clock
 */
void
refclock_unpeer(peer)
	struct peer *peer;	/* peer structure pointer */
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
refclock_transmit(peer)
	struct peer *peer;	/* peer structure pointer */
{
	struct refclockproc *pp;
	u_char clktype;
	int unit;
	u_char opeer_reach;

	pp = peer->procptr;
	clktype = peer->refclktype;
	unit = peer->refclkunit;
	peer->sent++;

	/*
	 * The transmit procedure is supposed to freeze a timestamp.
	 * Get one just for fun, and to tell when we last were here.
	 */
	get_systime(&peer->xmt);

	/*
	 * Fiddle reachability.
	 */
	opeer_reach = peer->reach;
	peer->reach <<= 1;
	if (peer->reach == 0) {
		/*
		 * Clear this one out. No need to redo selection since
		 * this fellow will definitely be suffering from
		 * dispersion madness.
		 */
		if (opeer_reach != 0) {
			peer_clear(peer);
			peer->timereachable = current_time;
			report_event(EVNT_UNREACH, peer);
		}

	/*
	 * Update reachability and poll variables
	 */
	} else if ((opeer_reach & 3) == 0) {
		l_fp off;

		if (peer->valid > 0)
			peer->valid--;
		L_CLR(&off);
		clock_filter(peer, &off, 0, NTP_MAXDISPERSE);
		if (peer->flags & FLAG_SYSPEER)
			clock_select();
	} else if (peer->valid < NTP_SHIFT)
		peer->valid++;

	/*
	 * If he wants to be polled, do it. New style drivers do not use
	 * the unit argument, since the fudge stuff is in the
	 * refclockproc structure.
	 */
	if (refclock_conf[clktype]->clock_poll != noentry)
		(refclock_conf[clktype]->clock_poll)(unit, peer);

	/*
	 * Finally, reset the timer
	 */
	peer->event_timer.event_time += (1 << peer->hpoll);
	TIMER_ENQUEUE(timerqueue, &peer->event_timer);
}


/*
 * Compare two l_fp's - used with qsort()
 */
static int
refclock_cmpl_fp(p1, p2)
	register void *p1, *p2;	/* l_fp to compare */
{

	if (!L_ISGEQ((l_fp *)p1, (l_fp *)p2))
		return (-1);
	if (L_ISEQU((l_fp *)p1, (l_fp *)p2))
		return (0);
	return (1);
}


/*
 * refclock_process - process a pile of samples from the clock
 *
 * This routine converts the timecode in the form days, hours, miinutes,
 * seconds, milliseconds/microseconds to internal timestamp format. It
 * then calculates the difference from the receive timestamp and
 * assembles the samples in a shift register. It implements a recursive
 * median filter to suppress spikes in the data, as well as determine a
 * rough dispersion estimate. A configuration constant time adjustment
 * fudgetime1 can be added to the final offset to compensate for various
 * systematic errors. The routine returns one if success and zero if
 * failure due to invalid timecode data or very noisy offsets.
 */
int
refclock_process(pp, nstart, nskeep)
	struct refclockproc *pp; /* peer structure pointer */
	int nstart;		/* stages of median filter */
	int nskeep;		/* stages after outlyer trim */
{
	int i, n;
	l_fp offset, median, lftmp;
	l_fp off[MAXSTAGE];
	u_fp disp;

	/*
	 * Compute the timecode timestamp from the days, hours, minutes,
	 * seconds and milliseconds/microseconds of the timecode. Use
	 * clocktime() for the aggregate seconds and the msec/usec for
	 * the fraction, when present. Note that this code relies on the
	 * filesystem time for the years and does not use the years of
	 * the timecode.
	 */
	pp->nstages = nstart;
	if (!clocktime(pp->day, pp->hour, pp->minute, pp->second, GMT,
	    pp->lastrec.l_ui, &pp->yearstart, &pp->lastref.l_ui))
		return (0);
	if (pp->usec) {
		TVUTOTSF(pp->usec, pp->lastref.l_uf);
	} else {
		MSUTOTSF(pp->msec, pp->lastref.l_uf);
	}

	/*
	 * Subtract the receive timestamp from the timecode timestamp
	 * to form the raw offset. Insert in the median filter shift
	 * register.
	 */
	i = ((int)(pp->coderecv)) % pp->nstages;
	offset = pp->lastref;
	L_SUB(&offset, &pp->lastrec);
	pp->filter[i] = offset;
	if (pp->coderecv == 0)
		for (i = 1; i < pp->nstages; i++)
			pp->filter[i] = pp->filter[0];
	pp->coderecv++;

	/*
	 * Copy the raw offsets and sort into ascending order
	 */
	for (i = 0; i < pp->nstages; i++)
		off[i] = pp->filter[i];
	qsort((char *)off, pp->nstages, sizeof(l_fp), refclock_cmpl_fp);

	/*
	 * Reject the furthest from the median of nstages samples until
	 * nskeep samples remain.
	 */
	i = 0;
	n = pp->nstages;
	while ((n - i) > nskeep) {
		lftmp = off[n - 1];
		median = off[(n + i) / 2];
		L_SUB(&lftmp, &median);
		L_SUB(&median, &off[i]);
		if (L_ISHIS(&median, &lftmp)) {
			/* reject low end */
			i++;
		} else {
			/* reject high end */
			n--;
		}
	}

	/*
	 * Compute the dispersion based on the difference between the
	 * extremes of the remaining offsets. Add to this the time since
	 * the last clock update, which represents the dispersion
	 * increase with time. We know that NTP_MAXSKEW is 16. If the
	 * sum is greater than the allowed sample dispersion, bail out.
	 * If the loop is unlocked, return the most recent offset;
	 * otherwise, return the median offset. In either case include
	 * the configured fudgetime1 adjustment.
	 */
	lftmp = off[n - 1];
	L_SUB(&lftmp, &off[i]);
	disp = LFPTOFP(&lftmp) + current_time - pp->lasttime;
	if (disp > REFCLOCKMAXDISPERSE)
		return (0);
	pp->offset = offset;
	L_ADD(&pp->offset, &pp->fudgetime1);
	pp->dispersion = disp;
	return (1);
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
refclock_receive(peer, offset, delay, dispersion, reftime, rectime, leap)
	struct peer *peer;	/* peer structure pointer */
	l_fp *offset;		/* computed offset (s) */
	s_fp delay;		/* computed delay to peer */
	u_fp dispersion;	/* computed dispersion to peer */
	l_fp *reftime;		/* time at last clock update */
	l_fp *rectime;		/* time at last peer update */
	int leap;		/* synchronization/leap code */
{
	int restrict;
	int trustable;
	u_fp precision;

	peer->received++;
#ifdef DEBUG
	if (debug)
		printf("refclock_receive: %s %s %s %s)\n",
		    ntoa(&peer->srcadr), lfptoa(offset, 6),
		    fptoa(delay, 5), ufptoa(dispersion, 5));
#endif

	/*
	 * The authentication and access-control machinery works, but
	 * its utility may be questionable.
	 */
	restrict = restrictions(&peer->srcadr);
	if (restrict & (RES_IGNORE|RES_DONTSERVE))
		return;
	peer->processed++;
	peer->timereceived = current_time;
	if (restrict & RES_DONTTRUST)
		trustable = 0;
	else
		trustable = 1;

	if (peer->flags & FLAG_AUTHENABLE) {
		if (trustable)
			peer->flags |= FLAG_AUTHENTIC;
		else
			peer->flags &= ~FLAG_AUTHENTIC;
	}
	peer->leap = leap;

	/*
	 * Set the timestamps. rec and org are in local time, while ref
	 * is in timecode time.
	 */
	peer->rec = peer->org = *rectime;
	peer->reftime = *reftime;

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
	 * Abandon ship if the radio came bum. We only got this far
	 * in order to make pretty billboards, even if bum.
	 */
	if (leap == LEAP_NOTINSYNC)
		return;
	/*
	 * If this guy was previously unreachable, report him
	 * reachable.
	 */
	if (peer->reach == 0) report_event(EVNT_REACH, peer);
		peer->reach |= 1;

	/*
	 * Give the data to the clock filter and update the clock. Note
	 * the clock reading precision initialized by the driver is
	 * added at this point.
	 */
	precision = FP_SECOND >> -(int)peer->precision;
	if (precision == 0)
		precision = 1;
	refclock_report(peer, CEVNT_NOMINAL);
	clock_filter(peer, offset, delay, dispersion + precision);
	clock_update(peer);
}


/*
 * refclock_gtlin - groom next input line and extract timestamp
 *
 * This routine processes the timecode received from the clock and
 * removes the parity bit and control characters. If a timestamp is
 * present in the timecode, as produced by the tty_clk line
 * discipline/streams module, it returns that as the timestamp;
 * otherwise, it returns the buffer timestamp. The routine return code
 * is the number of characters in the line.
 */
int
refclock_gtlin(rbufp, lineptr, bmax, tsptr)
	struct recvbuf *rbufp;	/* receive buffer pointer */
	char *lineptr;		/* current line pointer */
	int bmax;		/* remaining characters in line */
	l_fp *tsptr;		/* pointer to timestamp returned */
{
	char *dpt, *dpend, *dp;
	int i;
	l_fp trtmp, tstmp;
	char c;

	/*
	 * Check for the presence of a timestamp left by the tty_clock
	 * line discipline/streams module and, if present, use that
	 * instead of the buffer timestamp captured by the I/O routines.
	 * We recognize a timestamp by noting its value is earlier than
	 * the buffer timestamp, but not more than one second earlier.
	 */
	dpt = (char *)&rbufp->recv_space;
	dpend = dpt + rbufp->recv_length;
	trtmp = rbufp->recv_time;
	if (dpend >= dpt + 8) {
		if (buftvtots(dpend - 8, &tstmp)) {
			L_SUB(&trtmp, &tstmp);
			if (trtmp.l_ui == 0) {
#ifdef DEBUG
				if (debug) {
					printf(
				    "refclock_gtlin: fd %d ldisc %s",
					    rbufp->fd,
					    lfptoa(&trtmp, 6));
					gettstamp(&trtmp);
					L_SUB(&trtmp, &tstmp);
					printf(" sigio %s\n",
					    lfptoa(&trtmp, 6));
				}
#endif
				dpend -= 8;
				trtmp = tstmp;
			} else
				trtmp = rbufp->recv_time;
		}
	}

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
	if (debug)
        	printf("refclock_gtlin: fd %d time %s timecode %d %s\n",
		    rbufp->fd, ulfptoa(&trtmp, 6), i, lineptr);
#endif
	*tsptr = trtmp;
	return (i);
}


/*
 * refclock_open - open serial port for reference clock
 *
 * This routine opens a serial port for I/O and sets default options. It
 * returns the file descriptor if success and zero if failure.
 */
int
refclock_open(dev, speed, flags)
	char *dev;		/* device name pointer */
	int speed;		/* serial port speed (code) */
	int flags;		/* line discipline flags */
{
	int fd;
#ifdef HAVE_TERMIOS
	struct termios ttyb, *ttyp;
#endif /* HAVE_TERMIOS */
#ifdef HAVE_SYSV_TTYS
	struct termio ttyb, *ttyp;
#endif /* HAVE_SYSV_TTYS */
#ifdef HAVE_BSD_TTYS
	struct sgttyb ttyb, *ttyp;
#endif /* HAVE_BSD_TTYS */
#ifdef HAVE_MODEM_CONTROL
	u_long ltemp;
#endif /* HAVE_MODEM_CONTROL */

	/*
	 * Open serial port and set default options
	 */
	fd = open(dev, O_RDWR, 0777);
	if (fd == -1) {
		syslog(LOG_ERR, "refclock_open: %s: %m", dev);
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
		syslog(LOG_ERR,
		    "refclock_open: fd %d tcgetattr %m", fd);
		return (0);
	}

	/*
	 * Set canonical mode and local connection; set specified speed,
	 * 8 bits and no parity; map CR to NL; ignore break.
	 */
	ttyp->c_iflag = IGNBRK | IGNPAR | ICRNL;
	ttyp->c_oflag = 0;
	ttyp->c_cflag = CS8 | CLOCAL | CREAD;
	(void)cfsetispeed(&ttyb, speed);
	(void)cfsetospeed(&ttyb, speed);
	ttyp->c_lflag = ICANON;
	ttyp->c_cc[VERASE] = ttyp->c_cc[VKILL] = '\0';
#ifdef HAVE_MODEM_CONTROL
	/*
	 * If we have modem control, check to see if modem leads are
	 * active; if so, set remote connection. This is necessary for
	 * the kernel pps mods to work.
	 */
	ltemp = 0;
	if (ioctl(fd, TIOCMGET, (char *)&ltemp) < 0)
		syslog(LOG_ERR,
		    "refclock_open: fd %d TIOCMGET %m", fd);
#if DEBUG
	if (debug)
		printf("refclock_open: fd %d modem status %lx\n",
		    fd, ltemp);
#endif
	if (ltemp & TIOCM_DSR)
		ttyp->c_cflag &= ~CLOCAL;
#endif /* HAVE_MODEM_CONTROL */
	if (tcsetattr(fd, TCSANOW, ttyp) < 0) {
		syslog(LOG_ERR,
		    "refclock_open: fd %d tcsetattr %m", fd);
		return (0);
	}
	if (tcflush(fd, TCIOFLUSH) < 0) {
		syslog(LOG_ERR,
		    "refclock_open: fd %d tcflush %m", fd);
		return (0);
	}
#endif /* HAVE_TERMIOS */

#ifdef HAVE_SYSV_TTYS

	/*
	 * System V serial line parameters (termio interface)
	 *
	 */
	if (ioctl(fd, TCGETA, ttyp) < 0) {
		syslog(LOG_ERR,
		    "refclock_open: fd %d TCGETA %m", fd);
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
#ifdef HAVE_MODEM_CONTROL
	/*
	 * If we have modem control, check to see if modem leads are
	 * active; if so, set remote connection. This is necessary for
	 * the kernel pps mods to work.
	 */
	ltemp = 0;
	if (ioctl(fd, TIOCMGET, (char *)&ltemp) < 0)
		syslog(LOG_ERR,
		    "refclock_open: fd %d TIOCMGET %m", fd);
#if DEBUG
	if (debug)
		printf("refclock_open: fd %d modem status %lx\n",
		    fd, ltemp);
#endif
	if (ltemp & TIOCM_DSR)
		ttyp->c_cflag &= ~CLOCAL;
#endif /* HAVE_MODEM_CONTROL */
	if (ioctl(fd, TCSETA, ttyp) < 0) {
		syslog(LOG_ERR,
		    "refclock_open: fd %d TCSETA %m", fd);
		return (0);
	}
#endif /* HAVE_SYSV_TTYS */

#ifdef HAVE_BSD_TTYS

	/*
	 * 4.3bsd serial line parameters (sgttyb interface)
	 */
	if (ioctl(fd, TIOCGETP, (char *)ttyp) < 0) {
		syslog(LOG_ERR,
		    "refclock_open: fd %d TIOCGETP %m", fd);
		return (0);
	}
	ttyp->sg_ispeed = ttyp->sg_ospeed = speed;
	ttyp->sg_flags = EVENP | ODDP | CRMOD;
	if (ioctl(fd, TIOCSETP, (char *)ttyp) < 0) {
		syslog(LOG_ERR,
		    "refclock_open: TIOCSETP %m");
		return (0);
	}
#endif /* HAVE_BSD_TTYS */

	if (!refclock_ioctl(fd, flags)) {
		(void)close(fd);
		syslog(LOG_ERR, "refclock_open: fd %d ioctl fails",
		    fd);
		return (0);
	}
	return (fd);
}


/*
 * refclock_ioctl - set serial port control functions
 *
 * This routine attempts to hide the internal, system-specific details
 * of serial ports. It can handle POSIX (termios), SYSV (termio) and BSD
 * (sgtty) interfaces with varying degrees of success. The routine sets
 * up the tty_clk, chu_clk and ppsclock streams module/line discipline,
 * if compiled in the daemon and requested in the call. The routine
 * returns one if success and zero if failure.
 */
int
refclock_ioctl(fd, flags)
	int fd;			/* file descriptor */
	int flags;		/* line discipline flags */
{
#ifdef HAVE_TERMIOS
	struct termios ttyb, *ttyp;
#endif /* HAVE_TERMIOS */
#ifdef HAVE_SYSV_TTYS
        struct termio ttyb, *ttyp;
#endif /* HAVE_SYSV_TTYS */
#ifdef HAVE_BSD_TTYS
	struct sgttyb ttyb, *ttyp;
#endif /* HAVE_BSD_TTYS */

#ifdef DEBUG
	if (debug)
		printf("refclock_ioctl: fd %d flags %x\n",
		    fd, flags);
#endif

	/*
	 * The following sections select optional features, such as
	 * modem control, line discipline and so forth. Some require
	 * specific operating system support in the form of streams
	 * modules, which can be loaded and unloaded at run time without
	 * rebooting the kernel, or line discipline modules, which must
	 * be compiled in the kernel. The streams modules require System
	 * V STREAMS support, while the line discipline modules require
	 * 4.3bsd or later. The checking frenzy is attenuated here,
	 * since the device is already open.
	 *
	 * Note that both the clk and ppsclock modules are optional; the
	 * dang thing still works, but the accuracy improvement using
	 * them will not be available. The ppsclock module is associated
	 * with a specific, declared line and should be used only once.
	 * If requested, the chu module is mandatory, since the driver
	 * will not work without it.
	 *
	 * Use the LDISC_PPS option ONLY with Sun baseboard ttya or
	 * ttyb. Using it with the SPIF multipexor crashes the kernel.
	 */
	if (flags == 0)
		return (1);

#if !(defined(HAVE_TERMIOS) || defined(HAVE_BSD_TTYS))
	if (flags & (LDISC_CLK | LDISC_CHU | LDISC_PPS | LDISC_ACTS))
		syslog(LOG_ERR,
	    "refclock_ioctl: unsupported terminal interface");
		return (0);
#endif /* HAVE_TERMIOS HAVE_BSD_TTYS */

	ttyp = &ttyb;

#ifdef STREAM
#ifdef CLK

	/*
	 * The CLK option provides timestamping at the driver level.
	 * It requires the tty_clk streams module and System V STREAMS
	 * support.
	 */
	if (flags & (LDISC_CLK | LDISC_CLKPPS | LDISC_ACTS)) {
		if (ioctl(fd, I_PUSH, "clk") < 0)
			syslog(LOG_NOTICE,
	    "refclock_ioctl: optional clk streams module unavailable");
		else {
			char *str;

			if (flags & LDISC_PPS)
				str = "\377";
			else if (flags & LDISC_ACTS)
				str = "*";
			else
				str = "\n";
			if (ioctl(fd, CLK_SETSTR, str) < 0)
				syslog(LOG_ERR,
				    "refclock_ioctl: CLK_SETSTR %m");
		}
	}

	/*
	 * The ACTS line discipline requires additional line-ending
	 * character '*'.
	 */
	if (flags & LDISC_ACTS) {
		(void)tcgetattr(fd, ttyp);
		ttyp->c_cc[VEOL] = '*';
		(void)tcsetattr(fd, TCSANOW, ttyp);
	}
#else
	if (flags & LDISC_CLK)
		syslog(LOG_NOTICE,
	    "refclock_ioctl: optional clk streams module unsupported");
#endif /* CLK */
#ifdef CHU

	/*
	 * The CHU option provides timestamping and decoding for the CHU
	 * timecode. It requires the tty_chu streams module and System V
	 * STREAMS support.
	 */
	if (flags & LDISC_CHU) {
		(void)tcgetattr(fd, ttyp);
		ttyp->c_lflag = 0;
		ttyp->c_cc[VERASE] = ttyp->c_cc[VKILL] = '\0';
		ttyp->c_cc[VMIN] = 1;
		ttyp->c_cc[VTIME] = 0;
		(void)tcsetattr(fd, TCSANOW, ttyp);
		(void)tcflush(fd, TCIOFLUSH);
		while (ioctl(fd, I_POP, 0) >= 0);
		if (ioctl(fd, I_PUSH, "chu") < 0) {
			syslog(LOG_ERR,
	    "refclock_ioctl: required chu streams module unavailable");
			return (0);
		}
	}
#else
	if (flags & LDISC_CHU) {
		syslog(LOG_ERR,
	    "refclock_ioctl: required chu streams module unsupported");
		return (0);
	}
#endif /* CHU */
#ifdef PPS

	/*
	 * The PPS option provides timestamping at the driver level.
	 * It uses a 1-pps signal and level converter (gadget box) and
	 * requires the ppsclock streams module and System V STREAMS
	 * support.
	 */
	if (flags & LDISC_PPS) {
		if (fdpps != -1) {
			syslog(LOG_ERR,
		    "refclock_ioctl: ppsclock already configured");
			return (0);
		}
		if (ioctl(fd, I_PUSH, "ppsclock") < 0)
			syslog(LOG_NOTICE,
	    "refclock_ioctl: optional ppsclock streams module unavailable");
		else
			fdpps = fd;
	}
#else
	if (flags & LDISC_PPS)
		syslog(LOG_NOTICE,
	    "refclock_ioctl: optional ppsclock streams module unsupported");
#endif /* PPS */

#else /* STREAM */

#ifdef HAVE_TERMIOS
#ifdef CLK

	/*
	 * The CLK option provides timestamping at the driver level. It
	 * requires the tty_clk line discipline and 4.3bsd or later.
	 */
	if (flags & (LDISC_CLK | LDISC_CLKPPS | LDISC_ACTS)) {
		(void)tcgetattr(fd, ttyp);
		ttyp->c_lflag = 0;
		if (flags & LDISC_CLKPPS)
			ttyp->c_cc[VERASE] = ttyp->c_cc[VKILL] = '\377';
		else if (flags & LDISC_ACTS) {
			ttyp->c_cc[VERASE] = '*';
			ttyp->c_cc[VKILL] = '#';
		} else
			ttyp->c_cc[VERASE] = ttyp->c_cc[VKILL] = '\n';
		ttyp->c_cc[VMIN] = 1;
		ttyp->c_cc[VTIME] = 0;
		ttyp->c_line = CLKLDISC;
		(void)tcsetattr(fd, TCSANOW, ttyp);
		(void)tcflush(fd, TCIOFLUSH);
	}
#else
	if (flags & LDISC_CLK)
		syslog(LOG_NOTICE,
		"refclock_ioctl: optional clk line discipline unsupported");
#endif /* CLK */
#ifdef CHU
	/*
	 * The CHU option provides timestamping and decoding for the CHU
	 * timecode. It requires the tty_chu line disciplne and 4.3bsd
	 * or later.
	 */
	if (flags & LDISC_CHU) {
		(void)tcgetattr(fd, ttyp);
		ttyp->c_lflag = 0;
		ttyp->c_cc[VERASE] = ttyp->c_cc[VKILL] = '\r';
		ttyp->c_cc[VMIN] = 1;
		ttyp->c_cc[VTIME] = 0;
		ttyp->c_line = CHULDISC;
		(void)tcsetattr(fd, TCSANOW, ttyp) < 0);
		(void)tcflush(fd, TCIOFLUSH);
	}
#else
	if (flags & LDISC_CHU) {
		syslog(LOG_ERR,
		"refclock_ioctl: required chu line discipline unsupported");
		return (0);
	}
#endif /* CHU */
#endif /* HAVE_TERMIOS */

#ifdef HAVE_BSD_TTYS
#ifdef CLK

	/*
	 * The CLK option provides timestamping at the driver level. It
	 * requires the tty_clk line discipline and 4.3bsd or later.
	 */
	if (flags & (LDISC_CLK | LDISC_CLKPPS | LDISC_ACTS)) {
		int ldisc = CLKLDISC;

		(void)ioctl(fd, TIOCGETP, (char *)ttyp);
		if (flags & LDISC_CLKPPS)
			ttyp->sg_erase = ttyp->sg_kill = '\377';
		else if (flags & LDISC_ACTS) {
			ttyp->sg_erase = '*';
			ttyp->sg_kill = '#';
		} else
			ttyp->sg_erase = ttyp->sg_kill = '\r';
		ttyp->sg_flags = RAW;
		(void)ioctl(fd, TIOCSETP, ttyp);
		if (ioctl(fd, TIOCSETD, (char *)&ldisc) < 0)
			syslog(LOG_NOTICE,
	    "refclock_ioctl: optional clk line discipline unavailable");
	}
#else
	if (flags & LDISC_CLK)
		syslog(LOG_NOTICE,
	    "refclock_ioctl: optional clk line discipline unsupported");

#endif /* CLK */
#ifdef CHU

	/*
	 * The CHU option provides timestamping and decoding for the CHU
	 * timecode. It requires the tty_chu line disciplne and 4.3bsd
	 * or later.
	 */
	if (flags & LDISC_CHU) {
		int ldisc = CHULDISC;

		(void)ioctl(fd, TIOCGETP, (char *)ttyp);
		ttyp->sg_erase = ttyp->sg_kill = '\r';
		ttyp->sg_flags = RAW;
		(void)ioctl(fd, TIOCSETP, (char *)ttyp);
		if (ioctl(fd, TIOCSETD, (char *)&ldisc) < 0) {
			syslog(LOG_ERR,
	    "refclock_ioctl: required chu line discipline unavailable");
			return (0);
		}
	}
#else
	if (flags & LDISC_CHU) {
		syslog(LOG_ERR,
	    "refclock_ioctl: required chu line discipline unsupported");
		return (0);
	}
#endif /* CHU */
#endif /* HAVE_BSD_TTYS */

#endif /* STREAM */

	return (1);
}


/*
 * refclock_control - set and/or return clock values
 *
 * This routine is used mainly for debugging. It returns designated
 * values from the interface structure that can be displayed using
 * xntpdc and the clockstat command. It can also be used to initialize
 * configuration variables, such as fudgetimes, fudgevalues, reference
 * ID and stratum.
 */
void
refclock_control(srcadr, in, out)
	struct sockaddr_in *srcadr;
	struct refclockstat *in;
	struct refclockstat *out;
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
	clktype = REFCLOCKTYPE(srcadr);
	unit = REFCLOCKUNIT(srcadr);
	if (clktype >= num_refclock_conf || unit > MAXUNIT)
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
			peer->stratum = in->fudgeval1;
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
		if (in->flags & CLK_FLAG3)
			(void)refclock_ioctl(pp->io.fd, LDISC_PPS);
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
		out->flags = pp->sloppyclockflag;

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
		out->lastcode = pp->lastcode;
	}

	/*
	 * Give the stuff to the clock
	 */
	if (refclock_conf[clktype]->clock_control != noentry)
		(refclock_conf[clktype]->clock_control)(unit, in, out);
}


/*
 * refclock_buginfo - return debugging info
 *
 * This routine is used mainly for debugging. It returns designated
 * values from the interface structure that can be displayed using
 * xntpdc and the clkbug command.
 */
void
refclock_buginfo(srcadr, bug)
	struct sockaddr_in *srcadr; /* clock address */
	struct refclockbug *bug; /* output structure */
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
	clktype = REFCLOCKTYPE(srcadr);
	unit = REFCLOCKUNIT(srcadr);
	if (clktype >= num_refclock_conf || unit > MAXUNIT)
		return;
	if (!(peer = typeunit[clktype][unit]))
		return;
	pp = peer->procptr;

	/*
	 * Copy structure values
	 */
	bug->nvalues = 8;
	bug->values[0] = pp->year;
	bug->values[1] = pp->day;
	bug->values[2] = pp->hour;
	bug->values[3] = pp->minute;
	bug->values[4] = pp->second;
	bug->values[5] = pp->msec;
	bug->values[6] = pp->yearstart;
	bug->values[7] = pp->coderecv;

	bug->ntimes = pp->nstages + 3;
	if (bug->ntimes > NCLKBUGTIMES)
		bug->ntimes = NCLKBUGTIMES;
	bug->stimes = 0xfffffffc;
	bug->times[0] = pp->lastref;
	bug->times[1] = pp->lastrec;
	UFPTOLFP(pp->dispersion, &bug->times[2]);
	for (i = 0; i < bug->ntimes; i++)
		bug->times[i + 3] = pp->filter[i];

	/*
	 * Give the stuff to the clock
	 */
	if (refclock_conf[clktype]->clock_buginfo != noentry)
		(refclock_conf[clktype]->clock_buginfo)(unit, bug);
}

#endif /* REFCLOCK */
