/*
 * refclock_atom - clock driver for 1-pps signals
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <ctype.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_unixtime.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"

#if defined(REFCLOCK) && defined(CLOCK_ATOM)

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
 * produced by a cesium clock, timing receiver or related equipment. It
 * can be used to remove accumulated jitter and retime a secondary
 * server when synchronized to a primary server over a congested, wide-
 * area network and before redistributing the time to local clients.
 *
 * Before this driver becomes active, the local clock must be set to
 * within +-500 ms by another means, such as a radio clock or NTP
 * itself. There are two ways to connect the PPS signal, normally at TTL
 * levels, to the computer. One is to shift to EIA levels and connect to
 * pin 8 (DCD) of a serial port. This requires a level converter and
 * may require a one-shot flipflop to lengthen the pulse. The other is
 * to connect the PPS signal directly to pin 10 (ACK) of a PC paralell
 * port. These methods are architecture dependent.
 *
 * Both methods require a modified device driver and kernel interface
 * compatible with the Pulse-per-Second API for Unix-like Operating
 * Systems, Version 1.0, RFC-2783 (PPSAPI). Implementations are
 * available for FreeBSD, Linux, SunOS, Solaris and Alpha. However, at
 * present only the Alpha implementation provides the full generality of
 * the API with multiple PPS drivers and multiple handles per driver.
 *
 * In many configurations a single port is used for the radio timecode
 * and PPS signal. In order to provide for this configuration and others
 * involving dedicated multiple serial/parallel ports, the driver first
 * attempts to open the device /dev/pps%d, where %d is the unit number.
 * If this fails, the driver attempts to open the device specified by
 * the pps configuration command. If a port is to be shared, the pps
 * command must be placed before the radio device(s) and the radio
 * device(s) must be placed before the PPS driver(s) in the
 * configuration file.
 *
 * This driver normally uses the PLL/FLL clock discipline implemented in
 * the ntpd code. If kernel support is available, the kernel PLL/FLL
 * clock discipline is used instead. The default configuration is not to
 * use the kernel PPS discipline, if present. The kernel PPS discipline
 * can be enabled using the pps command.
 *
 * Fudge Factors
 *
 * There are no special fudge factors other than the generic. The fudge
 * time1 parameter can be used to compensate for miscellaneous device
 * driver and OS delays.
 */
/*
 * Interface definitions
 */
#ifdef HAVE_PPSAPI
extern int pps_assert;		/* selects rising or falling edge */
extern int pps_hardpps;		/* enables the kernel PPS interface */
#define DEVICE		"/dev/pps%d" /* device name and unit */
#endif /* HAVE_PPSAPI */

#define	PRECISION	(-20)	/* precision assumed (about 1 us) */
#define	REFID		"PPS\0"	/* reference ID */
#define	DESCRIPTION	"PPS Clock Discipline" /* WRU */
#define NANOSECOND	1000000000 /* one second (ns) */
#define RANGEGATE	500000	/* range gate (ns) */
#define ASTAGE		8	/* filter stages */

static struct peer *pps_peer;	/* atom driver for PPS sources */

#ifdef HAVE_PPSAPI
/*
 * PPS unit control structure
 */
struct ppsunit {
	struct timespec ts;	/* last timestamp */
	int fddev;		/* pps device descriptor */
	pps_params_t pps_params; /* pps parameters */
	pps_info_t pps_info;	/* last pps data */
	pps_handle_t handle;	/* pps handlebars */
};
#endif /* HAVE_PPSAPI */

/*
 * Function prototypes
 */
static	int	atom_start	P((int, struct peer *));
static	void	atom_poll	P((int, struct peer *));
#ifdef HAVE_PPSAPI
static	void	atom_shutdown	P((int, struct peer *));
static	void	atom_control	P((int, struct refclockstat *, struct
				    refclockstat *, struct peer *));
static	int	atom_pps	P((struct peer *));
static	int	atom_ppsapi	P((struct peer *, int, int));
#endif /* HAVE_PPSAPI */

/*
 * Transfer vector
 */
struct	refclock refclock_atom = {
	atom_start,		/* start up driver */
#ifdef HAVE_PPSAPI
	atom_shutdown,		/* shut down driver */
#else
	noentry,		/* shut down driver */
#endif /* HAVE_PPSAPI */
	atom_poll,		/* transmit poll message */
#ifdef HAVE_PPSAPI
	atom_control,		/* fudge control */
#else
	noentry,		/* fudge control */
#endif /* HAVE_PPSAPI */
	noentry,		/* initialize driver */
	noentry,		/* not used (old atom_buginfo) */
	NOFLAGS			/* not used */
};


/*
 * atom_start - initialize data for processing
 */
static int
atom_start(
	int unit,		/* unit number (not used) */
	struct peer *peer	/* peer structure pointer */
	)
{
	struct refclockproc *pp;
#ifdef HAVE_PPSAPI
	register struct ppsunit *up;
	char device[80];
#endif /* HAVE_PPSAPI */

	/*
	 * Allocate and initialize unit structure
	 */
	pps_peer = peer;
	pp = peer->procptr;
	peer->precision = PRECISION;
	pp->clockdesc = DESCRIPTION;
	memcpy((char *)&pp->refid, REFID, 4);
	peer->burst = ASTAGE;
	peer->stratum = STRATUM_UNSPEC; 
#ifdef HAVE_PPSAPI
	up = emalloc(sizeof(struct ppsunit));
	memset(up, 0, sizeof(struct ppsunit));
	pp->unitptr = (caddr_t)up;

	/*
	 * Open PPS device. If this fails and some driver has already
	 * opened the associated radio device, fdpps has the file
	 * descriptor for it.
	 */
	sprintf(device, DEVICE, unit);
	up->fddev = open(device, O_RDWR, 0777);
	if (up->fddev <= 0 && fdpps > 0) {
		strcpy(device, pps_device);
		up->fddev = fdpps;
	}
	if (up->fddev <= 0) {
		msyslog(LOG_ERR,
		    "refclock_atom: %s: %m", device);
		return (0);
	}

	/*
	 * Light off the PPSAPI interface. If this PPS device is shared
	 * with the radio device, take the default options from the pps
	 * command. This is for legacy purposes.
	 */
	if (time_pps_create(up->fddev, &up->handle) < 0) {
		msyslog(LOG_ERR,
		    "refclock_atom: time_pps_create failed: %m");
		return (0);
	}
	return (atom_ppsapi(peer, pps_assert, pps_hardpps));
#else /* HAVE_PPSAPI */
	return (1);
#endif /* HAVE_PPSAPI */
}


#ifdef HAVE_PPSAPI
/*
 * atom_control - fudge control
 */
static void
atom_control(
	int unit,		/* unit (not used */
	struct refclockstat *in, /* input parameters (not uded) */
	struct refclockstat *out, /* output parameters (not used) */
	struct peer *peer	/* peer structure pointer */
	)
{
	struct refclockproc *pp;

	pp = peer->procptr;
	atom_ppsapi(peer, pp->sloppyclockflag & CLK_FLAG2,
	    pp->sloppyclockflag & CLK_FLAG3);
}


/*
 * Initialize PPSAPI
 */
int
atom_ppsapi(
	struct peer *peer,	/* peer structure pointer */
	int enb_clear,		/* clear enable */
	int enb_hardpps		/* hardpps enable */
	)
{
	struct refclockproc *pp;
	register struct ppsunit *up;
	int capability;

	pp = peer->procptr;
	up = (struct ppsunit *)pp->unitptr;
	if (time_pps_getcap(up->handle, &capability) < 0) {
		msyslog(LOG_ERR,
		    "refclock_atom: time_pps_getcap failed: %m");
		return (0);
	}
	memset(&up->pps_params, 0, sizeof(pps_params_t));
	if (enb_clear)
		up->pps_params.mode = capability & PPS_CAPTURECLEAR;
	else
		up->pps_params.mode = capability & PPS_CAPTUREASSERT;
	if (!up->pps_params.mode) {
		msyslog(LOG_ERR,
		    "refclock_atom: invalid capture edge %d",
		    pps_assert);
		return (0);
	}
	up->pps_params.mode |= PPS_TSFMT_TSPEC;
	if (time_pps_setparams(up->handle, &up->pps_params) < 0) {
		msyslog(LOG_ERR,
		    "refclock_atom: time_pps_setparams failed: %m");
		return (0);
	}
	if (enb_hardpps) {
		if (time_pps_kcbind(up->handle, PPS_KC_HARDPPS,
		    up->pps_params.mode & ~PPS_TSFMT_TSPEC,
		    PPS_TSFMT_TSPEC) < 0) {
			msyslog(LOG_ERR,
			    "refclock_atom: time_pps_kcbind failed: %m");
			return (0);
		}
		pps_enable = 1;
	}
#if DEBUG
	if (debug) {
		time_pps_getparams(up->handle, &up->pps_params);
		printf(
		    "refclock_ppsapi: fd %d capability 0x%x version %d mode 0x%x kern %d\n",
		    up->fddev, capability, up->pps_params.api_version,
		    up->pps_params.mode, enb_hardpps);
	}
#endif
	return (1);
}


/*
 * atom_shutdown - shut down the clock
 */
static void
atom_shutdown(
	int unit,		/* unit number (not used) */
	struct peer *peer	/* peer structure pointer */
	)
{
	struct refclockproc *pp;
	register struct ppsunit *up;

	pp = peer->procptr;
	up = (struct ppsunit *)pp->unitptr;
	if (up->fddev > 0)
		close(up->fddev);
	if (up->handle != 0)
		time_pps_destroy(up->handle);
	if (pps_peer == peer)
		pps_peer = 0;
	free(up);
}


/*
 * atom_pps - receive data from the PPSAPI interface
 *
 * This routine is called once per second when the PPSAPI interface is
 * present. It snatches the PPS timestamp from the kernel and saves the
 * sign-extended fraction in a circular buffer for processing at the
 * next poll event.
 */
static int
atom_pps(
	struct peer *peer	/* peer structure pointer */
	)
{
	register struct ppsunit *up;
	struct refclockproc *pp;
	pps_info_t pps_info;
	struct timespec timeout, ts;
	double dtemp;

	/*
	 * Convert the timespec nanoseconds field to signed double and
	 * save in the median filter. for billboards. No harm is done if
	 * previous data are overwritten. If the discipline comes bum or
	 * the data grow stale, just forget it. A range gate rejects new
	 * samples if less than a jiggle time from the next second.
	 */ 
	pp = peer->procptr;
	up = (struct ppsunit *)pp->unitptr;
	if (up->handle == 0)
		return (-1);
	timeout.tv_sec = 0;
	timeout.tv_nsec = 0;
	memcpy(&pps_info, &up->pps_info, sizeof(pps_info_t));
	if (time_pps_fetch(up->handle, PPS_TSFMT_TSPEC, &up->pps_info,
	    &timeout) < 0)
		return (-1);
	if (up->pps_params.mode & PPS_CAPTUREASSERT) {
		if (pps_info.assert_sequence ==
		    up->pps_info.assert_sequence)
			return (1);
		ts = up->pps_info.assert_timestamp;
	} else if (up->pps_params.mode & PPS_CAPTURECLEAR) {
		if (pps_info.clear_sequence ==
		    up->pps_info.clear_sequence)
			return (1);
		ts = up->pps_info.clear_timestamp;
	} else {
		return (-1);
	}
	if (!((ts.tv_sec == up->ts.tv_sec && ts.tv_nsec -
	    up->ts.tv_nsec > NANOSECOND - RANGEGATE) ||
	    (ts.tv_sec - up->ts.tv_sec == 1 && ts.tv_nsec -
	    up->ts.tv_nsec < RANGEGATE))) {
		up->ts = ts;
		return (1);
	}
	up->ts = ts;
	pp->lastrec.l_ui = ts.tv_sec + JAN_1970;
	dtemp = ts.tv_nsec * FRAC / 1e9;
	if (dtemp >= FRAC)
		pp->lastrec.l_ui++;
	pp->lastrec.l_uf = (u_int32)dtemp;
	if (ts.tv_nsec > NANOSECOND / 2)
		ts.tv_nsec -= NANOSECOND;
	dtemp = -(double)ts.tv_nsec / NANOSECOND;
	SAMPLE(dtemp + pp->fudgetime1);
#ifdef DEBUG
	if (debug > 1)
		printf("atom_pps %f %f\n", dtemp, pp->fudgetime1);
#endif
	return (0);
}
#endif /* HAVE_PPSAPI */


/*
 * pps_sample - receive PPS data from some other clock driver
 *
 * This routine is called once per second when the external clock driver
 * processes PPS information. It processes the PPS timestamp and saves
 * the sign-extended fraction in a circular buffer for processing at the
 * next poll event. This works only for a single PPS device.
 */
int
pps_sample(
	   l_fp *offset		/* PPS offset */
	   )
{
	register struct peer *peer;
	struct refclockproc *pp;
	l_fp lftmp;
	double doffset;

	peer = pps_peer;
	if (peer == 0)		/* nobody home */
		return (1);
	pp = peer->procptr;

	/*
	 * Convert the timeval to l_fp and save for billboards. Sign-
	 * extend the fraction and stash in the buffer. No harm is done
	 * if previous data are overwritten. If the discipline comes bum
	 * or the data grow stale, just forget it.
	 */ 
	pp->lastrec = *offset;
	L_CLR(&lftmp);
	L_ADDF(&lftmp, pp->lastrec.l_f);
	LFPTOD(&lftmp, doffset);
	SAMPLE(-doffset + pp->fudgetime1);
	return (0);
}

/*
 * atom_poll - called by the transmit procedure
 *
 * This routine is called once per second when in burst mode to save PPS
 * sample offsets in the median filter. At the end of the burst period
 * the samples are processed as a heap and the clock filter updated.
 */
static void
atom_poll(
	int unit,		/* unit number (not used) */
	struct peer *peer	/* peer structure pointer */
	)
{
	struct refclockproc *pp;
#ifdef HAVE_PPSAPI
	int err;
#endif /* HAVE_PPSAPI */

	/*
	 * Accumulate samples in the median filter. If a noise sample,
	 * return with no prejudice; if a protocol error, get mean;
	 * otherwise, cool. At the end of each poll interval, do a
	 * little bookeeping and process the surviving samples.
	 */
	pp = peer->procptr;
	pp->polls++;
#ifdef HAVE_PPSAPI
	err = atom_pps(peer);
	if (err < 0) {
		refclock_report(peer, CEVNT_FAULT);
		return;
	}
#endif /* HAVE_PPSAPI */

	/*
	 * Valid time is returned only if the prefer peer has survived
	 * the intersection algorithm and within clock_max of local time
	 * and not too long ago. This ensures the PPS time is within
	 * +-0.5 s of the local time and the seconds numbering is
	 * unambiguous. Note that the leap bits are set no-warning on
	 * the first valid update and the stratum is set at the prefer
	 * peer.
	 */
	if (peer->burst > 0)
		return;
	peer->stratum = STRATUM_UNSPEC;
	if (pp->codeproc == pp->coderecv) {
		refclock_report(peer, CEVNT_TIMEOUT);
		peer->burst = ASTAGE;
		return;

	} else if (!sys_prefer) {
		pp->codeproc = pp->coderecv;
		peer->burst = ASTAGE;
		return;

	} else if (fabs(sys_prefer->offset) > clock_max) {
		pp->codeproc = pp->coderecv;
		peer->burst = ASTAGE;
		return;
	}
	peer->stratum = sys_prefer->stratum;
	if (peer->stratum <= 1)
		peer->refid = pp->refid;
	else
		peer->refid = peer->srcadr.sin_addr.s_addr;
	pp->leap = LEAP_NOWARNING;
	refclock_receive(peer);
	peer->burst = ASTAGE;
}
#else
int refclock_atom_bs;
int
pps_sample(
	   l_fp *offset		/* PPS offset */
	   )
{
	return 1;
}
#endif /* REFCLOCK */
