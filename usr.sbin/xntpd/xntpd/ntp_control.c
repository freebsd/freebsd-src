/*
 * ntp_control.c - respond to control messages and send async traps
 */
#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_refclock.h"
#include "ntp_control.h"
#include "ntp_stdlib.h"

/*
 * Structure to hold request procedure information
 */
#define	NOAUTH	0
#define	AUTH	1

#define	NO_REQUEST	(-1)

struct ctl_proc {
	short control_code;	/* defined request code */
	u_short flags;		/* flags word */
	void (*handler)();	/* routine to handle request */
};

/*
 * Only one flag.  Authentication required or not.
 */
#define	NOAUTH	0
#define	AUTH	1

/*
 * Request processing routines
 */
static	void	ctl_error	P((int));
static	u_short	ctlclkstatus	P((struct refclockstat *));
static	void	ctl_flushpkt	P((int));
static	void	ctl_putdata	P((char *, int, int));
static	void	ctl_putstr	P((char *, char *, int));
static	void	ctl_putlfp	P((char *, l_fp *));

#ifdef	UNUSED
static	void	ctl_putulfp	P((char *, l_fp *));
#endif	/* UNUSED */

static	void	ctl_putfp	P((char *, s_fp));
static	void	ctl_putufp	P((char *, u_fp));
static	void	ctl_putuint	P((char *, U_LONG));
static	void	ctl_puthex	P((char *, U_LONG));
static	void	ctl_putint	P((char *, LONG));
static	void	ctl_putts	P((char *, l_fp *));
static	void	ctl_putadr	P((char *, U_LONG));
static	void	ctl_putid	P((char *, char *));
static	void	ctl_putarray	P((char *, s_fp *, int));
static	void	ctl_putsys	P((int));
static	void	ctl_putpeer	P((int, struct peer *));
#ifdef REFCLOCK
static	void	ctl_putclock	P((int, struct refclockstat *, int));
#endif	/* REFCLOCK */
static	struct ctl_var *ctl_getitem	P((struct ctl_var *, char **));
static  unsigned long count_var P((struct ctl_var *));
static	void	control_unspec	P((struct recvbuf *, int));
static	void	read_status	P((struct recvbuf *, int));
static	void	read_variables	P((struct recvbuf *, int));
static	void	write_variables	P((struct recvbuf *, int));
static	void	read_clock_status	P((struct recvbuf *, int));
static	void	write_clock_status	P((struct recvbuf *, int));
static	void	set_trap	P((struct recvbuf *, int));
static	void	unset_trap	P((struct recvbuf *, int));
static	struct ctl_trap *ctlfindtrap	P((struct sockaddr_in *, struct interface *));

static	struct ctl_proc control_codes[] = {
	{ CTL_OP_UNSPEC,	NOAUTH,	control_unspec },
	{ CTL_OP_READSTAT,	NOAUTH,	read_status },
	{ CTL_OP_READVAR,	NOAUTH,	read_variables },
	{ CTL_OP_WRITEVAR,	AUTH,	write_variables },
	{ CTL_OP_READCLOCK,	NOAUTH,	read_clock_status },
	{ CTL_OP_WRITECLOCK,	NOAUTH,	write_clock_status },
	{ CTL_OP_SETTRAP,	NOAUTH,	set_trap },
	{ CTL_OP_UNSETTRAP,	NOAUTH,	unset_trap },
	{ NO_REQUEST,		0 }
};

/*
 * System variable values.  The array can be indexed by
 * the variable index to find the textual name.
 */
static	struct ctl_var sys_var[] = {
	{ 0,		PADDING, "" },		/* 0 */
	{ CS_LEAP,	RW,	"leap" },	/* 1 */
	{ CS_STRATUM,	RO,	"stratum" },	/* 2 */
	{ CS_PRECISION,	RO,	"precision" },	/* 3 */
	{ CS_ROOTDELAY,	RO,	"rootdelay" },	/* 4 */
	{ CS_ROOTDISPERSION, RO, "rootdispersion" }, /* 5 */
	{ CS_REFID,	RO,	"refid" },	/* 6 */
	{ CS_REFTIME,	RO,	"reftime" },	/* 7 */
	{ CS_POLL,	RO,	"poll" },	/* 8 */
	{ CS_PEERID,	RO,	"peer" },	/* 9 */
	{ CS_OFFSET,	RO,	"phase" },	/* 10 */
	{ CS_DRIFT,	RO,	"freq" },	/* 11 */
	{ CS_COMPLIANCE, RO,	"compliance" },	/* 12 */
	{ CS_CLOCK,	RO,	"clock" },	/* 13 */
	{ CS_LEAPIND,	RW,	"leapindicator" }, /* 14 */
	{ CS_LEAPWARNING, RW,	"leapwarning" }, /* 15 */
	{ CS_PROCESSOR,	RO,	"processor" },	/* 16 */
	{ CS_SYSTEM,	RO,	"system" },	/* 17 */
	{ CS_KEYID,	RO,	"keyid" },	/* 18 */
	{ CS_REFSKEW,	RO,	"refskew" },	/* 19 */
	{ CS_VARLIST,	RO,	"sys_var_list" },/* 20 */
	{ 0,		EOV,	""	}
};

static	struct ctl_var *ext_sys_var = (struct ctl_var *)0;

/*
 * System variables we print by default (in fuzzball order, more-or-less)
 */
static	u_char def_sys_var[] = {
	CS_SYSTEM,
	CS_LEAP,
	CS_STRATUM,
	CS_ROOTDELAY,
	CS_ROOTDISPERSION,
	CS_PEERID,
	CS_REFID,
	CS_REFTIME,
	CS_POLL,
	CS_CLOCK,
	CS_OFFSET,
	CS_DRIFT,
	CS_COMPLIANCE,
	0
};


/*
 * Peer variable list
 */
static	struct ctl_var peer_var[] = {
	{ 0,		PADDING, "" },		/* 0 */
	{ CP_CONFIG,	RO,	"config" },	/* 1 */
	{ CP_AUTHENABLE, RO,	"authenable" },	/* 2 */
	{ CP_AUTHENTIC,	RO,	"authentic" },	/* 3 */
	{ CP_SRCADR,	RO,	"srcadr" },	/* 4 */
	{ CP_SRCPORT,	RO,	"srcport" },	/* 5 */
	{ CP_DSTADR,	RO,	"dstadr" },	/* 6 */
	{ CP_DSTPORT,	RO,	"dstport" },	/* 7 */
	{ CP_LEAP,	RO,	"leap" },	/* 8 */
	{ CP_HMODE,	RO,	"hmode" },	/* 9 */
	{ CP_STRATUM,	RO,	"stratum" },	/* 10 */
	{ CP_PPOLL,	RO,	"ppoll" },	/* 11 */
	{ CP_HPOLL,	RO,	"hpoll" },	/* 12 */
	{ CP_PRECISION,	RO,	"precision" },	/* 13 */
	{ CP_ROOTDELAY,	RO,	"rootdelay" },	/* 14 */
	{ CP_ROOTDISPERSION, RO, "rootdispersion" }, /* 15 */
	{ CP_REFID,	RO,	"refid" },	/* 16 */
	{ CP_REFTIME,	RO,	"reftime" },	/* 17 */
	{ CP_ORG,	RO,	"org" },	/* 18 */
	{ CP_REC,	RO,	"rec" },	/* 19 */
	{ CP_XMT,	RO,	"xmt" },	/* 20 */
	{ CP_REACH,	RO,	"reach" },	/* 21 */
	{ CP_VALID,	RO,	"valid" },	/* 22 */
	{ CP_TIMER,	RO,	"timer" },	/* 23 */
	{ CP_DELAY,	RO,	"delay" },	/* 24 */
	{ CP_OFFSET,	RO,	"offset" },	/* 25 */
	{ CP_DISPERSION,RO,	"dispersion" },	/* 26 */
	{ CP_KEYID,	RO,	"keyid" },	/* 27 */
	{ CP_FILTDELAY,	RO,	"filtdelay" },	/* 28 */
	{ CP_FILTOFFSET, RO,	"filtoffset" },	/* 29 */
	{ CP_PMODE,	RO,	"pmode" },	/* 30 */
	{ CP_RECEIVED,	RO,	"received" },	/* 31 */
	{ CP_SENT,	RO,	"sent" },	/* 32 */
	{ CP_FILTERROR,	RO,	"filterror" },	/* 33 */
	{ CP_FLASH,	RO,	"flash" },	/* 34 */
	{ CP_DISP,	PADDING,"" },		/* 35 */
	{ CP_VARLIST,	RO,	"peer_var_list" }, /* 36 */
	{ 0,		EOV,	""	}
};


/*
 * Peer variables we print by default
 */
static	u_char def_peer_var[] = {
	CP_SRCADR,
	CP_SRCPORT,
	CP_DSTADR,
	CP_DSTPORT,
	CP_KEYID,
	CP_STRATUM,
	CP_PRECISION,
	CP_ROOTDELAY,
	CP_ROOTDISPERSION,
	CP_REFID,
	CP_REFTIME,
	CP_DELAY,
	CP_OFFSET,
	CP_DISPERSION,
	CP_REACH,
	CP_VALID,
	CP_HMODE,
	CP_PMODE,
	CP_HPOLL,
	CP_PPOLL,
	CP_LEAP,
	CP_FLASH,
	CP_ORG,
	CP_REC,
	CP_XMT,
	CP_FILTDELAY,
	CP_FILTOFFSET,
	CP_FILTERROR,
	0
};


#ifdef REFCLOCK
/*
 * Clock variable list
 */
static	struct ctl_var clock_var[] = {
	{ 0,		PADDING, "" },		/* 0 */
	{ CC_TYPE,	RO,	"type" },	/* 1 */
	{ CC_TIMECODE,	RO,	"timecode" },	/* 2 */
	{ CC_POLL,	RO,	"poll" },	/* 3 */
	{ CC_NOREPLY,	RO,	"noreply" },	/* 4 */
	{ CC_BADFORMAT,	RO,	"badformat" },	/* 5 */
	{ CC_BADDATA,	RO,	"baddata" },	/* 6 */
	{ CC_FUDGETIME1, RO,	"fudgetime1" },	/* 7 */
	{ CC_FUDGETIME2, RO,	"fudgetime2" },	/* 8 */
	{ CC_FUDGEVAL1,	RO,	"fudgeval1" },	/* 9 */
	{ CC_FUDGEVAL2,	RO,	"fudgeval2" },	/* 10 */
	{ CC_FLAGS,	RO,	"flags" },	/* 11 */
	{ CC_DEVICE,	RO,	"device" },	/* 12 */
	{ CC_VARLIST,	RO,	"clock_var_list" },/* 13 */
	{ 0,		EOV,	""	}
};


/*
 * Clock variables printed by default
 */
static	u_char def_clock_var[] = {
	CC_DEVICE,
	CC_TYPE,	/* won't be output if device= known */
	CC_TIMECODE,
	CC_POLL,
	CC_NOREPLY,
	CC_BADFORMAT,
	CC_BADDATA,
	CC_FUDGETIME1,
	CC_FUDGETIME2,
	CC_FUDGEVAL1,
	CC_FUDGEVAL2,
	CC_FLAGS,
	0
};
#endif


/*
 * System and processor definitions.  These will change for the gizmo board.
 */
#include <sys/utsname.h>
static struct utsname utsname;

/*
 * Trap structures.  We only allow a few of these, and send
 * a copy of each async message to each live one.  Traps time
 * out after an hour, it is up to the trap receipient to
 * keep resetting it to avoid being timed out.
 */
/* ntp_request.c */
	struct ctl_trap ctl_trap[CTL_MAXTRAPS];
	int num_ctl_traps;

/*
 * Type bits, for ctlsettrap() call.
 */
#define	TRAP_TYPE_CONFIG	0	/* used by configuration code */
#define	TRAP_TYPE_PRIO		1	/* priority trap */
#define	TRAP_TYPE_NONPRIO	2	/* nonpriority trap */


/*
 * List relating reference clock types to control message time sources.
 * Index by the reference clock type.
 * This list will only be used iff the reference clock driver doesn't
 * set peer->sstclktype to something different than CTL_SST_TS_UNSPEC.
 */
static u_char clocktypes[] = {
	CTL_SST_TS_NTP,		/* REFCLK_NONE */
	CTL_SST_TS_UNSPEC,	/* REFCLK_LOCALCLOCK */
	CTL_SST_TS_HF,		/* REFCLK_WWV_HEATH */
	CTL_SST_TS_HF,		/* REFCLK_WWV_PST */
	CTL_SST_TS_LF,		/* REFCLK_WWVB_SPECTRACOM */
	CTL_SST_TS_UHF,		/* REFCLK_GOES_TRUETIME */
	CTL_SST_TS_UHF,		/* REFCLK_GOES_TRAK */
	CTL_SST_TS_HF,		/* REFCLK_CHU */
	CTL_SST_TS_LF,		/* REFCLOCK_PARSE - default value - driver supplies actual value in peer->sstclktype */
	CTL_SST_TS_LF,		/* REFCLK_WWVB_SPECTRACOM_HP */
	CTL_SST_TS_UHF,		/* REFCLK_GPS_AS2201 */
	CTL_SST_TS_LF,		/* REFCLK_OMEGA_TRUETIME */
	CTL_SST_TS_UNSPEC,	/* Future expansion */
	CTL_SST_TS_UNSPEC,	/* Future expansion */
	CTL_SST_TS_UNSPEC,	/* Future expansion */
	CTL_SST_TS_UNSPEC	/* Future expansion */
};



/*
 * Keyid used for authenticating write requests.
 */
U_LONG ctl_auth_keyid;

/*
 * We keep track of the last error reported by the system internally
 */
static	u_char ctl_sys_last_event;
static	u_char ctl_sys_num_events;


/*
 * Statistic counters to keep track of requests and responses.
 */
U_LONG ctltimereset;		/* time stats reset */
U_LONG numctlreq;		/* number of requests we've received */
U_LONG numctlbadpkts;		/* number of bad control packets */
U_LONG numctlresponses;		/* number of resp packets sent with data */
U_LONG numctlfrags;		/* number of fragments sent */
U_LONG numctlerrors;		/* number of error responses sent */
U_LONG numctltooshort;		/* number of too short input packets */
U_LONG numctlinputresp;		/* number of responses on input */
U_LONG numctlinputfrag;		/* number of fragments on input */
U_LONG numctlinputerr;		/* number of input pkts with err bit set */
U_LONG numctlbadoffset;		/* number of input pkts with nonzero offset */
U_LONG numctlbadversion;	/* number of input pkts with unknown version */
U_LONG numctldatatooshort;	/* data too short for count */
U_LONG numctlbadop;		/* bad op code found in packet */
U_LONG numasyncmsgs;		/* number of async messages we've sent */

/*
 * Imported from the I/O module
 */
extern struct interface *any_interface;

/*
 * Imported from the main routines
 */
extern int	debug;

/*
 * Imported from the timer module
 */
extern U_LONG current_time;

extern struct peer *assoc_hash[];
extern int	pps_control;    /* flag for 1-pps signal present */
/*
 * Importations from the protocol module
 */
extern u_char	sys_leap;
extern u_char	sys_stratum;
extern s_char	sys_precision;
extern s_fp	sys_rootdelay;
extern u_fp	sys_rootdispersion;
extern U_LONG	sys_refid;
extern l_fp	sys_reftime;
extern l_fp	sys_refskew;
extern u_char	sys_poll;
extern struct peer *sys_peer;
/*
 * Imported from the loop filter module
 */
extern l_fp	last_offset;
extern s_fp	drift_comp;
extern int	time_constant;
/*
 * Imported from the leap module
 */
extern u_char	leap_indicator;
extern u_char	leap_warning;

/*
 * Response packet used by these routines.  Also some state information
 * so that we can handle packet formatting within a common set of
 * subroutines.  Note we try to enter data in place whenever possible,
 * but the need to set the more bit correctly means we occasionally
 * use the extra buffer and copy.
 */
static struct ntp_control rpkt;
static u_char	res_version;
static u_char	res_opcode;
static u_short	res_associd;
static int	res_offset;
static u_char *	datapt;
static u_char *	dataend;
static int	datalinelen;
static int	datanotbinflag;
static struct sockaddr_in *rmt_addr;
static struct interface *lcl_inter;

static u_char	res_authenticate;
static u_char	res_authokay;
static U_LONG	res_keyid;

#define	MAXDATALINELEN	(72)

static u_char	res_async;	/* set to 1 if this is async trap response */

/*
 * Pointers for saving state when decoding request packets
 */
static	char *reqpt;
static	char *reqend;

/*
 * init_control - initialize request data
 */
void
init_control()
{
	int i;

	uname(&utsname);

	ctl_clr_stats();

	ctl_auth_keyid = 0;
	ctl_sys_last_event = EVNT_UNSPEC;
	ctl_sys_num_events = 0;

	num_ctl_traps = 0;
	for (i = 0; i < CTL_MAXTRAPS; i++)
		ctl_trap[i].tr_flags = 0;
}


/*
 * ctl_error - send an error response for the current request
 */
static void
ctl_error(errcode)
	int errcode;
{
#ifdef DEBUG
	if (debug >= 4)
		printf("sending control error %d\n", errcode);
#endif
	/*
	 * fill in the fields.  We assume rpkt.sequence and rpkt.associd
	 * have already been filled in.
	 */
	rpkt.r_m_e_op = CTL_RESPONSE|CTL_ERROR|(res_opcode & CTL_OP_MASK);
	rpkt.status = htons((errcode<<8) & 0xff00);
	rpkt.count = 0;

	/*
	 * send packet and bump counters
	 */
	if (res_authenticate) {
		int maclen;

		*(U_LONG *)((u_char *)&rpkt + CTL_HEADER_LEN)
		    = htonl(res_keyid);
		maclen = 
		    authencrypt(res_keyid, (U_LONG *)&rpkt, CTL_HEADER_LEN);
		sendpkt(rmt_addr, lcl_inter, (struct pkt *)&rpkt,
		    CTL_HEADER_LEN + maclen);
	} else {
		sendpkt(rmt_addr, lcl_inter, (struct pkt *)&rpkt,
		    CTL_HEADER_LEN);
	}
	numctlerrors++;
}


/*
 * process_control - process an incoming control message
 */
void
process_control(rbufp, restrict)
	struct recvbuf *rbufp;
	int restrict;
{
	register struct ntp_control *pkt;
	register int req_count;
	register int req_data;
	register struct ctl_proc *cc;
	int properlen;
	int maclen;

#ifdef DEBUG
	if (debug)
		printf("in process_control()\n");
#endif

	/*
	 * Save the addresses for error responses
	 */
	numctlreq++;
	rmt_addr = &rbufp->recv_srcadr;
	lcl_inter = rbufp->dstadr;
	pkt = (struct ntp_control *)&rbufp->recv_pkt;

	/*
	 * If the length is less than required for the header, or
	 * it is a response or a fragment, ignore this.
	 */
	if (rbufp->recv_length < CTL_HEADER_LEN
	    || pkt->r_m_e_op & (CTL_RESPONSE|CTL_MORE|CTL_ERROR)
	    || pkt->offset != 0) {
#ifdef DEBUG
		if (debug)
			printf("invalid format in control packet\n");
#endif
		if (rbufp->recv_length < CTL_HEADER_LEN)
			numctltooshort++;
		if (pkt->r_m_e_op & CTL_RESPONSE)
			numctlinputresp++;
		if (pkt->r_m_e_op & CTL_MORE)
			numctlinputfrag++;
		if (pkt->r_m_e_op & CTL_ERROR)
			numctlinputerr++;
		if (pkt->offset != 0)
			numctlbadoffset++;
		return;
	}
	res_version = PKT_VERSION(pkt->li_vn_mode);
	if (res_version > NTP_VERSION || res_version < NTP_OLDVERSION) {
#ifdef DEBUG
		if (debug)
			printf("unknown version %d in control packet\n",
			    res_version);
#endif
		numctlbadversion++;
		return;
	}

	/*
	 * Pull enough data from the packet to make intelligent responses
	 */
	rpkt.li_vn_mode = PKT_LI_VN_MODE(sys_leap, res_version, MODE_CONTROL);
	res_opcode = pkt->r_m_e_op;
	rpkt.sequence = pkt->sequence;
	rpkt.associd = pkt->associd;
	rpkt.status = 0;
	res_offset = 0;
	res_associd = htons(pkt->associd);
	res_async = 0;
	res_authenticate = 0;
	res_keyid = 0;
	res_authokay = 0;
	req_count = (int)htons(pkt->count);
	datanotbinflag = 0;
	datalinelen = 0;
	datapt = rpkt.data;
	dataend = &(rpkt.data[CTL_MAX_DATA_LEN]);

	/*
	 * We're set up now.  Make sure we've got at least
	 * enough incoming data space to match the count.
	 */
	req_data = rbufp->recv_length - CTL_HEADER_LEN;
	if (req_data < req_count || rbufp->recv_length & 0x3) {
		ctl_error(CERR_BADFMT);
		numctldatatooshort++;
		return;
	}

	properlen = req_count + CTL_HEADER_LEN;
#ifdef DEBUG
	if (debug >= 2 && (rbufp->recv_length & 0x3) != 0)
		printf("Packet length %d unrounded\n", rbufp->recv_length);
#endif
	/* round up proper len to a 8 octet boundary */

	properlen = (properlen + 7) & ~7;

	if ((rbufp->recv_length & (sizeof(U_LONG)-1)) == 0
	    && (maclen = (rbufp->recv_length - properlen)) >= MIN_MAC_LEN
	    && maclen <= MAX_MAC_LEN) {

		res_authenticate = 1;
		res_keyid = ntohl(*(U_LONG *)((u_char *)pkt + properlen));

#ifdef DEBUG
		if (debug >= 3)
			printf(
      "recv_len %d, properlen %d, wants auth with keyid %d, MAC length=%d\n",
			    rbufp->recv_length, properlen, res_keyid, maclen);
#endif
		if (!authhavekey(res_keyid)) {
#ifdef DEBUG
			if (debug >= 2)
				printf("keyid %lu unknown\n", res_keyid);
#endif
		} else if (authdecrypt(res_keyid, (U_LONG *)pkt,
				       rbufp->recv_length - maclen)) {
#ifdef DEBUG
			if (debug >= 3)
				printf("authenticated okay\n");
#endif
			res_authokay = 1;
		} else {
#ifdef DEBUG
			if (debug >= 3)
				printf("authentication failed\n");
#endif
			res_keyid = 0;
		}
	}

	/*
	 * Set up translate pointers
	 */
	reqpt = (char *)pkt->data;
	reqend = reqpt + req_count;

	/*
	 * Look for the opcode processor
	 */
	for (cc = control_codes; cc->control_code != NO_REQUEST; cc++) {
		if (cc->control_code == res_opcode) {
#ifdef DEBUG	
			if (debug >= 2)
				printf("opcode %d, found command handler\n",
				    res_opcode);
#endif
			if (cc->flags == AUTH && (!res_authokay
			    || res_keyid != ctl_auth_keyid)) {
				ctl_error(CERR_PERMISSION);
				return;
			}
			(cc->handler)(rbufp, restrict);
			return;
		}
	}

	/*
	 * Can't find this one, return an error.
	 */
	numctlbadop++;
	ctl_error(CERR_BADOP);
	return;
}


/*
 * ctlpeerstatus - return a status word for this peer
 */
u_short
ctlpeerstatus(peer)
	register struct peer *peer;
{
	register u_short status;

	status = CTL_PST_SEL_REJECT;
	if (peer->was_sane != 0)
		status = CTL_PST_SEL_SANE;
	if (peer->correct != 0)
		status = CTL_PST_SEL_CORRECT;
	if (peer->candidate != 0)
		status = CTL_PST_SEL_SELCAND;
	if (peer->select != 0)
		status = CTL_PST_SEL_SYNCCAND;
	if (peer == sys_peer) {
		status = CTL_PST_SEL_DISTSYSPEER;
		if (peer->synch < NTP_MAXDISTANCE) {
			status = CTL_PST_SEL_SYSPEER;
			if (pps_control)
				status = CTL_PST_SEL_PPS;
		}
	}
	if (peer->flags & FLAG_CONFIG)
		status |= CTL_PST_CONFIG;
	if (peer->flags & FLAG_AUTHENABLE) {
		status |= CTL_PST_AUTHENABLE;
		if (peer->flags & FLAG_AUTHENTIC)
			status |= CTL_PST_AUTHENTIC;
	}
	if (peer->reach != 0)
		status |= CTL_PST_REACH;

	return (u_short)CTL_PEER_STATUS(status, peer->num_events,
	    peer->last_event);
}


/*
 * ctlclkstatus - return a status word for this clock
 */
static u_short
ctlclkstatus(clock)
	struct refclockstat *clock;
{
	return ((u_short)(clock->currentstatus) << 8)
	    | (u_short)(clock->lastevent);
}



/*
 * ctlsysstatus - return the system status word
 */
u_short
ctlsysstatus()
{
	register u_char clock;

	clock = CTL_SST_TS_UNSPEC;
	if (sys_peer != 0)
		if (sys_peer->sstclktype != CTL_SST_TS_UNSPEC)
			clock = sys_peer->sstclktype;
		else
			if (sys_peer->refclktype < sizeof(clocktypes))
				clock = clocktypes[sys_peer->refclktype];

	return (u_short)CTL_SYS_STATUS(sys_leap, clock, 
	    ctl_sys_num_events, ctl_sys_last_event);
}



/*
 * ctl_flushpkt - write out the current packet and prepare
 *		  another if necessary.
 */
static void
ctl_flushpkt(more)
	int more;
{
	int dlen;
	int sendlen;

	if (!more && datanotbinflag) {
		/*
		 * Big hack, output a trailing \r\n
		 */
		*datapt++ = '\r';
		*datapt++ = '\n';
	}
	dlen = datapt - (u_char *)rpkt.data;
	sendlen = dlen + CTL_HEADER_LEN;

	/*
	 * Pad to a multiple of 32 bits
	 */
	while (sendlen & 0x3) {
		*datapt++ = '\0';
		sendlen++;
	}

	/*
	 * Fill in the packet with the current info
	 */
	rpkt.r_m_e_op = CTL_RESPONSE|more|(res_opcode & CTL_OP_MASK);
	rpkt.count = htons((u_short)dlen);
	rpkt.offset = htons(res_offset);
	if (res_async) {
		register int i;

		for (i = 0; i < CTL_MAXTRAPS; i++) {
			if (ctl_trap[i].tr_flags & TRAP_INUSE) {
				rpkt.li_vn_mode = PKT_LI_VN_MODE(sys_leap,
				    ctl_trap[i].tr_version, MODE_CONTROL);
				rpkt.sequence = htons(ctl_trap[i].tr_sequence);
				sendpkt(&ctl_trap[i].tr_addr,
				    ctl_trap[i].tr_localaddr,
				    (struct pkt *)&rpkt, sendlen);
				if (!more)
					ctl_trap[i].tr_sequence++;
				numasyncmsgs++;
			}
		}
	} else {
		if (res_authenticate) {
			int maclen;
			int totlen = sendlen;

			/*
			 *  If we are going to authenticate, then there is
			 *  an additional requirement that the MAC begin on
			 *  a 64 bit boundary.
			 */
			while (totlen & 7) {
			    *datapt++ = '\0';
			    totlen++;
			}
			*(U_LONG *)datapt = htonl(res_keyid);
			maclen = 
			    authencrypt(res_keyid, (U_LONG *)&rpkt, totlen);

 			sendpkt(rmt_addr, lcl_inter, (struct pkt *)&rpkt,
			    totlen + maclen);
		} else {
 			sendpkt(rmt_addr, lcl_inter, (struct pkt *)&rpkt,
			    sendlen);
		}
		if (more)
			numctlfrags++;
		else
			numctlresponses++;
	}

	/*
	 * Set us up for another go around.
	 */
	res_offset += dlen;
	datapt = (u_char *)rpkt.data;
}


/*
 * ctl_putdata - write data into the packet, fragmenting and
 *		 starting another if this one is full.
 */
static void
ctl_putdata(dp, dlen, bin)
	char *dp;
	int dlen;
	int bin;	/* set to 1 when data is binary */
{
	int overhead;

	overhead = 0;
	if (!bin) {
		datanotbinflag = 1;
		overhead = 3;
		if (datapt != rpkt.data) {
			*datapt++ = ',';
			datalinelen++;
			if ((dlen + datalinelen + 1) >= MAXDATALINELEN) {
				*datapt++ = '\r';
				*datapt++ = '\n';
				datalinelen = 0;
			} else {
				*datapt++ = ' ';
				datalinelen++;
			}
		}
	}

	/*
	 * Save room for trailing junk
	 */
	if (dlen + overhead + datapt > dataend) {
		/*
		 * Not enough room in this one, flush it out.
		 */
		ctl_flushpkt(CTL_MORE);
	}

	memmove((char *)datapt, dp, dlen);
	datapt += dlen;
	datalinelen += dlen;
}


/*
 * ctl_putstr - write a tagged string into the response packet
 */
static void
ctl_putstr(tag, data, len)
	char *tag;
	char *data;
	int len;
{
	register char *cp, *cq;
	char buffer[400];

	cp = buffer;
	cq = tag;
	while (*cq != '\0')
		*cp++ = *cq++;
	
	if (len > 0) {
		*cp++ = '=';
		*cp++ = '"';
		if (len > (sizeof(buffer) - (cp - buffer) - 1))
			len = sizeof(buffer) - (cp - buffer) - 1;
		memmove(cp, data, len);
		cp += len;
		*cp++ = '"';
	}

	ctl_putdata(buffer, cp - buffer, 0);
}



/*
 * ctl_putlfp - write a tagged, signed l_fp into the response packet
 */
static void
ctl_putlfp(tag, ts)
	char *tag;
	l_fp *ts;
{
	register char *cp, *cq;
	char buffer[200];

	cp = buffer;
	cq = tag;
	while (*cq != '\0')
		*cp++ = *cq++;

	*cp++ = '=';
	cq = lfptoms(ts, 3);
	while (*cq != '\0')
		*cp++ = *cq++;
	
	ctl_putdata(buffer, cp - buffer, 0);
}


#ifdef	UNUSED
/*
 * ctl_putlfp - write a tagged, unsigned l_fp into the response
 */
static void
ctl_putulfp(tag, ts)
	char *tag;
	l_fp *ts;
{
	register char *cp, *cq;
	char buffer[200];

	cp = buffer;
	cq = tag;
	while (*cq != '\0')
		*cp++ = *cq++;

	*cp++ = '=';
	cq = ulfptoms(ts, 3);
	while (*cq != '\0')
		*cp++ = *cq++;
	
	ctl_putdata(buffer, cp - buffer, 0);
}
#endif	/* UNUSED */


/*
 * ctl_putfp - write a tagged s_fp number into the response
 */
static void
ctl_putfp(tag, fp)
	char *tag;
	s_fp fp;
{
	register char *cp, *cq;
	char buffer[200];

	cp = buffer;
	cq = tag;
	while (*cq != '\0')
		*cp++ = *cq++;

	*cp++ = '=';
	cq = fptoms(fp, 2);
	while (*cq != '\0')
		*cp++ = *cq++;
	
	ctl_putdata(buffer, cp - buffer, 0);
}


/*
 * ctl_putufp - write a tagged u_fp number into the response
 */
static void
ctl_putufp(tag, ufp)
	char *tag;
	u_fp ufp;
{
	register char *cp, *cq;
	char buffer[200];

	cp = buffer;
	cq = tag;
	while (*cq != '\0')
		*cp++ = *cq++;

	*cp++ = '=';
	cq = ufptoms(ufp, 2);
	while (*cq != '\0')
		*cp++ = *cq++;
	
	ctl_putdata(buffer, cp - buffer, 0);
}


/*
 * ctl_putuint - write a tagged unsigned integer into the response
 */
static void
ctl_putuint(tag, uval)
	char *tag;
	U_LONG uval;
{
	register char *cp, *cq;
	char buffer[200];

	cp = buffer;
	cq = tag;
	while (*cq != '\0')
		*cp++ = *cq++;

	*cp++ = '=';
	(void) sprintf(cp, "%u", uval);
	while (*cp != '\0')
		cp++;
	
	ctl_putdata(buffer, cp - buffer, 0);
}


/*
 * ctl_puthex - write a tagged unsigned integer, in hex, into the response
 */
static void
ctl_puthex(tag, uval)
	char *tag;
	U_LONG uval;
{
	register char *cp, *cq;
	char buffer[200];

	cp = buffer;
	cq = tag;
	while (*cq != '\0')
		*cp++ = *cq++;

	*cp++ = '=';
	(void) sprintf(cp, "0x%lx", uval);
	while (*cp != '\0')
		cp++;

	ctl_putdata(buffer, cp - buffer, 0);
}


/*
 * ctl_putint - write a tagged signed integer into the response
 */
static void
ctl_putint(tag, ival)
	char *tag;
	LONG ival;
{
	register char *cp, *cq;
	char buffer[200];

	cp = buffer;
	cq = tag;
	while (*cq != '\0')
		*cp++ = *cq++;

	*cp++ = '=';
	(void) sprintf(cp, "%d", ival);
	while (*cp != '\0')
		cp++;
	
	ctl_putdata(buffer, cp - buffer, 0);
}


/*
 * ctl_putts - write a tagged timestamp, in hex, into the response
 */
static void
ctl_putts(tag, ts)
	char *tag;
	l_fp *ts;
{
	register char *cp, *cq;
	char buffer[200];

	cp = buffer;
	cq = tag;
	while (*cq != '\0')
		*cp++ = *cq++;

	*cp++ = '=';
	(void) sprintf(cp, "0x%08lx.%08lx", ts->l_ui & 0xffffffffL,
	    ts->l_uf & 0xffffffffL);
	while (*cp != '\0')
		cp++;
	
	ctl_putdata(buffer, cp - buffer, 0);
}


/*
 * ctl_putadr - write a dotted quad IP address into the response
 */
static void
ctl_putadr(tag, addr)
	char *tag;
	U_LONG addr;
{
	register char *cp, *cq;
	char buffer[200];

	cp = buffer;
	cq = tag;
	while (*cq != '\0')
		*cp++ = *cq++;

	*cp++ = '=';
	cq = numtoa(addr);
	while (*cq != '\0')
		*cp++ = *cq++;
	
	ctl_putdata(buffer, cp - buffer, 0);
}


/*
 * ctl_putid - write a tagged clock ID into the response
 */
static void
ctl_putid(tag, id)
	char *tag;
	char *id;
{
	register char *cp, *cq;
	char buffer[200];

	cp = buffer;
	cq = tag;
	while (*cq != '\0')
		*cp++ = *cq++;

	*cp++ = '=';
	cq = id;
	while (*cq != '\0' && (cq - id) < 4)
		*cp++ = *cq++;
	
	ctl_putdata(buffer, cp - buffer, 0);
}


/*
 * ctl_putarray - write a tagged eight element s_fp array into the response
 */
static void
ctl_putarray(tag, arr, start)
	char *tag;
	s_fp *arr;
	int start;
{
	register char *cp, *cq;
	char buffer[200];
	int i, ind;
	int len;

	cp = buffer;
	cq = tag;
	while (*cq != '\0')
		*cp++ = *cq++;
	*cp++ = '=';
	/*
	 * Hack.  We know the tag is either filtdelay, filtoffset,
	 * or filterror.  Space over the shorter words one space.
	 */
	if ((cp - buffer) < 11)
		*cp++ = ' ';

	i = start;
	ind = 0;
	do {
		if (i == 0)
			i = NTP_SHIFT;
		i--;
		if (ind) {
			*cp++ = ' ';
		} else {
			ind = 1;
		}
		cq = fptoms(arr[i], 2);
		len = strlen(cq);
		while (len < 7) {
			*cp++ = ' ';
			len++;
		}
		while (*cq != '\0')
			*cp++ = *cq++;
	} while(i != start);

	ctl_putdata(buffer, cp - buffer, 0);
}


/*
 * ctl_putsys - output a system variable
 */
static void
ctl_putsys(varid)
	int varid;
{
	l_fp tmp;

	switch (varid) {
	case CS_LEAP:
		ctl_putuint(sys_var[CS_LEAP].text, (U_LONG)sys_leap);
		break;
	case CS_STRATUM:
		ctl_putuint(sys_var[CS_STRATUM].text, (U_LONG)sys_stratum);
		break;
	case CS_PRECISION:
		ctl_putint(sys_var[CS_PRECISION].text, (LONG)sys_precision);
		break;
	case CS_ROOTDELAY:
		ctl_putfp(sys_var[CS_ROOTDELAY].text, sys_rootdelay);
		break;
	case CS_ROOTDISPERSION:
		ctl_putufp(sys_var[CS_ROOTDISPERSION].text,
			   sys_rootdispersion);
		break;
	case CS_REFID:
		if (sys_stratum <= 1)
			ctl_putid(sys_var[CS_REFID].text, (char *)&sys_refid);
		else
			ctl_putadr(sys_var[CS_REFID].text, sys_refid);
		break;
	case CS_REFTIME:
		ctl_putts(sys_var[CS_REFTIME].text, &sys_reftime);
		break;
	case CS_POLL:
		ctl_putuint(sys_var[CS_POLL].text, (U_LONG)sys_poll);
		break;
	case CS_PEERID:
		if (sys_peer == NULL)
			ctl_putuint(sys_var[CS_PEERID].text, (U_LONG)0);
		else
			ctl_putuint(sys_var[CS_PEERID].text,
			    (U_LONG)sys_peer->associd);
		break;
	case CS_OFFSET:
		ctl_putlfp(sys_var[CS_OFFSET].text, &last_offset);
		break;
	case CS_DRIFT:
		ctl_putfp(sys_var[CS_DRIFT].text, drift_comp);
		break;
	case CS_COMPLIANCE:
		ctl_putuint(sys_var[CS_COMPLIANCE].text, (U_LONG)time_constant);
		break;
	case CS_CLOCK:
		get_systime(&tmp);
		ctl_putts(sys_var[CS_CLOCK].text, &tmp);
		break;
	case CS_LEAPIND:
		ctl_putuint(sys_var[CS_LEAPIND].text, (U_LONG)leap_indicator);
		break;
	case CS_LEAPWARNING:
		ctl_putuint(sys_var[CS_LEAPWARNING].text, (U_LONG)leap_warning);
		break;
	case CS_PROCESSOR:
		ctl_putstr(sys_var[CS_PROCESSOR].text, utsname.machine,
		    strlen(utsname.machine));
		break;
	case CS_SYSTEM:
		ctl_putstr(sys_var[CS_SYSTEM].text, utsname.sysname,
		    strlen(utsname.sysname));
		break;
	case CS_KEYID:
		ctl_putuint(sys_var[CS_KEYID].text, (U_LONG)0);
		break;
	case CS_REFSKEW:
		ctl_putlfp(sys_var[CS_REFSKEW].text, &sys_refskew);
		break;
	case CS_VARLIST:
		{
		  char buf[CTL_MAX_DATA_LEN];
		  register char *s, *ss, *t, *be;
		  register int i;
		  register struct ctl_var *k;

		  s = buf;
		  be = buf + sizeof(buf) - strlen(sys_var[CS_VARLIST].text) - 4;
		  if (s > be)
		    break;	/* really long var name 8-( - Killer */
		  
		  strcpy(s, sys_var[CS_VARLIST].text);
		  strcat(s, "=\"");
		  s += strlen(s);
		  t = s;

		  for (k = sys_var; !(k->flags &EOV); k++)
		    {
		      if (k->flags & PADDING)
			continue;

		      i = strlen(k->text);
		      if (s+i+1 >= be)
			break;
		      if (s != t)
			*s++ = ',';
		      strcpy(s, k->text);
		      s += i;
		    }

		  for (k = ext_sys_var; k && !(k->flags &EOV); k++)
		    {
		      if (k->flags & PADDING)
			continue;

		      ss = k->text;
		      if (!ss)
			continue;

		      while (*ss && *ss != '=')
			ss++;

		      i = ss - k->text;
		      if (s+i+1 >= be)
			break;
		      if (s != t)
			*s++ = ',';
		      strncpy(s, k->text, i);
		      s += i;
		    }

		  if (s+2 >= be)
		    break;
		  
		  *s++ = '"';
		  *s = '\0';

		  ctl_putdata(buf, s - buf, 0);
		}
		break;
	}
}


/*
 * ctl_putpeer - output a peer variable
 */
static void
ctl_putpeer(varid, peer)
	int varid;
	struct peer *peer;
{
	switch (varid) {
	case CP_CONFIG:
		ctl_putuint(peer_var[CP_CONFIG].text,
		    (U_LONG)((peer->flags & FLAG_CONFIG) != 0));
		break;
	case CP_AUTHENABLE:
		ctl_putuint(peer_var[CP_AUTHENABLE].text,
		    (U_LONG)((peer->flags & FLAG_AUTHENABLE) != 0));
		break;
	case CP_AUTHENTIC:
		ctl_putuint(peer_var[CP_AUTHENTIC].text,
		    (U_LONG)((peer->flags & FLAG_AUTHENTIC) != 0));
		break;
	case CP_SRCADR:
		ctl_putadr(peer_var[CP_SRCADR].text,
		    peer->srcadr.sin_addr.s_addr);
		break;
	case CP_SRCPORT:
		ctl_putuint(peer_var[CP_SRCPORT].text,
		    (U_LONG)ntohs(peer->srcadr.sin_port));
		break;
	case CP_DSTADR:
		ctl_putadr(peer_var[CP_DSTADR].text,
		    peer->dstadr->sin.sin_addr.s_addr);
		break;
	case CP_DSTPORT:
		ctl_putuint(peer_var[CP_DSTPORT].text,
		    (U_LONG)ntohs(peer->dstadr->sin.sin_port));
		break;
	case CP_LEAP:
		ctl_putuint(peer_var[CP_LEAP].text, (U_LONG)peer->leap);
		break;
	case CP_HMODE:
		ctl_putuint(peer_var[CP_HMODE].text, (U_LONG)peer->hmode);
		break;
	case CP_STRATUM:
		ctl_putuint(peer_var[CP_STRATUM].text, (U_LONG)peer->stratum);
		break;
	case CP_PPOLL:
		ctl_putuint(peer_var[CP_PPOLL].text, (U_LONG)peer->ppoll);
		break;
	case CP_HPOLL:
		ctl_putuint(peer_var[CP_HPOLL].text, (U_LONG)peer->hpoll);
		break;
	case CP_PRECISION:
		ctl_putint(peer_var[CP_PRECISION].text, (LONG)peer->precision);
		break;
	case CP_ROOTDELAY:
		ctl_putfp(peer_var[CP_ROOTDELAY].text, peer->rootdelay);
		break;
	case CP_ROOTDISPERSION:
		ctl_putufp(peer_var[CP_ROOTDISPERSION].text,
			   peer->rootdispersion);
		break;
	case CP_REFID:
		if (peer->stratum > 1)
			ctl_putadr(peer_var[CP_REFID].text, peer->refid);
		else
			ctl_putid(peer_var[CP_REFID].text,
			    (char *)&peer->refid);
		break;
	case CP_REFTIME:
		ctl_putts(peer_var[CP_REFTIME].text, &peer->reftime);
		break;
	case CP_ORG:
		ctl_putts(peer_var[CP_ORG].text, &peer->org);
		break;
	case CP_REC:
		ctl_putts(peer_var[CP_REC].text, &peer->rec);
		break;
	case CP_XMT:
		ctl_putts(peer_var[CP_XMT].text, &peer->xmt);
		break;
	case CP_REACH:
		ctl_puthex(peer_var[CP_REACH].text, (U_LONG)peer->reach);
		break;
	case CP_FLASH:
		ctl_puthex(peer_var[CP_FLASH].text, (U_LONG)peer->flash);
		break;
	case CP_VALID:
		ctl_putuint(peer_var[CP_VALID].text, (U_LONG)peer->valid);
		break;
	case CP_TIMER:
		ctl_putuint(peer_var[CP_TIMER].text,
		    peer->event_timer.event_time - current_time);
		break;
	case CP_DELAY:
		ctl_putfp(peer_var[CP_DELAY].text, peer->delay);
		break;
	case CP_OFFSET:
		ctl_putlfp(peer_var[CP_OFFSET].text, &peer->offset);
		break;
	case CP_DISPERSION:
		ctl_putufp(peer_var[CP_DISPERSION].text, peer->dispersion);
		break;
	case CP_KEYID:
		ctl_putuint(peer_var[CP_KEYID].text, peer->keyid);
		break;
	case CP_FILTDELAY:
		ctl_putarray(peer_var[CP_FILTDELAY].text,
		     peer->filter_delay, (int)peer->filter_nextpt);
		break;
	case CP_FILTOFFSET:
		ctl_putarray(peer_var[CP_FILTOFFSET].text,
		     peer->filter_soffset, (int)peer->filter_nextpt);
		break;
	case CP_FILTERROR:
		ctl_putarray(peer_var[CP_FILTERROR].text,
		     (s_fp *)peer->filter_error, (int)peer->filter_nextpt);
		break;
	case CP_PMODE:
		ctl_putuint(peer_var[CP_PMODE].text, (U_LONG)peer->pmode);
		break;
	case CP_RECEIVED:
		ctl_putuint(peer_var[CP_RECEIVED].text, peer->received);
		break;
	case CP_SENT:
		ctl_putuint(peer_var[CP_SENT].text, peer->sent);
		break;
	case CP_VARLIST:
		{
		  char buf[CTL_MAX_DATA_LEN];
		  register char *s, *t, *be;
		  register int i;
		  register struct ctl_var *k;

		  s = buf;
		  be = buf + sizeof(buf) - strlen(peer_var[CP_VARLIST].text) - 4;
		  if (s > be)
		    break;	/* really long var name 8-( - Killer */
		  
		  strcpy(s, peer_var[CP_VARLIST].text);
		  strcat(s, "=\"");
		  s += strlen(s);
		  t = s;

		  for (k = peer_var; !(k->flags &EOV); k++)
		    {
		      if (k->flags & PADDING)
			continue;

		      i = strlen(k->text);
		      if (s+i+1 >= be)
			break;
		      if (s != t)
			*s++ = ',';
		      strcpy(s, k->text);
		      s += i;
		    }

		  if (s+2 >= be)
		    break;
		  
		  *s++ = '"';
		  *s = '\0';

		  ctl_putdata(buf, s - buf, 0);
		}
		break;
	}
}


#ifdef REFCLOCK
/*
 * ctl_putclock - output clock variables
 */
static void
ctl_putclock(varid, clock, mustput)
	int varid;
	struct refclockstat *clock;
	int mustput;
{
	switch(varid) {
	case CC_TYPE:
		if (mustput || clock->clockdesc == NULL
		    || *(clock->clockdesc) == '\0') {
			ctl_putuint(clock_var[CC_TYPE].text,
			    (U_LONG)clock->type);
		}
		break;
	case CC_TIMECODE:
		ctl_putstr(clock_var[CC_TIMECODE].text, clock->lastcode,
		    (int)clock->lencode);
		break;
	case CC_POLL:
		ctl_putuint(clock_var[CC_POLL].text, (U_LONG)clock->polls);
		break;
	case CC_NOREPLY:
		ctl_putuint(clock_var[CC_NOREPLY].text, clock->noresponse);
		break;
	case CC_BADFORMAT:
		ctl_putuint(clock_var[CC_BADFORMAT].text, clock->badformat);
		break;
	case CC_BADDATA:
		ctl_putuint(clock_var[CC_BADDATA].text, clock->baddata);
		break;
	case CC_FUDGETIME1:
		if (mustput || (clock->haveflags & CLK_HAVETIME1))
			ctl_putlfp(clock_var[CC_FUDGETIME1].text,
			    &clock->fudgetime1);
		break;
	case CC_FUDGETIME2:
		if (mustput || (clock->haveflags & CLK_HAVETIME2))
			ctl_putlfp(clock_var[CC_FUDGETIME2].text,
			    &clock->fudgetime2);
		break;
	case CC_FUDGEVAL1:
		if (mustput || (clock->haveflags & CLK_HAVEVAL1))
			ctl_putint(clock_var[CC_FUDGEVAL1].text,
			    clock->fudgeval1);
		break;
	case CC_FUDGEVAL2:
		if (mustput || (clock->haveflags & CLK_HAVEVAL2))
			ctl_putint(clock_var[CC_FUDGEVAL2].text,
			    clock->fudgeval2);
		break;
	case CC_FLAGS:
		if (mustput || (clock->haveflags &
		    (CLK_HAVEFLAG1|CLK_HAVEFLAG2|CLK_HAVEFLAG3|CLK_HAVEFLAG4)))
			ctl_putuint(clock_var[CC_FLAGS].text,
			    (U_LONG)clock->flags);
		break;
	case CC_DEVICE:
		if (clock->clockdesc == NULL || *(clock->clockdesc) == '\0') {
			if (mustput)
				ctl_putstr(clock_var[CC_DEVICE].text, "", 0);
		} else {
			ctl_putstr(clock_var[CC_DEVICE].text, clock->clockdesc,
			    strlen(clock->clockdesc));
		}
		break;
	case CC_VARLIST:
		{
		  char buf[CTL_MAX_DATA_LEN];
		  register char *s, *ss, *t, *be;
		  register int i;
		  register struct ctl_var *k;

		  s = buf;
		  be = buf + sizeof(buf) - strlen(clock_var[CC_VARLIST].text) - 4;
		  if (s > be)
		    break;	/* really long var name 8-( - Killer */
		  
		  strcpy(s, clock_var[CC_VARLIST].text);
		  strcat(s, "=\"");
		  s += strlen(s);
		  t = s;

		  for (k = clock_var; !(k->flags &EOV); k++)
		    {
		      if (k->flags & PADDING)
			continue;

		      i = strlen(k->text);
		      if (s+i+1 >= be)
			break;
		      if (s != t)
			*s++ = ',';
		      strcpy(s, k->text);
		      s += i;
		    }

		  for (k = clock->kv_list; k && !(k->flags &EOV); k++)
		    {
		      if (k->flags & PADDING)
			continue;

		      ss = k->text;
		      if (!ss)
			continue;
		      
		      while (*ss && *ss != '=')
			ss++;

		      i = ss - k->text;
		      if (s+i+1 >= be)
			break;
		      if (s != t)
			*s++ = ',';
		      strncpy(s, k->text, i);
		      s += i;
		      *s = '\0';
		    }

		  if (s+2 >= be)
		    break;
		  
		  *s++ = '"';
		  *s = '\0';

		  ctl_putdata(buf, s - buf, 0);
		}
		break;
	}
}
#endif



/*
 * ctl_getitem - get the next data item from the incoming packet
 */
static struct ctl_var *
ctl_getitem(var_list, data)
	struct ctl_var *var_list;
	char **data;
{
	register struct ctl_var *v;
	register char *cp, *tp;
	static struct ctl_var eol = { 0, EOV, };
	static char buf[128];

	/*
	 * Delete leading commas and white space
	 */
	while (reqpt < reqend && (*reqpt == ',' || isspace(*reqpt))) {
		reqpt++;
	}

	if (reqpt >= reqend)
		return 0;

	if (var_list == (struct ctl_var *)0)
	  return &eol;
	
	/*
	 * Look for a first character match on the tag.  If we find
	 * one, see if it is a full match.
	 */
	v = var_list;
	cp = reqpt;
	while (!(v->flags & EOV)) {
		if (!(v->flags & PADDING) && *cp == *(v->text)) {
			tp = v->text;
			while (*tp != '\0' && *tp != '=' && cp < reqend && *cp == *tp) {
				cp++;
				tp++;
			}
			if ((*tp == '\0') || (*tp == '=')) {
				while (cp < reqend && isspace(*cp))
					cp++;
				if (cp == reqend || *cp == ',') {
					buf[0] = '\0';
					*data = buf;
					if (cp < reqend)
						cp++;
					reqpt = cp;
					return v;
				}
				if (*cp == '=') {
					cp++;
					tp = buf;
					while (cp < reqend && isspace(*cp))
						cp++;
					while (cp < reqend && *cp != ',')
						*tp++ = *cp++;
					if (cp < reqend)
						cp++;
					*tp = '\0';
					while (isspace(*(tp-1)))
						*(--tp) = '\0';
					reqpt = cp;
					*data = buf;
					return v;
				}
			}
			cp = reqpt;
		}
		v++;
	}
	return v;
}


/*
 * control_unspec - response to an unspecified op-code
 */
/*ARGSUSED*/
static void
control_unspec(rbufp, restrict)
	struct recvbuf *rbufp;
	int restrict;
{
	struct peer *peer;

	/*
	 * What is an appropriate response to an unspecified op-code?
	 * I return no errors and no data, unless a specified assocation
	 * doesn't exist.
	 */
	if (res_associd != 0) {
		if ((peer = findpeerbyassoc((int)res_associd)) == 0) {
			ctl_error(CERR_BADASSOC);
			return;
		}
		rpkt.status = htons(ctlpeerstatus(peer));
	} else {
		rpkt.status = htons(ctlsysstatus());
	}
	ctl_flushpkt(0);
}


/*
 * read_status - return either a list of associd's, or a particular
 *		 peer's status.
 */
/*ARGSUSED*/
static void
read_status(rbufp, restrict)
	struct recvbuf *rbufp;
	int restrict;
{
	register int i;
	register struct peer *peer;
	u_short ass_stat[CTL_MAX_DATA_LEN/sizeof(u_short)];

#ifdef DEBUG
	if (debug >= 2)
		printf("read_status: ID %d\n", res_associd);
#endif
	/*
	 * Two choices here.  If the specified association ID is
	 * zero we return all known assocation ID's.  Otherwise
	 * we return a bunch of stuff about the particular peer.
	 */
	if (res_associd == 0) {
		register int n;

		n = 0;
		rpkt.status = htons(ctlsysstatus());
		for (i = 0; i < HASH_SIZE; i++) {
			for (peer = assoc_hash[i]; peer != 0;
			    peer = peer->ass_next) {
				ass_stat[n++] = htons(peer->associd);
				ass_stat[n++] = htons(ctlpeerstatus(peer));
				if (n == CTL_MAX_DATA_LEN/sizeof(u_short)) {
					ctl_putdata((char *)ass_stat,
					    n * sizeof(u_short), 1);
					n = 0;
				}
			}
		}

		if (n != 0)
			ctl_putdata((char *)ass_stat, n * sizeof(u_short), 1);
		ctl_flushpkt(0);
	} else {
		peer = findpeerbyassoc((int)res_associd);
		if (peer == 0) {
			ctl_error(CERR_BADASSOC);
		} else {
			register u_char *cp;

			rpkt.status = htons(ctlpeerstatus(peer));
			if (res_authokay)
				peer->num_events = 0;
			/*
			 * For now, output everything we know about the peer.
			 * May be more selective later.
			 */
			for (cp = def_peer_var; *cp != 0; cp++)
				ctl_putpeer((int)*cp, peer);
			ctl_flushpkt(0);
		}
	}
}


/*
 * read_variables - return the variables the caller asks for
 */
/*ARGSUSED*/
static void
read_variables(rbufp, restrict)
	struct recvbuf *rbufp;
	int restrict;
{
	register struct ctl_var *v;
	register int i;
	char *valuep;
	u_char *wants;
	int gotvar = (CS_MAXCODE>CP_MAXCODE) ? (CS_MAXCODE+1) : (CP_MAXCODE+1);

	if (res_associd == 0) {
		/*
		 * Wants system variables.  Figure out which he wants
		 * and give them to him.
		 */
		rpkt.status = htons(ctlsysstatus());
		if (res_authokay)
			ctl_sys_num_events = 0;
		gotvar += count_var(ext_sys_var);
		wants = (u_char *)emalloc(gotvar);
		memset((char *)wants, 0, gotvar);
		gotvar = 0;
		while ((v = ctl_getitem(sys_var, &valuep)) != 0) {
			if (v->flags & EOV) {
			        if ((v = ctl_getitem(ext_sys_var, &valuep)) != 0) {
				        if (v->flags & EOV) {
					        ctl_error(CERR_UNKNOWNVAR);
						free((char *)wants);
						return;
					}
					wants[CS_MAXCODE+1+v->code] = 1;
					gotvar = 1;
					continue;
			       	} else {
				        break; /* shouldn't happen ! */
				}
			}
			wants[v->code] = 1;
			gotvar = 1;
		}
		if (gotvar) {
			for (i = 1; i <= CS_MAXCODE; i++)
				if (wants[i])
					ctl_putsys(i);
			for (i = 0; ext_sys_var && !(ext_sys_var[i].flags & EOV); i++)
			        if (wants[i+CS_MAXCODE+1])
				        ctl_putdata(ext_sys_var[i].text,
						    strlen(ext_sys_var[i].text), 0);
		} else {
			register u_char *cs;
			register struct ctl_var *kv;

			for (cs = def_sys_var; *cs != 0; cs++)
			        ctl_putsys((int)*cs);
			for (kv = ext_sys_var; kv && !(kv->flags & EOV); kv++)
			        if (kv->flags & DEF)
				        ctl_putdata(kv->text, strlen(kv->text), 0);
		}
		free((char *)wants);
	} else {
		register struct peer *peer;

		/*
		 * Wants info for a particular peer.  See if we know
		 * the guy.
		 */
		peer = findpeerbyassoc((int)res_associd);
		if (peer == 0) {
			ctl_error(CERR_BADASSOC);
			return;
		}

		rpkt.status = htons(ctlpeerstatus(peer));
		if (res_authokay)
			peer->num_events = 0;
		wants = (u_char *)emalloc(gotvar);
		memset((char*)wants, 0, gotvar);
		gotvar = 0;
		while ((v = ctl_getitem(peer_var, &valuep)) != 0) {
			if (v->flags & EOV) {
				ctl_error(CERR_UNKNOWNVAR);
				free((char *)wants);
				return;
			}
			wants[v->code] = 1;
			gotvar = 1;
		}
		if (gotvar) {
			for (i = 1; i <= CP_MAXCODE; i++)
				if (wants[i])
					ctl_putpeer(i, peer);
		} else {
			register u_char *cp;

			for (cp = def_peer_var; *cp != 0; cp++)
				ctl_putpeer((int)*cp, peer);
		}
		free((char *)wants);
	}
	ctl_flushpkt(0);
}


/*
 * write_variables - write into variables.  We only allow leap bit writing
 *		     this way.
 */
/*ARGSUSED*/
static void
write_variables(rbufp, restrict)
	struct recvbuf *rbufp;
	int restrict;
{
	register struct ctl_var *v;
	register int ext_var;
	char *valuep;
	LONG val;
	u_char leapind, leapwarn;

	/*
	 * If he's trying to write into a peer tell him no way
	 */
	if (res_associd != 0) {
		ctl_error(CERR_PERMISSION);
		return;
	}

	/*
	 * Set status
	 */
	rpkt.status = htons(ctlsysstatus());

	/*
	 * Set flags to not-in-sync so we can tell when we get something.
	 */
	leapind = (u_char)~0;
	leapwarn = (u_char)~0;

	/*
	 * Look through the variables.  Dump out at the first sign of trouble.
	 */
	while ((v = ctl_getitem(sys_var, &valuep)) != 0) {
		ext_var = 0;
		if (v->flags & EOV) {
			if ((v = ctl_getitem(ext_sys_var, &valuep)) != 0) {
				if (v->flags & EOV) {
					ctl_error(CERR_UNKNOWNVAR);
					return;
				}
				ext_var = 1;
			} else {
				break;
			}
		}
		if (!(v->flags & CAN_WRITE)) {
			ctl_error(CERR_PERMISSION);
			return;
		}
		if (!ext_var && (*valuep == '\0' || !atoint(valuep, &val))) {
			ctl_error(CERR_BADFMT);
			return;
		}
		if (!ext_var && (val & ~LEAP_NOTINSYNC) != 0) {
			ctl_error(CERR_BADVALUE);
			return;
		}

		if (ext_var) {
			char *s = emalloc(strlen(v->text)+strlen(valuep)+2);
		        char *t, *tt = s;

		        t = v->text;
			while (*t && *t != '=')
			  *tt++ = *t++;
			
			*tt++ = '=';
			strcat(tt, valuep);

			set_sys_var(s, strlen(s)+1, v->flags);
			free(s);
		} else {
			/*
			 * This one seems sane.  Save it.
			 */
			switch(v->code) {
			case CS_LEAP:
			case CS_LEAPIND:
				leapind = (u_char)val;
				break;
			case CS_LEAPWARNING:
				leapwarn = (u_char)val;
				break;
			default:
				ctl_error(CERR_UNSPEC);		/* our fault, really */
			  	return;
			}
		}
	}

	/*
	 * If we got anything, do it.
	 */
	if (leapind != (u_char)~0 || leapwarn != (u_char)~0) {
		if (!leap_setleap((int)leapind, (int)leapwarn)) {
			ctl_error(CERR_PERMISSION);
			return;
		}
	}
	ctl_flushpkt(0);
}


/*
 * read_clock_status - return clock radio status
 */
/*ARGSUSED*/
static void
read_clock_status(rbufp, restrict)
	struct recvbuf *rbufp;
	int restrict;
{
#ifndef REFCLOCK
	/*
	 * If no refclock support, no data to return
	 */
	ctl_error(CERR_BADASSOC);
#else
	register struct ctl_var *v;
	register int i;
	register struct peer *peer;
	char *valuep;
	u_char *wants;
	int gotvar;
	struct refclockstat clock;

	if (res_associd == 0) {
		/*
		 * Find a clock for this jerk.  If the system peer
		 * is a clock use it, else search the hash tables
		 * for one.
		 */
		if (sys_peer != 0 && (sys_peer->flags & FLAG_REFCLOCK)) {
			peer = sys_peer;
		} else {
			peer = 0;
			for (i = 0; peer == 0 && i < HASH_SIZE; i++) {
				for (peer = assoc_hash[i]; peer != 0;
				    peer = peer->ass_next) {
					if (peer->flags & FLAG_REFCLOCK)
						break;
				}
			}
			if (peer == 0) {
				ctl_error(CERR_BADASSOC);
				return;
			}
		}
	} else {
		peer = findpeerbyassoc((int)res_associd);
		if (peer == 0 || !(peer->flags & FLAG_REFCLOCK)) {
			ctl_error(CERR_BADASSOC);
			return;
		}
	}

	/*
	 * If we got here we have a peer which is a clock.  Get his status.
	 */
	clock.kv_list = (struct ctl_var *)0;
	
	refclock_control(&peer->srcadr, (struct refclockstat *)0, &clock);

	/*
	 * Look for variables in the packet.
	 */
	rpkt.status = htons(ctlclkstatus(&clock));
	gotvar = CC_MAXCODE+1+count_var(clock.kv_list);
	wants = (u_char *)emalloc(gotvar);
	memset((char*)wants, 0, gotvar);
	gotvar = 0;
	while ((v = ctl_getitem(clock_var, &valuep)) != 0) {
		if (v->flags & EOV) {
		        if ((v = ctl_getitem(clock.kv_list, &valuep)) != 0) {
			        if (v->flags & EOV) {
				        ctl_error(CERR_UNKNOWNVAR);
					free((char*)wants);
					free_varlist(clock.kv_list);
					return;
				}
				wants[CC_MAXCODE+1+v->code] = 1;
				gotvar = 1;
				continue;
			} else {
			        break; /* shouldn't happen ! */
			}
		}
		wants[v->code] = 1;
		gotvar = 1;
	}

	if (gotvar) {
		for (i = 1; i <= CC_MAXCODE; i++)
			if (wants[i])
				ctl_putclock(i, &clock, 1);
		for (i = 0; clock.kv_list && !(clock.kv_list[i].flags & EOV); i++)
			if (wants[i+CC_MAXCODE+1])
			        ctl_putdata(clock.kv_list[i].text,
					    strlen(clock.kv_list[i].text), 0);
	} else {
		register u_char *cc;
		register struct ctl_var *kv;

		for (cc = def_clock_var; *cc != 0; cc++)
			ctl_putclock((int)*cc, &clock, 0);
		for (kv = clock.kv_list; kv && !(kv->flags & EOV); kv++)
		  	if (kv->flags & DEF)
			        ctl_putdata(kv->text, strlen(kv->text), 0);
	}

	free((char*)wants);
	free_varlist(clock.kv_list);

	ctl_flushpkt(0);
#endif
}


/*
 * write_clock_status - we don't do this
 */
/*ARGSUSED*/
static void
write_clock_status(rbufp, restrict)
	struct recvbuf *rbufp;
	int restrict;
{
	ctl_error(CERR_PERMISSION);
}

/*
 * Trap support from here on down.  We send async trap messages when the
 * upper levels report trouble.  Traps can by set either by control
 * messages or by configuration.
 */

/*
 * set_trap - set a trap in response to a control message
 */
static void
set_trap(rbufp, restrict)
	struct recvbuf *rbufp;
	int restrict;
{
	int traptype;

	/*
	 * See if this guy is allowed
	 */
	if (restrict & RES_NOTRAP) {
		ctl_error(CERR_PERMISSION);
		return;
	}

	/*
	 * Determine his allowed trap type.
	 */
	traptype = TRAP_TYPE_PRIO;
	if (restrict & RES_LPTRAP)
		traptype = TRAP_TYPE_NONPRIO;

	/*
	 * Call ctlsettrap() to do the work.  Return
	 * an error if it can't assign the trap.
	 */
	if (!ctlsettrap(&rbufp->recv_srcadr, rbufp->dstadr, traptype,
			(int)res_version))
		ctl_error(CERR_NORESOURCE);
	ctl_flushpkt(0);
}


/*
 * unset_trap - unset a trap in response to a control message
 */
static void
unset_trap(rbufp, restrict)
	struct recvbuf *rbufp;
	int restrict;
{
	int traptype;

	/*
	 * We don't prevent anyone from removing his own
	 * trap unless the trap is configured.  Note we also
	 * must be aware of the possibility that restriction
	 * flags were changed since this guy last set his trap.
	 * Set the trap type based on this.
	 */
	traptype = TRAP_TYPE_PRIO;
	if (restrict & RES_LPTRAP)
		traptype = TRAP_TYPE_NONPRIO;

	/*
	 * Call ctlclrtrap() to clear this out.
	 */
	if (!ctlclrtrap(&rbufp->recv_srcadr, rbufp->dstadr, traptype))
		ctl_error(CERR_BADASSOC);
	ctl_flushpkt(0);
}


/*
 * ctlsettrap - called to set a trap
 */
int
ctlsettrap(raddr, linter, traptype, version)
	struct sockaddr_in *raddr;
	struct interface *linter;
	int traptype;
	int version;
{
	register struct ctl_trap *tp;
	register struct ctl_trap *tptouse;

	/*
	 * See if we can find this trap.  If so, we only need update
	 * the flags and the time.
	 */
	if ((tp = ctlfindtrap(raddr, linter)) != NULL) {
		switch (traptype) {
		case TRAP_TYPE_CONFIG:
			tp->tr_flags = TRAP_INUSE|TRAP_CONFIGURED;
			break;
		case TRAP_TYPE_PRIO:
			if (tp->tr_flags & TRAP_CONFIGURED)
				return 1;	/* don't change anything */
			tp->tr_flags = TRAP_INUSE;
			break;
		case TRAP_TYPE_NONPRIO:
			if (tp->tr_flags & TRAP_CONFIGURED)
				return 1;	/* don't change anything */
			tp->tr_flags = TRAP_INUSE|TRAP_NONPRIO;
			break;
		}
		tp->tr_settime = current_time;
		tp->tr_resets++;
		return 1;
	}

	/*
	 * First we heard of this guy.  Try to find a trap structure
	 * for him to use, clearing out lesser priority guys if we
	 * have to.  Clear out anyone who's expired while we're at it.
	 */
	tptouse = NULL;
	for (tp = ctl_trap; tp < &ctl_trap[CTL_MAXTRAPS]; tp++) {
		if ((tp->tr_flags & TRAP_INUSE) &&
		    !(tp->tr_flags & TRAP_CONFIGURED) &&
		    ((tp->tr_settime + CTL_TRAPTIME) > current_time)) {
			tp->tr_flags = 0;
			num_ctl_traps--;
		}

		if (!(tp->tr_flags & TRAP_INUSE)) {
			tptouse = tp;
		} else if (!(tp->tr_flags & TRAP_CONFIGURED)) {
			switch (traptype) {
			case TRAP_TYPE_CONFIG:
				if (tptouse == NULL) {
					tptouse = tp;
					break;
				}
				if (tptouse->tr_flags & TRAP_NONPRIO
				    && !(tp->tr_flags & TRAP_NONPRIO))
					break;
				if (!(tptouse->tr_flags & TRAP_NONPRIO)
				    && tp->tr_flags & TRAP_NONPRIO) {
					tptouse = tp;
					break;
				}
				if (tptouse->tr_origtime < tp->tr_origtime)
					tptouse = tp;
				break;
			case TRAP_TYPE_PRIO:
				if (tp->tr_flags & TRAP_NONPRIO) {
					if (tptouse == NULL ||
					    (tptouse->tr_flags & TRAP_INUSE
					    && tptouse->tr_origtime
					    < tp->tr_origtime))
						tptouse = tp;
				}
				break;
			case TRAP_TYPE_NONPRIO:
				break;
			}
		}
	}

	/*
	 * If we don't have room for him return an error.
	 */
	if (tptouse == NULL)
		return 0;
	
	/*
	 * Set up this structure for him.
	 */
	tptouse->tr_settime = tptouse->tr_origtime = current_time;
	tptouse->tr_count = tptouse->tr_resets = 0;
	tptouse->tr_sequence = 1;
	tptouse->tr_addr = *raddr;
	tptouse->tr_localaddr = linter;
	tptouse->tr_version = version;

	tptouse->tr_flags = TRAP_INUSE;
	if (traptype == TRAP_TYPE_CONFIG)
		tptouse->tr_flags |= TRAP_CONFIGURED;
	else if (traptype == TRAP_TYPE_NONPRIO)
		tptouse->tr_flags |= TRAP_NONPRIO;
	num_ctl_traps++;
	return 1;
}


/*
 * ctlclrtrap - called to clr a trap
 */
int
ctlclrtrap(raddr, linter, traptype)
	struct sockaddr_in *raddr;
	struct interface *linter;
	int traptype;
{
	register struct ctl_trap *tp;

	if ((tp = ctlfindtrap(raddr, linter)) == NULL)
		return 0;
	
	if (tp->tr_flags & TRAP_CONFIGURED
	    && traptype != TRAP_TYPE_CONFIG)
		return 0;
	
	tp->tr_flags = 0;
	num_ctl_traps--;
	return 1;
}


/*
 * ctlfindtrap - find a trap given the remote and local addresses
 */
static struct ctl_trap *
ctlfindtrap(raddr, linter)
	struct sockaddr_in *raddr;
	struct interface *linter;
{
	register struct ctl_trap *tp;

	for (tp = ctl_trap; tp < &ctl_trap[CTL_MAXTRAPS]; tp++) {
		if (tp->tr_flags & TRAP_INUSE
		    && NSRCADR(raddr) == NSRCADR(&tp->tr_addr)
		    && NSRCPORT(raddr) == NSRCPORT(&tp->tr_addr)
		    && linter == tp->tr_localaddr)
			return tp;
	}
	return (struct ctl_trap *)NULL;
}


/*
 * report_event - report an event to the trappers
 */
void
report_event(err, peer)
	int err;
	struct peer *peer;
{
	register int i;

	/*
	 * Record error code in proper spots, but have mercy on the
	 * log file.
	 */
	if (!(err & PEER_EVENT)) {
		if (ctl_sys_num_events < CTL_SYS_MAXEVENTS)
			ctl_sys_num_events++;
		if (ctl_sys_last_event != (u_char)err)
		    syslog(LOG_INFO, "system event %x status %x",
			err, ctlsysstatus());
		ctl_sys_last_event = (u_char)err;
	} else if (peer != 0) {
		peer->last_event = (u_char)(err & ~PEER_EVENT);
		if (peer->num_events < CTL_PEER_MAXEVENTS)
		    peer->num_events++;
		syslog(LOG_INFO, "peer %s event %x status %x",
		    ntoa(&peer->srcadr), err, ctlpeerstatus(peer));
	} else {
		syslog(LOG_ERR, "report_event: err %x, no peer", err);
		return;
	}

	/*
	 * If no trappers, return.
	 */
	if (num_ctl_traps <= 0)
		return;

	/*
	 * Set up the outgoing packet variables
	 */
	res_opcode = CTL_OP_ASYNCMSG;
	res_offset = 0;
	res_async = 1;
	res_authenticate = 0;
	datapt = rpkt.data;
	dataend = &(rpkt.data[CTL_MAX_DATA_LEN]);

	if (!(err & PEER_EVENT)) {
		rpkt.associd = 0;
		rpkt.status = htons(ctlsysstatus());

		/*
		 * For now, put everything we know about system
		 * variables.  Maybe more selective later
		 */
		for (i = 1; i <= CS_MAXCODE; i++)
			ctl_putsys(i);
#ifdef REFCLOCK
		/*
		 * for clock exception events:
		 *    add clock variables to reflect info on exception
		 */
		if (err == EVNT_CLOCKEXCPT) {
		        struct refclockstat clock;
			struct ctl_var *kv;

			clock.kv_list = (struct ctl_var *)0;

			refclock_control(&peer->srcadr,
					 (struct refclockstat *)0,
					 &clock);
			ctl_puthex("refclockstatus",
				   (U_LONG)ctlclkstatus(&clock));

			for (i = 1; i <= CC_MAXCODE; i++)
			        	ctl_putclock(i, &clock, 0);
			for (kv = clock.kv_list; kv && !(kv->flags & EOV); kv++)
			        if (kv->flags & DEF)
			                ctl_putdata(kv->text, strlen(kv->text), 0);

			free_varlist(clock.kv_list);
		}
#endif /*REFCLOCK*/
	} else {
		rpkt.associd = htons(peer->associd);
		rpkt.status = htons(ctlpeerstatus(peer));

		/*
		 * Dump it all.  Later, maybe less.
		 */
		for (i = 1; i <= CP_MAXCODE; i++)
			ctl_putpeer(i, peer);
#ifdef REFCLOCK
			/*
			 * for clock exception events:
			 *    add clock variables to reflect info on exception
			 */
			if (err == EVNT_PEERCLOCK) {
			        struct refclockstat clock;
				struct ctl_var *kv;

				clock.kv_list = (struct ctl_var *)0;
				
 				refclock_control(&peer->srcadr,
						 (struct refclockstat *)0,
						 &clock);

				ctl_puthex("refclockstatus",
					   (U_LONG)ctlclkstatus(&clock));

				for (i = 1; i <= CC_MAXCODE; i++)
 			        	ctl_putclock(i, &clock, 0);
				for (kv = clock.kv_list; kv && !(kv->flags & EOV); kv++)
				        if (kv->flags & DEF)
					        ctl_putdata(kv->text, strlen(kv->text), 0);

				free_varlist(clock.kv_list);
			}
#endif /*REFCLOCK*/
	}

	/*
	 * We're done, return.
	 */
	ctl_flushpkt(0);
}


/*
 * ctl_clr_stats - clear stat counters
 */
void
ctl_clr_stats()
{
	ctltimereset = current_time;
	numctlreq = 0;
	numctlbadpkts = 0;
	numctlresponses = 0;
	numctlfrags = 0;
	numctlerrors = 0;
	numctlfrags = 0;
	numctltooshort = 0;
	numctlinputresp = 0;
	numctlinputfrag = 0;
	numctlinputerr = 0;
	numctlbadoffset = 0;
	numctlbadversion = 0;
	numctldatatooshort = 0;
	numctlbadop = 0;
	numasyncmsgs = 0;
}

static unsigned long
count_var(k)
  struct ctl_var *k;
{
  register unsigned long c;

  c = 0;
  while (k && !(k++->flags & EOV))
    c++;

  return c;
}

char *
add_var(kv, size, def)
     struct ctl_var **kv;
     unsigned long size;
     int def;
{
  register unsigned long c;
  register struct ctl_var *k;

  c = count_var(*kv);

  k = *kv;
  *kv  = (struct ctl_var *)emalloc((c+2)*sizeof(struct ctl_var));
  if (k)
    {
      memmove((char *)*kv, (char *)k, sizeof(struct ctl_var)*c);
      free((char *)k);
    }

  (*kv)[c].code  = c;
  (*kv)[c].text  = (char *)emalloc(size);
  (*kv)[c].flags = def;
  (*kv)[c+1].code  = 0;
  (*kv)[c+1].text  = (char *)0;
  (*kv)[c+1].flags = EOV;
  return (*kv)[c].text;
}

void
set_var(kv, data, size, def)
     struct ctl_var **kv;
     char *data;
     unsigned long size;
     int def;
{
  register struct ctl_var *k;
  register char *s, *t;

  if (!data || !size)
    return;

  if ((k = *kv))
    {
      while (!(k->flags & EOV))
	{
	  s = data;
	  t = k->text;
	  if (t)
	    {
	      while (*t != '=' && *s - *t == 0)
		{
		  s++;
		  t++;
		}
	      if (*s == *t && ((*t == '=') || !*t))
		{
		  free(k->text);
		  k->text = (char *)emalloc(size);
		  memmove(k->text, data, size);
		  k->flags = def;
		  return;
		}
	    }
	  else
	    {
	      k->text = (char *)emalloc(size);
	      memmove(k->text, data, size);
	      k->flags = def;
	      return;
	    }
	  k++;
	}
    }
  t = add_var(kv, size, def);
  memmove(t, data, size);
}

void
set_sys_var(data, size, def)
     char *data;
     unsigned long size;
     int def;
{
  set_var(&ext_sys_var, data, size, def);
}

void
free_varlist(kv)
     struct ctl_var *kv;
{
  struct ctl_var *k;
  if (kv)
    {
      for (k = kv; !(k->flags & EOV); k++)
	free(k->text);
      free((char *)kv);
    }
}
