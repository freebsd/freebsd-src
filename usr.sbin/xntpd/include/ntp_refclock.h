/*
 * ntp_refclock.h - definitions for reference clock support
 */

#include "ntp_types.h"

#if !defined(SYSV_TTYS) && !defined(STREAM) & !defined(BSD_TTYS)
#define BSD_TTYS
#endif /* SYSV_TTYS STREAM BSD_TTYS */

/*
 * Macros to determine the clock type and unit numbers from a
 * 127.127.t.u address.
 */
#define	REFCLOCKTYPE(srcadr)	((SRCADR(srcadr) >> 8) & 0xff)
#define REFCLOCKUNIT(srcadr)	(SRCADR(srcadr) & 0xff)

/*
 * list of reference clock names
 * see lib/clocktypes.c (must also agree with xntpd/refclock_conf.c)
 */
struct clktype {
	int code;		/* driver "major" number */
	char *clocktype;	/* LONG description */
	char *abbrev;		/* short description */
};

/*
 * Definitions for default values
 */
#define	noentry		0	/* flag for null routine */

/*
 * Definitions for flags
 */
#define	NOFLAGS			0
#define	REF_FLAG_BCLIENT	0x1	/* clock prefers to run as a bclient */

/*
 * Flag values
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
	u_char type;
	u_char flags;
	u_char haveflags;
	u_short lencode;	/* ahem, we do have some longer "time-codes" */
	char *lastcode;
	U_LONG polls;
	U_LONG noresponse;
	U_LONG badformat;
	U_LONG baddata;
	U_LONG timereset;
	char *clockdesc;	/* description of clock, in ASCII */
	l_fp fudgetime1;
	l_fp fudgetime2;
	LONG fudgeval1;
	LONG fudgeval2;
	u_char currentstatus;
	u_char lastevent;
	u_char unused;
	struct ctl_var *kv_list;	/* additional variables */
};

/*
 * Reference clock I/O structure.  Used to provide an interface between
 * the reference clock drivers and the I/O module.
 */
struct refclockio {
	struct refclockio *next;
	void (*clock_recv)();
	caddr_t srcclock;	/* pointer to clock structure */
	int datalen;
	int fd;
	U_LONG recvcount;
};


/*
 * Sizes of things we return for debugging
 */
#define	NCLKBUGVALUES		16
#define	NCLKBUGTIMES		32

/*
 * Structure for returning debugging info
 */
struct refclockbug {
	u_char nvalues;
	u_char ntimes;
	u_short svalues;
	U_LONG stimes;
	U_LONG values[NCLKBUGVALUES];
	l_fp times[NCLKBUGTIMES];
};

/*
 * Struct refclock provides the interface between the reference
 * clock support and particular clock drivers.  There are entries
 * to open and close a unit, optional values to specify the
 * timer interval for calls to the transmit procedure and to
 * specify a polling routine to be called when the transmit
 * procedure executes.  There is an entry which is called when
 * the transmit routine is about to shift zeroes into the
 * filter register, and entries for stuffing fudge factors into
 * the driver and getting statistics from it.
 */
struct refclock {
  int	(*clock_start)	P((u_int, struct peer *));	/* start a clock unit */
  void	(*clock_shutdown) P((int));			/* shut a clock down */
  void	(*clock_poll)	P((int, struct peer *));	/* called from the xmit routine */
  void	(*clock_control) P((u_int, struct refclockstat *, struct refclockstat *));	/* set fudge values, return stats */
  void	(*clock_init)	P((void));			/* initialize driver data at startup */
  void	(*clock_buginfo) P((int, struct refclockbug *));	/* get clock dependent bug info */
  U_LONG clock_flags;		/* flag values */
};

extern	int	io_addclock_simple P((struct refclockio *));
extern	int	io_addclock	P((struct refclockio *));
extern	void	io_closeclock	P((struct refclockio *));

#ifdef	REFCLOCK
extern	void	refclock_buginfo P((struct sockaddr_in *, struct refclockbug *));
extern	void	refclock_control P((struct sockaddr_in *, struct refclockstat *, struct refclockstat *));
#endif	/* REFCLOCK */
