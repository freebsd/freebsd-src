/*
 * refclock_local - local pseudo-clock driver
 */
#if defined(REFCLOCK) && defined(LOCAL_CLOCK)

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"

/*
 * This is a hack to allow a machine to use its own system clock as a
 * reference clock, i.e., to free-run using no outside clock discipline
 * source. This is useful if you want to use NTP in an isolated
 * environment with no radio clock or NIST modem available. Pick a
 * machine that you figure has a good clock oscillator and configure it
 * with this driver. Set the clock using the best means available, like
 * eyeball-and-wristwatch. Then, point all the other machines at this
 * one or use broadcast (not multicast) mode to distribute time.
 *
 * Another application for this driver is if you want to use a
 * particular server's clock as the clock of last resort when all other
 * normal synchronization sources have gone away. This is especially
 * useful if that server has an ovenized oscillator. For this you would
 * configure this driver at a higher stratum (say 3 or 4) to prevent the
 * server's stratum from falling below that.
 *
 * A third application for this driver is when an external discipline
 * source is available, such as the NIST "lockclock" program, which
 * synchronizes the local clock via a telephone modem and the NIST
 * Automated Computer Time Service (ACTS), or the Digital Time
 * Synchronization Service (DTSS), which runs on DCE machines. In this
 * case the stratum should be set at zero, indicating a bona fide
 * stratum-1 source. Exercise some caution with this, since there is no
 * easy way to telegraph via NTP that something might be wrong in the
 * discipline source itself. In the case of DTSS, the local clock can
 * have a rather large jitter, depending on the interval between
 * corrections and the intrinsic frequency error of the clock
 * oscillator. In extreme cases, this can cause clients to exceed the
 * 128-ms slew window and drop off the NTP subnet.
 *
 * In the default mode the behavior of the clock selection algorithm is
 * modified when this driver is in use. The algorithm is designed so
 * that this driver will never be selected unless no other discipline
 * source is available. This can be overriden with the prefer keyword of
 * the server configuration command, in which case only this driver will
 * be selected for synchronization and all other discipline sources will
 * be ignored. This behavior is intended for use when an external
 * discipline source controls the system clock.
 *
 * Fudge Factors
 *
 * The stratum for this driver LCLSTRATUM is set at 3 by default, but
 * can be changed by the fudge command and/or the xntpdc utility. The
 * reference ID is "LCL" by default, but can be changed using the same
 * mechanisms. *NEVER* configure this driver to operate at a stratum
 * which might possibly disrupt a client with access to a bona fide
 * primary server, unless athe local clock oscillator is reliably
 * disciplined by another source. *NEVER NEVER* configure a server which
 * might devolve to an undisciplined local clock to use multicast mode.
 *
 * This driver provides a mechanism to trim the local clock in both time
 * and frequency, as well as a way to manipulate the leap bits. The
 * fudge time1 parameter adjusts the time, in seconds, and the fudge
 * time2 parameter adjusts the frequency, in ppm. Both parameters are
 * additive; that is, they add increments in time or frequency to the
 * present values. The fudge flag1 and fudge flag2 bits set the
 * corresponding leap bits; for example, setting flag1 causes a leap
 * second to be added at the end of the UTC day. These bits are not
 * reset automatically when the leap takes place; they must be turned
 * off manually after the leap event.
 */

/*
 * Local interface definitions
 */
#define	PRECISION	(-7)	/* about 10 ms precision */
#define	REFID		"LCL\0"	/* reference ID */
#define	DESCRIPTION	"Undisciplined local clock" /* WRU */

#define STRATUM		3	/* default stratum */
#define DISPERSION	(FP_SECOND / 100) /* default dispersion (10 ms) */

/*
 * Imported from the timer module
 */
extern u_long current_time;

/*
 * Imported from ntp_proto
 */
extern s_char sys_precision;

/*
 * Function prototypes
 */
static	int	local_start	P((int, struct peer *));
static	void	local_poll	P((int, struct peer *));

/*
 * Transfer vector
 */
struct	refclock refclock_local = {
	local_start,		/* start up driver */
	noentry,		/* shut down driver (not used) */
	local_poll,		/* transmit poll message */
	noentry,		/* not used (old lcl_control) */
	noentry,		/* initialize driver (not used) */
	noentry,		/* not used (old lcl_buginfo) */
	NOFLAGS			/* not used */
};


/*
 * local_start - start up the clock
 */
static int
local_start(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct refclockproc *pp;

	pp = peer->procptr;

	/*
	 * Initialize miscellaneous variables
	 */
	peer->precision = sys_precision;
	pp->clockdesc = DESCRIPTION;
	peer->stratum = STRATUM;
	memcpy((char *)&pp->refid, REFID, 4);
	return (1);
}


/*
 * local_poll - called by the transmit procedure
 */
static void
local_poll(unit, peer)
	int unit;
	struct peer *peer;
{
	struct refclockproc *pp;

	pp = peer->procptr;
	pp->polls++;
	pp->lasttime = current_time;

	/*
	 * Ramble through the usual filtering and grooming code, which
	 * is essentially a no-op and included mostly for pretty
	 * billboards. We fudge flags as the leap indicators and allow a
	 * one-time adjustment in time using fudge time1 (s) and
	 * frequency using fudge time 2 (ppm).
	 */
	pp->dispersion = DISPERSION;
	gettstamp(&pp->lastrec);
	refclock_receive(peer, &pp->fudgetime1, 0, pp->dispersion,
	    &pp->lastrec, &pp->lastrec, pp->sloppyclockflag);
	adj_frequency(LFPTOFP(&pp->fudgetime2));
	L_CLR(&pp->fudgetime1);
	L_CLR(&pp->fudgetime2);
}

#endif	/* REFCLOCK */
