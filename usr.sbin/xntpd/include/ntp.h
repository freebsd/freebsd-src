/* ntp.h,v 3.1 1993/07/06 01:06:47 jbj Exp
 * ntp.h - NTP definitions for the masses
 */

#include "ntp_types.h"

/*
 * How to get signed characters.  On machines where signed char works,
 * use it.  On machines where signed char doesn't work, char had better
 * be signed.
 */
#if !defined(S_CHAR_DEFINED)
#if defined(NO_SIGNED_CHAR_DECL)
typedef char s_char;
#else
typedef signed char s_char;
#endif
#ifdef sequent
#undef SO_RCVBUF
#undef SO_SNDBUF
#endif
#endif

/*
 * NTP protocol parameters.  See section 3.2.6 of the specification.
 */
#define	NTP_VERSION	((u_char)3) /* current version number */
#define	NTP_OLDVERSION	((u_char)1) /* oldest credible version */
#define	NTP_PORT	123	/* included for sake of non-unix machines */
#define	NTP_MAXSTRATUM	((u_char)15) /* max stratum, infinity a la Bellman-Ford */
#define	NTP_MAXAGE	86400	/* one day in seconds */
#define	NTP_MAXSKEW	1	/* 1 sec, skew after NTP_MAXAGE w/o updates */
#define	NTP_SKEWINC	49170	/* skew increment for clock updates (l_f) */
#define	NTP_SKEWFACTOR	16	/* approximation of factor for peer calcs */
#define	NTP_MAXDISTANCE (1*FP_SECOND) /* max. rootdelay for synchr. */
#define NTP_MINDPOLL	6	/* default min poll (64 sec) */
#define	NTP_MINPOLL	4	/* absolute min poll (16 sec) */
#define	NTP_MAXPOLL	10	/* actually 1<<10, or 1024 sec */
#define	NTP_MINCLOCK	3	/* minimum for outlyer detection */
#define	NTP_MAXCLOCK	10	/* maximum select list size */
#define	NTP_MINDISPERSE	0x28f	/* 0.01 sec in fp format */
#define	NTP_MAXDISPERSE	(16*FP_SECOND)	/* maximum dispersion (fp 16) */
#define	NTP_DISPFACTOR	20	/* MAXDISPERSE as a shift */
#define	NTP_WINDOW	8	/* reachability register size */
#define	NTP_SHIFT	8	/* 8 suitable for crystal time base */
#define	NTP_MAXKEY	65535	/* maximum authentication key number */

/*
 * Loop filter parameters.  See section 5.1 of the specification.
 *
 * Note that these are appropriate for a crystal time base.  If your
 * system clock is line frequency controlled you should read the
 * specification for appropriate modifications.  Note that the
 * loop filter code will have to change if you change CLOCK_MAX
 * to be greater than or equal to 500 ms.
 *
 * Note these parameters have been rescaled for a time constant range from
 * 0 through 10, with 2 corresoponding to the old time constant of 0.
 */
#define	CLOCK_MINSTEP	900	/* step timeout (sec) */
#define	CLOCK_ADJ	0	/* log2 adjustment interval (1 sec) */
#define	CLOCK_DSCALE	20	/* skew reg. scale: unit is 2**-20 ~= 1 ppm */
#define	CLOCK_FREQ	16	/* log2 frequency weight (65536) */
#define	CLOCK_PHASE	6	/* log2 phase weight (64) */
#define CLOCK_WEIGHTTC	5	/* log2 time constant weight (32) */
#define CLOCK_HOLDTC	128	/* time constant hold (sec) */

#define	CLOCK_MAX_F	0x20c49ba6	/* 128 ms, in time stamp format */
#define	CLOCK_MAX_I	0x0	/* both fractional and integral parts */

#define	CLOCK_WAYTOOBIG	1000	/* if clock 1000 sec off, forget it */

/*
 * Unspecified default.  sys.precision defaults to -6 unless otherwise
 * adjusted.
 */
#define	DEFAULT_SYS_PRECISION	(-6)

/*
 * Event timers are actually implemented as a sorted queue of expiry
 * times.  The queue is slotted, with each slot holding timers which
 * expire in a 2**(NTP_MINPOLL-1) (8) second period.  The timers in
 * each slot are sorted by increasing expiry time.  The number of
 * slots is 2**(NTP_MAXPOLL-(NTP_MINPOLL-1)), or 128, to cover a time
 * period of 2**NTP_MAXPOLL (1024) seconds into the future before
 * wrapping.
 */
#define	EVENT_TIMEOUT	CLOCK_ADJ

struct event {
	struct event *next;		/* next in chain */
	struct event *prev;		/* previous in chain */
	struct peer *peer;		/* peer this counter belongs to */
	void (*event_handler)();	/* routine to call to handle event */
	U_LONG event_time;		/* expiry time of counter */
};

#define	TIMER_SLOTTIME	(1<<(NTP_MINPOLL-1))
#define	TIMER_NSLOTS	(1<<(NTP_MAXPOLL-(NTP_MINPOLL-1)))
#define	TIMER_SLOT(t)	(((t) >> (NTP_MINPOLL-1)) & (TIMER_NSLOTS-1))

/*
 * TIMER_ENQUEUE() puts stuff on the timer queue.  It takes as
 * arguments (ea), an array of event slots, and (iev), the event
 * to be inserted.  This one searches the hash bucket from the
 * end, and is about optimum for the timing requirements of
 * NTP peers.
 */
#define	TIMER_ENQUEUE(ea, iev) \
	do { \
		register struct event *ev; \
		\
		ev = (ea)[TIMER_SLOT((iev)->event_time)].prev; \
		while (ev->event_time > (iev)->event_time) \
			ev = ev->prev; \
		(iev)->prev = ev; \
		(iev)->next = ev->next; \
		(ev)->next->prev = (iev); \
		(ev)->next = (iev); \
	} while(0)

/*
 * TIMER_INSERT() also puts stuff on the timer queue, but searches the
 * bucket from the top.  This is better for things that do very short
 * time outs, like clock support.
 */
#define	TIMER_INSERT(ea, iev) \
	do { \
		register struct event *ev; \
		\
		ev = (ea)[TIMER_SLOT((iev)->event_time)].next; \
		while (ev->event_time != 0 && \
		    ev->event_time < (iev)->event_time) \
			ev = ev->next; \
		(iev)->next = ev; \
		(iev)->prev = ev->prev; \
		(ev)->prev->next = (iev); \
		(ev)->prev = (iev); \
	} while(0)

/*
 * Remove an event from the queue.
 */
#define	TIMER_DEQUEUE(ev) \
	do { \
		if ((ev)->next != 0) { \
			(ev)->next->prev = (ev)->prev; \
			(ev)->prev->next = (ev)->next; \
			(ev)->next = (ev)->prev = 0; \
		} \
	} while (0)

/*
 * The interface structure is used to hold the addresses and socket
 * numbers of each of the interfaces we are using.
 */
struct interface {
	int fd;			/* socket this is opened on */
	int bfd;		/* socket for receiving broadcasts */
	struct sockaddr_in sin;	/* interface address */
	struct sockaddr_in bcast;	/* broadcast address */
	struct sockaddr_in mask;	/* interface mask */
	char name[8];		/* name of interface */
	int flags;		/* interface flags */
	LONG received;		/* number of incoming packets */
	LONG sent;		/* number of outgoing packets */
	LONG notsent;		/* number of send failures */
};

/*
 * Flags for interfaces
 */
#define	INT_BROADCAST	1	/* can broadcast out this interface */
#define	INT_BCASTOPEN	2	/* broadcast socket is open */
#define	INT_LOOPBACK	4	/* the loopback interface */

/*
 * Define flasher bits (tests 1 through 8 in packet procedure)
 * These reveal the state at the last grumble from the peer and are
 * most handy for diagnosing problems, even if not strictly a state
 * variable in the spec. These are recorded in the peer structure.
 */
#define TEST1		0x01	/* duplicate packet received */
#define TEST2		0x02	/* bogus packet received */
#define TEST3		0x04	/* protocol unsynchronized */
#define TEST4		0x08	/* peer delay/dispersion bounds check */
#define TEST5		0x10	/* peer authentication failed */
#define TEST6		0x20	/* peer clock unsynchronized */
#define TEST7		0x40	/* peer stratum out of bounds */
#define TEST8		0x80	/* root delay/dispersion bounds check */

/*
 * The peer structure.  Holds state information relating to the guys
 * we are peering with.  Most of this stuff is from section 3.2 of the
 * spec.
 */
struct peer {
	struct peer *next;
	struct peer *ass_next;		/* link pointer in associd hash */
	struct sockaddr_in srcadr;	/* address of remote host */
	struct interface *dstadr;	/* pointer to address on local host */
	u_char leap;			/* leap indicator */
	u_char hmode;			/* association mode with this peer */
	u_char pmode;			/* peer's association mode */
	u_char stratum;			/* stratum of remote peer */
	s_char precision;		/* peer's clock precision */
	u_char ppoll;			/* peer poll interval */
	u_char hpoll;			/* local host poll interval */
	u_char minpoll;			/* min local host poll interval */
	u_char maxpoll;			/* max local host poll interval */
	u_char version;			/* version number */
	u_char flags;			/* peer flags */
	u_char flash;			/* peer flashers (for maint) */
	u_char refclktype;		/* reference clock type */
	u_char refclkunit;		/* reference clock unit number */
	u_char sstclktype;		/* clock type for system status word */
	s_fp rootdelay;			/* distance from primary clock */
	u_fp rootdispersion;		/* peer clock dispersion */
	U_LONG refid;			/* peer reference ID */
	l_fp reftime;			/* time of peer's last update */
	struct event event_timer;	/* event queue entry */
	U_LONG keyid;			/* encription key ID */
	U_LONG pkeyid;			/* keyid used to encrypt last message */
	u_short associd;		/* association ID, a unique integer */
	u_char unused;
/* **Start of clear-to-zero area.*** */
/* Everything that is cleared to zero goes below here */
	u_char valid;			/* valid counter */
#define	clear_to_zero	valid
	u_char reach;			/* reachability, NTP_WINDOW bits */
	u_char unreach;			/* unreachable count */
	u_short filter_nextpt;		/* index into filter shift register */
	s_fp filter_delay[NTP_SHIFT];	/* delay part of shift register */
	l_fp filter_offset[NTP_SHIFT];	/* offset part of shift register */
	s_fp filter_soffset[NTP_SHIFT]; /* offset in s_fp format, for disp */
	l_fp org;			/* originate time stamp */
	l_fp rec;			/* receive time stamp */
	l_fp xmt;			/* transmit time stamp */
/* ***End of clear-to-zero area.*** */
/* Everything that is cleared to zero goes above here */
	u_char filter_order[NTP_SHIFT]; /* we keep the filter sorted here */
#define	end_clear_to_zero	filter_order[0]
	u_fp filter_error[NTP_SHIFT];	/* error part of shift register */
	LONG update;			/* base sys_clock for skew calc.s */
	s_fp delay;			/* filter estimated delay */
	u_fp dispersion;		/* filter estimated dispersion */
	l_fp offset;			/* filter estimated clock offset */
	s_fp soffset;			/* fp version of above */
	s_fp synch;			/* synch distance from above */
	u_fp selectdisp;		/* select dispersion */

	/*
	 * Stuff related to the experimental broadcast delay
	 * determination code.  The registers will probably go away
	 * later.
	 */
	U_LONG estbdelay;		/* broadcast delay, as a ts fraction */

	/*
	 * statistic counters
	 */
	U_LONG timereset;		/* time stat counters were reset */
	U_LONG sent;			/* number of updates sent */
	U_LONG received;		/* number of frames received */
	U_LONG timereceived;		/* last time a frame received */
	U_LONG timereachable;		/* last reachable/unreachable event */
	U_LONG processed;		/* processed by the protocol */
	U_LONG badauth;			/* bad credentials detected */
	U_LONG bogusorg;		/* rejected due to bogus origin */
	U_LONG bogusrec;		/* rejected due to bogus receive */
	U_LONG bogusdelay;		/* rejected due to bogus delay */
	U_LONG disttoolarge;		/* rejected due to large distance */
	U_LONG oldpkt;			/* rejected as duplicate packet */
	U_LONG seldisptoolarge;		/* too much dispersion for selection */
	U_LONG selbroken;		/* broken NTP detected in selection */
	U_LONG seltooold;		/* too LONG since sync in selection */
	u_char candidate;		/* position after candidate selection */
	u_char select;			/* position at end of falseticker sel */
	u_char was_sane;		/* set to 1 if it passed sanity check */
	u_char correct;			/* set to 1 if it passed correctness check */
	u_char last_event;		/* set to code for last peer error */
	u_char num_events;		/* num. of events which have occurred */
};

/*
 * Values for peer.leap, sys_leap
 */
#define	LEAP_NOWARNING	0x0	/* normal, no leap second warning */
#define	LEAP_ADDSECOND	0x1	/* last minute of day has 61 seconds */
#define	LEAP_DELSECOND	0x2	/* last minute of day has 59 seconds */
#define	LEAP_NOTINSYNC	0x3	/* overload, clock is free running */

/*
 * Values for peer.mode
 */
#define	MODE_UNSPEC	0	/* unspecified (probably old NTP version) */
#define	MODE_ACTIVE	1	/* symmetric active */
#define	MODE_PASSIVE	2	/* symmetric passive */
#define	MODE_CLIENT	3	/* client mode */
#define	MODE_SERVER	4	/* server mode */
#define	MODE_BROADCAST	5	/* broadcast mode */
#define	MODE_CONTROL	6	/* control mode packet */
#define	MODE_PRIVATE	7	/* implementation defined function */

#define	MODE_BCLIENT	8	/* a pseudo mode, used internally */


/*
 * Values for peer.stratum, sys_stratum
 */
#define	STRATUM_REFCLOCK ((u_char)0) /* stratum claimed by primary clock */
#define	STRATUM_PRIMARY	((u_char)1) /* host has a primary clock */
#define	STRATUM_INFIN ((u_char)NTP_MAXSTRATUM) /* infinity a la Bellman-Ford */
/* A stratum of 0 in the packet is mapped to 16 internally */
#define	STRATUM_PKT_UNSPEC ((u_char)0) /* unspecified in packet */
#define	STRATUM_UNSPEC	((u_char)(NTP_MAXSTRATUM+(u_char)1)) /* unspecified */

/*
 * Values for peer.flags
 */
#define	FLAG_CONFIG		0x1	/* association was configured */
#define	FLAG_AUTHENABLE		0x2	/* this guy needs authentication */
#define	FLAG_UNUSED		0x4	/* (not used) */
#define	FLAG_DEFBDELAY		0x8	/* using default bdelay */
#define	FLAG_AUTHENTIC		0x10	/* last message was authentic */
#define	FLAG_REFCLOCK		0x20	/* this is actually a reference clock */
#define	FLAG_SYSPEER		0x40	/* this is one of the selected peers */
#define FLAG_PREFER		0x80	/* this is the preferred peer */

/*
 * Definitions for the clear() routine.  We use memset() to clear
 * the parts of the peer structure which go to zero.  These are
 * used to calculate the start address and length of the area.
 */
#define	CLEAR_TO_ZERO(p)	((char *)&((p)->clear_to_zero))
#define	END_CLEAR_TO_ZERO(p)	((char *)&((p)->end_clear_to_zero))
#define	LEN_CLEAR_TO_ZERO	(END_CLEAR_TO_ZERO((struct peer *)0) \
				    - CLEAR_TO_ZERO((struct peer *)0))
/*
 * Reference clock identifiers (for pps signal)
 */
#define PPSREFID "PPS "			/* used when pps controls stratum > 1 */

/*
 * Reference clock types.  Added as necessary.
 */
#define	REFCLK_NONE		0	/* unknown or missing */
#define	REFCLK_LOCALCLOCK	1	/* external (e.g., ACTS) */
#define	REFCLK_GPS_TRAK		2	/* TRAK 8810 GPS Receiver */
#define	REFCLK_WWV_PST		3	/* PST/Traconex 1020 WWV/H */
#define	REFCLK_WWVB_SPECTRACOM	4	/* Spectracom 8170/Netclock WWVB */
#define	REFCLK_GOES_TRUETIME	5	/* TrueTime 468-DC GOES */
#define REFCLK_IRIG_AUDIO	6       /* IRIG-B audio decoder */
#define	REFCLK_CHU		7	/* scratchbuilt CHU (Canada) */
#define REFCLK_PARSE		8	/* generic driver (usually DCF77,GPS) */
#define	REFCLK_GPS_MX4200	9	/* Magnavox MX4200 GPS */
#define REFCLK_GPS_AS2201	10	/* Austron 2201A GPS */
#define	REFCLK_OMEGA_TRUETIME	11	/* TrueTime OM-DC OMEGA */
#define REFCLK_IRIG_TPRO	12	/* KSI/Odetics TPRO-S IRIG */
#define REFCLK_ATOM_LEITCH	13	/* Leitch CSD 5300 Master Clock */
#define REFCLK_MSF_EES		14	/* MSF EES M201, UK */
#define	REFCLK_GPSTM_TRUETIME	15	/* TrueTime GPS/TM-TMD */

/*
 * We tell reference clocks from real peers by giving the reference
 * clocks an address of the form 127.127.t.u, where t is the type and
 * u is the unit number.  We define some of this here since we will need
 * some sanity checks to make sure this address isn't interpretted as
 * that of a normal peer.
 */
#define	REFCLOCK_ADDR	0x7f7f0000	/* 127.127.0.0 */
#define	REFCLOCK_MASK	0xffff0000	/* 255.255.0.0 */

#define	ISREFCLOCKADR(srcadr)	((SRCADR(srcadr) & REFCLOCK_MASK) \
					== REFCLOCK_ADDR)

/*
 * Macro for checking for invalid addresses.  This is really, really
 * gross, but is needed so no one configures a host on net 127 now that
 * we're encouraging it the the configuration file.
 */
#define	LOOPBACKADR	0x7f000001
#define	LOOPNETMASK	0xff000000

#define	ISBADADR(srcadr)	(((SRCADR(srcadr) & LOOPNETMASK) \
				    == (LOOPBACKADR & LOOPNETMASK)) \
				    && (SRCADR(srcadr) != LOOPBACKADR))

/*
 * Utilities for manipulating addresses and port numbers
 */
#define	NSRCADR(src)	((src)->sin_addr.s_addr) /* address in net byte order */
#define	NSRCPORT(src)	((src)->sin_port)	/* port in net byte order */
#define	SRCADR(src)	(ntohl(NSRCADR((src))))	/* address in host byte order */
#define	SRCPORT(src)	(ntohs(NSRCPORT((src))))	/* host port */

/*
 * NTP packet format.  The mac field is optional.  It isn't really
 * an l_fp either, but for now declaring it that way is convenient.
 * See Appendix A in the specification.
 *
 * Note that all u_fp and l_fp values arrive in network byte order
 * and must be converted (except the mac, which isn't, really).
 */
struct pkt {
	u_char li_vn_mode;	/* contains leap indicator, version and mode */
	u_char stratum;		/* peer's stratum */
	u_char ppoll;		/* the peer polling interval */
	s_char precision;	/* peer clock precision */
	s_fp rootdelay;		/* distance to primary clock */
	u_fp rootdispersion;	/* clock dispersion */
	U_LONG refid;		/* reference clock ID */
	l_fp reftime;		/* time peer clock was last updated */
	l_fp org;		/* originate time stamp */
	l_fp rec;		/* receive time stamp */
	l_fp xmt;		/* transmit time stamp */

#define	MIN_MAC_LEN	(sizeof(U_LONG) + 8)		/* DES */
#define	MAX_MAC_LEN	(sizeof(U_LONG) + 16)		/* MD5 */

	U_LONG keyid;		/* key identification */
	u_char mac[MAX_MAC_LEN-sizeof(U_LONG)];/* message-authentication code */
	/*l_fp mac;*/
};

/*
 * Packets can come in two flavours, one with a mac and one without.
 */
#define	LEN_PKT_NOMAC	(sizeof(struct pkt) - MAX_MAC_LEN)

/*
 * Minimum size of packet with a MAC: has to include at least a key number.
 */
#define	LEN_PKT_MAC	(LEN_PKT_NOMAC + sizeof(U_LONG))

/*
 * Stuff for extracting things from li_vn_mode
 */
#define	PKT_MODE(li_vn_mode)	((u_char)((li_vn_mode) & 0x7))
#define	PKT_VERSION(li_vn_mode)	((u_char)(((li_vn_mode) >> 3) & 0x7))
#define	PKT_LEAP(li_vn_mode)	((u_char)(((li_vn_mode) >> 6) & 0x3))

/*
 * Stuff for putting things back into li_vn_mode
 */
#define	PKT_LI_VN_MODE(li, vn, md) \
	((u_char)((((li) << 6) & 0xc0) | (((vn) << 3) & 0x38) | ((md) & 0x7)))


/*
 * Dealing with stratum.  0 gets mapped to 16 incoming, and back to 0
 * on output.
 */
#define	PKT_TO_STRATUM(s)	((u_char)(((s) == (STRATUM_PKT_UNSPEC)) ?\
				(STRATUM_UNSPEC) : (s)))

#define	STRATUM_TO_PKT(s)	((u_char)(((s) == (STRATUM_UNSPEC)) ?\
				(STRATUM_PKT_UNSPEC) : (s)))

/*
 * Format of a recvbuf.  These are used by the asynchronous receive
 * routine to store incoming packets and related information.
 */

/*
 *  the maximum length NTP packet is a full length NTP control message with
 *  the maximum length message authenticator.  I hate to hard-code 468 and 12,
 *  but only a few modules include ntp_control.h...
 */   
#define	RX_BUFF_SIZE	(468+12+MAX_MAC_LEN)

struct recvbuf {
	struct recvbuf *next;		/* next buffer in chain */
	union {
		struct sockaddr_in X_recv_srcadr;
		caddr_t X_recv_srcclock;
	} X_from_where;
#define recv_srcadr	X_from_where.X_recv_srcadr
#define	recv_srcclock	X_from_where.X_recv_srcclock
	struct sockaddr_in srcadr;	/* where packet came from */
	struct interface *dstadr;	/* interface datagram arrived thru */
	l_fp recv_time;			/* time of arrival */
	void (*receiver)();		/* routine to receive buffer */
	int recv_length;		/* number of octets received */
	union {
		struct pkt X_recv_pkt;
		char X_recv_buffer[RX_BUFF_SIZE];
	} recv_space;
#define	recv_pkt	recv_space.X_recv_pkt
#define	recv_buffer	recv_space.X_recv_buffer
};


/*
 * Event codes.  Used for reporting errors/events to the control module
 */
#define	PEER_EVENT	0x80		/* this is a peer event */

#define	EVNT_UNSPEC	0
#define	EVNT_SYSRESTART	1
#define	EVNT_SYSFAULT	2
#define	EVNT_SYNCCHG	3
#define	EVNT_PEERSTCHG	4
#define	EVNT_CLOCKRESET	5
#define	EVNT_BADDATETIM	6
#define	EVNT_CLOCKEXCPT	7

#define	EVNT_PEERIPERR	(1|PEER_EVENT)
#define	EVNT_PEERAUTH	(2|PEER_EVENT)
#define	EVNT_UNREACH	(3|PEER_EVENT)
#define	EVNT_REACH	(4|PEER_EVENT)
#define	EVNT_PEERCLOCK	(5|PEER_EVENT)

/*
 * Clock event codes
 */
#define	CEVNT_NOMINAL	0
#define	CEVNT_TIMEOUT	1
#define	CEVNT_BADREPLY	2
#define	CEVNT_FAULT	3
#define	CEVNT_PROP	4
#define	CEVNT_BADDATE	5
#define	CEVNT_BADTIME	6
#define CEVNT_MAX	CEVNT_BADTIME

/*
 * Very misplaced value.  Default port through which we send traps.
 */
#define	TRAPPORT	18447


/*
 * To speed lookups, peers are hashed by the low order bits of the remote
 * IP address.  These definitions relate to that.
 */
#define	HASH_SIZE	32
#define	HASH_MASK	(HASH_SIZE-1)
#define	HASH_ADDR(src)	((SRCADR((src))^(SRCADR((src))>>8)) & HASH_MASK)


/*
 * The poll update procedure takes an extra argument which controls
 * how a random perturbation is applied to peer.timer.  The choice is
 * to not randomize at all, to randomize only if we're going to update
 * peer.timer, and to randomize no matter what (almost, the algorithm
 * is that we apply the random value if it is less than the current
 * timer count).
 */
#define	POLL_NOTRANDOM		0	/* don't randomize */
#define	POLL_RANDOMCHANGE	1	/* if you change, change randomly */
#define	POLL_MAKERANDOM		2	/* randomize next interval */


/*
 * How we randomize polls.  The poll interval is a power of two.
 * We chose a random value which is between 1/4 and 3/4 of the
 * poll interval we would normally use and which is an even multiple
 * of the EVENT_TIMEOUT.  The random number routine, given an argument
 * spread value of n, returns an integer between 0 and (1<<n)-1.  This
 * is shifted by EVENT_TIMEOUT and added to the base value.
 */
#define	RANDOM_SPREAD(poll)	((poll) - (EVENT_TIMEOUT+1))
#define	RANDOM_POLL(poll, rval)	((((rval)+1)<<EVENT_TIMEOUT) + (1<<((poll)-2)))

/*
 * min, min3 and max.  Makes it easier to transliterate the spec without
 * thinking about it.
 */
#define	min(a,b)	(((a) < (b)) ? (a) : (b))
#define	max(a,b)	(((a) > (b)) ? (a) : (b))
#define	min3(a,b,c)	min(min((a),(b)), (c))


/*
 * Configuration items.  These are for the protocol module (proto_config())
 */
#define	PROTO_BROADCLIENT	1
#define	PROTO_PRECISION		2
#define	PROTO_AUTHENTICATE	3
#define	PROTO_BROADDELAY	4
#define	PROTO_AUTHDELAY		5
#define	PROTO_MAXSKEW		6
#define	PROTO_SELECT		7

/*
 * Configuration items for the loop filter
 */
#define	LOOP_DRIFTCOMP		1	/* set frequency offset */
#define LOOP_PPSDELAY		2	/* set pps delay */
#define LOOP_PPSBAUD		3	/* set pps baud rate */

/*
 * Configuration items for the stats printer
 */
#define	STATS_FREQ_FILE		1	/* configure drift file */
#define STATS_STATSDIR		2	/* directory prefix for stats files */
#define	STATS_PID_FILE		3	/* configure xntpd PID file */

#define MJD_1970		40587	/* MJD for 1 Jan 1970 */

/*
 * Default parameters.  We use these in the absense of something better.
 */
#define	DEFPRECISION	(-5)		/* conservatively low */
#define	DEFBROADDELAY	(0x020c49ba)	/* 8 ms.  This is round trip delay */

/*
 * Structure used optionally for monitoring when this is turned on.
 */
struct mon_data {
	struct mon_data *hash_next;	/* next structure in hash list */
	struct mon_data *hash_prev;	/* previous structure in hash list */
	struct mon_data *mru_next;	/* next structure in MRU list */
	struct mon_data *mru_prev;	/* previous structure in MRU list */
	struct mon_data *fifo_next;	/* next structure in FIFO list */
	struct mon_data *fifo_prev;	/* previous structure in FIFO list */
	U_LONG lastdrop;		/* last time dropped due to RES_LIMIT*/
	U_LONG lasttime;		/* last time data updated */
	U_LONG firsttime;		/* time structure initialized */
	U_LONG count;			/* count we have seen */
	U_LONG rmtadr;			/* address of remote host */
	u_short rmtport;		/* remote port last came from */
	u_char mode;			/* mode of incoming packet */
	u_char version;			/* version of incoming packet */
};

/*
 * Values used with mon_enabled to indicate reason for enabling monitoring
 */
#define MON_OFF    0x00			/* no monitoring */
#define MON_ON     0x01			/* monitoring explicitly enabled */
#define MON_RES    0x02			/* implicit monitoring for RES_LIMITED */
/*
 * Structure used for restrictlist entries
 */
struct restrictlist {
	struct restrictlist *next;	/* link to next entry */
	U_LONG addr;			/* host address (host byte order) */
	U_LONG mask;			/* mask for address (host byte order) */
	U_LONG count;			/* number of packets matched */
	u_short flags;			/* accesslist flags */
	u_short mflags;			/* match flags */
};

/*
 * Access flags
 */
#define	RES_IGNORE		0x1	/* ignore if matched */
#define	RES_DONTSERVE		0x2	/* don't give him any time */
#define	RES_DONTTRUST		0x4	/* don't trust if matched */
#define	RES_NOQUERY		0x8	/* don't allow queries if matched */
#define	RES_NOMODIFY		0x10	/* don't allow him to modify server */
#define	RES_NOPEER		0x20	/* don't allocate memory resources */
#define	RES_NOTRAP		0x40	/* don't allow him to set traps */
#define	RES_LPTRAP		0x80	/* traps set by him are low priority */
#define RES_LIMITED		0x100   /* limit per net number of clients */

#define	RES_ALLFLAGS \
    (RES_IGNORE|RES_DONTSERVE|RES_DONTTRUST|RES_NOQUERY\
    |RES_NOMODIFY|RES_NOPEER|RES_NOTRAP|RES_LPTRAP|RES_LIMITED)

/*
 * Match flags
 */
#define	RESM_INTERFACE		0x1	/* this is an interface */
#define	RESM_NTPONLY		0x2	/* match ntp port only */

/*
 * Restriction configuration ops
 */
#define	RESTRICT_FLAGS		1	/* add flags to restrict entry */
#define	RESTRICT_UNFLAG		2	/* remove flags from restrict entry */
#define	RESTRICT_REMOVE		3	/* remove a restrict entry */


/*
 * Experimental alternate selection algorithm identifiers
 */
#define	SELECT_1	1
#define	SELECT_2	2
#define	SELECT_3	3
#define	SELECT_4	4
#define	SELECT_5	5

/*
 * Endpoint structure for the select algorithm
 */
struct endpoint {
	s_fp	val;			/* offset of endpoint */
	int	type;			/* interval entry/exit */
};
