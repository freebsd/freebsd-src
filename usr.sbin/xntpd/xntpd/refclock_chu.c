/*
 * refclock_chu - clock driver for the CHU time code
 */
#if defined(REFCLOCK) && defined(CHU)

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_unixtime.h"
#include <sys/chudefs.h>
#include "ntp_stdlib.h"

/*
 * The CHU time signal includes a time code which is modulated at the
 * standard Bell 103 frequencies (i.e. mark=2225Hz, space=2025Hz).
 * and formatted into 8 bit characters with one start bit and two
 * stop bits. The time code is composed of 10 8-bit characters.
 * The second 5 bytes of the timecode are a redundancy check, and
 * are a copy of the first 5 bytes.
 *
 * It is assumed that you have built or modified a Bell 103 standard
 * modem, attached the input to the output of a radio and cabled the
 * output to a serial port on your computer, i.e. what you are receiving
 * is essentially the output of your radio.  It is also assumed you have
 * installed a special CHU line discipline to condition the output from
 * the terminal driver and take accurate time stamps.
 *
 * There are two types of timecodes. One is sent in the 32nd
 * through 39th second of the minute.
 *
 *     6dddhhmmss6dddhhmmss
 *
 * where ddd is the day of the year, hh is the hour (in UTC), mm is
 * the minute and ss the second.  The 6 is a constant.  Note that
 * the code is sent twice.
 *
 * The second sort of timecode is sent only during the 31st second
 * past the minute.
 *
 *     xdyyyyttabXDYYYYTTAB
 *
 * In this case, the second part of the code is the one's complement
 * of the code. This differentiates it from the other timecode
 * format.
 *
 * d is the absolute value of DUT (in tenths of a second). yyyy
 * is the year. tt is the difference between UTC and TAI. a is
 * a canadian daylight time flag and b is a serial number.
 * x is a bitwise field. The least significant bit of x is
 * one if DUT is negative. The 2nd bit is set if a leap second
 * will be added at the next opportunity. The 3rd bit is set if
 * a leap second will be deleted at the next opportunity.
 * The 4th bit is an even parity bit for the other three bits
 * in this nibble.
 *
 * The start bit in each character has a precise relationship to
 * the on-time second. Most often UART's synchronize themselves to the
 * start bit and will post an interrupt at the center of the first stop
 * bit.  Thus each character's interrupt should occur at a fixed offset
 * from the on-time second. This means that a timestamp taken at the
 * arrival of each character in the code will provide an independent
 * estimate of the offset.  Since there are 10 characters in the time
 * code and the code is sent 9 times per minute, this means you
 * potentially get 90 offset samples per minute. Much of the code in
 * here is dedicated to producing a single offset estimate from these
 * samples.
 *
 * A note about the line discipline. It is possible to receive the
 * CHU time code in raw mode, but this has disadvantages. In particular,
 * this puts a lot of code between the interrupt and the time you freeze
 * a time stamp, decreasing precision. It is also expensive in terms of
 * context switches, and made even more expensive by the way I do I/O.
 * Worse, since you are listening directly to the output of your radio,
 * CHU is noisy and will make you spend a lot of time receiving noise.
 *
 * The line discipline fixes a lot of this. It knows that the CHU time
 * code consists of 10 bytes which arrive with an intercharacter
 * spacing of about 37 ms, and that the data is BCD, and filters on this
 * basis. It delivers block of ten characters plus their associated time
 * stamps all at once. The time stamps are hence about as accurate as
 * a Unix machine can get them, and much of the noise disappears in the
 * kernel with no context switching cost.
 *
 * The kernel module also will insure that the packets that are
 * delivered have the correct redundancy bytes, and will return
 * a flag in chutype to differentiate one sort of packet from
 * the other.
 */

/*
 * CHU definitions
 */
#define	DEVICE		"/dev/chu%d" /* device name and unit */
#define SPEED232 	B300	/* uart speed (300 baud) */
#define	PRECISION	(-9)	/* what the heck */
#define	REFID		"CHU\0"	/* reference ID */
#define	DESCRIPTION	"Scratchbuilt CHU Receiver" /* WRU */

#define	NCHUCODES	8	/* expect 8 CHU codes per minute */
#ifndef CHULDISC
#define CHULDISC	10	/* XXX temp CHU line discipline */
#endif

/*
 * To compute a quality for the estimate (a pseudo dispersion) we add a
 * fixed 10 ms for each missing code in the minute and add to this
 * the sum of the differences between the remaining offsets and the
 * estimated sample offset.
 */
#define	CHUDELAYPENALTY	0x0000028f

/*
 * Default fudge factors
 */
#define	DEFPROPDELAY	0x00624dd3	/* 0.0015 seconds, 1.5 ms */
#define	DEFFILTFUDGE	0x000d1b71	/* 0.0002 seconds, 200 us */

/*
 * Hacks to avoid excercising the multiplier.  I have no pride.
 */
#define	MULBY10(x)	(((x)<<3) + ((x)<<1))
#define	MULBY60(x)	(((x)<<6) - ((x)<<2))	/* watch overflow */
#define	MULBY24(x)	(((x)<<4) + ((x)<<3))

/*
 * Constants for use when multiplying by 0.1.  ZEROPTONE is 0.1
 * as an l_fp fraction, NZPOBITS is the number of significant bits
 * in ZEROPTONE.
 */
#define	ZEROPTONE	0x1999999a
#define	NZPOBITS	29

static char hexstring[]="0123456789abcdef";

/*
 * Unit control structure.
 */
struct chuunit {
	struct	peer *peer;	/* peer structure pointer */
	struct	event chutimer;	/* timeout timer structure */
	l_fp	offsets[NCHUCODES]; /* offsets computed from each code */
	l_fp	rectimes[NCHUCODES]; /* times we received this stuff */
	u_long	reftimes[NCHUCODES]; /* time of last code received */
	u_char	lastcode[NCHUCHARS * 4]; /* last code we received */
	u_char	expect;		/* the next offset expected */
	u_short	haveoffset;	/* flag word indicating valid offsets */
	u_short	flags;		/* operational flags */
	u_long	responses;	/* number of responses */
	int	pollcnt;	/* poll message counter */
};

#define	CHUTIMERSET	0x1	/* timer is set to fire */


/*
 * The CHU table. This gives the expected time of arrival of each
 * character after the on-time second and is computed as follows:
 * The CHU time code is sent at 300 bps.  Your average UART will
 * synchronize at the edge of the start bit and will consider the
 * character complete at the middle of the first stop bit, i.e.
 * 0.031667 ms later (some UARTS may complete the character at the
 * end of the stop bit instead of the middle, but you can fudge this).
 * Thus the expected time of each interrupt is the start bit time plus
 * 0.031667 seconds. These times are in chutable[].
 */
#define	CHARDELAY	0x081b4e82

static u_long chutable[NCHUCHARS] = {
	0x22222222 + CHARDELAY,		/* 0.1333333333 */
	0x2b851eb8 + CHARDELAY,		/* 0.170 (exactly) */
	0x34e81b4e + CHARDELAY,		/* 0.2066666667 */
	0x3f92c5f9 + CHARDELAY,		/* 0.2483333333 */
	0x47ae147b + CHARDELAY,		/* 0.280 (exactly) */
	0x51111111 + CHARDELAY,		/* 0.3166666667 */
	0x5a740da7 + CHARDELAY,		/* 0.3533333333 */
	0x63d70a3d + CHARDELAY,		/* 0.390 (exactly) */
	0x6d3a06d4 + CHARDELAY,		/* 0.4266666667 */
	0x769d0370 + CHARDELAY,		/* 0.4633333333 */
};

/*
 * Imported from the timer module
 */
extern u_long current_time;
extern struct event timerqueue[];

/*
 * Imported from ntpd module
 */
extern int debug;		/* global debug flag */

/*
 * Function prototypes
 */
static	int	chu_start	P((int, struct peer *));
static	void	chu_shutdown	P((int, struct peer *));
static	void	chu_receive	P((struct recvbuf *));
static	void	chu_process	P((struct chuunit *));
static	void	chu_poll	P((int, struct peer *));
static	void	chu_timeout	P((struct peer *));

/*
 * Transfer vector
 */
struct	refclock refclock_chu = {
	chu_start,		/* start up driver */
	chu_shutdown,		/* shut down driver */
	chu_poll,		/* transmit poll message */
	noentry,		/* not used (old chu_control) */
	noentry,		/* initialize driver (not used) */
	noentry,		/* not used (old chu_buginfo) */
	NOFLAGS			/* not used */
};


/*
 * chu_start - open the CHU device and initialize data for processing
 */
static int
chu_start(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct chuunit *up;
	struct refclockproc *pp;
	int fd;
	char device[20];

	/*
	 * Open serial port and set CHU line discipline
	 */
	(void) sprintf(device, DEVICE, unit);
	if (!(fd = refclock_open(device, SPEED232, LDISC_CHU)))
		return (0);

	/*
	 * Allocate and initialize unit structure
	 */
	if (!(up = (struct chuunit *)
	    emalloc(sizeof(struct chuunit)))) {
		(void) close(fd);
		return (0);
	}
	memset((char *)up, 0, sizeof(struct chuunit));
	up->chutimer.peer = (struct peer *)up;
	up->chutimer.event_handler = chu_timeout;
	up->peer = peer;
	pp = peer->procptr;
	pp->io.clock_recv = chu_receive;
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
	return (1);
}


/*
 * chu_shutdown - shut down the clock
 */
static void
chu_shutdown(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct chuunit *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct chuunit *)pp->unitptr;
	io_closeclock(&pp->io);
	free(up);
}


/*
 * chu_receive - receive data from a CHU clock, do format checks and compute
 *		 an estimate from the sample data
 */
static void
chu_receive(rbufp)
	struct recvbuf *rbufp;
{
	register struct chuunit *up;
	struct refclockproc *pp;
	struct peer *peer;
	int i;
	u_long date_ui;
	u_long tmp;
	u_char *code;
	struct chucode *chuc;
	int isneg;
	u_long reftime;
	l_fp off[NCHUCHARS];
	int day, hour, minute, second;

	/*
	 * Do a length check on the data.  Should be what we asked for.
	 */
	if (rbufp->recv_length != sizeof(struct chucode)) {
		syslog(LOG_ERR,
		    "chu_receive: received %d bytes, expected %d",
		    rbufp->recv_length, sizeof(struct chucode));
		return;
	}

	/*
	 * Get the clock this applies to and a pointer to the data
	 */
	peer = (struct peer *)rbufp->recv_srcclock;
	pp = peer->procptr;
	up = (struct chuunit *)pp->unitptr;
	chuc = (struct chucode *)&rbufp->recv_space;
	up->responses++;

	/*
	 * Just for fun, we can debug the whole frame if
	 * we want.
	 */
	for (i = 0; i < NCHUCHARS; i++) {
		pp->lastcode[2 * i] = hexstring[chuc->codechars[i] &
		    0xf];
		pp->lastcode[2 * i + 1] = hexstring[chuc->codechars[i]
		    >> 4];
	}
	pp->lencode = 2 * i;
	pp->lastcode[pp->lencode] = '\0';
#ifdef DEBUG
	if (debug > 3) {
		printf("chu: %s packet\n", (chuc->chutype == CHU_YEAR)?
		    "year":"time");
		for (i = 0; i < NCHUCHARS; i++) {
			char c[64];

			sprintf(c,"%c%c %s",
			    hexstring[chuc->codechars[i] & 0xf],
			    hexstring[chuc->codechars[i] >> 4],
			    ctime(&(chuc->codetimes[i].tv_sec)));
			c[strlen(c) - 1] = 0;	/* ctime() adds \n */
			printf("chu: %s .%06d\n", c,
			    chuc->codetimes[i].tv_usec);
		}
	}
#endif

	/*
	 * At this point we're assured that both halves of the
	 * data match because of what the kernel has done.
	 * But there's more than one data format. We need to
	 * check chutype to see what to do now. If it's a
	 * year packet, then we fiddle with it specially.
	 */

	if (chuc->chutype == CHU_YEAR)
	{
		u_char leapbits,parity;

		/*
	 	 * Break out the code into the BCD nibbles.
		 * Put it in the half of lastcode.
		 */
		code = up->lastcode;
		code += 2*NCHUCHARS;
		for (i = 0; i < NCHUCHARS; i++) {
			*code++ = chuc->codechars[i] & 0xf;
			*code++ = (chuc->codechars[i] >> 4) & 0xf;
		}

		leapbits = chuc->codechars[0]&0xf;

		/*
		 * Now make sure that the leap nibble
		 * is even parity.
		 */

		parity = (leapbits ^ (leapbits >> 2))&0x3;
		parity = (parity ^ (parity>>1))&0x1;
		if (parity)
		{
			refclock_report(peer, CEVNT_BADREPLY);
			return;
		}

		/*
		 * This just happens to work. :-)
		 */

		pp->leap = (leapbits >> 1) & 0x3;

		return;
	}

	if (chuc->chutype != CHU_TIME)
	{
		refclock_report(peer, CEVNT_BADREPLY);
		return;
	}

	/*
	 * Break out the code into the BCD nibbles.  Only need to fiddle
	 * with the first half since both are identical.  Note the first
	 * BCD character is the low order nibble, the second the high order.
	 */
	code = up->lastcode;
	for (i = 0; i < NCHUCHARS; i++) {
		*code++ = chuc->codechars[i] & 0xf;
		*code++ = (chuc->codechars[i] >> 4) & 0xf;
	}

	/*
	 * Format check.  Make sure the two halves match.
	 * There's really no need for this, but it can't hurt.
	 */
	for (i = 0; i < NCHUCHARS/2; i++)
		if (chuc->codechars[i] !=
		    chuc->codechars[i+(NCHUCHARS/2)]) {
			refclock_report(peer, CEVNT_BADREPLY);
			return;
		}

	/*
	 * If the first nibble isn't a 6, we're up the creek
	 */
	code = up->lastcode;
	if (*code++ != 6) {
		refclock_report(peer, CEVNT_BADREPLY);
		return;
	}

	/*
	 * Collect the day, the hour, the minute and the second.
	 */
	day = *code++;
	day = MULBY10(day) + *code++;
	day = MULBY10(day) + *code++;
	hour = *code++;
	hour = MULBY10(hour) + *code++;
	minute = *code++;
	minute = MULBY10(minute) + *code++;
	second = *code++;
	second = MULBY10(second) + *code++;

	/*
	 * Sanity check the day and time.  Note that this
	 * only occurs on the 32st through the 39th second
	 * of the minute.
	 */
	if (day < 1 || day > 366
	    || hour > 23 || minute > 59
	    || second < 32 || second > 39) {
		pp->baddata++;
		if (day < 1 || day > 366) {
			refclock_report(peer, CEVNT_BADDATE);
		} else {
			refclock_report(peer, CEVNT_BADTIME);
		}
		return;
	}

	/*
	 * Compute the NTP date from the input data and the
	 * receive timestamp.  If this doesn't work, mark the
	 * date as bad and forget it.
	 */
	if (!clocktime(day, hour, minute, second, 0,
	    rbufp->recv_time.l_ui, &pp->yearstart, (U_LONG *)&reftime)) {
		refclock_report(peer, CEVNT_BADDATE);
		return;
	}
	date_ui = reftime;;

	/*
	 * We've now got the integral seconds part of the time code (we hope).
	 * The fractional part comes from the table.  We next compute
	 * the offsets for each character.
	 */
	for (i = 0; i < NCHUCHARS; i++) {
		register u_long tmp2;

		off[i].l_ui = date_ui;
		off[i].l_uf = chutable[i];
		tmp = chuc->codetimes[i].tv_sec + JAN_1970;
		TVUTOTSF(chuc->codetimes[i].tv_usec, tmp2);
		M_SUB(off[i].l_ui, off[i].l_uf, tmp, tmp2);
	}

	if (!pp->sloppyclockflag) {
		u_short ord[NCHUCHARS];
		/*
		 * In here we assume the clock has adequate bits
		 * to take timestamps with reasonable accuracy.
		 * Note that the time stamps may contain errors
		 * for a couple of reasons.  Timing is actually
		 * referenced to the start bit in each character
		 * in the time code.  If this is obscured by static
		 * you can still get a valid character but have the
		 * timestamp offset by +-1.5 ms.  Also, we may suffer
		 * from interrupt delays if the interrupt is being
		 * held off when the character arrives.  Note the
		 * latter error is always in the form of a delay.
		 *
		 * After fiddling I arrived at the following scheme.
		 * We sort the times into order by offset.  We then
		 * drop the most positive 2 offset values (which may
		 * correspond to a character arriving early due to
		 * static) and the most negative 4 (which may correspond
		 * to delayed characters, either from static or from
		 * interrupt latency).  We then take the mean of the
		 * remaining 4 offsets as our estimate.
		 */
		
		/*
		 * Set up the order array.
		 */
		for (i = 0; i < NCHUCHARS; i++)
			ord[i] = (u_short)i;
		
		/*
		 * Sort them into order.  Reuse variables with abandon.
		 */
		for (tmp = 0; tmp < (NCHUCHARS-1); tmp++) {
			for (i = (int)tmp+1; i < NCHUCHARS; i++) {
				if (!L_ISGEQ(&off[ord[i]], &off[ord[tmp]])) {
					date_ui = (u_long)ord[i];
					ord[i] = ord[tmp];
					ord[tmp] = (u_short)date_ui;
				}
			}
		}

		/*
		 * Done the sort.  We drop 0, 1, 2 and 3 at the negative
		 * end, and 8 and 9 at the positive.  Take the sum of
		 * 4, 5, 6 and 7.
		 */
		date_ui = off[ord[4]].l_ui;
		tmp = off[ord[4]].l_uf;
		for (i = 5; i <= 7; i++)
			M_ADD(date_ui, tmp, off[ord[i]].l_ui, off[ord[i]].l_uf);
		
		/*
		 * Round properly, then right shift two bits for the
		 * divide by four.
		 */
		if (tmp & 0x2)
			M_ADDUF(date_ui, tmp, 0x4);
		M_RSHIFT(date_ui, tmp);
		M_RSHIFT(date_ui, tmp);
	} else {
		/*
		 * Here is a *big* problem.  On a machine where the
		 * low order bit in the clock is on the order of half
		 * a millisecond or more we don't really have enough
		 * precision to make intelligent choices about which
		 * samples might be in error and which aren't.  More
		 * than this, in the case of error free data we can
		 * pick up a few bits of precision by taking the mean
		 * of the whole bunch.  This is what we do.  The problem
		 * comes when it comes time to divide the 64 bit sum of
		 * the 10 samples by 10, a procedure which really sucks.
		 * Oh, well, grin and bear it.  Compute the sum first.
		 */
		date_ui = 0;
		tmp = 0;
		for (i = 0; i < NCHUCHARS; i++)
			M_ADD(date_ui, tmp, off[i].l_ui, off[i].l_uf);
		if (M_ISNEG(date_ui, tmp))
			isneg = 1;
		else
			isneg = 0;

		/*
		 * Here is a multiply-by-0.1 optimization that should apply
		 * just about everywhere.  If the magnitude of the sum
		 * is less than 9 we don't have to worry about overflow
		 * out of a 64 bit product, even after rounding.
		 */
		if (date_ui < 9 || date_ui > 0xfffffff7) {
			register u_long prod_ui;
			register u_long prod_uf;
	
			prod_ui = prod_uf = 0;
			/*
			 * This code knows the low order bit in 0.1 is zero
			 */
			for (i = 1; i < NZPOBITS; i++) {
				M_LSHIFT(date_ui, tmp);
				if (ZEROPTONE & (1<<i))
					M_ADD(prod_ui, prod_uf, date_ui, tmp);
			}

			/*
			 * Done, round it correctly.  Prod_ui contains the
			 * fraction.
			 */
			if (prod_uf & 0x80000000)
				prod_ui++;
			if (isneg)
				date_ui = 0xffffffff;
			else
				date_ui = 0;
			tmp = prod_ui;
			/*
			 * date_ui is integral part, tmp is fraction.
			 */
		} else {
			register u_long prod_ovr;
			register u_long prod_ui;
			register u_long prod_uf;
			register u_long highbits;

			prod_ovr = prod_ui = prod_uf = 0;
			if (isneg)
				highbits = 0xffffffff;	/* sign extend */
			else
				highbits = 0;
			/*
			 * This code knows the low order bit in 0.1 is zero
			 */
			for (i = 1; i < NZPOBITS; i++) {
				M_LSHIFT3(highbits, date_ui, tmp);
				if (ZEROPTONE & (1<<i))
					M_ADD3(prod_ovr, prod_uf, prod_ui,
					    highbits, date_ui, tmp);
			}

			if (prod_uf & 0x80000000)
				M_ADDUF(prod_ovr, prod_ui, (u_long)1);
			date_ui = prod_ovr;
			tmp = prod_ui;
		}
	}
	
	/*
	 * At this point we have the mean offset, with the integral
	 * part in date_ui and the fractional part in tmp.  Store
	 * it in the structure.
	 */
	i = second - 32;	/* gives a value 0 through 8 */
	if (i < (int)up->expect) {
		/*
		 * This shouldn't actually happen, but might if a single
		 * bit error occurred in the code which fooled us.
		 * Throw away all previous data.
		 */
		up->expect = 0;
		up->haveoffset = 0;
		if (up->flags & CHUTIMERSET) {
			TIMER_DEQUEUE(&up->chutimer);
			up->flags &= ~CHUTIMERSET;
		}
	}

	up->offsets[i].l_ui = date_ui;
	up->offsets[i].l_uf = tmp;
	up->rectimes[i] = rbufp->recv_time;
	up->reftimes[i] = reftime;

	up->expect = i + 1;
	up->haveoffset |= (1 << i);

	if (up->expect >= NCHUCODES) {
		/*
		 * Got a full second's worth.  Dequeue timer and
		 * process this.
		 */
		if (up->flags & CHUTIMERSET) {
			TIMER_DEQUEUE(&up->chutimer);
			up->flags &= ~CHUTIMERSET;
		}
		chu_process(up);
	} else if (!(up->flags & CHUTIMERSET)) {
		/*
		 * Try to take an interrupt sometime after the
		 * 42 second mark (leaves an extra 2 seconds for
		 * slop).  Round it up to an even multiple of
		 * 4 seconds.
		 */
		up->chutimer.event_time =
		    current_time + (u_long)(10 - i) + (1<<EVENT_TIMEOUT);
		up->chutimer.event_time &= ~((1<<EVENT_TIMEOUT) - 1);
		TIMER_INSERT(timerqueue, &up->chutimer);
		up->flags |= CHUTIMERSET;
	}
}


/*
 * chu_timeout - process a timeout event
 */
static void
chu_timeout(fakepeer)
	struct peer *fakepeer;
{
	/*
	 * If we got here it means we received some time codes
	 * but didn't get the one which should have arrived on
	 * the 39th second.  Process what we have.
	 */
	((struct chuunit *)fakepeer)->flags &= ~CHUTIMERSET;
	chu_process((struct chuunit *)fakepeer);
}


/*
 * chu_process - process the raw offset estimates we have and pass
 *		 the results on to the NTP clock filters.
 */
static void
chu_process(up)
	register struct chuunit *up;
{
	struct peer *peer;
	struct refclockproc *pp;
	int i;
	s_fp bestoff;
	s_fp tmpoff;
	u_fp dispersion;
	int imax;

	/*
	 * The most positive offset.
	 */
	peer = up->peer;
	pp = peer->procptr;
	imax = NCHUCODES;
	for (i = 0; i < NCHUCODES; i++)
		if (up->haveoffset & (1<<i))
			if (i < imax || L_ISGEQ(&up->offsets[i],
			    &up->offsets[imax]))
				imax = i;

	/*
	 * The most positive estimate is our best bet.  Go through
	 * the list again computing the dispersion.
	 */
	bestoff = LFPTOFP(&up->offsets[imax]);
	dispersion = 0;
	for (i = 0; i < NCHUCODES; i++) {
		if (up->haveoffset & (1<<i)) {
			tmpoff = LFPTOFP(&up->offsets[i]);
			dispersion += (bestoff - tmpoff);
		} else {
			dispersion += CHUDELAYPENALTY;
		}
	}

	pp->lasttime = current_time;
	up->pollcnt = 2;
	record_clock_stats(&peer->srcadr, pp->lastcode);
	refclock_receive(peer, &up->offsets[imax], 0,
	    dispersion, &up->rectimes[imax], &up->rectimes[imax],
	    pp->leap);
	
	/*
	 * Zero out unit for next code series
	 */
	up->haveoffset = 0;
	up->expect = 0;
	refclock_report(peer, CEVNT_NOMINAL);
}


/*
 * chu_poll - called by the transmit procedure
 */
static void
chu_poll(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct chuunit *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct chuunit *)pp->unitptr;
	if (up->pollcnt == 0)
		refclock_report(peer, CEVNT_TIMEOUT);
	else
		up->pollcnt--;
}

#endif
