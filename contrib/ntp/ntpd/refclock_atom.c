/*
 * refclock_atom - clock driver for 1-pps signals
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_ATOM)

#include <stdio.h>
#include <ctype.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_unixtime.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#ifdef HAVE_SYS_TERMIOS_H
# include <sys/termios.h>
#endif
#ifdef HAVE_SYS_PPSCLOCK_H
# include <sys/ppsclock.h>
#endif
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
 * This driver furnishes an interface for pulse-per-second (PPS) signals
 * produced by a cesium clock, timing receiver or related  equipment. It
 * can be used to remove accumulated jitter and retime a secondary
 * server when synchronized to a primary server over a congested, wide-
 * area network and before redistributing the time to local clients.
 *
 * In order for this driver to work, the local clock must be set to
 * within +-500 ms by another means, such as a radio clock or NTP
 * itself. The 1-pps signal is connected via a serial port and gadget
 * box consisting of a one-shot flopflop and RS232 level converter.
 * Conntection is either via the carrier detect (DCD) lead or via the
 * receive data (RD) lead. The incidental jitter using the DCD lead is
 * essentially the interrupt latency. The incidental jitter using the RD
 * lead has an additional component due to the line sampling clock. When
 * operated at 38.4 kbps, this arrangement has a worst-case jitter less
 * than 26 us.
 *
 * There are four ways in which this driver can be used. They are
 * described in decreasing order of merit below. The first way uses the
 * ppsapi STREAMS module and the LDISC_PPS line discipline, while the
 * second way uses the ppsclock STREAMS module and the LDISC_PPS line
 * discipline. Either of these works only for the baseboard serial ports
 * of the Sun SPARC IPC and clones. However, the ppsapi uses the
 * proposed IETF interface expected to become standard for PPS signals.
 * The serial port to be used is specified by the pps command in the
 * configuration file. This driver reads the timestamp directly by a
 * designated ioctl() system call.
 *
 * The third way uses the LDISC_CLKPPS line discipline and works for
 * any architecture supporting a serial port. If after a few seconds
 * this driver finds no ppsclock module configured, it attempts to open
 * a serial port device /dev/pps%d, where %d is the unit number, and
 * assign the LDISC_CLKPPS line discipline to it. If the line discipline
 * fails, no harm is done except the accuracy is reduced somewhat. The
 * pulse generator in the gadget box is adjusted to produce a start bit
 * of length 26 usec at 38400 bps. Used with the LDISC_CLKPPS line
 * discipline, this produces an ASCII DEL character ('\377') followed by
 * a timestamp at each seconds epoch. 
 *
 * The fourth way involves an auxiliary radio clock driver which calls
 * the PPS driver with a timestamp captured by that driver. This use is
 * documented in the source code for the driver(s) involved.  Note that
 * some drivers collect the sample information themselves before calling
 * pps_sample(), and others call knowing only that they are running
 * shortly after an on-time tick and they expect to retrieve the PPS
 * offset, fudge their result, and insert it into the timestream.
 *
 * Fudge Factors
 *
 * There are no special fudge factors other than the generic. The fudge
 * time1 parameter can be used to compensate for miscellaneous UART and
 * OS delays. Allow about 247 us for uart delays at 38400 bps and about
 * 1 ms for STREAMS nonsense with older workstations. Velocities may
 * vary with modern workstations. 
 */
/*
 * Interface definitions
 */
#ifdef HAVE_PPSAPI
extern int pps_assert;
#endif /* HAVE_PPSAPI */
#ifdef TTYCLK
#define DEVICE		"/dev/pps%d"	/* device name and unit */
#ifdef B38400
#define SPEED232	B38400	/* uart speed (38400 baud) */
#else
#define SPEED232	EXTB	/* as above */
#endif
#endif /* TTYCLK */

#define	PRECISION	(-20)	/* precision assumed (about 1 us) */
#define	REFID		"PPS\0"	/* reference ID */
#define	DESCRIPTION	"PPS Clock Discipline" /* WRU */

#define FLAG_TTY	0x01	/* tty_clk heard from */
#define FLAG_PPS	0x02	/* ppsclock heard from */
#define FLAG_AUX	0x04	/* auxiliary PPS source */

static struct peer *pps_peer;	/* atom driver for auxiliary PPS sources */

#ifdef TTYCLK
static	void	atom_receive	P((struct recvbuf *));
#endif /* TTYCLK */

/*
 * Unit control structure
 */
struct atomunit {
#ifdef HAVE_PPSAPI
	pps_info_t pps_info;	/* pps_info control */
#endif /* HAVE_PPSAPI */
#ifdef PPS
	struct	ppsclockev ev;	/* ppsclock control */
#endif /* PPS */
	int	flags;		/* flags that wave */
};

/*
 * Function prototypes
 */
static	int	atom_start	P((int, struct peer *));
static	void	atom_shutdown	P((int, struct peer *));
static	void	atom_poll	P((int, struct peer *));
#if defined(PPS) || defined(HAVE_PPSAPI)
static	int	atom_pps	P((struct peer *));
#endif /* PPS || HAVE_PPSAPI */

/*
 * Transfer vector
 */
struct	refclock refclock_atom = {
	atom_start,		/* start up driver */
	atom_shutdown,		/* shut down driver */
	atom_poll,		/* transmit poll message */
	noentry,		/* not used (old atom_control) */
	noentry,		/* initialize driver */
	noentry,		/* not used (old atom_buginfo) */
	NOFLAGS			/* not used */
};


/*
 * atom_start - initialize data for processing
 */
static int
atom_start(
	int unit,
	struct peer *peer
	)
{
	register struct atomunit *up;
	struct refclockproc *pp;
	int flags;
#ifdef TTYCLK
	int fd = 0;
	char device[20];
	int ldisc = LDISC_CLKPPS;
#endif /* TTYCLK */

	pps_peer = peer;
	flags = 0;

#ifdef TTYCLK
# if defined(SCO5_CLOCK)
	ldisc = LDISC_RAW;   /* DCD timestamps without any line discipline */
# endif
	/*
	 * Open serial port. Use LDISC_CLKPPS line discipline only
	 * if the LDISC_PPS line discipline is not availble,
	 */
# if defined(PPS) || defined(HAVE_PPSAPI)
	if (fdpps <= 0)
# endif
	{
		(void)sprintf(device, DEVICE, unit);
		if ((fd = refclock_open(device, SPEED232, ldisc)) != 0)
			flags |= FLAG_TTY;
	}
#endif /* TTYCLK */

	/*
	 * Allocate and initialize unit structure
	 */
	if (!(up = (struct atomunit *)emalloc(sizeof(struct atomunit)))) {
#ifdef TTYCLK
		if (flags & FLAG_TTY)
			(void) close(fd);
#endif /* TTYCLK */
		return (0);
	}
	memset((char *)up, 0, sizeof(struct atomunit));
	pp = peer->procptr;
	pp->unitptr = (caddr_t)up;
#ifdef TTYCLK
	if (flags & FLAG_TTY) {
		pp->io.clock_recv = atom_receive;
		pp->io.srcclock = (caddr_t)peer;
		pp->io.datalen = 0;
		pp->io.fd = fd;
		if (!io_addclock(&pp->io)) {
			(void) close(fd);
			free(up);
			return (0);
		}
	}
#endif /* TTYCLK */

	/*
	 * Initialize miscellaneous variables
	 */
	peer->precision = PRECISION;
	pp->clockdesc = DESCRIPTION;
	memcpy((char *)&pp->refid, REFID, 4);
	up->flags = flags;
	return (1);
}


/*
 * atom_shutdown - shut down the clock
 */
static void
atom_shutdown(
	int unit,
	struct peer *peer
	)
{
	register struct atomunit *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct atomunit *)pp->unitptr;
#ifdef TTYCLK
	if (up->flags & FLAG_TTY)
		io_closeclock(&pp->io);
#endif /* TTYCLK */
	if (pps_peer == peer)
		pps_peer = 0;
	free(up);
}


#if defined(PPS) || defined(HAVE_PPSAPI)
/*
 * atom_pps - receive data from the LDISC_PPS discipline
 */
static int
atom_pps(
	struct peer *peer
	)
{
	register struct atomunit *up;
	struct refclockproc *pp;
#ifdef HAVE_PPSAPI
	struct timespec timeout;
# ifdef HAVE_TIMESPEC
	struct timespec ts;
# else
	struct timeval ts;
# endif /* HAVE_TIMESPEC */
#endif /* HAVE_PPSAPI */
	l_fp lftmp;
	double doffset;
	int i;
#if !defined(HAVE_PPSAPI)
	int request =
# ifdef HAVE_CIOGETEV
	  CIOGETEV
# endif
# ifdef HAVE_TIOCGPPSEV
	  TIOCGPPSEV
# endif
	  ;
#endif /* HAVE_PPSAPI */

	/*
	 * This routine is called once per second when the LDISC_PPS
	 * discipline is present. It snatches the pps timestamp from the
	 * kernel and saves the sign-extended fraction in a circular
	 * buffer for processing at the next poll event.
	 */
	pp = peer->procptr;
	up = (struct atomunit *)pp->unitptr;

	/*
	 * Convert the timeval to l_fp and save for billboards. Sign-
	 * extend the fraction and stash in the buffer. No harm is done
	 * if previous data are overwritten. If the discipline comes bum
	 * or the data grow stale, just forget it. Round the nanoseconds
	 * to microseconds with great care.
	 */ 
	if (fdpps <= 0)
		return (1);
#ifdef HAVE_PPSAPI
	timeout.tv_sec = 0;
	timeout.tv_nsec = 0;
	i = up->pps_info.assert_sequence;
	if (time_pps_fetch(fdpps, PPS_TSFMT_TSPEC, &up->pps_info, &timeout)
	    < 0)
		return (2);
	if (i == up->pps_info.assert_sequence)
		return (3);
	if (pps_assert)
		ts = up->pps_info.assert_timestamp;
	else
		ts = up->pps_info.clear_timestamp;
	pp->lastrec.l_ui = ts.tv_sec + JAN_1970;
	ts.tv_nsec = (ts.tv_nsec + 500) / 1000;
	if (ts.tv_nsec > 1000000) {
		ts.tv_nsec -= 1000000;
		ts.tv_sec++;
	}
	TVUTOTSF(ts.tv_nsec, pp->lastrec.l_uf);
#else
	i = up->ev.serial;
	if (ioctl(fdpps, request, (caddr_t)&up->ev) < 0)
		return (2);
	if (i == up->ev.serial)
		return (3);
	pp->lastrec.l_ui = up->ev.tv.tv_sec + JAN_1970;
	TVUTOTSF(up->ev.tv.tv_usec, pp->lastrec.l_uf);
#endif /* HAVE_PPSAPI */ 
	up->flags |= FLAG_PPS;
	L_CLR(&lftmp);
	L_ADDF(&lftmp, pp->lastrec.l_f);
	LFPTOD(&lftmp, doffset);
	SAMPLE(-doffset + pp->fudgetime1);
	return (0);
}
#endif /* PPS || HAVE_PPSAPI */

#ifdef TTYCLK
/*
 * atom_receive - receive data from the LDISC_CLK discipline
 */
static void
atom_receive(
	struct recvbuf *rbufp
	)
{
	register struct atomunit *up;
	struct refclockproc *pp;
	struct peer *peer;
	l_fp lftmp;
	double doffset;

	/*
	 * This routine is called once per second when the serial
	 * interface is in use. It snatches the timestamp from the
	 * buffer and saves the sign-extended fraction in a circular
	 * buffer for processing at the next poll event.
	 */
	peer = (struct peer *)rbufp->recv_srcclock;
	pp = peer->procptr;
	up = (struct atomunit *)pp->unitptr;
	pp->lencode = refclock_gtlin(rbufp, pp->a_lastcode, BMAX,
	    &pp->lastrec);

	/*
	 * Save the timestamp for billboards. Sign-extend the fraction
	 * and stash in the buffer. No harm is done if previous data are
	 * overwritten. Do this only if the ppsclock gizmo is not
	 * working.
	 */
	if (up->flags & FLAG_PPS)
		return;
	L_CLR(&lftmp);
	L_ADDF(&lftmp, pp->lastrec.l_f);
	LFPTOD(&lftmp, doffset);
	SAMPLE(-doffset + pp->fudgetime1);
}
#endif /* TTYCLK */

/*
 * pps_sample - receive PPS data from some other clock driver
 */
int
pps_sample(
	   l_fp *offset
	   )
{
	register struct peer *peer;
	register struct atomunit *up;
	struct refclockproc *pp;
	l_fp lftmp;
	double doffset;

	/*
	 * This routine is called once per second when the external
	 * clock driver processes PPS information. It processes the pps
	 * timestamp and saves the sign-extended fraction in a circular
	 * buffer for processing at the next poll event.
	 */
	peer = pps_peer;
	if (peer == 0)		/* nobody home */
		return 1;
	pp = peer->procptr;
	up = (struct atomunit *)pp->unitptr;

	/*
	 * Convert the timeval to l_fp and save for billboards. Sign-
	 * extend the fraction and stash in the buffer. No harm is done
	 * if previous data are overwritten. If the discipline comes bum
	 * or the data grow stale, just forget it.
	 */ 
	up->flags |= FLAG_AUX;
	pp->lastrec = *offset;
	L_CLR(&lftmp);
	L_ADDF(&lftmp, pp->lastrec.l_f);
	LFPTOD(&lftmp, doffset);
	SAMPLE(-doffset + pp->fudgetime1);
	return (0);
}

/*
 * atom_poll - called by the transmit procedure
 */
static void
atom_poll(
	int unit,
	struct peer *peer
	)
{
#if defined(PPS) || defined(HAVE_PPSAPI)
	register struct atomunit *up;
#endif /* PPS || HAVE_PPSAPI */
	struct refclockproc *pp;

	/*
	 * Accumulate samples in the median filter. At the end of each
	 * poll interval, do a little bookeeping and process the
	 * samples.
	 */
	pp = peer->procptr;
#if defined(PPS) || defined(HAVE_PPSAPI)
	up = (struct atomunit *)pp->unitptr;
	if (!(up->flags & !(FLAG_AUX | FLAG_TTY))) {
		int err;

		err = atom_pps(peer);
		if (err > 0) {
			refclock_report(peer, CEVNT_FAULT);
			return;
		}
	}
#endif /* PPS || HAVE_PPSAPI */
	pp->polls++;
	if (peer->burst > 0)
		return;
	if (pp->coderecv == pp->codeproc) {
		refclock_report(peer, CEVNT_TIMEOUT);
		return;
	}

	/*
	 * Valid time (leap bits zero) is returned only if the prefer
	 * peer has survived the intersection algorithm and within
	 * clock_max of local time and not too long ago.  This ensures
	 * the pps time is within +-0.5 s of the local time and the
	 * seconds numbering is unambiguous.
	 */
	if (pps_update) {
		pp->leap = LEAP_NOWARNING;
	} else {
		pp->leap = LEAP_NOTINSYNC;
		return;
	}
	pp->variance = 0;
	record_clock_stats(&peer->srcadr, pp->a_lastcode);
	refclock_receive(peer);
	peer->burst = MAXSTAGE;
}

#else
int refclock_atom_bs;
#endif /* REFCLOCK */
