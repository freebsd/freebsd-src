/*
 * refclock_leitch - clock driver for the Leitch CSD-5300 Master Clock
 */
#if defined(REFCLOCK) && (defined(LEITCH) || defined(LEITCHCLK) || defined(LEITCHPPS))

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_unixtime.h"

#if defined(HAVE_BSD_TTYS)
#include <sgtty.h>
#endif /* HAVE_BSD_TTYS */

#if defined(HAVE_SYSV_TTYS)
#include <termio.h>
#endif /* HAVE_SYSV_TTYS */

#if defined(HAVE_TERMIOS)
#include <termios.h>
#endif
#ifdef STREAM
#include <stropts.h>
#if defined(LEITCHCLK)
#include <sys/clkdefs.h>
#endif /* LEITCHCLK */
#endif /* STREAM */

#if defined (LEITCHPPS)
#include <sys/ppsclock.h>
#endif /* LEITCHPPS */

#include "ntp_stdlib.h"

/*
 * Driver for Leitch CSD-5300 Master Clock System
 *
 * COMMANDS:
 *	DATE:	D <CR>
 *	TIME:	T <CR>
 *	STATUS:	S <CR>
 *	LOOP:	L <CR>
 *
 * FORMAT:
 *	DATE: YYMMDD<CR>
 *	TIME: <CR>/HHMMSS <CR>/HHMMSS <CR>/HHMMSS <CR>/
 *		second bondaried on the stop bit of the <CR>
 *		second boundaries at '/' above.
 *	STATUS: G (good), D (diag fail), T (time not provided) or
 *		P (last phone update failed)
 */
#define MAXUNITS 1		/* max number of LEITCH units */
#define LEITCH_DESCRIPTION "Leitch: CSD 5300 Master Clock System Driver"
#define LEITCH232 "/dev/leitch%d"	/* name of radio device */
#define SPEED232 B300		/* uart speed (300 baud) */ 
#define leitch_send(A,M) \
	if (debug) fprintf(stderr,"write leitch %s\n",M); \
	if ((write(A->leitchio.fd,M,sizeof(M)) < 0)) {\
		if (debug) \
			fprintf(stderr, "leitch_send: unit %d send failed\n", A->unit); \
		else \
			syslog(LOG_ERR, "leitch_send: unit %d send failed %m",A->unit);}
		
#define STATE_IDLE 0
#define STATE_DATE 1
#define STATE_TIME1 2
#define STATE_TIME2 3
#define STATE_TIME3 4

extern struct event timerqueue[];

/*
 * Imported from ntp_loopfilter module
 */
extern int fdpps;		/* pps file descriptor */

/*
 * Imported from ntpd module
 */
extern int debug;		/* global debug flag */

/*
 * LEITCH unit control structure
 */
struct leitchunit {
	struct peer *peer;
	struct event leitchtimer;
	struct refclockio leitchio;
	u_char unit;
	short year;
	short yearday;
	short month;
	short day;
	short hour;
	short second;
	short minute;
	short state;
	u_short fudge1;
	l_fp reftime1;
	l_fp reftime2;
	l_fp reftime3;
	l_fp codetime1;
	l_fp codetime2;
	l_fp codetime3;
	U_LONG yearstart;
};

/*
 * Function prototypes
 */
static	void	leitch_init	P((void));
static	int	leitch_start	P((u_int, struct peer *));
static	void	leitch_shutdown	P((int));
static	void	leitch_poll	P((int, struct peer *));
static	void	leitch_control	P((u_int, struct refclockstat *, struct refclockstat *));
#define	leitch_buginfo	noentry
static	void	leitch_receive	P((struct recvbuf *));
static	void	leitch_process	P((struct leitchunit *));
static	void	leitch_timeout	P((struct peer *));
static	int	leitch_get_date	P((struct recvbuf *, struct leitchunit *));
static	int	leitch_get_time	P((struct recvbuf *, struct leitchunit *, int));
static	int	dysize		P((int));

static struct leitchunit leitchunits[MAXUNITS];
static u_char unitinuse[MAXUNITS];

static	char days_in_month [] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

/*
 * Transfer vector
 */
struct	refclock refclock_leitch = {
	leitch_start, leitch_shutdown, leitch_poll,
	leitch_control, leitch_init, leitch_buginfo, NOFLAGS
};

/*
 * leitch_init - initialize internal leitch driver data
 */
static void
leitch_init()
{
	memset((char*)leitchunits, 0, sizeof(leitchunits));
	memset((char*)unitinuse, 0, sizeof(unitinuse));
}

/*
 * leitch_shutdown - shut down a LEITCH clock
 */
static void
leitch_shutdown(unit)
int unit;
{
#ifdef DEBUG
	if (debug)
		fprintf(stderr, "leitch_shutdown()\n");
#endif
}

/*
 * leitch_poll - called by the transmit procedure
 */
static void
leitch_poll(unit, peer)
        int unit;
        struct peer *peer;
{
	struct leitchunit *leitch;

	/* start the state machine rolling */

#ifdef DEBUG
	if (debug)
		fprintf(stderr, "leitch_poll()\n");
#endif
	if (unit > MAXUNITS) {
		/* XXXX syslog it */
		return;
	}

	leitch = &leitchunits[unit];

	if (leitch->state != STATE_IDLE) {
		/* reset and wait for next poll */
		/* XXXX syslog it */
		leitch->state = STATE_IDLE;
	} else {
		leitch_send(leitch,"D\r");
		leitch->state = STATE_DATE;
	}
}

static void
leitch_control(unit, in, out)
	u_int unit;
	struct refclockstat *in;
	struct refclockstat *out;
{
#if debug
	if (debug)
	    fprintf(stderr, "leitch_control()\n");
#endif
	if (unit >= MAXUNITS) {
		syslog(LOG_ERR,
		    "leitch_control: unit %d invalid", unit);
		return;
	}
	if (in) {
		/* WE DONT SET ANY THING */
	}
	if (out) {
		out->type = REFCLK_ATOM_LEITCH;
		out->flags = 0;
		out->haveflags = 0;
		out->lencode = 0;
		out->lastcode = "";
		out->polls = 0;
		out->noresponse = 0;
		out->badformat = 0;
		out->baddata = 0;
		out->timereset = 0;
		out->clockdesc = LEITCH_DESCRIPTION;
		out->fudgetime1.l_uf = 0;
		out->fudgetime1.l_ui = 0;
		out->fudgetime2.l_uf = 0;
		out->fudgetime2.l_ui = 0;
		out->currentstatus = 0;
		out->lastevent = 0;
	}
}

/*
 * leitch_start - open the LEITCH devices and initialize data for processing
 */
static int
leitch_start(unit, peer)
	u_int unit;
	struct peer *peer;
{
	struct leitchunit *leitch;
	int fd232;
	char leitchdev[20];

	/*
	 * Check configuration info.
	 */
	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "leitch_start: unit %d invalid", unit);
		return (0);
	}

	if (unitinuse[unit]) {
		syslog(LOG_ERR, "leitch_start: unit %d in use", unit);
		return (0);
	}

	/*
	 * Open serial port.
	 */
	(void) sprintf(leitchdev, LEITCH232, unit);
	fd232 = open(leitchdev, O_RDWR, 0777);
	if (fd232 == -1) {
		syslog(LOG_ERR,
		    "leitch_start: open of %s: %m", leitchdev);
		return (0);
	}

	leitch = &leitchunits[unit];
	memset((char*)leitch, 0, sizeof(*leitch));

#if defined(HAVE_SYSV_TTYS)
	/*
	 * System V serial line parameters (termio interface)
	 *
	 */
    {	struct termio ttyb;
	if (ioctl(fd232, TCGETA, &ttyb) < 0) {
                syslog(LOG_ERR,
		    "leitch_start: ioctl(%s, TCGETA): %m", leitchdev);
                goto screwed;
        }
        ttyb.c_iflag = IGNBRK|IGNPAR|ICRNL;
        ttyb.c_oflag = 0;
        ttyb.c_cflag = SPEED232|CS8|CLOCAL|CREAD;
        ttyb.c_lflag = ICANON;
	ttyb.c_cc[VERASE] = ttyb.c_cc[VKILL] = '\0';
        if (ioctl(fd232, TCSETA, &ttyb) < 0) {
                syslog(LOG_ERR,
		    "leitch_start: ioctl(%s, TCSETA): %m", leitchdev);
                goto screwed;
        }
    }
#endif /* HAVE_SYSV_TTYS */
#if defined(HAVE_TERMIOS)
	/*
	 * POSIX serial line parameters (termios interface)
	 *
	 * The LEITCHCLK option provides timestamping at the driver level. 
	 * It requires the tty_clk streams module.
	 *
	 * The LEITCHPPS option provides timestamping at the driver level.
	 * It uses a 1-pps signal and level converter (gadget box) and
	 * requires the ppsclock streams module and SunOS 4.1.1 or
	 * later.
	 */
    {	struct termios ttyb, *ttyp;

	ttyp = &ttyb;
	if (tcgetattr(fd232, ttyp) < 0) {
                syslog(LOG_ERR,
		    "leitch_start: tcgetattr(%s): %m", leitchdev);
                goto screwed;
        }
        ttyp->c_iflag = IGNBRK|IGNPAR|ICRNL;
        ttyp->c_oflag = 0;
        ttyp->c_cflag = SPEED232|CS8|CLOCAL|CREAD;
        ttyp->c_lflag = ICANON;
	ttyp->c_cc[VERASE] = ttyp->c_cc[VKILL] = '\0';
        if (tcsetattr(fd232, TCSANOW, ttyp) < 0) {
                syslog(LOG_ERR,
		    "leitch_start: tcsetattr(%s): %m", leitchdev);
                goto screwed;
        }
        if (tcflush(fd232, TCIOFLUSH) < 0) {
                syslog(LOG_ERR,
		    "leitch_start: tcflush(%s): %m", leitchdev);
                goto screwed;
        }
    }
#endif /* HAVE_TERMIOS */
#ifdef STREAM
#if defined(LEITCHCLK)
    if (ioctl(fd232, I_PUSH, "clk") < 0)
	    syslog(LOG_ERR,
		"leitch_start: ioctl(%s, I_PUSH, clk): %m", leitchdev);
    if (ioctl(fd232, CLK_SETSTR, "\n") < 0)
	    syslog(LOG_ERR,
		"leitch_start: ioctl(%s, CLK_SETSTR): %m", leitchdev);
#endif /* LEITCHCLK */
#if defined(LEITCHPPS)
    if (ioctl(fd232, I_PUSH, "ppsclock") < 0)
	    syslog(LOG_ERR,
		"leitch_start: ioctl(%s, I_PUSH, ppsclock): %m", leitchdev);
    else
	    fdpps = fd232;
#endif /* LEITCHPPS */
#endif /* STREAM */
#if defined(HAVE_BSD_TTYS)
	/*
	 * 4.3bsd serial line parameters (sgttyb interface)
	 *
	 * The LEITCHCLK option provides timestamping at the driver level. 
	 * It requires the tty_clk line discipline and 4.3bsd or later.
	 */
    {	struct sgttyb ttyb;
#if defined(LEITCHCLK)
	int ldisc = CLKLDISC;
#endif /* LEITCHCLK */

	if (ioctl(fd232, TIOCGETP, &ttyb) < 0) {
		syslog(LOG_ERR,
		    "leitch_start: ioctl(%s, TIOCGETP): %m", leitchdev);
		goto screwed;
	}
	ttyb.sg_ispeed = ttyb.sg_ospeed = SPEED232;
#if defined(LEITCHCLK)
	ttyb.sg_erase = ttyb.sg_kill = '\r';
	ttyb.sg_flags = RAW;
#else
	ttyb.sg_erase = ttyb.sg_kill = '\0';
	ttyb.sg_flags = EVENP|ODDP|CRMOD;
#endif /* LEITCHCLK */
	if (ioctl(fd232, TIOCSETP, &ttyb) < 0) {
		syslog(LOG_ERR,
		    "leitch_start: ioctl(%s, TIOCSETP): %m", leitchdev);
		goto screwed;
	}
#if defined(LEITCHCLK)
	if (ioctl(fd232, TIOCSETD, &ldisc) < 0) {
		syslog(LOG_ERR,
		    "leitch_start: ioctl(%s, TIOCSETD): %m",leitchdev);
		goto screwed;
	}
#endif /* LEITCHCLK */
    }
#endif /* HAVE_BSD_TTYS */

	/*
	 * Set up the structures
	 */
	leitch->peer = peer;
	leitch->unit = unit;
	leitch->state = STATE_IDLE;
	leitch->fudge1 = 15;	/* 15ms */

	leitch->leitchio.clock_recv = leitch_receive;
	leitch->leitchio.srcclock = (caddr_t) leitch;
	leitch->leitchio.datalen = 0;
	leitch->leitchio.fd = fd232;
	if (!io_addclock(&leitch->leitchio)) {
		goto screwed;
	}

	/*
	 * All done.  Initialize a few random peer variables, then
	 * return success. Note that root delay and root dispersion are
	 * always zero for this clock.
	 */
	peer->precision = 0;
	peer->rootdelay = 0;
	peer->rootdispersion = 0;
	peer->stratum = 0;
	peer->refid = htonl(0x41544f4d); /* ATOM */
	unitinuse[unit] = 1;
	return(1);

	/*
	 * Something broke; abandon ship.
	 */
screwed:
	close(fd232);
	return(0);
}

/*
 * leitch_receive - receive data from the serial interface on a leitch
 * clock
 */
static void
leitch_receive(rbufp)
	struct recvbuf *rbufp;
{
	struct leitchunit *leitch = (struct leitchunit *)rbufp->recv_srcclock;

#ifdef DEBUG
	if (debug)
		fprintf(stderr, "leitch_recieve(%*.*s)\n", 
		    rbufp->recv_length, rbufp->recv_length,
		    rbufp->recv_buffer);
#endif
	if (rbufp->recv_length != 7)
		return; /* The date is return with a trailing newline,
				discard it. */

	switch (leitch->state) {
	case STATE_IDLE:	/* unexpected, discard and resync */
		return;
	case STATE_DATE:
		if (!leitch_get_date(rbufp,leitch)) {
			leitch->state = STATE_IDLE;
			break;
		}
		leitch_send(leitch,"T\r");
#ifdef DEBUG
		if (debug)
			fprintf(stderr, "%u\n",leitch->yearday);
#endif
		leitch->state = STATE_TIME1;
		break;
	case STATE_TIME1:
		if (!leitch_get_time(rbufp,leitch,1)) {
		}
		if (!clocktime(leitch->yearday,leitch->hour,leitch->minute,
			leitch->second, 0, rbufp->recv_time.l_ui,
			&leitch->yearstart, &leitch->reftime1.l_ui)) {
			leitch->state = STATE_IDLE;
			break;
		}
#ifdef DEBUG
		if (debug)
			fprintf(stderr, "%u\n", leitch->reftime1.l_ui);
#endif
		MSUTOTSF(leitch->fudge1, leitch->reftime1.l_uf);
		leitch->codetime1 = rbufp->recv_time;
		leitch->state = STATE_TIME2;
		break;
	case STATE_TIME2:
		if (!leitch_get_time(rbufp,leitch,2)) {
		}
		if (!clocktime(leitch->yearday,leitch->hour,leitch->minute,
			leitch->second, 0, rbufp->recv_time.l_ui,
			&leitch->yearstart, &leitch->reftime2.l_ui)) {
			leitch->state = STATE_IDLE;
			break;
		}
#ifdef DEBUG
		if (debug)
			fprintf(stderr, "%u\n", leitch->reftime2.l_ui);
#endif
		MSUTOTSF(leitch->fudge1, leitch->reftime2.l_uf);
		leitch->codetime2 = rbufp->recv_time;
		leitch->state = STATE_TIME3;
		break;
	case STATE_TIME3:
		if (!leitch_get_time(rbufp,leitch,3)) {
		}
		if (!clocktime(leitch->yearday,leitch->hour,leitch->minute,
			leitch->second, 0, rbufp->recv_time.l_ui,
			&leitch->yearstart, &leitch->reftime3.l_ui)) {
			leitch->state = STATE_IDLE;
			break;
		}
#ifdef DEBUG
		if (debug)
			fprintf(stderr, "%u\n", leitch->reftime3.l_ui);
#endif
		MSUTOTSF(leitch->fudge1, leitch->reftime3.l_uf);
		leitch->codetime3 = rbufp->recv_time;
		leitch_process(leitch);
		leitch->state = STATE_IDLE;
		break;
	default:
		syslog(LOG_ERR,
		    "leitech_receive: invalid state %d unit %d",
		    leitch->state, leitch->unit);
	}
}

/*
 * leitch_process - process a pile of samples from the clock
 *
 * This routine uses a three-stage median filter to calculate offset and
 * dispersion. reduce jitter. The dispersion is calculated as the span
 * of the filter (max - min), unless the quality character (format 2) is
 * non-blank, in which case the dispersion is calculated on the basis of
 * the inherent tolerance of the internal radio oscillator, which is
 * +-2e-5 according to the radio specifications.
 */
static void
leitch_process(leitch)
	struct leitchunit *leitch;
{
	l_fp off;
	s_fp delay;
	l_fp codetime;
	l_fp tmp_fp;
	int isinsync = 1;
	u_fp dispersion = 10;

	delay = 20;

	codetime = leitch->codetime3;

	off = leitch->reftime1;
	L_SUB(&off,&leitch->codetime1);

#ifdef DEBUG
	if (debug)
	    fprintf(stderr,"%u %u %u %u %d %d\n",
		leitch->codetime1.l_ui, leitch->codetime1.l_uf,
		leitch->reftime1.l_ui, leitch->reftime1.l_uf,
		off.l_ui, off.l_uf);
#endif
	tmp_fp = leitch->reftime2;
	L_SUB(&tmp_fp,&leitch->codetime2);
	if (L_ISGEQ(&off,&tmp_fp))
		off = tmp_fp;
#ifdef DEBUG
	if (debug)
		fprintf(stderr,"%u %u %u %u %d %d\n",
		leitch->codetime2.l_ui, leitch->codetime2.l_uf,
		leitch->reftime2.l_ui, leitch->reftime2.l_uf,
		off.l_ui, off.l_uf);
#endif
	tmp_fp = leitch->reftime3;
	L_SUB(&tmp_fp,&leitch->codetime3);

	if (L_ISGEQ(&off,&tmp_fp))
		off = tmp_fp;

#ifdef DEBUG
	if (debug)
	    fprintf(stderr,"%u %u %u %u %d %d\n",
		leitch->codetime3.l_ui, leitch->codetime3.l_uf,
		leitch->reftime3.l_ui, leitch->reftime3.l_uf,
		off.l_ui, off.l_uf);
#endif
	refclock_receive(leitch->peer, &off, 0, dispersion, &codetime,
            &codetime, isinsync);
}

/*
 * leitch_timeout
 */
static void
leitch_timeout(fp)
	struct peer *fp;
{

#ifdef DEBUG
	if (debug)
		fprintf(stderr, "leitch_timeout()\n");
#endif

#ifdef	NOTYET
    {	struct leitchunit *leitch = (struct leitchunit *)fp;

	switch(leitch->state) {
	case STATE_IDLE:
		leitch_send(leitch,"D\r");
		leitch->state = STATE_DATE;
		break;
	case STATE_DATE:
		leitch_send(leitch,"T\r");
		leitch->state = STATE_TIME1;
		break;
	case STATE_TIME1:
	case STATE_TIME2:
	case STATE_TIME3:
	default:
		break;
	}
	
	leitch->leitchtimer.event_time += 30;
	TIMER_ENQUEUE(timerqueue, &leitch->leitchtimer);
    }
#endif	/* NOTYET */
}

/*
 * dysize
 */
static int
dysize(year)
int year;
{
	if (year%4) {	/* not a potential leap year */
		return (365);
	} else {
		if (year % 100) {	/* is a leap year */
			return (366);
		} else {	
			if (year % 400) {
				return (365);
			} else {
				return (366);
			}
		}
	}
}

static int
leitch_get_date(rbufp,leitch)
	struct recvbuf *rbufp;
	struct leitchunit *leitch;
{
	int i;

	if (rbufp->recv_length < 6)
		return(0);
#define BAD(A) (rbufp->recv_buffer[A] < '0') || (rbufp->recv_buffer[A] > '9')
	if (BAD(0)||BAD(1)||BAD(2)||BAD(3)||BAD(4)||BAD(5))
		return(0);
#define ATOB(A) ((rbufp->recv_buffer[A])-'0')
	leitch->year = ATOB(0)*10 + ATOB(1);
	leitch->month = ATOB(2)*10 + ATOB(3);
	leitch->day = ATOB(4)*10 + ATOB(5);

	/* sanity checks */
	if (leitch->month > 12)
		return(0);
	if (leitch->day > days_in_month[leitch->month-1])
		return(0);

	/* calculate yearday */
	i = 0;
	leitch->yearday = leitch->day;

	while ( i < (leitch->month-1) )
		leitch->yearday += days_in_month[i++];

	if ((dysize((leitch->year>90?1900:2000)+leitch->year)==365) && 
		leitch->month > 2)
		leitch->yearday--;

	return(1);
}

/*
 * leitch_get_time
 */
static int
leitch_get_time(rbufp,leitch,which)
	struct recvbuf *rbufp;
	struct leitchunit *leitch;
	int which;
{
	if (BAD(0)||BAD(1)||BAD(2)||BAD(3)||BAD(4)||BAD(5))
		return(0);
	leitch->hour = ATOB(0)*10 +ATOB(1);
	leitch->minute = ATOB(2)*10 +ATOB(3);
	leitch->second = ATOB(4)*10 +ATOB(5);

	if ((leitch->hour > 23) || (leitch->minute > 60) ||
		(leitch->second > 60))
		return(0);
	return(1);
}

#endif
