/*
 * refclock_atom - clock driver for 1-pps signals
 */
#if defined(REFCLOCK) && defined(ATOM)

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_unixtime.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"

#ifdef PPS
#include <sys/ppsclock.h>
#endif /* PPS */

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
 * box consisting of a one-shot and RS232 level converter. When operated
 * at 38.4 kbps with a SPARCstation IPC, this arrangement has a worst-
 * case jitter less than 26 us.
 *
 * There are three ways in which this driver can be used. The first way
 * uses the LDISC_PPS line discipline and works only for the baseboard
 * serial ports of the Sun SPARCstation. The PPS signal is connected via
 * a gadget box to the carrier detect (CD) line of a serial port and
 * flag3 of the driver configured for that port is set. This causes the
 * ppsclock streams module to be configured for that port and capture a
 * timestamp at the on-time transition of the PPS signal. This driver
 * then reads the timestamp directly by a designated ioctl() system
 * call. This provides the most accurate time and least jitter of any
 * other scheme. There is no need to configure a dedicated device for
 * this purpose, which ordinarily is the device used for the associated
 * radio clock.
 *
 * The second way uses the LDISC_CLKPPS line discipline and works for
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
 * The third way involves an auxiliary radio clock driver which calls
 * the PPS driver with a timestamp captured by that driver. This use is
 * documented in the source code for the driver(s) involved.
 *
 * Fudge Factors
 *
 * There are no special fudge factors other than the generic and those
 * explicitly defined above. The fudge time1 parameter can be used to
 * compensate for miscellaneous UART and OS delays. Allow about 247 us
 * for uart delays at 38400 bps and about 1 ms for SunOS streams
 * nonsense.
 */

/*
 * Interface definitions
 */
#define	DEVICE		"/dev/pps%d" /* device name and unit */
#ifdef B38400
#define	SPEED232	B38400	/* uart speed (38400 baud) */
#else
#define SPEED232	EXTB	/* as above */
#endif
#define	PRECISION	(-20)	/* precision assumed (about 1 usec) */
#define	REFID		"PPS\0"	/* reference ID */
#define	DESCRIPTION	"PPS Clock Discipline" /* WRU */

#define PPSMAXDISPERSE	(FP_SECOND / 100) /* max sample dispersion */
#define	NSAMPLES	32	/* final stages of median filter */
#ifdef PPS
#define PPS_POLL	2	/* ppsclock poll interval (s) */
#endif /* PPS */

/*
 * Imported from ntp_timer module
 */
extern u_long current_time;	/* current time (s) */
extern struct event timerqueue[]; /* inner space */

/*
 * Imported from ntpd module
 */
extern int debug;		/* global debug flag */

/*
 * Imported from ntp_loopfilter module
 */
extern int fdpps;		/* pps file descriptor */
extern int pps_update;		/* prefer peer valid update */

/*
 * Imported from ntp_proto module
 */
extern struct peer *sys_peer;	/* somebody in charge */

/*
 * Unit control structure
 */
struct atomunit {
#ifdef PPS
	struct	event timer;	/* pps poll interval timer */
	struct	ppsclockev ev;	/* ppsclock control */
#endif /* PPS */
	int	pollcnt;	/* poll message counter */
};

/*
 * Global variables
 */
struct peer *last_atom_peer;	/* peer structure pointer */

/*
 * Function prototypes
 */
static	int	atom_start	P((int, struct peer *));
static	void	atom_shutdown	P((int, struct peer *));
static	void	atom_receive	P((struct recvbuf *));
static	void	atom_poll	P((int, struct peer *));
#ifdef PPS
static	void	atom_pps	P((struct peer *));
#endif /* PPS */

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
atom_start(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct atomunit *up;
	struct refclockproc *pp;

	/*
	 * Allocate and initialize unit structure
	 */
	if (!(up = (struct atomunit *)
	    emalloc(sizeof(struct atomunit))))
		return (0);
	memset((char *)up, 0, sizeof(struct atomunit));
	pp = peer->procptr;
	pp->unitptr = (caddr_t)up;

	/*
	 * Initialize miscellaneous variables
	 */
	peer->precision = PRECISION;
	pp->clockdesc = DESCRIPTION;
	memcpy((char *)&pp->refid, REFID, 4);
	up->pollcnt = 2;
	pp->nstages = MAXSTAGE;

#ifdef PPS
	/*
	 * Arm the timer for the first interrupt. Give it ten seconds to
	 * allow the ppsclock line to be configured, since it could be
	 * assigned to another driver.
	 */
	up->timer.peer = (struct peer *)peer;
	up->timer.event_handler = atom_pps;
	up->timer.event_time = current_time + 10;
	TIMER_INSERT(timerqueue, &up->timer);
#endif /* PPS */
	last_atom_peer = peer;
	return (1);
}


/*
 * atom_shutdown - shut down the clock
 */
static void
atom_shutdown(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct atomunit *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct atomunit *)pp->unitptr;

	if (last_atom_peer == peer)
		last_atom_peer = 0;
#ifdef PPS
	TIMER_DEQUEUE(&up->timer);
#endif /* PPS */
	if (pp->io.fd)
		io_closeclock(&pp->io);
	free(up);
}

/*
 * pps_sample - process pps sample offset -- backwards compatible
 *		interface
 */
int
pps_sample(tsr)
	l_fp *tsr;
{
	struct peer *peer;
	struct refclockproc *pp;
	register struct atomunit *up;
	int i;
	l_fp lftemp;		/* l_fp temps */

	/*
	 * This routine is called once per second by an auxilliary
	 * routine in another driver. It saves the sign-extended
	 * fraction supplied in the argument in a circular buffer for
	 * processing at the next poll event.
	 */
	peer = last_atom_peer;
	if (!peer)
		return (-1);	/* no ATOM configured ? Forget it ! */

	pp = peer->procptr;
	up = (struct atomunit *)pp->unitptr;

	L_CLR(&lftemp);
	L_ADDF(&lftemp, tsr->l_f);
	i = ((int)(pp->coderecv)) % pp->nstages;
	pp->filter[i] = lftemp;
	if (pp->coderecv == 0)
		for (i = 1; i < pp->nstages; i++)
			pp->filter[i] = pp->filter[0];
	pp->coderecv++;
	up->pollcnt = 2;

	/* HACK -- use the local UN*X clock to get the time -- this is wrong */
	pp->lastrec.l_ui = time(0) - 2 + JAN_1970;
	pp->lastrec.l_uf = 0;

	return (0);
}

#ifdef PPS
/*
 * atom_pps - receive data from the LDISC_PPS discipline
 */
static void
atom_pps(peer)
	struct peer *peer;
{
	register struct atomunit *up;
	struct refclockproc *pp;
	l_fp lftmp;
	int i;

	/*
	 * This routine is called once per second when the LDISC_PPS
	 * discipline is present. It snatches the pps timestamp from the
	 * kernel and saves the sign-extended fraction in a circular
	 * buffer for processing at the next poll event.
	 */
	pp = peer->procptr;
	up = (struct atomunit *)pp->unitptr;

	/*
	 * Arm the timer for the next interrupt
	 */
	up->timer.event_time = current_time + PPS_POLL;
	TIMER_INSERT(timerqueue, &up->timer);

	/*
	 * Convert the timeval to l_fp and save for billboards. Sign-
	 * extend the fraction and stash in the buffer. No harm is done
	 * if previous data are overwritten. If the discipline comes bum
	 * or the data grow stale, just forget it.
	 */ 
	i = up->ev.serial;
	if (ioctl(fdpps, CIOGETEV, (caddr_t)&up->ev) < 0)
		return;
	if (i == up->ev.serial)
		return;
	pp->lastrec.l_ui = up->ev.tv.tv_sec + JAN_1970;
	TVUTOTSF(up->ev.tv.tv_usec, pp->lastrec.l_uf);
	L_CLR(&lftmp);
	L_ADDF(&lftmp, pp->lastrec.l_f);
	L_NEG(&lftmp);
	i = ((int)(pp->coderecv)) % pp->nstages;
	pp->filter[i] = lftmp;
	if (pp->coderecv == 0)
		for (i = 1; i < pp->nstages; i++)
			pp->filter[i] = pp->filter[0];
	pp->coderecv++;
	up->pollcnt = 2;
}
#endif /* PPS */

/*
 * atom_receive - receive data from the serial line interface
 */
static void
atom_receive(rbufp)
	struct recvbuf *rbufp;
{
	register struct atomunit *up;
	struct refclockproc *pp;
	struct peer *peer;
	l_fp lftmp;
	int i;

	/*
	 * This routine is called once per second when the serial
	 * interface is in use. It snatches the timestamp from the
	 * buffer and saves the sign-extended fraction in a circular
	 * buffer for processing at the next poll event.
	 */
	peer = (struct peer *)rbufp->recv_srcclock;
	pp = peer->procptr;
	up = (struct atomunit *)pp->unitptr;
	pp->lencode = refclock_gtlin(rbufp, pp->lastcode, BMAX,
	    &pp->lastrec);

	/*
	 * Save the timestamp for billboards. Sign-extend the fraction
	 * and stash in the buffer. No harm is done if previous data are
	 * overwritten.
	 */
	L_CLR(&lftmp);
	L_ADDF(&lftmp, pp->lastrec.l_f);
	L_NEG(&lftmp);
	i = ((int)(pp->coderecv)) % pp->nstages;
	pp->filter[i] = lftmp;
	if (pp->coderecv == 0)
		for (i = 1; i < pp->nstages; i++)
			pp->filter[i] = pp->filter[0];
	pp->coderecv++;
	up->pollcnt = 2;
}

/*
 * Compare two l_fp's - used with qsort()
 */
static int
atom_cmpl_fp(p1, p2)
	register void *p1, *p2;	/* l_fp to compare */
{

	if (!L_ISGEQ((l_fp *)p1, (l_fp *)p2))
		return (-1);
	if (L_ISEQU((l_fp *)p1, (l_fp *)p2))
		return (0);
	return (1);
}

/*
 * atom_poll - called by the transmit procedure
 */
static void
atom_poll(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct atomunit *up;
	struct refclockproc *pp;
	int i, n;
	l_fp median, lftmp;
	l_fp off[MAXSTAGE];
	u_fp disp;

	/*
	 * At each poll we check for timeout. At the first timeout we
	 * test to see if the LDISC_PPS discipline is present and, if
	 * so, use that. If not, we attempt to open a serial line with
	 * LDISC_CLKPPS discipline. If that fails, we bitch to the log
	 * and clam up.
	 */
	pp = peer->procptr;
	up = (struct atomunit *)pp->unitptr;
	pp->polls++;
	if (up->pollcnt == 0) {
		refclock_report(peer, CEVNT_FAULT);
		return;
	}
	up->pollcnt--;
	if (up->pollcnt == 0) {
		if (!pp->io.fd && fdpps == -1) {
			int fd;
			char device[20];

			/*
			 * Open serial port. Use CLKPPS line discipline,
			 * if available. If unavailable, the code works
			 * anyway, but at reduced accuracy.
			 */
			(void)sprintf(device, DEVICE, unit);
			if (!(fd = refclock_open(device, SPEED232,
	 		    LDISC_CLKPPS))) {
				refclock_report(peer, CEVNT_FAULT);
				return;
			}
			pp->io.clock_recv = atom_receive;
			pp->io.srcclock = (caddr_t)peer;
			pp->io.datalen = 0;
			pp->io.fd = fd;
			if (!io_addclock(&pp->io)) {
				(void) close(fd);
				refclock_report(peer, CEVNT_FAULT);
				return;
			}
		}
	}

 	/*
	 * Valid time (leap bits zero) is returned only if the prefer
	 * peer has survived the intersection algorithm and within
	 * CLOCK_MAX of local time and not too long ago. This insures
	 * the pps time is within +-0.5 s of the local time and the
	 * seconds numbering is unambiguous.
	 */
	if (pps_update) {
		pp->leap = 0;
		pp->lasttime = current_time;
	} else
		pp->leap = LEAP_NOTINSYNC;

	/*
	 * Copy the raw offsets and sort into ascending order
	 */
	for (i = 0; i < MAXSTAGE; i++)
		off[i] = pp->filter[i];
	qsort((char *)off, pp->nstages, sizeof(l_fp), atom_cmpl_fp);

	/*
	 * Reject the furthest from the median of nstages samples until
	 * nskeep samples remain.
	 */
	i = 0;
	n = pp->nstages;
	while ((n - i) > NSAMPLES) {
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
	 * Otherwise, return the median offset plus the configured
	 * fudgetime1 value.
	 */
	lftmp = off[n - 1];
	L_SUB(&lftmp, &off[i]);
	disp = LFPTOFP(&lftmp) + current_time - pp->lasttime;
	if (disp > PPSMAXDISPERSE) {
		refclock_report(peer, CEVNT_BADTIME);
		return;
	}
	pp->offset = off[(n + 1) / 2];
	L_ADD(&pp->offset, &pp->fudgetime1);
	pp->dispersion = disp;
	refclock_receive(peer, &pp->offset, 0, pp->dispersion,
	&pp->lastrec, &pp->lastrec, pp->leap);
}

#endif
