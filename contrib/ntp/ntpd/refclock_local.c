/* wjm 17-aug-1995: add a hook for special treatment of VMS_LOCALUNIT */

/*
 * refclock_local - local pseudo-clock driver
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef REFCLOCK

#include "ntpd.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"

#include <stdio.h>
#include <ctype.h>

#ifdef KERNEL_PLL
#include "ntp_syscall.h"
#endif

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
 * configure this driver at a higher stratum (say 5) to prevent the
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
 * THis driver includes provisions to telegraph synchronization state
 * and related variables by means of kernel variables with specially
 * modified kernels. This is done using the ntp_adjtime() syscall.
 * In the cases where another protocol or device synchronizes the local
 * host, the data given to the kernel can be slurped up by this driver
 * and distributed to clients by ordinary NTP messaging.
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
 * The stratum for this driver set at 5 by default, but it can be changed
 * by the fudge command and/or the ntpdc utility. The reference ID is
 * "LCL" by default, but can be changed using the same mechanism. *NEVER*
 * configure this driver to operate at a stratum which might possibly
 * disrupt a client with access to a bona fide primary server, unless the
 * local clock oscillator is reliably disciplined by another source.
 * *NEVER NEVER* configure a server which might devolve to an undisciplined
 * local clock to use multicast mode. Always remember that an improperly
 * configured local clock driver let loose in the Internet can cause
 * very serious disruption. This is why most of us who care about good
 * time use cryptographic authentication.
 *
 * This driver provides a mechanism to trim the local clock in both time
 * and frequency, as well as a way to manipulate the leap bits. The
 * fudge time1 parameter adjusts the time, in seconds, and the fudge
 * time2 parameter adjusts the frequency, in ppm. The fudge time1 parameter
 * is additive; that is, it adds an increment to the current time. The
 * fudge time2 parameter directly sets the frequency.
 */

/*
 * Local interface definitions
 */
#define PRECISION	(-7)	/* about 10 ms precision */
#define REFID		"LCL\0" /* reference ID */
#define DESCRIPTION "Undisciplined local clock" /* WRU */

#define STRATUM 	5	/* default stratum */
#define DISPERSION	.01	/* default dispersion (10 ms) */

/*
 * Imported from the timer module
 */
extern u_long current_time;

/*
 * Imported from ntp_proto
 */
extern s_char sys_precision;

#ifdef KERNEL_PLL
/*
 * Imported from ntp_loopfilter
 */
extern int pll_control; 	/* kernel pll control */
extern int kern_enable;		/* kernel pll enabled */
extern int ext_enable;		/* external clock enable */
#endif /* KERNEL_PLL */

/*
 * Function prototypes
 */
static	int local_start P((int, struct peer *));
static	void	local_poll	P((int, struct peer *));

/*
 * Local variables
 */
static	u_long poll_time;	/* last time polled */
	
/*
 * Transfer vector
 */
struct	refclock refclock_local = {
	local_start,		/* start up driver */
	noentry,		/* shut down driver (not used) */
	local_poll,	 	/* transmit poll message */
	noentry,		/* not used (old lcl_control) */
	noentry,		/* initialize driver (not used) */
	noentry,		/* not used (old lcl_buginfo) */
	NOFLAGS 		/* not used */
};


/*
 * local_start - start up the clock
 */
static int
local_start(
	int unit,
	struct peer *peer
	)
{
	struct refclockproc *pp;

	pp = peer->procptr;

	/*
	 * Initialize miscellaneous variables
	 */
	peer->precision = sys_precision;
	peer->stratum = STRATUM;
	pp->clockdesc = DESCRIPTION;
	memcpy((char *)&pp->refid, REFID, 4);
#if defined(VMS) && defined(VMS_LOCALUNIT)
	/* provide a non-standard REFID */
	if(unit == VMS_LOCALUNIT)
		memcpy((char *)&pp->refid,"LCLv",4);
#endif	/* VMS && VMS_LOCALUNIT */
	poll_time = current_time;
	return (1);
}


/*
 * local_poll - called by the transmit procedure
 */
static void
local_poll(
	int unit,
	struct peer *peer
	)
{
	struct refclockproc *pp;
#if defined(KERNEL_PLL) && defined(STA_CLK)
	struct timex ntv;
	int retval;
#endif /* KERNEL_PLL STA_CLK */

#if defined(VMS) && defined(VMS_LOCALUNIT)
	if(unit == VMS_LOCALUNIT) {
		extern void vms_local_poll(struct peer *);

		vms_local_poll(peer);
		return;
	}
#endif	/* VMS && VMS_LOCALUNIT */
	pp = peer->procptr;
	pp->polls++;

	/*
	 * Ramble through the usual filtering and grooming code, which
	 * is essentially a no-op and included mostly for pretty
	 * billboards. We allow a one-time time adjustment using fudge
	 * time1 (s) and a continuous frequency adjustment using fudge
	 * time 2 (ppm).
	 */
	get_systime(&pp->lastrec);
	pp->fudgetime1 += pp->fudgetime2 * 1e-6 * (current_time -
	    poll_time);
	poll_time = current_time;
	refclock_process_offset(pp, pp->lastrec, pp->lastrec, pp->fudgetime1);
	pp->leap = LEAP_NOWARNING;
	pp->disp = DISPERSION;
	pp->jitter = 0;
#if defined(KERNEL_PLL) && defined(STA_CLK)

	/*
	 * If the kernel pll code is up and running, somebody else
	 * may come diddle the clock. If so, they better use ntp_adjtime(),
	 * and set the STA_CLK bit in the status word. In this case, the
	 * performance information is read from the kernel and becomes the
	 * variables presented to the clock mitigation process.
	 */
	if (pll_control && kern_enable && (peer->flags & FLAG_PREFER)) {
		memset((char *)&ntv,  0, sizeof ntv);
		retval = ntp_adjtime(&ntv);
		if (ntv.status & STA_CLK) {
			ext_enable = 1;
			switch(retval) {

				case TIME_OK:
				pp->leap = LEAP_NOWARNING;
				break;

				case TIME_INS:
				pp->leap = LEAP_ADDSECOND;
				break;

				case TIME_DEL:
				pp->leap = LEAP_DELSECOND;
				break;

				case TIME_ERROR:
				pp->leap = LEAP_NOTINSYNC;
			}
			pp->disp = ntv.maxerror / 1e6;
			pp->jitter = SQUARE(ntv.esterror / 1e6);
		}
	} else {
		ext_enable = 0;
	}
#endif /* KERNEL_PLL STA_CLK */
	refclock_receive(peer);
	pp->fudgetime1 = 0;
}
#else
int refclock_local_bs;
#endif /* REFCLOCK */
