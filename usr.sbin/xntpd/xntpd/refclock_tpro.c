/*
 * refclock_tpro - clock driver for the  KSI/Odetics TPRO-S IRIG-B reader
 */
#if defined(REFCLOCK) && defined(TPRO)
#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_unixtime.h"
#include "sys/tpro.h"
#include "ntp_stdlib.h"

/*
 * This driver supports the KSI/Odetecs TPRO-S IRIG-B reader and TPRO-
 * SAT GPS receiver for the Sun Microsystems SBus. It requires that the
 * tpro.o device driver be installed and loaded.
 */ 

/*
 * Definitions
 */
#define	MAXUNITS 1		/* max number of TPRO units */
#define	TPROFD "/dev/tpro%d"	/* name of driver device */
#define BMAX 50			/* timecode buffer length */

/*
 * TPRO interface parameters. The "IRIG" can be changed to "GPS" for the
 * TPRO-GPS.
 */
#define	TPROPRECISION	(-20)	/* precision assumed (1 us) */
#define	TPROREFID	"IRIG"	/* reference id */
#define	TPRODESCRIPTION	"KSI/Odetics TPRO-S IRIG-B Reader" /* who we are */
#define	TPROHSREFID	0x7f7f0c0a /* 127.127.12.10 refid hi strata */
#define GMT		0	/* hour offset from Greenwich */

/*
 * Hack to avoid excercising the multiplier.  I have no pride.
 */
#define	MULBY10(x)	(((x)<<3) + ((x)<<1))

/*
 * Imported from ntp_timer module
 */
extern U_LONG current_time;	/* current time (s) */

/*
 * Imported from ntpd module
 */
extern int debug;		/* global debug flag */

/*
 * TPRO unit control structure.
 */
struct tprounit {
	struct peer *peer;	/* associated peer structure */
	struct refclockio io;	/* given to the I/O handler */
	struct tproval tprodata; /* data returned from tpro read */
	l_fp lastrec;		/* last local time */
	l_fp lastref;		/* last timecode time */
	char lastcode[BMAX];	/* last timecode received */
	u_char lencode;		/* length of last timecode */
	U_LONG lasttime;	/* last time clock heard from */
	u_char unit;		/* unit number for this guy */
	u_char status;		/* clock status */
	u_char lastevent;	/* last clock event */
	u_char year;		/* year of eternity */
	u_short day;		/* day of year */
	u_char hour;		/* hour of day */
	u_char minute;		/* minute of hour */
	u_char second;		/* seconds of minute */
	U_LONG usec;		/* microsecond of second */
	U_LONG yearstart;	/* start of current year */
	u_char leap;		/* leap indicators */
	/*
	 * Status tallies
 	 */
	U_LONG polls;		/* polls sent */
	U_LONG noreply;		/* no replies to polls */
	U_LONG coderecv;	/* timecodes received */
	U_LONG badformat;	/* bad format */
	U_LONG baddata;		/* bad data */
	U_LONG timestarted;	/* time we started this */
};

/*
 * Data space for the unit structures.  Note that we allocate these on
 * the fly, but never give them back.
 */
static struct tprounit *tprounits[MAXUNITS];
static u_char unitinuse[MAXUNITS];

/*
 * Keep the fudge factors separately so they can be set even
 * when no clock is configured.
 */
static l_fp fudgefactor[MAXUNITS];
static u_char stratumtouse[MAXUNITS];
static u_char sloppyclockflag[MAXUNITS];

/*
 * Function prototypes
 */
static	void	tpro_init	P(());
static	int	tpro_start	P((u_int, struct peer *));
static	void	tpro_shutdown	P((int));
static	void	tpro_report_event	P((struct tprounit *, int));
static	void	tpro_receive	P((struct recvbuf *));
static	void	tpro_poll	P((int unit, struct peer *));
static	void	tpro_control	P((u_int, struct refclockstat *, struct refclockstat *));
static	void	tpro_buginfo	P((int, struct refclockbug *));

/*
 * Transfer vector
 */
struct  refclock refclock_tpro = {
	tpro_start, tpro_shutdown, tpro_poll,
	tpro_control, tpro_init, tpro_buginfo, NOFLAGS
};

/*
 * tpro_init - initialize internal tpro driver data
 */
static void
tpro_init()
{
	register int i;
	/*
	 * Just zero the data arrays
	 */
	bzero((char *)tprounits, sizeof tprounits);
	bzero((char *)unitinuse, sizeof unitinuse);

	/*
	 * Initialize fudge factors to default.
	 */
	for (i = 0; i < MAXUNITS; i++) {
		fudgefactor[i].l_ui = 0;
		fudgefactor[i].l_uf = 0;
		stratumtouse[i] = 0;
		sloppyclockflag[i] = 0;
	}
}

/*
 * tpro_start - open the TPRO device and initialize data for processing
 */
static int
tpro_start(unit, peer)
	u_int unit;
	struct peer *peer;
{
	register struct tprounit *tpro;
	register int i;
	char tprodev[20];
	int fd_tpro;

	/*
	 * Check configuration info.
	 */
	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "tpro_start: unit %d invalid", unit);
		return (0);
	}
	if (unitinuse[unit]) {
		syslog(LOG_ERR, "tpro_start: unit %d in use", unit);
		return (0);
	}

	/*
	 * Open TPRO device
	 */
	(void) sprintf(tprodev, TPROFD, unit);
	fd_tpro = open(tprodev, O_RDWR, 0777);
	if (fd_tpro == -1) {
		syslog(LOG_ERR, "tpro_start: open of %s: %m", tprodev);
		return (0);
	}

	/*
	 * Allocate unit structure
	 */
	if (tprounits[unit] != 0) {
		tpro = tprounits[unit];	/* The one we want is okay */
	} else {
		for (i = 0; i < MAXUNITS; i++) {
			if (!unitinuse[i] && tprounits[i] != 0)
				break;
		}
		if (i < MAXUNITS) {
			/*
			 * Reclaim this one
			 */
			tpro = tprounits[i];
			tprounits[i] = 0;
		} else {
			tpro = (struct tprounit *)
			    emalloc(sizeof(struct tprounit));
		}
	}
	bzero((char *)tpro, sizeof(struct tprounit));
	tprounits[unit] = tpro;

	/*
	 * Set up the structures
	 */
	tpro->peer = peer;
	tpro->unit = (u_char)unit;
	tpro->timestarted = current_time;

	tpro->io.clock_recv = tpro_receive;
	tpro->io.srcclock = (caddr_t)tpro;
	tpro->io.datalen = 0;
	tpro->io.fd = fd_tpro;

	/*
	 * All done.  Initialize a few random peer variables, then
	 * return success. Note that root delay and root dispersion are
	 * always zero for this clock.
	 */
	peer->precision = TPROPRECISION;
	peer->rootdelay = 0;
	peer->rootdispersion = 0;
	peer->stratum = stratumtouse[unit];
	if (stratumtouse[unit] <= 1)
	    bcopy(TPROREFID, (char *)&peer->refid, 4);
	else
	    peer->refid = htonl(TPROHSREFID);
	unitinuse[unit] = 1;
	return (1);
}


/*
 * tpro_shutdown - shut down a TPRO clock
 */
static void
tpro_shutdown(unit)
	int unit;
{
	register struct tprounit *tpro;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "tpro_shutdown: unit %d invalid", unit);
		return;
	}
	if (!unitinuse[unit]) {
		syslog(LOG_ERR, "tpro_shutdown: unit %d not in use", unit);
		return;
	}

	/*
	 * Tell the I/O module to turn us off.  We're history.
	 */
	tpro = tprounits[unit];
	io_closeclock(&tpro->io);
	unitinuse[unit] = 0;
}

/*
 * tpro_report_event - note the occurance of an event
 *
 * This routine presently just remembers the report and logs it, but
 * does nothing heroic for the trap handler.
 */
static void
tpro_report_event(tpro, code)
	struct tprounit *tpro;
	int code;
{
	struct peer *peer;

	peer = tpro->peer;
	if (tpro->status != (u_char)code) {
		tpro->status = (u_char)code;
		if (code != CEVNT_NOMINAL)
			tpro->lastevent = (u_char)code;
		syslog(LOG_INFO,
		    "clock %s event %x", ntoa(&peer->srcadr), code);
	}
}


/*
 * tpro_receive - receive data from the TPRO device.
 *
 * Note: This interface would be interrupt-driven. We don't use that
 * now, but include a dummy routine for possible future adventures.
 */
static void
tpro_receive(rbufp)
	struct recvbuf *rbufp;
{
}

/*
 * tpro_poll - called by the transmit procedure
 */
static void
tpro_poll(unit, peer)
	int unit;
	struct peer *peer;
{
	struct tprounit *tpro;
	struct tproval *tptr;
	l_fp tstmp;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "tpro_poll: unit %d invalid", unit);
		return;
	}
	if (!unitinuse[unit]) {
		syslog(LOG_ERR, "tpro_poll: unit %d not in use", unit);
		return;
	}
	tpro = tprounits[unit];
	tpro->polls++;

	tptr = &tpro->tprodata;
	if (read(tpro->io.fd, (char *)tptr, sizeof(struct tproval)) < 0) {
		tpro_report_event(tpro, CEVNT_BADREPLY);
		return;
	}
	gettstamp(&tpro->lastrec);
	tpro->lasttime = current_time;

	/*
	 * Get TPRO time and convert to timestamp format. Note: we
	 * can't use the sec/usec conversion produced by the driver,
	 * since the year may be suspect.
	 */
	sprintf(tpro->lastcode,
	    "%1x%1x%1x %1x%1x:%1x%1x:%1x%1x.%1x%1x%1x%1x%1x%1x %1x",
	    tptr->day100, tptr->day10, tptr->day1, tptr->hour10, tptr->hour1,
	    tptr->min10, tptr->min1, tptr->sec10, tptr->sec1,
	    tptr->ms100, tptr->ms10, tptr->ms1, tptr->usec100, tptr->usec10,
	    tptr->usec1, tptr->status);
	record_clock_stats(&(tpro->peer->srcadr), tpro->lastcode);
	tpro->lencode = strlen(tpro->lastcode);

	tpro->day = MULBY10(MULBY10(tptr->day100) + tptr->day10) + tptr->day1;
	tpro->hour = MULBY10(tptr->hour10) + tptr->hour1;
	tpro->minute = MULBY10(tptr->min10) + tptr->min1;
	tpro->second = MULBY10(tptr->sec10) + tptr->sec1;
	tpro->usec = MULBY10(MULBY10(tptr->ms100) + tptr->ms10) + tptr->ms1;
	tpro->usec = tpro->usec * 10 + tptr->usec100;
	tpro->usec = tpro->usec * 10 + tptr->usec10;
	tpro->usec = tpro->usec * 10 + tptr->usec1;
#ifdef DEBUG
	if (debug)
		printf("tpro: %3d %02d:%02d:%02d.%06ld %1x\n",
		    tpro->day, tpro->hour, tpro->minute, tpro->second,
		    tpro->usec, tptr->status);
#endif
	if (tptr->status != 0xff) {
		tpro_report_event(tpro, CEVNT_BADREPLY);
                return;
	}

	/*
	 * Now, compute the reference time value. Use the heavy
	 * machinery for the seconds and the millisecond field for the
	 * fraction when present. If an error in conversion to internal
	 * format is found, the program declares bad data and exits.
	 * Note that this code does not yet know how to do the years and
	 * relies on the clock-calendar chip for sanity.
	 */
	if (!clocktime(tpro->day, tpro->hour, tpro->minute,
	    tpro->second, GMT, tpro->lastrec.l_ui,
	    &tpro->yearstart, &tpro->lastref.l_ui)) {
		tpro->baddata++;
		tpro_report_event(tpro, CEVNT_BADTIME);
		return;
	}
	TVUTOTSF(tpro->usec, tpro->lastref.l_uf);
	tstmp = tpro->lastref;
	L_SUB(&tstmp, &tpro->lastrec);
	tpro->coderecv++;
	L_ADD(&tstmp, &(fudgefactor[tpro->unit]));
	refclock_receive(tpro->peer, &tstmp, GMT, 0,
	    &tpro->lastrec, &tpro->lastrec, tpro->leap);
}

/*
 * tpro_control - set fudge factors, return statistics
 */
static void
tpro_control(unit, in, out)
	u_int unit;
	struct refclockstat *in;
	struct refclockstat *out;
{
	register struct tprounit *tpro;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "tpro_control: unit %d invalid)", unit);
		return;
	}

	if (in != 0) {
		if (in->haveflags & CLK_HAVETIME1)
			fudgefactor[unit] = in->fudgetime1;
		if (in->haveflags & CLK_HAVEVAL1) {
			stratumtouse[unit] = (u_char)(in->fudgeval1 & 0xf);
			if (unitinuse[unit]) {
				struct peer *peer;

				/*
				 * Should actually reselect clock, but
				 * will wait for the next timecode
				 */
				tpro = tprounits[unit];
				peer = tpro->peer;
				peer->stratum = stratumtouse[unit];
				if (stratumtouse[unit] <= 1)
					bcopy(TPROREFID, (char *)&peer->refid,
					    4);
				else
					peer->refid = htonl(TPROHSREFID);
			}
		}
		if (in->haveflags & CLK_HAVEFLAG1) {
			sloppyclockflag[unit] = in->flags & CLK_FLAG1;
		}
	}

	if (out != 0) {
		out->type = REFCLK_IRIG_TPRO;
		out->haveflags
		    = CLK_HAVETIME1|CLK_HAVEVAL1|CLK_HAVEVAL2|CLK_HAVEFLAG1;
		out->clockdesc = TPRODESCRIPTION;
		out->fudgetime1 = fudgefactor[unit];
		out->fudgetime2.l_ui = 0;
		out->fudgetime2.l_uf = 0;
		out->fudgeval1 = (LONG)stratumtouse[unit];
		out->fudgeval2 = 0;
		out->flags = sloppyclockflag[unit];
		if (unitinuse[unit]) {
			tpro = tprounits[unit];
			out->lencode = tpro->lencode;
			out->lastcode = tpro->lastcode;
			out->timereset = current_time - tpro->timestarted;
			out->polls = tpro->polls;
			out->noresponse = tpro->noreply;
			out->badformat = tpro->badformat;
			out->baddata = tpro->baddata;
			out->lastevent = tpro->lastevent;
			out->currentstatus = tpro->status;
		} else {
			out->lencode = 0;
			out->lastcode = "";
			out->polls = out->noresponse = 0;
			out->badformat = out->baddata = 0;
			out->timereset = 0;
			out->currentstatus = out->lastevent = CEVNT_NOMINAL;
		}
	}
}

/*
 * tpro_buginfo - return clock dependent debugging info
 */
static void
tpro_buginfo(unit, bug)
	int unit;
	register struct refclockbug *bug;
{
	register struct tprounit *tpro;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "tpro_buginfo: unit %d invalid)", unit);
		return;
	}

	if (!unitinuse[unit])
		return;
	tpro = tprounits[unit];

	bug->nvalues = 11;
	bug->ntimes = 5;
	if (tpro->lasttime != 0)
		bug->values[0] = current_time - tpro->lasttime;
	else
		bug->values[0] = 0;
	bug->values[2] = (U_LONG)tpro->year;
	bug->values[3] = (U_LONG)tpro->day;
	bug->values[4] = (U_LONG)tpro->hour;
	bug->values[5] = (U_LONG)tpro->minute;
	bug->values[6] = (U_LONG)tpro->second;
	bug->values[7] = (U_LONG)tpro->usec;
	bug->values[9] = tpro->yearstart;
	bug->stimes = 0x1c;
	bug->times[0] = tpro->lastref;
	bug->times[1] = tpro->lastrec;
}
#endif
