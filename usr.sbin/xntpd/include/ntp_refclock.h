/*
 * ntp_refclock.h - definitions for reference clock support
 */

#include "ntp_types.h"

#if defined(HAVE_BSD_TTYS)
#include <sgtty.h>
#endif /* HAVE_BSD_TTYS */

#if defined(HAVE_SYSV_TTYS)
#include <termio.h>
#endif /* HAVE_SYSV_TTYS */

#if defined(HAVE_TERMIOS)
#include <termios.h>
#endif

#if defined(STREAM)
#include <stropts.h>
#if defined(CLK)
#include <sys/clkdefs.h>
#endif /* CLK */
#endif /* STREAM */

#if !defined(SYSV_TTYS) && !defined(STREAM) & !defined(BSD_TTYS)
#define BSD_TTYS
#endif /* SYSV_TTYS STREAM BSD_TTYS */

/*
 * Macros to determine the clock type and unit numbers from a
 * 127.127.t.u address
 */
#define	REFCLOCKTYPE(srcadr)	((SRCADR(srcadr) >> 8) & 0xff)
#define REFCLOCKUNIT(srcadr)	(SRCADR(srcadr) & 0xff)

/*
 * List of reference clock names and descriptions. These must agree with
 * lib/clocktypes.c and xntpd/refclock_conf.c.
 */
struct clktype {
	int code;		/* driver "major" number */
	char *clocktype;	/* long description */
	char *abbrev;		/* short description */
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
 * Structure for returning clock status
 */
struct refclockstat {
	u_char	type;		/* clock type */
	u_char	flags;		/* clock flags */
	u_char	haveflags;	/* bit array of valid flags */
	u_char	lencode;	/* length of last timecode */
	char	*lastcode;	/* last timecode received */
	U_LONG	polls;		/* transmit polls */
	U_LONG	noresponse;	/* no response to poll */
	U_LONG	badformat;	/* bad format timecode received */
	U_LONG	baddata;	/* invalid data timecode received */
	U_LONG	timereset;	/* driver resets */
	char	*clockdesc;	/* ASCII description */
	l_fp	fudgetime1;	/* configure fudge time1 */
	l_fp	fudgetime2;	/* configure fudge time2 */
	LONG	fudgeval1;	/* configure fudge value1 */
	LONG	fudgeval2;	/* configure fudge value2 */
	u_char	currentstatus;	/* clock status */
	u_char	lastevent;	/* last exception event */
	u_char	unused;		/* spare */
	struct	ctl_var *kv_list; /* additional variables */
};

/*
 * Reference clock I/O structure.  Used to provide an interface between
 * the reference clock drivers and the I/O module.
 */
struct refclockio {
	struct	refclockio *next; /* link to next structure */
	void	(*clock_recv)();/* completion routine */
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
	U_LONG	stimes;		/* times format sign array */
	U_LONG	values[NCLKBUGVALUES]; /* real values */
	l_fp	times[NCLKBUGTIMES]; /* real times */
};

/*
 * Structure interface between the reference clock support
 * ntp_refclock.c and the driver utility routines
 */
#define MAXSTAGE	64	/* max stages in shift register */
#define BMAX		128	/* max timecode length */
#define GMT		0	/* I hope nobody sees this */
#define MAXDIAL		20	/* max length of modem dial strings */

/*
 * Line discipline flags. These require line discipline or streams
 * modules to be installed/loaded in the kernel. If specified, but not
 * installed, the code runs as if unspecified.
 */
#define LDISC_STD	0x0	/* standard */
#define LDISC_CLK	0x1	/* tty_clk \n intercept */
#define LDISC_CLKPPS	0x2	/* tty_clk \377 intercept */
#define LDISC_ACTS	0x4	/* tty_clk #* intercept */
#define LDISC_CHU	0x8	/* tty_chu */
#define LDISC_PPS	0x10	/* ppsclock */

struct refclockproc {
	struct	refclockio io;	/* I/O handler structure */
	caddr_t	unitptr;	/* pointer to unit structure */
	u_long	lasttime;	/* last clock update time */
	u_char	leap;		/* leap/synchronization code */
	u_char	currentstatus;	/* clock status */
	u_char	lastevent;	/* last exception event */
	u_char	type;		/* clock type */
	char	*clockdesc;	/* clock description */
	char	lastcode[BMAX];	/* last timecode received */
	u_char	lencode;	/* length of last timecode */

	u_int	year;		/* year of eternity */
	u_int	day;		/* day of year */
	u_int	hour;		/* hour of day */
	u_int	minute;		/* minute of hour */
	u_int	second;		/* second of minute */
	u_int	msec;		/* millisecond of second */
	u_long	usec;		/* microsecond of second (alt) */
	u_int	nstages;	/* median filter stages */
	u_long	yearstart;	/* beginning of year */
	u_long	coderecv;	/* sample counter */
	l_fp	lastref;	/* last reference timestamp */
	l_fp	lastrec;	/* last local timestamp */
	l_fp	offset;		/* median offset */
	u_fp	dispersion;	/* sample dispersion */
	l_fp	filter[MAXSTAGE]; /* median filter */

	/*
	 * Configuration data
	 */
	l_fp	fudgetime1;	/* fudge time1 */
	l_fp	fudgetime2;	/* fudge time2 */
	u_long	refid;		/* reference identifier */
	u_long	sloppyclockflag; /* fudge flags */

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
				    struct refclockstat *));
	void (*clock_init)	P((void));
	void (*clock_buginfo)	P((int, struct refclockbug *));
	u_long clock_flags;
};

/*
 * Function prototypes
 */
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
extern 	int	refclock_process P((struct refclockproc *, int, int));
extern	void	refclock_report	P((struct peer *, u_char));
extern	int	refclock_gtlin	P((struct recvbuf *, char *, int,
				    l_fp *));
#endif /* REFCLOCK */
