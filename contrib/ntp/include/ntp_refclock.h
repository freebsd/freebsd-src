/*
 * ntp_refclock.h - definitions for reference clock support
 */

#ifndef NTP_REFCLOCK_H
#define NTP_REFCLOCK_H

#include "ntp_types.h"

#if defined(HAVE_BSD_TTYS)
#include <sgtty.h>
#endif /* HAVE_BSD_TTYS */

#if defined(HAVE_SYSV_TTYS)
#include <termio.h>
#endif /* HAVE_SYSV_TTYS */

#if defined(HAVE_TERMIOS)
# ifdef TERMIOS_NEEDS__SVID3
#  define _SVID3
# endif
# include <termios.h>
# ifdef TERMIOS_NEEDS__SVID3
#  undef _SVID3
# endif
#endif

#if defined(HAVE_SYS_MODEM_H)
#include <sys/modem.h>
#endif

#if 0 /* If you need that, include ntp_io.h instead */
#if defined(STREAM)
#include <stropts.h>
#if defined(CLK) /* This is never defined, except perhaps by a system header file */
#include <sys/clkdefs.h>
#endif /* CLK */
#endif /* STREAM */
#endif

#include "recvbuff.h"

#if !defined(SYSV_TTYS) && !defined(STREAM) & !defined(BSD_TTYS)
#define BSD_TTYS
#endif /* SYSV_TTYS STREAM BSD_TTYS */

#define SAMPLE(x)	pp->filter[pp->coderecv++ % MAXSTAGE] = (x); \
			if (pp->coderecv % MAXSTAGE == pp->codeproc % MAXSTAGE) \
				pp->codeproc++;

/*
 * Macros to determine the clock type and unit numbers from a
 * 127.127.t.u address
 */
#define	REFCLOCKTYPE(srcadr)	((SRCADR(srcadr) >> 8) & 0xff)
#define REFCLOCKUNIT(srcadr)	(SRCADR(srcadr) & 0xff)

/*
 * List of reference clock names and descriptions. These must agree with
 * lib/clocktypes.c and ntpd/refclock_conf.c.
 */
struct clktype {
	int code;		/* driver "major" number */
	const char *clocktype;	/* long description */
	const char *abbrev;	/* short description */
};

/*
 * Configuration flag values
 */
#define	CLK_HAVETIME1	0x1
#define	CLK_HAVETIME2	0x2
#define	CLK_HAVEVAL1	0x4
#define	CLK_HAVEVAL2	0x8

#define	CLK_FLAG1	0x1
#define	CLK_FLAG2	0x2
#define	CLK_FLAG3	0x4
#define	CLK_FLAG4	0x8

#define	CLK_HAVEFLAG1	0x10
#define	CLK_HAVEFLAG2	0x20
#define	CLK_HAVEFLAG3	0x40
#define	CLK_HAVEFLAG4	0x80

/*
 * Constant for disabling event reporting in
 * refclock_receive. ORed in leap
 * parameter
 */
#define REFCLOCK_OWN_STATES	0x80

/*
 * Structure for returning clock status
 */
struct refclockstat {
	u_char	type;		/* clock type */
	u_char	flags;		/* clock flags */
	u_char	haveflags;	/* bit array of valid flags */
	u_short	lencode;	/* length of last timecode */
	const char *p_lastcode;	/* last timecode received */
	u_int32	polls;		/* transmit polls */
	u_int32	noresponse;	/* no response to poll */
	u_int32	badformat;	/* bad format timecode received */
	u_int32	baddata;	/* invalid data timecode received */
	u_int32	timereset;	/* driver resets */
	const char *clockdesc;	/* ASCII description */
	double	fudgetime1;	/* configure fudge time1 */
	double	fudgetime2;	/* configure fudge time2 */
	int32	fudgeval1;	/* configure fudge value1 */
	int32	fudgeval2;	/* configure fudge value2 */
	u_char	currentstatus;	/* clock status */
	u_char	lastevent;	/* last exception event */
	u_char	leap;		/* leap bits */
	struct	ctl_var *kv_list; /* additional variables */
};

/*
 * Reference clock I/O structure.  Used to provide an interface between
 * the reference clock drivers and the I/O module.
 */
struct refclockio {
	struct	refclockio *next; /* link to next structure */
	void	(*clock_recv) P((struct recvbuf *)); /* completion routine */
	int 	(*io_input)   P((struct recvbuf *)); /* input routine -
				to avoid excessive buffer use
				due to small bursts
				of refclock input data */
	caddr_t	srcclock;	/* pointer to clock structure */
	int	datalen;	/* lenth of data */
	int	fd;		/* file descriptor */
	u_long	recvcount;	/* count of receive completions */
};

/*
 * Structure for returning debugging info
 */
#define	NCLKBUGVALUES	16
#define	NCLKBUGTIMES	32

struct refclockbug {
	u_char	nvalues;	/* values following */
	u_char	ntimes;		/* times following */
	u_short	svalues;	/* values format sign array */
	u_int32	stimes;		/* times format sign array */
	u_int32	values[NCLKBUGVALUES]; /* real values */
	l_fp	times[NCLKBUGTIMES]; /* real times */
};

/*
 * Structure interface between the reference clock support
 * ntp_refclock.c and the driver utility routines
 */
#define MAXSTAGE	60	/* max median filter stages  */
#define NSTAGE		5	/* default median filter stages */
#define BMAX		128	/* max timecode length */
#define GMT		0	/* I hope nobody sees this */
#define MAXDIAL		60	/* max length of modem dial strings */

/*
 * Line discipline flags. These require line discipline or streams
 * modules to be installed/loaded in the kernel. If specified, but not
 * installed, the code runs as if unspecified.
 */
#define LDISC_STD	0x0	/* standard */
#define LDISC_CLK	0x1	/* tty_clk \n intercept */
#define LDISC_CLKPPS	0x2	/* tty_clk \377 intercept */
#define LDISC_ACTS	0x4	/* tty_clk #* intercept */
#define LDISC_CHU	0x8	/* depredated */
#define LDISC_PPS	0x10	/* ppsclock, ppsapi */
#define LDISC_RAW	0x20	/* raw binary */

struct refclockproc {
	struct	refclockio io;	/* I/O handler structure */
	caddr_t	unitptr;	/* pointer to unit structure */
	u_char	leap;		/* leap/synchronization code */
	u_char	currentstatus;	/* clock status */
	u_char	lastevent;	/* last exception event */
	u_char	type;		/* clock type */
	const char *clockdesc;	/* clock description */

	char	a_lastcode[BMAX]; /* last timecode received */
	u_short	lencode;	/* length of last timecode */

	int	year;		/* year of eternity */
	int	day;		/* day of year */
	int	hour;		/* hour of day */
	int	minute;		/* minute of hour */
	int	second;		/* second of minute */
	int	msec;		/* millisecond of second */
	long	usec;		/* microsecond of second (alt) */
	u_long	yearstart;	/* beginning of year */
	int	coderecv;	/* put pointer */
	int	codeproc;	/* get pointer */
	l_fp	lastref;	/* timecode timestamp */
	l_fp	lastrec;	/* local timestamp */
	double	offset;		/* mean offset */
	double	disp;		/* sample dispersion */
	double	jitter;		/* jitter (mean squares) */
	double	filter[MAXSTAGE]; /* median filter */

	/*
	 * Configuration data
	 */
	double	fudgetime1;	/* fudge time1 */
	double	fudgetime2;	/* fudge time2 */
	u_int32	refid;		/* reference identifier */
	u_char	sloppyclockflag; /* fudge flags */

	/*
	 * Status tallies
 	 */
	u_long	timestarted;	/* time we started this */
	u_long	polls;		/* polls sent */
	u_long	noreply;	/* no replies to polls */
	u_long	badformat;	/* bad format reply */
	u_long	baddata;	/* bad data reply */
};

/*
 * Structure interface between the reference clock support
 * ntp_refclock.c and particular clock drivers. This must agree with the
 * structure defined in the driver.
 */
#define	noentry	0		/* flag for null routine */
#define	NOFLAGS	0		/* flag for null flags */

struct refclock {
	int (*clock_start)	P((int, struct peer *));
	void (*clock_shutdown)	P((int, struct peer *));
	void (*clock_poll)	P((int, struct peer *));
	void (*clock_control)	P((int, struct refclockstat *,
				    struct refclockstat *, struct peer *));
	void (*clock_init)	P((void));
	void (*clock_buginfo)	P((int, struct refclockbug *, struct peer *));
	u_long clock_flags;
};

/*
 * Function prototypes
 */
/*
 * auxiliary PPS interface (implemented by refclock_atom())
 */
extern	int	pps_sample P((l_fp *));
extern	int	io_addclock_simple P((struct refclockio *));
extern	int	io_addclock	P((struct refclockio *));
extern	void	io_closeclock	P((struct refclockio *));

#ifdef REFCLOCK
extern	void	refclock_buginfo P((struct sockaddr_in *,
				    struct refclockbug *));
extern	void	refclock_control P((struct sockaddr_in *,
				    struct refclockstat *,
				    struct refclockstat *));
extern	int	refclock_open	P((char *, int, int));
extern	void	refclock_transmit P((struct peer *));
extern	int	refclock_ioctl	P((int, int));
extern 	int	refclock_process P((struct refclockproc *));
extern 	void	refclock_process_offset P((struct refclockproc *, l_fp, l_fp, double));
extern	void	refclock_report	P((struct peer *, int));
extern	int	refclock_gtlin	P((struct recvbuf *, char *, int,
				    l_fp *));
#endif /* REFCLOCK */

#endif /* NTP_REFCLOCK_H */
